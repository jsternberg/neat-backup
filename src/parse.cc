
#include "ast.h"
#include "lexer.h"
#include "parse.h"
#include "scope.h"
#include "util.h"
#include <memory>
#include <string>
#include <vector>
using namespace std;

void Messages::Error(const string& msg) {
  msgs_.push_back(Message(msg, Message::ERROR));
}

void Messages::Warning(const string& msg) {
  msgs_.push_back(Message(msg, Message::WARNING));
}

void Messages::Info(const string& msg) {
  msgs_.push_back(Message(msg, Message::INFO));
}

size_t Messages::Count(Message::Level level) const {
  size_t count = 0;
  for (auto& msg : msgs_) {
    if (msg.level() <= level)
      ++count;
  }
  return count;
}

namespace {
  /** Get precedence value of an operator token.
      A higher number has a higher precedence and will be grouped
      together first. If a number has the same precedence value,
      then the grouping may be from left-to-right or right-to-left.

      If the number returned is even, then the grouping is left-to-right.
      If the number returned is odd, then the grouping is right-to-left.
  */
  int GetTokPrecedence(const Lexer::Token& token) {
    if (token.type_ != Lexer::Token::OPER)
      return -1;

    char ch1 = token.val_[0];
    char ch2 = token.val_.size() > 1 ? token.val_[1] : 0;
    switch (ch1) {
      case '+':
        switch (ch2) {
          case 0:   return 20;
          case '=': return 5;
          default:  return -1;
        }
      case '-':
        switch (ch2) {
          case 0:   return 20;
          case '=': return 5;
          default:  return -1;
        }
      case '*':
        switch (ch2) {
          case 0:   return 40;
          case '=': return 5;
          default:  return -1;
        }
      case '/':
        switch (ch2) {
          case 0:   return 40;
          case '=': return 5;
          default:  return -1;
        }
      case '=':
        switch (ch2) {
          case 0:   return 5;
          case '=': return 10;
          default:  return -1;
        }
      default: return -1;
    }
  }

  struct FileParser {
    FileParser(llvm::LLVMContext& ctx, Messages& errs,
               const string& filename, const string& contents)
      : ctx_(ctx), filename_(filename), lexer_(contents), errs_(errs) {}

    unique_ptr<ast::TopLevel> Parse();
    unique_ptr<ast::TopLevel> TopLevel();
    unique_ptr<ast::TopLevel> Function();
    unique_ptr<ast::Statement> Statement();
    unique_ptr<ast::Statement> Var();
    unique_ptr<ast::Statement> If();
    unique_ptr<ast::Statement> While();
    unique_ptr<ast::Statement> Return();
    unique_ptr<ast::Expression> Primary();
    unique_ptr<ast::Expression> PrimaryRHS(unique_ptr<ast::Expression> LHS);
    unique_ptr<ast::Expression> Expression();
    unique_ptr<ast::Expression> BinOpRHS(int prec, unique_ptr<ast::Expression> LHS);

    bool ExpectToken(Lexer::Token::Type type, llvm::StringRef val);
    bool ExpectToken(Lexer::Token::Type type);

    void Error(const string& msg);

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
    string filename_;
    Lexer lexer_;
    Messages& errs_;
  };

  unique_ptr<ast::TopLevel> FileParser::Parse() {
    lexer_.ReadToken();
    auto program = unique_ptr<ast::Program>(new ast::Program());
    unique_ptr<ast::TopLevel> stmt = NULL;
    while ((stmt = TopLevel()) != NULL) {
      program->Append(move(stmt));
    }
    return lexer_.ExpectToken(Lexer::Token::TEOF) ? move(program) : NULL;
  }

  unique_ptr<ast::TopLevel> FileParser::TopLevel() {
    unique_ptr<ast::TopLevel> stmt = Function();
    if (stmt) return stmt;
    return NULL;
  }

