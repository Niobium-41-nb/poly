#include "table.h"

#include <algorithm>

#include "log.h"
#include "strutil.h"

namespace poly {

void print_table(const std::vector<Column>& cols) {
    if (cols.empty()) return;
    std::vector<size_t> width(cols.size(), 0);
    for (size_t i = 0; i < cols.size(); i++) {
        width[i] = display_width(cols[i].title);
        for (const std::string& c : cols[i].cells) width[i] = std::max(width[i], display_width(c));
    }
    std::string line;
    for (size_t i = 0; i < cols.size(); i++) {
        if (i) line += "  ";
        line += pad_display(cols[i].title, width[i]);
    }
    log_raw(line + "\n");
    std::string sep;
    for (size_t i = 0; i < cols.size(); i++) {
        if (i) sep += "  ";
        sep += std::string(width[i], '-');
    }
    log_raw(sep + "\n");
    size_t rows = 0;
    for (const Column& c : cols) rows = std::max(rows, c.cells.size());
    for (size_t r = 0; r < rows; r++) {
        std::string row;
        for (size_t i = 0; i < cols.size(); i++) {
            if (i) row += "  ";
            std::string cell = r < cols[i].cells.size() ? cols[i].cells[r] : "";
            row += pad_display(cell, width[i]);
        }
        log_raw(rtrim(row) + "\n");
    }
}

}  // namespace poly
