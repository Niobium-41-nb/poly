// 题目包导入：把 QDUOJ / FPS / Hydro / HOJ 四种格式的题目包解析出来并落成 poly 工作区。
//
// 各格式的字段定义与 exporter.cpp 是同一套（互为逆向）：
//   · QDUOJ  zip：`<n>/problem.json` + `<n>/testcase/*.in|*.out`
//   · FPS    xml：freeproblemset，<item> 里内嵌 CDATA 测试数据
//   · Hydro  zip：`<pid>/problem.yaml` + `problem.md` + `testdata/`
//   · HOJ    zip：`problem_<id>.json` + `problem_<id>/*.in|*.out`
//
// 依赖自己实现的 XML / YAML 子集解析与 HTML→Markdown 近似转换：不做通用解析，
// 只覆盖这几种包真实会用到的语法，遇到不认识的结构会记 note 而不是直接失败。
#include <algorithm>
#include <ctime>
#include <map>
#include <set>

#include "exporter.h"
#include "format.h"
#include "fsutil.h"
#include "importer.h"
#include "json.h"
#include "log.h"
#include "markdown.h"
#include "problem.h"
#include "strutil.h"
#include "zip.h"

namespace poly {

namespace {

// ================================================================ 小工具

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string collapse_space(const std::string& s) {
    std::string out;
    bool pending = false;
    for (char c : s) {
        if (is_space(c)) {
            pending = !out.empty();
            continue;
        }
        if (pending) {
            out.push_back(' ');
            pending = false;
        }
        out.push_back(c);
    }
    return out;
}

std::string html_entity_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] != '&') {
            out.push_back(s[i]);
            continue;
        }
        size_t semi = s.find(';', i + 1);
        if (semi == std::string::npos || semi - i > 12) {
            out.push_back(s[i]);
            continue;
        }
        std::string name = s.substr(i + 1, semi - i - 1);
        std::string decoded;
        if (name == "amp") decoded = "&";
        else if (name == "lt") decoded = "<";
        else if (name == "gt") decoded = ">";
        else if (name == "quot") decoded = "\"";
        else if (name == "apos") decoded = "'";
        else if (name == "nbsp") decoded = " ";
        else if (name == "hellip") decoded = "…";
        else if (name == "mdash") decoded = "—";
        else if (name == "ndash") decoded = "–";
        else if (name.size() > 1 && name[0] == '#') {
            long long code = 0;
            bool ok = true;
            if (name.size() > 2 && (name[1] == 'x' || name[1] == 'X')) {
                for (size_t k = 2; k < name.size() && ok; k++) {
                    char c = name[k];
                    int v = -1;
                    if (c >= '0' && c <= '9') v = c - '0';
                    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
                    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
                    else ok = false;
                    if (ok) code = code * 16 + v;
                }
            } else {
                for (size_t k = 1; k < name.size() && ok; k++) {
                    if (name[k] < '0' || name[k] > '9') ok = false;
                    else code = code * 10 + (name[k] - '0');
                }
            }
            if (ok && code > 0 && code < 0x110000) {
                // UTF-8 编码
                unsigned cp = static_cast<unsigned>(code);
                if (cp < 0x80) {
                    decoded.push_back(static_cast<char>(cp));
                } else if (cp < 0x800) {
                    decoded.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else if (cp < 0x10000) {
                    decoded.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                } else {
                    decoded.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                    decoded.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                    decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                }
            }
        }
        if (decoded.empty()) {
            out.push_back(s[i]);  // 不认识的实体原样保留
        } else {
            out += decoded;
            i = semi;
        }
    }
    return out;
}

// 解析 "1.5" / "1000" / "256" 之类的数值前缀。
double leading_number(const std::string& s, double def = 0) {
    size_t i = 0;
    while (i < s.size() && is_space(s[i])) i++;
    size_t start = i;
    while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == '-' || s[i] == '+')) i++;
    if (i == start) return def;
    return to_double(s.substr(start, i - start), def);
}

std::string rest_after_number(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && is_space(s[i])) i++;
    while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == '-' || s[i] == '+')) i++;
    return trim(s.substr(i));
}

long long clamp_time_ms(double ms) {
    if (ms < 1) ms = 1;
    if (ms > 600000) ms = 600000;
    return static_cast<long long>(ms + 0.5);
}

// 时限：值为秒或毫秒，取决于 unit（FPS）；"2000ms" / "2s" 则从字符串尾部识别。
long long time_ms_from(const std::string& value, const std::string& unit) {
    std::string u = to_lower(trim(unit));
    double v = leading_number(value, 1);
    if (u == "ms") return clamp_time_ms(v);
    if (u.empty() || u == "s" || u == "sec" || u == "second") {
        // 没写单位时可能是 "2000ms" 这种自带单位的写法
        std::string tail = to_lower(rest_after_number(value));
        if (starts_with(tail, "ms")) return clamp_time_ms(v);
        return clamp_time_ms(v * 1000.0);
    }
    return clamp_time_ms(v * 1000.0);
}

// 内存：返回 KB。
long long memory_kb_from(const std::string& value, const std::string& unit) {
    std::string u = to_lower(trim(unit));
    double v = leading_number(value, 256);
    if (u.empty()) {
        std::string tail = to_lower(rest_after_number(value));
        if (!tail.empty()) u = tail;
    }
    if (starts_with(u, "kb") || u == "k" || u == "kib") return static_cast<long long>(v + 0.5);
    if (starts_with(u, "gb") || u == "g") return static_cast<long long>(v * 1024 * 1024 + 0.5);
    if (starts_with(u, "mb") || u == "m" || starts_with(u, "mib")) return static_cast<long long>(v * 1024 + 0.5);
    return static_cast<long long>(v * 1024 + 0.5);  // 默认按 MB
}

std::vector<std::string> split_tags(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() {
        if (!cur.empty()) out.push_back(cur);
        cur.clear();
    };
    for (size_t i = 0; i < s.size();) {
        unsigned char u = static_cast<unsigned char>(s[i]);
        // 全角逗号 U+FF0C（UTF-8: EF BC 8C）也要当分隔符
        if (u == 0xEF && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xBC &&
            static_cast<unsigned char>(s[i + 2]) == 0x8C) {
            flush();
            i += 3;
            continue;
        }
        if (s[i] == ' ' || s[i] == ',' || s[i] == '\t' || s[i] == '\n' || s[i] == ';') {
            flush();
            i++;
            continue;
        }
        cur.push_back(s[i]);
        i++;
    }
    flush();
    return out;
}

// ================================================================ 极简 XML

struct XmlNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::string text;
    std::vector<XmlNode> children;

    const XmlNode* child(const std::string& n) const {
        for (const XmlNode& c : children) {
            if (c.name == n) return &c;
        }
        return nullptr;
    }
    std::string child_text(const std::string& n, const std::string& def = std::string()) const {
        const XmlNode* c = child(n);
        return c ? c->text : def;
    }
    std::vector<const XmlNode*> children_named(const std::string& n) const {
        std::vector<const XmlNode*> out;
        for (const XmlNode& c : children) {
            if (c.name == n) out.push_back(&c);
        }
        return out;
    }
    std::string attr(const std::string& key, const std::string& def = std::string()) const {
        for (const std::pair<std::string, std::string>& a : attrs) {
            if (a.first == key) return a.second;
        }
        return def;
    }
};

bool is_name_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == ':' ||
           static_cast<unsigned char>(c) >= 0x80;
}

