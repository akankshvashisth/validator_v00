# Validation Framework Guide

This guide explains how to use the validation framework in a practical, beginner-friendly way.

It covers:

- What the framework validates
- Core data structures
- Validation workflow and phases
- Rule features (aliases, required-if, defaults, transformations, ranges, allowed values)
- Cross-field constraints
- Stateful and one-shot APIs
- Realistic code samples
- Copy-paste starter template

---

## 1. What This Framework Does

The framework validates key-value input arguments against a set of rules and optional cross-key constraints.

Input is represented as:

- `aks::arguments_t` (a `std::map<std::string, data_t>`)
- `aks::data_t` (a `std::variant` of supported scalar/array/date types)

Validation produces a `validation_report` with:

- Normalized/transformed arguments (`report.arguments`)
- Which keys were consumed (`report.consumed_keys`)
- Which keys were left over (`report.unconsumed_keys`)
- Validation issues (`report.issues`)

---

## 2. Supported Value Types

`data_t` supports these types:

- `std::monostate` (`null`)
- `bool`
- `std::int32_t`
- `double`
- `std::string`
- `date_t` (`std::chrono::year_month_day`)
- Arrays as `std::deque<T>` of:
  - `bool`
  - `std::int32_t`
  - `double`
  - `std::string`
  - `date_t`

The corresponding `data_type` values are:

- `null`, `boolean`, `integer`, `floating_point`, `string`, `date`
- `boolean_array`, `integer_array`, `floating_point_array`, `string_array`, `date_array`

---

## 3. Include and Build Basics

Include the public header:

```cpp
#include "validator/validator.hpp"
```

Namespace:

```cpp
using namespace aks;
```

Project uses C++20 (`CMAKE_CXX_STANDARD 20`).

Run tests:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 4. Quick Start (One-Shot Validation)

Use `validator::validate_and_finalize(...)` when you want a single call that validates and finalizes consumption.

```cpp
#include <cstdint>
#include <iostream>
#include <vector>
#include "validator/validator.hpp"

int main() {
  aks::arguments_t args = {
      {"p", std::int32_t{8080}},
      {"host", std::string{"localhost"}}
  };

  std::vector<aks::validation_rule> rules = {
      {
          .key = "port",
          .aliases = {"p"},
          .required = true,
          .supported_types = {aks::data_type::integer}
      },
      {
          .key = "host",
          .required = true,
          .supported_types = {aks::data_type::string}
      }
  };

  aks::validation_report report = aks::validator::validate_and_finalize(args, rules);

  if (!report.ok()) {
    for (const auto &issue : report.issues) {
      std::cout << "[" << aks::enum_helper<aks::validation_issue_code>::to_string(issue.code)
                << "] key=" << issue.key
                << " phase=" << aks::enum_helper<aks::validation_phase>::to_string(issue.phase)
                << " message=" << issue.message << "\n";
    }
    return 1;
  }

  // Alias "p" is normalized to canonical key "port".
  std::cout << "port=" << std::get<std::int32_t>(report.arguments.at("port")) << "\n";
  std::cout << "host=" << std::get<std::string>(report.arguments.at("host")) << "\n";
}
```

---

## 5. Validation Options

`validation_options` controls runtime behavior:

```cpp
aks::validation_options options;
options.fail_fast = false;             // default false
options.strict_unconsumed_keys = true; // default true
```

### `fail_fast`

- `false`: collect as many issues as possible
- `true`: stop on first encountered issue in each pipeline stage

### `strict_unconsumed_keys`

- `true`: any unconsumed argument becomes an `UNCONSUMED_KEY` issue
- `false`: unconsumed keys are still tracked in `report.unconsumed_keys`, but do not fail validation

---

## 6. Rule Anatomy

A `validation_rule` supports:

- `key`: canonical argument name
- `aliases`: alternative input names that are normalized to `key`
- `required`: required or optional
- `supported_types`: allowed runtime types
- `custom_validator(value) -> validation_result`
- `transformer(value) -> data_t`
- `cross_validator(value, all_args) -> validation_result`
- `default_value_provider() -> data_t`
- `required_if(all_args) -> bool`
- `allowed_values`: exact post-transform value matching
- `min_value`, `max_value`: inclusive numeric bounds (scalar or numeric arrays)

Example:

