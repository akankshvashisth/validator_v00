#pragma once

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <variant>

#include "arguments.hpp"
#include "data.hpp"
#include "validation_constraint.hpp"
#include "validation_result.hpp"
#include "validation_rule.hpp"

namespace aks {

struct validation_options {
  bool fail_fast = false;
  bool strict_unconsumed_keys = true;
};

struct validator {

  validator() = default;

  explicit validator(const arguments_t &args, validation_options options = {})
      : options_(options) {
    report_.arguments = args;
  }

  void reset(const arguments_t &args, validation_options options = {}) {
    options_ = options;
    report_ = {};
    report_.arguments = args;
    finalized_ = false;
  }

  validator &
  validate(const std::vector<validation_rule> &rules,
           const std::vector<validation_constraint> &constraints = {}) {
    finalized_ = false;

    apply_rule_schema_validation(rules, report_, options_);
    if (should_stop(report_, options_)) {
      return *this;
    }

    apply_resolution_and_value_validation(rules, report_, options_);
    if (should_stop(report_, options_)) {
      return *this;
    }

    apply_transformations_and_defaults(rules, report_, options_);
    if (should_stop(report_, options_)) {
      return *this;
    }

    apply_value_constraint_validation(rules, report_, options_);
    if (should_stop(report_, options_)) {
      return *this;
    }

    apply_constraints(constraints, report_, options_);
    if (should_stop(report_, options_)) {
      return *this;
    }

    apply_cross_validations(rules, report_, options_);
    return *this;
  }

  const validation_report &report() const { return report_; }

  const validation_report &finalize() {
    if (!finalized_) {
      finalize_consumption(report_, options_);
      finalized_ = true;
    }
    return report_;
  }

private:
  validation_options options_{};
  validation_report report_{};
  bool finalized_ = false;

  static void add_issue(validation_report &report, const std::string &key,
                        validation_issue_code code, const std::string &message,
                        validation_phase phase) {
    report.issues.push_back(
        {.key = key, .code = code, .message = message, .phase = phase});
  }

  static std::string data_to_string(const data_t &value) {
    return to_string(value);
  }

  static std::string format_numeric(double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    std::string result = stream.str();

    const auto decimal_point = result.find('.');
    if (decimal_point != std::string::npos) {
      while (!result.empty() && result.back() == '0') {
        result.pop_back();
      }
      if (!result.empty() && result.back() == '.') {
        result.pop_back();
      }
    }

    if (result == "-0") {
      return "0";
    }
    return result;
  }

  static std::string join_keys(const std::set<std::string> &keys) {
    std::string result;
    for (const auto &key : keys) {
      if (!result.empty())
        result += ", ";
      result += quote_text(key);
    }
    return result;
  }

  static std::optional<std::string> resolve_key(arguments_t &args,
                                                const validation_rule &rule) {
    const auto it = args.find(rule.key);
    if (it != args.end()) {
      return rule.key;
    }

    for (const auto &alias : rule.aliases) {
      const auto alias_it = args.find(alias);
      if (alias_it != args.end()) {
        args[rule.key] = alias_it->second;
        args.erase(alias_it);
        return rule.key;
      }
    }

    return std::nullopt;
  }

  static void validate_value(const validation_rule &rule, const data_t &value,
                             validation_report &report,
                             validation_phase phase) {
    const auto actual_type = get_data_type(value);

    if (rule.supported_types.empty()) {
      add_issue(report, rule.key, validation_issue_code::no_supported_types,
                "No supported types defined for argument: " +
                    quote_text(rule.key),
                phase);
      return;
    }

    if (!rule.supported_types.contains(actual_type)) {
      add_issue(report, rule.key, validation_issue_code::type_mismatch,
                "Unsupported type for argument: " + quote_text(rule.key) +
                    ". Supported types: " + rule.supported_types_string() +
                    ". Actual type: " +
                    quote_text(enum_helper<data_type>::to_string(actual_type)),
                phase);
      return;
    }

    if (rule.custom_validator) {
      validation_result custom_result = rule.custom_validator(value);
      for (const auto &error : custom_result.errors) {
        add_issue(report, rule.key,
                  validation_issue_code::custom_validation_failed, error,
                  phase);
      }
    }
  }

