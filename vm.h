#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.h"

enum class OpCode : std::uint8_t {
  PUSH_INT,
  PUSH_FLOAT, // arg is index into float const pool
  I2F,

  LOAD,
  STORE,
  POP,
  DUP,

  ADD,
  SUB,
  MUL,
  DIV,
  MOD,
  NEG,
  NOT,

  CMP_LT,
  CMP_LE,
  CMP_GT,
  CMP_GE,
  CMP_EQ,
  CMP_NE,

  AND, // logical
  OR,  // logical

  JMP,
  JMP_IF_FALSE,

  PRINT,
  PRINT_STR,
  HALT
};

struct Instr {
  OpCode op{};
  std::int64_t arg = 0; // used by PUSH_INT, PUSH_FLOAT idx, LOAD/STORE addr, JMP targets
};

class VM {
public:
  struct Value {
    TypeKind kind = TypeKind::Int;
    std::int64_t i = 0;
    double f = 0.0;
  };

  explicit VM(std::vector<Instr> code,
              std::size_t numSlots,
              std::vector<std::string> strings = {},
              std::vector<double> floatConsts = {})
      : code_(std::move(code)),
        slots_(numSlots),
        strings_(std::move(strings)),
        floatConsts_(std::move(floatConsts)) {}

  void run() {
    std::ofstream trace("instr.txt", std::ios::out | std::ios::trunc);
    if (!trace) throw std::runtime_error("failed to open instr.txt for writing");

    std::size_t ip = 0;
    std::vector<Value> st;
    st.reserve(256);

    auto pop = [&]() -> Value {
      if (st.empty()) throw std::runtime_error("VM stack underflow");
      Value v = st.back();
      st.pop_back();
      return v;
    };
    auto push = [&](Value v) { st.push_back(v); };
    auto truthy = [&](const Value &v) {
      return (v.kind == TypeKind::Float) ? (v.f != 0.0) : (v.i != 0);
    };
    auto asDouble = [&](const Value &v) { return (v.kind == TypeKind::Float) ? v.f : static_cast<double>(v.i); };

    while (ip < code_.size()) {
      const std::size_t curIp = ip;
      const Instr ins = code_[ip++];

      trace << "ip=" << curIp << " op=" << opName(ins.op) << " arg=" << ins.arg << " st=[";
      for (std::size_t i = 0; i < st.size(); i++) {
        if (i) trace << ",";
        trace << asDouble(st[i]);
      }
      trace << "]\n";

      switch (ins.op) {
        case OpCode::PUSH_INT: {
          Value v;
          v.kind = TypeKind::Int;
          v.i = ins.arg;
          v.f = static_cast<double>(v.i);
          push(v);
          break;
        }
        case OpCode::PUSH_FLOAT: {
          if (ins.arg < 0 || static_cast<std::size_t>(ins.arg) >= floatConsts_.size()) {
            throw std::runtime_error("invalid float const index: " + std::to_string(ins.arg));
          }
          Value v;
          v.kind = TypeKind::Float;
          v.f = floatConsts_[static_cast<std::size_t>(ins.arg)];
          v.i = static_cast<std::int64_t>(v.f);
          push(v);
          break;
        }
        case OpCode::I2F: {
          Value a = pop();
          a.kind = TypeKind::Float;
          a.f = static_cast<double>(a.i);
          push(a);
          break;
        }
        case OpCode::LOAD: {
          checkSlot(ins.arg);
          push(slots_[static_cast<std::size_t>(ins.arg)]);
          break;
        }
        case OpCode::STORE: {
          checkSlot(ins.arg);
          slots_[static_cast<std::size_t>(ins.arg)] = pop();
          break;
        }
        case OpCode::POP: (void)pop(); break;
        case OpCode::DUP:
          if (st.empty()) throw std::runtime_error("VM stack underflow on DUP");
          push(st.back());
          break;

        case OpCode::ADD: {
          auto b = pop(); auto a = pop();
          if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) {
            Value r; r.kind = TypeKind::Float; r.f = asDouble(a) + asDouble(b); r.i = static_cast<std::int64_t>(r.f); push(r);
          } else { Value r; r.kind = TypeKind::Int; r.i = a.i + b.i; r.f = static_cast<double>(r.i); push(r); }
          break;
        }
        case OpCode::SUB: {
          auto b = pop(); auto a = pop();
          if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) {
            Value r; r.kind = TypeKind::Float; r.f = asDouble(a) - asDouble(b); r.i = static_cast<std::int64_t>(r.f); push(r);
          } else { Value r; r.kind = TypeKind::Int; r.i = a.i - b.i; r.f = static_cast<double>(r.i); push(r); }
          break;
        }
        case OpCode::MUL: {
          auto b = pop(); auto a = pop();
          if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) {
            Value r; r.kind = TypeKind::Float; r.f = asDouble(a) * asDouble(b); r.i = static_cast<std::int64_t>(r.f); push(r);
          } else { Value r; r.kind = TypeKind::Int; r.i = a.i * b.i; r.f = static_cast<double>(r.i); push(r); }
          break;
        }
        case OpCode::DIV: {
          auto b = pop(); auto a = pop();
          const double db = asDouble(b);
          if (db == 0.0) throw std::runtime_error("division by zero");
          if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) {
            Value r; r.kind = TypeKind::Float; r.f = asDouble(a) / db; r.i = static_cast<std::int64_t>(r.f); push(r);
          } else {
            Value r; r.kind = TypeKind::Int; r.i = a.i / b.i; r.f = static_cast<double>(r.i); push(r);
          }
          break;
        }
        case OpCode::MOD: {
          auto b = pop(); auto a = pop();
          if (a.kind == TypeKind::Float || b.kind == TypeKind::Float) throw std::runtime_error("modulo on float");
          if (b.i == 0) throw std::runtime_error("modulo by zero");
          Value r; r.kind = TypeKind::Int; r.i = a.i % b.i; r.f = static_cast<double>(r.i); push(r);
          break;
        }
        case OpCode::NEG: {
          auto a = pop();
          if (a.kind == TypeKind::Float) a.f = -a.f;
          else a.i = -a.i;
          push(a);
          break;
        }
        case OpCode::NOT: {
          auto a = pop();
          Value r; r.kind = TypeKind::Int; r.i = truthy(a) ? 0 : 1; r.f = static_cast<double>(r.i); push(r);
          break;
        }

        case OpCode::CMP_LT: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)<asDouble(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::CMP_LE: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)<=asDouble(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::CMP_GT: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)>asDouble(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::CMP_GE: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)>=asDouble(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::CMP_EQ: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)==asDouble(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::CMP_NE: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(asDouble(a)!=asDouble(b))?1:0; r.f=r.i; push(r); break; }

        case OpCode::AND: { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(truthy(a)&&truthy(b))?1:0; r.f=r.i; push(r); break; }
        case OpCode::OR:  { auto b = pop(); auto a = pop(); Value r; r.kind=TypeKind::Int; r.i=(truthy(a)||truthy(b))?1:0; r.f=r.i; push(r); break; }

        case OpCode::JMP: ip = static_cast<std::size_t>(ins.arg); break;
        case OpCode::JMP_IF_FALSE: {
          Value c = pop();
          if (!truthy(c)) ip = static_cast<std::size_t>(ins.arg);
          break;
        }
        case OpCode::PRINT: {
          Value v = pop();
          if (v.kind == TypeKind::Float) std::cout << v.f << "\n";
          else std::cout << v.i << "\n";
          break;
        }
        case OpCode::PRINT_STR: {
          if (ins.arg < 0 || static_cast<std::size_t>(ins.arg) >= strings_.size()) {
            throw std::runtime_error("invalid string constant index: " + std::to_string(ins.arg));
          }
          std::cout << strings_[static_cast<std::size_t>(ins.arg)] << "\n";
          break;
        }
        case OpCode::HALT: return;
      }
    }
  }

