# client/ — 电脑端 BLE 推送客户端

把本机 Codex 的**真实用量**推送到 ESP32-Pulsar 的圆屏表盘。

```
Codex 后端  ──HTTPS──▶  本脚本  ──BLE(GATT write)──▶  ESP32-Pulsar
 /api/codex/usage        读 ~/.codex/auth.json         hal/ble_usage.cpp
```

## 安装

```bash
cd client
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

`bleak` 在 macOS 上直接用系统 CoreBluetooth，无需额外驱动；首次运行会弹权限请求。

## 用法

```bash
python3 pulsar_ble_client.py               # 循环，每 60s 推一次
python3 pulsar_ble_client.py --once        # 只推一次
python3 pulsar_ble_client.py --dry-run     # 只打印 payload，不连 BLE（先验证取数）
python3 pulsar_ble_client.py --print-usage # 打印 Codex 原始 usage JSON
python3 pulsar_ble_client.py --interval 30
```

设备上电后应广播为 **ESP32-Pulsar**。客户端按名字或服务 UUID
`0b1e5a10-7e3d-4f1a-9c2b-1a2b3c4d5e60` 扫描。

## 数据格式

接口：`GET https://chatgpt.com/backend-api/codex/usage`（可用环境变量 `PULSAR_USAGE_URL` 覆盖），
鉴权头 `Authorization: Bearer <access_token>` + `chatgpt-account-id: <account_id>`。
真实响应关键字段（已实测）：

```json
{
  "plan_type": "plus",
  "rate_limit": {
    "primary_window":   { "used_percent": 0,  "reset_after_seconds": 18000, "reset_at": 1788942594 },
    "secondary_window": { "used_percent": 12, "reset_after_seconds": 522940, "reset_at": 1789447533 }
  },
  "credits": { "has_credits": false, "unlimited": false, "balance": "0" }
}
```

脚本把 Codex 响应压成一行扁平 JSON，写入特征值 `0b1e5a11-…`：

| 键 | 含义 |
|---|---|
| `cu` | current（5h 窗口）已用百分比 0..100 |
| `ci` | current 距离重置的秒数 |
| `wu` | weekly 已用百分比 |
| `wi` | weekly 距离重置的秒数 |
| `wl` | weekly 重置的本地时间文案，如 `16:14 on 18 May` |
| `pl` | 计划枚举（见下） |
| `cc` | 是否有额度 |
| `un` | 是否无限 |
| `rl` | 是否已触发限额 |

`pl` 映射（与固件 `usage_plan_t` 一致）：0 未知 / 1 free / 2 go / 3 plus /
4 pro / 5 team / 6 enterprise。

固件解析在 `core/usage_model.c`，改动键名要两端同步。

## 故障排查

| 现象 | 处理 |
|---|---|
| `接口 401` | access_token 过期，先在终端跑一次 `codex` 刷新凭据 |
| `接口 403` | 被 Cloudflare 限流（短时间内请求太密）。脚本会自动退避重试；也可加大 `--interval` |
| 扫不到设备 | 确认固件已烧入且上电、串口没有刷屏报错；设备广播名是 `ESP32-Pulsar` |
| 写入后屏幕仍显示 `NO DATA` | 用 `--dry-run` 看 payload 是否合法；确认 `cu`/`wu` 键存在 |
| 想换 endpoint | 设环境变量 `PULSAR_USAGE_URL=…` |
