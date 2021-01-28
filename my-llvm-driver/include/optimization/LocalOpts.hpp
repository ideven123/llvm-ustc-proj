#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace llvm
{
    ModulePass *createLocalOptsPass();
    void initializeLocalOptsPass(PassRegistry &);
} 

namespace
{

    class LocalOpts : public ModulePass
    {
    public:
        static char ID;

        LocalOpts() : ModulePass(ID) {
            initializeLocalOptsPass(*PassRegistry::getPassRegistry());
        }

        ~LocalOpts() override {}


        bool runOnModule(llvm::Module &M) override
        {
            bool transform = false;
            
            //遍历Module的每个Function
            for (Function &func : M)
                if (runOnFunction(func)){
                    std::cout << "run on func" << std::endl;
                    transform = true;
                }
            
            /*
            for (llvm::Module::iterator func = M.begin(), func_end = M.end(); func != func_end; ++func){
                for (llvm::Function::iterator BB = func->begin(), BB_end = func->end(); BB != BB_end; ++BB){
                    //待删除的指令
                    std::list<Instruction *> deleteInst;
                    //遍历BB的每条指令
                    for (llvm::BasicBlock::iterator iter = BB->begin(); iter != BB->end(); iter++)
                    {
                        Instruction &inst = *iter;
                        outs() << inst.getOpcodeName() << "\n";

                        //判断是否是二元操作的指令
                        if (inst.isBinaryOp())
                        {
                            std::cout << "is Binary Op" << std::endl;
                            switch (inst.getOpcode())
                            {
                            //加法指令，注意这里不用break
                            case Instruction::Add:
                                std::cout << "is add" << std::endl;
                                // x + 0 = 0 + x = x
                                for (int i = 0; i < 2; ++i)
                                {
                                    if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i)))
                                    {
                                        if (val->getZExtValue() == 0)
                                        {
                                            ++algebraic_identity_num;
                                            Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                                            inst.replaceAllUsesWith(another_val);
                                            deleteInst.push_back(&inst);
                                            break;
                                        }
                                    }
                                }
                                break;
                            //减法指令
                            case Instruction::Sub:
                                std::cout << "is sub" << std::endl;
                                // b = a + 1(another inst); c = b - 1(inst)  ==>  b = a + 1; c = a;
                                if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(1)))
                                {
                                    if (Instruction *another_inst = dyn_cast<Instruction>(inst.getOperand(0)))
                                    {
                                        // 如果这两条指令刚好一加一减，同时第二个操作数是一样的
                                        if (inst.getOpcode() + another_inst->getOpcode() == Instruction::Add + Instruction::Sub && another_inst->getOperand(1) == val)
                                        {
                                            ++multi_inst_optimization_num;
                                            inst.replaceAllUsesWith(another_inst->getOperand(0));
                                            deleteInst.push_back(&inst);
                                            break;
                                        }
                                    }
                                }

                                break;
                            //乘法指令
                            case Instruction::Mul:
                                // x * 2   ==>   x << 1
                                // 2 * x   ==>   x << 1
                                //遍历指令右边的每个操作数
                                std::cout << "is multiply" << std::endl;
                                for (int i = 0; i < 2; i++)
                                {
                                    //如果是常数
                                    if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i)))
                                    {
                                        //如果为求平方
                                        if (val->getZExtValue() == 2)
                                        {
                                            ++strength_reduction_num;
                                            //得到另一个操作数
                                            Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                                            //创建一条IR
                                            IRBuilder<> builder(&inst);
                                            //创建val(inst的基类),这是一个位移指令
                                            Value *val = builder.CreateShl(another_val, 1);
                                            //将inst的出现替换为val
                                            inst.replaceAllUsesWith(val);
                                            //将inst加入待删除列表
                                            deleteInst.push_back(&inst);
                                            //只执行一次
                                            break;
                                        }
                                    }
                                }
                                break;
                            case Instruction::SDiv:
                                std::cout << "is SDiv" << std::endl;
                                // x / 2   ==>   x >> 1
                                //遍历指令右边的每个操作数

                                //如果是常数
                                if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(1)))
                                {
                                    //如果为除以2
                                    if (val->getZExtValue() == 2)
                                    {
                                        ++strength_reduction_num;
                                        //得到另一个操作数
                                        Value *first_val = inst.getOperand(0);
                                        //创建一条IR
                                        IRBuilder<> builder(&inst);
                                        //创建val(inst的基类),这是算术右移指令
                                        Value *val = builder.CreateShl(first_val, 1);
                                        //将inst的出现替换为val
                                        inst.replaceAllUsesWith(val);
                                        //将inst加入待删除列表
                                        deleteInst.push_back(&inst);
                                    }
                                }
                                break;
                            }
                        }
                    }
                    // Instruction必须在遍历BasicBlock之后再删除，否则可能会导致iterator指向错误的地方
                    for (Instruction *inst : deleteInst)
                        if (inst->isSafeToRemove())
                            // 注意，用erase而不是remove
                            inst->eraseFromParent();
                    return true;
                }
            }*/
            //输出相关信息
            dumpInformation();
            return transform;
        }

    private:
        //算术不变量的数目
        int algebraic_identity_num = 0;
        //强度削弱的指令数
        int strength_reduction_num = 0;
        //连续两条指令的优化
        int two_inst_optimization_num = 0;

