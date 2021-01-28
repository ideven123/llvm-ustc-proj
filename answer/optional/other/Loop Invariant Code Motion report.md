[toc]

# Loop Invariant Code Motion report

## 实验目的

通过把`LLVM IR` 中的循环不变量外提 ，减少指令的执行次数，从而对`LLVM IR`  进行优化 。

## 实验说明

由于循环不变量的种类与情况比较复杂 ，刻画起来也较难 ，所以本次是验只针对了，部分满足一定特征的循环不变式 。 该特征是左值的确定，使用的操作数没有在循环中被定值 。

## 实验算法

> 核心思想

对一个循环，先确定循环内被定值的变量集合 ，然后对每一条指令查看其操作数，如果该指令的所有操作数不是循环内的定值变量，则说明该指令是循环不变量 。 然后把该指令加到循环入口的所有前驱节点中。

> 具体实现

+ 首先先获取循环的信息，`get_loop_information( func) `。 这个过程复用了必做部分的循环分析 。然后将循环的相关信息存放在容器中 。具体存放的信息有：

   ```c++
      // loops found in a function
      std::unordered_map<Function *, std::unordered_set<BBset_t *>> func2loop;
      // { entry bb of loop : loop }
      std::unordered_map<BasicBlock *, BBset_t *> base2loop;
      // { loop : entry bb of loop }
      std::unordered_map<BBset_t *, BasicBlock *> loop2base;
   ```

+ 然后是确定每个循环的定值变量集合。这里采用的方法是左值都是循环内被定值的。

  ```c++
   auto var= dynamic_cast<Value *>(&instr);    //最左边的参数
   var2loop[var].insert(loop); 
  ```

+ 然后对每一条指令判断是否循环不变式，判断的条件是：

  ```c++
  if( !isa<Constant>(var)&& var2loop.find(var)!=var2loop.end()&&(var2loop[var].find(loop)!=var2loop[var].end())){
                                  //说明该指令使用的变量是在循环里面定值的
                               
                                  }
  ```

+ 至此得到了循环不变式。最后一步是把指令外提到循环外。这里直接使用了原生的工具：

  ```c++
    /// Unlink this instruction from its current basic block and insert it into
    /// the basic block that MovePos lives in, right before MovePos.
    void moveBefore(Instruction *MovePos);
  ```

## 实验测试

1. 外提一层循环

   **loopinvhoist_1.c**

   ```c
   int main(){
       int num = 0 ,i,j;
       int a = 3;
       int b;
       for(int i=0;i<10;i++){     
           num = i + j;
           b = a + 9;
           num = i + j;
       }
   } 
   ```

   运行指令 `./mClang  ../tests/loopinvhoist_1.c -show-ir-after-pass` 。得到的结果为：

   ```CQL
   Processing function  main,  LoopInvHoist result is as follow 
   {
   ------
   loop begin
   LCM instruction add  %1 = add nsw i32 3, 9
   }
   ```

   `Pass` 过后的`IR`为 :

   ```CQL
   ; Function Attrs: noinline nounwind uwtable
   define dso_local i32 @main() #0 {
     %1 = add nsw i32 3, 9
     br label %2
   
   2:                                                ; preds = %8, %0
     %3 = phi i32 [ 0, %0 ], [ %9, %8 ]
     %4 = icmp slt i32 %3, 10
     br i1 %4, label %5, label %10
   
   5:                                                ; preds = %2
     %6 = add nsw i32 %3, undef
     %7 = add nsw i32 %3, undef
     br label %8
   
   8:                                                ; preds = %5
     %9 = add nsw i32 %3, 1
     br label %2
   
   10:                                               ; preds = %2
     ret i32 0
   }
   
   ```

    可以明确地看到指令是被外提到了循环外面。

2. 外提两层循环

   **loopinvhoist_2.c**

   ```c
   int main(){
       int num = 0 ,i,j;
       int a = 3;
       int b;
       for(int j=0;j<10;j++)
       for(int i=0;i<10;i++){     
           num = i + j;
           b = a + 9;    
           b = a + 9;
           num = i + j;
       }
   }    
   ```

   运行指令`./mClang  ../tests/loopinvhoist_2.c -show-ir-after-pass` 得到输出的提示信息为：

   ```CQL
   Processing function  main,  LoopInvHoist result is as follow 
   {
   ------
   loop begin
   LCM instruction add  %5 = add nsw i32 3, 9
   loop begin
   LCM instruction add  %1 = add nsw i32 3, 9
   LCM instruction add  %2 = add nsw i32 3, 9
   }
   ```

   最终的`IR` 为：

   ```CQL
   ; Function Attrs: noinline nounwind uwtable
   define dso_local i32 @main() #0 {
     %1 = add nsw i32 3, 9
     %2 = add nsw i32 3, 9
     br label %3
   
   3:                                                ; preds = %16, %0
     %4 = phi i32 [ 0, %0 ], [ %17, %16 ]
     %5 = icmp slt i32 %4, 10
     br i1 %5, label %6, label %18
   
   6:                                                ; preds = %3
     br label %7
   
   7:                                                ; preds = %13, %6
     %8 = phi i32 [ 0, %6 ], [ %14, %13 ]
     %9 = icmp slt i32 %8, 10
     br i1 %9, label %10, label %15
   
   10:                                               ; preds = %7
     %11 = add nsw i32 %8, %4
     %12 = add nsw i32 %8, %4
     br label %13
   
   13:                                               ; preds = %10
     %14 = add nsw i32 %8, 1
     br label %7
   
   15:                                               ; preds = %7
     br label %16
   
   16:                                               ; preds = %15
     %17 = add nsw i32 %4, 1
     br label %3
   
   18:                                               ; preds = %3
     ret i32 0
   }
   
   ```

    可以明确地看到指令是被外提到了循环外面。并且外提了两层。

## 实验总结

**实验现存问题：**

1.  由于对 ` void moveBefore(Instruction *MovePos);` 的行为不够理解， 导致每个循环只能外提指令的时候不能把所有的循环不变量外提。如果循环中有多个不变量 ，外提一个后，就无法继续遍历该基本快中的指令。 
2. 还有问题是，只对运算指令外提。对于直接赋常量语句，并没有识别成循环不变式。这个问题是一开始提到的，对循环不变式的刻画不够完备。

**如何解决：** 好在该实验已经对循环的分析比较完备 ，算法也是清晰的 。 相当于输入是可以的，剩余的问题是对循环不变式的刻画 ， 以及对移动指令后产生的结果的理解。