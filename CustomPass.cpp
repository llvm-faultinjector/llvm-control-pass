#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"
#include "llvm/Analysis/LoopInfo.h"
#include <vector>
#include <map>
#include <tuple>

using namespace llvm;

namespace {

  class DependencyManager
  {
    Function *function;
    std::vector<Value *> values;
    std::vector<Instruction *> dependency;
    std::map<Value *, std::vector<Instruction *>> dependency_map;

  public:

    DependencyManager(Function *func, std::vector<Value *>& val)
      : function(func), values(val)
    {
    }

    const std::map<Value *, std::vector<Instruction *>>& findDependency()
    {
      dependency_map.clear();

      for (auto value : values)
        instDependency(function, value);

      return dependency_map;
    }

  private:
    
    ///-------------------------------------------------
    ///
    /// Function Scope
    ///
    ///-------------------------------------------------

    void instDependency(Function *F, Value *V)
    {
      errs() << " - Dependent value : " << V->getName() << "\n";

      followFromBB(F, V);
      dependency_map.insert(std::pair<Value *, std::vector<Instruction *>>(V, dependency));
      dependency.clear();
    }

    void followRoot(Function *F, Value *V)
    {
      if (Instruction *inst = dyn_cast<Instruction> (V))
      {
        for (auto v : dependency)
          if (v == inst) return;
        dependency.push_back(inst);

        errs() << "     - " << *inst << "\n";

        if (PHINode *phi = dyn_cast<PHINode> (inst))
        {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
          {
            followRoot(F, phi->getIncomingValue(i));
            followFromBB(F, phi->getIncomingValue(i));
          }
        }
        else if (CallInst *ci = dyn_cast<CallInst> (inst))
        {
          for (auto arg : followFunction(ci->getCalledFunction()))
          {
            unsigned num = arg->getArgNo();
            followRoot(F, ci->getOperand(num));
            followFromBB(F, ci->getOperand(num));
          }
        }
        else 
        {
          for (unsigned i = 0; i < inst->getNumOperands(); i++)
          {
            followRoot(F, inst->getOperand(i));
            followFromBB(F, inst->getOperand(i));
          }
        }
      }
    }

    void followFromBB(Function *F, Value *V)
    {
      for (auto& bb : *F)
      {
        for (auto& i : bb)
        {
          if (StoreInst* si = dyn_cast<StoreInst> (&i))
          {
            if (si->getPointerOperand() == V)
            {
              followRoot(F, si->getValueOperand());
            }
          }
          else if (LoadInst *li = dyn_cast<LoadInst> (&i))
          {
            if (li->getPointerOperand() == V)
            {
              followRoot(F, li);
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            Function *CF = ci->getCalledFunction();
            for (auto arg = CF->arg_begin(); arg != CF->arg_end(); arg++)
            {
              if (ci->getOperand(arg->getArgNo()) == V && arg->getType()->isPointerTy())
              {
                std::vector<Argument *> args = findReferenceable(CF, arg);
                for (auto value : args)
                  followRoot(F, ci->getOperand(value->getArgNo()));
                if (args.size() > 0)
                  dependency.push_back(&i);
                break;
              }
            }
          }
        }
      }
    }

    ///-------------------------------------------------
    ///
    /// Interprocedural Scope - (1) returns
    ///
    ///-------------------------------------------------
    
    std::vector<std::tuple<Value *, Argument *>> overlap1;

    std::vector<Argument *> followFunction(Function *F)
    {
      std::vector<Argument *> vec;
      std::vector<Argument *> args;
      for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++)
        args.push_back(arg);
      
      for (auto& bb : *F)
      {
        for (auto& i : bb)
        {
          if (ReturnInst *RI = dyn_cast<ReturnInst>(&i))
          {
            for (auto arg : args)
            {
              overlap1.clear();
              if (followFunctionRoot(F, RI->getReturnValue(), arg))
                vec.push_back(arg);
            }
          }
        }
      }

      return vec;
    }

    bool followFunctionRoot(Function *F, Value *V, Argument *A)
    {
      if (V == A) return true;
      if (Instruction *inst = dyn_cast<Instruction> (V))
      {
        for (auto v : overlap1)
          if (std::get<0>(v) == V && std::get<1>(v) == A) return false;
        overlap1.push_back(std::tuple<Value *, Argument *>(V,A));

        errs() << "       - (" << F->getName() << ") " << *inst << "\n";

        if (PHINode *phi = dyn_cast<PHINode> (inst))
        {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
          {
            if (followFunctionRoot(F, phi->getIncomingValue(i), A) ||
              followFunctionFromBB(F, phi->getIncomingValue(i), A))
              return true;
          }
        }
        else if (CallInst *ci = dyn_cast<CallInst> (inst))
        {
          for (auto arg : followFunction(ci->getCalledFunction()))
          {
            unsigned num = arg->getArgNo();

            if (followFunctionRoot(F, ci->getOperand(num), A) ||
              followFunctionFromBB(F, ci->getOperand(num), A))
              return true;
          }
        }
        else 
        {
          for (unsigned i = 0; i < inst->getNumOperands(); i++)
          {
            if (followFunctionRoot(F, inst->getOperand(i), A) ||
              followFunctionFromBB(F, inst->getOperand(i), A))
              return true;
          }
        }
      }
      return false;
    }

