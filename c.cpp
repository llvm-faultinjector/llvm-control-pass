#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
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
      LoopInfo *LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
      //getAnalysis<LoopInfoWrapperPass>().print(errs());
      errs() << "\n";
      errs() << "Function " << F.getName() +"\n";
      for (Loop *L :*LI)
        InLoop(L, 0);
      //auto& argList = F.getArgumentList();
      //errs() << " ArgSize : " << F.arg_size() + "\n";
      errs() << "\n";
      return false;
    }
    void InLoop(Loop *L, unsigned nest)
    {
      errs() << "Level(" << nest << ")\n";
      /*BasicBlock *header = L->getHeader();
      errs() << " Header Block: " << header->getName() << "\n";
      BasicBlock::iterator h_iter;
      for ( h_iter = header->begin(); h_iter != header->end(); ++h_iter)
	{
	  /*if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*h_iter))
	    {
	      h_iter->print(errs());
	      errs() << "\n";
	      InBranch(cmpInst);
	      }
	  h_iter->print(errs());
	  errs() << '\n';
	  for (int i = 0; i < h_iter->getNumOperands(); i++)
	    errs() << std::addressof(*h_iter->getOperand(i)) << " ";
	  errs() << '\n';
	}*/

      for ( auto bb : L->getBlocks() )
	{
	  std::vector<std::string> strs;
	  std::vector<Value *> values;
	  errs() << " Block Name: " << bb->getName() << "\n";
	  BasicBlock::iterator h_iter;
	  for ( h_iter = bb->begin(); h_iter != bb->end(); ++h_iter)
	    {
	      //h_iter->print(errs());
	      //errs() <<"\t\t\t";// '\n';
	      //if (LoadInst *LI = dyn_cast<LoadInst>(h_iter))
	      //errs() << std::addressof(*h_iter) << "; ";//(uint32_t)(LI->getSyncScopeID()) << "; ";
		if (LoadInst *LI = dyn_cast<LoadInst>(h_iter))
		  {
		    h_iter->print(errs());
		    errs() << '\n';
		    strs.push_back(h_iter->getOperand(0)->getName());
		    // push address of load instruction, not operand of load instruction
		    values.push_back(LI);
		  }
		else
		  {
		    for (int i = 0; i < h_iter->getNumOperands(); i++){
		      //for (int j = 0; j < values.size(); j++){
			/*if ( values[j] == h_iter->getOperand(i) )
			  {
			  h_iter->print(errs());
			  errs() << "(" << strs[j] << ")\n";
			  }*/
			h_iter->print(errs());
		        errs() << "(" << h_iter->getOperand(i)->getName() << ")\n";
		      }
		    
		  }
	      for (int i = 0; i < h_iter->getNumOperands(); i++)
		{
		  //if (auto *v = dyn_cast<MetadataAsValue>(h_iter->getOperand(i)))
		  //if (h_iter->getOperand(i)->hasName())
		  //errs() << "("<< h_iter->getOperand(i)->getName() <<")";
		  //errs() << std::addressof(*h_iter->getOperand(i)) << " ";
		}
	      //errs() << '\n';
	    }
	  errs() << '\n';
	}
      
      std::vector<Loop *> subLoops = L->getSubLoops();
      Loop::iterator j, f;
      for (j = subLoops.begin(), f = subLoops.end(); j != f; ++j)
        InLoop(*j, nest + 1);
    }
    void InBranch(CmpInst *cmpInst)
    {
      errs() << "  Value 1: ";
      PrintValue(cmpInst->getOperand(0));
      errs() << "  Value 2: ";
      PrintValue(cmpInst->getOperand(1));
      errs() << "  Predicate: " << cmpInst->getPredicate() << "\n";
    }
    void PrintValue(Value *v)
    {
      v->dump();
    }
    void PrintX(CmpInst *ci)
    {
      errs() << "   opcode name: " << ci->getOpcodeName() << "\n";
      if (ci->hasMetadata())
	errs() << "  has metadata!!" << "\n";
    }
  };
}

char LoopCheck::ID = 0;
static RegisterPass<LoopCheck> X("loopcheck", "Hello World Pass");
