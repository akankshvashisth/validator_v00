#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <vector>

#define BOOST_TEST_MODULE validator_tests
#include <boost/test/included/unit_test.hpp>

#include "validator/validator.hpp"

using aks::arguments_t;
using aks::constraint_type;
using aks::data_t;
using aks::data_type;
using aks::date_t;
using aks::validation_constraint;
using aks::validation_issue;
using aks::validation_issue_code;
using aks::validation_options;
using aks::validation_phase;
using aks::validation_report;
using aks::validation_result;
using aks::validation_rule;
using aks::validator;

namespace {

std::chrono::year_month_day make_date(int year, unsigned month, unsigned day) {
  return std::chrono::year{year} / std::chrono::month{month} /
         std::chrono::day{day};
}

bool has_issue(const validation_report &report, const std::string &key,
               validation_issue_code code, const std::string &message,
               validation_phase phase) {
  return std::any_of(report.issues.begin(), report.issues.end(),
                     [&](const validation_issue &issue) {
                       return issue.key == key && issue.code == code &&
                              issue.message == message && issue.phase == phase;
                     });
}

std::size_t issue_count_for_code(const validation_report &report,
                                 validation_issue_code code) {
  return static_cast<std::size_t>(std::count_if(
      report.issues.begin(), report.issues.end(),
      [&](const validation_issue &issue) { return issue.code == code; }));
}

} // namespace