    bool followFunctionFromBB(Function *F, Value *V, Argument *A)
    {
      for (auto& bb : *F)
      {
        for (auto& i : bb)
        {
          if (StoreInst* si = dyn_cast<StoreInst> (&i))
          {
            if (si->getPointerOperand() == V)
            {
              if (followFunctionRoot(F, si->getValueOperand(), A))
                return true;
            }
          }
          else if (LoadInst* si = dyn_cast<LoadInst> (&i))
          {
            if (si->getPointerOperand() == V)
            {
              if (followFunctionRoot(F, si, A))
                return true;
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            Function *CF = ci->getCalledFunction();
            for (auto arg = CF->arg_begin(); arg != CF->arg_end(); arg++)
            {
              if (ci->getOperand(arg->getArgNo()) == V && arg->getType()->isPointerTy())
              {
                for (auto value : findReferenceable(CF, arg))
                  if (followFunctionRoot(F, ci->getOperand(value->getArgNo()), A))
                    return true;
                break;
              }
            }
          }
        }
      }
      return false;
    }
    
    ///-------------------------------------------------
    ///
    /// Interprocedural Scope - (2) referenceable
    ///
    ///-------------------------------------------------
    
    std::vector<std::tuple<Value *, Argument *>> overlap2;

    std::vector<Argument *> findReferenceable(Function *F, Argument *A)
    {
      std::vector<Argument *> vec;
      for (auto arg = F->arg_begin(); arg != F->arg_end(); arg++)
        if (arg != A)
        {
          overlap2.clear();
          if (findReferenceableUpBottom(F, arg, A))
            vec.push_back(arg);
        }
      errs() << "size: (" << F->getName() << ")" << vec.size() << " :" << *A <<"\n";
      return vec;
    }

    bool findReferenceableUpBottom(Function *F, Value *V, Argument *A)
    {
      if (V == A) return true;

      for (auto v : overlap2)
        if (std::get<0>(v) == V && std::get<1>(v) == A) return false;
      overlap2.push_back(std::tuple<Value *, Argument *>(V,A));

      errs() << "         - (" << F->getName() << ") " << *V << "\n";
      
      for (auto& bb : *F)
      {
        for (auto& i : bb)
        {
          if (StoreInst *si = dyn_cast<StoreInst> (&i))
          {
            if (si->getValueOperand() == V)
            {
              if (findReferenceableUpBottom(F, si->getPointerOperand(), A))
                return true;
            }
            else if (si->getPointerOperand() == V)
            {
              if (findReferenceableUpBottom(F, si->getValueOperand(), A))
                return true;
            }
          }
          else if (LoadInst *li = dyn_cast<LoadInst> (&i))
          {
            if (li->getPointerOperand() == V)
            {
              if (findReferenceableUpBottom(F, li, A))
                return true;
            }
          }
          else if (CallInst *ci = dyn_cast<CallInst> (&i))
          {
            Function *CF = ci->getCalledFunction();
            for (auto arg = CF->arg_begin(); arg != CF->arg_end(); arg++)
            {
              if (ci->getOperand(arg->getArgNo()) == V)
              {
                for (auto argv : followFunction(CF))
                {
                  if (argv->getArgNo() == arg->getArgNo())
                  {
                    if (findReferenceableUpBottom(F, &i, A))
                      return true;
                    break;
                  }
                }
                break;
              }
            }
          }
          else
          {
            for (unsigned j = 0; j < i.getNumOperands(); j++)
            {
              if (i.getOperand(j) == V)
                if (findReferenceableUpBottom(F, &i, A))
                  return true;
            }
          }
        }
      }

      if (Instruction *inst = dyn_cast<Instruction> (V))
      {
        if (PHINode *phi = dyn_cast<PHINode> (inst))
        {
          for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
          {
            if (findReferenceableUpBottom(F, phi->getIncomingValue(i), A))
              return true;
          }
        }
        else if (CallInst *ci = dyn_cast<CallInst> (inst))
        {
          for (auto arg : followFunction(ci->getCalledFunction()))
          {
            unsigned num = arg->getArgNo();

            if (findReferenceableUpBottom(F, ci->getOperand(num), A))
              return true;
          }
        }
        else 
        {
          for (unsigned i = 0; i < inst->getNumOperands(); i++)
          {
            if (findReferenceableUpBottom(F, inst->getOperand(i), A))
              return true;
          }
        }
      }
      
      /*std::vector<std::tuple<Value *, Argument *>> tmp = overlap1;
      overlap1.clear();
      if (followFunctionRoot(F, V, A))
        return true;
      overlap1 = tmp;*/

      return false;
    }

  };
  
  struct CustomPass : public FunctionPass {
    static char ID;

    CustomPass()
      : FunctionPass(ID)
    {
    }

    bool runOnFunction(Function &F) override
    {
      errs() << "\n";
      errs() << "Function " << F.getName() + "\n";
      std::vector<Value *> v = findAnnotation(F);
      DependencyManager dm(&F, v);
      const std::map<Value *, std::vector<Instruction *>>& insts = dm.findDependency();
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

  };

}

char CustomPass::ID = 0;
static RegisterPass<CustomPass> X("custom", "CustomPass");
