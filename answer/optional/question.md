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
2. To-do







#### 理解Global Pass

在[`MyPasses.hpp`](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp)中定义了[myGlobalPass](https://gitee.com/pei-qi-zhi/llvm-ustc-proj/blob/master/my-llvm-driver/include/optimization/MyPasses.hpp#L106)类，这是一个ModulePass，它重写了 `bool runOnModule(llvm::Module &M)`。请阅读并实践，回答： 

1）简述`skipModule()`的功能

2）请扩展增加对Module中类型定义、全局变量定义等的统计和输出

***

1. 该函数检查是否跳过可选的passes。如果`optimization bisect`超过了限制，或者`Attribute::OptimizeNone`被置位，则可选的passes会被跳过，`runOnModule`方法直接返回`false`

2. 全局变量 finished

   类型定义 to-do