BOOST_AUTO_TEST_CASE(validate_reports_missing_required_with_metadata) {
  const arguments_t args;

  const std::vector<validation_rule> rules = {
      {.key = "port",
       .aliases = {"p"},
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(
      report, "port", validation_issue_code::required_missing,
      "Missing required argument: 'port'. Aliases: 'p'. Supported types: "
      "'integer'",
      validation_phase::key_resolution));
}

BOOST_AUTO_TEST_CASE(validate_reports_duplicate_rule_keys) {
  const arguments_t args = {{"port", std::int32_t{8080}}};

  const std::vector<validation_rule> rules = {
      {.key = "port",
       .required = true,
       .supported_types = {data_type::integer}},
      {.key = "port",
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(has_issue(
      report, "port", validation_issue_code::rule_duplicate_key,
      "Duplicate rule key found: 'port'", validation_phase::rule_schema));
}

BOOST_AUTO_TEST_CASE(validate_reports_alias_conflicting_with_other_rule_key) {
  const arguments_t args = {{"host", std::string{"localhost"}},
                            {"port", std::int32_t{8080}}};

  const std::vector<validation_rule> rules = {
      {.key = "host",
       .aliases = {"port"},
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "port",
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(has_issue(report, "host",
                       validation_issue_code::rule_alias_conflicts_with_key,
                       "Alias conflicts with an existing rule key: 'port'",
                       validation_phase::rule_schema));
}

BOOST_AUTO_TEST_CASE(validate_reports_alias_equal_to_its_own_key) {
  const arguments_t args = {{"mode", std::string{"batch"}}};

  const std::vector<validation_rule> rules = {
      {.key = "mode",
       .aliases = {"mode"},
       .required = true,
       .supported_types = {data_type::string}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(has_issue(report, "mode",
                       validation_issue_code::rule_alias_equals_key,
                       "Alias cannot be the same as key: 'mode'",
                       validation_phase::rule_schema));
}

BOOST_AUTO_TEST_CASE(
    validate_reports_numeric_bounds_on_non_numeric_supported_types) {
  const arguments_t args = {{"direction", std::string{"buy"}}};

  const std::vector<validation_rule> rules = {
      {.key = "direction",
       .required = true,
       .supported_types = {data_type::string},
       .min_value = 1.0}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(has_issue(report, "direction",
                       validation_issue_code::rule_numeric_bounds_non_numeric,
                       "Numeric bounds (min_value/max_value) require one of "
                       "integer, floating_point, integer_array, or "
                       "floating_point_array in supported types for argument: "
                       "'direction'",
                       validation_phase::rule_schema));
}

BOOST_AUTO_TEST_CASE(validate_normalizes_alias_and_tracks_consumption) {
  const arguments_t args = {{"p", std::int32_t{8080}}};

  const std::vector<validation_rule> rules = {
      {.key = "port",
       .aliases = {"p"},
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(report.arguments.contains("port"));
  BOOST_TEST(!report.arguments.contains("p"));
  BOOST_TEST(std::get<std::int32_t>(report.arguments.at("port")) == 8080);
  BOOST_TEST(report.consumed_keys.contains("port"));
  BOOST_TEST(report.unconsumed_keys.empty());
}

BOOST_AUTO_TEST_CASE(
    validate_defaults_to_strict_unconsumed_keys_without_explicit_options) {
  const arguments_t args = {{"known", std::int32_t{1}}, {"extra", bool{true}}};

  const std::vector<validation_rule> rules = {
      {.key = "known",
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(report.unconsumed_keys.contains("extra"));
  BOOST_TEST(has_issue(report, "extra", validation_issue_code::unconsumed_key,
                       "Unconsumed argument: 'extra'",
                       validation_phase::consumption));
}

BOOST_AUTO_TEST_CASE(validate_reports_type_mismatch_with_full_message) {
  const arguments_t args = {{"port", std::string{"8080"}}};

  const std::vector<validation_rule> rules = {
      {.key = "port",
       .required = true,
       .supported_types = {data_type::integer}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(has_issue(report, "port", validation_issue_code::type_mismatch,
                       "Unsupported type for argument: 'port'. Supported "
                       "types: 'integer'. Actual type: 'string'",
                       validation_phase::value_validation));
}

BOOST_AUTO_TEST_CASE(
    validate_applies_transform_and_default_in_transformation_phase) {
  const arguments_t args = {{"log-level", std::string{"debug"}}};

  const std::vector<validation_rule> rules = {
      {.key = "log_level",
       .aliases = {"log-level"},
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string level = std::get<std::string>(value);
             std::transform(level.begin(), level.end(), level.begin(),
                            [](unsigned char c) {
                              return static_cast<char>(std::toupper(c));
                            });
             return data_t{level};
           }},
      {.key = "retry_count",
       .required = false,
       .supported_types = {data_type::integer},
       .default_value_provider = [] { return data_t{std::int32_t{3}}; }}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(std::get<std::string>(report.arguments.at("log_level")) ==
             "DEBUG");
  BOOST_TEST(std::get<std::int32_t>(report.arguments.at("retry_count")) == 3);
}

BOOST_AUTO_TEST_CASE(validate_required_if_triggered_when_condition_true) {
  const arguments_t args = {{"payment_method", std::string{"card"}}};

  const std::vector<validation_rule> rules = {
      {.key = "payment_method",
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "confirmation_code",
       .required = false,
       .supported_types = {data_type::string},
       .required_if = [](const arguments_t &all_args) {
         const auto it = all_args.find("payment_method");
         return it != all_args.end() &&
                std::get<std::string>(it->second) == "card";
       }}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(report, "confirmation_code",
                       validation_issue_code::required_if_failed,
                       "Conditionally required argument missing: "
                       "'confirmation_code'. Aliases: (none). Supported types: "
                       "'string'",
                       validation_phase::key_resolution));
}

BOOST_AUTO_TEST_CASE(validate_allowed_values_rejects_unknown_value) {
  const arguments_t args = {{"direction", std::string{"hold"}}};

  const std::vector<validation_rule> rules = {
      {.key = "direction",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"buy"}},
                          data_t{std::string{"sell"}}}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(
      report, "direction", validation_issue_code::value_not_allowed,
      "Value not allowed for argument: 'direction'. Allowed values: 'buy', "
      "'sell'",
      validation_phase::value_validation));
}

BOOST_AUTO_TEST_CASE(validate_range_rejects_value_below_min) {
  const arguments_t args = {{"quantity", std::int32_t{0}}};

  const std::vector<validation_rule> rules = {
      {.key = "quantity",
       .required = true,
       .supported_types = {data_type::integer},
       .min_value = 1.0,
       .max_value = 1000000.0}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(report, "quantity",
                       validation_issue_code::value_out_of_range,
                       "Value out of range for argument: 'quantity'. Min: 1. "
                       "Actual: 0",
                       validation_phase::value_validation));
}

BOOST_AUTO_TEST_CASE(validate_range_rejects_numeric_array_element_below_min) {
  const arguments_t args = {{"prices", std::deque<double>{99.50, -1.0}}};

  const std::vector<validation_rule> rules = {
      {.key = "prices",
       .required = true,
       .supported_types = {data_type::floating_point_array},
       .min_value = 0.0}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(report, "prices",
                       validation_issue_code::value_out_of_range,
                       "Value out of range for argument: 'prices[1]'. Min: 0. "
                       "Actual: -1",
                       validation_phase::value_validation));
}

BOOST_AUTO_TEST_CASE(validate_exactly_one_of_rejects_multiple_present_keys) {
  const arguments_t args = {{"host", std::string{"localhost"}},
                            {"api_key", std::string{"k"}},
                            {"password", std::string{"pw"}}};

  const std::vector<validation_rule> rules = {
      {.key = "host", .required = true, .supported_types = {data_type::string}},
      {.key = "api_key",
       .required = false,
       .supported_types = {data_type::string}},
      {.key = "oauth_token",
       .required = false,
       .supported_types = {data_type::string}},
      {.key = "password",
       .required = false,
       .supported_types = {data_type::string}}};

  const std::vector<validation_constraint> constraints = {
      {.type = constraint_type::exactly_one_of,
       .keys = {"api_key", "oauth_token", "password"}}};

  const auto report =
      validator::validate_and_finalize(args, rules, constraints);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(report, "",
                       validation_issue_code::exactly_one_of_violated,
                       "Exactly one of ['api_key', 'oauth_token', 'password'] "
                       "must be present, found 2",
                       validation_phase::constraint_validation));
}

BOOST_AUTO_TEST_CASE(
    validate_mutually_exclusive_rejects_multiple_present_keys) {
  const arguments_t args = {{"data", std::string{"x"}},
                            {"format", std::string{"json"}},
                            {"format_csv", bool{true}}};

  const std::vector<validation_rule> rules = {
      {.key = "data", .required = true, .supported_types = {data_type::string}},
      {.key = "format",
       .required = false,
       .supported_types = {data_type::string}},
      {.key = "format_xml",
       .required = false,
       .supported_types = {data_type::boolean}},
      {.key = "format_csv",
       .required = false,
       .supported_types = {data_type::boolean}}};

  const std::vector<validation_constraint> constraints = {
      {.type = constraint_type::mutually_exclusive,
       .keys = {"format", "format_xml", "format_csv"}}};

  const auto report =
      validator::validate_and_finalize(args, rules, constraints);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(
      report, "", validation_issue_code::mutually_exclusive_violated,
      "Mutually exclusive arguments present: ['format', 'format_csv']. At most "
      "one of ['format', 'format_csv', 'format_xml'] may be used",
      validation_phase::constraint_validation));
}

BOOST_AUTO_TEST_CASE(validate_fixed_coupon_bond_arguments) {
  const arguments_t args = {{"issuer", std::string{"ACME_CORP"}},
                            {"par_value", double{100.0}},
                            {"coupon_rate", double{0.0525}},
                            {"coupon_frequency", std::string{"semiannual"}},
                            {"day_count", std::string{"30/360"}},
                            {"currency", std::string{"usd"}},
                            {"maturity_date", make_date(2030, 12, 31)}};

  const std::vector<validation_rule> rules = {
      {.key = "issuer",
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "par_value",
       .required = true,
       .supported_types = {data_type::floating_point},
       .custom_validator =
           [](const data_t &value) {
             validation_result result{.key = "par_value"};
             if (std::get<double>(value) <= 0.0) {
               result.errors.push_back("par_value must be positive");
             }
             return result;
           },
       .min_value = 0.01},
      {.key = "coupon_rate",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.0,
       .max_value = 1.0},
      {.key = "coupon_frequency",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"annual"}},
                          data_t{std::string{"semiannual"}},
                          data_t{std::string{"quarterly"}}}},
      {.key = "day_count",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"30/360"}},
                          data_t{std::string{"act/360"}},
                          data_t{std::string{"act/365"}}}},
      {.key = "currency",
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string currency = std::get<std::string>(value);
             std::transform(currency.begin(), currency.end(), currency.begin(),
                            [](unsigned char ch) {
                              return static_cast<char>(std::toupper(ch));
                            });
             return data_t{currency};
           },
       .allowed_values = {data_t{std::string{"USD"}},
                          data_t{std::string{"EUR"}},
                          data_t{std::string{"GBP"}}}},
      {.key = "maturity_date",
       .required = true,
       .supported_types = {data_type::date}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(std::get<std::string>(report.arguments.at("currency")) == "USD");
  BOOST_TEST(report.consumed_keys.contains("issuer"));
  BOOST_TEST(report.consumed_keys.contains("par_value"));
  BOOST_TEST(report.consumed_keys.contains("coupon_rate"));
  BOOST_TEST(report.consumed_keys.contains("coupon_frequency"));
  BOOST_TEST(report.consumed_keys.contains("day_count"));
  BOOST_TEST(report.consumed_keys.contains("currency"));
  BOOST_TEST(report.consumed_keys.contains("maturity_date"));
}

BOOST_AUTO_TEST_CASE(validate_call_option_arguments) {
  const arguments_t args = {{"underlying", std::string{"AAPL"}},
                            {"strike", double{200.0}},
                            {"expiry", make_date(2026, 12, 18)},
                            {"style", std::string{"european"}},
                            {"option_side", std::string{"call"}},
                            {"premium", double{7.25}},
                            {"settlement_currency", std::string{"usd"}}};

  const std::vector<validation_rule> rules = {
      {.key = "underlying",
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "strike",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.01},
      {.key = "expiry", .required = true, .supported_types = {data_type::date}},
      {.key = "style",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"european"}},
                          data_t{std::string{"american"}}}},
      {.key = "option_side",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"call"}},
                          data_t{std::string{"put"}}}},
      {.key = "premium",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.0},
      {.key = "settlement_currency",
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string currency = std::get<std::string>(value);
             std::transform(currency.begin(), currency.end(), currency.begin(),
                            [](unsigned char ch) {
                              return static_cast<char>(std::toupper(ch));
                            });
             return data_t{currency};
           },
       .allowed_values = {data_t{std::string{"USD"}},
                          data_t{std::string{"EUR"}},
                          data_t{std::string{"JPY"}}}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(std::get<std::string>(
                 report.arguments.at("settlement_currency")) == "USD");
  BOOST_TEST(report.consumed_keys.contains("underlying"));
  BOOST_TEST(report.consumed_keys.contains("strike"));
  BOOST_TEST(report.consumed_keys.contains("expiry"));
  BOOST_TEST(report.consumed_keys.contains("style"));
  BOOST_TEST(report.consumed_keys.contains("option_side"));
  BOOST_TEST(report.consumed_keys.contains("premium"));
  BOOST_TEST(report.consumed_keys.contains("settlement_currency"));
}

