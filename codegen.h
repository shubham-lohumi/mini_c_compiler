#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.h"
#include "vm.h"

class CodeGen {
public:
  struct Result {
    std::vector<Instr> code;
    std::size_t numSlots = 0;
    std::vector<std::string> strings;
  };

  Result compile(const Program &p) {
    enterScope();
    for (const auto &s : p.stmts) genStmt(*s);
    exitScope();
    emit(OpCode::HALT);
    return Result{std::move(code_), nextSlot_, std::move(strings_)};
  }

private:
  void emit(OpCode op, std::int64_t arg = 0) { code_.push_back(Instr{op, arg}); }

  std::int64_t internString(const std::string &s) {
    auto it = strIndex_.find(s);
    if (it != strIndex_.end()) return it->second;
    std::int64_t idx = static_cast<std::int64_t>(strings_.size());
    strings_.push_back(s);
    strIndex_.emplace(strings_.back(), idx);
    return idx;
  }

  std::size_t emitPlaceholder(OpCode op) {
    code_.push_back(Instr{op, 0});
    return code_.size() - 1;
  }

  void patch(std::size_t at, std::int64_t arg) {
    if (at >= code_.size()) throw std::runtime_error("patch out of range");
    code_[at].arg = arg;
  }

  void enterScope() { scopes_.push_back({}); }

  void exitScope() {
    if (scopes_.empty()) throw std::runtime_error("scope stack underflow");
    scopes_.pop_back();
  }

  std::int64_t declareVar(const std::string &name) {
    if (scopes_.empty()) throw std::runtime_error("no scope");
    auto &m = scopes_.back();
    if (m.find(name) != m.end()) throw std::runtime_error("redeclaration of variable: " + name);
    std::int64_t slot = static_cast<std::int64_t>(nextSlot_++);
    m[name] = slot;
    return slot;
  }

  std::int64_t lookupVar(const std::string &name) {
    for (std::size_t i = scopes_.size(); i-- > 0;) {
      auto it = scopes_[i].find(name);
      if (it != scopes_[i].end()) return it->second;
    }
    throw std::runtime_error("use of undeclared variable: " + name);
  }

  void genStmt(const Stmt &s) {
    if (const auto *b = dynamic_cast<const BlockStmt*>(&s)) {
      enterScope();
      for (const auto &ss : b->stmts) genStmt(*ss);
      exitScope();
      return;
    }
    if (const auto *d = dynamic_cast<const DeclStmt*>(&s)) {
      std::int64_t slot = declareVar(d->name);
      if (d->init) {
        genExpr(*d->init);
        emit(OpCode::STORE, slot);
      } else {
        emit(OpCode::PUSH_CONST, 0);
        emit(OpCode::STORE, slot);
      }
      return;
    }
    if (const auto *es = dynamic_cast<const ExprStmt*>(&s)) {
      genExpr(*es->expr);
      emit(OpCode::POP);
      return;
    }
    if (const auto *p = dynamic_cast<const PrintfStmt*>(&s)) {
      genExpr(*p->expr);
      emit(OpCode::PRINT);
      return;
    }
    if (const auto *ps = dynamic_cast<const PrintfStrStmt*>(&s)) {
      std::int64_t idx = internString(ps->value);
      emit(OpCode::PRINT_STR, idx);
      return;
    }
    if (const auto *ifs = dynamic_cast<const IfStmt*>(&s)) {
      genExpr(*ifs->cond);
      std::size_t jFalse = emitPlaceholder(OpCode::JMP_IF_FALSE);
      genStmt(*ifs->thenS);
      if (ifs->elseS) {
        std::size_t jEnd = emitPlaceholder(OpCode::JMP);
        patch(jFalse, static_cast<std::int64_t>(code_.size()));
        genStmt(*ifs->elseS);
        patch(jEnd, static_cast<std::int64_t>(code_.size()));
      } else {
        patch(jFalse, static_cast<std::int64_t>(code_.size()));
      }
      return;
    }
    if (const auto *ws = dynamic_cast<const WhileStmt*>(&s)) {
      std::size_t loopStart = code_.size();
      genExpr(*ws->cond);
      std::size_t jOut = emitPlaceholder(OpCode::JMP_IF_FALSE);
      genStmt(*ws->body);
      emit(OpCode::JMP, static_cast<std::int64_t>(loopStart));
      patch(jOut, static_cast<std::int64_t>(code_.size()));
      return;
    }
    if (const auto *fs = dynamic_cast<const ForStmt*>(&s)) {
      enterScope(); // C-like: init decl scoped to the for
      if (fs->init) genStmt(*fs->init);

      std::size_t condPos = code_.size();
      if (fs->cond) genExpr(*fs->cond);
      else emit(OpCode::PUSH_CONST, 1);

      std::size_t jOut = emitPlaceholder(OpCode::JMP_IF_FALSE);
      genStmt(*fs->body);
      if (fs->post) {
        genExpr(*fs->post);
        emit(OpCode::POP);
      }
      emit(OpCode::JMP, static_cast<std::int64_t>(condPos));
      patch(jOut, static_cast<std::int64_t>(code_.size()));
      exitScope();
      return;
    }
    throw std::runtime_error("unknown stmt node");
  }

