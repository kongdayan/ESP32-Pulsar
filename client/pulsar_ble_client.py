#!/usr/bin/env python3
"""
pulsar_ble_client.py — 把电脑上的实时数据经 BLE 写入 ESP32-Pulsar。

数据源（每个都压成一行扁平 JSON 写入对应特征值）：
  · Codex 用量    ~/.codex/auth.json → GET chatgpt.com/backend-api/codex/usage
  · Claude 用量   Keychain「Claude Code-credentials」→ GET api.anthropic.com/api/oauth/usage
  · DeepSeek 余额 DEEPSEEK_API_KEY   → GET api.deepseek.com/user/balance

用量类数据共用特征值 0b1e5a11-…，用 JSON 里的 "p" 字段区分 provider
（0=Codex 1=Claude 2=NVIDIA 3=AMD 4=GLM，见 core/usage_model.h）；余额走 0b1e5a13-…。

用法：
  pip install -r requirements.txt
  python3 pulsar_ble_client.py                 # 循环，每 60s 推一次
  python3 pulsar_ble_client.py --once          # 只推一次
  python3 pulsar_ble_client.py --dry-run       # 只打印 payload，不连 BLE
  python3 pulsar_ble_client.py --print-usage --print-claude --print-balance

注意：
  · 所有凭据只在本地读取，绝不提交或写入固件。
  · Codex access_token 会过期，终端跑一次 `codex` 刷新；接口有 Cloudflare 限流（403 自动退避）。
  · Claude 凭据存在 macOS Keychain，仅 darwin 平台可读；其他平台自动跳过。
"""

from __future__ import annotations

import argparse
import asyncio
import datetime as _dt
import json
import os
import pathlib
import subprocess
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

# ── 与固件 core/usage_model.h 的 usage_provider_t 对齐 ────────────────────────
PROVIDER_CODEX = 0
PROVIDER_CLAUDE = 1
PROVIDER_NVIDIA = 2
PROVIDER_AMD = 3
PROVIDER_GLM = 4

# ── Codex 后端 ────────────────────────────────────────────────────────────────
USAGE_URL = os.environ.get(
    "PULSAR_USAGE_URL", "https://chatgpt.com/backend-api/codex/usage"
)
AUTH_PATH = pathlib.Path(
    os.environ.get("PULSAR_CODEX_AUTH", str(pathlib.Path.home() / ".codex" / "auth.json"))
)

# ── Claude 后端 ───────────────────────────────────────────────────────────────
CLAUDE_USAGE_URL = os.environ.get(
    "PULSAR_CLAUDE_USAGE_URL", "https://api.anthropic.com/api/oauth/usage"
)
CLAUDE_KEYCHAIN_SERVICE = "Claude Code-credentials"

# ── DeepSeek 后端 ─────────────────────────────────────────────────────────────
DEEPSEEK_BALANCE_URL = os.environ.get(
    "PULSAR_DEEPSEEK_URL", "https://api.deepseek.com/user/balance"
)
DEEPSEEK_KEY_ENV = "DEEPSEEK_API_KEY"

HTTP_TIMEOUT_S = 20
KEYCHAIN_TIMEOUT_S = 10

