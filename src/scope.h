#pragma once

#include <llvm/ADT/StringRef.h>
#include <llvm/BasicBlock.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace llvm {
  struct AllocaInst;
}

struct Scope : std::enable_shared_from_this<Scope> {
  Scope() {}

  llvm::AllocaInst *get(llvm::StringRef) const;
  bool has(llvm::StringRef) const;
  bool define(llvm::StringRef, llvm::AllocaInst*);

  typedef std::pair<llvm::BasicBlock*, llvm::BasicBlock*> Block;
  const Block *block(llvm::StringRef name = "") const;

  std::shared_ptr<Scope> derive() const;
  std::shared_ptr<Scope> derive(llvm::BasicBlock* start, llvm::BasicBlock* end) const;

private:
  Scope(const std::shared_ptr<const Scope>& parent) : parent_(parent) {}
  Scope(const std::shared_ptr<const Scope>& parent,
        llvm::BasicBlock *start, llvm::BasicBlock *end)
    : parent_(parent), block_(new Block(start, end)) {}

  typedef std::unordered_map<std::string,llvm::AllocaInst*> VariableMap;
  VariableMap vars_;

  std::shared_ptr<const Scope> parent_;
  std::unique_ptr<Block> block_;
};
