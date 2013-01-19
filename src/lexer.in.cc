
#include "lexer.h"
#include <ctype.h>
#include <stdio.h>

void Lexer::ReadToken() {
  /*!re2c
  re2c:define:YYCTYPE = "unsigned char";
  re2c:define:YYCURSOR = p;
  re2c:yyfill:enable = 0;

  whitespace = [ \t\n]*;
  ident = [a-zA-Z_][a-zA-Z0-9_]*;
  integer = [0-9]+;
  */

  SkipWhitespace();
  cur_.clear();

  if (contents_.empty()) {
    cur_.type_ = Token::TEOF;
    return;
  }

  const char *p = contents_.data();
  for (;;) {
    /*!re2c
    "if"   { get_token(p, Token::IF); return; }
    "else" { get_token(p, Token::ELSE); return; }
    "while" { get_token(p, Token::WHILE); return; }
    "fn"   { get_token(p, Token::FN); return; }
    "var"  { get_token(p, Token::VAR); return; }
    "return" { get_token(p, Token::RETURN); return; }
    "break" { get_token(p, Token::BREAK); return; }
    "continue" { get_token(p, Token::CONTINUE); return; }
    [{}]   { get_token(p, Token::BRACKET); return; }
    [()]   { get_token(p, Token::PAREN); return; }
    "->"   { get_token(p, Token::ARROW); return; }
    ":"    { get_token(p, Token::COLON); return; }
    ";"    { get_token(p, Token::SEMICOLON); return; }
    "+"[+=]? { get_token(p, Token::OPER); return; }
    "-"[-=]? { get_token(p, Token::OPER); return; }
    "*"[=]?  { get_token(p, Token::OPER); return; }
    "/"[=]?  { get_token(p, Token::OPER); return; }
    "="[=]?  { get_token(p, Token::OPER); return; }
    ident  { get_token(p, Token::IDENT); return; }
    integer { get_token(p, Token::INT); return; }
    [^] { fprintf(stderr, "invalid character: '%c'\n", *(p-1)); continue; }
    */
  }
}

void Lexer::SkipWhitespace() {
  const char *p = contents_.data();
  while (*p) {
    if (isspace(*p)) {
      ++p; continue;
    } else if (*p == '/') {
      if (p[1] == '/') {
        p += 2;
        while (*p && *p != '\n')
          ++p;
        continue;
      } else if (p[1] == '*') {
        p += 2;
        size_t depth = 1;
        while (*p && depth) {
          if (*p == '*' && p[1] == '/') {
            p += 2;
            --depth;
          } else if (*p == '/' && p[1] == '*') {
            p += 2;
            ++depth;
          } else {
            ++p;
          }
        }
        continue;
      }
    }
    break;
  }

  // TODO: need some way to report errors when comments are not terminated
  drop_front(p-contents_.data());
}

Lexer::LineInfo Lexer::GetLineInfo() const {
  const char *ptr = cur_.val_.data();
  const char *p = start_;
  const char *pbeg = p;
  const char *q = contents_.end();
  size_t line = 1;
  while (p != ptr) {
    if (*p == '\n') {
      line += 1;
      pbeg = p + 1;
    }
    p += 1;
  }

  // need to find the end of the line
  const char *pend = p;
  while (pend != q && *pend != '\n')
    pend += 1;

  return { llvm::StringRef(pbeg, pend-pbeg), line, static_cast<size_t>(p-pbeg)+1 };
}