  static bool should_stop(const validation_report &report,
                          const validation_options &options) {
    return options.fail_fast && !report.ok();
  }

  static void
  apply_rule_schema_validation(const std::vector<validation_rule> &rules,
                               validation_report &report,
                               const validation_options &options) {
    std::map<std::string, std::size_t> key_counts;
    std::map<std::string, std::size_t> alias_counts;

    for (const auto &rule : rules) {
      ++key_counts[rule.key];
      for (const auto &alias : rule.aliases) {
        ++alias_counts[alias];
      }
    }

    for (const auto &rule : rules) {
      if (key_counts[rule.key] > 1) {
        add_issue(report, rule.key, validation_issue_code::rule_duplicate_key,
                  "Duplicate rule key found: " + quote_text(rule.key),
                  validation_phase::rule_schema);
        if (should_stop(report, options)) {
          return;
        }
      }

      for (const auto &alias : rule.aliases) {
        if (alias == rule.key) {
          add_issue(report, rule.key,
                    validation_issue_code::rule_alias_equals_key,
                    "Alias cannot be the same as key: " + quote_text(alias),
                    validation_phase::rule_schema);
          if (should_stop(report, options)) {
            return;
          }
        }

        if (alias_counts[alias] > 1) {
          add_issue(report, rule.key,
                    validation_issue_code::rule_duplicate_alias,
                    "Duplicate alias found across rules: " + quote_text(alias),
                    validation_phase::rule_schema);
          if (should_stop(report, options)) {
            return;
          }
        }

        if (key_counts.contains(alias)) {
          add_issue(report, rule.key,
                    validation_issue_code::rule_alias_conflicts_with_key,
                    "Alias conflicts with an existing rule key: " +
                        quote_text(alias),
                    validation_phase::rule_schema);
          if (should_stop(report, options)) {
            return;
          }
        }
      }

      if (rule.min_value.has_value() || rule.max_value.has_value()) {
        const bool has_numeric_supported_type =
            rule.supported_types.contains(data_type::integer) ||
            rule.supported_types.contains(data_type::floating_point) ||
            rule.supported_types.contains(data_type::integer_array) ||
            rule.supported_types.contains(data_type::floating_point_array);
        if (!has_numeric_supported_type) {
          add_issue(report, rule.key,
                    validation_issue_code::rule_numeric_bounds_non_numeric,
                    "Numeric bounds (min_value/max_value) require one of "
                    "integer, floating_point, integer_array, or "
                    "floating_point_array in supported types for argument: " +
                        quote_text(rule.key),
                    validation_phase::rule_schema);
          if (should_stop(report, options)) {
            return;
          }
        }
      }
    }
  }

  static void apply_resolution_and_value_validation(
      const std::vector<validation_rule> &rules, validation_report &report,
      const validation_options &options) {
    for (const auto &rule : rules) {
      const auto resolved_key = resolve_key(report.arguments, rule);

      if (!resolved_key.has_value()) {
        if (rule.required) {
          add_issue(report, rule.key, validation_issue_code::required_missing,
                    "Missing required argument: " + quote_text(rule.key) +
                        ". Aliases: " + rule.aliases_string() +
                        ". Supported types: " + rule.supported_types_string(),
                    validation_phase::key_resolution);
        } else if (rule.required_if && rule.required_if(report.arguments)) {
          add_issue(report, rule.key, validation_issue_code::required_if_failed,
                    "Conditionally required argument missing: " +
                        quote_text(rule.key) +
                        ". Aliases: " + rule.aliases_string() +
                        ". Supported types: " + rule.supported_types_string(),
                    validation_phase::key_resolution);
        }
        if (should_stop(report, options)) {
          return;
        }
        continue;
      }

      report.consumed_keys.insert(rule.key);
      validate_value(rule, report.arguments.at(rule.key), report,
                     validation_phase::value_validation);

      if (should_stop(report, options)) {
        return;
      }
    }
  }

