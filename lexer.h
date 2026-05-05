#pragma once

#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "token.h"

class Lexer {
public:
  explicit Lexer(std::string src) : src_(std::move(src)) {}

  const Token &peek() {
    if (!has_peek_) {
      peek_tok_ = nextTokenInternal();
      has_peek_ = true;
    }
    return peek_tok_;
  }

  Token next() {
    if (has_peek_) {
      has_peek_ = false;
      return peek_tok_;
    }
    return nextTokenInternal();
  }

  bool consume(TokenKind k) {
    if (peek().kind == k) {
      next();
      return true;
    }
    return false;
  }

  [[noreturn]] void errorAt(std::size_t pos, const std::string &msg) const {
    throw std::runtime_error("Lexer error at " + std::to_string(pos) + ": " + msg);
  }

private:
  Token nextTokenInternal() {
    skipWhitespaceAndComments();
    if (i_ >= src_.size()) return makeTok(TokenKind::End, "", 0);

    std::size_t start = i_;
    char c = src_[i_];

    // String literal "..."
    if (c == '"') {
      ++i_; // consume opening quote
      std::string out;
      while (i_ < src_.size()) {
        char ch = src_[i_++];
        if (ch == '"') {
          return makeTok(TokenKind::String, std::move(out), 0, start);
        }
        if (ch == '\\') {
          if (i_ >= src_.size()) errorAt(start, "unterminated escape sequence in string literal");
          char esc = src_[i_++];
          switch (esc) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            default:
              errorAt(i_ - 1, std::string("unknown escape sequence \\") + esc);
          }
          continue;
        }
        if (ch == '\n') errorAt(start, "unterminated string literal");
        out.push_back(ch);
      }
      errorAt(start, "unterminated string literal");
    }

    // Ident / keyword
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      ++i_;
      while (i_ < src_.size()) {
        char d = src_[i_];
        if (std::isalnum(static_cast<unsigned char>(d)) || d == '_') ++i_;
        else break;
      }
      std::string text = src_.substr(start, i_ - start);
      if (text == "int")    return makeTok(TokenKind::KwInt,    text, 0, start);
      if (text == "if")     return makeTok(TokenKind::KwIf,     text, 0, start);
      if (text == "else")   return makeTok(TokenKind::KwElse,   text, 0, start);
      if (text == "while")  return makeTok(TokenKind::KwWhile,  text, 0, start);
      if (text == "for")    return makeTok(TokenKind::KwFor,    text, 0, start);
      if (text == "printf") return makeTok(TokenKind::KwPrintf, text, 0, start);
      return makeTok(TokenKind::Ident, text, 0, start);
    }

    // Number (decimal)
    if (std::isdigit(static_cast<unsigned char>(c))) {
      std::int64_t v = 0;
      while (i_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[i_]))) {
        int digit = src_[i_] - '0';
        if (v > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
          errorAt(start, "integer literal overflow");
        }
        v = v * 10 + digit;
        ++i_;
      }
      return makeTok(TokenKind::Number, src_.substr(start, i_ - start), v, start);
    }

    // Two-char ops
    if (i_ + 1 < src_.size()) {
      char d = src_[i_ + 1];
      if (c == '+' && d == '+') { i_ += 2; return makeTok(TokenKind::PlusPlus, "++", 0, start); }
      if (c == '-' && d == '-') { i_ += 2; return makeTok(TokenKind::MinusMinus, "--", 0, start); }
      if (c == '=' && d == '=') { i_ += 2; return makeTok(TokenKind::Eq, "==", 0, start); }
      if (c == '!' && d == '=') { i_ += 2; return makeTok(TokenKind::Ne, "!=", 0, start); }
      if (c == '<' && d == '=') { i_ += 2; return makeTok(TokenKind::Le, "<=", 0, start); }
      if (c == '>' && d == '=') { i_ += 2; return makeTok(TokenKind::Ge, ">=", 0, start); }
      if (c == '&' && d == '&') { i_ += 2; return makeTok(TokenKind::AndAnd, "&&", 0, start); }
      if (c == '|' && d == '|') { i_ += 2; return makeTok(TokenKind::OrOr, "||", 0, start); }
    }

    // One-char tokens
    ++i_;
    switch (c) {
      case '(': return makeTok(TokenKind::LParen, "(", 0, start);
      case ')': return makeTok(TokenKind::RParen, ")", 0, start);
      case '{': return makeTok(TokenKind::LBrace, "{", 0, start);
      case '}': return makeTok(TokenKind::RBrace, "}", 0, start);
      case ';': return makeTok(TokenKind::Semicolon, ";", 0, start);
      case ',': return makeTok(TokenKind::Comma, ",", 0, start);

      case '+': return makeTok(TokenKind::Plus, "+", 0, start);
      case '-': return makeTok(TokenKind::Minus, "-", 0, start);
      case '*': return makeTok(TokenKind::Star, "*", 0, start);
      case '/': return makeTok(TokenKind::Slash, "/", 0, start);
      case '%': return makeTok(TokenKind::Percent, "%", 0, start);

      case '=': return makeTok(TokenKind::Assign, "=", 0, start);
      case '<': return makeTok(TokenKind::Lt, "<", 0, start);
      case '>': return makeTok(TokenKind::Gt, ">", 0, start);
      case '!': return makeTok(TokenKind::Not, "!", 0, start);
      default: break;
    }

    errorAt(start, std::string("unexpected character '") + c + "'");
    return makeTok(TokenKind::End, "", 0, start);
  }

  void skipWhitespaceAndComments() {
    while (i_ < src_.size()) {
      char c = src_[i_];
      if (std::isspace(static_cast<unsigned char>(c))) {
        ++i_;
        continue;
      }
      // line comment //
      if (c == '/' && i_ + 1 < src_.size() && src_[i_ + 1] == '/') {
        i_ += 2;
        while (i_ < src_.size() && src_[i_] != '\n') ++i_;
        continue;
      }
      // block comment /* ... */
      if (c == '/' && i_ + 1 < src_.size() && src_[i_ + 1] == '*') {
        i_ += 2;
        while (i_ + 1 < src_.size() && !(src_[i_] == '*' && src_[i_ + 1] == '/')) ++i_;
        if (i_ + 1 >= src_.size()) errorAt(i_, "unterminated block comment");
        i_ += 2;
        continue;
      }
      break;
    }
  }

  static Token makeTok(TokenKind k, std::string text, std::int64_t num, std::size_t posOverride = 0) {
    Token t;
    t.kind = k;
    t.text = std::move(text);
    t.number = num;
    t.pos = posOverride;
    return t;
  }

  std::string src_;
  std::size_t i_ = 0;
  bool has_peek_ = false;
  Token peek_tok_{};
};

