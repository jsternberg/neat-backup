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
  void Error(const std::string& msg);
  void Warning(const std::string& msg);
  void Info(const std::string& msg);
  size_t Count(Message::Level level = Message::ERROR) const;

  const std::vector<Message>& messages() const { return msgs_; }

  operator bool const () {
    return Count() == 0;
  }

private:
  std::vector<Message> msgs_;
};

struct Parser {
  Parser(const std::string& name)
    : module_(name, ctx_) {}

  std::unique_ptr<Messages> Parse(const std::string& contents, const std::string& name = "<stdin>");
  std::unique_ptr<Messages> ParseFile(const std::string& path);

  llvm::LLVMContext& ctx() { return ctx_; }
  llvm::Module& module() { return module_; }

private:
  llvm::LLVMContext ctx_;
  llvm::Module module_;
};
