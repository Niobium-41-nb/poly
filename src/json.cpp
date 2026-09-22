#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "strutil.h"

namespace poly {

static const std::string kEmptyString;

Json Json::array() {
    Json j;
    j.type_ = Type::Array;
    return j;
}

Json Json::object() {
    Json j;
    j.type_ = Type::Object;
    return j;
}

const std::string& Json::as_string() const { return type_ == Type::String ? str_ : kEmptyString; }

size_t Json::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    return 0;
}

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
        str_.clear();
    }
    for (auto& kv : obj_)
        if (kv.first == key) return kv.second;
    obj_.emplace_back(key, Json());
    return obj_.back().second;
}

const Json& Json::operator[](const std::string& key) const {
    static const Json empty;
    for (auto& kv : obj_)
        if (kv.first == key) return kv.second;
    return empty;
}

bool Json::has(const std::string& key) const {
    if (type_ != Type::Object) return false;
    for (auto& kv : obj_)
        if (kv.first == key) return true;
    return false;
}

void Json::set(const std::string& key, Json value) { (*this)[key] = std::move(value); }

void Json::remove(const std::string& key) {
    for (size_t i = 0; i < obj_.size(); i++) {
        if (obj_[i].first == key) {
            obj_.erase(obj_.begin() + static_cast<long>(i));
            return;
        }
    }
}

std::vector<std::string> Json::keys() const {
    std::vector<std::string> out;
    for (auto& kv : obj_) out.push_back(kv.first);
    return out;
}

Json& Json::push_back(Json value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
        str_.clear();
    }
    arr_.push_back(std::move(value));
    return arr_.back();
}

const Json& Json::at(size_t i) const {
    static const Json empty;
    if (type_ == Type::Array && i < arr_.size()) return arr_[i];
    return empty;
}

static void dump_string(std::string& out, const std::string& s) {
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
}

