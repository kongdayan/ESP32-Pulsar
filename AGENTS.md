# Agent Instructions — ESP32-Pulsar

ESP32-S3 嵌入式固件项目，使用 **PlatformIO + Arduino + LVGL 8.3**。
目标硬件：ST77916 QSPI 圆形屏（360×360）+ CST816S 电容触摸 + TF 卡 + I2S 音频/麦克风。

---

## 一、快速上手

### 环境依赖

| 工具 | 版本要求 | 安装方式 |
|------|---------|---------|
| Python | ≥ 3.10 | [python.org](https://python.org) |
| PlatformIO CLI | 最新 | `pip install platformio` |
| esptool.py | 随 PlatformIO 自动安装 | — |
| clang / llvm-profdata | 跑主机单元测试时需要 | macOS 自带 `xcrun`；Linux `apt install clang llvm` |

> VS Code 用户安装 PlatformIO IDE 插件可直接使用图形界面。

---

## 二、编译链命令

### 常用命令速查

| 操作 | 命令 |
|------|------|
| 编译 | `pio run` |
| 编译 + 烧录 | `pio run --target upload` |
| 串口监视器（115200） | `pio device monitor` |
| 编译 + 烧录 + 监视器 | `pio run --target upload && pio device monitor` |
| 清除构建缓存 | `pio run --target clean` |
| 查看连接设备 | `pio device list` |
| 详细编译日志 | `pio run -v` |
| 查看固件大小 | `pio run --target size` |
| **主机单元测试** | `make -C tests test` |
| **只看某个用例** | `make -C tests test RUN=video` |
| **覆盖率报告** | `make -C tests coverage` |
| **覆盖率 HTML** | `make -C tests coverage-html` |
| **覆盖率门禁（≥90%）** | `make -C tests check-coverage` |
| **离线预览表盘切割方案** | `make -C tests preview`（产物在 `tests/build/preview/`） |

> **规则 1**：修改任何 `.c` / `.cpp` / `.h` 文件后，必须用 `pio run` 验证编译通过，再提交。
> **规则 2**：改动 `core/`、`common/`、`screens/`、`ui/`、`main.cpp` 中任何一个 `.c/.cpp`
> 之后，还必须跑 `make -C tests test`（涉及逻辑改动时再加 `make -C tests check-coverage`）。
> 纯常量头（`*_layout.h`、`app_config.h`、`pincfg.h`）只需 `pio run`。

### 手动烧录（esptool）

仅烧录应用分区：
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash 0x10000 .pio/build/esp32s3/firmware.bin
```

完整烧录（bootloader + 分区表 + 应用）：
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
  write_flash \
  0x0000  .pio/build/esp32s3/bootloader.bin \
  0x8000  .pio/build/esp32s3/partitions.bin \
  0xe000  ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin \
  0x10000 .pio/build/esp32s3/firmware.bin
```

> 若设备不能自动进入下载模式，按住 **BOOT** 键再插 USB，进入下载模式后松开。

### 固件产物

```
.pio/build/esp32s3/
  firmware.bin      ← 应用分区（烧录到 0x10000）
  firmware.elf      ← 含调试符号，用于 GDB / 崩溃地址反解
  bootloader.bin    ← Bootloader
  partitions.bin    ← 分区表
```

---

## 三、视频素材准备

`screen_video.c` 从 TF 卡读取原始 RGB565 视频文件：

```bash
# 用 ffmpeg 将任意视频转为 360×360 RGB565 原始格式
ffmpeg -i input.mp4 \
  -vf "scale=360:360:force_original_aspect_ratio=increase,crop=360:360" \
  -vcodec rawvideo -pix_fmt rgb565be \
  -f rawvideo video.rgb
```

将 `video.rgb` 复制到 TF 卡根目录，文件名固定为 `video.rgb`
（由 `SD_CARD_MOUNT_POINT` + `VIDEO_FILE_NAME` 拼出，见 `core/app_config.h`）。
每帧大小：`360 × 360 × 2 = 259200 bytes`（= `VIDEO_FRAME_BYTES`），帧率约 24fps（42ms/帧）。

> `video.rgb` / `*.mp4` 已加入 `.gitignore`，不要提交这类运行时素材。

---

## 四、项目目录结构

```
ESP32-Pulsar/
├── main.cpp                  ← Arduino 入口：只做装配（setup 顺序 / loop 心跳）
├── platformio.ini            ← 构建配置（平台、库依赖、编译 filter）
├── lv_conf.h                 ← LVGL 功能开关（只开启项目用到的控件）
├── ESP32-Pulsar.ino          ← Arduino IDE 兼容入口（不用于 PlatformIO 构建）
│
├── core/                     ← 纯逻辑层：不依赖 LVGL / Arduino / HAL，可主机单测
│   ├── app_config.h          ← 全局常量：屏幕尺寸、动画时长、路径、主题角色色
│   ├── ui_math.h/.c          ← 角度/夹取/百分比映射等公共数学工具
│   ├── ui_theme.c/.h         ← 深浅色主题调色板表（按"角色"取色，不写死 0xRRGGBB）
│   ├── nav_map.c/.h          ← 屏幕导航拓扑表（左滑/右滑 → 目标屏 + 动画）
│   ├── power_mgmt.c/.h       ← 息屏超时、昼夜亮度策略（纯计算）
│   ├── display_rotation.c/.h ← 屏幕旋转 + 触摸坐标重映射（纯计算）
│   ├── hexball.c/.h          ← Hex-Ball 小游戏的物理/计分/拖拽（含随机数注入）
│   ├── cube3d.c/.h           ← 3D 立方体投影、命中区、惯性（含可配置 params）
│   ├── watchface.c/.h        ← 点阵字形、百分比/电量格式化
│   ├── dial_layout.c/.h      ← 圆屏极坐标切割模型（环带/分格/命中/排版可用性，纯几何）
│   └── video_source.c/.h     ← TF 卡 RGB565 文件读取状态机（与 LVGL 无关）
│
├── common/                   ← 跨屏公共设施（仍依赖 LVGL）
│   └── ui_screen.c/.h        ← 屏幕根对象工厂、懒加载、导航事件、控件工厂、定时器生命周期
│
├── hal/                      ← 硬件抽象层（唯一允许直接操作外设的地方）
│   ├── pincfg.h              ← 全部 GPIO 宏定义（只放引脚，不放业务常量）
│   ├── display.cpp/.h        ← 屏/触摸/LVGL 驱动初始化；只保留"执行"，决策逻辑在 core/
│   └── sd_card.cpp/.h        ← SD 卡（SDMMC 4-bit）挂载，挂载点由 SD_CARD_MOUNT_POINT 定义
│
├── screens/                  ← 每屏一个 .c/.h 对 + 一个 *_layout.h 常量头
│   ├── screen_dashboard.c/.h + dashboard_layout.h
│   ├── screen_info.c/.h      + info_layout.h
│   ├── screen_image.c/.h     + image_layout.h
│   ├── screen_video.c/.h     + video_layout.h
│   ├── screen_about.c/.h     + about_layout.h
│   ├── screen_agent.c/.h     + agent_layout.h       ← Hex-Ball
│   ├── screen_3dmodel.c/.h   + model3d_layout.h     ← 3D 模型
│   └── screen_codex_usage.c/.h + codex_usage_layout.h ← Codex 使用量 Watch Face
│
├── ui/                       ← LVGL 基础层（SquareLine Studio 生成，尽量不手改）
│   ├── ui.c / ui.h           ← 主题初始化、开机首屏、全局 screen 声明
│   ├── ui_helpers.c / .h     ← _ui_screen_change()、_ui_arc_increment() 工具函数
│   └── ui_img_1539399133.c   ← 嵌入式图片资源（LV_IMG_DECLARE）
│
├── tests/                    ← 主机侧单元测试（详见 tests/README.md）
│   ├── Makefile              ← make test / coverage / check-coverage / preview / clean
│   ├── framework/minitest.*  ← 零依赖断言框架
│   ├── unit/                 ← core/、common/ 的纯逻辑用例
│   ├── lvgl/                 ← 真 LVGL 环境下的渲染/导航/交互用例
│   ├── support/              ← 替身：hal、Arduino、heap_caps、虚拟屏幕与触摸
│   ├── preview/              ← 离线渲染表盘切割方案（make preview，非 CI）
│   ├── reference/            ← 重构前实现的逐字副本，供差分测试比对（不计覆盖率）
│   └── tools/                ← 覆盖率汇总脚本（CI 门禁用）
│
├── assets/                   ← 设计原图（PNG），不编译进固件
├── example/                  ← 效果截图（dark/light 主题对比图）
├── test/                     ← 测试用真机照片
├── backup/                   ← SquareLine Studio 项目备份（.zip）
├── cache/                    ← SquareLine Studio 缩略图缓存（可忽略）
│
└── .github/
    ├── PULL_REQUEST_TEMPLATE.md  ← PR 中文模版（自动填充）
    └── workflows/
        ├── build.yml         ← push/PR 自动编译，产物保留 30 天
        ├── pr-check.yml      ← PR 检查：lint + 单元测试/覆盖率 + 密钥扫描 + 固件大小评论
        └── release.yml       ← 发布 Release 时编译固件并上传 zip + sha256
```

### 分层规则（谁可以依赖谁）

```
core/     →  只依赖标准库（无 LVGL / 无 Arduino / 无 hal）
common/   →  core/ + LVGL + ui/ui.h（屏幕对象声明）
screens/  →  core/ + common/ + LVGL（视图装配，不做纯计算）
hal/      →  Arduino + 外设库（决策逻辑一律下沉到 core/）
ui/       →  SquareLine 生成层，除 ui.c 外不手改
```

---

## 五、硬件平台参数

| 参数 | 值 |
|------|----|
| SoC | ESP32-S3（Xtensa LX7，240 MHz 双核） |
| Flash | 16 MB（QIO） |
| PSRAM | 8 MB（OPI） |
| 显示 | ST77916，QSPI 4-bit，360×360，圆形 |
| 触摸 | CST816S，I2C |
| 存储 | TF 卡（SDMMC 4-bit，最高 40 MHz） |
| 音频输出 | I2S DAC |
| 音频输入 | I2S MEMS 麦克风 |
| PlatformIO platform | pioarduino `53.03.11`（Arduino ESP32 3.1.1 / IDF 5.3） |
| 上传波特率 | 921600 |
| 串口波特率 | 115200 |

### GPIO 引脚一览

| 功能 | GPIO |
|------|------|
| 背光 PWM（LEDC） | 15 |
| 屏幕 RST | 47 |
| 屏幕 CS | 10 |
| 屏幕 SCK | 9 |
| 屏幕 DATA0–3（QSPI） | 11、12、13、14 |
| 触摸 SCL | 8 |
| 触摸 SDA | 7 |
| 触摸 INT | 41 |
| 触摸 RST | 40 |
| 按钮（BOOT） | 0 |
| SD D0–D3 | 2、1、6、5 |
| SD CLK | 3 |
| SD CMD | 4 |
| I2S BCK | 18 |
| I2S WS（LCK） | 16 |
| I2S DO（DIN） | 17 |
| 音频静音（低有效） | 48 |
| 麦克风 WS | 45 |
| 麦克风 SD | 46 |
| 麦克风 SCK | 42 |

> 宏定义见 `hal/pincfg.h`；屏幕尺寸/动画时长/路径/帧率等**非引脚**常量在 `core/app_config.h`。

---

## 六、屏幕导航架构

所有屏幕采用**懒加载**模式，`lv_obj_t *` 首次访问前为 `NULL`。

```
ui_init()
  └─ 创建主题 + 加载开机首屏（ui/ui.c: UI_STARTUP_SCREEN）
        ↓ 用户滑动 / 点击
  screen_xxx_get_ptr() → NULL ？→ screen_xxx_init()（内部调用 ui_screen_create）
        ↓
  _ui_screen_change() → lv_scr_load_anim()
```

每个屏幕文件对外暴露两个函数（签名不变）：

```c
void       screen_xxx_init(void);      // 创建所有 LVGL 控件
lv_obj_t **screen_xxx_get_ptr(void);   // 返回指向内部 scr 指针的地址
```

### 导航表在 core/nav_map.c，不写在屏幕里

旧版把"左滑去哪/右滑去哪/用什么动画"硬编码在每个屏幕的 `on_gesture()` 里。
现在统一由一张表描述，屏幕侧只需要 `ui_screen_create(NAV_SCREEN_XXX)`：

```c
[NAV_SCREEN_VIDEO] = { NAV_MOVE(NAV_SCREEN_ABOUT),  /* LEFT  */
                       NAV_MOVE(NAV_SCREEN_IMAGE),  /* RIGHT */
                       "video" },
```

- `NAV_MOVE` = 带 `APP_NAV_ANIM_MS` 的位移动画；`NAV_JUMP` = 无动画直切。
- 通用滑屏由 `common/ui_screen.c` 的 `ui_nav_event_cb` 处理（挂在屏幕根对象上）。
- **有自定义输入的屏**（agent 六边形小游戏、3dmodel 立方体、codex_usage 上下滑切主题）
  用 `ui_screen_create_ex(id, false)` 关掉通用回调，自己在 `on_input/on_gesture` 里
  调 `ui_nav_go(id, dir)`；注意 LVGL 的 `LV_EVENT_GESTURE` 只会发给**冒泡根**（屏幕对象），
  挂在子图层上的手势分支是收不到事件的（agent 屏即如此，重构保留了旧行为，
  并由 `test_agent_screen_keeps_legacy_gesture_quirk` 钉死）。

切换示例（直接用 LVGL helper，行为与旧版一致）：

```c
_ui_screen_change(screen_info_get_ptr(), LV_SCR_LOAD_ANIM_MOVE_LEFT,
                  APP_NAV_ANIM_MS, APP_NAV_ANIM_DELAY_MS, screen_info_init);
```

---

## 七、添加新屏幕（标准流程）

1. 在 `core/nav_map.h` 的 `nav_screen_id_t` 里加一个 id（放在 `NAV_SCREEN_COUNT` 之前）。
2. 在 `core/nav_map.c` 的 `k_nav_table` 里补上该屏的 LEFT/RIGHT 去向与动画类型；
   用**指定初始化器**，行下标即 id。
3. 在 `screens/` 创建 `screen_foo.c`、`screen_foo.h` 与 `foo_layout.h`：
   - `.h` 声明 `screen_foo_init()` / `screen_foo_get_ptr()`；
   - `.c` 内部维护 `static lv_obj_t *scr = NULL;`，用 `ui_screen_create(NAV_SCREEN_FOO)`
     建根对象，尺寸/颜色/文案全部来自 `foo_layout.h`；
   - `.c` 里需要 `#include "ui.h"`（拿全局 `screen_*` 声明）+ `"ui_screen.h"` + `"foo_layout.h"`。
4. 在 `ui/ui.h` 中声明该屏的两个函数，并在 `ui/ui.c` 的全局对象列表里加一行
   `lv_obj_t *screen_foo = NULL;`。
5. 在 `common/ui_screen.c` 的 `k_screen_refs[]` 中登记（`[NAV_SCREEN_FOO] = {...}`），
   这样导航表才能把它建出来。
6. 让某个已有屏幕能进入新屏：在该屏的 `*_layout.h` 加坐标/文案常量，在 `.c` 里加按钮，
   点击回调调用 `_ui_screen_change(screen_foo_get_ptr(), ...)`。
7. 在 `platformio.ini` 的 `build_src_filter` 中追加：
   ```ini
   +<screens/screen_foo.c>
   ```
8. `pio run` 验证编译；补两个用例：`tests/unit/` 里的常量一致性检查 +
   `tests/lvgl/test_screen_render.c` 会自动把新屏纳入"非黑屏"断言（把新屏加进
   `k_screens[]` 即可），导航链路则由 `test_nav_flow.c` 自动覆盖。
9. 跑 `make -C tests test && make -C tests check-coverage`。

---

## 八、CI / CD 工作流

| Workflow | 触发条件 | 做什么 |
|----------|---------|--------|
| `build.yml` | push 到 main / PR 到 main | 编译，artifact 保留 30 天 |
| `pr-check.yml` | PR 到 main | ① cppcheck lint（hal/screens/core/common/main.cpp）② 主机单元测试 + 覆盖率门禁（行 ≥90%，报告存 artifact）③ Gitleaks 密钥扫描 ④ 编译并将固件大小评论到 PR |
| `release.yml` | 发布 Release | 编译，打 zip，生成 sha256，上传到 Release 附件 |

### 分支保护规则（main）

- 禁止直接 push，所有改动必须通过 PR
- 唯一合并方式：**Squash and merge**
- PR 合并后自动删除 feature branch

---

## 九、编码约定

1. **禁止函数体内出现硬编码字面量**：颜色 `0xRRGGBB`、坐标/尺寸、文案、路径、
   帧大小/帧率、超时时长、GPIO 引脚，一律提前到编译期常量：
   - 全局常量 → `core/app_config.h`
   - 引脚 → `hal/pincfg.h`
   - 单屏几何/配色/文案 → `screens/<name>_layout.h`（配色只放"该屏固定色"，可主题化的走角色）
   - 算法默认参数 → `core/*.c` 里的 `const` 表（如 `k_cube3d_default_params`）
   布局常量用 `#define`（C 与 C++ 头都要能包），运行期不变的表用 `static const` /
   `extern const`；函数内的临时不变量用 `const`，需要编译期求值用 `enum` 常量。
2. **颜色必须走主题角色**：新增颜色先在 `core/ui_theme.h` 的 `ui_color_role_t` 里加角色，
   再用 `ui_theme_color(role)`（`core/ui_theme.h`）取；不允许在屏幕里写 `lv_color_hex(0x…)`。
   （`screen_codex_usage.c` 里的 `role_color()` 是该屏自己的浅封装，会跟随它本地深浅色状态。）
3. **纯计算下沉 core/**：任何不碰 LVGL/HAL 的判断、夹取、坐标换算、状态机都写进
   `core/`，在 `tests/unit/` 里直接测；HAL 只做"执行"，不做"决策"。
4. **每屏一文件**：所有静态状态变量放在该 `.c` 文件内部，不跨文件共享。
5. **跨屏引用**：通过 `screen_xxx_get_ptr()` 获取指针，不使用全局变量。
6. **LVGL 绘制回调**：在进入实际绘制逻辑前先检查 `lv_event_get_code(e) == LV_EVENT_DRAW_POST_BEGIN`。
7. **定时器生命周期**：用 `ui_timer_attach(scr, &binding)`（`common/ui_screen.h`），
   它在 `LV_EVENT_SCREEN_LOADED` 创建、`LV_EVENT_SCREEN_UNLOADED` 删除，不要手写这两个分支。
8. **新增源文件**：必须在 `platformio.ini` 的 `build_src_filter` 中手动注册，否则不会被编译；
   若新增了顶层源码目录，还要同步 `CMakeLists.txt`（SquareLine/ESP-IDF 侧）。
9. **指针类型转换**：使用 `static_cast<>` / `reinterpret_cast<>`，禁止 C 风格 `(Type *)` 转换（会被 cppcheck lint 拦截）。
10. **`ui/` 目录**：部分文件由 SquareLine Studio 生成，不纳入 cppcheck 检查，尽量不手动修改
    （`ui/ui.c` 例外：开机首屏与主题初始化在此维护）。
11. **测试即文档**：改算法前先确认 `tests/reference/` 里的旧实现副本；改完后差分测试必须仍然通过，
    除非你有意改变行为——那种改动要在 PR 描述里写清楚。

---

## 十、常见问题排查

| 现象 | 原因 | 解决 |
|------|------|------|
| `quad_mode` 编译报错 | IDF 版本不匹配 | 确认使用 pioarduino platform，不使用官方 espressif32 |
| `setup()` 未定义 | `main.cpp` 未加入 `build_src_filter` | 在 `platformio.ini` 中追加 `+<main.cpp>` |
| 屏幕切换后黑屏 | `init_fn` 未被调用或 `get_ptr` 返回 NULL | 检查 `_ui_screen_change` 的第二个参数是否传入了 `init` 函数 |
| 触摸无响应 | 控件未设置 `LV_OBJ_FLAG_CLICKABLE` | 在 `lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE)` |
| 触摸被上层透明控件拦截 | 透明层默认可点击 | 对透明层添加 `LV_OBJ_FLAG_EVENT_BUBBLE` 或 `clear_flag(CLICKABLE)` |
| SD 卡未挂载 | 卡未插入或格式非 FAT32 | 检查 `sd_card_is_mounted()` 返回值，确认 TF 卡格式化为 FAT32 |
| 视频卡顿/花屏 | `video.rgb` 格式不正确 | 用 ffmpeg 重新转换，确认 `-pix_fmt rgb565be` 和分辨率 360×360 |
| cppcheck lint 失败 | C 风格指针转换 | 改用 `static_cast<>` / `reinterpret_cast<>` |
| Gitleaks 密钥扫描失败 | 代码中含硬编码 token/密码 | 移除敏感字符串，改用配置文件或编译宏注入 |
| `make -C tests` 报"找不到 LVGL 源码" | 还没拉取库依赖 | 先 `pio run` 一次（测试用 `.pio/libdeps/*/lvgl` 的真实源码） |
| 主机测试里手势/滑屏没反应 | 事件挂在子图层，但 `LV_EVENT_GESTURE` 只发给冒泡根 | 手势回调挂屏幕根对象，或用 `ui_nav_go()` 直接驱动 |
| 主机测试崩溃在删除屏幕 | 删除了仍是 `act_scr` 的对象 | 先 `lv_scr_load()` 到中转屏，再删（见 `tests/lvgl/test_a_interactions.c`） |
| 覆盖率报告为空 / profraw 缺失 | 上一次运行异常退出 | `make -C tests clean && make -C tests coverage` |
