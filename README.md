# Validator

A C++20 validation framework for key-value arguments with rich rule definitions, transformations, cross-field checks, and detailed diagnostics.

## Start Here

- Beginner guide: [docs/validation-framework-guide.md](docs/validation-framework-guide.md)
- Core public API header: include/validator/validator.hpp
- Examples of expected behavior: tests/validator_test.cpp

## What You Get

- Strongly typed value model (`data_t` variant)
- Per-field rule validation (`validation_rule`)
- Cross-key constraints (`exactly_one_of`, `mutually_exclusive`)
- Alias resolution and canonical key normalization
- Value transformation and default injection
- Conditional requirements (`required_if`)
- Cross-field custom validation (`cross_validator`)
- Full validation report with phase and issue code metadata

## Build

```powershell
cmake -S . -B build
cmake --build build
```

## Run Tests

```powershell
ctest --test-dir build --output-on-failure
```

## Minimal Usage

```cpp
#include <vector>
#include "validator/validator.hpp"

int main() {
  aks::arguments_t args = {{"port", std::int32_t{8080}}};
  std::vector<aks::validation_rule> rules = {
      {.key = "port", .required = true, .supported_types = {aks::data_type::integer}}
  };

  const aks::validation_report report = aks::validator::validate_and_finalize(args, rules);
  return report.ok() ? 0 : 1;
}
```
