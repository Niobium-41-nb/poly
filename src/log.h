#pragma once

#include <string>

namespace poly {

void log_configure(bool colorEnabled, bool verbose, bool quiet);
bool log_color_enabled();
bool log_is_verbose();
bool log_is_quiet();

void log_raw(const std::string& s);
void log_info(const std::string& s);
void log_ok(const std::string& s);
void log_warn(const std::string& s);
void log_err(const std::string& s);
void log_step(const std::string& s);
void log_debug(const std::string& s);

// 把日志同时追写到一个字符串（去掉 ANSI 颜色码）。
// poly ui 用它把一个请求里触发的命令输出回显到网页上；传 nullptr 关闭。
void log_set_capture(std::string* sink);

// Helpers used by the verdict tables.
std::string colorize_verdict(const std::string& verdict);
int verdict_color_code(const std::string& verdict);

}  // namespace poly
