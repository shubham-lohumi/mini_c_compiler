#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "ast.h"
#include "lexer.h"
#include "token.h"

class Parser {
public:
  explicit Parser(Lexer lex) : lex_(std::move(lex)) {}

  Program parseProgram() {
    // Require a C-like entry point: `int main() { ... }`
    //
    // This compiler's IR/VM only supports executing a single sequence of statements,
    // so we treat the body of `main` as the whole program.
    if (lex_.peek().kind != TokenKind::KwInt) {
      const auto &t = lex_.peek();
      throw std::runtime_error(
          "Parse error at line " + std::to_string(t.pos) + " near '" + t.text +
          "': main() function missing. Expected: `int main() { ... }`");
    }
    expect(TokenKind::KwInt, "'int'");

    Token fnName = expect(TokenKind::Ident, "function name");
    if (fnName.text != "main") {
      throw std::runtime_error(
          "Parse error at line " + std::to_string(fnName.pos) +
          ": main() function missing. Expected function name `main`, got '" + fnName.text + "'");
    }

    expect(TokenKind::LParen, "'(' after main");
    if (lex_.peek().kind != TokenKind::RParen) {
      errorHere("`main` must not take parameters in this mini compiler");
    }
    expect(TokenKind::RParen, "')' after main(");

    StmtPtr body = parseBlock();
    auto *blk = dynamic_cast<BlockStmt *>(body.get());
    if (!blk) errorHere("internal error: main body should be a block");

    Program p;
    p.stmts = std::move(blk->stmts);

    if (lex_.peek().kind != TokenKind::End) {
      errorHere("only a single `int main() { ... }` function is allowed at top-level");
    }
    return p;
  }

private:
  [[noreturn]] void errorHere(const std::string &msg) {
    const auto &t = lex_.peek();
    throw std::runtime_error("Parse error at line " + std::to_string(t.pos) + " near '" + t.text + "': " + msg);
  }

  Token expect(TokenKind k, const std::string &what) {
    Token t = lex_.next();
    if (t.kind != k) {
      throw std::runtime_error("Parse error at line " + std::to_string(t.pos) + ": expected " + what + ", got '" + t.text + "'");
    }
    return t;
  }

  StmtPtr parseStmt() {
    switch (lex_.peek().kind) {
      case TokenKind::LBrace:   return parseBlock();
      case TokenKind::KwInt:
      case TokenKind::KwFloat:  return parseDeclStmt();
      case TokenKind::KwPrintf: return parsePrintfStmt();
      case TokenKind::KwIf:     return parseIfStmt();
      case TokenKind::KwWhile:  return parseWhileStmt();
      case TokenKind::KwFor:    return parseForStmt();
      default:                  return parseExprStmt();
    }
  }

  StmtPtr parseBlock() {
    std::size_t pos = lex_.peek().pos;
    expect(TokenKind::LBrace, "'{'");
    std::vector<StmtPtr> ss;
    while (lex_.peek().kind != TokenKind::RBrace) {
      if (lex_.peek().kind == TokenKind::End) errorHere("unexpected end of input in block");
      ss.push_back(parseStmt());
    }
    expect(TokenKind::RBrace, "'}'");
    return std::make_unique<BlockStmt>(pos, std::move(ss));
  }

  StmtPtr parseDeclStmt() {
    std::size_t pos = lex_.peek().pos;
    TypeKind t = parseType();
    Token id = expect(TokenKind::Ident, "identifier");
    ExprPtr init;
    if (lex_.consume(TokenKind::Assign)) {
      init = parseExpr();
    }
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<DeclStmt>(pos, t, std::move(id.text), std::move(init));
  }

  // For init in for( init ; ...): allow "int x = 1" (no trailing ;)
  StmtPtr parseForInit() {
    if (lex_.peek().kind == TokenKind::KwInt || lex_.peek().kind == TokenKind::KwFloat) {
      std::size_t pos = lex_.peek().pos;
      TypeKind t = parseType();
      Token id = expect(TokenKind::Ident, "identifier");
      ExprPtr init;
      if (lex_.consume(TokenKind::Assign)) init = parseExpr();
      return std::make_unique<DeclStmt>(pos, t, std::move(id.text), std::move(init));
    }
    // expression or empty
    if (lex_.peek().kind == TokenKind::Semicolon) {
      return nullptr;
    }
    ExprPtr e = parseExpr();
    return std::make_unique<ExprStmt>(e->pos, std::move(e));
  }