BOOST_AUTO_TEST_CASE(validate_swaption_arguments) {
  const arguments_t args = {{"underlier", std::string{"5Y IRS"}},
                            {"notional", double{25000000.0}},
                            {"fixed_rate", double{0.0325}},
                            {"exercise_style", std::string{"bermudan"}},
                            {"swaption_type", std::string{"payer"}},
                            {"expiry", make_date(2027, 6, 15)},
                            {"pay_currency", std::string{"eur"}}};

  const std::vector<validation_rule> rules = {
      {.key = "underlier",
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "notional",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 1.0},
      {.key = "fixed_rate",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.0,
       .max_value = 1.0},
      {.key = "exercise_style",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"european"}},
                          data_t{std::string{"bermudan"}},
                          data_t{std::string{"american"}}}},
      {.key = "swaption_type",
       .required = true,
       .supported_types = {data_type::string},
       .allowed_values = {data_t{std::string{"payer"}},
                          data_t{std::string{"receiver"}}}},
      {.key = "expiry", .required = true, .supported_types = {data_type::date}},
      {.key = "pay_currency",
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string currency = std::get<std::string>(value);
             std::transform(currency.begin(), currency.end(), currency.begin(),
                            [](unsigned char ch) {
                              return static_cast<char>(std::toupper(ch));
                            });
             return data_t{currency};
           },
       .allowed_values = {data_t{std::string{"USD"}},
                          data_t{std::string{"EUR"}},
                          data_t{std::string{"GBP"}}}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(std::get<std::string>(report.arguments.at("pay_currency")) ==
             "EUR");
  BOOST_TEST(report.consumed_keys.contains("underlier"));
  BOOST_TEST(report.consumed_keys.contains("notional"));
  BOOST_TEST(report.consumed_keys.contains("fixed_rate"));
  BOOST_TEST(report.consumed_keys.contains("exercise_style"));
  BOOST_TEST(report.consumed_keys.contains("swaption_type"));
  BOOST_TEST(report.consumed_keys.contains("expiry"));
  BOOST_TEST(report.consumed_keys.contains("pay_currency"));
}

