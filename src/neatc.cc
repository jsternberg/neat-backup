
#include "ast.h"
#include "lexer.h"
#include "parse.h"
#include "scope.h"
#include <llvm/LLVMContext.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <stdio.h>
using namespace std;
using namespace ast;

int main(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s <file>\n", argv[0]);
    return 1;
  }

  Parser parser(argv[1]);
  Messages errs = parser.ParseFile(argv[1]);
  for (auto& msg : errs.messages()) {
    fprintf(stderr, "%s\n", msg.msg().c_str());
  }

  if (!errs) {
    return 1;
  }

  llvm::raw_fd_ostream fd(fileno(stdout), false);
  parser.module().print(fd, NULL);
  return 0;
}
