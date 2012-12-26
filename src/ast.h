#pragma once

#include <llvm/Module.h>
#include <llvm/Value.h>
#include <llvm/Support/IRBuilder.h>
#include <string>
#include <string.h>
#include <vector>

namespace ast {
  struct TopLevel {
    virtual void Codegen(llvm::Module&) = 0;
  };

  struct Statement {
    virtual void Codegen(llvm::IRBuilder<>&) = 0;
  };

  struct Expression {
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&) = 0;
  };

  struct Program : TopLevel {
    std::vector<TopLevel*> stmts_;
    virtual void Codegen(llvm::Module&);
    void Append(TopLevel *stmt) { stmts_.push_back(stmt); }
  };

  struct Function : TopLevel {
    llvm::StringRef name_;
    llvm::Type *rettype_;
    std::vector<llvm::Type*> args_;
    std::vector<Statement*> stmts_;
    Function(llvm::StringRef name) : name_(name), rettype_(NULL) {}
    virtual void Codegen(llvm::Module&);
    void Append(Statement *stmt) { stmts_.push_back(stmt); }
  };

  struct ExpressionStatement : Statement {
    Expression *expr_;
    ExpressionStatement(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>& irb) {
      (void) expr_->Codegen(irb);
    }
  };

  struct If : Statement {
    Expression *expr_;
    std::vector<Statement*> then_stmts_;
    std::vector<Statement*> else_stmts_;
    If(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>&);
    void AppendThen(Statement *stmt) { then_stmts_.push_back(stmt); }
    void AppendElse(Statement *stmt) { else_stmts_.push_back(stmt); }
  };

  struct Return : Statement {
    Expression *expr_;
    Return(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>&);
  };

  struct IntegerLiteral : Expression {
    int value_;
    IntegerLiteral(int value) : value_(value) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&);
  };

  struct BinaryOperation : Expression {
    Expression *LHS_, *RHS_;
    llvm::StringRef oper_;
    BinaryOperation(llvm::StringRef oper, Expression *LHS, Expression *RHS)
      : oper_(oper), LHS_(LHS), RHS_(RHS) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&);
  };
}
