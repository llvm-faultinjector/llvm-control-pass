#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"
#include "llvm/Analysis/LoopInfo.h"
#include <vector>

using namespace llvm;

namespace {
  struct LoopCheck : public FunctionPass {
    static char ID;
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<LoopInfoWrapperPass>();
    }
    LoopCheck() : FunctionPass(ID) {}
    bool runOnFunction(Function &F) override {
      errs() << "\n";
      errs() << "Function " << F.getName() +"\n";
	  std::vector<Value *> v = findAnnotation(F);
	  for (auto s : v)
		findDependency(s, F);//errs() << "  " << s->getName() << "\n";
      errs() << "\n";
      return false;
    }
	std::vector<Value *> findAnnotation(Function &F)
	{
		std::vector<Value *> annotate;
		for ( auto& k : F.getEntryBlock() )
		{
			if ( CallInst *c = dyn_cast<CallInst> (&k) )
			{
				Function *cf = c->getCalledFunction();
				if ( cf->getName() == "llvm.var.annotation")
				{
					annotate.push_back((cast<BitCastInst>(c->getArgOperand(0)))->getOperand(0));
				}
			}
		}
		return annotate;
	}
	std::vector<Instruction *> findDependency(Value *V, Function &F)
	{
		errs() << " -Find dependency value : " << V->getName() << "\n";

		errs() << "   * store\n";
		std::vector<Instruction *> insts;
		std::vector<Value *> step1;
		for ( auto& bb : F )
			for ( auto& i : bb )
			{
				if (StoreInst* si = dyn_cast<StoreInst> (&i))
					if (si->getPointerOperand() == V)
					{
						errs() << "     - " << bb.getName() << "\n      - ";
						i.print(errs());
						errs() << "\n";
						step1.push_back(si->getValueOperand());
					}
			}

		errs() << "\n   * follow nodes\n";
		for ( auto& v : step1 )
			printFollowNodes(v);
		return insts;
	}
	void printFollowNodes(Value *X)
	{
		if (Instruction *inst = dyn_cast<Instruction> (X))
		{
			errs() << "     - ";
			inst->print(errs());
			errs() << "\n";
			for (int i = 0; i < inst->getNumOperands(); i++)
				printFollowNodes(inst->getOperand(i));
		}
	}

  };
}

char LoopCheck::ID = 0;
static RegisterPass<LoopCheck> X("loopcheck", "Hello World Pass");
