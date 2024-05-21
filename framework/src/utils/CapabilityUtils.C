//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include <string>
#include <iostream>
#include <deque>
#include <vector>

#include "MooseUtilsStandalone.h"
#include "MooseError.h"

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

struct Token
{
  enum class Type
  {
    Unknown,
    Result,
    VERSION,
    SYMBOL,
    OPERATOR,
    PARENLEFT,
    PARENRIGHT,
  } type;
  std::string value;
  std::size_t begin, len;
  int precedence;
  CapState state;

  void print() { std::cout << value << ' ' << static_cast<int>(type) << ' ' << precedence << '\n'; }
};

bool
isVersion(char d)
{
  return (d >= '0' && d <= '9') || d == '.';
}

bool
isSymbolBegin(char d)
{
  return d >= 'a' && d <= 'z';
}

bool
isSymbolCont(char d)
{
  return isSymbolBegin(d) || (d >= '0' && d <= '9') || d == '_';
}

std::deque<Token>
tokenize(const std::string & expr)
{
  std::deque<Token> tokens;
  const auto * p = expr.c_str();
  for (std::size_t i = 0; i < expr.length(); ++i)
  {
    if (p[i] == '\t' || p[i] == ' ')
      continue;
    else if (isVersion(p[i]))
    {
      const auto b = i;
      while (isVersion(p[i]))
        ++i;
      const auto s = std::string(p + b, p + i);
      tokens.push_back(Token{Token::Type::VERSION, s, b, i - b});
      --i;
    }
    else if (isSymbolBegin(p[i]))
    {
      const auto b = i;
      while (isSymbolCont(p[i]))
        ++i;
      const auto s = std::string(p + b, p + i);
      tokens.push_back(Token{Token::Type::SYMBOL, s, b, i - b});
      --i;
    }
    else
    {
      Token::Type t = Token::Type::Unknown;
      int precedence = -1;
      std::size_t l = 1; // token length
      switch (p[i])
      {
        default:
          break;
        case '(':
          t = Token::Type::PARENLEFT;
          break;
        case ')':
          t = Token::Type::PARENRIGHT;
          break;
        case '>':
          t = Token::Type::OPERATOR;
          precedence = 2;
          if (p[i + 1] == '=')
            l = 2;
          break;
        case '<':
          t = Token::Type::OPERATOR;
          precedence = 2;
          if (p[i + 1] == '=')
            l = 2;
          break;
        case '=':
          t = Token::Type::OPERATOR;
          precedence = 2;
          break;
        case '&':
          t = Token::Type::OPERATOR;
          precedence = 1;
          break;
        case '|':
          t = Token::Type::OPERATOR;
          precedence = 1;
          break;
        case '!':
          t = Token::Type::OPERATOR;
          if (p[i + 1] == '=')
          {
            precedence = 2;
            l = 2;
            break;
          }
          precedence = 3;
          break;
      }
      const auto s = std::string(p + i, p + i + l);
      tokens.push_back(Token{t, s, i, l, precedence});
      i += l - 1;
    }
  }

  return tokens;
}

std::deque<Token>
shuntingYard(const std::string & expr)
{
  std::deque<Token> queue;
  std::vector<Token> stack;
  const std::deque<Token> & tokens = tokenize(expr);

  // While there are tokens to be read:
  for (auto token : tokens)
  {
    // Read a token
    switch (token.type)
    {
      case Token::Type::VERSION:
      case Token::Type::SYMBOL:
        // If the token is a number, then add it to the output queue
        queue.push_back(token);
        break;

      case Token::Type::OPERATOR:
      {
        // If the token is operator, o1, then:
        const auto o1 = token;

        // while there is an operator token,
        while (!stack.empty())
        {
          // o2, at the top of stack, and
          const auto o2 = stack.back();

          // either o1 is left-associative and its precedence is
          // *less than or equal* to that of o2,
          // or o1 if right associative, and has precedence
          // *less than* that of o2,
          if (o1.precedence <= o2.precedence)
          {
            // then pop o2 off the stack,
            stack.pop_back();
            // onto the output queue;
            queue.push_back(o2);

            continue;
          }

          // @@ otherwise, exit.
          break;
        }

        // push o1 onto the stack.
        stack.push_back(o1);
      }
      break;

      case Token::Type::PARENLEFT:
        // If token is left parenthesis, then push it onto the stack
        stack.push_back(token);
        break;

      case Token::Type::PARENRIGHT:
        // If token is right parenthesis:
        {
          bool match = false;

          // Until the token at the top of the stack
          // is a left parenthesis,
          while (!stack.empty() && stack.back().type != Token::Type::PARENLEFT)
          {
            // pop operators off the stack
            // onto the output queue.
            queue.push_back(stack.back());
            stack.pop_back();
            match = true;
          }

          if (!match && stack.empty())
          {
            // If the stack runs out without finding a left parenthesis,
            // then there are mismatched parentheses.
            printf("RightParen error (%s)\n", token.value.c_str());
            return {};
          }

          // Pop the left parenthesis from the stack,
          // but not onto the output queue.
          stack.pop_back();
        }
        break;

      default:
        printf("error (%s)\n", token.value.c_str());
        return {};
    }

    // debugReport(token, queue, stack);
  }

  // When there are no more tokens to read:
  //   While there are still operator tokens in the stack:
  while (!stack.empty())
  {
    // If the operator token on the top of the stack is a parenthesis,
    // then there are mismatched parentheses.
    if (stack.back().type == Token::Type::PARENLEFT)
    {
      printf("Mismatched parentheses error\n");
      return {};
    }

    // Pop the operator onto the output queue.
    queue.push_back(std::move(stack.back()));
    stack.pop_back();
  }

  // debugReport(Token { Token::Type::Unknown, "End" }, queue, stack);

  // Exit.
  return queue;
}

