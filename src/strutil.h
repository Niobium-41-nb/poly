#pragma once

#include <string>
#include <vector>

namespace poly {

std::string trim(const std::string& s);
std::string ltrim(const std::string& s);
std::string rtrim(const std::string& s);
std::string to_lower(std::string s);
std::string to_upper(std::string s);

bool starts_with(const std::string& s, const std::string& prefix);
bool ends_with(const std::string& s, const std::string& suffix);
bool contains(const std::string& s, const std::string& sub);

std::vector<std::string> split(const std::string& s, char delim, bool keepEmpty = false);
std::vector<std::string> split_words(const std::string& s);
std::vector<std::string> split_lines(const std::string& s);
std::string join(const std::vector<std::string>& parts, const std::string& sep);

std::string replace_all(std::string s, const std::string& from, const std::string& to);

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));

bool is_integer(const std::string& s);
long long to_int(const std::string& s, long long def = 0);
double to_double(const std::string& s, double def = 0.0);

std::string lpad(const std::string& s, size_t n, char c = '0');
size_t display_width(const std::string& s);
std::string pad_display(const std::string& s, size_t width);
std::string rpad(const std::string& s, size_t n, char c = ' ');
std::string num(long long v);
std::string num_fixed(double v, int digits);

std::string human_size(long long bytes);
std::string human_ms(double ms);
std::string human_mem_kb(long long kb);

std::string escape_html(const std::string& s);
std::string strip_cr(const std::string& s);
bool has_suffix_ci(const std::string& s, const std::string& suffix);

}  // namespace poly