bool is_name_char(char c) {
    return is_name_start(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
}

class XmlParser {
public:
    explicit XmlParser(const std::string& s) : s_(s) {}

    bool parse_root(XmlNode& root, std::string& err) {
        skip_misc();
        if (!parse_element(root, err)) return false;
        skip_misc();
        return true;
    }

private:
    const std::string& s_;
    size_t i_ = 0;

    bool eof() const { return i_ >= s_.size(); }
    char peek(size_t off = 0) const { return i_ + off < s_.size() ? s_[i_ + off] : '\0'; }

    void skip_ws() {
        while (!eof() && is_space(s_[i_])) i_++;
    }

    void skip_misc() {
        for (;;) {
            skip_ws();
            if (peek() != '<') return;
            if (peek(1) == '?') {  // <?xml ... ?>
                size_t end = s_.find("?>", i_);
                i_ = end == std::string::npos ? s_.size() : end + 2;
                continue;
            }
            if (peek(1) == '!') {
                if (s_.compare(i_, 9, "<!--") == 0) {
                    size_t end = s_.find("-->", i_);
                    i_ = end == std::string::npos ? s_.size() : end + 3;
                    continue;
                }
                if (s_.compare(i_, 9, "<!DOCTYPE") == 0) {
                    // 跳过整个 DOCTYPE（可能带内部子集 [...] 与引号）
                    size_t p = i_ + 9;
                    int depth = 0;
                    char quote = '\0';
                    for (; p < s_.size(); p++) {
                        char c = s_[p];
                        if (quote) {
                            if (c == quote) quote = '\0';
                            continue;
                        }
                        if (c == '"' || c == '\'') {
                            quote = c;
                        } else if (c == '[') {
                            depth++;
                        } else if (c == ']') {
                            depth--;
                        } else if (c == '>' && depth <= 0) {
                            break;
                        }
                    }
                    i_ = p < s_.size() ? p + 1 : s_.size();
                    continue;
                }
                if (s_.compare(i_, 9, "<![CDATA[") == 0) return;  // 交给元素解析
                size_t end = s_.find('>', i_);
                i_ = end == std::string::npos ? s_.size() : end + 1;
                continue;
            }
            return;
        }
    }

    std::string parse_name() {
        size_t start = i_;
        if (!eof() && is_name_start(s_[i_])) i_++;
        while (!eof() && is_name_char(s_[i_])) i_++;
        return s_.substr(start, i_ - start);
    }

    bool parse_element(XmlNode& node, std::string& err) {
        if (peek() != '<') {
            err = "XML 结构错误：期望元素";
            return false;
        }
        i_++;
        node.name = parse_name();
        if (node.name.empty()) {
            err = "XML 结构错误：元素名为空";
            return false;
        }
        // 属性
        for (;;) {
            skip_ws();
            if (peek() == '/' && peek(1) == '>') {
                i_ += 2;
                return true;  // 自闭合
            }
            if (peek() == '>') {
                i_++;
                break;
            }
            if (eof()) {
                err = str("XML 结构错误：<{}> 未闭合", node.name);
                return false;
            }
            std::string key = parse_name();
            if (key.empty()) {
                i_++;  // 跳过异常字符，避免死循环
                continue;
            }
            skip_ws();
            std::string value;
            if (peek() == '=') {
                i_++;
                skip_ws();
                char quote = peek();
                if (quote == '"' || quote == '\'') {
                    i_++;
                    size_t end = s_.find(quote, i_);
                    if (end == std::string::npos) {
                        err = "XML 属性引号未闭合";
                        return false;
                    }
                    value = html_entity_decode(s_.substr(i_, end - i_));
                    i_ = end + 1;
                } else {
                    size_t start = i_;
                    while (!eof() && !is_space(s_[i_]) && s_[i_] != '>' && s_[i_] != '/') i_++;
                    value = s_.substr(start, i_ - start);
                }
            }
            node.attrs.push_back({key, value});
        }
        // 内容
        for (;;) {
            if (eof()) {
                err = str("XML 结构错误：<{}> 没有结束标签", node.name);
                return false;
            }
            if (peek() == '<') {
                if (s_.compare(i_, 9, "<![CDATA[") == 0) {
                    size_t end = s_.find("]]>", i_ + 9);
                    if (end == std::string::npos) {
                        err = "XML CDATA 未闭合";
                        return false;
                    }
                    node.text += s_.substr(i_ + 9, end - i_ - 9);
                    i_ = end + 3;
                    continue;
                }
                if (s_.compare(i_, 4, "<!--") == 0) {
                    size_t end = s_.find("-->", i_);
                    i_ = end == std::string::npos ? s_.size() : end + 3;
                    continue;
                }
                if (peek(1) == '/') {
                    i_ += 2;
                    std::string close = parse_name();
                    skip_ws();
                    if (peek() != '>') {
                        err = str("XML 结构错误：</{}> 之后缺少 >", close);
                        return false;
                    }
                    i_++;
                    if (close != node.name) {
                        err = str("XML 结构错误：<{}> 与 </{}> 不匹配", node.name, close);
                        return false;
                    }
                    return true;
                }
                XmlNode child;
                if (!parse_element(child, err)) return false;
                node.children.push_back(std::move(child));
                continue;
            }
            size_t start = i_;
            while (!eof() && s_[i_] != '<') i_++;
            node.text += html_entity_decode(s_.substr(start, i_ - start));
        }
    }
};

// ================================================================ 极简 YAML
//
// 只支持这三种题目包里会出现的写法：顶层 "key: value"、引号标量、内联 [a, b]、
// 以及块序列（含 "- key: value" 的「序列里套映射」）。不认识的结构会被忽略。

struct YamlValue {
    enum class Kind { Null, Scalar, Seq, Map };

    Kind kind = Kind::Null;
    std::string scalar;
    std::vector<YamlValue> seq;
    std::vector<std::pair<std::string, YamlValue>> map;

    const YamlValue* find(const std::string& key) const {
        for (const std::pair<std::string, YamlValue>& kv : map) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
    std::string as_string(const std::string& def = std::string()) const {
        if (kind == Kind::Scalar) return scalar;
        return def;
    }
    bool is_null() const { return kind == Kind::Null; }
    size_t size() const { return kind == Kind::Seq ? seq.size() : 0; }
};

struct YamlLine {
    int indent = 0;
    std::string body;
};

// 摘掉行尾注释：只在引号之外、且 # 前面是空白时才算注释。
std::string strip_yaml_comment(const std::string& line) {
    char quote = '\0';
    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];
        if (quote) {
            if (quote == '"' && c == '\\') i++;
            else if (c == quote) quote = '\0';
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == '#' && (i == 0 || is_space(line[i - 1]))) return line.substr(0, i);
    }
    return line;
}

std::string yaml_unquote(const std::string& raw) {
    std::string s = trim(raw);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        std::string out;
        for (size_t i = 1; i + 1 < s.size(); i++) {
            if (s[i] == '\\' && i + 2 < s.size()) {
                char n = s[i + 1];
                if (n == 'n') out.push_back('\n');
                else if (n == 't') out.push_back('\t');
                else if (n == 'r') out.push_back('\r');
                else out.push_back(n);
                i++;
                continue;
            }
            out.push_back(s[i]);
        }
        return out;
    }
    if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'') {
        std::string out;
        for (size_t i = 1; i + 1 < s.size(); i++) {
            if (s[i] == '\'' && i + 2 < s.size() && s[i + 1] == '\'') {
                out.push_back('\'');
                i++;
                continue;
            }
            out.push_back(s[i]);
        }
        return out;
    }
    return s;
}

// 按逗号切分内联序列，尊重引号。
std::vector<std::string> split_flow_items(const std::string& body) {
    std::vector<std::string> out;
    std::string cur;
    char quote = '\0';
    for (size_t i = 0; i < body.size(); i++) {
        char c = body[i];
        if (quote) {
            cur.push_back(c);
            if (quote == '"' && c == '\\' && i + 1 < body.size()) {
                cur.push_back(body[++i]);
                continue;
            }
            if (c == quote) quote = '\0';
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            cur.push_back(c);
            continue;
        }
        if (c == ',') {
            out.push_back(trim(cur));
            cur.clear();
            continue;
        }
        cur.push_back(c);
    }
    if (!trim(cur).empty() || !out.empty()) out.push_back(trim(cur));
    return out;
}

YamlValue yaml_scalar_or_flow(const std::string& raw);

// 找顶层 "key:" 的冒号位置（忽略引号内的冒号）。
size_t find_key_colon(const std::string& body) {
    char quote = '\0';
    for (size_t i = 0; i < body.size(); i++) {
        char c = body[i];
        if (quote) {
            if (quote == '"' && c == '\\') i++;
            else if (c == quote) quote = '\0';
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == ':') return i;
    }
    return std::string::npos;
}

bool looks_like_map_entry(const std::string& body) {
    size_t colon = find_key_colon(body);
    if (colon == std::string::npos) return false;
    if (colon + 1 < body.size() && body[colon + 1] != ' ' && body[colon + 1] != '\t') return false;
    return colon > 0;
}

YamlValue yaml_scalar_or_flow(const std::string& raw) {
    YamlValue v;
    std::string s = trim(raw);
    if (s.empty() || s == "~" || s == "null" || s == "Null" || s == "NULL") return v;
    if (s.front() == '[' && s.back() == ']') {
        v.kind = YamlValue::Kind::Seq;
        for (const std::string& item : split_flow_items(s.substr(1, s.size() - 2))) {
            if (trim(item).empty()) continue;
            v.seq.push_back(yaml_scalar_or_flow(item));
        }
        return v;
    }
    if (s.front() == '{' && s.back() == '}') {
        v.kind = YamlValue::Kind::Map;
        for (const std::string& item : split_flow_items(s.substr(1, s.size() - 2))) {
            size_t colon = find_key_colon(item);
            if (colon == std::string::npos) continue;
            v.map.push_back({yaml_unquote(item.substr(0, colon)),
                             yaml_scalar_or_flow(item.substr(colon + 1))});
        }
        return v;
    }
    v.kind = YamlValue::Kind::Scalar;
    v.scalar = yaml_unquote(s);
    return v;
}

