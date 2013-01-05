#pragma once

#include "scope.h"
#include <llvm/Module.h>
#include <llvm/Value.h>
#include <llvm/Support/IRBuilder.h>
#include <string>
#include <string.h>
#include <vector>

namespace ast {
  struct TopLevel {
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>) = 0;
  };

  struct Statement {
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>) = 0;
  };

  struct Expression {
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>) = 0;
    virtual llvm::AllocaInst *lvalue(std::shared_ptr<Scope>) const { return NULL; }
  };

  struct Program : TopLevel {
    std::vector<TopLevel*> stmts_;
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>);
    void Append(TopLevel *stmt) { stmts_.push_back(stmt); }
  };

  struct Function : TopLevel {
    llvm::StringRef name_;
    llvm::Type *rettype_;
    std::vector<llvm::Type*> args_;
    std::vector<Statement*> stmts_;
    Function(llvm::StringRef name) : name_(name), rettype_(NULL) {}
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>);
    void Append(Statement *stmt) { stmts_.push_back(stmt); }
  };

  struct VariableAssignment : Statement {
    llvm::StringRef name_;
    Expression *expr_;
    VariableAssignment(llvm::StringRef name, Expression *expr)
      : name_(name), expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct ExpressionStatement : Statement {
    Expression *expr_;
    ExpressionStatement(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>& irb, llvm::Module& m, std::shared_ptr<Scope> scope) {
      (void) expr_->Codegen(irb, m, scope);
    }
  };

  struct If : Statement {
    Expression *expr_;
    std::vector<Statement*> then_stmts_;
    std::vector<Statement*> else_stmts_;
    If(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
    void AppendThen(Statement *stmt) { then_stmts_.push_back(stmt); }
    void AppendElse(Statement *stmt) { else_stmts_.push_back(stmt); }
  };

  struct Return : Statement {
    Expression *expr_;
    Return(Expression *expr) : expr_(expr) {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct IntegerLiteral : Expression {
    int value_;
    IntegerLiteral(int value) : value_(value) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct Variable : Expression {
    llvm::StringRef ident_;
    Variable(llvm::StringRef ident) : ident_(ident) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
    virtual llvm::AllocaInst *lvalue(std::shared_ptr<Scope> scope) const {
      return scope->get(ident_);
    }
  };

  struct UnaryOperation : Expression {
    llvm::StringRef oper_;
    Expression *expr_;
    UnaryOperation(llvm::StringRef oper, Expression *expr)
      : oper_(oper), expr_(expr) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct BinaryOperation : Expression {
    llvm::StringRef oper_;
    Expression *LHS_, *RHS_;
    BinaryOperation(llvm::StringRef oper, Expression *LHS, Expression *RHS)
      : oper_(oper), LHS_(LHS), RHS_(RHS) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct CallOperation : Expression {
    Expression *expr_;
    CallOperation(Expression *expr)
      : expr_(expr) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };
}