```cpp
aks::validation_rule currency_rule{
    .key = "currency",
    .required = true,
    .supported_types = {aks::data_type::string},
    .transformer = [](const aks::data_t &value) {
      std::string ccy = std::get<std::string>(value);
      std::transform(ccy.begin(), ccy.end(), ccy.begin(),
                     [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
      return aks::data_t{ccy};
    },
    .allowed_values = {
        aks::data_t{std::string{"USD"}},
        aks::data_t{std::string{"EUR"}},
        aks::data_t{std::string{"GBP"}}
    }
};
```

---

## 7. Validation Phases (Execution Order)

Validation runs in this order:

1. `rule_schema`
2. `key_resolution`
3. `value_validation`
4. `transformation`
5. `constraint_validation`
6. `cross_validation`
7. `consumption` (during `finalize()`)

What this means in practice:

- Rule-level problems (duplicate keys, alias conflicts, invalid numeric-bounds config) are found first.
- Aliases are resolved before value checks.
- Transformers/defaults run before allowed-values/range checks complete.
- Unconsumed-key issues are only finalized in the consumption stage.

---

## 8. Common Feature Examples

### 8.1 Aliases + Transformation + Defaults

```cpp
aks::arguments_t args = {
    {"log-level", std::string{"debug"}}
};

std::vector<aks::validation_rule> rules = {
    {
        .key = "log_level",
        .aliases = {"log-level"},
        .required = true,
        .supported_types = {aks::data_type::string},
        .transformer = [](const aks::data_t &v) {
          std::string s = std::get<std::string>(v);
          std::transform(s.begin(), s.end(), s.begin(),
                         [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
          return aks::data_t{s};
        }
    },
    {
        .key = "retry_count",
        .required = false,
        .supported_types = {aks::data_type::integer},
        .default_value_provider = [] { return aks::data_t{std::int32_t{3}}; }
    }
};

auto report = aks::validator::validate_and_finalize(args, rules);
```

Results:

- Input key `log-level` becomes canonical `log_level`
- `log_level` transformed to uppercase
- Missing `retry_count` is injected with default value `3`

### 8.2 Conditional Requirement (`required_if`)

```cpp
std::vector<aks::validation_rule> rules = {
    {
        .key = "payment_method",
        .required = true,
        .supported_types = {aks::data_type::string}
    },
    {
        .key = "confirmation_code",
        .required = false,
        .supported_types = {aks::data_type::string},
        .required_if = [](const aks::arguments_t &all_args) {
          const auto it = all_args.find("payment_method");
          return it != all_args.end() &&
                 std::get<std::string>(it->second) == "card";
        }
    }
};
```

If `payment_method == "card"`, then `confirmation_code` must be present.

### 8.3 Allowed Values + Ranges

```cpp
std::vector<aks::validation_rule> rules = {
    {
        .key = "direction",
        .required = true,
        .supported_types = {aks::data_type::string},
        .allowed_values = {
            aks::data_t{std::string{"buy"}},
            aks::data_t{std::string{"sell"}}
        }
    },
    {
        .key = "quantity",
        .required = true,
        .supported_types = {aks::data_type::integer},
        .min_value = 1.0,
        .max_value = 1000000.0
    },
    {
        .key = "prices",
        .required = true,
        .supported_types = {aks::data_type::floating_point_array},
        .min_value = 0.0
    }
};
```

For numeric arrays, min/max are applied element-by-element (issues include index, e.g. `prices[1]`).

### 8.4 Custom Validator

```cpp
aks::validation_rule par_value_rule{
    .key = "par_value",
    .required = true,
    .supported_types = {aks::data_type::floating_point},
    .custom_validator = [](const aks::data_t &value) {
      aks::validation_result result{.key = "par_value"};
      if (std::get<double>(value) <= 0.0) {
        result.errors.push_back("par_value must be positive");
      }
      return result;
    }
};
```

### 8.5 Cross Validator (Depends on Other Fields)

```cpp
aks::validation_rule end_date_rule{
    .key = "end_date",
    .required = true,
    .supported_types = {aks::data_type::date},
    .cross_validator = [](const aks::data_t &value, const aks::arguments_t &all_args) {
      aks::validation_result result{.key = "end_date"};

      const auto it = all_args.find("start_date");
      if (it != all_args.end()) {
        const auto start = std::get<aks::date_t>(it->second);
        const auto end = std::get<aks::date_t>(value);
        if (end < start) {
          result.errors.push_back("end_date must be on or after start_date");
        }
      }
      return result;
    }
};
```

---

## 9. Cross-Key Constraints

Use `validation_constraint` for set-level requirements.

### Exactly One Of

```cpp
std::vector<aks::validation_constraint> constraints = {
    {
        .type = aks::constraint_type::exactly_one_of,
        .keys = {"api_key", "oauth_token", "password"}
    }
};
```

