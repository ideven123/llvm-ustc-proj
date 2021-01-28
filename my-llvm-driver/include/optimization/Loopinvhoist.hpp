#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/IteratedDominanceFrontier.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>
#include <set>
#include <iostream>
#include <unordered_set>
#include <fstream>
#include<unordered_map>
using namespace llvm;

#define DEBUG_TYPE "loop-search"
/***************************************************************************/
// new_define_pass  , LoopInvHoist.

namespace llvm {
  FunctionPass * createLoopInvHoistPass();
  void initializeLoopInvHoistPassPass(PassRegistry&);
}

namespace {

struct LoopInvHoistPass : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid

    LoopInvHoistPass() : FunctionPass(ID) {
    initializeLoopInvHoistPassPass(*PassRegistry::getPassRegistry());
  }
struct CFGNode
{
    std::unordered_set<CFGNode *> succs;
    std::unordered_set<CFGNode *> prevs;
    BasicBlock *bb;
    int index;   // the index of the node in CFG
    int lowlink; // the min index of the node in the strongly connected componets
    bool onStack;
};

using CFGNodePtr = CFGNode*;
using CFGNodePtrSet = std::unordered_set<CFGNode*>;
using BBset_t = std::unordered_set<BasicBlock *>;
    int index_count;
    std::vector<CFGNodePtr> stack;
    // loops found
    std::unordered_set<BBset_t *> loop_set;
    // loops found in a function
    std::unordered_map<Function *, std::unordered_set<BBset_t *>> func2loop;
    // { entry bb of loop : loop }
    std::unordered_map<BasicBlock *, BBset_t *> base2loop;
    // { loop : entry bb of loop }
    std::unordered_map<BBset_t *, BasicBlock *> loop2base;
    // { bb :  entry bb of loop} 默认最低层次的loop
    std::unordered_map<BasicBlock *, BasicBlock *> bb2base;
void build_cfg(Function &func, std::unordered_set<CFGNode *> &result){
    
    std::unordered_map<BasicBlock *, CFGNode *> bbtocfg_node;

    for (BasicBlock &bb : func)
    {
        auto node = new CFGNode;
        node->bb = &bb;
        node->index = node->lowlink = -1;
        node->onStack = false;
        bbtocfg_node.insert({&bb, node});
        result.insert(node);
    }
    for (BasicBlock &bb : func)
    {
        auto node = bbtocfg_node[&bb];
        std::string succ_string = "success node: ";
        for (BasicBlock *succ : successors(&bb) )  //
        {
          //  succ_string = succ_string + (std::string)succ->getName() + " ";
            CFGNodePtr CFGPtr;
            CFGPtr = bbtocfg_node[succ];
            node->succs.insert(CFGPtr);  //creat succs list.   //workout the succ_begin 
        }
        std::string prev_string = "previous node: ";
        for (BasicBlock *prev : predecessors(&bb))
        {
          //  prev_string = prev_string + (std::string)prev->getName() + " ";
          //llvm::outs() <<"get CFGPtr"<< "\n";
            node->prevs.insert(bbtocfg_node[prev]);
        }
    }
}
bool strongly_connected_components(CFGNodePtrSet &nodes,std::list<CFGNodePtrSet *> &result){
    index_count = 0;
    stack.clear();
    for (auto n : nodes)
    {
        if (n->index == -1)  // not travere yet
            traverse(n, result);
    }
    return result.size() != 0;
}
void traverse(CFGNodePtr n,std::list<CFGNodePtrSet *> &result)
{
    n->index = index_count++;
    n->lowlink = n->index;
    stack.push_back(n);
    n->onStack = true;
    for (auto su : n->succs)
    {
        // has not visited su
        if (su->index == -1)
        {
            traverse(su, result);
            n->lowlink = std::min(su->lowlink, n->lowlink);
        }
        // has visited su
        else if (su->onStack)
        {
            n->lowlink = std::min(su->index, n->lowlink);
        }
    }
    // nodes that in the same strongly connected component will be popped out of stack
    if (n->index == n->lowlink)
    {
        auto set = new CFGNodePtrSet;
        CFGNodePtr tmp;
        do
        {
            tmp = stack.back();
            tmp->onStack = false;
            set->insert(tmp);
            stack.pop_back();
        } while (tmp != n);
        if (set->size() == 1)
            delete set;
        else
            result.push_front(set);
    }
}

CFGNodePtr find_loop_base(CFGNodePtrSet *set,CFGNodePtrSet &reserved){
    CFGNodePtr base = nullptr;
    bool hadBeen = false;
    for (auto n : *set)
    {
        for (auto prev : n->prevs)
        {
            if (set->find(prev) == set->end())
            {
                base = n;
            }
        }
    }
    if (base != nullptr)
        return base;
    for (auto res : reserved)
    {
        for (auto succ : res->succs)
        {
            if (set->find(succ) != set->end())
            {
                base = succ;
            }
        }
    }
    return base;
}

void get_loop_information(Function &func)
{
            CFGNodePtrSet nodes;
            CFGNodePtrSet reserved;
            std::list<CFGNodePtrSet *> sccs;
            
            // step 1: build cfg
            build_cfg(func, nodes);

            // step 2: find strongly connected graph from external to internal
            int scc_index = 0;
            while (strongly_connected_components(nodes, sccs))
            {
                if (sccs.size() == 0)
                {       
                    break;
                }
                else
                {
                    // step 3: find loop base node for each strongly connected graph
                    for (auto scc : sccs)
                    {
                        scc_index += 1;
                        auto base = find_loop_base(scc, reserved);
                        // step 4: store result
                        auto bb_set = new BBset_t;
                        for (auto n : *scc)
                        {
                            bb_set->insert(n->bb);
                        }
                        loop_set.insert(bb_set);
                        func2loop[&func].insert(bb_set);
                        base2loop.insert({base->bb, bb_set});
                        loop2base.insert({bb_set, base->bb});
                        for (auto bb : *bb_set)
                        {
                            if (bb2base.find(bb) == bb2base.end())
                            {
                                bb2base.insert({bb, base->bb});
                            }
                            else
                            {
                                bb2base[bb] = base->bb;
                            }
                        }
                        // step 5: map each node to loop base
                        for (auto bb : *bb_set)
                        {
                            if (bb2base.find(bb) == bb2base.end())
                                bb2base.insert({bb, base->bb});
                            else
                                bb2base[bb] = base->bb;
                        }

                        // step 6: remove loop base node for researching inner loop
                        reserved.insert(base);
                        nodes.erase(base);
                        for (auto su : base->succs)
                        {
                            su->prevs.erase(base);
                        }
                        for (auto prev : base->prevs)
                        {
                            prev->succs.erase(base);
                        }

                    } // for (auto scc : sccs)
                    for (auto scc : sccs)
                        delete scc;
                    sccs.clear();
                    for (auto n : nodes)
                    {
                        n->index = n->lowlink = -1;
                        n->onStack = false;
                    }
                } // else
            }     // while (strongly_connected_components(nodes, sccs))
            // clear
            reserved.clear();
            for (auto node : nodes)
            {
                delete node;
            }
            nodes.clear();
}

BBset_t *get_inner_loop(BasicBlock* bb){
        if(bb2base.find(bb) == bb2base.end())
            return nullptr;
        return base2loop[bb2base[bb]];
}

BBset_t *get_parent_loop(BBset_t *loop)
{
    auto base = loop2base[loop];
    for ( auto prev : predecessors(base))    ////////////////////////qian qu
    {
        if (loop->find(prev) != loop->end())
            continue;
        auto loop = get_inner_loop(prev);
        if (loop == nullptr || loop->find(base) == loop->end())
            return nullptr;
        else
        {
            return loop;
        }
    }
    return nullptr;
}

BasicBlock* get_loop_base(BBset_t *loop) { return loop2base[loop]; }
std::unordered_set<BBset_t *> get_loops_in_func(Function *f)
{
    return func2loop.count(f) ? func2loop[f] : std::unordered_set<BBset_t *>();
}

void loopinvhoist_run(Function &func)
{   
    // 先通过LoopSearch获取循环的相关信息
    get_loop_information( func)  ;
    std::unordered_map<BasicBlock *,std::unordered_set<BBset_t *>> bb2loop;
    std::unordered_map<Value *,std::unordered_set<BBset_t *>> var2loop;
    std::vector<Instruction *> wait_delete;
    //找到每个循环的定值集合
        for(auto loop : get_loops_in_func(&func)){    //实现一个  get_loops_in_func()
            for(auto bb: *loop){
                for(Instruction &instr: *bb){         // iterate every instr over bb    
                    //printf("bb\n");
                    if(!instr.isTerminator()){
                        auto var= dynamic_cast<Value *>(&instr);    //最左边的参数
                        var2loop[var].insert(loop);
                    }
                }
                bb2loop[bb].insert(loop);   //得到每个bb块所在的循环集合
            }
        }
    
    printf("------\n");
    //查找循环不变式并修改：   
        for(auto loop : get_loops_in_func(&func)){       //还是 get_loops_in_func
            printf("loop begin\n");
            for(auto bb: *loop){
                for (BasicBlock::iterator iter = bb->begin(); iter != bb->end(); iter++){
                     Instruction &instr = *iter;
                    if( !instr.isTerminator()){             //不是最后一条指令
                        int flag=0;
                        if(instr.getOpcode() == Instruction::PHI){                                //   该指令是不是 phi
                            flag=1;
                        }
                        else{                          
                            for (auto iter = instr.op_begin(); !flag && iter != instr.op_end(); ++iter){           //取操作数 //////////////////////              
                                Value *var = dyn_cast<Value>(*iter);
                                assert(var != NULL);
                                if( !isa<Constant>(var)&& var2loop.find(var)!=var2loop.end()&&(var2loop[var].find(loop)!=var2loop[var].end())){
                                //说明该指令使用的变量是在循环里面定值的
                                    flag=1;
                                    break;
                                }
                            }
                        }
                        if(flag==0){
                            //该指令是循环不变式
                            auto base= get_loop_base(loop);             //返回循环的入口
                            for(auto prev: predecessors(base)){         //  入口的前驱
                                if(loop->find(prev)==loop->end()){      //找到循环的 前驱 块  
                                    //找到循环前驱块
                                    Value *var= dyn_cast<Value>(&instr);
                                    if( get_parent_loop(loop)==nullptr)        
                                        var2loop.erase(var);
                                    else{
                                        var2loop[var]=bb2loop[prev];
                                    }
                                    instr.moveBefore(prev->getTerminator());
                                    /*if(prev->getTerminator()==nullptr)  
                                        instr.moveBefore(prev->getTerminator()) ;
                                        prev->add_instruction(instr); //添加指令
                                    else{
                                        //如果末尾有跳转指令
                                        auto endinstr=prev->get_terminator();
                                        prev->delete_instr(endinstr);                     //  添加指令
                                        prev->add_instruction(instr);
                                        prev->add_instruction(endinstr);
                                    }
                                    */
                                     wait_delete.push_back(&instr);
                                     outs() <<"wai ti zhi ling " <<instr.getOpcodeName()<< instr << "\n";
                                    //此时获得的值也可以在循环外面定值
                                    break;
                                }
                            }
                        }
                    }
                    else{
                        break;
                    }
                }
                //访问完一个bb块后删掉多余的指令：
             /*   for ( auto instr : wait_delete)
                {
                    bb->delete_instr(instr);
                }
              */  
                wait_delete.clear(); //清空
            }
        }
    //修改指令：
}
  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    llvm::outs()<<"Processing function  "<<F.getName()<<","<<"  LoopInvHoist result is as follow \n{\n";
    loopinvhoist_run(F) ;
    llvm::outs()<<"}\n" ;
    return true;
  }
};
} // end anonymous namespace
char LoopInvHoistPass::ID = 0;
INITIALIZE_PASS(LoopInvHoistPass, "LoopInvHoistPass", "LoopInvHoist", false, false)

FunctionPass *llvm::createLoopInvHoistPass() { return new LoopInvHoistPass(); }

