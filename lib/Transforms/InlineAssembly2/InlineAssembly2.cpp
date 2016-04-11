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

using namespace llvm;


std::string fctx_begin = ""
"define void @ir_func() #0 {\n"
"entry:\n";
std::string fctx_end = "\n"
"ret void\n"
"}\n";

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
    dbgs() << "Visit " << CI << "\n";
    //dbgs().flush();
    for (auto it = CI.op_begin(); it != CI.op_end(); ++it) {
      if (InlineAsm *s = dyn_cast<InlineAsm>(it->get())) {
        //if (s->getDialect() != s->AD_IR) continue;

        Module *M = CI.getModule();
        //Function *F = CI.getFunction();
        //llvm::getGlobalContext()

        const std::string& asm_str = s->getAsmString();
        dbgs() << asm_str << "\n";
        std::string module_str = fctx_begin + asm_str + fctx_end;

        llvm::SMDiagnostic Error;
        auto &&Mod = parseAssemblyString(module_str, Error, M->getContext());
        if (!Mod) {
          std::string ErrMsg;
          llvm::raw_string_ostream Errs(ErrMsg);
          Errs << "failed to parse inline assembly:" << "\n";
          Errs << std::string(Error.getMessage()) << "\n";
          Errs << "line " << Error.getLineNo() << "\n";
          Errs << std::string(Error.getLineContents()) << "\n";
          llvm::report_fatal_error(Errs.str());
          return;
        }
        llvm::Linker::linkModules(*M, std::move(Mod));

        StringRef fname("ir_func");
        Function *f_new = static_cast<llvm::Function*>(M->getFunction(fname));
        FunctionType *f_type = f_new->getFunctionType();
        ArrayRef<Value*> args;
        CallInst *inst = CallInst::Create(f_type, f_new, args);
        inst->insertAfter(&CI);
        CI.removeFromParent();

        //IRBuilder<> builder(F->getContext());
        //CallInst *instr2 = builder.CreateCall(f_new, args);
        //instr2->insertBefore(&CI);
        //instr2->insertAfter(&CI);
        modified = true;
      }
    }
  }

};


struct InlineAssembly2 : public FunctionPass {
  static char ID;
  InlineAssembly2() : FunctionPass(ID) {}

  virtual bool runOnFunction(Function &F) {
    InlineAssemblyVisitor visitor;
    visitor.visit(F);
    //dbgs() << F.getName() << ":\n" << F << "\n";
    return visitor.modified;
  }
};

char InlineAssembly2::ID = 0;
static RegisterPass<InlineAssembly2> Y("InlineAssembly2", "InlineAssembly2");
