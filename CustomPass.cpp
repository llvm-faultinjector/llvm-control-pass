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
		//std::vector<Value *> v = findAnnotation(F);
		//for (auto s : v)
		//	findDependency(s, F);
		//std::vector<Argument *> args = findFunctionArgumentReturnDependency(F);
		//for ( auto arg : args )
		//    errs() << " - Argument \"" << arg->getName() << "\" affect the return value.\n";
		findFunctionCallByReferenceDependency(F);
		errs() << "\n";
		return false;
	}
	
	// get annotated message
	// int __attribute__((annotate("xxx"))) a; => "xxx"
	StringRef getAnnotatedString(CallInst *c)
	{
		Constant *cs = (cast<GlobalVariable>((cast<ConstantExpr>(c->getArgOperand(1)))->getOperand(0)))->getInitializer();
		return cast<ConstantDataArray>(cs)->getAsCString();
	}
	
	// find all annotated valuable
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
			printFollowNodes(F, v);
		errs() << "\n";
		dependency_map.insert(std::pair<Value *, std::vector<Instruction *>>(V, dependency));
		dependency.clear();
	}
	void printFollowNodes(Function &F, Value *X)
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
				{
					printFollowNodes(F, phi->getIncomingValue(i));
					printFollowNodesByStoreInst(F, phi->getIncomingValue(i));
				}
			}
			else
			{			
				for (unsigned i = 0; i < inst->getNumOperands(); i++)
				{
					printFollowNodes(F, inst->getOperand(i));
					printFollowNodesByStoreInst(F, phi->getOperand(i));
				}
			}
		}
	}
	void printFollowNodesByStoreInst(Function &F, Value *v)
	{
		for ( auto& bb : F )
		{
			for ( auto& i : bb )
			{
				if (StoreInst* si = dyn_cast<StoreInst> (&i))
				{
					if (si->getPointerOperand() == v)
					{
					    printFollowNodes(F, si);
					}
				}
			}
		}
	}
	
	// Checks which arguments affect the return value of a function.
	std::vector<Argument *> findFunctionArgumentReturnDependency(Function &F)
	{
	    std::vector<Argument *> vec;
	    if (F.doesNotReturn())
	        return vec; 
	    std::vector<Argument *> args;
	    for (auto arg = F.arg_begin(); arg != F.arg_end(); arg++)
	        args.push_back(arg);
		for ( auto& bb : F )
		{
			for ( auto& i : bb )
			{
			    if (ReturnInst *RI = dyn_cast<ReturnInst>(&i))
			    {
			        for ( auto arg : args )
			        {
			            if (_InternalfindFunctionArgumentReturnDependency(F, RI->getOperand(0), arg))
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
			if ( PHINode *phi = dyn_cast<PHINode> (inst) )
			{
				for (unsigned i = 0; i < phi->getNumIncomingValues(); i++)
				{
					if (_InternalfindFunctionArgumentReturnDependency(F, phi->getIncomingValue(i), arg) ||
					    _InternalfindFunctionArgumentReturnDependencyByStoreInst(F, phi->getIncomingValue(i), arg))
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
		for ( auto& bb : F )
		{
			for ( auto& i : bb )
			{
				if (StoreInst* si = dyn_cast<StoreInst> (&i))
				{
					if (si->getPointerOperand() == v)
					{
					    if (_InternalfindFunctionArgumentReturnDependency(F, si->getValueOperand(), arg))
					        return true;
					}
				}
			}
		}
		return false;
	}
	
	std::vector<Argument *> findFunctionCallByReferenceDependency(Function &F)
	{
	    std::vector<Argument *> vec;
	    // find dereferenceable variable
	    std::vector<Argument *> args;
	    for (auto arg = F.arg_begin(); arg != F.arg_end(); arg++)
	    {
	        if (arg->getType()->isPointerTy())
	            if (arg->getDereferenceableBytes() > 0)
	            {
	                errs() << " - derefer: " << arg->getName() << "\n";
	                args.push_back(arg);
	            }
	    }
	    return vec;
	}

  };
}

char CustomPass::ID = 0;
static RegisterPass<CustomPass> X("custom", "CustomPass");