  unique_ptr<ast::TopLevel> FileParser::Function() {
    if (!lexer_.ExpectToken(Lexer::Token::FN))
      return NULL;

    Lexer::Token t = lexer_.PeekToken();
    auto f = unique_ptr<ast::Function>(new ast::Function(t.val_));
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

        llvm::StringRef name;
        if (lexer_.ExpectToken(Lexer::Token::COLON)) {
          name = t.val_;
          t = lexer_.PeekToken();
          if (t.type_ != Lexer::Token::IDENT)
            return NULL;
          lexer_.ReadToken();
        }

        llvm::Type *type = TranslateType(t.val_);
        if (!type)
          return NULL;

        f->name_args_.push_back(name);
        f->type_args_.push_back(type);
        need_comma = true;
      }

      if (!ExpectToken(Lexer::Token::PAREN, ")"))
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

    unique_ptr<ast::Statement> stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      f->Append(move(stmt));
    }

    if (!ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;
    return move(f);
  }

  unique_ptr<ast::Statement> FileParser::Statement() {
    unique_ptr<ast::Statement> stmt = NULL;
    for (;;) {
      stmt = If();
      if (stmt) return stmt;
      stmt = While();
      if (stmt) return stmt;
      stmt = Var();
      if (stmt) break;
      stmt = Return();
      if (stmt) break;
      {
        auto expr = Expression();
        if (expr) {
          stmt = unique_ptr<ast::Statement>(new ast::ExpressionStatement(move(expr)));
          break;
        }
      }
      return NULL;
    }

    if (!ExpectToken(Lexer::Token::SEMICOLON))
      return NULL;
    return stmt;
  }

  unique_ptr<ast::Statement> FileParser::Var() {
    if (!lexer_.ExpectToken(Lexer::Token::VAR))
      return NULL;

    Lexer::Token ident = lexer_.PeekToken();
    if (ident.type_ != Lexer::Token::IDENT)
      return NULL;
    lexer_.ReadToken();

    if (!ExpectToken(Lexer::Token::OPER, "="))
      return NULL;

    auto expr = Expression();
    if (!expr)
      return NULL;

    return unique_ptr<ast::Statement>(new ast::VariableAssignment(ident.val_, move(expr)));
  }

  unique_ptr<ast::Statement> FileParser::If() {
    if (!lexer_.ExpectToken(Lexer::Token::IF))
      return NULL;

    auto if_ = unique_ptr<ast::If>(new ast::If(Expression()));
    if (!ExpectToken(Lexer::Token::BRACKET, "{"))
      return NULL;

    unique_ptr<ast::Statement> stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      if_->AppendThen(move(stmt));
    }

    if (!ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;

    if (lexer_.ExpectToken(Lexer::Token::ELSE)) {
      if (!ExpectToken(Lexer::Token::BRACKET, "{"))
        return NULL;

      while ((stmt = Statement()) != NULL) {
        if_->AppendElse(move(stmt));
      }

      if (!ExpectToken(Lexer::Token::BRACKET, "}"))
        return NULL;
    }
    return move(if_);
  }

  unique_ptr<ast::Statement> FileParser::While() {
    if (!lexer_.ExpectToken(Lexer::Token::WHILE))
      return NULL;

    auto while_ = unique_ptr<ast::While>(new ast::While(Expression()));
    if (!ExpectToken(Lexer::Token::BRACKET, "{"))
      return NULL;

    unique_ptr<ast::Statement> stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      while_->Append(move(stmt));
    }

    if (!ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;

    return move(while_);
  }

  unique_ptr<ast::Statement> FileParser::Return() {
    if (!lexer_.ExpectToken(Lexer::Token::RETURN))
      return NULL;
    return unique_ptr<ast::Statement>(new ast::Return(Expression()));
  }

  unique_ptr<ast::Expression> FileParser::Primary() {
    Lexer::Token t = lexer_.PeekToken();
    switch (t.type_) {
      case Lexer::Token::OPER:
        lexer_.ReadToken();
        return unique_ptr<ast::Expression>(new ast::UnaryOperation(t.val_, Primary()));
      case Lexer::Token::PAREN: {
        if (t.val_ != "(") return NULL;
        lexer_.ReadToken();

        auto expr = Expression();
        if (!expr || !ExpectToken(Lexer::Token::PAREN, ")"))
          return NULL;
        return expr;
      }
      case Lexer::Token::INT:
        lexer_.ReadToken();
        return PrimaryRHS(unique_ptr<ast::Expression>(new ast::IntegerLiteral(atoi(t.val_.str().c_str()))));
      case Lexer::Token::IDENT:
        lexer_.ReadToken();
        return PrimaryRHS(unique_ptr<ast::Expression>(new ast::Variable(t.val_)));
      default:
        return NULL;
    }
  }

  unique_ptr<ast::Expression> FileParser::PrimaryRHS(unique_ptr<ast::Expression> LHS) {
    for (;;) {
      Lexer::Token t = lexer_.PeekToken();
      switch (t.type_) {
        case Lexer::Token::PAREN: {
          if (t.val_ == "(") {
            lexer_.ReadToken();

            auto function = unique_ptr<ast::CallOperation>(new ast::CallOperation(move(LHS)));
            bool need_comma = false;
            for (;;) {
              if (lexer_.ExpectToken(Lexer::Token::PAREN, ")"))
                break;

              if (need_comma) {
                if (!ExpectToken(Lexer::Token::OPER, ","))
                  return NULL;
              }

              need_comma = true;
              function->args_.push_back(Expression());
            }
            LHS = move(function);
            break;
          }
          return LHS;
        }
        default:
          return LHS;
      }
    }
  }

  unique_ptr<ast::Expression> FileParser::Expression() {
    auto LHS = Primary();
    if (!LHS) return NULL;
    return BinOpRHS(0, move(LHS));
  }

  unique_ptr<ast::Expression> FileParser::BinOpRHS(int prec, unique_ptr<ast::Expression> LHS) {
    for (;;) {
      Lexer::Token t = lexer_.PeekToken();
      int tok_prec = GetTokPrecedence(t);
      if (tok_prec < prec)
        return LHS;

      llvm::StringRef binop = t.val_;
      lexer_.ReadToken();

      auto RHS = Primary();
      if (!RHS) return NULL;

      t = lexer_.PeekToken();
      int next_prec = GetTokPrecedence(t);
      if (tok_prec < next_prec || (tok_prec == next_prec && tok_prec % 2)) {
        RHS = BinOpRHS(tok_prec, move(RHS));
        if (!RHS) return NULL;
      }

      LHS = unique_ptr<ast::Expression>(new ast::BinaryOperation(binop, move(LHS), move(RHS)));
    }
  }

  bool FileParser::ExpectToken(Lexer::Token::Type type, llvm::StringRef val) {
    if (!lexer_.ExpectToken(type, val)) {
      Error("unexpected token");
      return false;
    }
    return true;
  }

  bool FileParser::ExpectToken(Lexer::Token::Type type) {
    if (!lexer_.ExpectToken(type)) {
      Error("unexpected token");
      return false;
    }
    return true;
  }

  void FileParser::Error(const string& msg) {
    char buf[4096];
    Lexer::LineInfo info = lexer_.GetLineInfo();
    snprintf(buf, 4096, "%s:%lu:%lu: error: %s", filename_.c_str(), info.line_, info.col_, msg.c_str());

    errs_.Error(buf);
  }
}

unique_ptr<Messages> Parser::Parse(const string& contents, const string& name) {
  auto msgs = unique_ptr<Messages>(new Messages);
  FileParser parser(ctx_, *msgs, name, contents);
  auto ast = parser.Parse();
  if (!ast)
    return msgs;

  auto scope = shared_ptr<Scope>(new Scope);
  ast->Codegen(module_, scope);
  return msgs;
}

unique_ptr<Messages> Parser::ParseFile(const string& path) {
  string contents = ReadFile(path);
  return Parse(contents, path);
}