PLAN_IDS = {
    "free": 1, "go": 2, "plus": 3, "pro": 4, "prolite": 4, "team": 5,
    "business": 6, "ent26": 6, "enterprise": 6, "edu": 6, "edu_plus": 6, "edu_pro": 6,
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


def _first(d: dict, *keys):
    for k in keys:
        if k in d and d[k] is not None:
            return d[k]
    return None


def _local_label(epoch_s: int) -> str:
    dt = _dt.datetime.fromtimestamp(epoch_s)
    return f"{dt:%H:%M} on {dt.day} {MONTHS[dt.month - 1]}"


def _iso_epoch(iso: str | None) -> int | None:
    if not iso:
        return None
    try:
        return int(_dt.datetime.fromisoformat(iso).timestamp())
    except ValueError:
        return None


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


def _window(rate_limit: dict, *names) -> dict:
    """窗口在不同版本里叫 primary|primary_window，两种都兼容。"""
    for name in names:
        win = rate_limit.get(name)
        if isinstance(win, dict):
            return win
    return {}


def _reset_epoch(window: dict, now: int) -> int | None:
    reset_at = _first(window, "reset_at", "resets_at", "resetsAt")
    if reset_at:
        return int(reset_at)
    after = _first(window, "reset_after_seconds", "resetAfterSeconds")
    return now + int(after) if after is not None else None


def _reset_in(window: dict, now: int) -> int:
    after = _first(window, "reset_after_seconds", "resetAfterSeconds")
    if after is not None:
        return max(0, int(after))
    epoch = _reset_epoch(window, now)
    return max(0, epoch - now) if epoch is not None else 0


def build_payload(usage: dict) -> tuple[bytes, dict]:
    """Codex 响应 → 扁平 JSON。"""
    rate_limit = usage.get("rate_limit") or {}
    primary = _window(rate_limit, "primary", "primary_window")
    secondary = _window(rate_limit, "secondary", "secondary_window")

    now = int(time.time())
    weekly_epoch = _reset_epoch(secondary, now)
    plan_raw = str(_first(rate_limit, "plan_type", "planType")
                   or _first(usage, "plan_type", "planType") or "").lower()
    credits = usage.get("credits") or rate_limit.get("credits") or {}

    fields = {
        "p": PROVIDER_CODEX,
        "cu": max(0, min(100, int(_first(primary, "used_percent", "usedPercent") or 0))),
        "ci": _reset_in(primary, now),
        "wu": max(0, min(100, int(_first(secondary, "used_percent", "usedPercent") or 0))),
        "wi": _reset_in(secondary, now),
        "wl": _local_label(weekly_epoch) if weekly_epoch is not None else "",
        "pl": PLAN_IDS.get(plan_raw, 0),
        "cc": 1 if credits.get("has_credits") else 0,
        "un": 1 if credits.get("unlimited") else 0,
        "rl": 1 if _first(rate_limit, "limit_reached", "rate_limit_reached") else 0,
    }
    return json.dumps(fields, separators=(",", ":"), ensure_ascii=True).encode(), fields


# ── Claude ────────────────────────────────────────────────────────────────────

def load_claude_token() -> str | None:
    """从 macOS Keychain 读 Claude Code 的 OAuth accessToken（只读，不打印）。"""
    if sys.platform != "darwin":
        return None
    try:
        out = subprocess.run(
            ["security", "find-generic-password", "-s", CLAUDE_KEYCHAIN_SERVICE, "-w"],
            capture_output=True, text=True, timeout=KEYCHAIN_TIMEOUT_S,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    if out.returncode != 0:
        return None
    try:
        cred = json.loads(out.stdout)
    except json.JSONDecodeError:
        return None
    return ((cred.get("claudeAiOauth") or {}).get("accessToken")) or None


def fetch_claude_usage(token: str) -> dict:
    return _http_get_json(CLAUDE_USAGE_URL, {
        "Authorization": f"Bearer {token}",
        "Accept": "application/json",
        "User-Agent": "pulsar-ble-client/0.1",
        "anthropic-version": "2023-06-01",
    })


def _claude_window(usage: dict, kind: str, fallback: str) -> dict:
    """优先用规整的 limits[]（kind=session/weekly_all），退回 five_hour/seven_day。"""
    for lim in (usage.get("limits") or []):
        if isinstance(lim, dict) and lim.get("kind") == kind:
            return lim
    win = usage.get(fallback)
    return win if isinstance(win, dict) else {}


def build_claude_payload(usage: dict) -> tuple[bytes, dict]:
    """Claude /api/oauth/usage → 与 Codex 同一套扁平 JSON（p=1）。"""
    now = int(time.time())
    session = _claude_window(usage, "session", "five_hour")
    weekly = _claude_window(usage, "weekly_all", "seven_day")

    def pct(win: dict) -> int:
        v = float(_first(win, "percent", "utilization") or 0)
        return max(0, min(100, int(round(v))))

    def resets_in(win: dict) -> int:
        ep = _iso_epoch(_first(win, "resets_at", "resetsAt"))
        return max(0, ep - now) if ep is not None else 0

    weekly_epoch = _iso_epoch(_first(weekly, "resets_at", "resetsAt"))

    fields = {
        "p": PROVIDER_CLAUDE,
        "cu": pct(session),
        "ci": resets_in(session),
        "wu": pct(weekly),
        "wi": resets_in(weekly),
        "wl": _local_label(weekly_epoch) if weekly_epoch is not None else "",
        "pl": 0,
        "cc": 0,
        "un": 0,
        "rl": 0,
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

def _collect(args, token: str, account_id: str):
    """取回所有 payload，各源失败互不影响。返回 (usage列表, 余额, errors)。"""
    usage_payloads: list[tuple[str, bytes]] = []
    balance_payload: bytes | None = None
    errors: list[ApiError] = []

    if not args.no_codex:
        try:
            usage = fetch_usage(token, account_id)
            if args.print_usage:
                print(json.dumps(usage, indent=2, ensure_ascii=False))
            payload, _ = build_payload(usage)
            usage_payloads.append(("codex", payload))
            log(f"usage   codex  ({len(payload):3d}B): {payload.decode()}")
        except ApiError as exc:
            errors.append(exc)
            log(f"Codex 失败（{exc.status}）：{str(exc)[:160]}")

    if not args.no_claude:
        claude_token = load_claude_token()
        if not claude_token:
            log("读不到 Claude Code 凭据（仅 macOS Keychain），跳过")
        else:
            try:
                usage = fetch_claude_usage(claude_token)
                if args.print_claude:
                    print(json.dumps(usage, indent=2, ensure_ascii=False))
                payload, _ = build_claude_payload(usage)
                usage_payloads.append(("claude", payload))
                log(f"usage   claude ({len(payload):3d}B): {payload.decode()}")
            except ApiError as exc:
                errors.append(exc)
                log(f"Claude 失败（{exc.status}）：{str(exc)[:160]}")

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
                log(f"balance deepseek({len(balance_payload):3d}B): {balance_payload.decode()}")
            except ApiError as exc:
                errors.append(exc)
                log(f"DeepSeek 失败（{exc.status}）：{str(exc)[:160]}")

    return usage_payloads, balance_payload, errors


async def run_cycle(args, token: str, account_id: str) -> bool:
    usage_payloads, balance_payload, errors = _collect(args, token, account_id)

    if args.dry_run:
        return not errors and (bool(usage_payloads) or balance_payload is not None)

    if not usage_payloads and balance_payload is None:
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
        for _name, payload in usage_payloads:
            await client.write_gatt_char(USAGE_CHAR_UUID, payload, response=True)
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
    log(f"Claude   endpoint: {CLAUDE_USAGE_URL}")
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
    parser = argparse.ArgumentParser(description="用量/余额 → ESP32-Pulsar (BLE)")
    parser.add_argument("--once", action="store_true", help="只推送一次")
    parser.add_argument("--dry-run", action="store_true", help="只打印 payload，不连 BLE")
    parser.add_argument("--print-usage", action="store_true", help="打印 Codex 原始 JSON")
    parser.add_argument("--print-claude", action="store_true", help="打印 Claude 原始 JSON")
    parser.add_argument("--print-balance", action="store_true", help="打印 DeepSeek 原始 JSON")
    parser.add_argument("--no-codex", action="store_true", help="跳过 Codex 用量")
    parser.add_argument("--no-claude", action="store_true", help="跳过 Claude 用量")
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
