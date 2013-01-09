
#include "scope.h"
#include <llvm/Instructions.h>
using namespace std;

llvm::AllocaInst *Scope::get(llvm::StringRef name) const {
  auto iter = vars_.find(name);
  if (iter != vars_.end())
    return iter->second;
  return parent_ ? parent_->get(name) : NULL;
}

bool Scope::has(llvm::StringRef name) const {
  if (vars_.count(name) > 0)
    return true;
  return parent_ && parent_->has(name);
}

bool Scope::define(llvm::StringRef name, llvm::AllocaInst *var) {
  if (has(name))
    return false;
  vars_[name] = var;
  return true;
}

const Scope::Block *Scope::block(llvm::StringRef name) const {
  if (block_)
    return block_.get();
  return parent_ ? parent_->block(name) : nullptr;
}

shared_ptr<Scope> Scope::derive() const {
  return shared_ptr<Scope>(new Scope(shared_from_this()));
}

shared_ptr<Scope> Scope::derive(llvm::BasicBlock *start, llvm::BasicBlock *end) const {
  return shared_ptr<Scope>(new Scope(shared_from_this(), start, end));
}
