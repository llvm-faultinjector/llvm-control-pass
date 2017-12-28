#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"
#include "llvm/Analysis/LoopInfo.h"
#include <vector>
#include <map>

using namespace llvm;

namespace {
  struct CustomPass : public FunctionPass {
    static char ID;
    std::vector<Instruction *> dependency;
    std::map<Value *, std::vector<Instruction *>> dependency_map;
    CustomPass()
      : FunctionPass(ID)
    {
    }

    bool runOnFunction(Function &F) override
    {
      errs() << "\n";
      errs() << "Function " << F.getName() + "\n";
      std::vector<Value *> v = findAnnotation(F);
      for (auto s : v)
        findDependency(s, F);
      //std::vector<Argument *> args = findFunctionArgumentReturnDependency(F);
      //for ( auto arg : args )
      //    errs() << " - Argument \"" << arg->getName() << "\" affect the return value.\n";
      //findFunctionCallByReferenceDependency(F);
      errs() << "\n";
      return false;
    }

    /// annotate의 메시지를 출력합니다.
    /// 예를 들어, int __attribute__((annotate("xxx"))) a; 같은 구문에서 "xxx"를 가져옵니다.
    StringRef getAnnotatedString(CallInst *c)
    {
      Constant *cs = (cast<GlobalVariable>((cast<ConstantExpr>(c->getArgOperand(1)))->getOperand(0)))->getInitializer();
      return cast<ConstantDataArray>(cs)->getAsCString();
    }

    /// 모든 annotate변수를 찾습니다.
    /// 반환되는 벡터에는 annotate된 변수 그 자체의 포인터만 들어갑니다.
    std::vector<Value *> findAnnotation(Function &F)
    {
      std::vector<Value *> annotate;
      for (auto& bb : F)
      {
        for (auto& k : bb)
        {
          if (CallInst *c = dyn_cast<CallInst> (&k))
          {
            Function *cf = c->getCalledFunction();
            if (cf->getName() == "llvm.var.annotation")
            {
              Value *v = (cast<BitCastInst>(c->getArgOperand(0)))->getOperand(0);
              annotate.push_back(v);

              errs() << " - Detected annotated string : " << getAnnotatedString(c) << "("
                << v->getName() << ")" << "\n";
            }
          }
        }
      }
      errs() << "\n";
      return annotate;
    }

