#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Module.h"

#include "llvm/AsmParser/Parser.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/ErrorHandling.h"

#include "llvm/IR/InstVisitor.h"
#include "llvm/Support/Debug.h"

#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/IR/InstIterator.h"

using namespace llvm;



struct InlineAssemblyVisitor
  : public InstVisitor<InlineAssemblyVisitor>
{
  bool modified;

  InlineAssemblyVisitor()
    : modified(false)
  {}

  void visitCallInst(CallInst &CI) {
    if (!CI.isInlineAsm()){
      return;
    }
    for (auto &ciOp : CI.operands()) {
      if (InlineAsm *iasm = dyn_cast<InlineAsm>(ciOp)) {
        //if (iasm->getDialect() != s->AD_IR) continue;

        //Module *M = CI.getModule();
        Function *F = CI.getFunction();
        LLVMContext &FC = F->getContext();

        std::string tempFuncName = "ir_func";
        std::string entryStr = "entry";

        auto emptyFuncMod = make_unique<Module>("ir_inline_asm", getGlobalContext());
        Type *retType = CI.getType();
        Function *emptyFunc = cast<Function>(emptyFuncMod->getOrInsertFunction(tempFuncName, retType, NULL));
        for (auto &arg : CI.arg_operands()) {
          new llvm::Argument(arg->getType(), "", emptyFunc);
        }
        IRBuilder<> Builder(emptyFuncMod->getContext());
        BasicBlock *BB = BasicBlock::Create(emptyFuncMod->getContext(), entryStr, emptyFunc);
        Builder.SetInsertPoint(BB);
        if (retType->isVoidTy()) {
          Builder.CreateRetVoid();
        }

        std::string modStr;
        raw_string_ostream modStream(modStr);
        modStream << *emptyFuncMod;
        modStream.flush();

        std::string asm_str = iasm->getAsmString();
        std::string entryInsertStr = entryStr + ":";
        size_t entryPos = modStr.find(entryInsertStr);
        if (entryPos == std::string::npos) {
          llvm::report_fatal_error("can't find entry");
          return;
        }
        modStr.insert(entryPos + entryInsertStr.length(), "\n" + asm_str);
        //dbgs() << "module:\n" << modStr << "\n\n";

        SMDiagnostic Error;
        auto parsedFuncMod = parseAssemblyString(modStr, Error, getGlobalContext());
        if (!parsedFuncMod) {
          std::string ErrMsg;
          raw_string_ostream Errs(ErrMsg);
          Errs << "failed to parse inline assembly:\n";
          Errs << Error.getMessage() << "\n";
          Errs << "line " << Error.getLineNo() << "\n";
          Errs << Error.getLineContents() << "\n";
          llvm::report_fatal_error(Errs.str());
          return;
        }
        Function* parsedFunc = parsedFuncMod->getFunction(tempFuncName);

        std::vector<Use*> fargs;
        for (auto &src_it : CI.operands()) {
          fargs.push_back(&src_it);
        }

        bool hasReturnInst = false;
        for (inst_iterator I = inst_begin(parsedFunc), E = inst_end(parsedFunc); I != E; ++I) {
          if (ReturnInst* ret = dyn_cast<ReturnInst>(&*I)) {
            if (hasReturnInst) {
              llvm::report_fatal_error("ir inline asm should have only one return instruction");
            }
            if (ret->getNumOperands() != 1) {
              llvm::report_fatal_error("ir inline asm should have one return operand");
            }
            auto op = ret->op_begin()->get();
            if (!op->getType()->isVoidTy()) {
              CI.replaceAllUsesWith(op);
            }
            hasReturnInst = true;
            continue;
          }
          Instruction *i = I->clone();
          I->replaceAllUsesWith(i);
          if (I->hasName()) {
            i->setName(I->getName());
          }
          for (auto &op : i->operands()) {
            if (Argument *arg = dyn_cast<Argument>(op)) {
              if (arg->getParent() == parsedFunc) {
                op = *fargs[arg->getArgNo()];
              }
            }
          }
          i->setMetadata("ir_asm", MDNode::get(FC, MDString::get(FC, "some useful info")));
          i->insertBefore(&CI);
        }
        CI.eraseFromParent();
        modified = true;
      }
    }
  }

};


struct InlineAssembly2 : public FunctionPass {
  static char ID;
  InlineAssembly2()
    : FunctionPass(ID)
  {}

  virtual bool runOnFunction(Function &F) {
    InlineAssemblyVisitor visitor;
    visitor.visit(F);
    return visitor.modified;
  }
};

char InlineAssembly2::ID = 0;
static RegisterPass<InlineAssembly2> Y("InlineAssembly2", "InlineAssembly2");
