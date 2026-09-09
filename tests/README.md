# tests/ — 主机侧单元测试与覆盖率

不需要烧录、不需要硬件：把固件里的 UI 代码（真实的 LVGL 8.3 源码 + 本项目的
`screens/ common/ core/ ui/ main.cpp`）在 PC 上编译成一个可执行测试程序运行。

```bash
pio run                       # 只需一次：让 .pio/libdeps/*/lvgl 存在
make -C tests test             # 跑全部用例（可选 RUN=<名字子串> 过滤）
make -C tests coverage         # 行/函数/分支覆盖率
make -C tests coverage-html    # tests/build/coverage-html/index.html
make -C tests check-coverage   # 行覆盖率 < MIN_LINE_COVERAGE(默认90) 时非零退出
make -C tests preview          # 离线渲染表盘切割方案到 tests/build/preview/*.bmp
make -C tests clean
```

本机依赖：`clang` + `llvm-profdata` / `llvm-cov`（macOS 上通过 `xcrun` 自动取得）。
CI 见 `.github/workflows/pr-check.yml` 的 `unit-tests` job。

## 替身（fake）怎么做

| 真实依赖 | 主机替身 | 位置 |
|---|---|---|
| `hal/display.cpp`（QSPI 屏、CST816S、LEDC 背光、中断） | 记录调用次数的空实现 | `support/host_stubs.c` |
| `hal/sd_card.cpp`（SD_MMC 挂载） | 可由测试设定"是否挂载" | 同上 |
| `Arduino.h`（`delay/millis/Serial/pinMode/analogRead`） | 头文件桩 + 可控时钟 | `support/stubs/Arduino.h` |
| `esp_heap_caps.h`（PSRAM 分配） | `malloc` + 可注入分配失败 | `support/heap_caps_stub.c` |
| 物理屏幕 360×360 | LVGL 软件渲染到内存帧缓冲，可逐像素断言 | `support/lv_host.c` |
| 电容触摸 | 指针式 indev，可 `touch_down/swipe` 造真实手势 | 同上 |
| LVGL `lv_conf.h` 的 `LV_TICK_CUSTOM_INCLUDE "Arduino.h"` | sed 生成 host 版，时钟由测试推进 | `build/gen/lv_conf.h` |

因为跑的是真 LVGL，所以绘制回调、样式、布局、事件冒泡、动画、内存池都在测试范围内
——这也是能断言"这一屏不是黑屏"的前提。

## 三层测试

1. **`unit/`（纯逻辑）** — `core/*`、`common/*` 的算法与边界：角度/夹取、主题表、
   导航表、息屏与背光策略、旋转坐标映射、视频文件读取、点阵字形、弧度盘增量。
2. **`reference/` + `unit/test_legacy_equivalence.c`（差分测试）** —
   把重构**前**的 `screen_agent.c` / `screen_3dmodel.c` 数学逐字抄成
   `legacy_hexball.c` / `legacy_cube3d.c`，在相同输入（含相同伪随机序列）下逐帧
   比对位置、速度、环半径/缺口、得分、投影顶点、 painter's order、惯性衰减。
   这是"行为不变"的机器证明，不插桩、不计入覆盖率。
3. **`lvgl/`（真 LVGL）** — 每一屏建屏 + 渲染 + 像素断言、真实滑动手势按导航表切屏、
   首页 +/- 按钮、3D 屏边缘滑动与缩放弧、Codex 盘上下滑切主题、TF 卡视频播放与
   读取失败降级、`ui/ui_helpers.c` 全量分支、`main.cpp` 的 `setup()/loop()` 装配顺序。

## 覆盖率口径

统计范围（`make coverage` 的分母）：

```
core/*.c  common/*.c  screens/*.c  ui/ui.c  ui/ui_helpers.c  main.cpp
```

**不计入**（并说明原因）：

| 排除项 | 原因 | 补偿手段 |
|---|---|---|
| `hal/display.cpp` | 直接操作 QSPI/LEDC/中断/ESP32_Display_Panel，主机无法实例化 | 可判定逻辑已抽到 `core/power_mgmt.c`（息屏超时、夜间唤醒策略）与 `core/display_rotation.c`（旋转+触摸重映射），两者 100% 行覆盖；`display_power_tick()` 的调用链由 `test_app_entry.cpp` 与 `host_stubs` 计数验证 |
| `hal/sd_card.cpp` | 依赖 SD_MMC 外设 | 挂载状态改为桩注入；文件解析/播放状态机全在 `core/video_source.c`，100% 行覆盖 |
| `ui/ui_img_*.c`、`ui/` 其余生成文件 | SquareLine 生成的资源表与壳 | 由 `test_screen_render.c` 间接验证（图片能画出像素） |
| LVGL 库本体（`.pio/libdeps`） | 第三方 | 以 `-w` 编译、不插桩 |
| `tests/` 自身、`ESP32-Pulsar.ino` | 测试代码 / Arduino IDE 空壳 | — |

结果（`make check-coverage` 会打印同一份数字）：

```
line     99.58%  (1653/1660)
function 100.00%  (215/215)
branch   90.85%   (665/732)
```

剩下 7 行未覆盖全部是重构前就存在的死分支：`screen_agent.c` 与 `screen_3dmodel.c`
里挂在游戏图层上的 `LV_EVENT_GESTURE` 处理 —— LVGL 只会把手势事件发给冒泡根
（屏幕对象），所以这两段代码在旧版同样收不到事件。`test_agent_screen_keeps_legacy_gesture_quirk`
把"滑屏不出屏"这个旧版行为钉死，避免以后被无声改掉。

## 表盘切割方案离线预览（非测试）

`tests/preview/preview_dials.c` 用同一套主机侧 LVGL 把几套圆屏排版方案
（单环进度 / 三层同心环 / 24 小时环 / 周×时双层）渲染成图片，方便先看效果再落代码。
所有几何都走 `core/dial_layout.h`，不手写绝对坐标：

```bash
make -C tests preview           # -> tests/build/preview/*.bmp
make -C tests preview RUN=C     # 只渲染名字含 "C" 的方案
make -C tests preview-PNG       # 再转 PNG（需要 macOS sips 或 ImageMagick）
```

它自带 `main()`，单独链接，**不参与覆盖率、不跑在 CI 上**；改动时至少手动跑一次
`make -C tests preview` 确认能出图。

## 加一个用例
```c
/* tests/unit/test_xxx.c 或 tests/lvgl/test_xxx.c，Makefile 自动 glob */
#include "minitest.h"

MT_TEST(test_my_thing)
{
    CHECK_EQ(2 + 2, 4);
    CHECK_NEAR(0.1 + 0.2, 0.3, 1e-6);
    CHECK_STR_EQ("a", "a");
}
```

新增不参与固件构建的目录时，记得同步 `tests/Makefile` 的 `TEST_C_SRCS` glob 与
`COVERAGE_SOURCES`（默认按 `core/ common/ screens/ ui/*.c main.cpp` 收集）。