  static void
  apply_transformations_and_defaults(const std::vector<validation_rule> &rules,
                                     validation_report &report,
                                     const validation_options &options) {
    for (const auto &rule : rules) {
      bool updated = false;

      if (report.arguments.contains(rule.key)) {
        if (rule.transformer) {
          report.arguments[rule.key] =
              rule.transformer(report.arguments.at(rule.key));
          updated = true;
        }
      } else if (rule.default_value_provider) {
        report.arguments[rule.key] = rule.default_value_provider();
        report.consumed_keys.insert(rule.key);
        updated = true;
      }

      if (updated) {
        validate_value(rule, report.arguments.at(rule.key), report,
                       validation_phase::transformation);
        if (should_stop(report, options)) {
          return;
        }
      }
    }
  }

  static void validate_value_constraints(const validation_rule &rule,
                                         const data_t &value,
                                         validation_report &report) {
    if (!rule.allowed_values.empty()) {
      const bool in_allowed =
          std::any_of(rule.allowed_values.begin(), rule.allowed_values.end(),
                      [&](const data_t &allowed) { return allowed == value; });
      if (!in_allowed) {
        const std::string allowed_str = rule.allowed_values_string();
        add_issue(report, rule.key, validation_issue_code::value_not_allowed,
                  "Value not allowed for argument: " + quote_text(rule.key) +
                      ". Allowed values: " + allowed_str,
                  validation_phase::value_validation);
      }
    }

    const auto actual_type = get_data_type(value);
    if (rule.min_value.has_value() || rule.max_value.has_value()) {
      if (actual_type == data_type::integer ||
          actual_type == data_type::floating_point) {
        const double numeric =
            (actual_type == data_type::integer)
                ? static_cast<double>(std::get<std::int32_t>(value))
                : std::get<double>(value);
        if (rule.min_value.has_value() && numeric < *rule.min_value) {
          add_issue(report, rule.key, validation_issue_code::value_out_of_range,
                    "Value out of range for argument: " + quote_text(rule.key) +
                        ". Min: " + format_numeric(*rule.min_value) +
                        ". Actual: " + format_numeric(numeric),
                    validation_phase::value_validation);
        }
        if (rule.max_value.has_value() && numeric > *rule.max_value) {
          add_issue(report, rule.key, validation_issue_code::value_out_of_range,
                    "Value out of range for argument: " + quote_text(rule.key) +
                        ". Max: " + format_numeric(*rule.max_value) +
                        ". Actual: " + format_numeric(numeric),
                    validation_phase::value_validation);
        }
      } else if (actual_type == data_type::integer_array ||
                 actual_type == data_type::floating_point_array) {
        auto validate_numeric_sequence = [&](const auto &values) {
          for (std::size_t i = 0; i < values.size(); ++i) {
            const double numeric = static_cast<double>(values[i]);
            if (rule.min_value.has_value() && numeric < *rule.min_value) {
              add_issue(
                  report, rule.key, validation_issue_code::value_out_of_range,
                  "Value out of range for argument: " +
                      quote_text(rule.key + "[" + std::to_string(i) + "]") +
                      ". Min: " + format_numeric(*rule.min_value) +
                      ". Actual: " + format_numeric(numeric),
                  validation_phase::value_validation);
            }
            if (rule.max_value.has_value() && numeric > *rule.max_value) {
              add_issue(
                  report, rule.key, validation_issue_code::value_out_of_range,
                  "Value out of range for argument: " +
                      quote_text(rule.key + "[" + std::to_string(i) + "]") +
                      ". Max: " + format_numeric(*rule.max_value) +
                      ". Actual: " + format_numeric(numeric),
                  validation_phase::value_validation);
            }
          }
        };

        if (actual_type == data_type::integer_array) {
          validate_numeric_sequence(std::get<std::deque<std::int32_t>>(value));
        } else {
          validate_numeric_sequence(std::get<std::deque<double>>(value));
        }
      }
    }
  }

