#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "problem.h"

namespace poly {

struct Args {
    std::string command;
    std::vector<std::string> positional;
    std::map<std::string, std::vector<std::string>> values;
    std::set<std::string> flags;

    bool has(const std::string& key) const;
    bool has_flag(const std::string& key) const;
    std::string get(const std::string& key, const std::string& def = std::string()) const;
    std::vector<std::string> get_all(const std::string& key) const;
    long long get_int(const std::string& key, long long def) const;
    double get_double(const std::string& key, double def) const;
    bool get_bool(const std::string& key, bool def) const;
};

struct Context {
    std::string exeDir;
    std::string workspace;
    std::string problemName;
    std::string compiler;
    std::string cxxflags;
    std::string stdFlag = "-std=c++17";
    int jobs = 0;
    bool verbose = false;
    bool quiet = false;
    bool color = true;

    std::string problemDir() const;
    bool requireProblem(Problem& p) const;
    bool loadProblem(Problem& p) const;
    int resolvedJobs() const;
};

struct CommandSpec {
    const char* name;
    const char* args;
    const char* desc;
    int (*fn)(Context&, const Args&);
};

bool parse_args(const std::vector<std::string>& raw, Args& out, std::string& err);
bool resolve_context(const Args& args, Context& ctx, std::string& err);
const CommandSpec* find_command(const std::string& name);

}  // namespace poly
