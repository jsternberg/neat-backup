
#include "ast.h"
#include "scope.h"
#include <llvm/PassManager.h>
#include <llvm/Transforms/Scalar.h>
using std::shared_ptr;
using namespace llvm;

namespace ast {
  void Program::Codegen(Module& m, shared_ptr<Scope> scope) {
    for (auto& stmt : stmts_) {
      stmt->Codegen(m, scope);
    }
  }

  void Function::Codegen(Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = m.getContext();
    FunctionType *prototype = FunctionType::get(rettype_ ? rettype_ : Type::getVoidTy(ctx), type_args_, false);
    llvm::Function *f = static_cast<llvm::Function*>(m.getOrInsertFunction(name_, prototype));

    IRBuilder<> irb(BasicBlock::Create(ctx, "entry", f));
    auto innerScope = scope->derive();
    llvm::Function::arg_iterator args = f->arg_begin();
    for (auto& name : name_args_) {
      llvm::Value *v = args++;
      if (!name.empty()) {
        v->setName(name);
        llvm::AllocaInst *arg = irb.CreateAlloca(v->getType());
        irb.CreateStore(v, arg);
        innerScope->define(name, arg);
      }
    }

    for (auto& stmt : stmts_) {
      stmt->Codegen(irb, m, innerScope);
    }

    if (irb.GetInsertBlock()->getTerminator() == NULL)
      irb.CreateRetVoid();

    llvm::FunctionPassManager pm(&m);
    pm.add(llvm::createCFGSimplificationPass());
    pm.run(*f);
  }