void Json::dump_to(std::string& out, int indent, int depth) const {
    std::string pad(indent > 0 ? static_cast<size_t>(indent) * static_cast<size_t>(depth + 1) : 0, ' ');
    std::string padEnd(indent > 0 ? static_cast<size_t>(indent) * static_cast<size_t>(depth) : 0, ' ');
    const char* nl = indent > 0 ? "\n" : "";
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += bool_ ? "true" : "false"; break;
        case Type::Number: {
            if (std::isfinite(num_) && num_ == std::floor(num_) && std::fabs(num_) < 1e15) {
                out += fmt("%lld", static_cast<long long>(num_));
            } else if (!std::isfinite(num_)) {
                out += "0";
            } else {
                out += fmt("%.10g", num_);
            }
            break;
        }
        case Type::String: dump_string(out, str_); break;
        case Type::Array: {
            if (arr_.empty()) {
                out += "[]";
                break;
            }
            out += "[";
            out += nl;
            for (size_t i = 0; i < arr_.size(); i++) {
                out += pad;
                arr_[i].dump_to(out, indent, depth + 1);
                if (i + 1 < arr_.size()) out += ",";
                out += nl;
            }
            out += padEnd;
            out += "]";
            break;
        }
        case Type::Object: {
            if (obj_.empty()) {
                out += "{}";
                break;
            }
            out += "{";
            out += nl;
            for (size_t i = 0; i < obj_.size(); i++) {
                out += pad;
                dump_string(out, obj_[i].first);
                out += indent > 0 ? ": " : ":";
                obj_[i].second.dump_to(out, indent, depth + 1);
                if (i + 1 < obj_.size()) out += ",";
                out += nl;
            }
            out += padEnd;
            out += "}";
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dump_to(out, indent, 0);
    return out;
}

namespace {

struct Parser {
    const std::string& s;
    size_t i = 0;
    std::string err;

    explicit Parser(const std::string& text) : s(text) {}

    void skip_ws() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                i++;
            } else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') i++;
            } else {
                break;
            }
        }
    }

    bool fail(const std::string& msg) {
        if (err.empty()) err = fmt("offset %zu: %s", i, msg.c_str());
        return false;
    }

    bool parse_value(Json& out) {
        skip_ws();
        if (i >= s.size()) return fail("unexpected end of input");
        char c = s[i];
        switch (c) {
            case '{': return parse_object(out);
            case '[': return parse_array(out);
            case '"': {
                std::string str;
                if (!parse_string(str)) return false;
                out = Json(str);
                return true;
            }
            case 't':
                if (s.compare(i, 4, "true") == 0) {
                    i += 4;
                    out = Json(true);
                    return true;
                }
                return fail("invalid literal");
            case 'f':
                if (s.compare(i, 5, "false") == 0) {
                    i += 5;
                    out = Json(false);
                    return true;
                }
                return fail("invalid literal");
            case 'n':
                if (s.compare(i, 4, "null") == 0) {
                    i += 4;
                    out = Json();
                    return true;
                }
                return fail("invalid literal");
            default: return parse_number(out);
        }
    }

    bool parse_number(Json& out) {
        size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) i++;
        bool any = false;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            i++;
            any = true;
        }
        if (i < s.size() && s[i] == '.') {
            i++;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                i++;
                any = true;
            }
        }
        if (any && i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
            size_t save = i;
            i++;
            if (i < s.size() && (s[i] == '-' || s[i] == '+')) i++;
            bool digits = false;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                i++;
                digits = true;
            }
            if (!digits) i = save;
        }
        if (!any) return fail("invalid number");
        out = Json(strtod(s.substr(start, i - start).c_str(), nullptr));
        return true;
    }

    bool parse_string(std::string& out) {
        if (i >= s.size() || s[i] != '"') return fail("expected string");
        i++;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i >= s.size()) return fail("bad escape");
            char e = s[i++];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (i + 4 > s.size()) return fail("bad \\u escape");
                    unsigned cp = 0;
                    for (int k = 0; k < 4; k++) {
                        char h = s[i + k];
                        unsigned d;
                        if (h >= '0' && h <= '9')
                            d = static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f')
                            d = static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F')
                            d = static_cast<unsigned>(h - 'A' + 10);
                        else
                            return fail("bad hex in \\u escape");
                        cp = cp * 16 + d;
                    }
                    i += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        unsigned lo = 0;
                        bool ok = true;
                        for (int k = 0; k < 4; k++) {
                            char h = s[i + 2 + k];
                            unsigned d;
                            if (h >= '0' && h <= '9')
                                d = static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f')
                                d = static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F')
                                d = static_cast<unsigned>(h - 'A' + 10);
                            else {
                                ok = false;
                                break;
                            }
                            lo = lo * 16 + d;
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            i += 6;
                        }
                    }
                    if (cp < 0x80) {
                        out.push_back(static_cast<char>(cp));
                    } else if (cp < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else if (cp < 0x10000) {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool parse_array(Json& out) {
        out = Json::array();
        i++;
        skip_ws();
        if (i < s.size() && s[i] == ']') {
            i++;
            return true;
        }
        while (true) {
            Json v;
            if (!parse_value(v)) return false;
            out.push_back(std::move(v));
            skip_ws();
            if (i < s.size() && s[i] == ',') {
                i++;
                continue;
            }
            if (i < s.size() && s[i] == ']') {
                i++;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parse_object(Json& out) {
        out = Json::object();
        i++;
        skip_ws();
        if (i < s.size() && s[i] == '}') {
            i++;
            return true;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string(key)) return false;
            skip_ws();
            if (i >= s.size() || s[i] != ':') return fail("expected ':'");
            i++;
            Json v;
            if (!parse_value(v)) return false;
            out.set(key, std::move(v));
            skip_ws();
            if (i < s.size() && s[i] == ',') {
                i++;
                continue;
            }
            if (i < s.size() && s[i] == '}') {
                i++;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }
};

}  // namespace

bool Json::parse(const std::string& text, Json& out, std::string* error) {
    // 编辑器（记事本、VS Code 的“UTF-8 with BOM”）可能在开头写 BOM，
    // 直接交给解析器会报 "offset 0: invalid number"，这里先跳过。
    std::string body = text;
    if (body.size() >= 3 && static_cast<unsigned char>(body[0]) == 0xEF &&
        static_cast<unsigned char>(body[1]) == 0xBB &&
        static_cast<unsigned char>(body[2]) == 0xBF) {
        body.erase(0, 3);
    }
    Parser p(body);
    Json v;
    if (!p.parse_value(v)) {
        if (error) *error = p.err;
        return false;
    }
    p.skip_ws();
    if (p.i != body.size()) {
        if (error) *error = fmt("offset %zu: trailing content", p.i);
        return false;
    }
    out = std::move(v);
    return true;
}

Json Json::parse_or(const std::string& text, Json fallback) {
    Json v;
    if (parse(text, v)) return v;
    return fallback;
}

}  // namespace poly
