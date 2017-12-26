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
		errs() << "Function " << F.getName() +"\n";
		std::vector<Value *> v = findAnnotation(F);
		for (auto s : v)
			findDependency(s, F);
		errs() << "\n";
		return false;
	}
	StringRef getAnnotatedString(CallInst *c)
	{
		Constant *cs = (cast<GlobalVariable>((cast<ConstantExpr>(c->getArgOperand(1)))->getOperand(0)))->getInitializer();
		return cast<ConstantDataArray>(cs)->getAsCString();
	}
	std::vector<Value *> findAnnotation(Function &F)
	{
		std::vector<Value *> annotate;
		for ( auto& bb : F )
		{
			for ( auto& k : bb)
			{
				if ( CallInst *c = dyn_cast<CallInst> (&k) )
				{
					Function *cf = c->getCalledFunction();
					if ( cf->getName() == "llvm.var.annotation")
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
	void findDependency(Value *V, Function &F)
	{
		errs() << " - Dependent value : " << V->getName() << "\n";

		errs() << "   * store\n";
		std::vector<Value *> step;
		for ( auto& bb : F )
		{
			for ( auto& i : bb )
			{
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
			}
		}
		errs() << "\n   * follow nodes\n";
		for ( auto& v : step )
			printFollowNodes(v);
		errs() << "\n";
		dependency_map.insert(std::pair<Value *, std::vector<Instruction *>>(V, dependency));
		dependency.clear();
	}
	void printFollowNodes(Value *X)
	{
		if (Instruction *inst = dyn_cast<Instruction> (X))
		{
			// check if exist already
			for (auto& v : dependency)
				if ( v == inst ) return;
			dependency.push_back(inst);
			errs() << "     - ";
			inst->print(errs());
			errs() << "\n";
			if ( PHINode *phi = dyn_cast<PHINode> (inst) )
			{
				for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
					printFollowNodes(phi->getIncomingValue(i));
			}
			else
			{			
				for (unsigned i = 0; i < inst->getNumOperands(); i++)
					printFollowNodes(inst->getOperand(i));
			}
		}
	}

  };
}

char CustomPass::ID = 0;
static RegisterPass<CustomPass> X("custom", "Hello World Pass");
