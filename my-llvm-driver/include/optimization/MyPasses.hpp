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
    //判断I是否为死代码，如果是，则将其加入WorkList并返回真，否则返回假
    static bool DCEInstruction(Instruction *I, SmallSetVector<Instruction *, 16> &WorkList, 
        const TargetLibraryInfo * TLI) {
        //当前指令use_empty()返回值为真 且 不是跳转指令或者返回指令 且 没有副作用
        //Terminator Instruction--In LLVM, a BasicBlock will always end with a TerminatorInst. 
        //TerminatorInsts cannot appear anywhere else other than at the end of a BasicBlock.
        //对于一个Instruction *I，I指向的是该指令的%开头的左值，其右值有三种情况：1.常数 2.@开头的全局变量 3.%开头的局部变量
        if (I->use_empty() && !I->isTerminator() && !I->mayHaveSideEffects()) {
            //对于该指令的每个操作数（右值）
            for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i) {
                Value *OpV = I->getOperand(i);  //OpV为指向当前操作数的指针
                I->setOperand(i, nullptr);      //设置指向该操作数的指针为空??getOperandList()[i] = nullptr;

                if (!OpV->use_empty() || I == OpV)  //如果该操作数use_empty()返回值为假 或 指令的左值为当前操作数
                    continue;                       //进入下一轮循环

                if (Instruction *OpI = dyn_cast<Instruction>(OpV))  //如果该操作数不符合前面if的条件，且可以动态转换为对应的IR指令（也就是说它是%开头的）
                    if (isInstructionTriviallyDead(OpI, TLI))   //如果这条指令产生的该值没被使用过且这个指令没有其他影响
                        WorkList.insert(OpI);           //将该指令插入WorkList
            }
            //print:将对象格式化到指定的缓冲区。如果成功，将返回格式化字符串的长度。如果缓冲区太小，则返回一个大于BufferSize的长度以供重试。
            I->print(llvm::outs()); //
            std::cout << " " << I->getOpcodeName() << std::endl;    //输出该指令的操作码
            I->eraseFromParent();                                   //将该指令从包含它的基本块中解除链接并删除它
            return true;    //成功删除一条死代码,返回真
        }
        return false;   //否则返回假
    }
    //根据当前可用的库函数的信息，删除函数F中的死代码
    static bool eliminateDeadCode(Function &F, TargetLibraryInfo *TLI) {
        bool MadeChange = false;
        SmallSetVector<Instruction *, 16> WorkList;
        std::cout << "The Elimated Instructions: {" << std::endl;
        //遍历F中的每一条指令，找到其中的死代码并加入WorkList
        for (inst_iterator FI = inst_begin(F), FE = inst_end(F); FI != FE;) {
            Instruction *I = &*FI;
            ++FI;

            if (!WorkList.count(I)) //如果I不在WorkList中
            MadeChange |= DCEInstruction(I, WorkList, TLI); //判断I是否为死代码，如果是则加入WorkList并将MadeChange置为真
        }

        //WorkList相当于一个栈，现在从栈顶开始，判断I是否为死代码，如果是则加入WorkList并将MadeChange置为真
        while (!WorkList.empty()) { 
            Instruction *I = WorkList.pop_back_val();
            MadeChange |= DCEInstruction(I, WorkList, TLI);
        }
        std::cout << "}" << std::endl;
        return MadeChange;  //返回MadeChange，表示IR是否发生变化
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
                //注意printf和scanf函数会引入全局变量，应该是对应的格式化字符串
                std::cout << "The number of global variable is " << num_of_globalVariable << "." << std::endl;
                std::cout << "The type alias are as follows:" << std::endl;
                for(auto i : M.getIdentifiedStructTypes()){
                    llvm::outs() << i->getStructName() << "\n";
                    llvm::outs() << i->getNumContainedTypes() << "\n";
                }
                std::cout << "The number of type alias is " << M.getIdentifiedStructTypes().size() << "." << std::endl;
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