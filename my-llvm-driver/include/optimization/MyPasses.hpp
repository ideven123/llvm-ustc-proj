#include "llvm/Transforms/Scalar/DCE.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/DebugCounter.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/AssumeBundleBuilder.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"

#include <iostream>

using namespace llvm;

namespace llvm {
    FunctionPass * createmyDCEPass();
    void initializemyDCEPassPass(PassRegistry&);
}

namespace {
    static bool DCEInstruction(Instruction *I, SmallSetVector<Instruction *, 16> &WorkList, 
        const TargetLibraryInfo * TLI) {
        //当前指令use_empty()返回值为真 且 不是跳转指令或者返回指令(Terminator Instruction) 且 没有副作用
        if (I->use_empty() && !I->isTerminator() && !I->mayHaveSideEffects()) {
            //对于该指令的每个操作数
            for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i) {
                Value *OpV = I->getOperand(i);  //OpV指向当前操作数的值
                I->setOperand(i, nullptr);      //设置指向该操作数的指针为空

                if (!OpV->use_empty() || I == OpV)
                    continue;

                if (Instruction *OpI = dyn_cast<Instruction>(OpV))
                    if (isInstructionTriviallyDead(OpI, TLI))
                        WorkList.insert(OpI);
            }
            I->print(llvm::outs());
            std::cout << " " << I->getOpcodeName() << std::endl;
            I->eraseFromParent();
            return true;
        }
        return false;
    }

    static bool eliminateDeadCode(Function &F, TargetLibraryInfo *TLI) {
        bool MadeChange = false;
        SmallSetVector<Instruction *, 16> WorkList;
        std::cout << "The Elimated Instructions: {" << std::endl;
        for (inst_iterator FI = inst_begin(F), FE = inst_end(F); FI != FE;) {
            Instruction *I = &*FI;
            ++FI;

            if (!WorkList.count(I))
            MadeChange |= DCEInstruction(I, WorkList, TLI);
        }

        while (!WorkList.empty()) {
            Instruction *I = WorkList.pop_back_val();
            MadeChange |= DCEInstruction(I, WorkList, TLI);
        }
        std::cout << "}" << std::endl;
        return MadeChange;
    }

    struct myDCEPass : public FunctionPass {
        static char ID;
        myDCEPass() : FunctionPass(ID) {
            initializemyDCEPassPass(*PassRegistry::getPassRegistry());
        }
        bool runOnFunction(Function &F) override {
            if (skipFunction(F)) {
                return false;
            }

            auto * TLIP = getAnalysisIfAvailable<TargetLibraryInfoWrapperPass>();
            TargetLibraryInfo *TLI = TLIP ? &TLIP->getTLI(F) : nullptr;

            return eliminateDeadCode(F, TLI);
        }
        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
        }
    };
}

char myDCEPass::ID = 0;
INITIALIZE_PASS(myDCEPass, "mydce", "My dead code elimination", false, false)
FunctionPass *llvm::createmyDCEPass() {
    return new myDCEPass();
}


//===--------------------------------------------------------------------===//
// Module Pass Demo
//
namespace llvm {
    ModulePass * createmyGlobalPass();
    void initializemyGlobalPassPass(PassRegistry&);
}

namespace {
    class myGlobalPass : public ModulePass {
        public:
            static char ID;
            myGlobalPass() : ModulePass(ID) {
                initializemyGlobalPassPass(*PassRegistry::getPassRegistry());
            }
            bool runOnModule(llvm::Module& M) override {
                if (skipModule(M)) 
                    return false;
                int num_of_func = 0;
                for (llvm::Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++ FI, ++ num_of_func);
                std::cout << "The number of functions is " << num_of_func << "." << std::endl;
                int num_of_globalVariable = 0;
                std::cout << "The global variables are as follows:" << std::endl;
                for(llvm::Module::global_iterator GI = M.global_begin(), GE = M.global_end(); GI != GE; ++GI, ++num_of_globalVariable){
                    //std::cout << GI->getGlobalIdentifier() << std::endl;
                    llvm::outs() << GI->getName() << "\n";
                }
                //注意printf和scanf函数会引入全局变量，猜测应该是对应的格式化字符串
                std::cout << "The number of global variable is " << num_of_globalVariable << "." << std::endl;
                int num_of_alias = 0;
                for(llvm::Module::alias_iterator AI = M.alias_begin(), AE = M.alias_end(); AI != AE; ++AI, num_of_alias++){
                    llvm::outs() << AI->getName() << "\n";
                }
                std::cout << "The number of alias is " << num_of_alias << "." << std::endl;
                /*
                int num_of_named_metadata = 0;
                for(llvm::Module::named_metadata_iterator NI = M.named_metadata_begin(), NE = M.named_metadata_end(); NI != NE; ++NI, num_of_named_metadata++){
                    llvm::outs() << NI->getName() << "\n";
                }
                std::cout << "The number of named metadata is " << num_of_named_metadata << "." << std::endl;
                */

                //llvm::outs() << Module::getInstructionCount() << "\n";
                return true;
            }
    };
}   

char myGlobalPass::ID = 0;
INITIALIZE_PASS(myGlobalPass, "global",
                "My Global Pass", false, false)

ModulePass * llvm::createmyGlobalPass() {
    return new myGlobalPass();
}