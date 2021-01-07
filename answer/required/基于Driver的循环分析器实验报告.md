# 基于[Driver](https://gitee.com/s4plus/llvm-ustc-proj/blob/master/my-llvm-driver/include/Driver/driver.h#L23)的循环分析器实验报告

## 实验要求

实现一个基于driver的分析器，使其能够 统计给定[Function](https://github.com/llvm/llvm-project/blob/llvmorg-11.0.0/llvm/include/llvm/IR/Function.h#L61)中的循环信息：

- 并列循环的数量
- 每个循环的深度信息

## 算法实现

> 核心思想

将控制流图分解为若干个极大强连通分量，每个强连通分量代表一个外层循环（包含内部的嵌套循环）。对每个极大强连通分量，找到他的base结点，并从控制流图上删去，相当于脱去一层循环。然后对剩下的结点，递归地分解强连通分量，可得内层循环信息 。

> 具体实现

**数据结构：**

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

**算法：**

+ 首先创建控制流图 。 `void build_cfg(Function &func, std::set<CFGNode *> &result)`  传入`Function` . 根据`BasicBlocks`  间的继承关系，创建控制流图，维护先驱后继结点 。

+ 分解强连通分量 `bool strongly_connected_components(CFGNodePtrSet &nodes,std::set<CFGNodePtrSet *> &result)`  , 传入控制流图 。分解强连通分量，用的是Tarjin 算法。该算法的时间复杂度较好，只有O(V+E) 。

+ 根据分解出的强连通分量，递归的分析循环 `void rec_analyse_loop(CFGNodePtrSet &result,CFGNodePtrSet reserved,int depth，std::vector<int> ith)`  第一个参数是，控制流图，第二个可用于扩展，可以存循环的base 结点 。第三个为深度 ，便于递归的时候记录当前层数,第四个ith ,记录当前循环是，当前层的第几个循环。重点解释：

  ```c++
  void rec_analyse_loop(CFGNodePtrSet &result,CFGNodePtrSet reserved,int depth,std::vector<int> ith){
      std::list<CFGNodePtrSet *> sccs;
      if(strongly_connected_components(result,sccs)){  //如果有强连通分量
           depth++ ;                   //递归一次，深度加一
           int this_ith = 1;           //并列数初始化为1 ，
           ith.insert(ith.begin()+depth-1,0);   // 并列次序插入容器
          for(auto scc: sccs){    // 对每一个强连通分量迭代 ，一个强连通分量代表一个并列的 循环
              ith[depth-1] = this_ith++ ;   //并列数加一
              print_depth_space(depth,ith);       //打印当前的深度，和并列关系
              auto base = find_loop_base(scc, reserved);  //返回该分量的 base 结点（循环入口）
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
              rec_analyse_loop( *scc, reserved,depth,ith) ;  //递归进入
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

  

## 测试样例

1. 对`loop_z1.c` 是一个多个函数的例子

   ```c
   int main(){
       int num = 0 ,i,j,k;
       for(int i=0;i<10;i++){
           for(int j=i;j<10;j++){
               num++;
           }
       }
       for(int j=i;j<10;j++){
           for(int k=j;k<10;k++){
   			num++;
           }
       }
   
       return num;
   }
   int fun(){
       int num;
       while(1){
           for(int i = 0;i<3 ; i++){
               if(num == i){
                   for(int j=i;j<10;j++){
                       for(int k=j;k<10;k++){
   			             num++;
                       }
                    }
               }
               else
               {
                   int j;
                   for(int k=j;k<10;k++){
   			             num++;
                   }   
               }
           }
       }
   }
   ```

   分析结果的输出如下：

   ```c
   Processing function  main, loop message is as follow 
   {
           L1  depth:1
                   L11  depth:2
           L2  depth:1
                   L21  depth:2
   }
   Processing function  fun, loop message is as follow 
   {
           L1  depth:1
                   L11  depth:2
                           L111  depth:3
                                   L1111  depth:4
                           L112  depth:3
   }
   ```

2. 对`loop.c` ，是一个单个函数的例子

   ```c
   int main(){
       int num = 0 ,i,j,k;
       for(int i=0;i<10;i++){
           for(int j=i;j<10;j++){
               num++;
           }
       }
       for(int j=i;j<10;j++){
               for(int k=j;k<10;k++){
   				num++;
               }
           }
       for(int k=j;k<10;k++){
   				num++;
        } 
       for(int i=0;i<10;i++){
           for(int j=i;j<10;j++){
               for(int k=j;k<10;k++){
   				num++;
               }
           }
           for(int j=i;j<10;j++){
               for(int k=j;k<10;k++){
   				num++;
               }
           }
       }   
       return num;
   }
   ```

   分析结果的输出如下：

   ```c
   Processing function  main, loop message is as follow 
   {
           L1  depth:1
                   L11  depth:2
           L2  depth:1
                   L21  depth:2
           L3  depth:1
           L4  depth:1
                   L41  depth:2
                           L411  depth:3
                   L42  depth:2
                           L421  depth:3
   }
   ```

   

## 实验总结

1. 用强连通分量，分析循环是一种直观的方法，且采用Tarjin算法分解强连通分量的时间复杂度并不高只有O(V+E) 。但是在递归分析循环的时候，需要在去掉base结点后重新分解强连通分量，实则是一种浪费。
2. 本实验，在`BasicBlock` 层面的控制流图上 ，又包装了一层`CFGNode`，然后在`CFGNode` 的层面上再创建控制流图， 这是为了给 分解强连通分量提供一些信息 ，` int index;  `,`   int lowlink;`,`bool onStack; ` .
3. ~~从支配树上分析循环，或许会效率更高。~~