# FunctionInfo

统计函数的相关信息，包括

* 函数名
* 函数参数的个数
* 函数被调用的次数
* 函数所包含的基本块个数
* 函数包含的LLVM IR指令数

```c++
            for (auto iter = M.begin(); iter != M.end(); ++iter){
                Function &func = *iter;
                //函数名
                llvm::outs() << "Name:" << func.getName() << "\n";
                //函数参数个数
                llvm::outs() << "Number of Arguments: " << func.arg_size() << "\n";
                //函数被调用的次数（Module中定义的非main如果被调用，则输出为调用次数+1）（main函数对应为0）
                llvm::outs() << "Number of Direct Call Sites in the same LLVM module: " << func.getNumUses() << "\n";
                //函数的基本块个数
                llvm::outs() << "Number of Basic Blocks: " << func.size() << "\n";
                //函数所包含的IR数
                llvm::outs() << "Number of Instructions: " << func.getInstructionCount() << "\n";
                transformed = true;
            }
```

