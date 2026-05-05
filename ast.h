#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class BinOp {
  Add, Sub, Mul, Div, Mod,
  Lt, Le, Gt, Ge, Eq, Ne,
  And, Or,
  Assign
};

enum class UnOp { Neg, Not, PreInc, PreDec };
enum class PostOp { PostInc, PostDec };

struct Expr {
  virtual ~Expr() {}
};

struct Stmt {
  virtual ~Stmt() {}
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct NumberExpr : Expr {
  std::int64_t value;
  explicit NumberExpr(std::int64_t v) : value(v) {}
};

struct VarExpr : Expr {
  std::string name;
  explicit VarExpr(std::string n) : name(std::move(n)) {}
};

struct BinaryExpr : Expr {
  BinOp op;
  ExprPtr lhs;
  ExprPtr rhs;
  BinaryExpr(BinOp o, ExprPtr a, ExprPtr b) : op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct UnaryExpr : Expr {
  UnOp op;
  ExprPtr expr;
  UnaryExpr(UnOp o, ExprPtr e) : op(o), expr(std::move(e)) {}
};

struct PostfixExpr : Expr {
  PostOp op;
  ExprPtr expr;
  PostfixExpr(PostOp o, ExprPtr e) : op(o), expr(std::move(e)) {}
};

struct BlockStmt : Stmt {
  std::vector<StmtPtr> stmts;
  explicit BlockStmt(std::vector<StmtPtr> s) : stmts(std::move(s)) {}
};

struct DeclStmt : Stmt {
  std::string name;
  ExprPtr init; // may be null
  DeclStmt(std::string n, ExprPtr i) : name(std::move(n)), init(std::move(i)) {}
};

struct ExprStmt : Stmt {
  ExprPtr expr;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct PrintfStmt : Stmt {
  ExprPtr expr;
  explicit PrintfStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct PrintfStrStmt : Stmt {
  std::string value;
  explicit PrintfStrStmt(std::string v) : value(std::move(v)) {}
};

struct IfStmt : Stmt {
  ExprPtr cond;
  StmtPtr thenS;
  StmtPtr elseS; // may be null
  IfStmt(ExprPtr c, StmtPtr t, StmtPtr e) : cond(std::move(c)), thenS(std::move(t)), elseS(std::move(e)) {}
};

struct WhileStmt : Stmt {
  ExprPtr cond;
  StmtPtr body;
  WhileStmt(ExprPtr c, StmtPtr b) : cond(std::move(c)), body(std::move(b)) {}
};

struct ForStmt : Stmt {
  StmtPtr init; // may be null
  ExprPtr cond; // may be null
  ExprPtr post; // may be null
  StmtPtr body;
  ForStmt(StmtPtr i, ExprPtr c, ExprPtr p, StmtPtr b)
      : init(std::move(i)), cond(std::move(c)), post(std::move(p)), body(std::move(b)) {}
};

struct Program {
  std::vector<StmtPtr> stmts;
};

