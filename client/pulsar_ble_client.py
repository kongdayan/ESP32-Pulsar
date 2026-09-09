#!/usr/bin/env python3
"""
pulsar_ble_client.py — 从 Codex 后端取用量，经 BLE 写入 ESP32-Pulsar。

流程：
  1. 读 ~/.codex/auth.json 拿 OAuth access_token + account_id
  2. GET https://chatgpt.com/backend-api/api/codex/usage
  3. 提取 primary(5h)/secondary(weekly) 的 used_percent + resets_at
  4. 扫描 BLE 设备 "ESP32-Pulsar"，连接后把一行扁平 JSON 写入 usage 特征值

用法：
  pip install -r requirements.txt
  python3 pulsar_ble_client.py                 # 循环，每 60s 推一次
  python3 pulsar_ble_client.py --once          # 只推一次
  python3 pulsar_ble_client.py --dry-run       # 只打印 payload，不连 BLE
  python3 pulsar_ble_client.py --print-usage   # 额外打印原始 usage JSON

注意：
  * auth.json 里是 OAuth 凭据，脚本只在本地读取，绝不要提交或写入固件。
  * access_token 会过期；Codex CLI 会自动刷新。若接口返回 401，
    先在终端跑一次 `codex`（或 `codex login`）刷新凭据再试。
"""

from __future__ import annotations

import argparse
import asyncio
import datetime as _dt
import json
import os
import pathlib
import sys
import time
import urllib.error
import urllib.request

# ── 与固件 hal/ble_usage.cpp 保持一致，改一端必须改另一端 ─────────────────────
DEVICE_NAME = "ESP32-Pulsar"
SERVICE_UUID = "0b1e5a10-7e3d-4f1a-9c2b-1a2b3c4d5e60"
USAGE_CHAR_UUID = "0b1e5a11-7e3d-4f1a-9c2b-1a2b3c4d5e60"
STATUS_CHAR_UUID = "0b1e5a12-7e3d-4f1a-9c2b-1a2b3c4d5e60"

# ── Codex 后端 ────────────────────────────────────────────────────────────────
USAGE_URL = os.environ.get(
    "PULSAR_USAGE_URL", "https://chatgpt.com/backend-api/codex/usage"
)
AUTH_PATH = pathlib.Path(
    os.environ.get("PULSAR_CODEX_AUTH", str(pathlib.Path.home() / ".codex" / "auth.json"))
)
HTTP_TIMEOUT_S = 20

# ── 与固件 core/usage_model.h 的 usage_plan_t 对齐 ────────────────────────────
PLAN_IDS = {
    "free": 1,
    "go": 2,
    "plus": 3,
    "pro": 4,
    "prolite": 4,
    "team": 5,
    "business": 6,
    "ent26": 6,
    "enterprise": 6,
    "edu": 6,
    "edu_plus": 6,
    "edu_pro": 6,
}

MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]


def log(msg: str) -> None:
    print(f"[pulsar] {msg}", flush=True)


def load_codex_tokens() -> tuple[str, str]:
    """返回 (access_token, account_id)。"""
    if not AUTH_PATH.exists():
        raise SystemExit(
            f"找不到 {AUTH_PATH}；请先在本机登录 Codex CLI（终端运行一次 `codex`）。"
        )
    try:
        auth = json.loads(AUTH_PATH.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"读取 {AUTH_PATH} 失败：{exc}") from exc

    tokens = auth.get("tokens") or {}
    token = tokens.get("access_token")
    account_id = tokens.get("account_id")
    if not token or not account_id:
        raise SystemExit(f"{AUTH_PATH} 里缺少 tokens.access_token / tokens.account_id。")
    return token, account_id


class CodexApiError(RuntimeError):
    """Codex 后端错误（含 HTTP 状态码）；由主循环决定是否退避重试。"""

    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