Validation fails unless exactly one key in the set is present.

### Mutually Exclusive

```cpp
std::vector<aks::validation_constraint> constraints = {
    {
        .type = aks::constraint_type::mutually_exclusive,
        .keys = {"format", "format_xml", "format_csv"}
    }
};
```

Validation fails if more than one key in the set is present.

---

## 10. Stateful API (Multi-Step Pipelines)

Use stateful `validator` when you need staged validation.

```cpp
aks::arguments_t args = {
    {"k", std::string{"bond"}},
    {"portfolio_id", std::int32_t{7}},
    {"coupon_rate", double{0.035}},
    {"maturity_date", std::chrono::year{2032}/6/30},
    {"debug_flag", true}
};

aks::validation_options options;
options.strict_unconsumed_keys = true;

aks::validator pipeline(args, options);

pipeline.validate(common_rules);   // stage 1
pipeline.validate(bond_rules);     // stage 2

const auto &final_report = pipeline.finalize();
```

Why use this API:

- Build rule sets incrementally
- Validate discriminator/common fields first, then specialized rules
- Reuse `pipeline.report()` between stages

Also available:

- `reset(args, options)` to reuse the same validator object for another payload

---

## 11. Reading the Report

`validation_report` fields:

- `ok()`: `true` when `issues` is empty
- `arguments`: canonicalized, transformed, and default-injected values
- `consumed_keys`: keys matched by rules
- `unconsumed_keys`: keys that remain unmatched after finalize
- `issues`: list of `validation_issue`

Each `validation_issue` includes:

- `key` (possibly empty for set constraints)
- `code` (`validation_issue_code`)
- `phase` (`validation_phase`)
- `message` (human-readable)

Simple printer:

```cpp
void print_report(const aks::validation_report &report) {
  for (const auto &issue : report.issues) {
    std::cout
        << "[" << aks::enum_helper<aks::validation_issue_code>::to_string(issue.code) << "] "
        << "phase=" << aks::enum_helper<aks::validation_phase>::to_string(issue.phase)
        << " key=" << (issue.key.empty() ? "<global>" : issue.key)
        << " msg=" << issue.message
        << "\n";
  }
}
```

---

## 12. Best Practices for New Users

1. Start with canonical keys and use aliases only for compatibility.
2. Keep `supported_types` explicit and minimal.
3. Prefer transformers for normalization (uppercase, trimming, canonical format).
4. Use `allowed_values` after transformation so checks happen on normalized data.
5. Keep `strict_unconsumed_keys = true` in production APIs to detect unknown input.
6. Use `strict_unconsumed_keys = false` for discovery/parsing phases.
7. Use `required_if` and `cross_validator` for business logic spanning multiple fields.
8. Use constraints for key-set logic (`exactly_one_of`, `mutually_exclusive`).
9. Use `fail_fast = false` while developing to get full diagnostics.
10. Add tests for every rule set variant (valid, missing fields, wrong types, edge range values).

---

## 13. Common Pitfalls

- Forgetting to call `finalize()` when using stateful API: unconsumed-key checks are not completed.
- Defining numeric bounds but omitting numeric `supported_types`: triggers rule-schema issue.
- Expecting aliases to remain in `report.arguments`: aliases are normalized into canonical keys.
- Using `allowed_values` with unnormalized input but no transformer.
- Assuming optional fields are validated when absent: most validations run only when value is present (unless `required_if`/required rules apply).

---

## 14. Minimal End-to-End Example

