#pragma once

#include <string>

namespace poly {

// Returns the full source of the bundled testlib.h (embedded at build time when
// possible, otherwise read from disk next to the executable).
const std::string& testlib_source();

// Directory that contains testlib/testlib.h on disk, or an empty string.
std::string testlib_disk_path();

}  // namespace poly
