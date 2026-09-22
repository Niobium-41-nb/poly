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

bool stdout_is_tty();

}  // namespace poly