```cpp
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <vector>

#include "validator/validator.hpp"

int main() {
  using namespace aks;

  arguments_t args = {
      {"symbol", std::string{"AAPL"}},
      {"side", std::string{"buy"}},
      {"qty", std::int32_t{100}},
      {"currency", std::string{"usd"}},
      {"extra", bool{true}}
  };

  std::vector<validation_rule> rules = {
      {
          .key = "symbol",
          .required = true,
          .supported_types = {data_type::string}
      },
      {
          .key = "side",
          .required = true,
          .supported_types = {data_type::string},
          .allowed_values = {data_t{std::string{"buy"}}, data_t{std::string{"sell"}}}
      },
      {
          .key = "quantity",
          .aliases = {"qty"},
          .required = true,
          .supported_types = {data_type::integer},
          .min_value = 1.0
      },
      {
          .key = "currency",
          .required = true,
          .supported_types = {data_type::string},
          .transformer = [](const data_t &value) {
            std::string c = std::get<std::string>(value);
            std::transform(c.begin(), c.end(), c.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
            return data_t{c};
          },
          .allowed_values = {
              data_t{std::string{"USD"}},
              data_t{std::string{"EUR"}},
              data_t{std::string{"GBP"}}
          }
      }
  };

  validation_options options;
  options.strict_unconsumed_keys = true;

  const validation_report report = validator::validate_and_finalize(args, rules, {}, options);

  if (!report.ok()) {
    std::cout << "Validation failed with " << report.issues.size() << " issue(s):\n";
    for (const auto &issue : report.issues) {
      std::cout << " - ["
                << enum_helper<validation_issue_code>::to_string(issue.code)
                << "] " << issue.message << "\n";
    }
    return 1;
  }

  std::cout << "Validation OK\n";
  std::cout << "symbol=" << std::get<std::string>(report.arguments.at("symbol")) << "\n";
  std::cout << "side=" << std::get<std::string>(report.arguments.at("side")) << "\n";
  std::cout << "quantity=" << std::get<std::int32_t>(report.arguments.at("quantity")) << "\n";
  std::cout << "currency=" << std::get<std::string>(report.arguments.at("currency")) << "\n";

  return 0;
}
```

---

## 15. API Cheatsheet

- One-shot:
  - `validator::validate_and_finalize(args, rules, constraints, options)`
- Stateful:
  - `validator(args, options)`
  - `validate(rules, constraints)`
  - `report()`
  - `finalize()`
  - `reset(args, options)`

Primary structs/enums:

- `validation_rule`
- `validation_constraint`
- `validation_options`
- `validation_report`
- `validation_issue`
- `validation_phase`
- `validation_issue_code`
- `data_t` / `data_type`

This should be enough to build robust, testable validation rule sets for CLI, config, API payload, or domain-model input validation workflows.

---

## 16. Copy-Paste Starter Template

Use this as a drop-in starting point for a new validator setup.

```cpp
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <vector>

#include "validator/validator.hpp"

int main() {
  using namespace aks;

  arguments_t args = {
      {"id", std::string{"trade-001"}},
      {"qty", std::int32_t{250}},
      {"currency", std::string{"usd"}},
      {"auth_token", std::string{"abc123"}}
  };

  std::vector<validation_rule> rules = {
      {
          .key = "id",
          .required = true,
          .supported_types = {data_type::string}
      },
      {
          .key = "quantity",
          .aliases = {"qty"},
          .required = true,
          .supported_types = {data_type::integer},
          .min_value = 1.0
      },
      {
          .key = "currency",
          .required = true,
          .supported_types = {data_type::string},
          .transformer = [](const data_t &v) {
            std::string s = std::get<std::string>(v);
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return data_t{s};
          },
          .allowed_values = {
              data_t{std::string{"USD"}},
              data_t{std::string{"EUR"}},
              data_t{std::string{"GBP"}}
          }
      },
      {
          .key = "auth_token",
          .required = false,
          .supported_types = {data_type::string}
      },
      {
          .key = "api_key",
          .required = false,
          .supported_types = {data_type::string}
      }
  };

  std::vector<validation_constraint> constraints = {
      {
          .type = constraint_type::exactly_one_of,
          .keys = {"auth_token", "api_key"}
      }
  };

  validation_options options;
  options.fail_fast = false;
  options.strict_unconsumed_keys = true;

  const validation_report report =
      validator::validate_and_finalize(args, rules, constraints, options);

  if (!report.ok()) {
    std::cout << "Validation failed with " << report.issues.size() << " issue(s):\n";
    for (const auto &issue : report.issues) {
      std::cout << " - ["
                << enum_helper<validation_issue_code>::to_string(issue.code)
                << "] phase="
                << enum_helper<validation_phase>::to_string(issue.phase)
                << " key=" << (issue.key.empty() ? "<global>" : issue.key)
                << " msg=" << issue.message << "\n";
    }
    return 1;
  }

  std::cout << "Validation OK\n";
  std::cout << "id=" << std::get<std::string>(report.arguments.at("id")) << "\n";
  std::cout << "quantity=" << std::get<std::int32_t>(report.arguments.at("quantity")) << "\n";
  std::cout << "currency=" << std::get<std::string>(report.arguments.at("currency")) << "\n";
  return 0;
}
```

Checklist when adapting this template:

1. Replace keys and types with your domain fields.
2. Add aliases only where backward compatibility is needed.
3. Add `required_if` and `cross_validator` for business rules across fields.
4. Keep strict unconsumed keys on for production payloads.
5. Add tests for both valid payloads and each failure branch.
