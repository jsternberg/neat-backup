
#include "lexer.h"
#include <ctype.h>

void Lexer::ReadToken() {
  // skip leading whitespace
  {
    size_t n = 0;
    while (n < contents_.size() && isspace(contents_[n]))
      ++n;
    contents_ = contents_.drop_front(n);
  }
  cur_.clear();

  if (contents_.empty()) {
    cur_.type_ = Token::TEOF;
    return;
  }

  char front = contents_.front();
  switch (front) {
    case ';':
      cur_.type_ = Token::SEMICOLON;
      break;
    case '{': case '}':
      cur_.type_ = Token::BRACKET;
      break;
    case '(': case ')':
      cur_.type_ = Token::PAREN;
      break;
  }

  if (cur_.type_ != Token::UNKNOWN) {
    cur_.val_ = contents_.substr(0, 1);
    contents_ = contents_.drop_front(1);
  } else if (isdigit(front) || (front == '-' && contents_.size() > 1 && isdigit(contents_[1]))) {
    size_t n = 1;
    while (n < contents_.size() && isdigit(contents_[n]))
      ++n;
    cur_.type_ = Token::INT;
    cur_.val_ = contents_.substr(0, n);
    contents_ = contents_.drop_front(n);
  } else if (isalpha(front)) {
    size_t n = 1;
    while (n < contents_.size() && isalnum(contents_[n]))
      ++n;
    cur_.val_ = contents_.substr(0, n);
    if (cur_.val_ == "if") {
      cur_.type_ = Token::IF;
    } else if (cur_.val_ == "fn") {
      cur_.type_ = Token::FN;
    } else if (cur_.val_ == "return") {
      cur_.type_ = Token::RETURN;
    } else {
      cur_.type_ = Token::IDENT;
    }
    contents_ = contents_.drop_front(n);
  } else if (llvm::StringRef("+-*/%=!<>^,").find(front) != llvm::StringRef::npos) {
    // we have an operator
    cur_.type_ = Token::OPER;

    // is this operator two characters or one?
    if (contents_.size() < 2) {
      cur_.val_ = contents_.substr(0, 1);
      contents_ = contents_.drop_front(1);
      return;
    }

    bool consume_two = false;
    if (contents_[1] == '=') {
      consume_two = true;
    } else {
      switch (front) {
        case '<': case '>': case '+':
          consume_two = front == contents_[1];
          break;
        case '-':
          switch (contents_[1]) {
            case '>':
              cur_.type_ = Token::ARROW;
            case '-':
              consume_two = true;
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
    }

    cur_.val_ = contents_.substr(0, consume_two ? 2 : 1);
    contents_ = contents_.drop_front(consume_two ? 2 : 1);
  } else {
    // take a single character and label it as "unknown"
    cur_.type_ = Token::UNKNOWN;
    cur_.val_ = contents_.substr(0, 1);
    contents_ = contents_.drop_front(1);
  }
}
