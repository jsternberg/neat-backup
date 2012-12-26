
#include "ast.h"
using namespace llvm;

namespace ast {
  void Program::Codegen(Module& m) {
    for (TopLevel *stmt : stmts_) {
      stmt->Codegen(m);
    }
  }

  void Function::Codegen(Module& m) {
    LLVMContext& ctx = m.getContext();
    FunctionType *prototype = FunctionType::get(rettype_ ? rettype_ : Type::getVoidTy(ctx), args_, false);
    llvm::Function *f = static_cast<llvm::Function*>(m.getOrInsertFunction(name_, prototype));
    IRBuilder<> irb(BasicBlock::Create(ctx, "entry", f));
    for (Statement *stmt : stmts_) {
      stmt->Codegen(irb);
    }
  }

  void If::Codegen(IRBuilder<>& irb) {
    LLVMContext& ctx = irb.getContext();
    BasicBlock *start = irb.GetInsertBlock();
    llvm::Function *f = start->getParent();
    BasicBlock *then = BasicBlock::Create(ctx);
    BasicBlock *else_ = BasicBlock::Create(ctx);
    BasicBlock *end = NULL;

    Value *expr = irb.CreateBitCast(expr_->Codegen(irb), Type::getInt8Ty(ctx));
    Value *cond = irb.CreateICmpNE(expr, irb.getInt8(0));
    irb.CreateCondBr(cond, then, else_);

    f->getBasicBlockList().push_back(then);
    irb.SetInsertPoint(then);

    for (Statement *stmt : then_stmts_) {
      stmt->Codegen(irb);
    }
    if (then->getTerminator() == NULL) {
      if (!end)
        end = BasicBlock::Create(ctx);
      irb.CreateBr(end);
    }

    f->getBasicBlockList().push_back(else_);
    irb.SetInsertPoint(else_);

    for (Statement *stmt : else_stmts_) {
      stmt->Codegen(irb);
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

  void Return::Codegen(IRBuilder<>& irb) {
    irb.CreateRet(expr_ ? expr_->Codegen(irb) : NULL);
  }

  Value *IntegerLiteral::Codegen(IRBuilder<>& irb) {
    return irb.getInt32(value_);
  }
}

namespace ast {
  Value *BinaryOperation::Codegen(IRBuilder<>& irb) {
    Value *LHS = LHS_->Codegen(irb);
    Value *RHS = RHS_->Codegen(irb);

    switch (oper_.front()) {
      case '+':
        return irb.CreateAdd(LHS, RHS);
      case '-':
        return irb.CreateSub(LHS, RHS);
      default:
        return NULL;
    }
    return irb.CreateAdd(LHS, RHS);
  }
}
