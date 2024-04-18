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
  MAYBE_TRUE,
  TRUE
};


const std::map<std::string, bool> caps = {{"petsc", true}, {"nope", false}};

// capability name
bp::rule<struct start_letter_tag, char> start_letter = "first letter of an identifier";
bp::rule<struct cont_letter_tag, char> cont_letter = "continuation of an identifier";
bp::rule<struct name_tag, std::string> name = "capability name";
auto const start_letter_def = bp::lower | bp::upper | '_';
auto const cont_letter_def = start_letter_def | bp::digit;
auto const name_def = start_letter >> *(cont_letter);
BOOST_PARSER_DEFINE_RULES(start_letter, cont_letter, name);

// check bool existence
const auto f_identifier = [](auto & ctx)
{
  std::cout << "f_identifier " << _attr(ctx) << '\n';
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
  std::cout << "f_compare ";
  const auto & [left, op, right] = _attr(ctx);
  // type_is<decltype(_attr(ctx))>();
  if (std::holds_alternative<std::string>(right))
    std::cout << "string: " << left << ' ' << op << ' ' << std::get<std::string>(right) << '\n';
  else if (std::holds_alternative<std::vector<unsigned int>>(right))
  {
    std::cout << "version: " << left << ' ';
    for (const auto num : std::get<std::vector<unsigned int>>(right))
      std::cout << num << '.';
    std::cout << '\n';
  }
  else
    std::cout << "WHAT?!\n";

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
  }
}

const auto f_conjunction = [](auto & ctx)
{
  const auto & [s0, ss] = _attr(ctx);

  std::cout << "f_conjunction: ";
  std::cout << pcap(s0);
  for (const auto & [op, sn] : ss)
    std::cout << ' ' << (op ? '|' : '&') << ' ' << pcap(sn);
  std::cout << '\n';
};

const auto f_and = [](auto & ctx)
{
  std::cout << "f_and\n";
  const auto & [left, right] = _attr(ctx);
  const auto states = {
      CapState::FALSE, CapState::MAYBE_FALSE, CapState::MAYBE_TRUE, CapState::TRUE};

  for (const auto state : states)
    if (left == state || right == state)
    {
      _val(ctx) = state;
      return;
    }
};

const auto f_or = [](auto & ctx)
{
  std::cout << "f_or\n";
  const auto & [left, right] = _attr(ctx);
  const auto states = {
      CapState::TRUE, CapState::MAYBE_TRUE, CapState::MAYBE_FALSE, CapState::FALSE};

  for (const auto state : states)
    if (left == state || right == state)
    {
      _val(ctx) = state;
      return;
    }
};

const auto f_negate = [](auto & ctx)
{
  // negate current capability state
  std::cout << "f_negate\n";
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
  }
};

const auto f_pass = [](auto & ctx)
{
  // pass through value
  _val(ctx) = _attr(ctx);
  std::cout << "f_pass\n";
};

// bool_statement
bp::rule<struct bool_statement_tag, CapState> bool_statement = "bool statement";

// expression
bp::rule<struct p_conjunction_tag, CapState> p_conjunction = "conjunction expression";
bp::rule<struct expr_tag, CapState> expr = "boolean expression";

auto const p_conjunction_def = (bool_statement > *(conjunction > bool_statement))[f_conjunction];
// auto const p_conjunction_def = (expr > "&" > expr)[f_and] | (expr > "|" > expr)[f_or];

auto const expr_def = p_conjunction;

auto const bool_statement_def = ('!' > name)[f_not_identifier] |
                                (name >> comparison >> value)[f_compare] | name[f_identifier] |
                                ('(' > expr > ')')[f_pass] | ("!(" > expr > ')')[f_negate];

// auto const expr_def = p_negate[f_negate] | p_conjunction | p_pass[f_pass]; //[f_conjunction];

BOOST_PARSER_DEFINE_RULES(p_conjunction, expr, bool_statement);
}

/*
<name> ::= [a-z] ([a-z] | [0-9])*
<comp> ::= (">" | "<") ("=")? | "!=" | "="
<conj> ::= "&" | "|"
<value> ::= ([a-z] | [0-9] | "_" | "-")+ | [0-9] ("." [0-9]+)*
<bool> ::= <name> | <name> <comp> <value>
<expr> ::= "!(" <expr> ")" | "(" <expr> ")" | <bool> | <expr> <conj> <expr>

https://bnfplayground.pauliankline.com/?bnf=%3Cname%3E%20%3A%3A%3D%20%5Ba-z%5D%20(%5Ba-z%5D%20%7C%20%5B0-9%5D)*%0A%3Ccomp%3E%20%3A%3A%3D%20(%22%3E%22%20%7C%20%22%3C%22)%20(%22%3D%22)%3F%20%7C%20%22!%3D%22%20%7C%20%22%3D%22%0A%3Cconj%3E%20%3A%3A%3D%20%22%26%22%20%7C%20%22%7C%22%0A%3Cvalue%3E%20%3A%3A%3D%20(%5Ba-z%5D%20%7C%20%5B0-9%5D%20%7C%20%22_%22%20%7C%20%22-%22)%2B%20%7C%20%5B0-9%5D%20(%22.%22%20%5B0-9%5D%2B)*%20%0A%3Cbool%3E%20%3A%3A%3D%20%3Cname%3E%20%7C%20%3Cname%3E%20%3Ccomp%3E%20%3Cvalue%3E%0A%3Cexpr%3E%20%3A%3A%3D%20%22!(%22%20%3Cexpr%3E%20%22)%22%20%7C%20%22(%22%20%3Cexpr%3E%20%22)%22%20%7C%20%3Cbool%3E%20%7C%20%3Cexpr%3E%20%3Cconj%3E%20%3Cexpr%3E&name=Real%20Numbers*/
int
main()
{
  // std::string input = "2.445.5";
  std::string input = "petsc & f>23.4.12 & thermochimica";
  // std::string input = "a & b";

  auto const result = bp::parse(input, expr, bp::ws); //, bp::trace::on);
  // type_is<decltype(*result)>();

  // if (result)
  // {
  //   std::cout << "Great! It looks like you entered:\n";
  //   // std::cout << *result << "\n";
  //   for (auto & x : *result)
  //   {
  //     std::cout << x << "\n";
  //   }
  // }
  // else
  // {
  //   std::cout << "Good job!  Please proceed to the recovery annex for cake.\n";
  // }

  return 0;
}
