#include "checker.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "format.h"
#include "fsutil.h"
#include "strutil.h"

namespace poly {

namespace {

struct Token {
    std::string text;
    bool numeric = false;
    double value = 0;
};

void add_tokens(const std::string& data, std::vector<Token>& out) {
    size_t i = 0;
    while (i < data.size()) {
        while (i < data.size() && static_cast<unsigned char>(data[i]) <= ' ') i++;
        size_t start = i;
        while (i < data.size() && static_cast<unsigned char>(data[i]) > ' ') i++;
        if (i > start) {
            Token t;
            t.text = data.substr(start, i - start);
            std::string probe = t.text;
            if (!probe.empty() && (probe[0] == '+' || probe[0] == '-')) probe = probe.substr(1);
            bool isNumber = !probe.empty();
            int dots = 0;
            for (char c : probe) {
                if (c == '.') {
                    dots++;
                    if (dots > 1) isNumber = false;
                } else if (c < '0' || c > '9') {
                    isNumber = false;
                }
            }
            if (isNumber) {
                t.numeric = true;
                t.value = strtod(t.text.c_str(), nullptr);
            }
            out.push_back(t);
        }
    }
}

std::vector<std::string> split_lines_ws(const std::string& data) {
    std::vector<std::string> out;
    for (const std::string& line : split_lines(strip_cr(data))) {
        std::string t = rtrim(line);
        out.push_back(t);
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

std::string short_token(const std::string& s) {
    if (s.size() <= 40) return s;
    return s.substr(0, 37) + "...";
}

CheckResult compare_tokens(const std::string& outData, const std::string& ansData, double eps, bool numericAware) {
    std::vector<Token> a, b;
    add_tokens(outData, a);
    add_tokens(ansData, b);
    if (a.size() != b.size()) {
        return {Verdict::WA, str("输出 token 个数不同：期望 {} 个，实际 {} 个", b.size(), a.size())};
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (numericAware && a[i].numeric && b[i].numeric) {
            if (eps > 0) {
                double diff = std::fabs(a[i].value - b[i].value);
                double scale = std::max(1.0, std::fabs(b[i].value));
                if (diff > eps * scale) {
                    return {Verdict::WA, str("第 {} 个数字不一致：期望 {}，实际 {}", i + 1, b[i].text, a[i].text)};
                }
            } else if (a[i].text != b[i].text && a[i].value != b[i].value) {
                return {Verdict::WA, str("第 {} 个数字不一致：期望 {}，实际 {}", i + 1, b[i].text, a[i].text)};
            }
            continue;
        }
        if (a[i].text != b[i].text) {
            return {Verdict::WA, str("第 {} 个 token 不一致：期望 {}，实际 {}", i + 1, short_token(b[i].text),
                                     short_token(a[i].text))};
        }
    }
    return {Verdict::OK, ""};
}

CheckResult compare_lines(const std::string& outData, const std::string& ansData) {
    std::vector<std::string> a = split_lines_ws(outData);
    std::vector<std::string> b = split_lines_ws(ansData);
    if (a.size() != b.size()) {
        return {Verdict::WA, str("行数不同：期望 {} 行，实际 {} 行", b.size(), a.size())};
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            return {Verdict::WA, str("第 {} 行不同：期望 [{}]，实际 [{}]", i + 1, b[i], a[i])};
        }
    }
    return {Verdict::OK, ""};
}

CheckResult compare_yesno(const std::string& outData, const std::string& ansData) {
    auto first = [](const std::string& d) {
        std::vector<Token> t;
        add_tokens(d, t);
        return t.empty() ? std::string() : to_upper(t[0].text);
    };
    std::string a = first(outData);
    std::string b = first(ansData);
    if (a != b) return {Verdict::WA, str("期望 {}，实际 {}", b, a)};
    return {Verdict::OK, ""};
}

}  // namespace

bool is_builtin_checker(const std::string& name) {
    std::string n = to_lower(trim(name));
    return n == "ncmp" || n == "wcmp" || n == "lcmp" || n == "fcmp" || n == "dcmp" || n == "rcmp4" ||
           n == "rcmp6" || n == "rcmp9" || n == "yesno" || n == "hcmp" || n == "fcmp";
}

std::string builtin_checker_list() { return "ncmp, wcmp, lcmp, fcmp, dcmp, rcmp4, rcmp6, rcmp9, yesno"; }

CheckResult builtin_check(const std::string& name, const std::string& inputPath, const std::string& outputPath,
                          const std::string& answerPath) {
    std::string outData, ansData;
    if (!fs::read_file(outputPath, outData)) {
        return {Verdict::FAIL, str("无法读取选手输出 {}", outputPath)};
    }
    if (!fs::read_file(answerPath, ansData)) {
        return {Verdict::FAIL, str("无法读取标准答案 {}", answerPath)};
    }
    (void)inputPath;
    std::string n = to_lower(trim(name));
    if (n == "lcmp") return compare_lines(outData, ansData);
    if (n == "yesno") return compare_yesno(outData, ansData);
    if (n == "rcmp4") return compare_tokens(outData, ansData, 1e-4, true);
    if (n == "rcmp6" || n == "dcmp") return compare_tokens(outData, ansData, 1e-6, true);
    if (n == "rcmp9") return compare_tokens(outData, ansData, 1e-9, true);
    if (n == "fcmp") return compare_tokens(outData, ansData, 1e-6, true);
    if (n == "wcmp") return compare_tokens(outData, ansData, 0.0, false);
    return compare_tokens(outData, ansData, 0.0, true);
}

}  // namespace poly
