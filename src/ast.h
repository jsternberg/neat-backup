#pragma once

#include <llvm/Module.h>
#include <llvm/Value.h>
#include <llvm/Support/IRBuilder.h>
#include <string>
#include <string.h>
#include <vector>

struct Scope;

namespace ast {
  struct TopLevel {
    virtual ~TopLevel() {}
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>) = 0;
  };

  struct Statement {
    virtual ~Statement() {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>) = 0;
  };

  struct Expression {
    virtual ~Expression() {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>) = 0;
    virtual llvm::AllocaInst *lvalue(std::shared_ptr<Scope>) const { return NULL; }
  };

  struct Program : TopLevel {
    std::vector<std::unique_ptr<TopLevel>> stmts_;
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>);
    void Append(std::unique_ptr<TopLevel> stmt) { stmts_.push_back(std::move(stmt)); }
  };

  struct Function : TopLevel {
    llvm::StringRef name_;
    llvm::Type *rettype_;
    std::vector<llvm::StringRef> name_args_;
    std::vector<llvm::Type*> type_args_;
    std::vector<std::unique_ptr<Statement>> stmts_;
    Function(llvm::StringRef name) : name_(name), rettype_(NULL) {}
    virtual void Codegen(llvm::Module&, std::shared_ptr<Scope>);
    void Append(std::unique_ptr<Statement> stmt) { stmts_.push_back(std::move(stmt)); }
  };

  struct VariableAssignment : Statement {
    llvm::StringRef name_;
    std::unique_ptr<Expression> expr_;
    VariableAssignment(llvm::StringRef name, std::unique_ptr<Expression> expr)
      : name_(name), expr_(std::move(expr)) {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct ExpressionStatement : Statement {
    std::unique_ptr<Expression> expr_;
    ExpressionStatement(std::unique_ptr<Expression> expr)
      : expr_(std::move(expr)) {}
    virtual void Codegen(llvm::IRBuilder<>& irb, llvm::Module& m, std::shared_ptr<Scope> scope) {
      (void) expr_->Codegen(irb, m, scope);
    }
  };

  struct If : Statement {
    std::unique_ptr<Expression> expr_;
    std::vector<std::unique_ptr<Statement>> then_stmts_;
    std::vector<std::unique_ptr<Statement>> else_stmts_;
    If(std::unique_ptr<Expression> expr) : expr_(std::move(expr)) {}
    virtual void Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
    void AppendThen(std::unique_ptr<Statement> stmt) { then_stmts_.push_back(std::move(stmt)); }
    void AppendElse(std::unique_ptr<Statement> stmt) { else_stmts_.push_back(std::move(stmt)); }
  };

  struct Return : Statement {
    std::unique_ptr<Expression> expr_;
    Return(std::unique_ptr<Expression> expr) : expr_(std::move(expr)) {}
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
    virtual llvm::AllocaInst *lvalue(std::shared_ptr<Scope> scope) const;
  };

  struct UnaryOperation : Expression {
    llvm::StringRef oper_;
    std::unique_ptr<Expression> expr_;
    UnaryOperation(llvm::StringRef oper, std::unique_ptr<Expression> expr)
      : oper_(oper), expr_(std::move(expr)) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct BinaryOperation : Expression {
    llvm::StringRef oper_;
    std::unique_ptr<Expression> LHS_, RHS_;
    BinaryOperation(llvm::StringRef oper, std::unique_ptr<Expression> LHS, std::unique_ptr<Expression> RHS)
      : oper_(oper), LHS_(std::move(LHS)), RHS_(std::move(RHS)) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };

  struct CallOperation : Expression {
    std::unique_ptr<Expression> expr_;
    std::vector<std::unique_ptr<Expression>> args_;
    CallOperation(std::unique_ptr<Expression> expr)
      : expr_(std::move(expr)) {}
    virtual llvm::Value *Codegen(llvm::IRBuilder<>&, llvm::Module&, std::shared_ptr<Scope>);
  };
}
