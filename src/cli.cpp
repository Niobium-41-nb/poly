#include "cli.h"

#include <algorithm>
#include <thread>

#include "commands.h"
#include "format.h"
#include "fsutil.h"
#include "log.h"
#include "plat.h"
#include "strutil.h"

namespace poly {

namespace {

const std::set<std::string> kFlagOptions = {
    "verbose", "quiet", "no-color", "force", "all", "keep", "pdf", "html", "open", "no-open",
    "validate", "no-validate", "stop-on-fail", "keep-going", "rebuild", "json", "strict",
    "no-check", "help", "version", "clean", "list", "yes", "dry-run", "samples-only",
    "no-interactive", "show-verdicts", "profile", "qduoj", "fps", "hydro", "hoj", "polygon",
    "no-solutions", "web", "no-open",
};

const std::map<std::string, std::string> kShortOptions = {
    {"v", "verbose"}, {"q", "quiet"},   {"p", "problem"}, {"s", "solution"}, {"t", "test"},
    {"n", "iterations"}, {"o", "output"}, {"j", "jobs"},  {"w", "workspace"}, {"g", "gen"},
    {"c", "checker"}, {"h", "help"},
};

}  // namespace

bool Args::has(const std::string& key) const { return values.count(key) > 0; }

bool Args::has_flag(const std::string& key) const { return flags.count(key) > 0; }

std::string Args::get(const std::string& key, const std::string& def) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return def;
    return it->second.back();
}

std::vector<std::string> Args::get_all(const std::string& key) const {
    auto it = values.find(key);
    if (it == values.end()) return {};
    return it->second;
}

long long Args::get_int(const std::string& key, long long def) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return def;
    return to_int(it->second.back(), def);
}

double Args::get_double(const std::string& key, double def) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return def;
    return to_double(it->second.back(), def);
}

bool Args::get_bool(const std::string& key, bool def) const {
    if (!has(key)) return def;
    std::string v = to_lower(get(key));
    if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
    if (v == "0" || v == "false" || v == "no" || v == "off") return false;
    return def;
}

bool parse_args(const std::vector<std::string>& raw, Args& out, std::string& err) {
    size_t i = 0;
    if (!raw.empty()) {
        out.command = raw[0];
        i = 1;
    }
    for (; i < raw.size(); i++) {
        const std::string& tok = raw[i];
        if (tok == "--") {
            for (size_t k = i + 1; k < raw.size(); k++) out.positional.push_back(raw[k]);
            break;
        }
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] == '-') {
            std::string body = tok.substr(2);
            std::string key = body;
            std::string value;
            bool hasValue = false;
            size_t eq = body.find('=');
            if (eq != std::string::npos) {
                key = body.substr(0, eq);
                value = body.substr(eq + 1);
                hasValue = true;
            }
            key = to_lower(key);
            if (hasValue) {
                out.values[key].push_back(value);
            } else if (kFlagOptions.count(key)) {
                out.flags.insert(key);
            } else if (i + 1 < raw.size() && (raw[i + 1].size() < 2 || raw[i + 1][0] != '-')) {
                out.values[key].push_back(raw[++i]);
            } else {
                out.flags.insert(key);
            }
            continue;
        }
        if (tok.size() >= 2 && tok[0] == '-') {
            std::string body = tok.substr(1);
            auto it = kShortOptions.find(body);
            if (it == kShortOptions.end()) {
                err = str("未知选项: {}", tok);
                return false;
            }
            const std::string& key = it->second;
            if (kFlagOptions.count(key)) {
                out.flags.insert(key);
            } else if (i + 1 < raw.size()) {
                out.values[key].push_back(raw[++i]);
            } else {
                err = str("选项 {} 缺少参数", tok);
                return false;
            }
            continue;
        }
        out.positional.push_back(tok);
    }
    return true;
}

static void load_workspace_config(Context& ctx) {
    std::string cfg = fs::join(ctx.workspace, "poly.json");
    std::string text;
    if (!fs::read_file(cfg, text)) return;
    Json j;
    if (!Json::parse(text, j)) return;
    if (j.has("compiler")) ctx.compiler = j["compiler"].as_string(ctx.compiler);
    if (j.has("cxxflags")) ctx.cxxflags = j["cxxflags"].as_string(ctx.cxxflags);
    if (j.has("std")) ctx.stdFlag = j["std"].as_string(ctx.stdFlag);
    if (j.has("jobs")) ctx.jobs = static_cast<int>(j["jobs"].as_int(ctx.jobs));
}

bool resolve_context(const Args& args, Context& ctx, std::string& err) {
    (void)err;
    ctx.exeDir = fs::exe_dir();

    std::string ws = args.get("workspace");
    if (ws.empty()) ws = env_get("POLY_WORKSPACE");
    if (ws.empty()) {
        std::string cwd = fs::cwd();
        if (fs::is_file(fs::join(cwd, "poly.json")) || fs::is_dir(fs::join(cwd, "problems")))
            ws = fs::join(cwd, "problems");
        else
            ws = fs::join(cwd, "problems");
    }
    ctx.workspace = fs::absolute(ws);

    ctx.problemName = args.get("problem");
    if (ctx.problemName.empty() && !args.positional.empty()) {
        static const std::set<std::string> kNoProblemCommands = {"list", "doctor", "help",
                                                                "version", "testlib", "init",
                                                                "new", "create", "import", "ui"};
        if (!kNoProblemCommands.count(args.command)) ctx.problemName = args.positional[0];
    }

    ctx.compiler = args.get("compiler");
    if (ctx.compiler.empty()) ctx.compiler = env_get("POLY_CXX");
    ctx.cxxflags = args.get("flags");
    ctx.verbose = args.has_flag("verbose");
    ctx.quiet = args.has_flag("quiet");
    ctx.jobs = static_cast<int>(args.get_int("jobs", 0));

    load_workspace_config(ctx);

    if (ctx.compiler.empty()) ctx.compiler = "g++";

    ctx.color = !args.has_flag("no-color") && env_get("NO_COLOR").empty() && stdout_is_tty();
    if (args.has_flag("json") || ctx.quiet) ctx.color = false;
    return true;
}

std::string Context::problemDir() const { return fs::join(workspace, problemName); }

bool Context::requireProblem(Problem& p) const {
    if (problemName.empty()) {
        log_err("未指定题目，请用 -p <name> 或先进入工作区");
        return false;
    }
    std::string err;
    if (!load_problem(workspace, problemName, p, err)) {
        log_err(err);
        return false;
    }
    return true;
}

bool Context::loadProblem(Problem& p) const {
    if (problemName.empty()) return false;
    std::string err;
    return load_problem(workspace, problemName, p, err);
}

int Context::resolvedJobs() const {
    if (jobs > 0) return jobs;
    unsigned n = std::thread::hardware_concurrency();
    if (n == 0) n = 1;
    return static_cast<int>(std::min<unsigned>(n, 8));
}

const CommandSpec* find_command(const std::string& name) {
    for (const CommandSpec* c = command_table(); c->name; c++) {
        if (name == c->name) return c;
    }
    return nullptr;
}

}  // namespace poly
