#define BOOST_PARSER_DISABLE_HANA_TUPLE
#include <boost/parser/parser.hpp>
#include <iostream>
#include <map>

// anonymous namespace
namespace {

namespace bp = boost::parser;

// debug helper
template <typename T>
struct type_is;

enum class CapState
{
  FALSE,
  MAYBE_FALSE,
  UNKNOWN,
  MAYBE_TRUE,
  TRUE
};

const auto f_name = [](auto & ctx)
{
  const auto name = _attr(ctx);
  std::cout << "NAME: " << name << '\n';
  _val(ctx) = name;
};

const std::map<std::string, bool> caps = {{"petsc", true}, {"nope", false}};

// capability name
bp::rule<struct start_letter_tag, char> start_letter = "first letter of an identifier";
bp::rule<struct cont_letter_tag, char> cont_letter = "continuation of an identifier";
bp::rule<struct name_tag, std::string> name = "capability name";
auto const start_letter_def = bp::lower | bp::upper | '_';
auto const cont_letter_def = start_letter_def | bp::digit;
auto const name_def = (start_letter >> *(cont_letter))[f_name];
BOOST_PARSER_DEFINE_RULES(start_letter, cont_letter, name);

// check bool existence
const auto f_identifier = [](auto & ctx)
{
  if (caps.find(_attr(ctx)) != caps.end())
    _val(ctx) = CapState::TRUE;
  else
    _val(ctx) = CapState::MAYBE_FALSE;
};

const auto f_not_identifier = [](auto & ctx)
{
  if (caps.find(_attr(ctx)) != caps.end())
    _val(ctx) = CapState::FALSE;
  else
    _val(ctx) = CapState::MAYBE_TRUE;
};

const auto f_compare = [](auto & ctx)
{
  const auto & [left, op, right] = _attr(ctx);
  static_assert(
      std::is_same_v<decltype(right), const std::variant<std::string, std::vector<unsigned int>>>,
      "Unexpected RHS value type in comparison");

  // check existence
  if (caps.find(left) == caps.end())
  {
    // return an unknown if the capability does not exist, this is important as it
    // stays unknown upon negation
    _val(ctx) = CapState::UNKNOWN;
    return;
  }

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

  // string comparison
  if (std::holds_alternative<std::string>(right))
    std::cout << "string: " << left << ' ' << op << ' ' << std::get<std::string>(right) << '\n';

  // number or version comparison
  if (std::holds_alternative<std::vector<unsigned int>>(right))
  {
    std::cout << "version: " << left << ' ';
    for (const auto num : std::get<std::vector<unsigned int>>(right))
      std::cout << num << '.';
    std::cout << '\n';
  }

  _val(ctx) = CapState::FALSE;
};

bp::symbols<int> const comparison = {
    {"<=", 0}, {">=", 1}, {"<", 2}, {">", 3}, {"!=", 4}, {"==", 5}, {"=", 5}};

bp::symbols<int> const conjunction = {{"&", 0}, {"|", 1}};

// capability value
bp::rule<struct generic_tag, std::string> generic = "generic capability value";
bp::rule<struct version_tag, std::vector<unsigned int>> version = "version number";
bp::rule<struct value_tag, std::variant<std::string, std::vector<unsigned int>>> value =
    "capability value";
auto const generic_def = +(bp::lower | bp::upper | bp::digit | '_' | '-');
auto const version_def = bp::uint_ >> *('.' >> bp::uint_);
auto const value_def = version | generic;
// auto const value_def = +(bp::lower | bp::upper | '_' | '-') | (bp::digit >> *('.' >>
// +bp::digit));
BOOST_PARSER_DEFINE_RULES(generic, version, value);

std::string
pcap(CapState c)
{
  switch (c)
  {
    case CapState::FALSE:
      return "false";
    case CapState::TRUE:
      return "true";
    case CapState::MAYBE_FALSE:
      return "maybe_false";
    case CapState::MAYBE_TRUE:
      return "maybe_true";
    default:
      return "UNKNOWN";
  }
}

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

// bool_statement
bp::rule<struct bool_statement_tag, CapState> bool_statement = "bool statement";

// expression
bp::rule<struct p_conjunction_tag, CapState> p_conjunction = "conjunction expression";
bp::rule<struct expr_tag, CapState> expr = "boolean expression";

auto const p_conjunction_def = (bool_statement > *(conjunction > bool_statement))[f_conjunction];

auto const expr_def = p_conjunction;

auto const bool_statement_def = ('!' >> name)[f_not_identifier] |
                                (name >> comparison >> value)[f_compare] | name[f_identifier] |
                                ('(' > expr > ')')[f_pass] | ("!(" > expr > ')')[f_negate];

BOOST_PARSER_DEFINE_RULES(p_conjunction, expr, bool_statement);
}

int
main()
{
  // std::string input = "2.445.5";
  std::string input = "petsc & !(f>23.4.12 & thermochimica)";
  // std::string input = "a & b";

  auto const result = bp::parse(input, expr, bp::ws); //, bp::trace::on);
  std::cout << pcap(*result) << '\n';

  return 0;
}
