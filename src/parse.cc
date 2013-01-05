
#include "lexer.h"
#include "parse.h"
#include "scope.h"
#include "util.h"
#include <memory>
#include <string>
#include <vector>
using namespace std;

struct MessagesImpl {
  void Error(const string& msg) {
    msgs_.push_back(Message(msg, Message::ERROR));
  }

  void Warning(const string& msg) {
    msgs_.push_back(Message(msg, Message::WARNING));
  }

  void Info(const string& msg) {
    msgs_.push_back(Message(msg, Message::INFO));
  }

  size_t Count(Message::Level level) const {
    size_t count = 0;
    for (auto& msg : msgs_) {
      if (msg.level() <= level)
        ++count;
    }
    return count;
  }

  const vector<Message>& messages() const { return msgs_; }

private:
  vector<Message> msgs_;
};

Messages::Messages() : impl_(new MessagesImpl) {}

void Messages::Error(const string& msg) {
  impl_->Error(msg);
}

void Messages::Warning(const string& msg) {
  impl_->Warning(msg);
}

void Messages::Info(const string& msg) {
  impl_->Info(msg);
}

size_t Messages::Count(Message::Level level) const {
  return impl_->Count(level);
}

const std::vector<Message>& Messages::messages() const {
  return impl_->messages();
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

    ast::TopLevel *Parse();
    ast::TopLevel *TopLevel();
    ast::TopLevel *Function();
    ast::Statement *Statement();
    ast::Statement *Var();
    ast::Statement *If();
    ast::Statement *Return();
    ast::Expression *Primary();
    ast::Expression *PrimaryRHS(ast::Expression *LHS);
    ast::Expression *Expression();
    ast::Expression *BinOpRHS(int prec, ast::Expression *LHS);

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

  ast::TopLevel *FileParser::Parse() {
    lexer_.ReadToken();
    ast::Program *program = new ast::Program();
    ast::TopLevel *stmt = NULL;
    while ((stmt = TopLevel()) != NULL) {
      program->Append(stmt);
    }
    return lexer_.ExpectToken(Lexer::Token::TEOF) ? program : NULL;
  }

  ast::TopLevel *FileParser::TopLevel() {
    ast::TopLevel *stmt = Function();
    if (stmt) return stmt;
    return NULL;
  }

  ast::TopLevel *FileParser::Function() {
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

    ast::Statement *stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      f->Append(stmt);
    }

    if (!ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;
    return f;
  }

  ast::Statement *FileParser::Statement() {
    ast::Statement *stmt = NULL;
    for (;;) {
      stmt = If();
      if (stmt) return stmt;
      stmt = Var();
      if (stmt) break;
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

    if (!ExpectToken(Lexer::Token::SEMICOLON))
      return NULL;
    return stmt;
  }

  ast::Statement *FileParser::Var() {
    if (!lexer_.ExpectToken(Lexer::Token::VAR))
      return NULL;

    Lexer::Token ident = lexer_.PeekToken();
    if (ident.type_ != Lexer::Token::IDENT)
      return NULL;
    lexer_.ReadToken();

    if (!ExpectToken(Lexer::Token::OPER, "="))
      return NULL;

    ast::Expression *expr = Expression();
    if (!expr)
      return NULL;

    return new ast::VariableAssignment(ident.val_, expr);
  }

  ast::Statement *FileParser::If() {
    if (!lexer_.ExpectToken(Lexer::Token::IF))
      return NULL;

    ast::Expression *expr = Expression();
    if (!ExpectToken(Lexer::Token::BRACKET, "{"))
      return NULL;

    ast::If *if_ = new ast::If(expr);
    ast::Statement *stmt = NULL;
    while ((stmt = Statement()) != NULL) {
      if_->AppendThen(stmt);
    }

    if (!ExpectToken(Lexer::Token::BRACKET, "}"))
      return NULL;

    if (lexer_.ExpectToken(Lexer::Token::ELSE)) {
      if (!ExpectToken(Lexer::Token::BRACKET, "{"))
        return NULL;

      while ((stmt = Statement()) != NULL) {
        if_->AppendElse(stmt);
      }

      if (!ExpectToken(Lexer::Token::BRACKET, "}"))
        return NULL;
    }
    return if_;
  }

  ast::Statement *FileParser::Return() {
    if (!lexer_.ExpectToken(Lexer::Token::RETURN))
      return NULL;
    return new ast::Return(Expression());
  }

  ast::Expression *FileParser::Primary() {
    Lexer::Token t = lexer_.PeekToken();
    switch (t.type_) {
      case Lexer::Token::OPER:
        lexer_.ReadToken();
        return new ast::UnaryOperation(t.val_, Primary());
      case Lexer::Token::PAREN: {
        if (t.val_ != "(") return NULL;
        lexer_.ReadToken();

        ast::Expression *expr = Expression();
        if (!expr || !ExpectToken(Lexer::Token::PAREN, ")"))
          return NULL;
        return expr;
      }
      case Lexer::Token::INT:
        lexer_.ReadToken();
        return PrimaryRHS(new ast::IntegerLiteral(atoi(t.val_.str().c_str())));
      case Lexer::Token::IDENT:
        lexer_.ReadToken();
        return PrimaryRHS(new ast::Variable(t.val_));
      default:
        return NULL;
    }
  }

  ast::Expression *FileParser::PrimaryRHS(ast::Expression *LHS) {
    for (;;) {
      Lexer::Token t = lexer_.PeekToken();
      switch (t.type_) {
        case Lexer::Token::PAREN: {
          if (t.val_ == "(") {
            lexer_.ReadToken();

            auto function = new ast::CallOperation(LHS);
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
            LHS = function;
            break;
          }
          return LHS;
        }
        default:
          return LHS;
      }
    }
  }

  ast::Expression *FileParser::Expression() {
    ast::Expression *LHS = Primary();
    if (!LHS) return NULL;
    return BinOpRHS(0, LHS);
  }

  ast::Expression *FileParser::BinOpRHS(int prec, ast::Expression *LHS) {
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
      if (tok_prec < next_prec || (tok_prec == next_prec && tok_prec % 2)) {
        RHS = BinOpRHS(tok_prec, RHS);
        if (!RHS) return NULL;
      }

      LHS = new ast::BinaryOperation(binop, LHS, RHS);
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

bool Parser::ImportFile(const string& path) {
  return false;
}

Messages Parser::Parse(const string& contents, const string& name) {
  Messages msgs;
  FileParser parser(ctx_, msgs, name, contents);
  ast::TopLevel *ast = parser.Parse();
  if (!ast)
    return msgs;

  auto scope = shared_ptr<Scope>(new Scope);
  ast->Codegen(module_, scope);
  return msgs;
}

Messages Parser::ParseFile(const string& path) {
  string contents = ReadFile(path);
  return Parse(contents, path);
}
