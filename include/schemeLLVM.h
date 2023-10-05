#pragma once

#include "lexer.h"
#include "llvm-13/llvm/IR/IRBuilder.h"
#include "llvm-13/llvm/IR/LLVMContext.h"
#include "llvm-13/llvm/IR/Module.h"
#include "llvm-13/llvm/IR/Verifier.h"
#include "parser.h"
#include "llvm/Support/Alignment.h"
#include <memory>
#include <string>
#include <vector>

class SchemeLLVM {
public:
  SchemeLLVM() {
    moduleInit();
    setupExternFunctions();
  }

  void exec(const std::string &program) {
    module->print(llvm::outs(), nullptr);
    Lexer *lexer = new Lexer();
    auto tokens = lexer->lex(program);
    Parser *parser = new Parser(tokens);
    ProgramNode *programNode = parser->parse();
    compile(programNode);
    saveModuleToFile("./out.ll");
  }

private:
  std::unique_ptr<llvm::LLVMContext> ctx;
  std::unique_ptr<llvm::Module> module;
  std::unique_ptr<llvm::IRBuilder<>> builder;
  llvm::Function *fn;

  void moduleInit() {
    ctx = std::make_unique<llvm::LLVMContext>();
    module = std::make_unique<llvm::Module>("schemeLLVM", *ctx);
    builder = std::make_unique<llvm::IRBuilder<>>(*ctx);
  }

  void saveModuleToFile(const std::string &fileName) {
    std::error_code errorCode;
    llvm::raw_fd_ostream outLL(fileName, errorCode);
    module->print(outLL, nullptr);
  }

  void compile(ProgramNode *programNode) {
    fn = createFunction("main",
                        llvm::FunctionType::get(builder->getInt32Ty(), false));
    gen(programNode);
    builder->CreateRet(builder->getInt32(0));
  }

  void gen(ProgramNode *programNode) {
    auto statements = programNode->statements;
    for (auto it = statements.begin(); it < statements.end(); it++) {
      switch ((*it)->type) {
      case StatementType::Let: {
        auto letStatement = (*it)->letStatement;
        llvm::Value *value = genExpr(letStatement->value);
        module->getOrInsertGlobal(letStatement->identifier, value->getType());
        auto variable = module->getNamedGlobal(letStatement->identifier);
        variable->setAlignment(llvm::MaybeAlign(4));
        variable->setConstant(false);
        variable->setInitializer((llvm::Constant *)value);
      }
      }
    }
  }

  llvm::Value *genExpr(ExpressionNode *expressionNode) {
    llvm::Value *dummy;
    switch (expressionNode->type) {
    case ExpressionType::String: {
      auto str =
          builder->CreateGlobalStringPtr(expressionNode->stringExp->value);
      return str;
    }
    case ExpressionType::Number: {
      auto num = builder->getInt32(expressionNode->integerExp->intValue);
      return num;
    }
    case ExpressionType::Infix: {
      auto lhs = genExpr(expressionNode->infixExpr->lhs);
      auto rhs = genExpr(expressionNode->infixExpr->rhs);
      return builder->CreateAdd(lhs, rhs);
    }
    default:
      assert(false && "Should'nt hit here");
    }
    return dummy;
  }

  llvm::Function *createFunction(const std::string &fnName,
                                 llvm::FunctionType *fnType) {
    auto fn = module->getFunction(fnName);
    if (fn == nullptr) {
      fn = createFunctionProto(fnName, fnType);
    }
    createFunctionBlock(fn);
    return fn;
  }

  llvm::Function *createFunctionProto(const std::string &fnName,
                                      llvm::FunctionType *fnType) {
    auto fn = llvm::Function::Create(fnType, llvm::Function::ExternalLinkage,
                                     fnName, *module);
    verifyFunction(*fn);
    return fn;
  }

  void createFunctionBlock(llvm::Function *fn) {
    auto entry = createBB("entry", fn);
    builder->SetInsertPoint(entry);
  }

  llvm::BasicBlock *createBB(std::string name, llvm::Function *fn = nullptr) {
    return llvm::BasicBlock::Create(*ctx, name, fn);
  }

  void setupExternFunctions() {
    auto bytePtrArr = builder->getInt8Ty()->getPointerTo();
    module->getOrInsertFunction(
        "printf", llvm::FunctionType::get(builder->getInt32Ty(), bytePtrArr,
                                          true /*vararg*/));
  }
};