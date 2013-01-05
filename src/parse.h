#pragma once

#include "ast.h"
#include <string>
#include <vector>
#include <llvm/LLVMContext.h>

struct Message {
  enum Level {
    ERROR = 0,
    WARNING = 1,
    INFO = 2
  };

  Message(const std::string& msg, Level level)
    : msg_(msg), level_(level) {}
  const std::string& msg() const { return msg_; }
  Level level() const { return level_; }

private:
  std::string msg_;
  Level level_;
};

struct MessagesImpl;
struct Messages {
  Messages();

  void Error(const std::string& msg);
  void Warning(const std::string& msg);
  void Info(const std::string& msg);
  size_t Count(Message::Level level = Message::ERROR) const;

  const std::vector<Message>& messages() const;

  operator bool const () {
    return Count() == 0;
  }

  std::shared_ptr<MessagesImpl> impl_;
};

struct Parser {
  Parser(const std::string& name)
    : module_(name, ctx_) {}

  Messages Parse(const std::string& contents, const std::string& name = "<stdin>");
  Messages ParseFile(const std::string& path);

  llvm::LLVMContext& ctx() { return ctx_; }
  llvm::Module& module() { return module_; }

private:
  llvm::LLVMContext ctx_;
  llvm::Module module_;
};

ast::TopLevel *Parse(llvm::LLVMContext& ctx, const std::string& contents);
