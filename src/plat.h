#pragma once

#include <string>
#include <vector>

namespace poly {

void init_console();
std::vector<std::string> get_args_utf8(int argc, char** argv);

#ifdef _WIN32
std::wstring to_wide(const std::string& utf8);
std::string to_utf8(const std::wstring& w);
std::string to_utf8_acp(const std::string& local);
#endif

std::string quote_arg(const std::string& a);
std::string join_args(const std::vector<std::string>& args);

std::string env_get(const std::string& name);
void env_set(const std::string& name, const std::string& value);

// 用户的“文档”目录（Windows 走 SHGetFolderPathW，取不到时退回 %USERPROFILE%\Documents），
// 取不到返回空串。优先用系统 API：中文用户名下环境变量里是 ANSI 字节，不能当 UTF-8 用。
std::string documents_dir();

// 用户配置目录：Windows = %APPDATA%\poly，其它平台 = ~/.config/poly。取不到返回空串。
std::string config_dir();

bool stdout_is_tty();

}  // namespace poly
