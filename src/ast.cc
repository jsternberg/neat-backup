
#include "ast.h"
using std::shared_ptr;
using namespace llvm;

namespace ast {
  void Program::Codegen(Module& m, shared_ptr<Scope> scope) {
    for (TopLevel *stmt : stmts_) {
      stmt->Codegen(m, scope);
    }
  }

  void Function::Codegen(Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = m.getContext();
    FunctionType *prototype = FunctionType::get(rettype_ ? rettype_ : Type::getVoidTy(ctx), args_, false);
    llvm::Function *f = static_cast<llvm::Function*>(m.getOrInsertFunction(name_, prototype));
    IRBuilder<> irb(BasicBlock::Create(ctx, "entry", f));
    auto innerScope = scope->derive();
    for (Statement *stmt : stmts_) {
      stmt->Codegen(irb, m, innerScope);
    }
  }

  void VariableAssignment::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = irb.getContext();
    llvm::AllocaInst *inst = irb.CreateAlloca(Type::getInt32Ty(ctx));
    irb.CreateStore(expr_->Codegen(irb, m, scope), inst);
    scope->define(name_, inst);
  }

  void If::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    LLVMContext& ctx = irb.getContext();
    BasicBlock *start = irb.GetInsertBlock();
    llvm::Function *f = start->getParent();
    BasicBlock *then = BasicBlock::Create(ctx);
    BasicBlock *else_ = BasicBlock::Create(ctx);
    BasicBlock *end = NULL;

    Value *expr = expr_->Codegen(irb, m, scope);
    Value *cond = irb.CreateICmpNE(expr, ConstantInt::get(expr->getType(), 0));
    irb.CreateCondBr(cond, then, else_);

    f->getBasicBlockList().push_back(then);
    irb.SetInsertPoint(then);

    auto thenScope = scope->derive();
    for (Statement *stmt : then_stmts_) {
      stmt->Codegen(irb, m, thenScope);
    }

    if (then->getTerminator() == NULL) {
      if (!end)
        end = BasicBlock::Create(ctx);
      irb.CreateBr(end);
    }

    f->getBasicBlockList().push_back(else_);
    irb.SetInsertPoint(else_);

    auto elseScope = scope->derive();
    for (Statement *stmt : else_stmts_) {
      stmt->Codegen(irb, m, elseScope);
    }

    if (else_->getTerminator() == NULL) {
      if (!end)
        end = BasicBlock::Create(ctx);
      irb.CreateBr(end);
    }

    if (end) {
      f->getBasicBlockList().push_back(end);
      irb.SetInsertPoint(end);
    }
  }

  void Return::Codegen(IRBuilder<>& irb, Module& m, shared_ptr<Scope> scope) {
    irb.CreateRet(expr_ ? expr_->Codegen(irb, m, scope) : NULL);
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
    return f ? irb.CreateCall(f) : NULL;
  }
}