BOOST_AUTO_TEST_CASE(validate_strict_mode_reports_unconsumed_keys) {
  const arguments_t args = {{"known", std::int32_t{1}}, {"extra", bool{true}}};

  const std::vector<validation_rule> rules = {
      {.key = "known",
       .required = true,
       .supported_types = {data_type::integer}}};

  validation_options options;
  options.strict_unconsumed_keys = true;

  const auto report =
      validator::validate_and_finalize(args, rules, {}, options);

  BOOST_TEST(report.consumed_keys.contains("known"));
  BOOST_TEST(report.unconsumed_keys.contains("extra"));
  BOOST_TEST(has_issue(report, "extra", validation_issue_code::unconsumed_key,
                       "Unconsumed argument: 'extra'",
                       validation_phase::consumption));
}

BOOST_AUTO_TEST_CASE(validate_two_pass_rules_with_discriminator_key_selection) {
  const arguments_t args = {{"k", std::string{"bond"}},
                            {"portfolio_id", std::int32_t{42}},
                            {"coupon_rate", double{0.0525}},
                            {"maturity_date", make_date(2031, 12, 31)},
                            {"debug_flag", bool{true}}};

  const std::vector<validation_rule> common_rules = {
      {.key = "kind",
       .aliases = {"k"},
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string s = std::get<std::string>(value);
             std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
               return static_cast<char>(std::toupper(c));
             });
             return data_t{s};
           },
       .allowed_values = {data_t{std::string{"BOND"}},
                          data_t{std::string{"OPTION"}}}},
      {.key = "portfolio_id",
       .required = true,
       .supported_types = {data_type::integer}}};

  validation_options permissive;
  permissive.strict_unconsumed_keys = false;

  const auto common_report =
      validator::validate_and_finalize(args, common_rules, {}, permissive);
  BOOST_TEST(common_report.ok());
  BOOST_TEST(common_report.arguments.contains("kind"));
  BOOST_TEST(!common_report.arguments.contains("k"));
  BOOST_TEST(std::get<std::string>(common_report.arguments.at("kind")) ==
             "BOND");

  const std::string kind =
      std::get<std::string>(common_report.arguments.at("kind"));

  std::vector<validation_rule> specific_rules;
  if (kind == "BOND") {
    specific_rules = {
        {.key = "coupon_rate",
         .required = true,
         .supported_types = {data_type::floating_point},
         .min_value = 0.0,
         .max_value = 1.0},
        {.key = "maturity_date",
         .required = true,
         .supported_types = {data_type::date}},
    };
  } else if (kind == "OPTION") {
    specific_rules = {
        {.key = "strike",
         .required = true,
         .supported_types = {data_type::floating_point},
         .min_value = 0.01},
        {.key = "expiry",
         .required = true,
         .supported_types = {data_type::date}},
    };
  }

  const auto specific_report = validator::validate_and_finalize(
      common_report.arguments, specific_rules, {}, permissive);
  BOOST_TEST(specific_report.ok());
  BOOST_TEST(specific_report.consumed_keys.contains("coupon_rate"));
  BOOST_TEST(specific_report.consumed_keys.contains("maturity_date"));

  std::vector<validation_rule> all_rules = common_rules;
  all_rules.insert(all_rules.end(), specific_rules.begin(),
                   specific_rules.end());

  validation_options strict;
  strict.strict_unconsumed_keys = true;

  const auto final_report = validator::validate_and_finalize(
      specific_report.arguments, all_rules, {}, strict);
  BOOST_TEST(!final_report.ok());
  BOOST_TEST(final_report.unconsumed_keys.contains("debug_flag"));
  BOOST_TEST(has_issue(
      final_report, "debug_flag", validation_issue_code::unconsumed_key,
      "Unconsumed argument: 'debug_flag'", validation_phase::consumption));
}