  static void
  apply_value_constraint_validation(const std::vector<validation_rule> &rules,
                                    validation_report &report,
                                    const validation_options &options) {
    for (const auto &rule : rules) {
      if (!report.arguments.contains(rule.key)) {
        continue;
      }
      validate_value_constraints(rule, report.arguments.at(rule.key), report);
      if (should_stop(report, options)) {
        return;
      }
    }
  }

  static void apply_cross_validations(const std::vector<validation_rule> &rules,
                                      validation_report &report,
                                      const validation_options &options) {
    for (const auto &rule : rules) {
      if (!rule.cross_validator || !report.arguments.contains(rule.key)) {
        continue;
      }

      const validation_result cross_result =
          rule.cross_validator(report.arguments.at(rule.key), report.arguments);
      for (const auto &error : cross_result.errors) {
        const std::string issue_key =
            cross_result.key.empty() ? rule.key : cross_result.key;
        add_issue(report, issue_key,
                  validation_issue_code::cross_validation_failed, error,
                  validation_phase::cross_validation);
      }

      if (should_stop(report, options)) {
        return;
      }
    }
  }

  static void
  apply_constraints(const std::vector<validation_constraint> &constraints,
                    validation_report &report,
                    const validation_options &options) {
    for (const auto &constraint : constraints) {
      std::vector<std::string> present_keys;
      for (const auto &key : constraint.keys) {
        if (report.arguments.contains(key)) {
          present_keys.push_back(key);
        }
      }

      if (constraint.type == constraint_type::exactly_one_of) {
        if (present_keys.size() != 1) {
          add_issue(report, "", validation_issue_code::exactly_one_of_violated,
                    "Exactly one of [" + join_keys(constraint.keys) +
                        "] must be present, found " +
                        std::to_string(present_keys.size()),
                    validation_phase::constraint_validation);
        }
      } else if (constraint.type == constraint_type::mutually_exclusive) {
        if (present_keys.size() > 1) {
          std::string present_str;
          for (const auto &k : present_keys) {
            if (!present_str.empty())
              present_str += ", ";
            present_str += quote_text(k);
          }
          add_issue(report, "",
                    validation_issue_code::mutually_exclusive_violated,
                    "Mutually exclusive arguments present: [" + present_str +
                        "]. At most one of [" + join_keys(constraint.keys) +
                        "] may be used",
                    validation_phase::constraint_validation);
        }
      }

      if (should_stop(report, options)) {
        return;
      }
    }
  }

  static void finalize_consumption(validation_report &report,
                                   const validation_options &options) {
    report.unconsumed_keys.clear();
    report.issues.erase(
        std::remove_if(report.issues.begin(), report.issues.end(),
                       [](const validation_issue &issue) {
                         return issue.code ==
                                validation_issue_code::unconsumed_key;
                       }),
        report.issues.end());

    for (const auto &[key, value] : report.arguments) {
      (void)value;
      if (!report.consumed_keys.contains(key)) {
        report.unconsumed_keys.insert(key);
      }
    }

    if (options.strict_unconsumed_keys) {
      for (const auto &key : report.unconsumed_keys) {
        add_issue(report, key, validation_issue_code::unconsumed_key,
                  "Unconsumed argument: " + quote_text(key),
                  validation_phase::consumption);
      }
    }
  }

  static std::string quote_text(const std::string &text) {
    return "'" + text + "'";
  }

public:
  static validation_report validate_and_finalize(
      const arguments_t &args, const std::vector<validation_rule> &rules,
      const std::vector<validation_constraint> &constraints = {},
      validation_options options = {}) {
    validator stateful_validator(args, options);
    stateful_validator.validate(rules, constraints);
    return stateful_validator.finalize();
  }
};
} // namespace aks
