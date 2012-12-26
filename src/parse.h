#pragma once

#include "ast.h"
#include <llvm/LLVMContext.h>

ast::TopLevel *Parse(llvm::LLVMContext& ctx, const std::string& contents);