BOOST_AUTO_TEST_CASE(
    validate_member_api_accumulates_report_and_finalizes_once) {
  const arguments_t args = {{"k", std::string{"bond"}},
                            {"portfolio_id", std::int32_t{7}},
                            {"coupon_rate", double{0.035}},
                            {"maturity_date", make_date(2032, 6, 30)},
                            {"debug_flag", bool{true}}};

  const std::vector<validation_rule> common_rules = {
      {.key = "kind",
       .aliases = {"k"},
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string s = std::get<std::string>(value);
             std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
               return static_cast<char>(std::toupper(c));
             });
             return data_t{s};
           },
       .allowed_values = {data_t{std::string{"BOND"}},
                          data_t{std::string{"OPTION"}}}},
      {.key = "portfolio_id",
       .required = true,
       .supported_types = {data_type::integer}}};

  const std::vector<validation_rule> bond_rules = {
      {.key = "coupon_rate",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.0,
       .max_value = 1.0},
      {.key = "maturity_date",
       .required = true,
       .supported_types = {data_type::date}}};

  validation_options options;
  options.strict_unconsumed_keys = true;

  validator pipeline(args, options);
  pipeline.validate(common_rules);
  BOOST_TEST(pipeline.report().ok());
  BOOST_TEST(std::get<std::string>(pipeline.report().arguments.at("kind")) ==
             "BOND");

  pipeline.validate(bond_rules);
  BOOST_TEST(pipeline.report().ok());
  BOOST_TEST(pipeline.report().consumed_keys.contains("kind"));
  BOOST_TEST(pipeline.report().consumed_keys.contains("portfolio_id"));
  BOOST_TEST(pipeline.report().consumed_keys.contains("coupon_rate"));
  BOOST_TEST(pipeline.report().consumed_keys.contains("maturity_date"));

  const auto &final_report = pipeline.finalize();
  BOOST_TEST(!final_report.ok());
  BOOST_TEST(final_report.unconsumed_keys.contains("debug_flag"));
  BOOST_TEST(has_issue(
      final_report, "debug_flag", validation_issue_code::unconsumed_key,
      "Unconsumed argument: 'debug_flag'", validation_phase::consumption));
}

