#pragma once

#include <string>
#include <vector>

namespace poly {

struct ScriptCommand {
    int line = 0;
    bool hasTarget = false;
    int from = 0;
    int to = 0;
    std::vector<std::vector<std::string>> stages;  // stage = {command, args...}
};

struct Script {
    std::vector<ScriptCommand> commands;
};

bool parse_test_script(const std::string& text, Script& out, std::string& err);

// Expands {i} / {index} / {seed} inside the arguments of one test.
std::vector<std::string> expand_script_args(const std::vector<std::string>& args, int index,
                                            const std::string& seedSource);

}  // namespace poly