class YamlParser {
public:
    explicit YamlParser(const std::string& text) {
        for (const std::string& raw : split_lines(strip_cr(text))) {
            std::string noComment = strip_yaml_comment(raw);
            if (trim(noComment).empty()) continue;
            if (trim(noComment) == "---") continue;
            int indent = 0;
            while (indent < static_cast<int>(noComment.size()) && noComment[indent] == ' ') indent++;
            YamlLine line;
            line.indent = indent;
            line.body = trim(noComment);
            // tab 缩进按 2 空格计
            for (char c : noComment.substr(0, indent)) (void)c;
            lines_.push_back(line);
        }
    }

    YamlValue parse() {
        size_t i = 0;
        if (lines_.empty()) return YamlValue();
        return parse_block(i, lines_[0].indent);
    }

private:
    std::vector<YamlLine> lines_;

    bool is_seq_line(const std::string& body) const {
        return body == "-" || starts_with(body, "- ") || starts_with(body, "-\t");
    }

    YamlValue parse_block(size_t& i, int indent) {
        if (i >= lines_.size() || lines_[i].indent < indent) return YamlValue();
        if (is_seq_line(lines_[i].body)) return parse_seq(i, indent);
        return parse_map(i, indent);
    }

    YamlValue parse_map(size_t& i, int indent) {
        YamlValue m;
        m.kind = YamlValue::Kind::Map;
        while (i < lines_.size() && lines_[i].indent == indent && !is_seq_line(lines_[i].body)) {
            std::string body = lines_[i].body;
            size_t colon = find_key_colon(body);
            if (colon == std::string::npos) {
                i++;
                continue;
            }
            std::string key = yaml_unquote(body.substr(0, colon));
            std::string rest = trim(body.substr(colon + 1));
            i++;
            if (!rest.empty()) {
                m.map.push_back({key, yaml_scalar_or_flow(rest)});
                continue;
            }
            if (i < lines_.size() && lines_[i].indent > indent) {
                m.map.push_back({key, parse_block(i, lines_[i].indent)});
            } else {
                m.map.push_back({key, YamlValue()});
            }
        }
        return m;
    }

    YamlValue parse_seq(size_t& i, int indent) {
        YamlValue s;
        s.kind = YamlValue::Kind::Seq;
        while (i < lines_.size() && lines_[i].indent == indent && is_seq_line(lines_[i].body)) {
            std::string body = lines_[i].body;
            std::string rest = trim(body.size() > 1 ? body.substr(1) : std::string());
            if (!rest.empty() && looks_like_map_entry(rest)) {
                // "- key: value"：把这一行改写成更深缩进的普通行，再按映射解析，
                // 这样后续同级缩进的 "key2: value2" 会自动归到同一个 item 上。
                lines_[i].indent = indent + 2;
                lines_[i].body = rest;
                s.seq.push_back(parse_block(i, indent + 2));
                continue;
            }
            i++;
            if (rest.empty()) {
                if (i < lines_.size() && lines_[i].indent > indent) {
                    s.seq.push_back(parse_block(i, lines_[i].indent));
                } else {
                    s.seq.push_back(YamlValue());
                }
            } else {
                s.seq.push_back(yaml_scalar_or_flow(rest));
            }
        }
        return s;
    }
};

bool parse_yaml(const std::string& text, YamlValue& out, std::string& err) {
    YamlParser p(text);
    out = p.parse();
    if (out.kind != YamlValue::Kind::Map && out.kind != YamlValue::Kind::Seq) {
        err = "YAML 内容为空或不是映射";
        return false;
    }
    return true;
}

// ================================================================ HTML → Markdown

std::string strip_tags(const std::string& html) {
    std::string out;
    bool inTag = false;
    for (size_t i = 0; i < html.size(); i++) {
        char c = html[i];
        if (inTag) {
            if (c == '>') inTag = false;
            continue;
        }
        if (c == '<') {
            inTag = true;
            continue;
        }
        out.push_back(c);
    }
    return html_entity_decode(out);
}

std::string table_to_markdown(const std::string& tableHtml) {
    std::vector<std::vector<std::string>> rows;
    size_t p = 0;
    while (true) {
        size_t tr = tableHtml.find("<tr", p);
        if (tr == std::string::npos) break;
        size_t trEnd = tableHtml.find("</tr>", tr);
        if (trEnd == std::string::npos) break;
        std::string rowHtml = tableHtml.substr(tr, trEnd - tr);
        std::vector<std::string> cells;
        size_t q = 0;
        while (true) {
            size_t td = rowHtml.find("<td", q);
            size_t th = rowHtml.find("<th", q);
            size_t cell = std::min(td == std::string::npos ? rowHtml.size() : td,
                                   th == std::string::npos ? rowHtml.size() : th);
            if (cell >= rowHtml.size()) break;
            bool header = (th != std::string::npos && th == cell);
            const char* closeTag = header ? "</th>" : "</td>";
            size_t open = rowHtml.find('>', cell);
            if (open == std::string::npos) break;
            size_t close = rowHtml.find(closeTag, open);
            if (close == std::string::npos) close = rowHtml.size();
            std::string text = collapse_space(strip_tags(rowHtml.substr(open + 1, close - open - 1)));
            for (char& c : text) {
                if (c == '|') c = '/';
            }
            cells.push_back(trim(text));
            q = close + 1;
        }
        if (!cells.empty()) rows.push_back(cells);
        p = trEnd + 5;
    }
    if (rows.empty()) return std::string();
    size_t cols = rows[0].size();
    for (const std::vector<std::string>& r : rows) {
        if (r.size() != cols) return std::string();  // 列数不一致就不转表格
    }
    std::string out;
    auto emit_row = [&](const std::vector<std::string>& r) {
        out += "|";
        for (const std::string& c : r) out += " " + c + " |";
        out += "\n";
    };
    emit_row(rows[0]);
    out += "|";
    for (size_t i = 0; i < cols; i++) out += " --- |";
    out += "\n";
    for (size_t i = 1; i < rows.size(); i++) emit_row(rows[i]);
    return out;
}

