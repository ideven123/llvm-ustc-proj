#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include<iostream>
using namespace llvm;
namespace llvm
{
    ModulePass *createFuncInfoPass();
    void initializeFunctionInfoPass(PassRegistry &);
} 
namespace
{
    class FunctionInfo : public ModulePass{
    public:
        static char ID;

        FunctionInfo() : ModulePass(ID) {
            initializeFunctionInfoPass(*PassRegistry::getPassRegistry());
        }
         ~FunctionInfo() override {}

        bool runOnModule(llvm::Module &M) override
        {
            llvm::outs() << "Functions Information Pass" << "\n";
            bool transformed = false;
            // 遍历Function内的所有函数
            for (auto iter = M.begin(); iter != M.end(); ++iter)
            {
                Function &func = *iter;
                //函数名
                llvm::outs() << "Name:" << func.getName() << "\n";
                //函数参数个数
                llvm::outs() << "Number of Arguments: " << func.arg_size() << "\n";
                //函数被调用的次数（Module中定义的非main如果被调用，则输出为调用次数+1）（main函数对应为0）
                llvm::outs() << "Number of Direct Call Sites in the same LLVM module: " << func.getNumUses() << "\n";
                //函数的基本块个数
                llvm::outs() << "Number of Basic Blocks: " << func.size() << "\n";
                //函数所包含的IR数
                llvm::outs() << "Number of Instructions: " << func.getInstructionCount() << "\n";
                transformed = true;
            }
            return transformed;
        }
    };
} // namespace

char FunctionInfo::ID = 0;
// 注册自定义pass
INITIALIZE_PASS(FunctionInfo, "Func Info",
                "Function Info", false, false)

ModulePass *llvm::createFuncInfoPass()
{
    return new FunctionInfo();
}