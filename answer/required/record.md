[toc]

## 实验文档

### llvm11.0.0编译

在[官网][ https://releases.llvm.org/download.html ]下载`llvm-11.0.0.src.tar.xz`  和`clang-11.0.0.src.tar.xz` .

在指定的文件夹：

```shell
# 解压缩
tar xvf llvm-11.0.0.src.tar.xz
mv llvm-11.0.0.src llvm
tar xvf clang-11.0.0.src.tar.xz
mv clang-11.0.0.src llvm/tools/clang
```

编译

```shell
mkdir llvm-build && cd llvm-build
# Release
cmake ../llvm -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`cd .. && pwd`/llvm-install
# Debug
cmake ../llvm -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=`cd .. && pwd`/llvm-install
# 编译安装,下面的N表示的是你采取的同时执行的编译任务的数目.你需要将其替换为具体的数值,如8,4,1
# 如果内存充足,一般推荐将N设置为cpu核数的2倍,如果未指明N,将会尽可能多地并行编译
make -jN install
# 这一过程可能有半小时至两小时不等,如果内存不足,可以尝试减小N并重新make,已经编译好的部分不会重新编译
```

环境变量配置///Users/zhangwanlin/workspace/POC_lab/LLVM11/llvm-install

```bash
cd ~
vi .bashrc
------------在底部增加----------
export  C_INCLUDE_PATH=the_path_to_your_llvm-install_bin/include:$C_INCLUDE_PATH
export  CPLUS_INCLUDE_PATH=the_path_to_your_llvm-install_bin/include:$CPLUS_INCLUDE_PATH
export  PATH=the_path_to_your_llvm-install_bin/bin:$PATH
-------------------------------
source .bashrc  
关闭并再次打开终端

# 最后,检查PATH配置是否成功,执行命令:
llvm-config --version
# 成功标志:
11.0.0 #或类似
# 失败标志:
zsh: command not found: llvm-config #或类似
```



### 项目结构 理解



+ main函数实例化一个driver 。

+ driver 的方法中 包含了以下

  + `ParseArgs` 参数解析 ，

  + `BuildCI`没看明白 ，

  + `FrontendCodeGen`  主要是使用clang 编译工具生成较初期的中间代码,(我们应该不需要动)

  + `InitializePasses`  关键初始化`pass maneger` ,一个pass的管理模块，pass是在每一遍pass是进行分析优化的过程。（我们应该不需要动）

  + `passadd`  , 就是把pass 加到`pass maneger`管理器里，管理器可以自动调用我们加进去的Pass，所以我们只需要写自己的Pass。
  + ....

+ 我们的工作相当于扩展driver的功能 ， 具体来说是扩展driver的 Pass 。

+ 助教已经给出几种pass，分别有，，， 。我们的工作与这几个Pass是并列关系。（我们的分析器可能就是再加一个pass ,使他具有分析循环的能力 ）,参考lc 班的实现逻辑。



### 写自己的Pass 

pass 要最好写在optimization 文件夹下。

模版：

```c++
namespace llvm {
    ModulePass * createmyToyPass();
    void initializemyToyPassPass(PassRegistry&);
}

namespace {
    class myToyPass : public ModulePass {
        public:
            static char ID;
            myToyPass() : ModulePass(ID) {
                initializemyToyPassPass(*PassRegistry::getPassRegistry());
            }
            bool runOnModule(Module& M) override {
      			// 如果是继承FunctionPass，这里的runOnModule函数需要替换成
             
              //这个函数 才是我们主要的工作 , 
              
              
      			// bool runOnFunction(Function& F) override {
              
            }
    };
}   

char myToyPass::ID = 0;
INITIALIZE_PASS(myToyPass, "mytoy", "My Toy Pass", false, false)
ModulePass *llvm::createmyToyPass() {
    return new myToyPass();
}
```

该模版是针对 Moudle的Pass 创建过程 ，针对Function的Pass ,类似 。可参考助教例子`MyPasses.hpp` 文件

### 必做——分析循环Pass实现

数据结构

```c++
struct CFGNode  //控制流图的结点
{
    std::unordered_set<CFGNode *> succs;  //后继结点集合
    std::unordered_set<CFGNode *> prevs;  //前驱结点集合
    BasicBlock *bb;                //基本快
    int index;   // the index of the node in CFG
    int lowlink; // the min index of the node in the strongly connected componets //一个用于计算强联通分量的量 。
    bool onStack; //用于计算强连通分量
};

using CFGNodePtr = CFGNode*;  
using CFGNodePtrSet = std::set<CFGNode*>;  // 可以 存储强连通分量的结点
```

算法：

+ 首先创建控制流图 。 `void build_cfg(Function &func, std::set<CFGNode *> &result)`  传入Function ，和存放结果的Set :: result 。

+ 分解强连通分量 `bool strongly_connected_components(CFGNodePtrSet &nodes,std::set<CFGNodePtrSet *> &result)`  , 传入控制流图 ，和存放结果(**结果为 若干个极大强连通分量**)的 一个数据结构。返回是否存在强连通分量即（是否有循化）。

+ 根据分解出的强连通分量，递归的分析循环 `void rec_analyse_loop(CFGNodePtrSet &result,CFGNodePtrSet reserved,int depth)`  第一个参数是，控制流图，第二个没什么用(可以存循环的base 结点) 。第三个为深度 ，便于递归的时候 记录当前层数。重点解释：

  ```c++
  void rec_analyse_loop(CFGNodePtrSet &result,CFGNodePtrSet reserved,int depth){
      std::set<CFGNodePtrSet *> sccs;
      if(strongly_connected_components(result,sccs)){  //如果有强连通分量
           depth++ ;
          for(auto scc: sccs){    // 对每一个强连通分量迭代
              auto base = find_loop_base(scc, reserved);  //返回该分量的 base 结点（循环入口）
              print_depth_space(depth);       
              llvm::outs()<< base->index <<" depth:";
              llvm::outs()<< depth <<"\n";               //简单打印，可以更秀
              reserved.insert(base);        //保存base  
              scc->erase(base);             //在当前的强连通分量里 删去base ，为了分析内层循环
              for (auto su : base->succs)
              {
               su->prevs.erase(base);
              }
              for (auto prev : base->prevs)
              {
               prev->succs.erase(base);
              }                             //至此是 ，删去 base的入边和出边。
              for (auto node : scc){        //还原 ，CFG结点 ，为了递归分析
                  node->index = node->lowlink = -1;
                  node->onStack = false;       
                  // node->prevs.clear();
                  // node->succs.clear();
              }
              rec_analyse_loop( *scc, reserved,depth) ;  //递归进入
          }
      }else
      {
          return ;  //无强联通分量
      }
      for (auto scc : sccs)  //释放内存
          delete scc;
      sccs.clear();
  
  }
  ```

  

### 目前的问题

当前的一个输出：

```c++
    14 depth:1
    12 depth:1
            2 depth:2
    8 depth:1
            3 depth:2
                    0 depth:3
            5 depth:2
                    1 depth:3
    10 depth:1
            1 depth:2
```

+ 目前是用base结点序号给 循环命名(已有不是很好的解决)。

+ 并列的循环的顺序可能不是按照原文件的顺序输出（重点）。

  分析可能原因：

  1.  在function中迭代BasicBlock ,  `for (BasicBlock &bb : func)`  .时，没有按照从根结点开始 的顺序 。

  2.  更可能是 ， 在分解强连通分量时，没有按照希望的顺序，往容器里加。
  3.  不太可能， 容器中迭代 单个强连通分量时，不是按顺序迭代。

  .......

  经查资料好像是，set容器自身的原因。 （已解决，采用list 容器，而不是unorder_set）。
  
+ 希望有结构性更好的输出方法。