std::string html_to_markdown(const std::string& html) {
    std::string out;
    bool inPre = false;
    std::string preBuffer;
    int listDepth = 0;
    int orderedDepth = 0;
    std::vector<std::string> linkHrefs;  // <a> 的 href，配对 </a> 时补上

    auto ensure_blank_line = [&out]() {
        if (out.empty()) return;
        if (out.size() >= 2 && out[out.size() - 1] == '\n' && out[out.size() - 2] == '\n') return;
        if (out.back() != '\n') out.push_back('\n');
        out.push_back('\n');
    };

    for (size_t i = 0; i < html.size();) {
        if (html[i] != '<') {
            size_t next = html.find('<', i);
            std::string text = html.substr(i, next == std::string::npos ? std::string::npos : next - i);
            if (inPre) {
                preBuffer += html_entity_decode(text);
            } else {
                std::string decoded = html_entity_decode(text);
                std::string collapsed = collapse_space(decoded);
                if (!collapsed.empty()) {
                    // 只有在原文本身带空白时才补空格，否则 <strong>xx</strong> 这类
                    // 行内标签会被插入多余空格（中文尤其明显）。
                    bool leadingSpace = !decoded.empty() && is_space(decoded.front());
                    if (leadingSpace && !out.empty() && !is_space(out.back())) out.push_back(' ');
                    out += collapsed;
                }
            }
            i = next == std::string::npos ? html.size() : next;
            continue;
        }
        // 一段标签
        if (html.compare(i, 4, "<!--") == 0) {
            size_t end = html.find("-->", i);
            i = end == std::string::npos ? html.size() : end + 3;
            continue;
        }
        size_t tagEnd = html.find('>', i);
        if (tagEnd == std::string::npos) break;
        std::string tagBody = html.substr(i + 1, tagEnd - i - 1);
        bool closing = !tagBody.empty() && tagBody[0] == '/';
        std::string name = to_lower(trim(closing ? tagBody.substr(1) : tagBody));
        bool selfClosed = !name.empty() && name.back() == '/';
        if (selfClosed) name = trim(name.substr(0, name.size() - 1));
        size_t space = name.find_first_of(" \t\n");
        std::string attrs;
        if (space != std::string::npos) {
            attrs = name.substr(space + 1);
            name = name.substr(0, space);
        }
        i = tagEnd + 1;

        auto attr_value = [&attrs](const char* key) -> std::string {
            std::string needle = std::string(key) + "=";
            size_t p = to_lower(attrs).find(needle);
            if (p == std::string::npos) return std::string();
            size_t q = p + needle.size();
            if (q >= attrs.size()) return std::string();
            char quote = attrs[q];
            if (quote == '"' || quote == '\'') {
                size_t end = attrs.find(quote, q + 1);
                if (end == std::string::npos) return std::string();
                return html_entity_decode(attrs.substr(q + 1, end - q - 1));
            }
            size_t end = attrs.find_first_of(" \t\n", q);
            return html_entity_decode(attrs.substr(q, end == std::string::npos ? std::string::npos : end - q));
        };

        if (name == "pre") {
            if (!closing) {
                ensure_blank_line();
                inPre = true;
                preBuffer.clear();
            } else {
                inPre = false;
                std::string fence = "```";
                while (preBuffer.find(fence) != std::string::npos) fence += "`";
                out += fence + "\n";
                out += rtrim(preBuffer);
                out += "\n" + fence + "\n\n";
                preBuffer.clear();
            }
            continue;
        }
        if (inPre) continue;  // pre 里除 </pre> 外的标签一律忽略

        if (name == "br" || name == "hr") {
            if (name == "br") out += "\n";
            else {
                ensure_blank_line();
                out += "---\n\n";
            }
            continue;
        }
        if (name == "p" || name == "div" || name == "section" || name == "article" ||
            name == "blockquote" || name == "body" || name == "html" || name == "center") {
            ensure_blank_line();
            continue;
        }
        if (name == "h1" || name == "h2" || name == "h3" || name == "h4" || name == "h5" || name == "h6") {
            ensure_blank_line();
            if (!closing) {
                out += std::string(name[1] - '0', '#') + " ";
            } else {
                out += "\n\n";
            }
            continue;
        }
        if (name == "ul" || name == "ol") {
            ensure_blank_line();
            if (!closing) {
                listDepth++;
                if (name == "ol") orderedDepth++;
            } else {
                listDepth = std::max(0, listDepth - 1);
                if (name == "ol") orderedDepth = std::max(0, orderedDepth - 1);
                ensure_blank_line();
            }
            continue;
        }
        if (name == "li") {
            if (!closing) {
                if (!out.empty() && out.back() != '\n') out.push_back('\n');
                out += orderedDepth > 0 ? "1. " : "- ";
            } else {
                out.push_back('\n');
            }
            continue;
        }
        if (name == "table") {
            if (!closing) {
                size_t end = html.find("</table>", i);
                if (end != std::string::npos) {
                    std::string htmlTable = html.substr(i, end - i);
                    std::string md = table_to_markdown(htmlTable);
                    ensure_blank_line();
                    if (!md.empty()) {
                        out += md + "\n";
                    } else {
                        out += collapse_space(strip_tags(htmlTable)) + "\n\n";
                    }
                    i = end + 8;
                }
            }
            continue;
        }
        if (name == "tr" || name == "td" || name == "th" || name == "tbody" || name == "thead") {
            continue;  // 表格已在 table 分支整体处理
        }
        if (name == "strong" || name == "b") {
            out += "**";
            continue;
        }
        if (name == "em" || name == "i") {
            out += "*";
            continue;
        }
        if (name == "code") {
            if (closing) {
                out += "`";
            } else {
                out += "`";
            }
            continue;
        }
        if (name == "a") {
            if (closing) {
                std::string href = linkHrefs.empty() ? std::string() : linkHrefs.back();
                if (!linkHrefs.empty()) linkHrefs.pop_back();
                if (!href.empty()) out += "](" + href + ")";
                continue;
            }
            std::string href = attr_value("href");
            linkHrefs.push_back(href);
            if (!href.empty()) out += "[";
            continue;
        }
        if (name == "img") {
            std::string src = attr_value("src");
            std::string alt = attr_value("alt");
            if (!src.empty()) out += "![" + (alt.empty() ? std::string("img") : alt) + "](" + src + ")";
            continue;
        }
        // 其他标签（span/font/u/small/sup...）直接透传内容
    }
    if (inPre && !preBuffer.empty()) {
        out += "```\n" + rtrim(preBuffer) + "\n```\n";
    }
    // 压掉连续空行
    std::string cleaned;
    int blanks = 0;
    for (const std::string& line : split_lines(out)) {
        if (trim(line).empty()) {
            blanks++;
            if (blanks > 1) continue;
        } else {
            blanks = 0;
        }
        cleaned += rtrim(line) + "\n";
    }
    return trim(cleaned);
}

// ================================================================ FPS

bool parse_fps(const std::string& data, std::vector<ImportedProblem>& out, std::string& err) {
    XmlNode root;
    XmlParser parser(data);
    if (!parser.parse_root(root, err)) return false;
    if (to_lower(root.name) != "fps") {
        err = str("不是有效的 FPS 文件（根元素是 <{}>，应为 <fps>）", root.name);
        return false;
    }
    std::vector<const XmlNode*> items = root.children_named("item");
    if (items.empty()) {
        err = "FPS 文件里没有任何 <item>";
        return false;
    }
    for (const XmlNode* item : items) {
        ImportedProblem p;
        p.title = trim(collapse_space(item->child_text("title")));
        p.timeLimitMs = time_ms_from(item->child_text("time_limit", "1"),
                                     item->child("time_limit") ? item->child("time_limit")->attr("unit") : "");
        p.memoryLimitKb = memory_kb_from(item->child_text("memory_limit", "256"),
                                         item->child("memory_limit") ? item->child("memory_limit")->attr("unit") : "");
        p.description = html_to_markdown(item->child_text("description"));
        p.input = html_to_markdown(item->child_text("input"));
        p.output = html_to_markdown(item->child_text("output"));
        p.hint = html_to_markdown(item->child_text("hint"));
        p.tags = split_tags(item->child_text("source"));

        std::vector<const XmlNode*> sampleIn = item->children_named("sample_input");
        std::vector<const XmlNode*> sampleOut = item->children_named("sample_output");
        for (size_t i = 0; i < sampleIn.size(); i++) {
            ImportedProblem::Sample s;
            s.input = sampleIn[i]->text;
            if (i < sampleOut.size()) s.output = sampleOut[i]->text;
            p.samples.push_back(s);
        }

        std::vector<const XmlNode*> testIn = item->children_named("test_input");
        std::vector<const XmlNode*> testOut = item->children_named("test_output");
        if (!testOut.empty() && testOut.size() != testIn.size()) {
            p.notes.push_back(str("FPS 里 test_input({}) 与 test_output({}) 数量不同，按较少的一侧配对",
                                  testIn.size(), testOut.size()));
        }
        size_t testCount = testOut.empty() ? testIn.size() : std::min(testIn.size(), testOut.size());
        for (size_t i = 0; i < testCount; i++) {
            ImportedProblem::Case c;
            c.name = testIn[i]->attr("name", num(static_cast<long long>(i + 1)));
            c.input = testIn[i]->text;
            if (i < testOut.size() && !trim(testOut[i]->text).empty()) {
                c.output = testOut[i]->text;
                c.hasOutput = true;
            }
            p.tests.push_back(c);
        }
        if (!testIn.empty() && testOut.empty()) {
            p.notes.push_back("FPS 里只有 test_input，已全部作为输入（无标准答案）");
        }

        if (const XmlNode* spj = item->child("spj")) {
            if (trim(spj->text).empty()) {
                p.notes.push_back("FPS 里的 <spj> 是空的，已忽略");
            } else {
                p.checkerLanguage = spj->attr("language", "C++");
                p.checkerSource = spj->text;
            }
        }
        if (const XmlNode* inter = item->child("interactor")) {
            if (!trim(inter->text).empty()) {
                p.interactorSource = inter->text;
                p.interactive = true;
            }
        }
        for (const XmlNode* sol : item->children_named("solution")) {
            std::string lang = sol->attr("language", "C++");
            std::string lower = to_lower(lang);
            if ((lower == "c++" || lower == "cpp" || lower == "g++") && p.mainSolution.empty()) {
                p.mainSolution = sol->text;
                p.mainSolutionLanguage = "C++";
            }
        }
        if (item->child("remote_oj") && !trim(item->child_text("remote_oj")).empty()) {
            p.notes.push_back(str("这是远程题目（{} / {}），只导入了题面与测试数据",
                                  trim(item->child_text("remote_oj")), trim(item->child_text("remote_id"))));
        }
        if (p.title.empty()) p.title = "P" + num(static_cast<long long>(out.size() + 1));
        out.push_back(std::move(p));
    }
    return true;
}

// ================================================================ QDUOJ