private:
  static const char *opName(OpCode op) {
    switch (op) {
      case OpCode::PUSH_INT: return "PUSH_INT";
      case OpCode::PUSH_FLOAT: return "PUSH_FLOAT";
      case OpCode::I2F: return "I2F";
      case OpCode::LOAD: return "LOAD";
      case OpCode::STORE: return "STORE";
      case OpCode::POP: return "POP";
      case OpCode::DUP: return "DUP";
      case OpCode::ADD: return "ADD";
      case OpCode::SUB: return "SUB";
      case OpCode::MUL: return "MUL";
      case OpCode::DIV: return "DIV";
      case OpCode::MOD: return "MOD";
      case OpCode::NEG: return "NEG";
      case OpCode::NOT: return "NOT";
      case OpCode::CMP_LT: return "CMP_LT";
      case OpCode::CMP_LE: return "CMP_LE";
      case OpCode::CMP_GT: return "CMP_GT";
      case OpCode::CMP_GE: return "CMP_GE";
      case OpCode::CMP_EQ: return "CMP_EQ";
      case OpCode::CMP_NE: return "CMP_NE";
      case OpCode::AND: return "AND";
      case OpCode::OR: return "OR";
      case OpCode::JMP: return "JMP";
      case OpCode::JMP_IF_FALSE: return "JMP_IF_FALSE";
      case OpCode::PRINT: return "PRINT";
      case OpCode::PRINT_STR: return "PRINT_STR";
      case OpCode::HALT: return "HALT";
    }
    return "UNKNOWN";
  }

  void checkSlot(std::int64_t idx) {
    if (idx < 0 || static_cast<std::size_t>(idx) >= slots_.size()) {
      throw std::runtime_error("invalid slot index: " + std::to_string(idx));
    }
  }

  std::vector<Instr> code_;
  std::vector<Value> slots_;
  std::vector<std::string> strings_;
  std::vector<double> floatConsts_;
};