  void genExpr(const Expr &e) {
    if (const auto *n = dynamic_cast<const NumberExpr*>(&e)) {
      emit(OpCode::PUSH_CONST, n->value);
      return;
    }
    if (const auto *v = dynamic_cast<const VarExpr*>(&e)) {
      std::int64_t slot = lookupVar(v->name);
      emit(OpCode::LOAD, slot);
      return;
    }
    if (const auto *u = dynamic_cast<const UnaryExpr*>(&e)) {
      if (u->op == UnOp::Neg) {
        genExpr(*u->expr);
        emit(OpCode::NEG);
        return;
      }
      if (u->op == UnOp::Not) {
        genExpr(*u->expr);
        emit(OpCode::NOT);
        return;
      }
      if (u->op == UnOp::PreInc || u->op == UnOp::PreDec) {
        const auto *v2 = dynamic_cast<const VarExpr*>(u->expr.get());
        if (!v2) throw std::runtime_error("pre ++/-- requires a variable");
        std::int64_t slot = lookupVar(v2->name);
        emit(OpCode::LOAD, slot);
        emit(OpCode::PUSH_CONST, (u->op == UnOp::PreInc) ? 1 : -1);
        emit(OpCode::ADD);
        emit(OpCode::DUP);         // keep result as expression value
        emit(OpCode::STORE, slot); // store consumes one copy
        return;
      }
    }
    if (const auto *p = dynamic_cast<const PostfixExpr*>(&e)) {
      const auto *v = dynamic_cast<const VarExpr*>(p->expr.get());
      if (!v) throw std::runtime_error("post ++/-- requires a variable");
      std::int64_t slot = lookupVar(v->name);
      emit(OpCode::LOAD, slot); // old
      emit(OpCode::DUP);        // keep old as expression value
      emit(OpCode::PUSH_CONST, (p->op == PostOp::PostInc) ? 1 : -1);
      emit(OpCode::ADD);        // new
      emit(OpCode::STORE, slot);
      return;
    }
    if (const auto *b = dynamic_cast<const BinaryExpr*>(&e)) {
      if (b->op == BinOp::Assign) {
        const auto *lv = dynamic_cast<const VarExpr*>(b->lhs.get());
        if (!lv) throw std::runtime_error("left side of assignment must be a variable");
        std::int64_t slot = lookupVar(lv->name);
        genExpr(*b->rhs);
        emit(OpCode::DUP);        // assignment expression evaluates to assigned value
        emit(OpCode::STORE, slot);
        return;
      }

      // For this mini-compiler, && and || are not short-circuiting.
      genExpr(*b->lhs);
      genExpr(*b->rhs);
      switch (b->op) {
        case BinOp::Add: emit(OpCode::ADD); break;
        case BinOp::Sub: emit(OpCode::SUB); break;
        case BinOp::Mul: emit(OpCode::MUL); break;
        case BinOp::Div: emit(OpCode::DIV); break;
        case BinOp::Mod: emit(OpCode::MOD); break;

        case BinOp::Lt: emit(OpCode::CMP_LT); break;
        case BinOp::Le: emit(OpCode::CMP_LE); break;
        case BinOp::Gt: emit(OpCode::CMP_GT); break;
        case BinOp::Ge: emit(OpCode::CMP_GE); break;
        case BinOp::Eq: emit(OpCode::CMP_EQ); break;
        case BinOp::Ne: emit(OpCode::CMP_NE); break;

        case BinOp::And: emit(OpCode::AND); break;
        case BinOp::Or:  emit(OpCode::OR);  break;
        case BinOp::Assign: break;
      }
      return;
    }
    throw std::runtime_error("unknown expr node");
  }

  std::vector<Instr> code_;
  std::vector<std::unordered_map<std::string, std::int64_t>> scopes_;
  std::size_t nextSlot_ = 0;
  std::vector<std::string> strings_;
  std::unordered_map<std::string, std::int64_t> strIndex_;
};