bool parse_qduoj(const std::string& data, std::vector<ImportedProblem>& out, std::string& err) {
    ZipReader zip;
    if (!zip.open_data(data, err)) return false;

    std::vector<std::string> manifests;
    for (const ZipEntryInfo& e : zip.entries()) {
        if (e.name.size() >= 12 && ends_with(e.name, "/problem.json")) manifests.push_back(e.name);
    }
    if (manifests.empty()) {
        err = "压缩包里找不到 */problem.json（不是 QDUOJ 导出的题目包）";
        return false;
    }
    std::sort(manifests.begin(), manifests.end());

    for (const std::string& manifest : manifests) {
        std::string text;
        if (!zip.read(manifest, text)) {
            err = str("读取 {} 失败：{}", manifest, zip.error());
            return false;
        }
        Json doc;
        std::string perr;
        if (!Json::parse(text, doc, &perr)) {
            err = str("{} 不是有效的 JSON：{}", manifest, perr);
            return false;
        }
        ImportedProblem p;
        p.title = trim(doc["title"].as_string());
        p.pid = trim(doc["display_id"].as_string());
        p.timeLimitMs = clamp_time_ms(static_cast<double>(doc["time_limit"].as_int(1000)));
        p.memoryLimitKb = memory_kb_from(num(doc["memory_limit"].as_int(256)), "mb");

        auto take_html = [](const Json& node) {
            if (!node.is_object()) {
                if (node.is_string()) return std::string(node.as_string());
                return std::string();
            }
            std::string value = node["value"].as_string();
            std::string format = to_lower(node["format"].as_string("html"));
            if (format == "html") return html_to_markdown(value);
            return trim(value);
        };
        p.description = take_html(doc["description"]);
        p.input = take_html(doc["input_description"]);
        p.output = take_html(doc["output_description"]);
        p.hint = take_html(doc["hint"]);

        const Json& source = doc["source"];
        std::string sourceText;
        if (source.is_string()) sourceText = source.as_string();
        else if (source.is_object()) sourceText = source["value"].as_string();
        p.tags = split_tags(sourceText);
        if (doc.has("tags") && doc["tags"].is_array()) {
            for (const Json& t : doc["tags"].items()) {
                if (t.is_string() && !trim(t.as_string()).empty()) p.tags.push_back(trim(t.as_string()));
            }
        }
        if (doc.has("samples") && doc["samples"].is_array()) {
            for (const Json& s : doc["samples"].items()) {
                ImportedProblem::Sample sample;
                sample.input = s["input"].as_string();
                sample.output = s["output"].as_string();
                if (!sample.input.empty() || !sample.output.empty()) p.samples.push_back(sample);
            }
        }
        if (doc.has("spj") && doc["spj"].is_object()) {
            p.checkerLanguage = doc["spj"]["language"].as_string("C++");
            p.checkerSource = doc["spj"]["code"].as_string();
        }
        std::string rule = doc["rule_type"].as_string("ACM");
        if (rule == "OI") p.notes.push_back("原题是 OI 赛制（按测试点计分），poly 只判对错");

        std::string dir = manifest.substr(0, manifest.size() - std::string("problem.json").size());
        if (doc.has("test_case_score") && doc["test_case_score"].is_array() &&
            !doc["test_case_score"].items().empty()) {
            for (const Json& tc : doc["test_case_score"].items()) {
                std::string inName = tc["input_name"].as_string();
                std::string outName = tc["output_name"].as_string();
                if (inName.empty()) continue;
                ImportedProblem::Case c;
                c.name = inName;
                if (!zip.read(dir + "testcase/" + inName, c.input)) {
                    p.notes.push_back(str("缺少测试点输入 {}", inName));
                    continue;
                }
                if (!outName.empty() && outName != "-") {
                    c.hasOutput = zip.read(dir + "testcase/" + outName, c.output);
                }
                p.tests.push_back(c);
            }
        } else {
            // 没有清单时退化为按目录里的 *.in / *.out 配对
            std::map<std::string, std::pair<std::string, std::string>> found;
            for (const ZipEntryInfo& e : zip.entries()) {
                if (e.name.size() < dir.size() + 10) continue;
                if (e.name.compare(0, dir.size() + 9, dir + "testcase/") != 0) continue;
                std::string leaf = e.name.substr(dir.size() + 9);
                if (leaf.find('/') != std::string::npos) continue;
                std::string stem = fs::stem(leaf);
                std::string ext = to_lower(fs::extension(leaf));
                if (ext == "in") found[stem].first = e.name;
                else if (ext == "out" || ext == "ans") found[stem].second = e.name;
            }
            for (const auto& kv : found) {
                if (kv.second.first.empty()) continue;
                ImportedProblem::Case c;
                c.name = fs::basename(kv.second.first);
                if (!zip.read(kv.second.first, c.input)) continue;
                if (!kv.second.second.empty()) c.hasOutput = zip.read(kv.second.second, c.output);
                p.tests.push_back(c);
            }
            if (!p.tests.empty()) p.notes.push_back("压缩包里没有 test_case_score，按文件名配对测试点");
        }
        if (p.title.empty()) {
            err = str("{} 里没有 title 字段", manifest);
            return false;
        }
        if (p.tests.empty()) p.notes.push_back("这道题没有带测试数据（原包可能只含题面）");
        out.push_back(std::move(p));
    }
    return true;
}

// ================================================================ Hydro

// Hydro 的 problem.md 会用 ```input1 / ```output1 代码块内嵌样例，
// 这里把它们抽出来换成 {{samples}}，避免 poly 再注入一遍。
std::string extract_hydro_samples(const std::string& md, std::vector<ImportedProblem::Sample>& out,
                                  bool& found) {
    std::vector<std::string> lines = split_lines(strip_cr(md));
    std::vector<std::string> kept;
    found = false;
    for (size_t i = 0; i < lines.size(); i++) {
        std::string t = trim(lines[i]);
        if (!starts_with(t, "```")) {
            kept.push_back(lines[i]);
            continue;
        }
        std::string lang = to_lower(trim(t.substr(3)));
        bool isInput = starts_with(lang, "input");
        bool isOutput = starts_with(lang, "output");
        if (!isInput && !isOutput) {
            kept.push_back(lines[i]);
            continue;
        }
        int index = to_int(lang.substr(5), 0);  // "input1" -> 1
        std::string body;
        size_t j = i + 1;
        for (; j < lines.size(); j++) {
            if (starts_with(trim(lines[j]), "```")) break;
            body += lines[j] + "\n";
        }
        if (index <= 0) index = static_cast<int>(out.size()) + 1;
        while (static_cast<int>(out.size()) < index) out.push_back(ImportedProblem::Sample());
        if (isInput) out[static_cast<size_t>(index) - 1].input = body;
        else out[static_cast<size_t>(index) - 1].output = body;
        found = true;
        i = j;  // 跳过结束围栏
    }
    std::string result = join(kept, "\n");
    return result;
}

