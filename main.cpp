#include <exception>
#include <fstream>
#include <iostream>
#include <string>

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "pygen.h"
#include "vm.h"

static std::string readAllStdin() {
  std::string s, line;
  while (std::getline(std::cin, line)) {
    s += line;
    s += "\n";
  }
  return s;
}

int main(int argc, char **) {
  try {
    std::string input;
    if (argc > 1) {
      input = readAllStdin();
    } else {
      input = R"(
int main() {
  int x = 5;
  int y = 10;
  if (x < y) {
    printf("x is smaller");
    printf(x);
  } else {
    printf("y is smaller");
    printf(y);
  }

  for (int i = 0; i < 3; i++) {
    printf(i);
  }
}
)";
    }

    Lexer lex(input);
    Parser parser(std::move(lex));
    Program prog = parser.parseProgram();

    PyGen pg;
    std::string pyCode = pg.generate(prog);
    std::ofstream pyOut("generated.py");
    if (!pyOut) {
      throw std::runtime_error("failed to open generated.py for writing");
    }
    pyOut << pyCode;
    if (!pyOut) {
      throw std::runtime_error("failed to write generated.py");
    }
    CodeGen cg;
    auto out = cg.compile(prog);

    VM vm(std::move(out.code), out.numSlots, std::move(out.strings), std::move(out.floatConsts));
    vm.run();
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}


//g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o mini_cc
//Get-Content test.c | ./mini_cc