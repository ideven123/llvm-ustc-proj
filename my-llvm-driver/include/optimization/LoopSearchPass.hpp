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
using namespace llvm;

#define DEBUG_TYPE "loop-search"
// STATISTIC(LoopSearched, "Number of loops has been found");

namespace llvm {
  FunctionPass * createLSPass();
  void initializeLSPassPass(PassRegistry&);
}

namespace {

struct LSPass : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid

  LSPass() : FunctionPass(ID) {
    initializeLSPassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    size_t BackEdgeNum = 0;
    DominatorTree DT(F);
    PostDominatorTree PDT(F);
    std::vector<DomTreeNode*> WorkList;
    WorkList.push_back(DT.getRootNode());
    while(!WorkList.empty()){
      auto CurNode = WorkList.back();
      WorkList.pop_back();
      auto BB = CurNode->getBlock();
      for(auto sSucc = succ_begin(BB), eSucc = succ_end(BB);sSucc != eSucc; ++sSucc){
        auto SuccNode = DT.getNode(*sSucc);
        if(DT.dominates(SuccNode, CurNode)){
          BackEdgeNum++;
        }
      }
      WorkList.insert(WorkList.end(), CurNode->begin(), CurNode->end());
    }
    llvm::outs() << "Processing function " << F.getName() << ", number of Backedges is " << BackEdgeNum << "\n";
    std::error_code err;
    raw_fd_ostream outfile_dt(StringRef(F.getName().str() + "_dt.txt"), err);
    raw_fd_ostream outfile_pdt(StringRef(F.getName().str() + "_pdt.txt"), err);
    DT.print(outfile_dt);
    PDT.print(outfile_pdt);
    return true;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.setPreservesCFG();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addPreserved<PostDominatorTreeWrapperPass>();
    AU.addPreserved<GlobalsAAWrapperPass>();
  }
};

} // end anonymous namespace

char LSPass::ID = 0;
INITIALIZE_PASS(LSPass, "LSPass", "Loop search", false, false)

FunctionPass *llvm::createLSPass() { return new LSPass(); }

/***************************************************************************/
// new_define_pass

namespace llvm {
  FunctionPass * createLoopSearchPass();
  void initializeLoopSearchPassPass(PassRegistry&);
}

namespace {

struct LoopSearchPass : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid

    LoopSearchPass() : FunctionPass(ID) {
    initializeLoopSearchPassPass(*PassRegistry::getPassRegistry());
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

void build_cfg(Function &func, std::unordered_set<CFGNode *> &result){
    std::unordered_map<BasicBlock *, CFGNode *> bb2cfg_node;
    for (BasicBlock &bb : func)
    {
        auto node = new CFGNode;
        node->bb = &bb;
        node->index = node->lowlink = -1;
        node->onStack = false;
        bb2cfg_node.insert({&bb, node});
        result.insert(node);
    }
    for (BasicBlock &bb : func)
    {
        auto node = bb2cfg_node[&bb];
        std::string succ_string = "success node: ";
        for (BasicBlock *succ : successors(&bb) )  //
        {
          //  succ_string = succ_string + (std::string)succ->getName() + " ";
            CFGNodePtr CFGPtr;
            CFGPtr = bb2cfg_node[succ];
            node->succs.insert(CFGPtr);  //creat succs list.   //workout the succ_begin 
        }
        std::string prev_string = "previous node: ";
        for (BasicBlock *prev : predecessors(&bb))
        {
          //  prev_string = prev_string + (std::string)prev->getName() + " ";
          //llvm::outs() <<"get CFGPtr"<< "\n";
            node->prevs.insert(bb2cfg_node[prev]);
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

void print_depth(int depth,std::vector<int> ith){
    for(int i = 0 ; i <depth ;i++){
        llvm::outs()<< "\t";
    }  
    llvm::outs()<<"L";      
    for(int i=0 ;i< depth ;i++){
        llvm::outs()<<ith[i] ;
    }
    llvm::outs()<<"  depth:" <<depth <<"\n";
}
void rec_analyse_loop(CFGNodePtrSet &result,CFGNodePtrSet reserved,int depth,std::vector<int> ith){
    std::list<CFGNodePtrSet *> sccs;
    if(strongly_connected_components(result,sccs)){
         depth++ ;
         int this_ith = 1;
         ith.insert(ith.begin()+depth-1,0);
        for(auto scc: sccs){
            ith[depth-1] = this_ith++ ;
            print_depth(depth,ith);
            auto base = find_loop_base(scc, reserved);
            reserved.insert(base);
            scc->erase(base);
            for (auto su : base->succs)
            {
             su->prevs.erase(base);
            }
            for (auto prev : base->prevs)
            {
             prev->succs.erase(base);
            }
            for (auto node : *scc){
                node->index = node->lowlink = -1;
                node->onStack = false;
                
                // node->prevs.clear();
                // node->succs.clear();
            }
            rec_analyse_loop( *scc, reserved,depth,ith) ;
        }
    }else
    {
        return ;
    }
    for (auto scc : sccs)
        delete scc;
    sccs.clear();

}

 void analyseloop(Function &func,CFGNodePtrSet &result){
      build_cfg(func,result);
      std::list<CFGNodePtrSet *> sccs;
      CFGNodePtrSet reserved;
    
      std::vector<int> ith;
      int depth = 0 ;
      //以下注释掉部分是，中间过程打印控制流图的代码。
      /*     strongly_connected_components(result,sccs);
      // print CFG graph (stupid)
      for (auto scc : result){
          llvm::outs()<< scc->index<< "prev: " ;
          for (auto prev : scc->prevs){
               llvm::outs()<< prev->index<<"  " ;
          }
          llvm::outs()<< "\n";
          llvm::outs()<< scc->index<< " succ : " ;
          for (auto succ : scc->succs){
              llvm::outs()<< succ->index<<"  " ;
          }
          llvm::outs()<< "\n";
      }
      // print SCC information
      for (auto scc : sccs){
          auto base = find_loop_base(scc, reserved);
           llvm::outs()<< base->index<<":" ;
         for (auto cfn : *scc){
             llvm::outs()<< cfn->index<<" " ;
         }
         llvm::outs()<< "\n" ;
      }
      //
 */      
      rec_analyse_loop( result, reserved,depth,ith) ;
 }
  bool runOnFunction(Function &F) override {
    if (skipFunction(F))
      return false;
    CFGNodePtrSet nodes;
    CFGNodePtrSet reserved;
    llvm::outs()<<"Processing function  "<<F.getName()<<","<<" loop message is as follow \n{\n";
    analyseloop(F, nodes);
    llvm::outs()<<"}\n" ;
    return true;
  }
  
};
} // end anonymous namespace
char LoopSearchPass::ID = 0;
INITIALIZE_PASS(LoopSearchPass, "LoopSearchPass", "LoopSearch", false, false)

FunctionPass *llvm::createLoopSearchPass() { return new LoopSearchPass(); }

