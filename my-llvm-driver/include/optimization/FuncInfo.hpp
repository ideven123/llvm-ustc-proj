#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include<iostream>
using namespace llvm;
namespace llvm
{
    ModulePass *createFuncInfoPass();
    void initializeFunctionInfoPass(PassRegistry &);
} // namespace llvm
namespace
{
    // 继承自ModulePass
    class FunctionInfo : public ModulePass{
    public:
        static char ID;

        FunctionInfo() : ModulePass(ID) {
            initializeFunctionInfoPass(*PassRegistry::getPassRegistry());
        }
         ~FunctionInfo() override {}

        // We don't modify the program, so we preserve all analysis.
        void getAnalysisUsage(AnalysisUsage &AU) const override
        {
            AU.setPreservesAll();
        }

        bool runOnModule(llvm::Module &M) override
        {
            llvm::outs() << "CSCD70 Functions Information Pass"
                   << "\n";
            bool transformed = false;
            // 遍历内含的所有函数
            for (auto iter = M.begin(); iter != M.end(); ++iter)
            {
                Function &func = *iter;
                llvm::outs() << "Name:" << func.getName() << "\n";
                llvm::outs() << "Number of Arguments: " << func.arg_size() << "\n";
                llvm::outs() << "Number of Direct Call Sites in the same LLVM module: "
                       << func.getNumUses() << "\n";
                llvm::outs() << "Number of Basic Blocks: " << func.size() << "\n";
                llvm::outs() << "Number of Instructions: " << func.getInstructionCount() << "\n";
                transformed = true;
            }
            return transformed;
        }
    };
} // namespace

char FunctionInfo::ID = 0;
// 注册自定义pass
INITIALIZE_PASS(FunctionInfo, "Func",
                "Function Info", false, false)

ModulePass *llvm::createFuncInfoPass()
{
    return new FunctionInfo();
}