bool parse_hydro(const std::string& data, std::vector<ImportedProblem>& out, std::string& err) {
    ZipReader zip;
    if (!zip.open_data(data, err)) return false;

    // 题目目录 = 含 problem.yaml 的那一层（也可能是压缩包根目录）
    std::set<std::string> dirs;
    bool rootHasYaml = false;
    for (const ZipEntryInfo& e : zip.entries()) {
        if (e.name == "problem.yaml") rootHasYaml = true;
        size_t slash = e.name.find_last_of('/');
        if (slash == std::string::npos) continue;
        std::string dir = e.name.substr(0, slash + 1);
        if (zip.has(dir + "problem.yaml")) dirs.insert(dir);
    }
    if (dirs.empty() && rootHasYaml) dirs.insert(std::string());
    if (dirs.empty()) {
        err = "压缩包里找不到 problem.yaml（不是 Hydro 导出的题目包）";
        return false;
    }

    for (const std::string& dir : dirs) {
        ImportedProblem p;
        std::string yamlText;
        if (!zip.read(dir + "problem.yaml", yamlText)) {
            err = str("读取 {}problem.yaml 失败：{}", dir, zip.error());
            return false;
        }
        YamlValue meta;
        std::string yerr;
        if (!parse_yaml(yamlText, meta, yerr)) {
            err = str("{}problem.yaml 解析失败：{}", dir, yerr);
            return false;
        }
        p.title = trim(meta.find("title") ? meta.find("title")->as_string() : std::string());
        if (p.title.empty() && meta.find("name")) p.title = trim(meta.find("name")->as_string());
        if (meta.find("pid")) p.pid = trim(meta.find("pid")->as_string());
        if (const YamlValue* tags = meta.find("tag")) {
            for (const YamlValue& t : tags->seq) {
                std::string v = trim(t.as_string());
                if (!v.empty()) p.tags.push_back(v);
            }
        }
        if (const YamlValue* content = meta.find("content")) {
            p.fullStatement = trim(content->as_string());
        }

        // 题面：problem.md 优先，其次 problem_<lang>.md
        std::string statementPath;
        if (zip.has(dir + "problem.md")) {
            statementPath = dir + "problem.md";
        } else {
            for (const ZipEntryInfo& e : zip.entries()) {
                size_t slash = e.name.find_last_of('/');
                std::string leaf = slash == std::string::npos ? e.name : e.name.substr(slash + 1);
                if (!starts_with(leaf, "problem_")) continue;
                if (!ends_with(leaf, ".md") && !ends_with(leaf, ".markdown")) continue;
                statementPath = e.name;
                break;
            }
        }
        if (!statementPath.empty()) {
            std::string text2;
            if (zip.read(statementPath, text2)) {
                text2 = trim(text2);
                // {zh: "...", en: "..."} 这种本地化内容
                if (!text2.empty() && text2.front() == '{') {
                    Json localized;
                    if (Json::parse(text2, localized) && localized.is_object()) {
                        std::string picked;
                        for (const char* key : {"zh", "zh-CN", "zh_cn", "en"}) {
                            if (localized.has(key) && localized[key].is_string()) {
                                picked = localized[key].as_string();
                                break;
                            }
                        }
                        if (picked.empty() && !localized.keys().empty()) {
                            const Json& first = localized[localized.keys()[0]];
                            if (first.is_string()) picked = first.as_string();
                        }
                        if (!picked.empty()) text2 = picked;
                    }
                }
                bool found = false;
                std::string stripped = extract_hydro_samples(text2, p.samples, found);
                if (found) {
                    if (stripped.find("{{samples}}") == std::string::npos) {
                        stripped += "\n\n## 样例\n\n{{samples}}\n";
                    }
                }
                p.fullStatement = trim(stripped);
            }
        }
        if (p.fullStatement.empty()) {
            p.notes.push_back("压缩包里没有题面（problem.md）");
            p.fullStatement = "# " + (p.title.empty() ? std::string("导入的题目") : p.title) + "\n";
        }

        // 配置
        YamlValue cfg;
        std::string configPath = dir + "testdata/config.yaml";
        if (!zip.has(configPath)) configPath = zip.find_basename("config.yaml");
        if (!configPath.empty()) {
            std::string cfgText;
            if (zip.read(configPath, cfgText)) {
                std::string cerr2;
                if (!parse_yaml(cfgText, cfg, cerr2)) {
                    p.notes.push_back(str("testdata/config.yaml 解析失败（{}），已忽略其中的时限/内存/checker 设置",
                                          cerr2));
                }
            }
        }
        if (const YamlValue* t = cfg.find("time")) {
            p.timeLimitMs = time_ms_from(t->as_string("1s"), "");
        }
        if (const YamlValue* m = cfg.find("memory")) {
            p.memoryLimitKb = memory_kb_from(m->as_string("256m"), "");
        }
        if (const YamlValue* type = cfg.find("type")) {
            std::string v = to_lower(type->as_string());
            if (v == "interactive" || v == "communication") p.interactive = true;
            if (v == "objective" || v == "submit_answer") {
                p.notes.push_back(str("原题类型是 {}，poly 只支持传统题与交互题", v));
            }
        }
        if (const YamlValue* ck = cfg.find("checker")) {
            std::string file = ck->as_string();
            if (file.empty() && ck->kind == YamlValue::Kind::Map) {
                if (const YamlValue* f = ck->find("file")) file = f->as_string();
            }
            if (!file.empty()) {
                std::string entry = dir + "testdata/" + file;
                if (!zip.has(entry)) entry = zip.find_basename(file);
                if (!entry.empty() && zip.read(entry, p.checkerSource)) {
                    p.checkerLanguage = "C++";
                } else {
                    p.notes.push_back(str("找不到 checker 源文件 {}，导入后需要自行补上", file));
                }
            }
        }
        if (const YamlValue* inter = cfg.find("interactor")) {
            std::string file = inter->as_string();
            if (file.empty() && inter->kind == YamlValue::Kind::Map) {
                if (const YamlValue* f = inter->find("file")) file = f->as_string();
            }
            if (!file.empty()) {
                std::string entry = dir + "testdata/" + file;
                if (!zip.has(entry)) entry = zip.find_basename(file);
                if (!entry.empty() && zip.read(entry, p.interactorSource)) p.interactive = true;
            }
        }
        if (const YamlValue* fname = cfg.find("filename")) {
            if (!fname->as_string().empty()) {
                p.notes.push_back(str("原题使用文件 IO（{}.*），poly 只支持标准输入输出",
                                      fname->as_string()));
            }
        }

        // 测试点：优先按配置里的 subtasks/cases 顺序
        std::vector<std::pair<std::string, std::string>> pairs;
        if (const YamlValue* subtasks = cfg.find("subtasks")) {
            for (const YamlValue& st : subtasks->seq) {
                const YamlValue* cases = st.kind == YamlValue::Kind::Map ? st.find("cases") : nullptr;
                if (!cases) continue;
                for (const YamlValue& cs : cases->seq) {
                    std::string in, outp;
                    if (cs.kind == YamlValue::Kind::Map) {
                        if (const YamlValue* a = cs.find("input")) in = a->as_string();
                        if (const YamlValue* b = cs.find("output")) outp = b->as_string();
                    }
                    if (!in.empty()) pairs.push_back({in, outp});
                }
            }
        } else if (const YamlValue* flat = cfg.find("cases")) {
            for (const YamlValue& cs : flat->seq) {
                std::string in, outp;
                if (cs.kind == YamlValue::Kind::Map) {
                    if (const YamlValue* a = cs.find("input")) in = a->as_string();
                    if (const YamlValue* b = cs.find("output")) outp = b->as_string();
                }
                if (!in.empty()) pairs.push_back({in, outp});
            }
        }

        if (pairs.empty()) {
            // 退化为按 *.in / *.out|*.ans 配对
            std::map<std::string, std::pair<std::string, std::string>> found;
            std::string prefix = dir + "testdata/";
            for (const ZipEntryInfo& e : zip.entries()) {
                if (e.name.compare(0, std::min(prefix.size(), e.name.size()), prefix) != 0) continue;
                std::string leaf = e.name.substr(prefix.size());
                if (leaf.empty() || leaf.find('/') != std::string::npos) continue;
                std::string lower = to_lower(leaf);
                if (lower == "config.yaml" || lower == "config.yml") continue;
                std::string stem = fs::stem(leaf);
                std::string ext = to_lower(fs::extension(leaf));
                if (ext == "in") found[stem].first = leaf;
                else if (ext == "out" || ext == "ans") found[stem].second = leaf;
            }
            for (const auto& kv : found) {
                if (kv.second.first.empty()) continue;
                pairs.push_back({kv.second.first, kv.second.second});
            }
            if (!pairs.empty()) p.notes.push_back("配置里没有测试点清单，按文件名配对测试点");
        }

        for (const auto& pair : pairs) {
            ImportedProblem::Case c;
            c.name = fs::basename(pair.first);
            std::string entry = dir + "testdata/" + pair.first;
            if (!zip.has(entry)) entry = zip.find_basename(pair.first);
            if (entry.empty() || !zip.read(entry, c.input)) {
                p.notes.push_back(str("缺少测试点输入 {}", pair.first));
                continue;
            }
            if (!pair.second.empty() && pair.second != "/dev/null") {
                std::string outEntry = dir + "testdata/" + pair.second;
                if (!zip.has(outEntry)) outEntry = zip.find_basename(pair.second);
                if (!outEntry.empty()) c.hasOutput = zip.read(outEntry, c.output);
            }
            p.tests.push_back(c);
        }
        if (p.tests.empty() && !p.samples.empty()) {
            p.notes.push_back("包里没有测试点文件，只有题面里的样例");
        }

        // 题解（solution/ 目录里的 markdown）
        for (const ZipEntryInfo& e : zip.entries()) {
            if (!ends_with(to_lower(e.name), ".md")) continue;
            if (e.name.compare(0, std::min((dir + "solution/").size(), e.name.size()),
                               dir + "solution/") != 0) {
                continue;
            }
            std::string text3;
            if (zip.read(e.name, text3) && !trim(text3).empty()) p.tutorial = trim(text3);
        }

        if (p.title.empty()) p.title = fs::basename(dir.empty() ? std::string("hydro") : dir);
        out.push_back(std::move(p));
    }
    return true;
}

// ================================================================ HOJ

// HOJ 的 examples 字段是拼在一起的样例块：
//   <input>样例输入</input><output>样例输出</output>...
// （hoj-vue/src/common/utils.js 的 stringToExamples 就是这么解的）
std::vector<ImportedProblem::Sample> parse_examples_block(const std::string& text) {
    std::vector<ImportedProblem::Sample> out;
    size_t pos = 0;
    while (true) {
        size_t inStart = text.find("<input>", pos);
        if (inStart == std::string::npos) break;
        size_t inEnd = text.find("</input>", inStart);
        if (inEnd == std::string::npos) break;
        size_t outStart = text.find("<output>", inEnd);
        if (outStart == std::string::npos) break;
        size_t outEnd = text.find("</output>", outStart);
        if (outEnd == std::string::npos) break;
        ImportedProblem::Sample s;
        s.input = text.substr(inStart + 7, inEnd - (inStart + 7));
        s.output = text.substr(outStart + 8, outEnd - (outStart + 8));
        if (!trim(s.input).empty() || !trim(s.output).empty()) out.push_back(s);
        pos = outEnd + 9;
    }
    return out;
}

