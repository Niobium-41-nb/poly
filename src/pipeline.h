#pragma once

#include <string>
#include <vector>

#include "cli.h"
#include "compile.h"
#include "judge.h"
#include "problem.h"

namespace poly {

struct BuildOptions {
    std::string what = "all";  // all | gen | validator | checker | interactor | solutions
    bool rebuild = false;
    bool quiet = false;
};

// Compiles generators / validator / checker / interactor / solutions, skipping
// artifacts that are newer than their sources.
bool build_problem(Context& ctx, Problem& p, const BuildOptions& opt, Compiler& cc,
                   std::vector<std::string>& errors);

// Builds only the artifacts needed by the requested stage, quietly.
bool ensure_built(Context& ctx, Problem& p, const std::string& what, Compiler& cc, std::string& err);

std::string generator_exe_rel(const std::string& stem, const std::string& suffix);
std::string solution_exe_rel(const std::string& stem, const std::string& suffix);
std::string checker_exe_rel(const Problem& p, const std::string& suffix);
std::string interactor_exe_rel(const Problem& p, const std::string& suffix);

// Test script -> tests/NN
struct GenerateStats {
    int generated = 0;
    int failed = 0;
    std::vector<std::string> errors;
};

GenerateStats generate_tests(Context& ctx, Problem& p, Compiler& cc, const std::vector<int>& only);

// Validator over the given tests (empty = all).
bool validate_tests(Context& ctx, Problem& p, Compiler& cc, const std::vector<int>& only,
                    std::vector<std::string>& errors, int& checked);

std::vector<int> selected_tests(const Problem& p, const std::vector<int>& only, int single);

// Builds the artifacts a judging stage needs and fills in the JudgeEnv paths.
bool prepare_judge(Context& ctx, Problem& p, const std::string& what, JudgeEnv& env, std::string& err);

// Parses "1,3,5-7" into {1,3,5,6,7}.
std::vector<int> parse_test_list(const std::string& spec);

}  // namespace poly