  void VariableAssignment::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = irb.getContext();
    llvm::AllocaInst *inst = irb.CreateAlloca(Type::getInt32Ty(ctx));
    irb.CreateStore(expr_->Codegen(irb, m, scope), inst);
    scope->define(name_, inst);
  }

  void If::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = irb.getContext();
    llvm::Function *f = irb.GetInsertBlock()->getParent();
    BasicBlock *if_ = BasicBlock::Create(ctx, "", f);
    BasicBlock *then = BasicBlock::Create(ctx);
    BasicBlock *else_ = BasicBlock::Create(ctx);
    BasicBlock *end = BasicBlock::Create(ctx);

    irb.CreateBr(if_);
    irb.SetInsertPoint(if_);

    Value *expr = expr_->Codegen(irb, m, scope);
    Value *cond = irb.CreateICmpNE(expr, ConstantInt::get(expr->getType(), 0));
    irb.CreateCondBr(cond, then, else_);

    f->getBasicBlockList().push_back(then);
    irb.SetInsertPoint(then);

    auto thenScope = scope->derive();
    for (auto& stmt : then_stmts_) {
      stmt->Codegen(irb, m, thenScope);
    }

    if (then->getTerminator() == NULL)
      irb.CreateBr(end);

    f->getBasicBlockList().push_back(else_);
    irb.SetInsertPoint(else_);

    auto elseScope = scope->derive();
    for (auto& stmt : else_stmts_) {
      stmt->Codegen(irb, m, elseScope);
    }

    if (else_->getTerminator() == NULL)
      irb.CreateBr(end);

    f->getBasicBlockList().push_back(end);
    irb.SetInsertPoint(end);
  }

  void While::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = irb.getContext();
    llvm::Function *f = irb.GetInsertBlock()->getParent();
    BasicBlock *start = BasicBlock::Create(ctx, "", f);
    BasicBlock *then = BasicBlock::Create(ctx);
    BasicBlock *end = BasicBlock::Create(ctx);

    irb.CreateBr(start);
    irb.SetInsertPoint(start);

    Value *expr = expr_->Codegen(irb, m, scope);
    Value *cond = irb.CreateICmpNE(expr, ConstantInt::get(expr->getType(), 0));
    irb.CreateCondBr(cond, then, end);

    f->getBasicBlockList().push_back(then);
    irb.SetInsertPoint(then);

    auto innerScope = scope->derive(then, end);
    for (auto& stmt : stmts_) {
      stmt->Codegen(irb, m, innerScope);
    }
    if (irb.GetInsertBlock()->getTerminator() == NULL)
      irb.CreateBr(start);

    f->getBasicBlockList().push_back(end);
    irb.SetInsertPoint(end);
  }

  void Return::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    irb.CreateRet(expr_ ? expr_->Codegen(irb, m, scope) : NULL);
  }

  void Break::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    // TODO: signal an error when break is used incorrectly
    const Scope::Block *block = scope->block();
    if (block)
      irb.CreateBr(block->second);
  }

  void Continue::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    // TODO: signal an error when continue is used incorrectly
    const Scope::Block *block = scope->block();
    if (block)
      irb.CreateBr(block->first);
  }

  Value *IntegerLiteral::Codegen(IRBuilder<>& irb, Module&, shared_ptr<Scope>) {
    return irb.getInt32(value_);
  }

  Value *Variable::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    llvm::Function *f = m.getFunction(ident_);
    if (f)
      return f;

    auto val = lvalue(scope);
    return val ? irb.CreateLoad(val) : NULL;
  }

  AllocaInst *Variable::lvalue(std::shared_ptr<Scope> scope) const {
    return scope->get(ident_);
  }

  Value *UnaryOperation::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    char ch1 = oper_[0];
    char ch2 = oper_.size() > 1 ? oper_[1] : 0;
    switch (ch1) {
      case '+':
        switch (ch2) {
          case 0:
            return expr_->Codegen(irb, m, scope);
          case '+': {
            AllocaInst *ptr = expr_->lvalue(scope);
            if (!ptr) return NULL;
            Value *val = irb.CreateAdd(expr_->Codegen(irb, m, scope), irb.getInt32(1));
            irb.CreateStore(val, ptr);
            return val;
          }
        }
      case '-':
        switch (ch2) {
          case 0:
            return irb.CreateNeg(expr_->Codegen(irb, m, scope));
          case '-': {
            AllocaInst *ptr = expr_->lvalue(scope);
            if (!ptr) return NULL;
            Value *val = irb.CreateSub(expr_->Codegen(irb, m, scope), irb.getInt32(1));
            irb.CreateStore(val, ptr);
            return val;
          }
        }
    }
    return NULL;
  }

  Value *BinaryOperation::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    char ch1 = oper_[0];
    char ch2 = oper_.size() > 1 ? oper_[1] : 0;
    switch (ch1) {
      case '+':
        switch (ch2) {
          case 0: {
            Value *LHS = LHS_->Codegen(irb, m, scope);
            Value *RHS = RHS_->Codegen(irb, m, scope);
            return irb.CreateAdd(LHS, RHS);
          }
          case '=': {
            AllocaInst *ptr = LHS_->lvalue(scope);
            if (!ptr) return NULL;
            Value *val = irb.CreateAdd(LHS_->Codegen(irb, m, scope),
                                       RHS_->Codegen(irb, m, scope));
            irb.CreateStore(val, ptr);
            return val;
          }
        }
      case '-':
        switch (ch2) {
          case 0: {
            Value *LHS = LHS_->Codegen(irb, m, scope);
            Value *RHS = RHS_->Codegen(irb, m, scope);
            return irb.CreateSub(LHS, RHS);
          }
          case '=': {
            AllocaInst *ptr = LHS_->lvalue(scope);
            if (!ptr) return NULL;
            Value *val = irb.CreateSub(LHS_->Codegen(irb, m, scope),
                                       RHS_->Codegen(irb, m, scope));
            irb.CreateStore(val, ptr);
            return val;
          }
        }
      case '=':
        switch (ch2) {
          case 0: {
            AllocaInst *ptr = LHS_->lvalue(scope);
            if (!ptr) return NULL;
            Value *RHS = RHS_->Codegen(irb, m, scope);
            irb.CreateStore(RHS, ptr);
            return RHS;
          }
          case '=': {
            Value *LHS = LHS_->Codegen(irb, m, scope);
            Value *RHS = RHS_->Codegen(irb, m, scope);
            return irb.CreateICmpEQ(LHS, RHS);
          }
        }
    }
    return NULL;
  }

  llvm::Value *CallOperation::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    llvm::Function *f = llvm::dyn_cast<llvm::Function>(expr_->Codegen(irb, m, scope));
    if (!f || f->getArgumentList().size() != args_.size())
      return NULL;

    std::vector<llvm::Value*> args;
    for (auto& expr : args_) {
      args.push_back(expr->Codegen(irb, m, scope));
    }
    return f ? irb.CreateCall(f, args) : NULL;
  }
}
