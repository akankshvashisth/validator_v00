#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "arguments.hpp"
#include "data.hpp"
#include "validation_result.hpp"

namespace aks {

struct validation_rule {
  std::string key;
  std::set<std::string> aliases = {};
  bool required = true;
  std::set<data_type> supported_types = {};
  std::function<validation_result(const data_t &)> custom_validator = nullptr;
  std::function<data_t(const data_t &)> transformer = nullptr;
  std::function<validation_result(const data_t &, const arguments_t &)>
      cross_validator = nullptr;
  std::function<data_t()> default_value_provider = nullptr;

  // required_if: if this predicate returns true the field is required even
  // when required == false.
  std::function<bool(const arguments_t &)> required_if = nullptr;

  // allowed_values: the resolved (post-transform) value must equal one of
  // these entries.  Leave empty to skip the check.
  std::vector<data_t> allowed_values = {};

  // min_value / max_value: inclusive numeric bounds applied to integer and
  // floating_point values.  Leave nullopt to skip the corresponding check.
  std::optional<double> min_value = std::nullopt;
  std::optional<double> max_value = std::nullopt;

  std::string supported_types_string() const {
    std::string result;
    for (const auto &type : supported_types) {
      if (!result.empty()) {
        result += ", ";
      }
      result += quote_text(enum_helper<data_type>::to_string(type));
    }
    return result.empty() ? "(none)" : result;
  }

  std::string aliases_string() const {
    std::string result;
    for (const auto &alias : aliases) {
      if (!result.empty()) {
        result += ", ";
      }
      result += quote_text(alias);
    }
    return result.empty() ? "(none)" : result;
  }

  std::string allowed_values_string() const {
    std::string result;
    for (const auto &av : allowed_values) {
      if (!result.empty()) {
        result += ", ";
      }
      result += quote_text(aks::to_string(av));
    }
    return result.empty() ? "(none)" : result;
  }

private:
  static std::string quote_text(const std::string &text) {
    return "'" + text + "'";
  }
};

} // namespace aks
