#include <iostream>
#include <ostream>
#include <vector>
#include <map>
#include <set>
#include <typeinfo>

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

using namespace llvm;

namespace llvm {
    FunctionPass * createGetAvailExpr();
    void initializeGetAvailExprPass(PassRegistry&);
}

namespace {

    class Expr{
        public:
            unsigned opcode;
            Value *loperand, *roperand, *inst;
            bool isUnary;

            Expr(Instruction *i){
                inst = i;
                opcode = i->getOpcode();
                loperand = i->getOperand(0);
                isUnary = (isa<llvm::UnaryOperator>(i));
                if (!isUnary) roperand = i->getOperand(1);
                else roperand = nullptr;
            }
            bool operator==(const Expr a){
                /***
                 * 只考虑表达式完全相同的情况，
                 * 不考虑由于交换律等数学性质相等，
                 * 如认为 a + b , b + a 不是相等的表达式。
                 ***/
                if (a.isUnary != this->isUnary) return false;
                if (a.isUnary)
                    return (a.opcode == this->opcode && a.loperand == this->loperand);
                else
                    return (a.opcode == this->opcode && a.loperand == this->loperand && a.roperand == this->roperand);
            }

            friend raw_ostream & operator<<(raw_ostream & outs, const Expr & expr);
    };

    raw_ostream & operator<<(raw_ostream & outs, const Expr & expr)
    {
        outs << "[" << Instruction::getOpcodeName(expr.opcode) << " ";
        expr.loperand->printAsOperand(outs, false); outs << ", ";
        if (!expr.isUnary)
            expr.roperand->printAsOperand(outs, false); outs << "]";
        return outs;
    }

    class GetAvailExpr : public FunctionPass {
        public:
            static char ID;
            GetAvailExpr() : FunctionPass(ID) {
                initializeGetAvailExprPass(*PassRegistry::getPassRegistry());
            }
            bool runOnFunction(Function &F) override {
                /***
                 * 考虑应用到公共子表达式删除：
                 * 将 all, in, out 作为这个类的成员字段以供使用，
                 * 将这个函数作为计算上述三个变量的方法，
                 * 然后另外写runOnFunction方法，将这个Pass扩展为公共子表达式删除Pass。
                 * 需要根据是否删除了公共子表达式设置返回值
                 ***/ 
                std::map<BasicBlock *, std::set<Expr *>> gen, kill;
                std::set<Expr *> all;
                std::vector<Value *> def;       // temp variable reused through loop
                std::map<BasicBlock *, BitVector> in, out, gen_e, kill_e;
                /***
                 * get gen, kill in each basic block
                 ***/
                for (BasicBlock &bb : F){
                    def.clear();
                    gen[&bb] = {};
                    kill[&bb] = {};
                    // get def, add expressions to "all"
                    for (Instruction &i : bb){
                        if (!isa<llvm::BinaryOperator>(i) && !isa<llvm::UnaryOperator>(i))
                            continue;
                        Expr *t = new Expr(&i);
                        //std::cout << *t << std::endl;
                        bool is_in = false;
                        for (Expr *each : all){
                            if (*each == *t){
                                is_in = true;
                                break;
                            }
                        }
                        if (!is_in) all.insert(t);
                        def.push_back(t->inst);
                    }
                    // get gen, kill
                    int def_index = -1;
                    bool redefed;
                    for (Instruction &i : bb){
                        if (!isa<llvm::BinaryOperator>(i) && !isa<llvm::UnaryOperator>(i))
                            continue;
                        Expr *t = new Expr(&i);
                        def_index++;
                        redefed = false;
                        for (Expr *expr : all){
                            if (*expr == *t){
                                for (int k = def_index; k < def.size(); k++){
                                    if (def[k] == expr->inst){
                                        redefed = true;
                                        break;
                                    }
                                }
                                if (!redefed)
                                    gen[&bb].insert(expr);
                                break;
                            }
                        }
                        for (auto expr : all){
                            if (&i == expr->loperand || &i == expr->roperand)
                                kill[&bb].insert(expr);
                        }
                    }
                }
                // change set<Value *> to BitVector
                int bit_vector_size = all.size();
                for (BasicBlock &bb : F) {
                    gen_e[&bb] = BitVector(bit_vector_size, false);
                    kill_e[&bb] = BitVector(bit_vector_size, false);
                    for (auto expr : gen[&bb]){
                        int i = -1;
                        for (auto find : all){
                            i++;
                            if (expr == find){
                                gen_e[&bb].set(i);
                                break;
                            }
                        }
                    }
                    for (auto expr : kill[&bb]){
                        int i = -1;
                        for (auto find : all){
                            i++;
                            if (expr == find){
                                kill_e[&bb].set(i);
                                break;
                            }
                        }
                    }
                }
                /***
                 * initialize out
                 ***/
                bool entry = true;
                for (BasicBlock &bb : F){
                    if (entry){
                        out[&bb] = BitVector(bit_vector_size, false);
                        entry = false;
                    }
                    else out[&bb] = BitVector(bit_vector_size, true);
                }
                /***
                 * compute in, out
                 * using BitVector
                 ***/
                bool changed = true;
                while (changed){
                    changed = false;
                    entry = true;
                    for (BasicBlock &bb : F){
                        if (entry){
                            entry = false;
                            continue;
                        }
                        BitVector tmp = BitVector(bit_vector_size, true);
                        auto *I = succ_begin(bb);
                        while (I != succ_end(bb)){
                            tmp = tmp | out[I];
                            I++;
                        }
                        in[&bb] = tmp;
                        tmp = (tmp - kill_e[&bb]) & gen_e[&bb];
                        if (tmp != out[&bb]){
                            changed = true;
                            out[&bb] = tmp;
                        }
                    }
                }
                /***
                 * output available expression in each basic block
                 * not clear output
                 * TODO: how to use llvm::raw_ostream
                 ***/

                return false;
            }
    };
}

char GetAvailExpr::ID = 0;
INITIALIZE_PASS(GetAvailExpr, "availexpr", "get available expression", false, false)
FunctionPass * llvm::createGetAvailExpr() {
    return new GetAvailExpr();
}