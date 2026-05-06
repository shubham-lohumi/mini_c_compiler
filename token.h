#pragma once

#include <cstdint>
#include <string>

enum class TokenKind {
  End,
  Ident,
  Number,
  String,

  // Keywords
  KwInt,
  KwFloat,
  KwIf,
  KwElse,
  KwWhile,
  KwFor,
  KwPrintf,

  // Punct / operators
  LParen,
  RParen,
  LBrace,
  RBrace,
  Semicolon,
  Comma,

  Plus,
  Minus,
  Star,
  Slash,
  Percent,

  Assign,     // =
  Eq,         // ==
  Ne,         // !=
  Lt,         // <
  Le,         // <=
  Gt,         // >
  Ge,         // >=

  AndAnd,     // &&
  OrOr,       // ||
  Not,        // !

  PlusPlus,   // ++
  MinusMinus  // --
};

struct Token {
  TokenKind kind{};
  std::string text;
  std::int64_t number = 0;
  std::size_t pos = 0;
};

