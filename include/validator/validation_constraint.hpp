#pragma once

#include <set>
#include <string>

namespace aks {

enum class constraint_type {
  exactly_one_of,
  mutually_exclusive,
};

struct validation_constraint {
  constraint_type type;
  std::set<std::string> keys;
};

} // namespace aks
