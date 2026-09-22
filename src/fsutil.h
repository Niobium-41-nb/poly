#pragma once

#include <string>
#include <vector>

namespace poly {
namespace fs {

// All paths in this API are UTF-8 encoded (native separators allowed).

std::string join(const std::string& a, const std::string& b);
std::string join(const std::string& a, const std::string& b, const std::string& c);
std::string basename(const std::string& p);
std::string dirname(const std::string& p);
std::string stem(const std::string& p);
std::string extension(const std::string& p);
std::string normalize(const std::string& p);
std::string absolute(const std::string& p, const std::string& base = std::string());
std::string relative_to(const std::string& p, const std::string& base);
bool is_absolute(const std::string& p);

bool exists(const std::string& p);
bool is_file(const std::string& p);
bool is_dir(const std::string& p);

bool read_file(const std::string& p, std::string& out);
std::string read_file(const std::string& p);
bool write_file(const std::string& p, const std::string& data, bool createParents = true);
bool append_file(const std::string& p, const std::string& data);

bool mkdirs(const std::string& p);
bool remove_file(const std::string& p);
bool remove_all(const std::string& p);
bool copy_file(const std::string& from, const std::string& to, bool createParents = true);
bool copy_tree(const std::string& from, const std::string& to);
bool rename_path(const std::string& from, const std::string& to);

std::vector<std::string> list_dir(const std::string& p, bool filesOnly = false);
std::vector<std::string> list_dir_sorted(const std::string& p, bool filesOnly = false);
std::vector<std::string> list_files_recursive(const std::string& p);

long long file_size(const std::string& p);
long long modified_time(const std::string& p);

std::string cwd();
std::string temp_dir();
std::string unique_temp_path(const std::string& prefix);
std::string exe_path();
std::string exe_dir();
std::string home_dir();

// Searches PATH (and the usual Windows executable suffixes) for `name`.
std::string resolve_executable(const std::string& name);

std::string separator();

}  // namespace fs
}  // namespace poly