  StmtPtr parsePrintfStmt() {
    std::size_t pos = lex_.peek().pos;
    expect(TokenKind::KwPrintf, "'printf'");
    expect(TokenKind::LParen, "'('");
    if (lex_.peek().kind == TokenKind::String) {
      Token s = lex_.next();
      expect(TokenKind::RParen, "')'");
      expect(TokenKind::Semicolon, "';'");
      return std::make_unique<PrintfStrStmt>(pos, std::move(s.text));
    }
    ExprPtr e = parseExpr();
    expect(TokenKind::RParen, "')'");
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<PrintfStmt>(pos, std::move(e));
  }

  StmtPtr parseIfStmt() {
    std::size_t pos = lex_.peek().pos;
    expect(TokenKind::KwIf, "'if'");
    expect(TokenKind::LParen, "'('");
    ExprPtr cond = parseExpr();
    expect(TokenKind::RParen, "')'");
    StmtPtr thenS = parseStmt();
    StmtPtr elseS;
    if (lex_.consume(TokenKind::KwElse)) {
      elseS = parseStmt();
    }
    return std::make_unique<IfStmt>(pos, std::move(cond), std::move(thenS), std::move(elseS));
  }

  StmtPtr parseWhileStmt() {
    std::size_t pos = lex_.peek().pos;
    expect(TokenKind::KwWhile, "'while'");
    expect(TokenKind::LParen, "'('");
    ExprPtr cond = parseExpr();
    expect(TokenKind::RParen, "')'");
    StmtPtr body = parseStmt();
    return std::make_unique<WhileStmt>(pos, std::move(cond), std::move(body));
  }

  StmtPtr parseForStmt() {
    std::size_t pos = lex_.peek().pos;
    expect(TokenKind::KwFor, "'for'");
    expect(TokenKind::LParen, "'('");
    StmtPtr init = parseForInit();
    expect(TokenKind::Semicolon, "';'");
    ExprPtr cond;
    if (lex_.peek().kind != TokenKind::Semicolon) cond = parseExpr();
    expect(TokenKind::Semicolon, "';'");
    ExprPtr post;
    if (lex_.peek().kind != TokenKind::RParen) post = parseExpr();
    expect(TokenKind::RParen, "')'");
    StmtPtr body = parseStmt();
    return std::make_unique<ForStmt>(pos, std::move(init), std::move(cond), std::move(post), std::move(body));
  }

  StmtPtr parseExprStmt() {
    ExprPtr e = parseExpr();
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<ExprStmt>(e->pos, std::move(e));
  }

  // Expression grammar (lowest -> highest):
  // assignment (=) right-assoc
  // logical_or (||)
  // logical_and (&&)
  // equality (== !=)
  // relational (< <= > >=)
  // additive (+ -)
  // multiplicative (* / %)
  // unary (! - ++ --)
  // postfix (x++ x--)
  // primary (number, ident, (expr))

  ExprPtr parseExpr() { return parseAssign(); }