def fetch_usage(token: str, account_id: str) -> dict:
    req = urllib.request.Request(
        USAGE_URL,
        headers={
            "Authorization": f"Bearer {token}",
            "chatgpt-account-id": account_id,
            "originator": "codex_cli_rs",
            "User-Agent": "pulsar-ble-client/0.1",
            "Accept": "application/json",
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")[:300]
        raise CodexApiError(exc.code, body) from exc
    except urllib.error.URLError as exc:
        raise CodexApiError(0, f"网络错误：{exc.reason}") from exc


def _first(d: dict, *keys):
    for k in keys:
        if k in d and d[k] is not None:
            return d[k]
    return None


def _window(rate_limit: dict, *names) -> dict:
    """窗口在不同版本里叫 primary|primary_window，两种都兼容。"""
    for name in names:
        win = rate_limit.get(name)
        if isinstance(win, dict):
            return win
    return {}


def _reset_epoch(window: dict, now: int) -> int | None:
    """窗口重置的绝对时刻（epoch 秒）。优先 reset_at，其次由 reset_after_seconds 推算。"""
    reset_at = _first(window, "reset_at", "resets_at", "resetsAt")
    if reset_at:
        return int(reset_at)
    after = _first(window, "reset_after_seconds", "resetAfterSeconds")
    return now + int(after) if after is not None else None


def _reset_in(window: dict, now: int) -> int:
    """距重置剩余秒数。优先服务端直接给的 reset_after_seconds（避开时钟偏差）。"""
    after = _first(window, "reset_after_seconds", "resetAfterSeconds")
    if after is not None:
        return max(0, int(after))
    epoch = _reset_epoch(window, now)
    return max(0, epoch - now) if epoch is not None else 0


def _local_label(epoch_s: int) -> str:
    dt = _dt.datetime.fromtimestamp(epoch_s)
    return f"{dt:%H:%M} on {dt.day} {MONTHS[dt.month - 1]}"


def build_payload(usage: dict) -> tuple[bytes, dict]:
    """把 Codex 响应压成固件认识的扁平 JSON。"""
    rate_limit = usage.get("rate_limit") or {}
    primary = _window(rate_limit, "primary", "primary_window")
    secondary = _window(rate_limit, "secondary", "secondary_window")

    now = int(time.time())
    current_pct = int(_first(primary, "used_percent", "usedPercent") or 0)
    weekly_pct = int(_first(secondary, "used_percent", "usedPercent") or 0)

    current_in = _reset_in(primary, now)
    weekly_in = _reset_in(secondary, now)
    weekly_epoch = _reset_epoch(secondary, now)
    weekly_label = _local_label(weekly_epoch) if weekly_epoch is not None else ""

    plan_raw = str(_first(rate_limit, "plan_type", "planType")
                   or _first(usage, "plan_type", "planType") or "").lower()
    credits = usage.get("credits") or rate_limit.get("credits") or {}
    limit_reached = bool(_first(rate_limit, "limit_reached", "rate_limit_reached"))

    fields = {
        "cu": max(0, min(100, current_pct)),
        "ci": current_in,
        "wu": max(0, min(100, weekly_pct)),
        "wi": weekly_in,
        "wl": weekly_label,
        "pl": PLAN_IDS.get(plan_raw, 0),
        "cc": 1 if credits.get("has_credits") else 0,
        "un": 1 if credits.get("unlimited") else 0,
        "rl": 1 if limit_reached else 0,
    }
    payload = json.dumps(fields, separators=(",", ":"), ensure_ascii=True).encode()
    return payload, fields


async def push_once(args, token: str, account_id: str) -> bool:
    usage = fetch_usage(token, account_id)
    if args.print_usage:
        print(json.dumps(usage, indent=2, ensure_ascii=False))

    payload, fields = build_payload(usage)
    log(f"payload ({len(payload)}B): {payload.decode()}")

    if args.dry_run:
        return True

    from bleak import BleakClient, BleakScanner

    def match(device, adv) -> bool:
        if device.name == args.device_name:
            return True
        uuids = [u.lower() for u in (adv.service_uuids or [])]
        return SERVICE_UUID.lower() in uuids

    device = await BleakScanner.find_device_by_filter(match, timeout=args.scan_timeout)
    if device is None:
        log(f"没扫描到 {args.device_name}（设备上电并处于广播状态？）")
        return False

    log(f"连接 {device.address} …")
    async with BleakClient(device, timeout=args.connect_timeout) as client:
        await client.write_gatt_char(USAGE_CHAR_UUID, payload, response=True)
        try:
            status = bytes(await client.read_gatt_char(STATUS_CHAR_UUID)).decode(
                "utf-8", "replace"
            )
            log(f"写入完成，固件状态：{status}")
        except Exception:  # noqa: BLE001 - 状态读不到不影响主流程
            log("写入完成（未读到状态特征值）")
    return True


async def main_async(args) -> int:
    token, account_id = load_codex_tokens()
    log(f"usage endpoint: {USAGE_URL}")

    backoff = args.interval
    while True:
        try:
            ok = await push_once(args, token, account_id)
            backoff = args.interval
        except CodexApiError as exc:
            if exc.status == 401:
                log("401：access_token 过期，先在终端跑一次 `codex` 刷新凭据")
            elif exc.status == 403:
                log("403：被 Cloudflare 限流/拦截，降低推送频率后重试")
            else:
                log(f"接口错误 {exc.status}：{str(exc)[:200]}")
            ok = False
        except Exception as exc:  # noqa: BLE001 - 循环里不让单次失败退出
            log(f"本次失败：{type(exc).__name__}: {exc}")
            ok = False

        if args.once:
            return 0 if ok else 1
        if not ok:
            backoff = min(max(backoff, args.interval) * 2, args.max_backoff)
            log(f"{backoff:.0f}s 后重试")
        await asyncio.sleep(backoff)


def main() -> int:
    parser = argparse.ArgumentParser(description="Codex 用量 → ESP32-Pulsar (BLE)")
    parser.add_argument("--once", action="store_true", help="只推送一次")
    parser.add_argument("--dry-run", action="store_true", help="只打印 payload，不连 BLE")
    parser.add_argument("--print-usage", action="store_true", help="打印原始 usage JSON")
    parser.add_argument("--interval", type=float, default=60.0, help="循环间隔秒（默认 60）")
    parser.add_argument("--max-backoff", type=float, default=600.0, help="失败退避上限秒")
    parser.add_argument("--device-name", default=DEVICE_NAME, help="BLE 广播名")
    parser.add_argument("--scan-timeout", type=float, default=10.0)
    parser.add_argument("--connect-timeout", type=float, default=20.0)
    args = parser.parse_args()

    try:
        return asyncio.run(main_async(args))
    except KeyboardInterrupt:
        log("已停止")
        return 130


if __name__ == "__main__":
    sys.exit(main())
