#pragma once

#include <string>

#include "cli.h"

namespace poly {

// 原生 Win32 窗口版出题工作台（Windows）。非 Windows 平台返回错误码。
int run_win32_ui(Context& ctx);

// 界面里记住的工作区（%APPDATA%\poly\ui.json）。没设置过、或目录已不存在时返回空串。
std::string ui_saved_workspace();
// 记住工作区，下次启动窗口版直接用它。
bool ui_save_workspace(const std::string& workspace);

}  // namespace poly
