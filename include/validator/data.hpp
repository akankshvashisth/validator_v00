#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace aks {

using date_t = std::chrono::year_month_day;

using data_t = std::variant<std::monostate, bool, std::int32_t, double,
                            std::string, date_t, std::deque<bool>,
                            std::deque<std::int32_t>, std::deque<double>,
                            std::deque<std::string>, std::deque<date_t>>;

enum class data_type {
  null,
  boolean,
  integer,
  floating_point,
  string,
  date,
  boolean_array,
  integer_array,
  floating_point_array,
  string_array,
  date_array,
};

template <typename T> struct enum_helper;

template <> struct enum_helper<data_type> {
  static constexpr std::array<data_type, 12> values = {
      data_type::null,
      data_type::boolean,
      data_type::integer,
      data_type::floating_point,
      data_type::string,
      data_type::date,
      data_type::boolean_array,
      data_type::integer_array,
      data_type::floating_point_array,
      data_type::string_array,
      data_type::date_array};

  static constexpr std::array<const char *, 12> names = {
      "null",           "boolean",       "integer",
      "floating_point", "string",        "date",
      "boolean_array",  "integer_array", "floating_point_array",
      "string_array",   "date_array"};

  static constexpr const char *to_string(data_type type) {
    for (size_t i = 0; i < values.size(); ++i) {
      if (values[i] == type) {
        return names[i];
      }
    }
    return "unknown";
  }

  static constexpr data_type from_string(const std::string &name) {
    for (size_t i = 0; i < names.size(); ++i) {
      if (names[i] == name) {
        return values[i];
      }
    }
    throw std::invalid_argument("Invalid data type name: " + name);
  }
};

template <data_type T> struct data_type_traits;

template <> struct data_type_traits<data_type::null> {
  using type = std::monostate;
};

template <> struct data_type_traits<data_type::boolean> {
  using type = bool;
};

template <> struct data_type_traits<data_type::integer> {
  using type = std::int32_t;
};

template <> struct data_type_traits<data_type::floating_point> {
  using type = double;
};

template <> struct data_type_traits<data_type::string> {
  using type = std::string;
};

template <> struct data_type_traits<data_type::date> {
  using type = date_t;
};

template <> struct data_type_traits<data_type::boolean_array> {
  using type = std::deque<bool>;
};

template <> struct data_type_traits<data_type::integer_array> {
  using type = std::deque<std::int32_t>;
};

template <> struct data_type_traits<data_type::floating_point_array> {
  using type = std::deque<double>;
};

template <> struct data_type_traits<data_type::string_array> {
  using type = std::deque<std::string>;
};

template <> struct data_type_traits<data_type::date_array> {
  using type = std::deque<date_t>;
};

data_type get_data_type(const data_t &data) {
  return std::visit(
      [](const auto &value) -> data_type {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return data_type::null;
        } else if constexpr (std::is_same_v<T, bool>) {
          return data_type::boolean;
        } else if constexpr (std::is_same_v<T, std::int32_t>) {
          return data_type::integer;
        } else if constexpr (std::is_same_v<T, double>) {
          return data_type::floating_point;
        } else if constexpr (std::is_same_v<T, std::string>) {
          return data_type::string;
        } else if constexpr (std::is_same_v<T, date_t>) {
          return data_type::date;
        } else if constexpr (std::is_same_v<T, std::deque<bool>>) {
          return data_type::boolean_array;
        } else if constexpr (std::is_same_v<T, std::deque<std::int32_t>>) {
          return data_type::integer_array;
        } else if constexpr (std::is_same_v<T, std::deque<double>>) {
          return data_type::floating_point_array;
        } else if constexpr (std::is_same_v<T, std::deque<std::string>>) {
          return data_type::string_array;
        } else if constexpr (std::is_same_v<T, std::deque<date_t>>) {
          return data_type::date_array;
        } else {
          throw std::invalid_argument("Unsupported data type");
        }
      },
      data);
}

std::string to_string(const data_t &value) {

  auto date_to_string = [](const date_t &d) {
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << static_cast<int>(d.year())
        << '-' << std::setw(2) << static_cast<unsigned>(d.month()) << '-'
        << std::setw(2) << static_cast<unsigned>(d.day());
    return oss.str();
  };

  return std::visit(
      [&](const auto &v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>)
          return "null";
        else if constexpr (std::is_same_v<T, bool>)
          return v ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::int32_t>)
          return std::to_string(v);
        else if constexpr (std::is_same_v<T, double>)
          return std::to_string(v);
        else if constexpr (std::is_same_v<T, std::string>)
          return v;
        else if constexpr (std::is_same_v<T, std::chrono::year_month_day>) {
          return date_to_string(v);
        } else if constexpr (std::is_same_v<T, std::deque<bool>>) {
          std::string result = "[";
          for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += v[i] ? "true" : "false";
          }
          result += "]";
          return result;
        } else if constexpr (std::is_same_v<T, std::deque<std::int32_t>>) {
          std::string result = "[";
          for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += std::to_string(v[i]);
          }
          result += "]";
          return result;
        } else if constexpr (std::is_same_v<T, std::deque<double>>) {
          std::string result = "[";
          for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += std::to_string(v[i]);
          }
          result += "]";
          return result;
        } else if constexpr (std::is_same_v<T, std::deque<std::string>>) {
          std::string result = "[";
          for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += v[i];
          }
          result += "]";
          return result;
        } else if constexpr (std::is_same_v<T, std::deque<date_t>>) {
          std::string result = "[";
          for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += date_to_string(v[i]);
          }
          result += "]";
          return result;
        } else {
          throw std::invalid_argument("Unsupported data type");
        }
      },
      value);
}

} // namespace aks
