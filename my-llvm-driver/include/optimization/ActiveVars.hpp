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

#include <iostream>

using namespace llvm;


namespace llvm {
    FunctionPass * createLivenessPass();
    void initializeLivenessPass(PassRegistry&);
}


namespace{
    class Liveness : public FunctionPass{
    public:
        std::map<BasicBlock *, std::set<Value *>> live_in, live_out;
        std::map<BasicBlock *, std::set<Value *>> defB, useB;
        std::set<Value *> tmpDef;//暂存已经分析过的块中指令的定值变量
        static char ID;
        Liveness() : FunctionPass(ID) {
            initializeLivenessPass(*PassRegistry::getPassRegistry());
        }

        bool runOnFunction(llvm::Function & F) override{
            llvm::outs()<<"activevar pass run begin\n";
            if (skipFunction(F)) {
                return false;
            }
            live_in.clear();
            live_out.clear();
            defB.clear();
            useB.clear();

            //先初始化函数所有基本块的use和def集合
            for (BasicBlock &bb : F) {
                tmpDef.clear();
                for (Instruction &instr : bb) {//从基本块第一条指令开始遍历
                    for(auto iter = instr.op_begin(); iter != instr.op_end(); iter++){//遍历一条指令的操作数(等式右边的)
                        if(auto *A1=dyn_cast<ConstantFP>(*iter)){//若该指令为常数
                            //llvm::outs()<<" pass ";
                            continue;
                        }
                        else if(auto *A2=dyn_cast<ConstantInt>(*iter)){
                            //llvm::outs()<<" pass ";
                            continue;
                        }
                        Value* val = dyn_cast<Value>(*iter);
                        //val->printAsOperand(llvm::outs(),false);
                        auto t=val->getType();
                        //llvm::outs()<<" typeid: "<<t->getTypeID();
                        //llvm::outs()<<", ";
                        //if(t->isFunctionTy()) llvm::outs()<<"functype ";
                        //if(t->isLabelTy()) llvm::outs()<<"labeltype ";
                        if(!t->isPointerTy() && !t->isLabelTy() && tmpDef.find(val)==tmpDef.end()){//该操作数不是label,不是函数名,且在该引用前B中无定值,则加入use集合
                            useB[&bb].insert(val);
                        }
                    }
                    //llvm::outs()<<"\n"<<instr.getOpcode()<<"\n";
                    //instr.printAsOperand(llvm::outs(),false);
                    //llvm::outs()<<",\n";
                    Value* Lval=dyn_cast<Value>(&instr);
                    //if(isa<PHINode>(instr) || instr.isBinaryOp() || isa<LoadInst>(instr)){//是等式左边参数，则加入def集合
                    if(!instr.isTerminator() && !instr.isIndirectTerminator() && !instr.isExceptionalTerminator()){
                        defB[&bb].insert(Lval);
                        tmpDef.insert(Lval);//加入用于判断useB
                    }
                }
                llvm::outs()<<"\nuseB:\n";
                for (auto v = useB[&bb].begin(); v != useB[&bb].end(); v++) {
                    (*v)->printAsOperand(llvm::outs(),false);
                    llvm::outs()<< ", " ;       
                }
                llvm::outs()<<"\ndefB:\n";
                for (auto v = defB[&bb].begin(); v != defB[&bb].end(); v++) {
                    (*v)->printAsOperand(llvm::outs(),false);
                    llvm::outs()<<", " ;       
                }
                llvm::outs()<<"\n\n";
            }
            //开始迭代计算
            bool change=true;
            int iteration=0;
            while(change){
                change = false;
                iteration++;
                //llvm::outs()<<iteration<<"\n";
                for(BasicBlock &bb : F){//遍历每一个基本块
                    //计算OUT[B]
                    auto tmpIn = live_in[&bb];//用于比较在该次迭代前后是否有基本块的IN集合发生变化
                    live_in[&bb].clear();
                    live_out[&bb].clear();
                    for(BasicBlock* block : successors(&bb)){//对bb块的所有直接后继块遍历
                        for(auto iter=live_in[block].begin();iter!=live_in[block].end();iter++){
                            //先把该后继块的In集全部加入bb块的OUT集
                            live_out[&bb].insert(*iter);
                        }
                        //再根据phi结点作删减
                        for(auto phi_iter = block->phis().begin(); phi_iter != block->phis().end(); phi_iter++){//对所有phi指令遍历
                            PHINode & phi_inst = *phi_iter;
                            for(auto phi_inst_iter = phi_inst.block_begin(); //对当前phi指令的所有前驱块遍历
                                phi_inst_iter != phi_inst.block_end(); phi_inst_iter++){
                                // 获取PHI指令中的各个前驱基础块
                                BasicBlock* &curr_bb = *phi_inst_iter;
                                // 如果该前驱基础块不是现在的基础块
                                if(curr_bb != &bb){
                                    Value* curr_val = phi_inst.getIncomingValueForBlock(curr_bb);//得到该前驱基础块对应在phi指令中的变量
                                    live_out[&bb].erase(curr_val);//删除之
                                }
                            }
                        }
                    }

                    //计算IN[B]
                    live_in[&bb].insert(useB[&bb].begin(), useB[&bb].end());//把use集合的值全部加入
                    for (auto v = live_out[&bb].begin(); v != live_out[&bb].end(); v++) {//对OUT[B]中每个值，若其不在defB中，则加入IN[B]
                        if (defB[&bb].find(*v) == defB[&bb].end()) {
                            live_in[&bb].insert(*v);
                        }
                    } 
                    //看IN[B]在迭代前后是否发生变化,互相判断，若A中某值在B中找不到，说明变化了
                    for (auto v = live_in[&bb].begin(); v != live_in[&bb].end(); v++) {
                        if (tmpIn.find(*v) == tmpIn.end()) {
                            change = true;
                        }
                    }
                    for (auto v = tmpIn.begin(); v != tmpIn.end(); v++) {
                        if (live_in[&bb].find(*v) == live_in[&bb].end()) {
                            change = true;
                        }
                    }
                }
            }

            printActiveVars(F);
            return true;
        }
                        
                        
        void printActiveVars(llvm::Function & F){
            llvm::outs()<<"function  "<<F.getName()<<": \n";
            llvm::outs()<<"active vars : \nIN:\n";
            for (BasicBlock &bb : F) {
                llvm::outs()<<"label ";
                bb.printAsOperand(llvm::outs(),false);
                llvm::outs()<<" ";
                llvm::outs()<<"[ ";
                for (std::set<Value*>::iterator v = live_in[&bb].begin(); v != live_in[&bb].end(); v++) {
                    (*v)->printAsOperand(llvm::outs(),false);
                    llvm::outs()<<" , ";
                }
                llvm::outs()<<" ]\n";
            }
            llvm::outs()<<"\nOUT:\n";
            for (BasicBlock &bb : F) {
                llvm::outs()<<"label ";
                bb.printAsOperand(llvm::outs(),false);
                llvm::outs()<<" ";
                llvm::outs()<<"[ ";
                for (std::set<Value*>::iterator v = live_out[&bb].begin(); v != live_out[&bb].end(); v++) {
                    (*v)->printAsOperand(llvm::outs(),false);
                    llvm::outs()<<" , ";
                }
                llvm::outs()<<" ]\n";
            }
        }
        
        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
        }
    };
}




char Liveness::ID = 0;
INITIALIZE_PASS(Liveness, "liveness", "My Liveness Pass", false, false)
FunctionPass * llvm::createLivenessPass() {
    return new Liveness();
}