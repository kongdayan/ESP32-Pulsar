#!/usr/bin/env python3
"""
pulsar_ble_client.py — 把电脑上的实时数据经 BLE 写入 ESP32-Pulsar。

两个数据源，各写一个 GATT 特征值：
  1. Codex 用量   ~/.codex/auth.json → GET chatgpt.com/backend-api/codex/usage
  2. DeepSeek 余额 DEEPSEEK_API_KEY   → GET api.deepseek.com/user/balance

用法：
  pip install -r requirements.txt
  python3 pulsar_ble_client.py                 # 循环，每 60s 推一次
  python3 pulsar_ble_client.py --once          # 只推一次
  python3 pulsar_ble_client.py --dry-run       # 只打印 payload，不连 BLE
  python3 pulsar_ble_client.py --print-usage   # 额外打印 Codex 原始 JSON
  python3 pulsar_ble_client.py --print-balance # 额外打印 DeepSeek 原始 JSON

注意：
  * auth.json 里是 OAuth 凭据，只在本地读取，绝不要提交或写入固件。
  * access_token 会过期；Codex CLI 会自动刷新。接口返回 401 时，
    先在终端跑一次 `codex` 刷新凭据再试。
  * Codex 用量接口有 Cloudflare 限流，短时间请求过密会 403；脚本会自动退避。
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
from decimal import Decimal, InvalidOperation

# ── 与固件 hal/ble_usage.cpp 保持一致，改一端必须改另一端 ─────────────────────
DEVICE_NAME = "ESP32-Pulsar"
SERVICE_UUID = "0b1e5a10-7e3d-4f1a-9c2b-1a2b3c4d5e60"
USAGE_CHAR_UUID = "0b1e5a11-7e3d-4f1a-9c2b-1a2b3c4d5e60"
STATUS_CHAR_UUID = "0b1e5a12-7e3d-4f1a-9c2b-1a2b3c4d5e60"
BALANCE_CHAR_UUID = "0b1e5a13-7e3d-4f1a-9c2b-1a2b3c4d5e60"

# ── Codex 后端 ────────────────────────────────────────────────────────────────
USAGE_URL = os.environ.get(
    "PULSAR_USAGE_URL", "https://chatgpt.com/backend-api/codex/usage"
)
AUTH_PATH = pathlib.Path(
    os.environ.get("PULSAR_CODEX_AUTH", str(pathlib.Path.home() / ".codex" / "auth.json"))
)

# ── DeepSeek 后端 ─────────────────────────────────────────────────────────────
DEEPSEEK_BALANCE_URL = os.environ.get(
    "PULSAR_DEEPSEEK_URL", "https://api.deepseek.com/user/balance"
)
DEEPSEEK_KEY_ENV = "DEEPSEEK_API_KEY"

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


class ApiError(RuntimeError):
    """后端返回错误（含 HTTP 状态码）；由主循环决定是否退避重试。"""

    def __init__(self, status: int, message: str):
        super().__init__(message)
        self.status = status


def log(msg: str) -> None:
    print(f"[pulsar] {msg}", flush=True)


def _http_get_json(url: str, headers: dict) -> dict:
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT_S) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", "replace")[:300]
        raise ApiError(exc.code, body) from exc
    except urllib.error.URLError as exc:
        raise ApiError(0, f"网络错误：{exc.reason}") from exc


# ── Codex ─────────────────────────────────────────────────────────────────────

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


def fetch_usage(token: str, account_id: str) -> dict:
    return _http_get_json(USAGE_URL, {
        "Authorization": f"Bearer {token}",
        "chatgpt-account-id": account_id,
        "originator": "codex_cli_rs",
        "User-Agent": "pulsar-ble-client/0.1",
        "Accept": "application/json",
    })


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
    return json.dumps(fields, separators=(",", ":"), ensure_ascii=True).encode(), fields


# ── DeepSeek ──────────────────────────────────────────────────────────────────

def fetch_deepseek_balance(api_key: str) -> dict:
    return _http_get_json(DEEPSEEK_BALANCE_URL, {
        "Authorization": f"Bearer {api_key}",
        "Accept": "application/json",
        "User-Agent": "pulsar-ble-client/0.1",
    })


def _to_cents(value) -> int:
    """DeepSeek 返回字符串金额（如 "28.55"）→ 分（整数），用 Decimal 避免浮点误差。"""
    try:
        return int((Decimal(str(value)) * 100).to_integral_value())
    except (InvalidOperation, TypeError, ValueError):
        return 0


def build_balance_payload(balance: dict) -> tuple[bytes, dict]:
    """把 DeepSeek /user/balance 响应压成扁平 JSON（金额单位为分）。"""
    infos = balance.get("balance_infos") or []
    info = infos[0] if infos else {}
    fields = {
        "cur": str(info.get("currency") or "")[:3],
        "tot": _to_cents(info.get("total_balance")),
        "gr": _to_cents(info.get("granted_balance")),
        "top": _to_cents(info.get("topped_up_balance")),
        "av": 1 if balance.get("is_available") else 0,
    }
    return json.dumps(fields, separators=(",", ":"), ensure_ascii=True).encode(), fields


# ── 一轮：取数 + 推送 ─────────────────────────────────────────────────────────

def _collect(args, token: str, account_id: str) -> tuple[bytes | None, bytes | None, list[ApiError]]:
    """取回两个 payload（各自失败互不影响），返回 (usage, balance, errors)。"""
    usage_payload: bytes | None = None
    balance_payload: bytes | None = None
    errors: list[ApiError] = []

    if not args.no_codex:
        try:
            usage = fetch_usage(token, account_id)
            if args.print_usage:
                print(json.dumps(usage, indent=2, ensure_ascii=False))
            usage_payload, _ = build_payload(usage)
            log(f"usage   ({len(usage_payload):3d}B): {usage_payload.decode()}")
        except ApiError as exc:
            errors.append(exc)
            log(f"Codex 失败（{exc.status}）：{str(exc)[:160]}")

    if not args.no_deepseek:
        key = args.deepseek_key or os.environ.get(DEEPSEEK_KEY_ENV, "")
        if not key:
            log(f"未设置 {DEEPSEEK_KEY_ENV}，跳过余额")
        else:
            try:
                balance = fetch_deepseek_balance(key)
                if args.print_balance:
                    print(json.dumps(balance, indent=2, ensure_ascii=False))
                balance_payload, _ = build_balance_payload(balance)
                log(f"balance ({len(balance_payload):3d}B): {balance_payload.decode()}")
            except ApiError as exc:
                errors.append(exc)
                log(f"DeepSeek 失败（{exc.status}）：{str(exc)[:160]}")

    return usage_payload, balance_payload, errors


async def run_cycle(args, token: str, account_id: str) -> bool:
    usage_payload, balance_payload, errors = _collect(args, token, account_id)

    if args.dry_run:
        return not errors and (usage_payload is not None or balance_payload is not None)

    if usage_payload is None and balance_payload is None:
        return False

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
        if usage_payload is not None:
            await client.write_gatt_char(USAGE_CHAR_UUID, usage_payload, response=True)
        if balance_payload is not None:
            await client.write_gatt_char(BALANCE_CHAR_UUID, balance_payload, response=True)
        try:
            status = bytes(await client.read_gatt_char(STATUS_CHAR_UUID)).decode(
                "utf-8", "replace"
            )
            log(f"写入完成，固件状态：{status}")
        except Exception:  # noqa: BLE001 - 状态读不到不影响主流程
            log("写入完成（未读到状态特征值）")

    return not errors


async def main_async(args) -> int:
    token, account_id = "", ""
    if not args.no_codex:
        token, account_id = load_codex_tokens()

    log(f"Codex    endpoint: {USAGE_URL}")
    log(f"DeepSeek endpoint: {DEEPSEEK_BALANCE_URL}")

    backoff = args.interval
    while True:
        try:
            ok = await run_cycle(args, token, account_id)
            backoff = args.interval
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
    parser = argparse.ArgumentParser(description="Codex 用量 + DeepSeek 余额 → ESP32-Pulsar (BLE)")
    parser.add_argument("--once", action="store_true", help="只推送一次")
    parser.add_argument("--dry-run", action="store_true", help="只打印 payload，不连 BLE")
    parser.add_argument("--print-usage", action="store_true", help="打印 Codex 原始 JSON")
    parser.add_argument("--print-balance", action="store_true", help="打印 DeepSeek 原始 JSON")
    parser.add_argument("--no-codex", action="store_true", help="跳过 Codex 用量")
    parser.add_argument("--no-deepseek", action="store_true", help="跳过 DeepSeek 余额")
    parser.add_argument("--deepseek-key", default="", help="覆盖 DEEPSEEK_API_KEY")
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
