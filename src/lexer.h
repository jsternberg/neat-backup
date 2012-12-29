#pragma once

#include <llvm/ADT/StringRef.h>
#include <vector>

struct Lexer {
  Lexer(llvm::StringRef contents) : contents_(contents) {}

  struct Token {
    enum Type {
      IDENT,
      OPER,
      INT,
      FLOAT,
      BRACKET,
      SEMICOLON,
      IF,
      ELSE,
      FN,
      VAR,
      RETURN,
      ARROW,
      PAREN,
      UNKNOWN,
      TEOF
    };

    void clear() {
      val_ = llvm::StringRef();
      type_ = UNKNOWN;
    }

    llvm::StringRef val_;
    Type type_;
  };

  void drop_front(size_t n) {
    contents_ = contents_.drop_front(n);
  }

  void drop_until(const char *new_) {
    drop_front(new_-contents_.data());
  }

  void get_token(const char *new_, Token::Type type) {
    cur_.type_ = type;
    size_t n = new_-contents_.data();
    cur_.val_ = contents_.substr(0, n);
    drop_front(n);
  }

  void SkipWhitespace();
  void ReadToken();
  Token PeekToken() const { return cur_; }

  Token GetToken() {
    Token t = PeekToken();
    ReadToken();
    return t;
  }

  bool ExpectToken(Token::Type type, llvm::StringRef val) {
    if (val != cur_.val_)
      return false;
    return ExpectToken(type);
  }

  bool ExpectToken(Token::Type type) {
    bool retval = cur_.type_ == type;
    if (retval)
      ReadToken();
    return retval;
  }

  void Save() {
    stack_.push_back(contents_);
  }

  void Load() {
    contents_ = stack_.back();
    stack_.pop_back();
  }

  void Drop() {
    stack_.pop_back();
  }

  llvm::StringRef contents_;
  std::vector<llvm::StringRef> stack_;
  Token cur_;
};