    /// 어떤 함수에 포함된 특정 변수가 주어지면, 해당 변수를
    /// 변경시키는 모든 구문을 찾습니다.
    /// 이 함수는 StoreInst, PHINode뿐만아니라 CallInst의 Argument가
    /// 해당 변수를 변경시키는지 여부와, Call by reference로 Argument로
    /// 넘겨질 경우까지 모두 포함됩니다.
    void findDependency(Value *V, Function &F)
    {
      errs() << " - Dependent value : " << V->getName() << "\n";

      errs() << "   * store\n";
      std::vector<Value *> step;
      for (auto& bb : F)
      {
        for (auto& i : bb)
        {
          ///
          /// annotate된 변수를 직접적으로 변화시키는 variable를 찾습니다.
          ///
          if (StoreInst* si = dyn_cast<StoreInst> (&i))
          {
            if (si->getPointerOperand() == V)
            {
              errs() << "     - " << bb.getName() << "\n      - ";
              i.print(errs());
              errs() << "\n";
              step.push_back(si->getValueOperand());
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            std::vector<Argument *> args = findFunctionCallByReferenceDependency(*ci->getCalledFunction());
            for (auto& arg : args)
            {
              unsigned num = arg->getArgNo();
              step.push_back(ci->getOperand(num));
            }
          }
        }
      }
      errs() << "\n   * follow nodes\n";
      for (auto& v : step)
        printFollowNodes(F, v);
      errs() << "\n";
      dependency_map.insert(std::pair<Value *, std::vector<Instruction *>>(V, dependency));
      dependency.clear();
    }

    /// annotate된 변수가 포함된 함수에서 의존성 검사를 시작합니다.
    void printFollowNodes(Function &F, Value *X)
    {
      ///
      /// X가 Instruction이 아닌 경우 Operand도 없으므로 더 이상
      /// 탐색할 수 없습니다.
      ///
      if (Instruction *inst = dyn_cast<Instruction> (X))
      {
        ///
        /// 노드를 탐색할 때 같은 Inst를 찾아 무한루프에 빠지는 것을 방지합니다.
        ///
        for (auto& v : dependency)
          if (v == inst) return;
        dependency.push_back(inst);

        ///===----------------------------------------===
        ///
        /// annotate된 변수를 변화시키는 모든 instruction이
        /// 이 부분을 거치게 됩니다.
        ///
        ///===----------------------------------------===
        errs() << "     - ";
        inst->print(errs());
        errs() << "\n";

        ///
        /// PHINode도 어떤 레지스터 값을 변경시킬 수 있습니다.
        /// operand의 접근방법이 다른 instruction과 달라 다른
        /// 방법을 사용합니다.
        ///
        if (PHINode *phi = dyn_cast<PHINode> (inst))
        {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
          {
            printFollowNodes(F, phi->getIncomingValue(i));
            printFollowNodesByStoreCallInst(F, phi->getIncomingValue(i));
          }
        }

        ///
        /// 호출되는 함수의 모든 Operand가 annotated된 변수 변화에 영향을
        /// 주는 지 이 단계에선 알 수 없습니다.
        /// 따라서 호출되는 함수의 함수인자가 그 함수 출력에 영향을 주는지 검사합니다.
        ///
        else if (CallInst *ci = dyn_cast<CallInst> (inst))
        {
          std::vector<Argument *> args = findFunctionArgumentReturnDependency(*ci->getCalledFunction());
          for (auto& arg : args)
          {
            unsigned num = arg->getArgNo();
            printFollowNodes(F, ci->getOperand(num));
            printFollowNodesByStoreCallInst(F, ci->getOperand(num));
          }
        }

        ///
        /// 일반적인 instruction입니다.
        ///
        else
        {
          for (unsigned i = 0; i < inst->getNumOperands(); i++)
          {
            printFollowNodes(F, inst->getOperand(i));
            printFollowNodesByStoreCallInst(F, inst->getOperand(i));
          }
        }
      }
    }

    ///
    /// 함수 내부에 특정 value를 변화시키는 구문을 찾습니다.
    ///
    void printFollowNodesByStoreCallInst(Function &F, Value *v)
    {
      for (auto& bb : F)
      {
        for (auto& i : bb)
        {
          if (StoreInst* si = dyn_cast<StoreInst> (&i))
          {
            if (si->getPointerOperand() == v)
            {
              printFollowNodes(F, si);
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            std::vector<Argument *> args = findFunctionCallByReferenceDependency(*ci->getCalledFunction());
            for (auto& arg : args)
            {
              unsigned num = arg->getArgNo();
              if (ci->getOperand(num) == v)
                printFollowNodes(F, ci->getOperand(num));
            }
          }
        }
      }
    }

    /// 어떤 함수인자가 변수 출력에 영향을 주는지 찾습니다.
    /// 모든 경우의 수를 찾는 printFollowNodes와는 달리 반환값에 변화를
    /// 주는 어떤 argument가 있는지만 찾으면 되므로 이런 방법으로 구현했습니다.
    /// 알고리즘은 printFollowNodes와 같습니다.
    std::vector<Argument *> findFunctionArgumentReturnDependency(Function &F)
    {
      std::vector<Argument *> vec;
      if (F.doesNotReturn())
        return vec;
      std::vector<Argument *> args;
      for (auto arg = F.arg_begin(); arg != F.arg_end(); arg++)
        args.push_back(&*arg);

      ///
      /// return inst에 변화를 줄 수 있는 모든 구문을 찾습니다.
      ///
      for (auto& bb : F)
      {
        for (auto& i : bb)
        {
          if (ReturnInst *RI = dyn_cast<ReturnInst>(&i))
          {
            for (auto arg : args)
            {
              if (_InternalfindFunctionArgumentReturnDependency(F, RI->getReturnValue(), arg))
                vec.push_back(arg);
            }
          }
        }
      }
      return vec;
    }
    bool _InternalfindFunctionArgumentReturnDependency(Function &F, Value *v, Argument *arg)
    {
      if (v == arg) return true;
      if (Instruction *inst = dyn_cast<Instruction> (v))
      {
        if (PHINode *phi = dyn_cast<PHINode> (inst))
        {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
          {
            if (_InternalfindFunctionArgumentReturnDependency(F, phi->getIncomingValue(i), arg) ||
              _InternalfindFunctionArgumentReturnDependencyByStoreInst(F, phi->getIncomingValue(i), arg))
              return true;
          }
        }
        else if (CallInst *ci = dyn_cast<CallInst> (inst))
        {
          std::vector<Argument *> args = findFunctionArgumentReturnDependency(*ci->getCalledFunction());
          for (auto& _arg : args)
          {
            unsigned num = _arg->getArgNo();
            if (_InternalfindFunctionArgumentReturnDependency(F, ci->getOperand(num), arg) ||
              _InternalfindFunctionArgumentReturnDependencyByStoreInst(F, ci->getOperand(num), arg))
              return true;
          }
        }
        else
        {
          for (unsigned i = 0; i < inst->getNumOperands(); i++)
          {
            if (_InternalfindFunctionArgumentReturnDependency(F, inst->getOperand(i), arg) ||
              _InternalfindFunctionArgumentReturnDependencyByStoreInst(F, inst->getOperand(i), arg))
              return true;
          }
        }
      }
      return false;
    }
    bool _InternalfindFunctionArgumentReturnDependencyByStoreInst(Function &F, Value *v, Argument *arg)
    {
      for (auto& bb : F)
      {
        for (auto& i : bb)
        {
          if (StoreInst* si = dyn_cast<StoreInst> (&i))
          {
            if (si->getPointerOperand() == v)
            {
              if (_InternalfindFunctionArgumentReturnDependency(F, si->getValueOperand(), arg))
                return true;
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            std::vector<Argument *> args = findFunctionCallByReferenceDependency(*ci->getCalledFunction());
            for (auto& _arg : args)
            {
              unsigned num = _arg->getArgNo();
              if (ci->getOperand(num) != v)
                continue;
              if (_InternalfindFunctionArgumentReturnDependency(F, ci->getOperand(num), arg))
                return true;
            }
          }
        }
      }
      return false;
    }

    /// 함수인자 중 call by reference가 있을 경우, 그것을 찾습니다.
    std::vector<Argument *> findFunctionCallByReferenceDependency(Function &F)
    {
      std::vector<Argument *> vec;
      for (auto arg = F.arg_begin(); arg != F.arg_end(); arg++)
      {
        if (arg->getType()->isPointerTy())
          if (arg->getDereferenceableBytes() > 0)
          {
            vec.push_back(arg);
          }
      }
      return vec;
    }

  };
}

char CustomPass::ID = 0;
static RegisterPass<CustomPass> X("custom", "CustomPass");
