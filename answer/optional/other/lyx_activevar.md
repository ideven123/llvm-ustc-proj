### 活跃变量分析思路

* class Liveness: 选择继承FunctionPass，是针对每一个函数的基本块作分析。

* 存储：使用`std::map<BasicBlock *, std::set<Value *>>`来存放每个块的IN[B],OUT[B],$def_B, use_B$, 一方面方便与基本块对应，另一方面set的特性使得集合元素互异

* 主要函数为runOnFunction

  * 首先初始化函数所有的基本块的def和use集合

    * 遍历每个基本块的每条指令的每个右操作数

      * 若不是常量、不是label、不是函数名、同时在该基本块该指令前无定值，则加入use集合

    * 对每条指令的左值，若指令不是跳转或返回指令`!instr.isTerminator()`则加入def集合。

      当然，用到tmpDef集合来记录基本块里已经分析到的定值变量。

  * 然后开始按书上算法迭代计算

    * IN、OUT集时，只需对对应的live_in, live_out里的一项set作插入删除操作即可

    * 需重点注意phi结点的特殊处理，因为若块1(有变量x1)和块2（有变量x2）的后继都是块3，并且在块3里有指令形如`%11 = phi i32 [ %x1, %1 ], [ %x2, %2 ] `，那么在计算块1的OUT集时，由于x2存在于块3的IN集中，所以需要剔除掉。

      ```c++
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
      ```

* 在对一条指令的操作数遍历时，要注意

  * `for(auto iter = instr.op_begin(); iter != instr.op_end(); iter++)`

    * 有等号的指令：会依次遍历该指令等号右边的操作数； 无等号指令：全部操作数

      包括

      * 常量

      * 变量，label（IR里均会打上%）

        如`br label %return`的return ；`ret i32 %retval.0`中的retval.0

      * 对于函数调用等情况

        `%call = call i32 @gcd(i32 %v, i32 %sub)` ,会依次看 v,sub, gcd (即先局部的% ,再全局的@)

      * 特别的，对于phi指令：不会遍历标号

        `%retval.0 = phi i32 [ %u, %if.then ], [ %call, %if.else ]`只有u,call

  * 对于instr本身，也代表了等式左边操作数(若不是等式的指令，则`instr.printAsOperand(llvm::outs(),false);`输出为`<badref>`

* 遇到的问题：参见我发的issue

* 对Value类型(Instruction, BasicBlock, Value)的打印：`llvm::outs()<<v->getName();`或者`bb.printAsOperand(llvm::outs(),false);`,参考llvm手册说明，getName()有时会返回空串(我的机器上返回"",服务器上有时会返回llvm ir的变量名)

* 验证文件：`./tests/activeVar[1-4].c`

  这里以`./tests/activeVar1.c`为例

  输出的llvm ir （-show-ir-after-pass命令后输出在./build/Promote_Memory_to_Register_0.ll）

  ```
  define dso_local i32 @gcd(i32 %0, i32 %1) #0 {
    %3 = icmp eq i32 %1, 0
    br i1 %3, label %4, label %5
  
  4:                                                ; preds = %2
    br label %10
  
  5:                                                ; preds = %2
    %6 = sdiv i32 %0, %1
    %7 = mul nsw i32 %6, %1
    %8 = sub nsw i32 %0, %7
    %9 = call i32 @gcd(i32 %1, i32 %8)
    br label %10
  
  10:                                               ; preds = %5, %4
    %11 = phi i32 [ %0, %4 ], [ %9, %5 ]
    ret i32 %11
  }
  
  ; Function Attrs: noinline nounwind uwtable
  define dso_local void @main() #0 {
    %1 = icmp slt i32 7, 8
    br i1 %1, label %2, label %3
  
  2:                                                ; preds = %0
    br label %3
  
  3:                                                ; preds = %2, %0
    %4 = phi i32 [ 7, %2 ], [ 8, %0 ]
    %5 = phi i32 [ 8, %2 ], [ 7, %0 ]
    %6 = call i32 @gcd(i32 %5, i32 %4)
    %7 = call i32 (i32, ...) bitcast (i32 (...)* @output to i32 (i32, ...)*)(i32 %6)
    ret void
  }
  ```

  找到的活跃变量：

  ```
  function  gcd: 
  active vars : 
  IN:
  label %2 [ %0 , %1 ,  ]
  label %4 [ %0 ,  ]
  label %5 [ %0 , %1 ,  ]
  label %10 [ %0 , %9 ,  ]
  
  OUT:
  label %2 [ %0 , %1 ,  ]
  label %4 [ %0 ,  ]
  label %5 [ %9 ,  ]
  label %10 [  ]
  ```

  ```
  function  main: 
  active vars : 
  IN:
  label %0 [  ]
  label %2 [  ]
  label %3 [  ]
  
  OUT:
  label %0 [  ]
  label %2 [  ]
  label %3 [  ]
  ```

  



中文手册：

https://blog.csdn.net/qq_23599965/article/details/88538590