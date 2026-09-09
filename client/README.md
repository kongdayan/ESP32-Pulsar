# client/ — 电脑端 BLE 推送客户端

把电脑上的实时数据经 BLE 写入 ESP32-Pulsar 的圆屏。两个数据源各写一个特征值：

```
Codex 后端    ──HTTPS──┐
 /codex/usage          │   pulsar_ble_client.py            ESP32-Pulsar
                       ├─▶  (读 auth.json / API key) ──BLE──▶  hal/ble_usage.cpp
DeepSeek 后端 ──HTTPS──┘                                        │
 /user/balance                                                  ▼
                                                    core/*_model.c → 屏幕
```

| 数据源 | 接口 | 凭据 | 写入特征值 |
|---|---|---|---|
| Codex 用量 | `GET chatgpt.com/backend-api/codex/usage` | `~/.codex/auth.json`（OAuth，本地读取） | `0b1e5a11-…` |
| Claude 用量 | `GET api.anthropic.com/api/oauth/usage` | macOS Keychain `Claude Code-credentials` | `0b1e5a11-…` |
| DeepSeek 余额 | `GET api.deepseek.com/user/balance` | 环境变量 `DEEPSEEK_API_KEY` | `0b1e5a13-…` |

> 用量类数据**共用** `0b1e5a11-…`，用 JSON 里的 `p` 区分 provider
> （0=Codex 1=Claude 2=NVIDIA 3=AMD 4=GLM，见 `core/usage_model.h`）。

## 安装

```bash
cd client
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

`bleak` 在 macOS 上直接用系统 CoreBluetooth，无需额外驱动；首次运行会弹权限请求。

## 用法

```bash
export DEEPSEEK_API_KEY=sk-...             # 只在当前 shell 生效

python3 pulsar_ble_client.py               # 循环，每 60s 推一次
python3 pulsar_ble_client.py --once        # 只推一次
python3 pulsar_ble_client.py --dry-run     # 只打印 payload，不连 BLE（先验证取数）
python3 pulsar_ble_client.py --no-codex    # 只推 Claude + DeepSeek
python3 pulsar_ble_client.py --no-claude   # 跳过 Claude
python3 pulsar_ble_client.py --no-deepseek # 跳过余额
python3 pulsar_ble_client.py --print-usage --print-claude --print-balance   # 原始 JSON
python3 pulsar_ble_client.py --interval 30
```

设备上电后应广播为 **ESP32-Pulsar**。客户端按名字或服务 UUID
`0b1e5a10-7e3d-4f1a-9c2b-1a2b3c4d5e60` 扫描。

## 数据格式

### 用量（`0b1e5a11-…`，Codex / Claude 共用）

Codex 实测响应关键字段：

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

压成扁平 JSON（`p` 选择 provider）：

| 键 | 含义 |
|---|---|
| `p` | provider：0 Codex / 1 Claude / 2 NVIDIA / 3 AMD / 4 GLM |
| `cu` / `wu` | current(5h/session) / weekly 已用百分比 0..100 |
| `ci` / `wi` | 距重置剩余秒数（优先服务端给的剩余秒） |
| `wl` | weekly 重置的本地时间文案，如 `16:14 on 18 May` |
| `pl` | 计划：0 未知 / 1 free / 2 go / 3 plus / 4 pro / 5 team / 6 ent |
| `cc` / `un` / `rl` | 有额度 / 无限 / 已触发限额 |

Claude 实测响应（`five_hour` / `seven_day`，或等价的 `limits[]`）：

```json
{ "five_hour": { "utilization": 0.0,  "resets_at": "2026-09-09T08:29:59+00:00" },
  "seven_day": { "utilization": 20.0, "resets_at": "2026-09-14T17:59:59+00:00" } }
```

实测扁平结果：`{"p":1,"cu":0,"ci":16380,"wu":20,"wi":482580,"wl":"01:59 on 15 Sep",...}`

### DeepSeek 余额（`0b1e5a13-…`）

实测响应：

```json
{ "is_available": true,
  "balance_infos": [ { "currency": "CNY", "total_balance": "28.55",
                       "granted_balance": "0.00", "topped_up_balance": "28.55" } ] }
```

金额用 **分（整数）** 传输，避免浮点误差（用 `Decimal` 换算）：

| 键 | 含义 |
|---|---|
| `cur` | 货币代码 `CNY` / `USD` |
| `tot` / `gr` / `top` | 总余额 / 赠金 / 充值余额（分） |
| `av` | `is_available` → 0/1 |

固件解析分别在 `core/usage_model.c` 与 `core/balance_model.c`，键名改动要两端同步。

## 故障排查

| 现象 | 处理 |
|---|---|
| `接口 401` | Codex access_token 过期，先在终端跑一次 `codex` 刷新凭据 |
| `接口 403` | Codex 接口被 Cloudflare 限流（请求过密）。脚本会自动退避；也可加大 `--interval` |
| `未设置 DEEPSEEK_API_KEY` | `export DEEPSEEK_API_KEY=sk-...` 后重跑 |
| DeepSeek 401 | API key 无效或余额不足，去 platform.deepseek.com 检查 |
| `读不到 Claude Code 凭据` | 仅 macOS 支持；确认已用 Claude Code 登录过（Keychain 里有 `Claude Code-credentials`） |
| Claude 401 | Claude OAuth token 过期，终端跑一次 `claude` 刷新 |
| 扫不到设备 | 确认固件已烧入且上电、串口没有刷屏报错；设备广播名是 `ESP32-Pulsar` |
| 屏幕仍显示 `Waiting for BLE` / `--` | 用 `--dry-run` 看 payload 是否合法；确认已连接成功 |
| 想换 endpoint | 环境变量 `PULSAR_USAGE_URL` / `PULSAR_DEEPSEEK_URL` |
