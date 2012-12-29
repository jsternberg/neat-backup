
#include "ast.h"
#include "lexer.h"
#include "parse.h"
#include "scope.h"
#include "util.h"
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

  string contents = ReadFile(argv[1]);

  llvm::LLVMContext ctx;
  llvm::Module module(argv[1], ctx);
  std::shared_ptr<Scope> scope = std::shared_ptr<Scope>(new Scope);

  TopLevel *ast = Parse(ctx, contents);
  ast->Codegen(module, scope);

  llvm::raw_fd_ostream fd(fileno(stdout), false);
  module.print(fd, NULL);
  return 0;
}
