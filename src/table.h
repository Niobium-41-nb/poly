#pragma once

#include <string>
#include <vector>

namespace poly {

struct Column {
    std::string title;
    std::vector<std::string> cells;

    explicit Column(std::string t) : title(std::move(t)) {}
};

// ASCII box-free table, written to stdout through log_raw.
void print_table(const std::vector<Column>& cols);

}  // namespace poly