// 测试数据在 <key>/ 目录里。别人重新打包时可能多套一层目录，所以先按精确路径找，
// 找不到再在 <key>/ 下面按叶子名找（不会跨题串到别的目录）。
bool hoj_read_case(ZipReader& zip, const std::string& key, const std::string& name,
                   std::string& content) {
    std::string direct = key + "/" + name;
    if (zip.has(direct)) return zip.read(direct, content);
    std::string suffix = "/" + name;
    for (const ZipEntryInfo& e : zip.entries()) {
        std::string path = replace_all(e.name, "\\", "/");
        if (!starts_with(path, key + "/")) continue;
        if (!ends_with(path, suffix)) continue;
        return zip.read(e.name, content);
    }
    return false;
}

bool parse_hoj(const std::string& data, std::vector<ImportedProblem>& out, std::string& err) {
    ZipReader zip;
    if (!zip.open_data(data, err)) return false;

    // 只看根目录下的 json：HOJ 的 importProblem 同样要求根目录「除 json 就是题目目录」，
    // 且 json 文件名（去 .json）必须与测试数据目录同名。
    std::vector<std::string> manifests;
    for (const ZipEntryInfo& e : zip.entries()) {
        std::string path = replace_all(e.name, "\\", "/");
        if (path.find('/') != std::string::npos) continue;
        if (!ends_with(to_lower(path), ".json")) continue;
        manifests.push_back(e.name);
    }
    if (manifests.empty()) {
        err = "压缩包里找不到根目录的 <编号>.json（不是 HOJ 导出的题目包）";
        return false;
    }
    std::sort(manifests.begin(), manifests.end());

    for (const std::string& manifest : manifests) {
        std::string key = fs::basename(replace_all(manifest, "\\", "/"));
        key = key.substr(0, key.size() - std::string(".json").size());
        std::string text;
        if (!zip.read(manifest, text)) {
            err = str("读取 {} 失败：{}", manifest, zip.error());
            return false;
        }
        Json doc;
        std::string perr;
        if (!Json::parse(text, doc, &perr)) {
            err = str("{} 不是有效的 JSON：{}", manifest, perr);
            return false;
        }
        const Json& prob = doc["problem"];
        if (!prob.is_object()) {
            err = str("{} 里没有 problem 字段（不是 HOJ 的 ImportProblemVO 结构）", manifest);
            return false;
        }

        ImportedProblem p;
        p.title = trim(prob["title"].as_string());
        p.pid = trim(prob["problemId"].as_string());
        p.timeLimitMs = clamp_time_ms(static_cast<double>(prob["timeLimit"].as_int(1000)));
        p.memoryLimitKb = prob["memoryLimit"].as_int(65536);  // HOJ 的 memory_limit 就是 KB
        if (p.memoryLimitKb < 1) p.memoryLimitKb = 65536;
        p.description = html_to_markdown(prob["description"].as_string());
        p.input = html_to_markdown(prob["input"].as_string());
        p.output = html_to_markdown(prob["output"].as_string());
        p.hint = html_to_markdown(prob["hint"].as_string());
        p.samples = parse_examples_block(prob["examples"].as_string());

        if (doc.has("tags") && doc["tags"].is_array()) {
            for (const Json& t : doc["tags"].items()) {
                if (t.is_string() && !trim(t.as_string()).empty()) {
                    p.tags.push_back(trim(t.as_string()));
                }
            }
        }
        std::string source = trim(prob["source"].as_string());
        if (!source.empty()) p.notes.push_back(str("原题来源：{}", collapse_space(source)));

        // 特判 / 交互程序都放在 spjCode + spjLanguage 里
        std::string mode = to_lower(trim(prob["judgeMode"].as_string("default")));
        if (mode == "spj" || mode == "interactive") {
            std::string code = prob["spjCode"].as_string();
            if (trim(code).empty()) {
                p.notes.push_back(
                    str("原题 judgeMode 是 {}，但包里没带 spjCode，导入后需要自己补 checker", mode));
            } else if (mode == "interactive") {
                p.interactive = true;
                p.interactorSource = code;
            } else {
                p.checkerLanguage = prob["spjLanguage"].as_string("C++");
                p.checkerSource = code;
            }
        } else if (mode != "default" && !mode.empty()) {
            p.notes.push_back(str("未知的 judgeMode「{}」，按普通题导入", mode));
        }

        bool oi = prob["type"].as_int(0) == 1;
        if (oi) p.notes.push_back("原题是 OI 题型（按测试点计分），poly 只判对错");
        std::string caseMode = to_lower(trim(prob["judgeCaseMode"].as_string("default")));
        if (caseMode == "subtask_lowest" || caseMode == "subtask_average") {
            p.notes.push_back(str("原题测试点按子任务（{}）计分，poly 按单点计分", caseMode));
        }
        if (prob["isFileIO"].as_bool(false)) p.notes.push_back("原题用文件 IO（freopen），poly 按标准输入输出出题");
        if (doc.has("codeTemplates") && doc["codeTemplates"].is_array() &&
            !doc["codeTemplates"].items().empty()) {
            p.notes.push_back("包里带了代码模板（codeTemplates），poly 没有该字段，已忽略");
        }

        // 测试点：samples[] 给出的是判题数据文件名，内容在同名目录里
        if (doc.has("samples") && doc["samples"].is_array()) {
            for (const Json& s : doc["samples"].items()) {
                std::string inName = fs::basename(replace_all(s["input"].as_string(), "\\", "/"));
                std::string outName = fs::basename(replace_all(s["output"].as_string(), "\\", "/"));
                if (inName.empty()) continue;
                ImportedProblem::Case c;
                c.name = inName;
                if (!hoj_read_case(zip, key, inName, c.input)) {
                    p.notes.push_back(str("缺少测试点输入 {}", inName));
                    continue;
                }
                if (!outName.empty()) c.hasOutput = hoj_read_case(zip, key, outName, c.output);
                p.tests.push_back(c);
            }
        }
        if (p.tests.empty()) {
            // 没有 samples 清单时退化为按 <key>/ 下的 *.in / *.out 配对
            std::map<std::string, std::pair<std::string, std::string>> found;
            for (const ZipEntryInfo& e : zip.entries()) {
                std::string path = replace_all(e.name, "\\", "/");
                if (!starts_with(path, key + "/")) continue;
                std::string leaf = path.substr(key.size() + 1);
                if (leaf.empty() || leaf.find('/') != std::string::npos) continue;
                std::string stem = fs::stem(leaf);
                std::string ext = to_lower(fs::extension(leaf));
                if (ext == "in") found[stem].first = e.name;
                else if (ext == "out" || ext == "ans") found[stem].second = e.name;
            }
            for (const auto& kv : found) {
                if (kv.second.first.empty()) continue;
                ImportedProblem::Case c;
                c.name = fs::basename(kv.second.first);
                if (!zip.read(kv.second.first, c.input)) continue;
                if (!kv.second.second.empty()) c.hasOutput = zip.read(kv.second.second, c.output);
                p.tests.push_back(c);
            }
            if (!p.tests.empty()) p.notes.push_back("json 里没有 samples 清单，按目录文件名配对测试点");
        }

        if (p.title.empty()) {
            err = str("{} 里没有 title 字段", manifest);
            return false;
        }
        if (p.tests.empty()) {
            p.notes.push_back("这道题没有带测试数据（原包可能只含题面）");
        }
        out.push_back(std::move(p));
    }
    return true;
}

// ================================================================ 落盘

std::string sanitize_problem_name(const std::string& raw) {
    std::string out;
    for (char c : raw) {
        unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20) continue;
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
            continue;
        out.push_back(c);
    }
    out = trim(out);
    while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && out.front() == '.') out.erase(out.begin());
    if (out.size() > 60) out = out.substr(0, 60);
    return out;
}

bool name_taken(const std::string& workspace, const std::string& name) {
    std::string dir = fs::join(workspace, name);
    return fs::is_dir(dir) && fs::is_file(fs::join(dir, "problem.json"));
}

std::string unique_problem_name(const std::string& workspace, const std::string& preferred) {
    std::string base = sanitize_problem_name(preferred);
    if (base.empty()) base = "imported";
    if (!name_taken(workspace, base)) return base;
    for (int i = 2; i < 1000; i++) {
        std::string candidate = base + "-" + num(i);
        if (!name_taken(workspace, candidate)) return candidate;
    }
    return base + "-" + num(static_cast<long long>(std::time(nullptr)));
}

std::string sample_fences(const std::vector<ImportedProblem::Sample>& samples) {
    std::string out;
    for (size_t i = 0; i < samples.size(); i++) {
        out += "**样例输入 #" + num(static_cast<long long>(i + 1)) + "**\n\n```\n" +
               rtrim(strip_cr(samples[i].input)) + "\n```\n\n";
        if (!trim(samples[i].output).empty()) {
            out += "**样例输出 #" + num(static_cast<long long>(i + 1)) + "**\n\n```\n" +
                   rtrim(strip_cr(samples[i].output)) + "\n```\n\n";
        }
    }
    return out;
}

