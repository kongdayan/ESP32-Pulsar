# 主机侧单元测试与覆盖率（不需要真实硬件）
#
#   make test        编译并运行全部单元测试（make test RUN=<名字子串> 可过滤）
#   make coverage    生成 llvm-cov 行/函数覆盖率报告（默认要求 ≥90% 行覆盖）
#   make coverage-html  输出可视化 HTML 到 build/coverage-html
#   make check-coverage 覆盖率不达标时以非零码退出（CI 用）
#   make clean       清理 build/
#
# 依赖：clang (Apple/LLVM) + xcrun llvm-cov；LVGL 源码来自 PlatformIO 的 libdeps。

ROOT        := $(abspath ..)
BUILD       := build
OBJ         := $(BUILD)/obj
LVGL_SRC    := $(firstword $(wildcard $(ROOT)/.pio/libdeps/*/lvgl))

ifeq ($(LVGL_SRC),)
$(error 找不到 LVGL 源码，请先在项目根目录执行 `pio run` 拉取依赖)
endif

# make 自带 CC=cc / CXX=g++，用 ?= 覆盖不了；只有用户显式传入时才尊重其选择。
# 覆盖率插桩用的是 LLVM 的 -fprofile-instr-generate，GCC 不认，所以必须 clang。
CC  := $(if $(filter-out default,$(origin CC)),$(CC),clang)
CXX := $(if $(filter-out default,$(origin CXX)),$(CXX),clang++)

ifeq ($(shell command -v $(CC) >/dev/null 2>&1 && echo yes),)
$(error 找不到编译器 `$(CC)`：覆盖率插桩需要 clang（macOS 装 Xcode CLT，Linux `apt install clang llvm`），或显式 make CC=...)
endif

# macOS 上 llvm 工具要通过 xcrun 找；Linux 直接用 PATH 里的 llvm-profdata / llvm-cov（含带版本号的包名）
XCRUN := $(shell command -v xcrun >/dev/null 2>&1 && echo xcrun)

LLVM_TOOL_NAMES := llvm-profdata llvm-profdata-21 llvm-profdata-20 llvm-profdata-19 llvm-profdata-18 llvm-profdata-17 llvm-profdata-16
LLVM_COV_NAMES  := llvm-cov llvm-cov-21 llvm-cov-20 llvm-cov-19 llvm-cov-18 llvm-cov-17 llvm-cov-16
find_first = $(firstword $(foreach c,$(1),$(if $(shell command -v $(c) 2>/dev/null),$(c))))

ifeq ($(XCRUN),xcrun)
PROFDATA := xcrun llvm-profdata
LLVMCOV  := xcrun llvm-cov
else
PROFDATA := $(call find_first,$(LLVM_TOOL_NAMES))
LLVMCOV  := $(call find_first,$(LLVM_COV_NAMES))
ifeq ($(PROFDATA),)
$(error 找不到 llvm-profdata：Linux 请 `apt install clang llvm`（或 brew install llvm）)
endif
ifeq ($(LLVMCOV),)
$(error 找不到 llvm-cov：Linux 请 `apt install clang llvm`（或 brew install llvm）)
endif
endif
COV_FLAGS   := -fprofile-instr-generate -fcoverage-mapping
COMMON_WARN := -Wall -Wextra -Wshadow -Wundef -Wpointer-arith -Wcast-align -Wwrite-strings \
               -Wmissing-prototypes -Wstrict-prototypes -Wno-unused-parameter \
               -Wno-cast-align -Wno-padded

# 被测代码（.c）与 C++ 入口
APP_C_SRCS  := $(wildcard $(ROOT)/core/*.c $(ROOT)/common/*.c $(ROOT)/screens/*.c $(ROOT)/ui/ui.c $(ROOT)/ui/ui_helpers.c)
APP_CPP_SRCS:= $(ROOT)/main.cpp
TEST_C_SRCS := $(wildcard unit/*.c support/*.c framework/*.c reference/*.c lvgl/*.c)
TEST_CPP_SRCS := $(wildcard lvgl/*.cpp)

# 静态数据文件（不参与覆盖率统计）
APP_DATA_C  := $(ROOT)/ui/ui_img_1539399133.c

INCLUDES    := -I$(BUILD)/gen -Isupport/stubs -Isupport -Iframework -Ireference -Ilvgl \
               -I$(ROOT)/core -I$(ROOT)/common -I$(ROOT)/screens -I$(ROOT)/ui -I$(ROOT)/hal \
               -I$(LVGL_SRC)

DEFINES     := -DLV_CONF_INCLUDE_SIMPLE

DEPFLAGS    := -MMD -MP
HOST_CFLAGS   := -O0 -g $(COMMON_WARN) $(INCLUDES) $(DEFINES) $(DEPFLAGS)
HOST_CXXFLAGS := -O0 -g -Wno-write-strings $(INCLUDES) $(DEFINES) $(DEPFLAGS)
COVERAGE_SOURCES := $(APP_C_SRCS) $(APP_CPP_SRCS)

RUNNER := $(BUILD)/run_tests

# 只跑名字包含该子串的用例：make test RUN=video
RUN ?=

.PHONY: all test coverage coverage-html coverage-json check-coverage clean gen

all: test

gen: $(BUILD)/gen/lv_conf.h $(BUILD)/gen/host_tick.h

$(BUILD)/gen/lv_conf.h: $(ROOT)/lv_conf.h Makefile
	@mkdir -p $(BUILD)/gen
	@sed -e 's|#define LV_TICK_CUSTOM_INCLUDE "Arduino.h".*|#define LV_TICK_CUSTOM_INCLUDE "host_tick.h"|' \
	     -e 's|#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis()).*|#define LV_TICK_CUSTOM_SYS_TIME_EXPR (host_millis())|' \
	     $(ROOT)/lv_conf.h > $@
	@grep -q 'LV_TICK_CUSTOM_INCLUDE "host_tick.h"' $@ || (echo "生成 host lv_conf.h 失败"; exit 1)

$(BUILD)/gen/host_tick.h: support/host_tick.h
	@mkdir -p $(BUILD)/gen
	@cp support/host_tick.h $@

# ── LVGL 本体（不插桩，加快编译并保持覆盖率报告干净） ──────────────────────
LVGL_SRCS := $(shell find $(LVGL_SRC)/src -name '*.c')
LVGL_OBJS := $(patsubst $(ROOT)/%.c,$(OBJ)/lvgl_%.o,$(LVGL_SRCS))

$(OBJ)/lvgl_%.o: $(ROOT)/%.c | gen
	@mkdir -p $(dir $@)
	$(QUIET_CC)$(CC) $(HOST_CFLAGS) -w -c $< -o $@

# ── 被测代码（插桩） ────────────────────────────────────────────────────────
$(OBJ)/app_%.o: $(ROOT)/%.c | gen
	@mkdir -p $(dir $@)
	$(QUIET_CC)$(CC) $(HOST_CFLAGS) $(COV_FLAGS) -c $< -o $@

$(OBJ)/app_main.o: $(ROOT)/main.cpp | gen
	@mkdir -p $(dir $@)
	$(QUIET_CXX)$(CXX) $(HOST_CXXFLAGS) $(COV_FLAGS) -c $< -o $@

$(OBJ)/data_%.o: $(ROOT)/%.c | gen
	@mkdir -p $(dir $@)
	$(QUIET_CC)$(CC) $(HOST_CFLAGS) -w -c $< -o $@

# ── 测试与夹具（不插桩） ────────────────────────────────────────────────────
$(OBJ)/test_%.o: %.c | gen
	@mkdir -p $(dir $@)
	$(QUIET_CC)$(CC) $(HOST_CFLAGS) -c $< -o $@

$(OBJ)/test_%.o: %.cpp | gen
	@mkdir -p $(dir $@)
	$(QUIET_CXX)$(CXX) $(HOST_CXXFLAGS) -c $< -o $@

APP_OBJS   := $(patsubst $(ROOT)/%.c,$(OBJ)/app_%.o,$(APP_C_SRCS)) $(OBJ)/app_main.o
DATA_OBJS  := $(patsubst $(ROOT)/%.c,$(OBJ)/data_%.o,$(APP_DATA_C))
TEST_OBJS  := $(patsubst %.c,$(OBJ)/test_%.o,$(TEST_C_SRCS)) $(patsubst %.cpp,$(OBJ)/test_%.o,$(TEST_CPP_SRCS))

$(RUNNER): $(LVGL_OBJS) $(APP_OBJS) $(DATA_OBJS) $(TEST_OBJS)
	@mkdir -p $(BUILD)
	$(QUIET_LINK)$(CXX) $(COV_FLAGS) $^ -o $@ -lm

test: $(RUNNER)
	@mkdir -p $(BUILD)/sdcard $(BUILD)/prof
	@rm -f $(BUILD)/prof/*.profraw
	@LLVM_PROFILE_FILE="$(BUILD)/prof/test-%p.profraw" ./$(RUNNER) $(RUN)

# ── 覆盖率 ──────────────────────────────────────────────────────────────────
$(BUILD)/coverage.profdata: $(RUNNER)
	@mkdir -p $(BUILD) $(BUILD)/prof
	@rm -f $(BUILD)/prof/*.profraw
	@LLVM_PROFILE_FILE="$(BUILD)/prof/cov-%p.profraw" ./$(RUNNER) >/dev/null
	@$(PROFDATA) merge -sparse $(BUILD)/prof/*.profraw -o $@

coverage: $(BUILD)/coverage.profdata
	@$(LLVMCOV) report $(RUNNER) -instr-profile=$(BUILD)/coverage.profdata $(COVERAGE_SOURCES)

coverage-json: $(BUILD)/coverage.profdata
	@$(MAKE) --no-print-directory check-coverage

coverage-html: $(BUILD)/coverage.profdata
	@mkdir -p $(BUILD)/coverage-html
	@$(LLVMCOV) show $(RUNNER) -instr-profile=$(BUILD)/coverage.profdata --format=html \
	    --output-dir=$(BUILD)/coverage-html $(COVERAGE_SOURCES)
	@echo "HTML 报告: $(BUILD)/coverage-html/index.html"

MIN_LINE_COVERAGE ?= 90

check-coverage: $(BUILD)/coverage.profdata
	@$(LLVMCOV) export $(RUNNER) -instr-profile=$(BUILD)/coverage.profdata \
	    -format=text --summary-only $(COVERAGE_SOURCES) > $(BUILD)/coverage.json
	@python3 tools/coverage_summary.py $(BUILD)/coverage.json $(MIN_LINE_COVERAGE)

clean:
	@rm -rf $(BUILD)
	@echo "cleaned"

QUIET_CC   ?= @
QUIET_CXX  ?= @
QUIET_LINK ?= @

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
