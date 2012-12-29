
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
  const char *p = contents_.data();
  cur_.clear();

  if (contents_.empty()) {
    cur_.type_ = Token::TEOF;
    return;
  }

  for (;;) {
    /*!re2c
    "if"   { get_token(p, Token::IF); return; }
    "else" { get_token(p, Token::ELSE); return; }
    "fn"   { get_token(p, Token::FN); return; }
    "var"  { get_token(p, Token::VAR); return; }
    "return" { get_token(p, Token::RETURN); return; }
    [{}]   { get_token(p, Token::BRACKET); return; }
    [()]   { get_token(p, Token::PAREN); return; }
    "->"   { get_token(p, Token::ARROW); return; }
    ";"    { get_token(p, Token::SEMICOLON); return; }
    "+"[+=] { get_token(p, Token::OPER); return; }
    [+-/*] { get_token(p, Token::OPER); return; }
    ident  { get_token(p, Token::IDENT); return; }
    integer { get_token(p, Token::INT); return; }
    [^] { fprintf(stderr, "invalid character: '%c'\n", *(p-1)); continue; }
    */
  }
}

void Lexer::SkipWhitespace() {
  size_t n = 0;
  while (n < contents_.size() && isspace(contents_[n]))
    ++n;
  drop_front(n);
}
