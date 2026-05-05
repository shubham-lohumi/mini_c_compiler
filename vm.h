#pragma once

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

enum class OpCode : std::uint8_t {  
  PUSH_CONST,
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
  std::int64_t arg = 0; // used by PUSH_CONST, LOAD/STORE addr, JMP targets
};

class VM {
public:
  explicit VM(std::vector<Instr> code, std::size_t numSlots, std::vector<std::string> strings = {})
      : code_(std::move(code)), slots_(numSlots, 0), strings_(std::move(strings)) {}

  void run() {
    std::ofstream trace("instr.txt", std::ios::out | std::ios::trunc);
    if (!trace) throw std::runtime_error("failed to open instr.txt for writing");

    std::size_t ip = 0;
    std::vector<std::int64_t> st;
    st.reserve(256);

    auto pop = [&]() -> std::int64_t {
      if (st.empty()) throw std::runtime_error("VM stack underflow");
      std::int64_t v = st.back();
      st.pop_back();
      return v;
    };
    auto push = [&](std::int64_t v) { st.push_back(v); };
    auto truthy = [&](std::int64_t v) { return v != 0; };

    while (ip < code_.size()) {
      const std::size_t curIp = ip;
      const Instr ins = code_[ip++];

      // Trace before execution.
      trace << "ip=" << curIp << " op=" << opName(ins.op) << " arg=" << ins.arg << " st=[";
      for (std::size_t i = 0; i < st.size(); i++) {
        if (i) trace << ",";
        trace << st[i];
      }
      trace << "]\n";

      switch (ins.op) {
        case OpCode::PUSH_CONST: push(ins.arg); break;
        case OpCode::LOAD: {
          checkSlot(ins.arg);
          push(slots_[static_cast<std::size_t>(ins.arg)]);
          break;
        }
        case OpCode::STORE: {
          checkSlot(ins.arg);
          std::int64_t v = pop();
          slots_[static_cast<std::size_t>(ins.arg)] = v;
          break;
        }
        case OpCode::POP: {
          (void)pop();
          break;
        }
        case OpCode::DUP: {
          if (st.empty()) throw std::runtime_error("VM stack underflow on DUP");
          push(st.back());
          break;
        }
        case OpCode::ADD: { auto b = pop(); auto a = pop(); push(a + b); break; }
        case OpCode::SUB: { auto b = pop(); auto a = pop(); push(a - b); break; }
        case OpCode::MUL: { auto b = pop(); auto a = pop(); push(a * b); break; }
        case OpCode::DIV: { auto b = pop(); auto a = pop(); if (b == 0) throw std::runtime_error("division by zero"); push(a / b); break; }
        case OpCode::MOD: { auto b = pop(); auto a = pop(); if (b == 0) throw std::runtime_error("modulo by zero"); push(a % b); break; }
        case OpCode::NEG: { auto a = pop(); push(-a); break; }
        case OpCode::NOT: { auto a = pop(); push(truthy(a) ? 0 : 1); break; }

        case OpCode::CMP_LT: { auto b = pop(); auto a = pop(); push(a < b ? 1 : 0); break; }
        case OpCode::CMP_LE: { auto b = pop(); auto a = pop(); push(a <= b ? 1 : 0); break; }
        case OpCode::CMP_GT: { auto b = pop(); auto a = pop(); push(a > b ? 1 : 0); break; }
        case OpCode::CMP_GE: { auto b = pop(); auto a = pop(); push(a >= b ? 1 : 0); break; }
        case OpCode::CMP_EQ: { auto b = pop(); auto a = pop(); push(a == b ? 1 : 0); break; }
        case OpCode::CMP_NE: { auto b = pop(); auto a = pop(); push(a != b ? 1 : 0); break; }

        case OpCode::AND: { auto b = pop(); auto a = pop(); push((truthy(a) && truthy(b)) ? 1 : 0); break; }
        case OpCode::OR:  { auto b = pop(); auto a = pop(); push((truthy(a) || truthy(b)) ? 1 : 0); break; }

        case OpCode::JMP: {
          ip = static_cast<std::size_t>(ins.arg);
          break;
        }
        case OpCode::JMP_IF_FALSE: {
          std::int64_t c = pop();
          if (!truthy(c)) ip = static_cast<std::size_t>(ins.arg);
          break;
        }
        case OpCode::PRINT: {
          std::int64_t v = pop();
          std::cout << v << "\n";
          break;
        }
        case OpCode::PRINT_STR: {
          if (ins.arg < 0 || static_cast<std::size_t>(ins.arg) >= strings_.size()) {
            throw std::runtime_error("invalid string constant index: " + std::to_string(ins.arg));
          }
          std::cout << strings_[static_cast<std::size_t>(ins.arg)] << "\n";
          break;
        }
        case OpCode::HALT:
          return;
      }
    }
  }

private:
  static const char *opName(OpCode op) {
    switch (op) {
      case OpCode::PUSH_CONST: return "PUSH_CONST";
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
  std::vector<std::int64_t> slots_;
  std::vector<std::string> strings_;
};