std::string compose_statement(const ImportedProblem& p) {
    if (!p.fullStatement.empty()) return p.fullStatement;
    std::string title = p.title.empty() ? std::string("题目") : p.title;
    std::string md = "# " + title + "\n\n";
    if (!trim(p.description).empty()) md += "## 题目描述\n\n" + trim(p.description) + "\n\n";
    if (!trim(p.input).empty()) md += "## 输入格式\n\n" + trim(p.input) + "\n\n";
    if (!trim(p.output).empty()) md += "## 输出格式\n\n" + trim(p.output) + "\n\n";
    if (p.tests.empty() && !p.samples.empty()) {
        md += "## 样例\n\n" + sample_fences(p.samples);
    } else {
        md += "## 样例\n\n{{samples}}\n\n";
    }
    if (!trim(p.hint).empty()) {
        std::string hint = trim(p.hint);
        // hint 里可能已经带着「## 数据范围」这种小标题，不要再多套一层「提示」
        if (starts_with(hint, "#")) md += hint + "\n\n";
        else md += "## 提示\n\n" + hint + "\n\n";
    }
    return md;
}

bool write_imported_problem(const Context& ctx, ImportFormat format, const std::string& filename,
                            ImportedProblem& src, ImportOutcome& result, std::string& err) {
    // 目录名优先用包里的编号（更接近原题 ID），没有就用标题
    std::string preferred = !trim(src.pid).empty() ? trim(src.pid) : src.title;
    std::string name = unique_problem_name(ctx.workspace, preferred);
    std::string dir = fs::join(ctx.workspace, name);
    for (const char* sub : {"files", "solutions", "statements", "tests", "output/answers", "stress"}) {
        fs::mkdirs(fs::join(dir, sub));
    }

    const int digits = 2;
    int index = 0;
    for (const ImportedProblem::Case& c : src.tests) {
        index++;
        std::string base = lpad(num(index), static_cast<size_t>(digits));
        if (!fs::write_file(fs::join(dir, fs::join("tests", base)), c.input)) {
            err = str("无法写入 {} 的测试点", name);
            return false;
        }
        if (c.hasOutput) {
            if (!fs::write_file(fs::join(dir, fs::join("output/answers", base)), c.output)) {
                err = str("无法写入 {} 的标准答案", name);
                return false;
            }
        }
    }

    fs::write_file(fs::join(dir, "statements/statement.md"), compose_statement(src) + "\n");
    if (!trim(src.tutorial).empty()) {
        fs::write_file(fs::join(dir, "statements/tutorial.md"), trim(src.tutorial) + "\n");
    }

    Problem p;
    p.name = name;
    p.dir = fs::absolute(dir);
    p.timeLimitMs = static_cast<int>(src.timeLimitMs);
    p.memoryLimitKb = src.memoryLimitKb;
    p.interactive = src.interactive;
    p.multitest = false;
    p.tags = src.tags;
    p.note = str("从 {} 导入（{}）", import_format_name(format), filename);
    p.generators.clear();
    p.validator.clear();
    p.checkerFile.clear();
    p.interactor.clear();
    p.checker = "ncmp";

    bool wroteChecker = false;
    if (!trim(src.checkerSource).empty()) {
        std::string lang = to_lower(src.checkerLanguage);
        bool cpp = lang.empty() || lang == "c++" || lang == "cpp" || lang == "g++";
        std::string rel = fs::join("files", cpp ? "checker.cpp" : "checker.c");
        if (fs::write_file(fs::join(dir, rel), src.checkerSource)) {
            p.checkerFile = rel;
            p.checker = "custom";
            wroteChecker = true;
        }
    }
    if (!trim(src.interactorSource).empty()) {
        std::string rel = fs::join("files", "interactor.cpp");
        if (fs::write_file(fs::join(dir, rel), src.interactorSource)) {
            p.interactor = rel;
            p.interactive = true;
        }
    }
    if (!trim(src.mainSolution).empty()) {
        fs::write_file(fs::join(dir, fs::join("solutions", "main.cpp")), src.mainSolution);
    } else {
        src.notes.push_back("包里没有 C++ 标程，评测前请把标程放到 solutions/main.cpp");
    }

    // 没有生成器，留个空脚本占位（poly gen 不会做任何事），避免其它命令找不到文件。
    fs::write_file(fs::join(dir, "files/testscript.txt"),
                   "# 本题目由导入生成，测试数据已在 tests/ 下。\n"
                   "# 若要用生成器重造数据，请先写 files/gen.cpp，再把生成命令写在这里。\n");

    for (int i = 1; i <= index && i <= 2; i++) p.sampleTests.push_back(i);

    std::string saveErr;
    if (!p.save(saveErr)) {
        err = saveErr;
        return false;
    }

    std::string readme = "# " + (src.title.empty() ? name : src.title) + "\n\n";
    readme += str("由 `poly ui` 从 {} 包 `{}` 导入。\n\n", import_format_name(format), filename);
    readme += "```\n";
    readme += str("题目名    {}\n", name);
    readme += str("时限/内存 {}/{} MB\n", str("{} ms", src.timeLimitMs), src.memoryLimitKb / 1024);
    readme += str("测试点    {}\n", index);
    readme += str("checker   {}\n", wroteChecker ? "已导入（files/checker.cpp）" : "内置 ncmp");
    readme += str("交互题    {}\n", p.interactive ? "是" : "否");
    readme += "```\n\n";
    if (!src.tags.empty()) readme += "题目来源/标签：" + join(src.tags, " ") + "\n\n";
    if (!src.notes.empty()) {
        readme += "## 导入提示\n\n";
        for (const std::string& n : src.notes) readme += "- " + n + "\n";
        readme += "\n";
    }
    readme += "## 建议的后续操作\n\n";
    readme += "```bash\n";
    readme += str("poly build {}\n", name);
    readme += str("poly test {}\n", name);
    readme += str("poly statement {}\n", name);
    readme += str("poly package {} --format hydro\n", name);
    readme += "```\n";
    fs::write_file(fs::join(dir, "README.md"), readme);

    result.name = name;
    result.dir = dir;
    result.tests = index;
    result.interactive = p.interactive;
    result.hasChecker = wroteChecker;
    result.notes = src.notes;
    return true;
}

}  // namespace

bool parse_import_format(const std::string& name, ImportFormat& out) {
    std::string v = to_lower(trim(name));
    if (v == "qduoj" || v == "qd" || v == "qdu") {
        out = ImportFormat::Qduoj;
        return true;
    }
    if (v == "fps" || v == "freeproblemset" || v == "hustoj" || v == "xml") {
        out = ImportFormat::Fps;
        return true;
    }
    if (v == "hydro" || v == "hydrooj") {
        out = ImportFormat::Hydro;
        return true;
    }
    // HOJ 原生包（后端 ImportProblemVO）；以前 hoj 被当成 Hydro 的别名，现已分开。
    if (v == "hoj" || v == "hcode" || v == "hoj-zip" || v == "hoj-native") {
        out = ImportFormat::Hoj;
        return true;
    }
    return false;
}

const char* import_format_name(ImportFormat format) {
    switch (format) {
        case ImportFormat::Qduoj: return "QDUOJ";
        case ImportFormat::Fps: return "FPS";
        case ImportFormat::Hydro: return "Hydro";
        case ImportFormat::Hoj: return "HOJ";
    }
    return "未知";
}

bool parse_problem_package(ImportFormat format, const std::string& filename, const std::string& data,
                           std::vector<ImportedProblem>& out, std::string& err) {
    (void)filename;
    bool ok = false;
    switch (format) {
        case ImportFormat::Qduoj: ok = parse_qduoj(data, out, err); break;
        case ImportFormat::Fps: ok = parse_fps(data, out, err); break;
        case ImportFormat::Hydro: ok = parse_hydro(data, out, err); break;
        case ImportFormat::Hoj: ok = parse_hoj(data, out, err); break;
    }
    if (!ok) return false;
    if (out.empty()) {
        err = "包里没有可导入的题目";
        return false;
    }
    return true;
}

bool import_into_workspace(const Context& ctx, ImportFormat format, const std::string& filename,
                           const std::string& data, std::vector<ImportOutcome>& out,
                           std::string& err) {
    std::vector<ImportedProblem> parsed;
    if (!parse_problem_package(format, filename, data, parsed, err)) return false;
    if (!fs::mkdirs(ctx.workspace)) {
        err = str("无法创建工作区 {}", ctx.workspace);
        return false;
    }
    std::string firstError;
    for (ImportedProblem& p : parsed) {
        ImportOutcome outcome;
        std::string perr;
        if (!write_imported_problem(ctx, format, filename, p, outcome, perr)) {
            if (firstError.empty()) firstError = perr;
            continue;
        }
        out.push_back(outcome);
    }
    if (out.empty()) {
        err = firstError.empty() ? "导入失败" : firstError;
        return false;
    }
    return true;
}

}  // namespace poly
