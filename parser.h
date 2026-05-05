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
          "Parse error at " + std::to_string(t.pos) + " near '" + t.text +
          "': main() function missing. Expected: `int main() { ... }`");
    }
    expect(TokenKind::KwInt, "'int'");

    Token fnName = expect(TokenKind::Ident, "function name");
    if (fnName.text != "main") {
      throw std::runtime_error(
          "Parse error at " + std::to_string(fnName.pos) +
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
    throw std::runtime_error("Parse error at " + std::to_string(t.pos) + " near '" + t.text + "': " + msg);
  }

  Token expect(TokenKind k, const std::string &what) {
    Token t = lex_.next();
    if (t.kind != k) {
      throw std::runtime_error("Parse error at " + std::to_string(t.pos) + ": expected " + what + ", got '" + t.text + "'");
    }
    return t;
  }

  StmtPtr parseStmt() {
    switch (lex_.peek().kind) {
      case TokenKind::LBrace:   return parseBlock();
      case TokenKind::KwInt:    return parseDeclStmt();
      case TokenKind::KwPrintf: return parsePrintfStmt();
      case TokenKind::KwIf:     return parseIfStmt();
      case TokenKind::KwWhile:  return parseWhileStmt();
      case TokenKind::KwFor:    return parseForStmt();
      default:                  return parseExprStmt();
    }
  }

  StmtPtr parseBlock() {
    expect(TokenKind::LBrace, "'{'");
    std::vector<StmtPtr> ss;
    while (lex_.peek().kind != TokenKind::RBrace) {
      if (lex_.peek().kind == TokenKind::End) errorHere("unexpected end of input in block");
      ss.push_back(parseStmt());
    }
    expect(TokenKind::RBrace, "'}'");
    return std::make_unique<BlockStmt>(std::move(ss));
  }

  StmtPtr parseDeclStmt() {
    expect(TokenKind::KwInt, "'int'");
    Token id = expect(TokenKind::Ident, "identifier");
    ExprPtr init;
    if (lex_.consume(TokenKind::Assign)) {
      init = parseExpr();
    }
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<DeclStmt>(std::move(id.text), std::move(init));
  }

  // For init in for( init ; ...): allow "int x = 1" (no trailing ;)
  StmtPtr parseForInit() {
    if (lex_.peek().kind == TokenKind::KwInt) {
      expect(TokenKind::KwInt, "'int'");
      Token id = expect(TokenKind::Ident, "identifier");
      ExprPtr init;
      if (lex_.consume(TokenKind::Assign)) init = parseExpr();
      return std::make_unique<DeclStmt>(std::move(id.text), std::move(init));
    }
    // expression or empty
    if (lex_.peek().kind == TokenKind::Semicolon) {
      return nullptr;
    }
    ExprPtr e = parseExpr();
    return std::make_unique<ExprStmt>(std::move(e));
  }

  StmtPtr parsePrintfStmt() {
    expect(TokenKind::KwPrintf, "'printf'");
    expect(TokenKind::LParen, "'('");
    if (lex_.peek().kind == TokenKind::String) {
      Token s = lex_.next();
      expect(TokenKind::RParen, "')'");
      expect(TokenKind::Semicolon, "';'");
      return std::make_unique<PrintfStrStmt>(std::move(s.text));
    }
    ExprPtr e = parseExpr();
    expect(TokenKind::RParen, "')'");
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<PrintfStmt>(std::move(e));
  }

  StmtPtr parseIfStmt() {
    expect(TokenKind::KwIf, "'if'");
    expect(TokenKind::LParen, "'('");
    ExprPtr cond = parseExpr();
    expect(TokenKind::RParen, "')'");
    StmtPtr thenS = parseStmt();
    StmtPtr elseS;
    if (lex_.consume(TokenKind::KwElse)) {
      elseS = parseStmt();
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenS), std::move(elseS));
  }

  StmtPtr parseWhileStmt() {
    expect(TokenKind::KwWhile, "'while'");
    expect(TokenKind::LParen, "'('");
    ExprPtr cond = parseExpr();
    expect(TokenKind::RParen, "')'");
    StmtPtr body = parseStmt();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
  }

  StmtPtr parseForStmt() {
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
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(post), std::move(body));
  }

  StmtPtr parseExprStmt() {
    ExprPtr e = parseExpr();
    expect(TokenKind::Semicolon, "';'");
    return std::make_unique<ExprStmt>(std::move(e));
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
    if (lex_.consume(TokenKind::Assign)) {
      if (dynamic_cast<VarExpr*>(lhs.get()) == nullptr) errorHere("left side of assignment must be a variable");
      auto rhs = parseAssign(); // right associative
      return std::make_unique<BinaryExpr>(BinOp::Assign, std::move(lhs), std::move(rhs));
    }
    return lhs;
  }

  ExprPtr parseLogicalOr() {
    auto e = parseLogicalAnd();
    while (lex_.consume(TokenKind::OrOr)) {
      auto r = parseLogicalAnd();
      e = std::make_unique<BinaryExpr>(BinOp::Or, std::move(e), std::move(r));
    }
    return e;
  }

  ExprPtr parseLogicalAnd() {
    auto e = parseEquality();
    while (lex_.consume(TokenKind::AndAnd)) {
      auto r = parseEquality();
      e = std::make_unique<BinaryExpr>(BinOp::And, std::move(e), std::move(r));
    }
    return e;
  }

  ExprPtr parseEquality() {
    auto e = parseRelational();
    while (true) {
      if (lex_.consume(TokenKind::Eq)) {
        auto r = parseRelational();
        e = std::make_unique<BinaryExpr>(BinOp::Eq, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Ne)) {
        auto r = parseRelational();
        e = std::make_unique<BinaryExpr>(BinOp::Ne, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseRelational() {
    auto e = parseAdditive();
    while (true) {
      if (lex_.consume(TokenKind::Lt)) {
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(BinOp::Lt, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Le)) {
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(BinOp::Le, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Gt)) {
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(BinOp::Gt, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Ge)) {
        auto r = parseAdditive();
        e = std::make_unique<BinaryExpr>(BinOp::Ge, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseAdditive() {
    auto e = parseMultiplicative();
    while (true) {
      if (lex_.consume(TokenKind::Plus)) {
        auto r = parseMultiplicative();
        e = std::make_unique<BinaryExpr>(BinOp::Add, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Minus)) {
        auto r = parseMultiplicative();
        e = std::make_unique<BinaryExpr>(BinOp::Sub, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseMultiplicative() {
    auto e = parseUnary();
    while (true) {
      if (lex_.consume(TokenKind::Star)) {
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(BinOp::Mul, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Slash)) {
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(BinOp::Div, std::move(e), std::move(r));
      } else if (lex_.consume(TokenKind::Percent)) {
        auto r = parseUnary();
        e = std::make_unique<BinaryExpr>(BinOp::Mod, std::move(e), std::move(r));
      } else break;
    }
    return e;
  }

  ExprPtr parseUnary() {
    if (lex_.consume(TokenKind::Not)) {
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(UnOp::Not, std::move(e));
    }
    if (lex_.consume(TokenKind::Minus)) {
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(UnOp::Neg, std::move(e));
    }
    if (lex_.consume(TokenKind::PlusPlus)) {
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(UnOp::PreInc, std::move(e));
    }
    if (lex_.consume(TokenKind::MinusMinus)) {
      auto e = parseUnary();
      return std::make_unique<UnaryExpr>(UnOp::PreDec, std::move(e));
    }
    return parsePostfix();
  }

  ExprPtr parsePostfix() {
    auto e = parsePrimary();
    while (true) {
      if (lex_.consume(TokenKind::PlusPlus)) {
        e = std::make_unique<PostfixExpr>(PostOp::PostInc, std::move(e));
      } else if (lex_.consume(TokenKind::MinusMinus)) {
        e = std::make_unique<PostfixExpr>(PostOp::PostDec, std::move(e));
      } else break;
    }
    return e;
  }

  ExprPtr parsePrimary() {
    const Token &t = lex_.peek();
    if (t.kind == TokenKind::Number) {
      Token n = lex_.next();
      return std::make_unique<NumberExpr>(n.number);
    }
    if (t.kind == TokenKind::Ident) {
      Token id = lex_.next();
      return std::make_unique<VarExpr>(std::move(id.text));
    }
    if (lex_.consume(TokenKind::LParen)) {
      auto e = parseExpr();
      expect(TokenKind::RParen, "')'");
      return e;
    }
    errorHere("expected primary expression");
    return std::make_unique<NumberExpr>(0);
  }

  Lexer lex_;
};

