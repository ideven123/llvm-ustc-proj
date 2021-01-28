# 选做部分问答题

####  理解DCE Pass

在[`MyPasses.hpp`](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp)中定义了[myDCEPass](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp#L69)类，这是一个FunctionPass，它重写了 `bool runOnFunction(Function &F)`。请阅读并实践，回答： 

1）简述`skipFunction()`、`getAnalysisIfAvailable<TargetLibraryInfoWrapperPass>()`的功能

2）请简述DCE的数据流分析过程，即`eliminateDeadCode()`和`DCEInstruction()`



***

1. * `skipFunction()`：该函数检查是否跳过可选的passes。如果`optimization bisect`超过了限制，或者`Attribute::OptimizeNone`被置位，则可选的passes会被跳过，`runOnModule`方法直接返回`false`
   * `getAnalysisIfAvailable<TargetLibraryInfoWrapperPass>()`：
     * 子类使用这个函数来获取可能存在的分析信息，例如更新它。
     * 经常被用来转换API。在转换执行时该方法自动更新一次pass的分析结果
     * 与`getAnalysis`不同，因为`getAnalysis`可能会失败(如果分析结果还没有计算出来)，所以应该在分析不可用的情况下且你能够处理时使用`getAnalysis`。
2. * `eliminateDeadCode()`
      ```C++
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
      ```
   * `DCEInstruction()`
      ```C++
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
                     I->setOperand(i, nullptr);      //设置指向该操作数的指针为空??

                     if (!OpV->use_empty() || I == OpV)  //如果该操作数use_empty()返回值为假 或 指令的左值为当前操作数
                        continue;                       //进入下一轮循环

                     if (Instruction *OpI = dyn_cast<Instruction>(OpV))  //如果该操作数不符合前面if的条件，且可以动态转换为对应的IR指令（也就是说它是%开头的）
                        if (isInstructionTriviallyDead(OpI, TLI))   //如果这条指令产生的该值没被使用过且这个指令没有其他影响
                              WorkList.insert(OpI);           //将该指令插入WorkList
                  }
                  //print:将对象格式化到指定的缓冲区。如果成功，将返回格式化字符串的长度。如果缓冲区太小，则返回一个大于BufferSize的长度以供重试。
                  I->print(llvm::outs()); //??
                  std::cout << " " << I->getOpcodeName() << std::endl;    //输出该指令的操作码
                  I->eraseFromParent();                                   //将该指令从包含它的基本块中解除链接并删除它
                  return true;    //成功删除一条死代码,返回真
            }
            return false;   //否则返回假
         }
      ```







#### 理解Global Pass

在[`MyPasses.hpp`](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp)中定义了[myGlobalPass](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp#L106)类，这是一个ModulePass，它重写了 `bool runOnModule(llvm::Module &M)`。请阅读并实践，回答： 

1）简述`skipModule()`的功能

2）请扩展增加对Module中类型定义、全局变量定义等的统计和输出
（张昱老师和刘硕助教：类型定义指形如`typedef struct`）

***

1. 该函数检查是否跳过可选的passes。如果`optimization bisect`超过了限制，则可选的passes会被跳过，`runOnModule`方法直接返回`false`

2. 全局变量定义 
   输出了全局变量的名称和数量
   类型定义 
   输出了类型定义的名称和数量
   具体请参考`MyPasses.hpp`