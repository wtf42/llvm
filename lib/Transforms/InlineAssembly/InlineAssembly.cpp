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

using namespace llvm;

#define DEBUG_TYPE "InlineAssembly"

class InlineAssembly : public ModulePass {
public:
  static char ID;

  InlineAssembly() : ModulePass(ID) {}

  bool runOnModule(Module &M) override;

private:
};

char InlineAssembly::ID = 0;

bool InlineAssembly::runOnModule(Module &M) {
  //TODO: if (assembly is not llvm ir) return false;
  std::string AsmStr = M.getModuleInlineAsm();
  M.setModuleInlineAsm(StringRef());
  LLVMContext &Ctx = llvm::getGlobalContext();
  SMDiagnostic Error;
  auto &&Mod = parseAssemblyString(AsmStr, Error, Ctx);
  if (!Mod) {
    std::string ErrMsg;
    raw_string_ostream Errs(ErrMsg);
    Errs << "failed to parse inline assembly:" << "\n";
    Errs << std::string(Error.getMessage()) << "\n";
    Errs << "line " << Error.getLineNo() << "\n";
    Errs << std::string(Error.getLineContents()) << "\n";
    report_fatal_error(Errs.str());
  }
  Linker::linkModules(M, std::move(Mod));
  return true;
}

static RegisterPass<InlineAssembly> Z("InlineAssembly", "InlineAssembly Pass");
