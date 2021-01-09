
// #include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Basic/TargetInfo.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Transforms/Utils.h"


#include "Driver/driver.h"
#include "optimization/LoopSearchPass.hpp"
#include "optimization/MyPasses.hpp"
#include "optimization/FuncInfo.hpp"
#include "optimization/LocalOpts.hpp"
#include "optimization/ActiveVars.hpp"
using namespace llvm;
using namespace clang;
using namespace mDriver;

// driver封装，main来new driver实例
int main(int argc, const char **argv) {

  // 创建诊断函数
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticPrinter *DiagClient =
    new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);

  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagClient);

  // 得到当前进程运行所在的目标平台信息，用于JIT中把代码load进当前进程
  const std::string TripleStr = llvm::sys::getProcessTriple();
  llvm::Triple T(TripleStr);

  // 调用Driver，实例化
  llvm::ErrorOr<std::string> ClangPath = llvm::sys::findProgramByName("clang");
  if (!ClangPath) {
      llvm::errs() << "clang not found.\n";
      exit(1);
  }
  llvm::outs() << "Use clang: " <<  (*ClangPath).c_str() << "\n";
  Driver TheDriver(StringRef((*ClangPath).c_str()), T.str(), Diags);//新建driver
  SmallVector<const char *, 16> Args(argv, argv + argc);
  Args.push_back("-fsyntax-only");
  if(TheDriver.ParseArgs(Args) && TheDriver.BuildCI(Diags)){
    llvm::outs() << "Dump IR successfully.\n";
  }
  else{
    llvm::errs() << "Failed. Early return.\n";
    exit(1);
  }
  
  TheDriver.FrontendCodeGen();

  TheDriver.runChecker();

  TheDriver.InitializePasses();

  //内存到寄存器的分配
  TheDriver.addPass(createPromoteMemoryToRegisterPass());
  //基于LLVM IR的CFG信息和支配树信息查找给定Function代码中存在的所有回边(BackEdge)，每条回边代表代码中存在的一个自然循环
  TheDriver.addPass(createLSPass());
  TheDriver.addPass(llvm::createConstantPropagationPass());
  //死代码删除,将转换为SSA格式后的LLVM IR中use_empty()返回值为真的指令从指令列表中删除(数据流分析)
  TheDriver.addPass(createmyDCEPass()); 
  //对当前Module所定义的函数数目进行计数(数据流分析)
  TheDriver.addPass(createmyGlobalPass());


  //加入必做部分的PASS，PASS代码 在LoopSearchPass.hpp文件中，后半部分
  TheDriver.addPass(createLoopSearchPass());
  TheDriver.addPass(createFuncInfoPass());
  TheDriver.addPass(createLocalOptsPass());
  TheDriver.addPass(createLivenessPass());
  TheDriver.run();
  // TheDriver.printASTUnit();
  // Shutdown.
  llvm::llvm_shutdown();
  return 0;
}
