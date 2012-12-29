#pragma once

#include <llvm/ADT/StringRef.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace llvm {
  struct AllocaInst;
}

struct Scope : std::enable_shared_from_this<Scope> {
  Scope() {}

  llvm::AllocaInst *get(llvm::StringRef) const;
  bool has(llvm::StringRef) const;
  bool define(llvm::StringRef, llvm::AllocaInst*);
  std::shared_ptr<Scope> derive() const;

private:
  Scope(const std::shared_ptr<const Scope>& parent) : parent_(parent) {}

  typedef std::unordered_map<std::string,llvm::AllocaInst*> VariableMap;
  VariableMap vars_;

  std::shared_ptr<const Scope> parent_;
};
