# 必做部分思考题

请解释FindFunctionBackedges函数中InStack变量的物理意义（例如Visited变量的物理意义为存储已访问的BB块集合、VisitStack变量的物理意义为栈中待处理的边集合）

***

```c++
/// FindFunctionBackedges - Analyze the specified function to find all of the
/// loop backedges in the function and return them.  This is a relatively cheap
/// (compared to computing dominators and loop info) analysis.
///
/// The output is added to Result, as pairs of <from,to> edge info.
void llvm::FindFunctionBackedges(const Function &F,
     SmallVectorImpl<std::pair<const BasicBlock*,const BasicBlock*> > &Result) {
  const BasicBlock *BB = &F.getEntryBlock();	//获得函数入口BB
  if (succ_empty(BB))							//若没有后继BB
    return;

  SmallPtrSet<const BasicBlock*, 8> Visited;	//已访问的BB块集合
  SmallVector<std::pair<const BasicBlock *, const_succ_iterator>, 8> VisitStack;	//栈，里面是待处理的边集合
  SmallPtrSet<const BasicBlock*, 8> InStack;

  Visited.insert(BB);	//标记当前(入口)BB已访问
  VisitStack.push_back(std::make_pair(BB, succ_begin(BB)));	//将BB引出的第一条边加入到待处理集合
  InStack.insert(BB);		//记录当前BB
  do {
    std::pair<const BasicBlock *, const_succ_iterator> &Top = VisitStack.back();	//栈顶的边出栈
    const BasicBlock *ParentBB = Top.first;		//ParentBB为引出该边的BB
    const_succ_iterator &I = Top.second;		//I为该边

    bool FoundNew = false;	//标记
    while (I != succ_end(ParentBB)) {	//遍历ParentBB的所有出边
      BB = *I++;		//记录当前的后继BB，并更新迭代器I
      if (Visited.insert(BB).second) {	//insert的声明类型：std::pair<iterator, bool> insert(PtrType Ptr)，这里即判断是否插入成功，成功证明该基本块未访问过
        FoundNew = true;	//发现新的基本块
        break;
      }
      // Successor is in VisitStack, it's a back edge.
      // count - Return 1 if the specified pointer is in the set, 0 otherwise.
      //当前BB==succ_end（ParentBB）
      if (InStack.count(BB))
        Result.push_back(std::make_pair(ParentBB, BB));	//记录发现的回边
    }

    if (FoundNew) {
      // Go down one level if there is a unvisited successor.
      InStack.insert(BB);	//记录当前BB
      VisitStack.push_back(std::make_pair(BB, succ_begin(BB)));	//标记当前边待处理
    } else {
      // Go up one level.回溯
      InStack.erase(VisitStack.pop_back_val().first);	//从InStack中删除对应部分
    }
  } while (!VisitStack.empty());
}
```

InStack变量的物理意义：记录**深度优先搜索**路径上的结点(基本块)