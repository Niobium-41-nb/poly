#pragma once

#include <string>
#include <vector>

#include "compile.h"
#include "problem.h"
#include "verdict.h"

namespace poly {

struct JudgeLimits {
    double timeLimitMs = 2000;
    long long memoryLimitKb = 262144;
};

struct TestOutcome {
    int test = 0;
    Verdict verdict = Verdict::OK;
    double timeMs = 0;
    double wallMs = 0;
    long long memoryKb = 0;
    std::string message;
    std::string crashLog;
};

struct JudgeEnv {
    Problem problem;
    Compiler compiler;
    std::string checkerExe;     // absolute path or empty
    std::string interactorExe;  // absolute path or empty
    std::string exeSuffix;
};

std::string compiled_name(const std::string& stem, const std::string& suffix);
std::string answer_rel(int test, const Problem& p);
std::string answer_rel_dir();

// Runs `solExe` on test `testIndex` and judges the result.
TestOutcome judge_test(JudgeEnv& env, const std::string& solExe, const std::string& solName, int testIndex,
                       const JudgeLimits& lim, bool check);

// Produces the reference answers with the given solution (defaults to "main").
bool ensure_answers(JudgeEnv& env, const std::string& answerSolution, std::vector<std::string>& errors);

// Interactive judging shares the limits but talks to the interactor over pipes.
TestOutcome judge_interactive(JudgeEnv& env, const std::string& solExe, const std::string& solName, int testIndex,
                              const JudgeLimits& lim);

}  // namespace poly
