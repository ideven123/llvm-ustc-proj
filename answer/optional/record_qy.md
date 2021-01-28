### 可用表达式分析

#### 计算流程

* GetAvailExpr 类从 FunctionPass 继承，具体计算都在 runOnFunction 函数中完成。
* 1. 计算各个基本块中的gen，kill集合。
  * 第一次遍历块中指令，将指令加入all集合中以便迭代计算in，out，并且得到该基本块中有定值的变量合集，存入def向量中。
  * 第二次遍历块中指令，若该条指令后没有对其中变量定值，则将对应表达式加入gen集合（def向量是按顺序存储的，可以只查找该条指令之后的def）；并且将含有该条指令等号左边变量的表达式加入kill集合。
* 2. 将各个基本块对应的gen，kill集合转换为BitVector形式存储。
* 3. 初始化各个基本块in，out集合，即新建BitVector变量。
* 4. 根据可用表达式算法进行迭代计算。计算过程中的集合操作更改为BitVector变量按位操作。如：
  ~~~
    //tmp = (tmp - kill_e[&bb]) & gen_e[&bb];
    for (int i = 0; i < bit_vector_size; i++){
        tmp[i] = (tmp[i] && !kill_e[&bb][i]) && gen_e[&bb][i];
     }
  ~~~
* 5. 输出计算结果。输出全集all中包含的表达式，直接以BitVector的01串形式输出in, out集合。

#### 数据结构和其他细节

* 定义了Expr类，用于表示一个表达式。类成员和构造函数：
~~~
    class Expr{
        public:
            unsigned opcode;
            Value *loperand, *roperand, *inst;
            bool isUnary;

            Expr(Instruction *i){
                inst = i;
                opcode = i->getOpcode();
                loperand = i->getOperand(0);
                isUnary = (isa<llvm::UnaryOperator>(i));
                if (!isUnary) roperand = i->getOperand(1);
                else roperand = nullptr;
            }
    }
~~~
原先设计希望能够处理单目运算符和双目运算符，但实际中发现LLVM IR将 -a 翻译为 0 - a ，算数运算中基本没有单目运算符。
* Expr类的输出：使用 Instruction::getOpcodeName 函数输出运算符，使用 Value.printAsOperand(llvm::outs(), false) 函数输出变量。
* 上述提到的各个集合的数据类型：
~~~
std::map<BasicBlock *, std::set<Expr *>> gen, kill;
std::set<Expr *> all;
std::vector<Value *> def;       // temp variable
std::map<BasicBlock *, llvm::BitVector> in, out, gen_e, kill_e;
~~~
* 遍历基本块中指令时，只对 BinaryOperator, UnaryOperator 类型的指令进行处理。大多数遍历指令的循环中都有这一行：
~~~
if (!isa<llvm::BinaryOperator>(i) && !isa<llvm::UnaryOperator>(i)) continue;
~~~
* 在初始化in, out和输出时，entry块需要单独处理。由于 `for (BasicBlock &bb : F)` 遍历时第一个得到的块就是entry，只需要在第一次for循环体执行时处理就可以。
* 由于IR代码是ssa形式，各个块必定有 `kill[bb] = ∅`, `out[bb] = gen[bb]`，各个gen集合是对全集的一个划分。在该可用表达式分析的基础上写公共子表达式删除也没有意义。

#### 测试和输出

* 测试代码：/tests/Availexpr.c
~~~
int a=1, b=2, c=3, d=4, e=5;

int main(){
    c = a + b;
    d = a + b;
    e = -a;
    b = c - d;
    a = b + e;
    c = b + e;
    for (int i = 0; i < 5; i++){
        if (b<a) b = b*2;
        else b = a+b;
    }
    return 0;
}
~~~
* 生成IR：
~~~
; ModuleID = '../tests/Availexpr.c'
source_filename = "../tests/Availexpr.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@a = dso_local global i32 1, align 4
@b = dso_local global i32 2, align 4
@c = dso_local global i32 3, align 4
@d = dso_local global i32 4, align 4
@e = dso_local global i32 5, align 4

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 0, i32* %retval, align 4
  %0 = load i32, i32* @a, align 4
  %1 = load i32, i32* @b, align 4
  %add = add nsw i32 %0, %1
  store i32 %add, i32* @c, align 4
  %2 = load i32, i32* @a, align 4
  %3 = load i32, i32* @b, align 4
  %add1 = add nsw i32 %2, %3
  store i32 %add1, i32* @d, align 4
  %4 = load i32, i32* @a, align 4
  %sub = sub nsw i32 0, %4
  store i32 %sub, i32* @e, align 4
  %5 = load i32, i32* @c, align 4
  %6 = load i32, i32* @d, align 4
  %sub2 = sub nsw i32 %5, %6
  store i32 %sub2, i32* @b, align 4
  %7 = load i32, i32* @b, align 4
  %8 = load i32, i32* @e, align 4
  %add3 = add nsw i32 %7, %8
  store i32 %add3, i32* @a, align 4
  %9 = load i32, i32* @b, align 4
  %10 = load i32, i32* @e, align 4
  %add4 = add nsw i32 %9, %10
  store i32 %add4, i32* @c, align 4
  store i32 0, i32* %i, align 4
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %11 = load i32, i32* %i, align 4
  %cmp = icmp slt i32 %11, 5
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %12 = load i32, i32* @b, align 4
  %13 = load i32, i32* @a, align 4
  %cmp5 = icmp slt i32 %12, %13
  br i1 %cmp5, label %if.then, label %if.else

if.then:                                          ; preds = %for.body
  %14 = load i32, i32* @b, align 4
  %mul = mul nsw i32 %14, 2
  store i32 %mul, i32* @b, align 4
  br label %if.end

if.else:                                          ; preds = %for.body
  %15 = load i32, i32* @a, align 4
  %16 = load i32, i32* @b, align 4
  %add6 = add nsw i32 %15, %16
  store i32 %add6, i32* @b, align 4
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  br label %for.inc

for.inc:                                          ; preds = %if.end
  %17 = load i32, i32* %i, align 4
  %inc = add nsw i32 %17, 1
  store i32 %inc, i32* %i, align 4
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret i32 0
}

attributes #0 = { noinline nounwind optnone uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 11.0.0"}
~~~

* 可用表达式分析输出：
~~~
all expressions: %9.add.%10   %5.sub.%6   %2.add.%3   %7.add.%8   %0.add.%1   0.sub.%4   %i.add.1   %13.mul.2   %14.add.%15
%entry      in: 000000000   out: 000000000  gen: 111111000  kill: 000000000
%for.cond       in: 111111111   out: 000000000  gen: 000000000  kill: 000000000
%for.body       in: 111111111   out: 000000000  gen: 000000000  kill: 000000000
%if.then        in: 111111111   out: 000000010  gen: 000000010  kill: 000000000
%if.else        in: 111111111   out: 000000001  gen: 000000001  kill: 000000000
%if.end     in: 111111111   out: 000000000  gen: 000000000  kill: 000000000
%for.inc        in: 111111111   out: 000000100  gen: 000000100  kill: 000000000
%for.end        in: 111111111   out: 000000000  gen: 000000000  kill: 000000000
~~~