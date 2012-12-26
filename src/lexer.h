#pragma once

#include <llvm/ADT/StringRef.h>

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
      FN,
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

  void ReadToken();
  Token PeekToken() const { return cur_; }

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

  llvm::StringRef contents_;
  Token cur_;
};