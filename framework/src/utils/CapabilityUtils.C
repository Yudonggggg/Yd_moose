//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#define BOOST_PARSER_DISABLE_HANA_TUPLE
#include "libmesh/ignore_warnings.h"
#include <boost/parser/parser.hpp>
#include "libmesh/restore_warnings.h"

#include "CapabilityUtils.h"
#include "MooseUtilsStandalone.h"
#include "MooseError.h"
#include <vector>
#include <set>

namespace CapabilityUtils
{

enum class CapState
{
  FALSE,
  MAYBE_FALSE,
  UNKNOWN,
  MAYBE_TRUE,
  TRUE
};

// anonymous namespace
namespace
{
namespace bp = boost::parser;

//
// semantic actions
//

// matching a capability name
const auto f_name = [](auto & ctx)
{
  const auto name = _attr(ctx);
  auto & seen_capabilities = std::get<1>(_globals(ctx));
  seen_capabilities.insert(name);
  _val(ctx) = name;
};

// check bool existence
const auto f_identifier = [](auto & ctx)
{
  const auto & app_capabilities = std::get<0>(_globals(ctx));
  const auto it = app_capabilities.find(_attr(ctx));
  if (it != app_capabilities.end())
  {
    const auto app_value = it->second.first;
    if (std::holds_alternative<bool>(app_value) && std::get<bool>(app_value) == false)
    {
      _val(ctx) = CapState::FALSE;
      return;
    }
    _val(ctx) = CapState::TRUE;
  }
  else
    _val(ctx) = CapState::MAYBE_FALSE;
};

// check non-existence
const auto f_not_identifier = [](auto & ctx)
{
  const auto & app_capabilities = std::get<0>(_globals(ctx));
  const auto it = app_capabilities.find(_attr(ctx));
  if (it != app_capabilities.end())
  {
    const auto app_value = it->second.first;
    if (std::holds_alternative<bool>(app_value) && std::get<bool>(app_value) == false)
    {
      _val(ctx) = CapState::TRUE;
      return;
    }
    _val(ctx) = CapState::FALSE;
  }
  else
    _val(ctx) = CapState::MAYBE_TRUE;
};

// comparison operation
const auto f_compare = [](auto & ctx)
{
  const auto & [left, op, right] = _attr(ctx);
  static_assert(
      std::is_same_v<decltype(right), const std::variant<std::string, std::vector<unsigned int>>>,
      "Unexpected RHS value type in comparison");

  // check existence
  const auto & app_capabilities = std::get<0>(_globals(ctx));
  const auto it = app_capabilities.find(left);
  if (it == app_capabilities.end())
  {
    // return an unknown if the capability does not exist, this is important as it
    // stays unknown upon negation
    _val(ctx) = CapState::UNKNOWN;
    return;
  }

  // capability is registered by the app
  const auto & [app_value, doc] = it->second;

  // comparator
  auto comp = [](int i, auto a, auto b)
  {
    switch (i)
    {
      case 0:
        return a <= b;
      case 1:
        return a >= b;
      case 2:
        return a < b;
      case 3:
        return a > b;
      case 4:
        return a != b;
      case 5:
      case 6:
        return a == b;
    }
    return false;
  };

  // false bool capability will always fail any other comparison
  if (std::holds_alternative<bool>(app_value) && !std::get<bool>(app_value))
  {
    _val(ctx) = CapState::FALSE;
    return;
  }

  // string comparison
  if (std::holds_alternative<std::string>(right))
  {
    // the app value has to be a string
    if (!std::holds_alternative<std::string>(app_value))
    {
      _error_handler(ctx).diagnose(
          boost::parser::diagnostic_kind::error, "Unexpected comparison to a string.", ctx);
      return;
    }

    _val(ctx) = comp(op,
                     std::get<std::string>(app_value),
                     MooseUtils::toLower(std::get<std::string>(right)))
                    ? CapState::TRUE
                    : CapState::FALSE;
    return;
  }

  // number or version comparison
  if (std::holds_alternative<std::vector<unsigned int>>(right))
  {
    const auto & test_value = std::get<std::vector<unsigned int>>(right);

    // int comparison
    if (std::holds_alternative<int>(app_value))
    {
      if (test_value.size() != 1)
      {
        _error_handler(ctx).diagnose(
            boost::parser::diagnostic_kind::error, "Expected an integer value.", ctx);
        return;
      }

      _val(ctx) =
          comp(op, std::get<int>(app_value), test_value[0]) ? CapState::TRUE : CapState::FALSE;
      return;
    }

    // version comparison
    std::vector<unsigned int> app_value_version;
    if (!std::holds_alternative<std::string>(app_value) ||
        !MooseUtils::tokenizeAndConvert(std::get<std::string>(app_value), app_value_version, "."))
    {
      if (test_value.size() == 1)
        _error_handler(ctx).diagnose(boost::parser::diagnostic_kind::error,
                                     test_value.size() == 1
                                         ? "Cannot compare capability to a number."
                                         : "Cannot compare capability to a version number.",
                                     ctx);
      return;
    }

    // compare versions
    _val(ctx) = comp(op, app_value_version, test_value) ? CapState::TRUE : CapState::FALSE;
    return;
  }
};

const auto f_conjunction = [](auto & ctx)
{
  const auto & [s0, ss] = _attr(ctx);
  CapState s = s0;
  for (const auto & [op, sn] : ss)
  {
    if (op == 0)
    {
      // and
      const auto states = {CapState::FALSE,
                           CapState::MAYBE_FALSE,
                           CapState::UNKNOWN,
                           CapState::MAYBE_TRUE,
                           CapState::TRUE};
      for (const auto state : states)
        if (s == state || sn == state)
        {
          s = state;
          break;
        }
    }
    else if (op == 1)
    {
      // or
      const auto states = {CapState::TRUE,
                           CapState::MAYBE_TRUE,
                           CapState::UNKNOWN,
                           CapState::MAYBE_FALSE,
                           CapState::FALSE};
      for (const auto state : states)
        if (s == state || sn == state)
        {
          s = state;
          break;
        }
    }
    else
      _error_handler(ctx).diagnose(boost::parser::diagnostic_kind::error, "Unknown operator", ctx);
  }
  _val(ctx) = s;
};

const auto f_negate = [](auto & ctx)
{
  // negate current capability state
  switch (_attr(ctx))
  {
    case CapState::FALSE:
      _val(ctx) = CapState::TRUE;
      break;
    case CapState::TRUE:
      _val(ctx) = CapState::FALSE;
      break;
    case CapState::MAYBE_FALSE:
      _val(ctx) = CapState::MAYBE_TRUE;
      break;
    case CapState::MAYBE_TRUE:
      _val(ctx) = CapState::MAYBE_FALSE;
      break;
    default:
      _val(ctx) = CapState::UNKNOWN;
  }
};

const auto f_pass = [](auto & ctx)
{
  // pass through value
  _val(ctx) = _attr(ctx);
};

// capability name
bp::rule<struct start_letter_tag, char> start_letter = "first letter of an identifier";
bp::rule<struct cont_letter_tag, char> cont_letter = "continuation of an identifier";
bp::rule<struct name_tag, std::string> name = "capability name";

auto const start_letter_def = bp::lower | bp::upper | bp::char_('_');
auto const cont_letter_def = start_letter_def | bp::digit;
auto const name_def = (start_letter >> *(cont_letter))[f_name];

BOOST_PARSER_DEFINE_RULES(start_letter, cont_letter, name);

// symbols
bp::symbols<int> const comparison = {
    {"<=", 0}, {">=", 1}, {"<", 2}, {">", 3}, {"!=", 4}, {"==", 5}, {"=", 5}};
bp::symbols<int> const conjunction = {{"&", 0}, {"|", 1}};

// capability value
bp::rule<struct generic_tag, std::string> generic = "generic capability value";
bp::rule<struct version_tag, std::vector<unsigned int>> version = "version number";
bp::rule<struct value_tag, std::variant<std::string, std::vector<unsigned int>>> value =
    "capability value";

auto const generic_def = +(bp::lower | bp::upper | bp::digit | bp::char_('_') | bp::char_('-'));
auto const version_def = bp::uint_ >> *('.' >> bp::uint_);
auto const value_def = version | generic;

BOOST_PARSER_DEFINE_RULES(generic, version, value);

// expression
bp::rule<struct expr_tag, CapState> expr = "boolean expression";
bp::rule<struct p_conjunction_tag, CapState> p_conjunction = "conjunction expression";
bp::rule<struct bool_statement_tag, CapState> bool_statement = "bool statement";

auto const expr_def = p_conjunction;
auto const p_conjunction_def = (bool_statement > *(conjunction > bool_statement))[f_conjunction];
auto const bool_statement_def = ('!' >> name)[f_not_identifier] |
                                (name >> comparison >> value)[f_compare] | name[f_identifier] |
                                ('(' > expr > ')')[f_pass] | ("!(" > expr > ')')[f_negate];

BOOST_PARSER_DEFINE_RULES(p_conjunction, expr, bool_statement);
} // namespace

Result
check(const std::string & requirements, const Registry & app_capabilities)
{
  std::set<std::string> seen_capabilities;

  // globals for the parser
  auto globals = std::tie(app_capabilities, seen_capabilities);

  // error handler
  bp::callback_error_handler ceh([](std::string const & msg) { mooseError(msg); },
                                 [](std::string const & msg) { mooseWarning(msg); });
  // parse
  auto const result = bp::parse(requirements,
                                bp::with_error_handler(bp::with_globals(expr, globals), ceh),
                                bp::ws); //, bp::trace::on);

  // reduce result
  if (!result.has_value())
    mooseError("Failed to parse requirements '", requirements, "'");

  CheckState state = static_cast<CheckState>(result.value());
  std::string reason;
  std::string doc;

  return {state, reason, doc};
}
} // namespace CapabilityUtils

// Fix: ../modules/solid_mechanics/examples/cframe_iga/tests
// ../test/tests/meshgenerators/file_mesh_generator/tests
