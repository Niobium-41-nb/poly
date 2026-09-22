#pragma once

#include <string>

#include "verdict.h"

namespace poly {

struct CheckResult {
    Verdict verdict = Verdict::OK;
    std::string message;
};

bool is_builtin_checker(const std::string& name);
std::string builtin_checker_list();

CheckResult builtin_check(const std::string& name, const std::string& inputPath, const std::string& outputPath,
                          const std::string& answerPath);

}  // namespace poly
