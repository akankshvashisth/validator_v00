#pragma once

#include <array>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "arguments.hpp"

namespace aks {

struct validation_result {
  std::string key;
  std::vector<std::string> errors = {};
};

enum class validation_phase {
  rule_schema,
  key_resolution,
  value_validation,
  transformation,
  cross_validation,
  constraint_validation,
  consumption,
};

enum class validation_issue_code {
  no_supported_types,
  type_mismatch,
  custom_validation_failed,
  rule_duplicate_key,
  rule_alias_equals_key,
  rule_duplicate_alias,
  rule_alias_conflicts_with_key,
  rule_numeric_bounds_non_numeric,
  required_missing,
  cross_validation_failed,
  unconsumed_key,
  required_if_failed,
  value_not_allowed,
  value_out_of_range,
  exactly_one_of_violated,
  mutually_exclusive_violated,
};

template <> struct enum_helper<validation_phase> {
  static constexpr std::array<validation_phase, 7> values = {
      validation_phase::rule_schema,
      validation_phase::key_resolution,
      validation_phase::value_validation,
      validation_phase::transformation,
      validation_phase::cross_validation,
      validation_phase::constraint_validation,
      validation_phase::consumption};

  static constexpr std::array<const char *, 7> names = {
      "rule_schema",    "key_resolution",   "value_validation",
      "transformation", "cross_validation", "constraint_validation",
      "consumption"};

  static constexpr const char *to_string(validation_phase phase) {
    for (size_t i = 0; i < values.size(); ++i) {
      if (values[i] == phase) {
        return names[i];
      }
    }
    return "unknown";
  }

  static constexpr validation_phase from_string(const std::string &name) {
    for (size_t i = 0; i < names.size(); ++i) {
      if (names[i] == name) {
        return values[i];
      }
    }
    throw std::invalid_argument("Invalid validation phase name: " + name);
  }
};

template <> struct enum_helper<validation_issue_code> {
  static constexpr std::array<validation_issue_code, 16> values = {
      validation_issue_code::no_supported_types,
      validation_issue_code::type_mismatch,
      validation_issue_code::custom_validation_failed,
      validation_issue_code::rule_duplicate_key,
      validation_issue_code::rule_alias_equals_key,
      validation_issue_code::rule_duplicate_alias,
      validation_issue_code::rule_alias_conflicts_with_key,
      validation_issue_code::rule_numeric_bounds_non_numeric,
      validation_issue_code::required_missing,
      validation_issue_code::cross_validation_failed,
      validation_issue_code::unconsumed_key,
      validation_issue_code::required_if_failed,
      validation_issue_code::value_not_allowed,
      validation_issue_code::value_out_of_range,
      validation_issue_code::exactly_one_of_violated,
      validation_issue_code::mutually_exclusive_violated};

  static constexpr std::array<const char *, 16> names = {
      "NO_SUPPORTED_TYPES",
      "TYPE_MISMATCH",
      "CUSTOM_VALIDATION_FAILED",
      "RULE_DUPLICATE_KEY",
      "RULE_ALIAS_EQUALS_KEY",
      "RULE_DUPLICATE_ALIAS",
      "RULE_ALIAS_CONFLICTS_WITH_KEY",
      "RULE_NUMERIC_BOUNDS_NON_NUMERIC",
      "REQUIRED_MISSING",
      "CROSS_VALIDATION_FAILED",
      "UNCONSUMED_KEY",
      "REQUIRED_IF_FAILED",
      "VALUE_NOT_ALLOWED",
      "VALUE_OUT_OF_RANGE",
      "EXACTLY_ONE_OF_VIOLATED",
      "MUTUALLY_EXCLUSIVE_VIOLATED"};

  static constexpr const char *to_string(validation_issue_code code) {
    for (size_t i = 0; i < values.size(); ++i) {
      if (values[i] == code) {
        return names[i];
      }
    }
    return "UNKNOWN_ISSUE_CODE";
  }

  static constexpr validation_issue_code from_string(const std::string &name) {
    for (size_t i = 0; i < names.size(); ++i) {
      if (names[i] == name) {
        return values[i];
      }
    }
    throw std::invalid_argument("Invalid validation issue code name: " + name);
  }
};

struct validation_issue {
  std::string key;
  validation_issue_code code = validation_issue_code::custom_validation_failed;
  std::string message;
  validation_phase phase = validation_phase::value_validation;
};

struct validation_report {
  arguments_t arguments;
  std::set<std::string> consumed_keys;
  std::set<std::string> unconsumed_keys;
  std::vector<validation_issue> issues;

  [[nodiscard]] bool ok() const { return issues.empty(); }
};

} // namespace aks