BOOST_AUTO_TEST_CASE(validate_finance_collection_argument_types) {
  const std::deque<date_t> coupon_dates = {make_date(2027, 6, 15),
                                           make_date(2027, 12, 15)};
  const arguments_t args = {
      {"coupon_dates", coupon_dates},
      {"coupon_rates", std::deque<double>{0.0425, 0.0450}},
      {"call_schedule_active", std::deque<bool>{true, false}},
      {"amortization_steps", std::deque<std::int32_t>{100, 80}},
      {"basket_isins",
       std::deque<std::string>{"US0123456789", "US9876543210"}}};

  const std::vector<validation_rule> rules = {
      {.key = "coupon_dates",
       .required = true,
       .supported_types = {data_type::date_array}},
      {.key = "coupon_rates",
       .required = true,
       .supported_types = {data_type::floating_point_array}},
      {.key = "call_schedule_active",
       .required = true,
       .supported_types = {data_type::boolean_array}},
      {.key = "amortization_steps",
       .required = true,
       .supported_types = {data_type::integer_array}},
      {.key = "basket_isins",
       .required = true,
       .supported_types = {data_type::string_array}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(report.ok());
  BOOST_TEST(report.consumed_keys.contains("coupon_dates"));
  BOOST_TEST(report.consumed_keys.contains("coupon_rates"));
  BOOST_TEST(report.consumed_keys.contains("call_schedule_active"));
  BOOST_TEST(report.consumed_keys.contains("amortization_steps"));
  BOOST_TEST(report.consumed_keys.contains("basket_isins"));
}

BOOST_AUTO_TEST_CASE(format_finance_collection_values) {
  BOOST_TEST(aks::to_string(data_t{std::deque<bool>{true, false, true}}) ==
             "[true, false, true]");
  BOOST_TEST(aks::to_string(data_t{std::deque<std::int32_t>{100, 80, 60}}) ==
             "[100, 80, 60]");
  BOOST_TEST(aks::to_string(data_t{std::deque<double>{0.0425, 0.0450}}) ==
             "[0.042500, 0.045000]");
  BOOST_TEST(aks::to_string(data_t{std::deque<std::string>{"6M", "1Y"}}) ==
             "[6M, 1Y]");
  BOOST_TEST(aks::to_string(data_t{std::deque<date_t>{
                 std::chrono::year{2027} / std::chrono::month{6} /
                     std::chrono::day{15},
                 std::chrono::year{2027} / std::chrono::month{12} /
                     std::chrono::day{15}}}) == "[2027-06-15, 2027-12-15]");
}

BOOST_AUTO_TEST_CASE(validate_finance_collection_errors_render_array_values) {
  const arguments_t args = {
      {"exercise_dates", std::deque<std::string>{"2027-06-15", "2027-12-15"}},
      {"coupon_schedule",
       std::deque<date_t>{make_date(2027, 6, 15), make_date(2027, 12, 15)}}};

  const std::vector<validation_rule> rules = {
      {.key = "exercise_dates",
       .required = true,
       .supported_types = {data_type::date_array}},
      {.key = "coupon_schedule",
       .required = true,
       .supported_types = {data_type::date_array},
       .allowed_values = {data_t{std::deque<date_t>{
           make_date(2028, 6, 15), make_date(2028, 12, 15)}}}}};

  const auto report = validator::validate_and_finalize(args, rules);

  BOOST_TEST(!report.ok());
  BOOST_TEST(has_issue(report, "exercise_dates",
                       validation_issue_code::type_mismatch,
                       "Unsupported type for argument: 'exercise_dates'. "
                       "Supported types: 'date_array'. Actual type: "
                       "'string_array'",
                       validation_phase::value_validation));
  BOOST_TEST(has_issue(report, "coupon_schedule",
                       validation_issue_code::value_not_allowed,
                       "Value not allowed for argument: 'coupon_schedule'. "
                       "Allowed values: '[2028-06-15, 2028-12-15]'",
                       validation_phase::value_validation));
}

BOOST_AUTO_TEST_CASE(validate_complex_finance_portfolio_scenario) {
  // A large, realistic finance portfolio submission exercising many
  // validation framework features: arrays, transformers, defaults,
  // conditional required fields, custom validators, and constraints.
  const arguments_t args = {
      {"portfolio_name", std::string{"AlphaFund"}},
      {"total_notional", double{50000000.0}},
      {"positions_count", std::int32_t{2}},
      {"isins", std::deque<std::string>{"US1234567890", "US9876543210"}},
      {"quantities", std::deque<std::int32_t>{100, 200}},
      {"prices", std::deque<double>{99.50, 101.25}},
      {"currencies", std::deque<std::string>{"usd", "eur"}},
      {"maturities",
       std::deque<date_t>{make_date(2030, 1, 1), make_date(2028, 6, 30)}},
      {"has_options", bool{true}},
      {"option_underlyings", std::deque<std::string>{"AAPL"}},
      {"option_strikes", std::deque<double>{200.0}},
      {"option_expiries", std::deque<date_t>{make_date(2026, 12, 18)}},
      {"option_types", std::deque<std::string>{"call"}},
      {"settlement_method", std::string{"cash"}},
      {"settlement_currency", std::string{"usd"}},
      // an unexpected extra key to later demonstrate strict-mode failure
      {"extra", bool{true}}};

  const std::vector<validation_rule> rules = {
      {.key = "portfolio_name",
       .required = true,
       .supported_types = {data_type::string}},
      {.key = "total_notional",
       .required = true,
       .supported_types = {data_type::floating_point},
       .min_value = 0.0},
      {.key = "positions_count",
       .required = true,
       .supported_types = {data_type::integer}},
      {.key = "isins",
       .required = true,
       .supported_types = {data_type::string_array}},
      {.key = "quantities",
       .required = true,
       .supported_types = {data_type::integer_array},
       .custom_validator =
           [](const data_t &value) {
             validation_result result{.key = "quantities"};
             const auto &vals = std::get<std::deque<std::int32_t>>(value);
             for (std::size_t i = 0; i < vals.size(); ++i) {
               if (vals[i] <= 0) {
                 result.errors.push_back("quantities must be positive");
                 break;
               }
             }
             return result;
           }},
      {.key = "prices",
       .required = true,
       .supported_types = {data_type::floating_point_array},
       .min_value = 0.0},
      {.key = "currencies",
       .required = true,
       .supported_types = {data_type::string_array},
       .transformer =
           [](const data_t &value) {
             const auto in = std::get<std::deque<std::string>>(value);
             std::deque<std::string> out;
             for (const auto &s : in) {
               std::string u = s;
               std::transform(u.begin(), u.end(), u.begin(),
                              [](unsigned char c) {
                                return static_cast<char>(std::toupper(c));
                              });
               out.push_back(u);
             }
             return data_t{out};
           }},
      {.key = "maturities",
       .required = true,
       .supported_types = {data_type::date_array}},
      {.key = "has_options",
       .required = true,
       .supported_types = {data_type::boolean}},
      {.key = "option_underlyings",
       .required = false,
       .supported_types = {data_type::string_array},
       .required_if =
           [](const arguments_t &all_args) {
             const auto it = all_args.find("has_options");
             return it != all_args.end() && std::get<bool>(it->second);
           }},
      {.key = "option_strikes",
       .required = false,
       .supported_types = {data_type::floating_point_array},
       .required_if =
           [](const arguments_t &all_args) {
             const auto it = all_args.find("has_options");
             return it != all_args.end() && std::get<bool>(it->second);
           }},
      {.key = "option_expiries",
       .required = false,
       .supported_types = {data_type::date_array},
       .required_if =
           [](const arguments_t &all_args) {
             const auto it = all_args.find("has_options");
             return it != all_args.end() && std::get<bool>(it->second);
           }},
      {.key = "option_types",
       .required = false,
       .supported_types = {data_type::string_array},
       .transformer =
           [](const data_t &value) {
             const auto in = std::get<std::deque<std::string>>(value);
             std::deque<std::string> out;
             for (const auto &s : in) {
               std::string u = s;
               std::transform(u.begin(), u.end(), u.begin(),
                              [](unsigned char c) {
                                return static_cast<char>(std::toupper(c));
                              });
               out.push_back(u);
             }
             return data_t{out};
           },
       .required_if =
           [](const arguments_t &all_args) {
             const auto it = all_args.find("has_options");
             return it != all_args.end() && std::get<bool>(it->second);
           },
       .allowed_values = {data_t{std::deque<std::string>{"CALL"}},
                          data_t{std::deque<std::string>{"PUT"}}}},
      {.key = "settlement_method",
       .required = false,
       .supported_types = {data_type::string}},
      {.key = "settlement_account",
       .required = false,
       .supported_types = {data_type::string}},
      {.key = "settlement_currency",
       .required = true,
       .supported_types = {data_type::string},
       .transformer =
           [](const data_t &value) {
             std::string currency = std::get<std::string>(value);
             std::transform(currency.begin(), currency.end(), currency.begin(),
                            [](unsigned char ch) {
                              return static_cast<char>(std::toupper(ch));
                            });
             return data_t{currency};
           },
       .allowed_values = {data_t{std::string{"USD"}},
                          data_t{std::string{"EUR"}},
                          data_t{std::string{"GBP"}}}},
  };

  const std::vector<validation_constraint> constraints = {
      {.type = constraint_type::mutually_exclusive,
       .keys = {"settlement_method", "settlement_account"}},
      {.type = constraint_type::exactly_one_of,
       .keys = {"settlement_method", "settlement_account"}}};

  // 1) Validate in permissive mode: many transformations and custom validators
  // should succeed and the framework should consume all known keys.
  validation_options permissive_options;
  permissive_options.strict_unconsumed_keys = false;

  const auto permissive_report = validator::validate_and_finalize(
      args, rules, constraints, permissive_options);

  std::string debug_msg;
  if (!permissive_report.ok()) {
    for (const auto &issue : permissive_report.issues) {
      debug_msg += "issue key=" + issue.key +
                   " code=" + std::to_string(static_cast<int>(issue.code)) +
                   " phase=" + std::to_string(static_cast<int>(issue.phase)) +
                   " msg='" + issue.message + "'\n";
    }
    for (const auto &k : permissive_report.unconsumed_keys) {
      debug_msg += "unconsumed key: " + k + "\n";
    }
  }
  BOOST_CHECK_MESSAGE(permissive_report.ok(), debug_msg);
  // currencies and settlement_currency should be uppercased by transformers
  const auto currencies = std::get<std::deque<std::string>>(
      permissive_report.arguments.at("currencies"));
  BOOST_TEST(currencies[0] == "USD");
  BOOST_TEST(currencies[1] == "EUR");
  BOOST_TEST(std::get<std::string>(permissive_report.arguments.at(
                 "settlement_currency")) == "USD");

  // option types transformed to upper-case and allowed_values matched
  const auto option_types = std::get<std::deque<std::string>>(
      permissive_report.arguments.at("option_types"));
  BOOST_TEST(option_types[0] == "CALL");

  // Many keys were consumed
  BOOST_TEST(permissive_report.consumed_keys.contains("portfolio_name"));
  BOOST_TEST(permissive_report.consumed_keys.contains("isins"));
  BOOST_TEST(permissive_report.consumed_keys.contains("quantities"));
  BOOST_TEST(permissive_report.consumed_keys.contains("option_types"));

  // 2) Validate in strict mode: the unexpected `extra` key should be reported
  validation_options strict_options;
  strict_options.strict_unconsumed_keys = true;

  const auto strict_report = validator::validate_and_finalize(
      args, rules, constraints, strict_options);

  BOOST_TEST(!strict_report.ok());
  BOOST_TEST(strict_report.unconsumed_keys.contains("extra"));
  BOOST_TEST(
      has_issue(strict_report, "extra", validation_issue_code::unconsumed_key,
                "Unconsumed argument: 'extra'", validation_phase::consumption));
}
