UNAME_S  := $(shell uname -s 2>/dev/null)
IS_WIN   := $(if $(filter MINGW% MSYS% CYGWIN% Windows_NT,$(UNAME_S) $(OS)),1,)
EXE      :=
ifeq ($(IS_WIN),1)
EXE      := .exe

# --- Windows / MSYS2 environment fixes -------------------------------------
# 1. Native binaries (the assembler in particular) need TMP/TEMP to be real
#    Windows paths; GNU make strips them from the inherited environment.
# 2. Two MSYS2 prefixes on PATH at once break the compiler process silently,
#    so prefer a prefix that demonstrably compiles something.
POLY_TMP := $(shell cygpath -m /tmp 2>/dev/null)
ifeq ($(POLY_TMP),)
POLY_TMP := $(shell cd /tmp 2>/dev/null && pwd -W 2>/dev/null)
endif
ifeq ($(POLY_TMP),)
POLY_TMP := $(CURDIR)
endif
export TMP  := $(POLY_TMP)
export TEMP := $(POLY_TMP)

POLY_CXX := $(shell sh tools/pick-toolchain.sh 2>/dev/null)
ifneq ($(POLY_CXX),)
CXX := $(POLY_CXX)
endif
export PATH := $(dir $(POLY_CXX)):$(PATH)
endif

CXX      ?= g++
PYTHON   ?= $(shell command -v python 2>/dev/null || command -v python3 2>/dev/null)
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -MMD -MP
LDFLAGS  ?=

SRCDIR   := src
BUILDDIR := build
TARGET   := bin/poly$(EXE)
GUI_TARGET := bin/poly-gui$(EXE)

SRCS := $(wildcard $(SRCDIR)/*.cpp)
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)
# 两个入口各自只链自己的 main：控制台版排除 gui_main.o，窗口版排除 main.o
CONSOLE_OBJS := $(filter-out $(BUILDDIR)/gui_main.o,$(OBJS))
GUI_OBJS     := $(filter-out $(BUILDDIR)/main.o,$(OBJS))
EMBED := $(SRCDIR)/testlib_embed.inc

# 图标 / 版本信息（assets/*.rc 用 windres 编译）；windres 不在时自动跳过，仍能构建
CONSOLE_RES :=
GUI_RES     :=
ifeq ($(IS_WIN),1)
WINDRES := $(shell command -v windres 2>/dev/null)
ifneq ($(WINDRES),)
CONSOLE_RES := $(BUILDDIR)/poly_res.o
GUI_RES     := $(BUILDDIR)/poly-gui_res.o
endif
endif

ifeq ($(IS_WIN),1)
# ws2_32：poly ui --web 的本地 HTTP 服务（Winsock）
# gdi32/comctl32/comdlg32：窗口版界面（控制台版也链它，因为 poly ui 可以就地开窗）
LDFLAGS += -static -static-libgcc -static-libstdc++ -lws2_32 -lgdi32 -lcomctl32 -lcomdlg32
endif

.PHONY: all clean distclean env
ifeq ($(IS_WIN),1)
all: $(TARGET) $(GUI_TARGET)
else
all: $(TARGET)
endif

$(TARGET): $(CONSOLE_OBJS) $(CONSOLE_RES)
	@mkdir -p $(dir $@)
	$(CXX) $(CONSOLE_OBJS) $(CONSOLE_RES) -o $@ $(LDFLAGS)
	@echo "built $@"

# 窗口版（仅 Windows）：GUI 子系统，运行时不弹控制台窗口
ifeq ($(IS_WIN),1)
$(GUI_TARGET): $(GUI_OBJS) $(GUI_RES)
	@mkdir -p $(dir $@)
	$(CXX) $(GUI_OBJS) $(GUI_RES) -o $@ -mwindows $(LDFLAGS)
	@echo "built $@"
endif

# 资源文件：中文串必须 --codepage=65001，否则按 ANSI 读成乱码
$(BUILDDIR)/%_res.o: assets/%.rc assets/icon.ico
	@mkdir -p $(dir $@)
	$(WINDRES) --codepage=65001 -i $< -o $@
	@echo "built $@"

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# testlib.h is embedded into the binary; failure here is not fatal because the
# tool falls back to reading testlib/testlib.h from disk at runtime.
$(EMBED): testlib/testlib.h tools/embed.py
	-@if [ -n "$(PYTHON)" ]; then $(PYTHON) tools/embed.py $< $@; \
	  else echo "note: python not found, skipping testlib embedding"; fi

$(OBJS): | $(EMBED)

env:
	@echo "IS_WIN   = $(IS_WIN)"
	@echo "TMP      = $(POLY_TMP)"
	@echo "POLY_BIN = $(POLY_BIN)"
	@echo "CXX      = $(CXX)"

clean:
	rm -rf $(BUILDDIR)

distclean: clean
	rm -rf bin

-include $(DEPS)