        bool runOnBasicBlock(llvm::BasicBlock &B)
        {
            //待删除的指令
            std::list<Instruction *> deleteInst;
            //遍历BB的每条指令
            for (BasicBlock::iterator iter = B.begin(); iter != B.end(); ++iter)
            {
                Instruction &inst = *iter;
                outs() << inst.getOpcodeName() << "\n";

                //判断是否是二元操作的指令
                if (inst.isBinaryOp()){
                    std::cout << "is Binary Op" << std::endl;
                    switch (inst.getOpcode())
                    {
                    //加法指令，注意这里不用break
                    case Instruction::Add:
                        std::cout << "is add" << std::endl;
                        // x + 0 = 0 + x = x
                        for (int i = 0; i < 2; ++i)
                        {
                            if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i)))
                            {
                                if (val->getZExtValue() == 0)
                                {
                                    ++algebraic_identity_num;
                                    Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                                    inst.replaceAllUsesWith(another_val);
                                    deleteInst.push_back(&inst);
                                    break;
                                }
                            }
                        }
                        break;
                    //减法指令
                    case Instruction::Sub:
                        std::cout << "is sub" << std::endl;
                        // b = a + 1(another inst); c = b - 1(inst)  ==>  b = a + 1
                        if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(1)))
                        {
                            //std::cout << "Yes1" << std::endl;
                            if (Instruction *another_inst = dyn_cast<Instruction>(inst.getOperand(0)))
                            {
                                //std::cout << "Yes2" << std::endl;
                                // 如果这两条指令刚好一加一减，同时第二个操作数是一样的
                                if (inst.getOpcode() + another_inst->getOpcode() == Instruction::Add + Instruction::Sub && another_inst->getOperand(1) == val)
                                {
                                    //std::cout << "Yes3" << std::endl;
                                    ++two_inst_optimization_num;
                                    inst.replaceAllUsesWith(another_inst->getOperand(0));
                                    deleteInst.push_back(&inst);
                                    break;
                                }
                            }
                        }

                        break;
                    //乘法指令
                    case Instruction::Mul:
                    {
                        // x * 2   ==>   x << 1
                        // 2 * x   ==>   x << 1
                        //遍历指令右边的每个操作数
                        std::cout << "is multiply" << std::endl;
                        bool flag1 = false;
                        for (int i = 0; i < 2; ++i)
                        {
                            if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i)))
                            {
                                if (val->getZExtValue() == 1)
                                {
                                    ++algebraic_identity_num;
                                    Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                                    inst.replaceAllUsesWith(another_val);
                                    deleteInst.push_back(&inst);
                                    flag1 = true;
                                    break;
                                }
                            }
                        }

                        if(!flag1){
                            for (int i = 0; i < 2; i++)
                            {
                                //如果是常数
                                if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(i)))
                                {
                                    //如果为乘2
                                    if (val->getZExtValue() == 2)
                                    {
                                        ++strength_reduction_num;
                                        //得到另一个操作数
                                        Value *another_val = inst.getOperand(i == 0 ? 1 : 0);
                                        //创建一条IR
                                        IRBuilder<> builder(&inst);
                                        //创建val(inst的基类),这是一个位移指令
                                        Value *val = builder.CreateShl(another_val, 1);
                                        //将inst的出现替换为val
                                        inst.replaceAllUsesWith(val);
                                        //将inst加入待删除列表
                                        deleteInst.push_back(&inst);
                                        //只执行一次
                                        break;
                                    }
                                }
                            }
                        }
                        break;
                    }

                    case Instruction::SDiv:
                        std::cout << "is SDiv" << std::endl;
                        // x / 2   ==>   x >> 1
                        //遍历指令右边的每个操作数
                
                        //如果是常数
                        if (ConstantInt *val = dyn_cast<ConstantInt>(inst.getOperand(1)))
                        {
                            //如果为除以2
                            if (val->getZExtValue() == 2)
                            {
                                ++strength_reduction_num;
                                //得到另一个操作数
                                Value *first_val = inst.getOperand(0);
                                //创建一条IR
                                IRBuilder<> builder(&inst);
                                //创建val(inst的基类),这是算术右移指令
                                Value *val = builder.CreateAShr(first_val, 1);
                                //将inst的出现替换为val
                                inst.replaceAllUsesWith(val);
                                //将inst加入待删除列表
                                deleteInst.push_back(&inst);
                            
                            }
                        }
                        break;
                    }
                    

                }
            }
            //Instruction必须在遍历BasicBlock之后再删除，否则可能会导致iterator指向错误的地方
            for (Instruction *inst : deleteInst)
                if (inst->isSafeToRemove())
                    inst->eraseFromParent();
            return true;

        }

        bool runOnFunction(llvm::Function &F)
        {
            bool transform = false;
            //遍历Function的每个BasicBlock
            for (BasicBlock &block : F)
                if (runOnBasicBlock(block)){
                    std::cout << "run on block" << std::endl;
                    transform = true;
                }
                    
            return transform;
        }

        //打印相关信息
        void dumpInformation()
        {
            outs() << "Transformations applied:\n";
            outs() << "\tThe num of Algebraic Identity: " << algebraic_identity_num << "\n";
            outs() << "\tThe num of Strength Reduction: " << strength_reduction_num << "\n";
            outs() << "\tThe num of Two-Inst Optimization: " << two_inst_optimization_num << "\n";
        }
    };

} 

char LocalOpts::ID = 0;
INITIALIZE_PASS(LocalOpts, "Local", "Local Opts", false, false)

ModulePass *llvm::createLocalOptsPass()
{
    return new LocalOpts();
}