Result
check(const std::string & requirements, const Registry & app_capabilities)
{
  if (requirements == "")
    return {CapabilityUtils::CERTAIN_PASS, "Empty requirements", ""};

  // build postfix notation (program)
  auto program = CapabilitiesParser::shuntingYard(requirements);

  // helper functions
  auto makeTruthy =
      [&app_capabilities, &requirements](Token & arg)
  {
    arg.type = Token::type::Result;

    const auto it = app_capabilities.find(arg.value);
    if (it != app_capabilities.end())
    {
      const auto app_value = it->second.first;
      if (std::holds_alternative<bool>(app_value) && std::get<bool>(app_value) == false)
        arg.state = CapState::FALSE;
      else
        arg.state = CapState::TRUE;
    }
    else
      arg.state = CapState::MAYBE_FALSE;
  }
  // comparator
  auto comp = [](std::string & c, auto a, auto b)
  {
    static const std::unordered_map<std::string, int> comparison = {
        {"<=", 0}, {">=", 1}, {"<", 2}, {">", 3}, {"!=", 4}, {"==", 5}, {"=", 5}};
    const auto it = comparison.find(c);
    switch (it->second)
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
        return a == b;
    }
    throw std::runtime_error("invalid comparison operator");
  };

  // execute program
  std::stack<Token> stack;
  for (const auto & instruction : program)
    switch (instruction.type)
    {
      case Token::Type::VERSION:
      case Token::Type::SYMBOL:
        // If the token is a number, then add it to the output queue
        stack.push(token);
        break;

      case Token::Type::OPERATOR:
      {
        // unary operator
        if (instruction.value == "!")
        {
          if (stack.size() == 0)
            throw std::runtime_error("Missing argument to ! operator.");
          auto arg = stack.pop();
          if (arg.type == Token::Type::SYMBOL || arg.type == Token::Type::Result)
          {
            if (arg.type == Token::Type::SYMBOL)
              makeTruthy(arg);

            // negate current capability state
            switch (arg.state)
            {
              case CapState::FALSE:
                arg.state = CapState::TRUE;
                break;
              case CapState::TRUE:
                arg.state = CapState::FALSE;
                break;
              case CapState::MAYBE_FALSE:
                arg.state = CapState::MAYBE_TRUE;
                break;
              case CapState::MAYBE_TRUE:
                arg.state = CapState::MAYBE_FALSE;
                break;
              default:
                arg.state = CapState::UNKNOWN;
            }

            // push result onto the stack
            stack.push(arg);
          }
          else
          {
            throw std::runtime_error("Invalid argument to ! operator.");
          }
        }

        else
        {
          // binary operator
          if (stack.size < 2)
            std::runtime_error("Insufficient arguments to " + instruction.value + " operator.");
          right = stack.pop();
          left = stack.pop();

          if (instruction.value == "&" || instruction.value == "|")
          {
            // logic operators
            if (left.type != Token::Type::Result)
              makeTruthy(left);
            if (right.type != Token::Type::Result)
              makeTruthy(right);

            // execute operator
            const auto states = instruction.value == "&"
                                    ? std::vector<CapState>{CapState::FALSE,
                                                            CapState::MAYBE_FALSE,
                                                            CapState::UNKNOWN,
                                                            CapState::MAYBE_TRUE,
                                                            CapState::TRUE}
                                    : std::vector<CapState>{CapState::TRUE,
                                                            CapState::MAYBE_TRUE,
                                                            CapState::UNKNOWN,
                                                            CapState::MAYBE_FALSE,
                                                            CapState::FALSE};

            for (const auto state : states)
              if (left.state == state || right.state == state)
              {
                left.state = state;
                break;
              }

            // push back result
            stack.push(left);
          }
          else
          {
            // comparison operators
            if (left.type != Token::Type::SYMBOL)
              std::runtime_error(
                  "Expected capability symbol on the left hand side..."); // but found

            // perform comparison
          }
        }

        default:
          std::cout << "Error in program: ";
          instruction.print();
          break;
      }
    }
}
}
