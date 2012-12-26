
#include "lexer.h"
#include "parse.h"
#include <string>
using namespace std;

namespace {
  int GetTokPrecedence(const Lexer::Token& token) {
    switch (token.type_) {
      case Lexer::Token::OPER:
        switch (token.val_.front()) {
          case '+': case '-':
            return 10;
          case '*': case '/':
            return 20;
          default:
            return 5;
        }
      default:
        return -1;
    }
  }

  struct Parser {
    Parser(llvm::LLVMContext& ctx, const string& contents)
      : ctx_(ctx), lexer_(contents) {}

    ast::TopLevel *Parse();
    ast::TopLevel *TopLevel();
    ast::TopLevel *Function();
    ast::Statement *Statement();
    ast::Statement *If();
    ast::Statement *Return();
    ast::Expression *Primary();
    ast::Expression *Expression();
    ast::Expression *BinOpRHS(int prec, ast::Expression *LHS);

    llvm::Type *TranslateType(llvm::StringRef type) {
      using llvm::Type;
      if (type == "void")
        return Type::getVoidTy(ctx_);
      else if (type == "int")
        return Type::getInt32Ty(ctx_);
      else if (type == "float")
        return Type::getFloatTy(ctx_);
      else if (type == "double")
        return Type::getDoubleTy(ctx_);
      else return NULL;
    }

    llvm::LLVMContext& ctx_;
    Lexer lexer_;
  };

  ast::TopLevel *Parser::Parse() {
    lexer_.ReadToken();
    ast::Program *program = new ast::Program();
    ast::TopLevel *stmt = NULL;
    while ((stmt = TopLevel()) != NULL) {
      program->Append(stmt);
    }
    return lexer_.ExpectToken(Lexer::Token::TEOF) ? program : NULL;
  }

  ast::TopLevel *Parser::TopLevel() {
    ast::TopLevel *stmt = Function();
    if (stmt) return stmt;
    return NULL;
  }

  ast::TopLevel *Parser::Function() {
    if (!lexer_.ExpectToken(Lexer::Token::FN))
      return NULL;

    Lexer::Token t = lexer_.PeekToken();
    ast::Function *f = new ast::Function(t.val_);
    lexer_.ReadToken();

    if (lexer_.ExpectToken(Lexer::Token::PAREN, "(")) {
      bool need_comma = false;
      for (;;) {
        if (need_comma) {
          if (!lexer_.ExpectToken(Lexer::Token::OPER, ","))
            break;
        }

        t = lexer_.PeekToken();
        if (t.type_ != Lexer::Token::IDENT)
          break;
        lexer_.ReadToken();

        llvm::Type *type = TranslateType(t.val_);
        if (!type)
          return NULL;

        f->args_.push_back(type);
        need_comma = true;
      }

      if (!lexer_.ExpectToken(Lexer::Token::PAREN, ")"))
        return NULL;
    }

    if (lexer_.ExpectToken(Lexer::Token::ARROW)) {
      t = lexer_.PeekToken();
      f->rettype_ = TranslateType(t.val_);
      if (!f->rettype_)
        return NULL;
      lexer_.ReadToken();
    }

    if (!lexer_.ExpectToken(Lexer::Token::BRACKET, "{"))
      return NULL;

    if (!lexer_.ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;
    return f;
  }

  ast::Statement *Parser::Statement() {
    ast::Statement *stmt = NULL;
    for (;;) {
      stmt = If();
      if (stmt) return stmt;
      stmt = Return();
      if (stmt) break;
      {
        ast::Expression *expr = Expression();
        if (expr) {
          stmt = new ast::ExpressionStatement(expr);
          break;
        }
      }
      return NULL;
    }

    if (!lexer_.ExpectToken(Lexer::Token::SEMICOLON))
      return NULL;
    return stmt;
  }

  ast::Statement *Parser::If() {
    if (!lexer_.ExpectToken(Lexer::Token::IF))
      return NULL;

    ast::Expression *expr = Expression();
    if (!lexer_.ExpectToken(Lexer::Token::BRACKET, "{"))
      return NULL;

    ast::If *if_ = new ast::If(expr);
    ast::Statement *stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      if_->Append(stmt);
    }

    if (!lexer_.ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;
    return if_;
  }

  ast::Statement *Parser::Return() {
    if (!lexer_.ExpectToken(Lexer::Token::RETURN))
      return NULL;
    return new ast::Return(Expression());
  }

  ast::Expression *Parser::Primary() {
    Lexer::Token t = lexer_.PeekToken();
    switch (t.type_) {
      case Lexer::Token::INT:
        lexer_.ReadToken();
        return new ast::IntegerLiteral(atoi(t.val_.str().c_str()));
      default:
        break;
    }
    return NULL;
  }

  ast::Expression *Parser::Expression() {
    ast::Expression *LHS = Primary();
    if (!LHS) return NULL;
    return BinOpRHS(0, LHS);
  }

  ast::Expression *Parser::BinOpRHS(int prec, ast::Expression *LHS) {
    for (;;) {
      Lexer::Token t = lexer_.PeekToken();
      int tok_prec = GetTokPrecedence(t);
      if (tok_prec < prec)
        return LHS;

      llvm::StringRef binop = t.val_;
      lexer_.ReadToken();

      ast::Expression *RHS = Primary();
      if (!RHS) return NULL;

      t = lexer_.PeekToken();
      int next_prec = GetTokPrecedence(t);
      if (tok_prec < next_prec) {
        RHS = BinOpRHS(tok_prec, RHS);
        if (!RHS) return NULL;
      }

      LHS = new ast::BinaryOperation(binop, LHS, RHS);
    }
  }
}

ast::TopLevel *Parse(llvm::LLVMContext& ctx, const string& contents) {
  Parser parser(ctx, contents);
  return parser.Parse();
}
