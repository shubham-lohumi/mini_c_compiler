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

enum class TypeKind { Int, Float };

struct Expr {
  std::size_t pos = 0;
  explicit Expr(std::size_t p) : pos(p) {}
  virtual ~Expr() {}
};

struct Stmt {
  std::size_t pos = 0;
  explicit Stmt(std::size_t p) : pos(p) {}
  virtual ~Stmt() {}
};

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct NumberExpr : Expr {
  TypeKind kind;
  std::int64_t iValue = 0;
  double fValue = 0.0;
  static NumberExpr makeInt(std::size_t p, std::int64_t v) {
    NumberExpr n(p, TypeKind::Int);
    n.iValue = v;
    n.fValue = static_cast<double>(v);
    return n;
  }
  static NumberExpr makeFloat(std::size_t p, double v) {
    NumberExpr n(p, TypeKind::Float);
    n.fValue = v;
    n.iValue = static_cast<std::int64_t>(v);
    return n;
  }
private:
  NumberExpr(std::size_t p, TypeKind k) : Expr(p), kind(k) {}
};

struct VarExpr : Expr {
  std::string name;
  VarExpr(std::size_t p, std::string n) : Expr(p), name(std::move(n)) {}
};

struct BinaryExpr : Expr {
  BinOp op;
  ExprPtr lhs;
  ExprPtr rhs;
  BinaryExpr(std::size_t p, BinOp o, ExprPtr a, ExprPtr b)
      : Expr(p), op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct UnaryExpr : Expr {
  UnOp op;
  ExprPtr expr;
  UnaryExpr(std::size_t p, UnOp o, ExprPtr e) : Expr(p), op(o), expr(std::move(e)) {}
};

struct PostfixExpr : Expr {
  PostOp op;
  ExprPtr expr;
  PostfixExpr(std::size_t p, PostOp o, ExprPtr e) : Expr(p), op(o), expr(std::move(e)) {}
};

struct BlockStmt : Stmt {
  std::vector<StmtPtr> stmts;
  BlockStmt(std::size_t p, std::vector<StmtPtr> s) : Stmt(p), stmts(std::move(s)) {}
};

struct DeclStmt : Stmt {
  TypeKind type;
  std::string name;
  ExprPtr init; // may be null
  DeclStmt(std::size_t p, TypeKind t, std::string n, ExprPtr i)
      : Stmt(p), type(t), name(std::move(n)), init(std::move(i)) {}
};

struct ExprStmt : Stmt {
  ExprPtr expr;
  ExprStmt(std::size_t p, ExprPtr e) : Stmt(p), expr(std::move(e)) {}
};

struct PrintfStmt : Stmt {
  ExprPtr expr;
  PrintfStmt(std::size_t p, ExprPtr e) : Stmt(p), expr(std::move(e)) {}
};

struct PrintfStrStmt : Stmt {
  std::string value;
  PrintfStrStmt(std::size_t p, std::string v) : Stmt(p), value(std::move(v)) {}
};

struct IfStmt : Stmt {
  ExprPtr cond;
  StmtPtr thenS;
  StmtPtr elseS; // may be null
  IfStmt(std::size_t p, ExprPtr c, StmtPtr t, StmtPtr e)
      : Stmt(p), cond(std::move(c)), thenS(std::move(t)), elseS(std::move(e)) {}
};

struct WhileStmt : Stmt {
  ExprPtr cond;
  StmtPtr body;
  WhileStmt(std::size_t p, ExprPtr c, StmtPtr b) : Stmt(p), cond(std::move(c)), body(std::move(b)) {}
};

struct ForStmt : Stmt {
  StmtPtr init; // may be null
  ExprPtr cond; // may be null
  ExprPtr post; // may be null
  StmtPtr body;
  ForStmt(std::size_t p0, StmtPtr i, ExprPtr c, ExprPtr p, StmtPtr b)
      : Stmt(p0), init(std::move(i)), cond(std::move(c)), post(std::move(p)), body(std::move(b)) {}
};

struct Program {
  std::vector<StmtPtr> stmts;
};

