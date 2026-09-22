#pragma once

#include <string>

#include "cli.h"

namespace poly {

// 原生 Win32 窗口版出题工作台（Windows）。非 Windows 平台返回错误码。
int run_win32_ui(Context& ctx);

}  // namespace poly