  ExprPtr parseAssign() {
    auto lhs = parseLogicalOr();
    if (lex_.peek().kind == TokenKind::Assign) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      if (dynamic_cast<VarExpr*>(lhs.get()) == nullptr) errorHere("left side of assignment must be a variable");
      auto rhs = parseAssign(); // right associative
      return std::make_unique<BinaryExpr>(pos, BinOp::Assign, std::move(lhs), std::move(rhs));
    }
    return lhs;
  }

  ExprPtr parseLogicalOr() {
    auto e = parseLogicalAnd();
    while (lex_.peek().kind == TokenKind::OrOr) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto r = parseLogicalAnd();
      e = std::make_unique<BinaryExpr>(pos, BinOp::Or, std::move(e), std::move(r));
    }
    return e;
  }

  ExprPtr parseLogicalAnd() {
    auto e = parseEquality();
    while (lex_.peek().kind == TokenKind::AndAnd) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto r = parseEquality();
      e = std::make_unique<BinaryExpr>(pos, BinOp::And, std::move(e), std::move(r));
    }
    return e;
  }

  ExprPtr parseEquality() {
    auto e = parseRelational();
    while (true) {
      if (lex_.peek().kind == TokenKind::Eq) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseRelational();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Eq, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Ne) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseRelational();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Ne, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseRelational() {
    auto e = parseAdditive();
    while (true) {
      if (lex_.peek().kind == TokenKind::Lt) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Lt, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Le) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Le, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Gt) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Gt, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Ge) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Ge, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseAdditive() {
    auto e = parseMultiplicative();
    while (true) {
      if (lex_.peek().kind == TokenKind::Plus) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseMultiplicative();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Add, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Minus) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseMultiplicative();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Sub, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseMultiplicative() {
    auto e = parseUnary();
    while (true) {
      if (lex_.peek().kind == TokenKind::Star) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Mul, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Slash) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Div, std::move(e), std::move(r));
      } else if (lex_.peek().kind == TokenKind::Percent) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(pos, BinOp::Mod, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseUnary() {
    if (lex_.peek().kind == TokenKind::Not) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(pos, UnOp::Not, std::move(e));
    }
    if (lex_.peek().kind == TokenKind::Minus) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(pos, UnOp::Neg, std::move(e));
    }
    if (lex_.peek().kind == TokenKind::PlusPlus) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(pos, UnOp::PreInc, std::move(e));
    }
    if (lex_.peek().kind == TokenKind::MinusMinus) {
      std::size_t pos = lex_.peek().pos;
      lex_.next();
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(pos, UnOp::PreDec, std::move(e));
    }
    return parsePostfix();
  }

  ExprPtr parsePostfix() {
    auto e = parsePrimary();
    while (true) {
      if (lex_.peek().kind == TokenKind::PlusPlus) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        e = std::make_unique<PostfixExpr>(pos, PostOp::PostInc, std::move(e));
      } else if (lex_.peek().kind == TokenKind::MinusMinus) {
        std::size_t pos = lex_.peek().pos;
        lex_.next();
        e = std::make_unique<PostfixExpr>(pos, PostOp::PostDec, std::move(e));
      } else break;
    }
    return e;
  }

  ExprPtr parsePrimary() {
    const Token &t = lex_.peek();
    if (t.kind == TokenKind::Number) {
      Token n = lex_.next();
      // If the lexer text contains '.', treat as float.
      if (n.text.find('.') != std::string::npos) {
        try {
          double v = std::stod(n.text);
          auto nn = NumberExpr::makeFloat(n.pos, v);
          return std::make_unique<NumberExpr>(std::move(nn));
        } catch (...) {
          throw std::runtime_error("Parse error at line " + std::to_string(n.pos) + ": invalid float literal '" + n.text + "'");
        }
      }
      auto nn = NumberExpr::makeInt(n.pos, n.number);
      return std::make_unique<NumberExpr>(std::move(nn));
    }
    if (t.kind == TokenKind::Ident) {
      Token id = lex_.next();
      return std::make_unique<VarExpr>(id.pos, std::move(id.text));
    }
    if (lex_.consume(TokenKind::LParen)) {
      auto e = parseExpr();
      expect(TokenKind::RParen, "')'");
      return e;
    }
    errorHere("expected primary expression");
    auto nn = NumberExpr::makeInt(t.pos, 0);
    return std::make_unique<NumberExpr>(std::move(nn));
  }

  TypeKind parseType() {
    if (lex_.consume(TokenKind::KwInt)) return TypeKind::Int;
    if (lex_.consume(TokenKind::KwFloat)) return TypeKind::Float;
    errorHere("expected type specifier (int/float)");
    return TypeKind::Int;
  }

  Lexer lex_;
};

