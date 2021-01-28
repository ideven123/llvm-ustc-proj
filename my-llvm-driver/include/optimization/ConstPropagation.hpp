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
#include <cassert>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
//#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/Support/TargetSelect.h"
#include <algorithm>
#include <iostream>

using namespace llvm;

namespace llvm {
    ModulePass * createConstProPagationPass();
    void initializeConstPropagationPass(PassRegistry &);
}

namespace{
    class ConstPropagation : public ModulePass{
    public:
        static char ID;
        //static LLVMContext MyGlobalContext;
        //LLVMContext &Context = MyGlobalContext;
        //llvm::Module *module = new llvm::Module("ConstPropagation", Context);
        //IRBuilder<> builder(Context);

        ConstPropagation() : ModulePass(ID) {
            initializeConstPropagationPass(*PassRegistry::getPassRegistry());
        }
        bool runOnModule(llvm::Module &M) override {
            bool transform = false;
            //遍历Module的每个Function
            for (Function &func : M){
                if (runOnFunction(func)){
                    std::cout << "run on func" << std::endl;
                    transform = true;
                }
            }
            return transform;
        }


        bool runOnFunction(llvm::Function &F)
        {
            bool transform = false;
            //遍历Function的每个BasicBlock
            for (BasicBlock &block : F)
                if (runOnBasicBlock(block)){
                    std::cout << "run on block" << std::endl;
                    transform = true;
                }
                    
            return transform;
        }

        bool runOnBasicBlock(llvm::BasicBlock &B){
            std::vector<Instruction *>wait_delete;
            std::unordered_map<Value *, Value *> vGlobal;
            std::map<Value *, int *> iGlobal;
            std::map<Value *, float *> fGlobal;
            for (Instruction &instr : B) {//从基本块第一条指令开始遍历
                bool allConst = false;
                if (instr.isBinaryOp()) {
                    Value *oper0,*oper1;
                    //auto opID = instr.getType();//getopcode?
                    auto opID = instr.getOpcode();
                    oper0 = instr.getOperand(0);
                    oper1 = instr.getOperand(1);
                    if(oper0->getType()->isFloatTy()){
                        if(auto constFP0=dyn_cast<ConstantFP>(oper0)){
                            if(auto constFP1=dyn_cast<ConstantFP>(oper1)){
                                allConst=true;
                                //IRBuilder<> builder(&instr);
                                //创建val(inst的基类),这是一个位移指令
                                //Value *val = builder.getFloatTy();
                                auto ans = compute(opID,constFP0,constFP1,&instr);
                                instr.replaceAllUsesWith(ans);
                            }
                        }
                    }
                    /*
                    else if(oper0->getType()->isIntegerTy()){
                        if(auto constInt0=dyn_cast<ConstantInt>(oper0)){
                            if(auto constInt1=dyn_cast<ConstantInt>(oper1)){
                                allConst=true;
                                auto ans = compute(opID,constInt0,constInt1);
                                instr.replaceAllUsesWith(ans);
                            }
                        }
                    }
                    */
                }

                if (allConst) {
                    wait_delete.push_back(&instr);
                }
            }
            //删除需要删除的instr
            for (Instruction *inst : wait_delete)
                if (inst->isSafeToRemove())
                    inst->eraseFromParent();   
        }


        Constant *compute(unsigned int op,ConstantFP *value1,ConstantFP *value2,Instruction* instr) {
            float c_value1 = value1->getValue().convertToFloat();
            float c_value2 = value2->getValue().convertToFloat();
            llvm::outs()<<"op1"<<c_value1<<"\n";
            llvm::outs()<<"op2"<<c_value2<<"\n";
            IRBuilder<> builder(instr);
            switch (op) {
            case Instruction::FAdd:
                return ConstantFP::get(builder.getFloatTy(),c_value1 + c_value2);
                break;
            case Instruction::FSub:
                //return ConstantFP::get(llvm::Type::getFloatTy(Context),c_value1 - c_value2);
                break;
            case Instruction::FMul:
                //return ConstantFP::get(llvm::Type::getFloatTy(Context),c_value1 * c_value2);
                break;
            case Instruction::FDiv:
                //return ConstantFP::get(llvm::Type::getFloatTy(Context),c_value1 / c_value2);
                break;
            default:
                return nullptr;
                break;
            }
        }

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
        }
    };
}



char ConstPropagation::ID = 0;
INITIALIZE_PASS(ConstPropagation, "ConstPropagation", "My ConstPropagation Pass", false, false)
ModulePass *llvm::createConstProPagationPass() {
    return new ConstPropagation();
}