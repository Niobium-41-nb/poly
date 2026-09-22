#include "script.h"

#include <cstdint>

#include "format.h"
#include "strutil.h"

namespace poly {

namespace {

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

std::string strip_comment(const std::string& line) {
    std::string out;
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '#') break;
        if (line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/') break;
        out.push_back(line[i]);
    }
    return out;
}

bool parse_target(const std::string& spec, int& from, int& to, std::string& err) {
    std::string s = trim(spec);
    if (s.empty()) {
        err = "缺少测试点编号";
        return false;
    }
    size_t dash = s.find('-');
    if (dash == std::string::npos) {
        if (!is_integer(s)) {
            err = str("非法的测试点编号 {}", s);
            return false;
        }
        from = to = static_cast<int>(to_int(s));
        return true;
    }
    std::string a = trim(s.substr(0, dash));
    std::string b = trim(s.substr(dash + 1));
    if (!is_integer(a) || !is_integer(b)) {
        err = str("非法的测试点范围 {}", s);
        return false;
    }
    from = static_cast<int>(to_int(a));
    to = static_cast<int>(to_int(b));
    if (from <= 0 || to < from) {
        err = str("非法的测试点范围 {}", s);
        return false;
    }
    return true;
}

}  // namespace

bool parse_test_script(const std::string& text, Script& out, std::string& err) {
    out = Script();
    std::vector<std::string> lines = split_lines(strip_cr(text));
    for (size_t n = 0; n < lines.size(); n++) {
        std::string line = trim(strip_comment(lines[n]));
        if (line.empty()) continue;
        std::vector<std::string> tokens = split_words(line);

        ScriptCommand cmd;
        cmd.line = static_cast<int>(n + 1);

        std::vector<std::string> body;
        std::string targetSpec;
        for (size_t i = 0; i < tokens.size(); i++) {
            const std::string& t = tokens[i];
            if (t == ">") {
                for (size_t j = i + 1; j < tokens.size(); j++) {
                    if (!targetSpec.empty()) targetSpec += " ";
                    targetSpec += tokens[j];
                }
                break;
            }
            if (t.size() > 1 && t[0] == '>') {
                targetSpec = t.substr(1);
                while (i + 1 < tokens.size()) targetSpec += " " + tokens[++i];
                break;
            }
            body.push_back(t);
        }
        if (body.empty()) {
            err = str("第 {} 行：缺少生成命令", cmd.line);
            return false;
        }
        if (!targetSpec.empty()) {
            std::string terr;
            if (!parse_target(targetSpec, cmd.from, cmd.to, terr)) {
                err = str("第 {} 行：{}", cmd.line, terr);
                return false;
            }
            cmd.hasTarget = true;
        }

        std::vector<std::string> stage;
        for (size_t i = 0; i < body.size(); i++) {
            if (body[i] == "|") {
                if (stage.empty()) {
                    err = str("第 {} 行：管道中有空的命令", cmd.line);
                    return false;
                }
                cmd.stages.push_back(stage);
                stage.clear();
                continue;
            }
            stage.push_back(body[i]);
        }
        if (stage.empty()) {
            err = str("第 {} 行：管道中有空的命令", cmd.line);
            return false;
        }
        cmd.stages.push_back(stage);
        out.commands.push_back(cmd);
    }
    return true;
}

std::vector<std::string> expand_script_args(const std::vector<std::string>& args, int index,
                                            const std::string& seedSource) {
    std::string seed = num(static_cast<long long>(fnv1a(seedSource + "#" + num(index)) % 1000000007LL));
    std::vector<std::string> out;
    out.reserve(args.size());
    for (const std::string& a : args) {
        std::string v = replace_all(a, "{i}", num(index));
        v = replace_all(v, "{index}", num(index));
        v = replace_all(v, "{seed}", seed);
        out.push_back(v);
    }
    return out;
}

}  // namespace poly
