#include "Dataflow.h"

using namespace llvm;
namespace {
    class Reaching : public FunctionPass {
        public:
        static char ID;

        virtual void getAnalysisUsage (AnalysisUsage& AU) const {
            AU.setPreservesAll();
        }

        Reaching() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F) {
            outs() << "Function: " << F.getName() << "\n";
            DataFlowResult result;
            Domain domain;

            VSet boudary, interior;
            // use functions arguments to initialize
            for (auto arg = F.arg_begin(); arg != F.arg_end(); ++arg) {
                domain.push_back(&*arg);
                boudary.insert(domain.size() - 1);
            }
            
            // add remaining instruction
            for (auto I = inst_begin(F); I != inst_end(F); ++I) {
                if (Value* val = dyn_cast<Value> (&*I)) {
                    if (getValueName(val) != "") {
                        if (std::find(domain.begin(), domain.end(), val) == domain.end()) {
                            domain.push_back(val);
                        }
                    }
                }
            }

            Analysis analysis  = Analysis(Direction::FORWARD, MeetOp::UNION, domain);
            result = analysis.run(F, boudary, interior);
            // output in/out
            for (auto &BB : F) {
                outs () << "\n<" << BB.getName() << ">\n";
                outs () << "in: ";
                for (auto i : result.result[&BB].in) {
                    outs() << getValueName((Value *)analysis.domain[i]) << " ";
                }
                outs () <<"\nout: ";
                for (auto i : result.result[&BB].out) {
                    outs() << getValueName((Value *)analysis.domain[i]) << " ";
                }            
                outs () << "\n";    
            }

            return false;
        }

        private:
        class Analysis : public Dataflow {
            public:
            Analysis (Direction direction, MeetOp meetop, Domain domain)
                : Dataflow (direction, meetop, domain) {}
            
            TransferOutput transferFn (VSet input, BasicBlock *curr) {
                TransferOutput output;
                VSet gen, kill;
                for (auto inst = curr->begin(); inst != curr->end(); ++inst) {
                    std::string val = "";
                    if (isa<Instruction> (&*inst)) {
                        val = getValueName(&*inst);
                    }

                    if (auto load_ins = dyn_cast<StoreInst>(&*inst)) {
                        val = getValueName(load_ins->getPointerOperand());
                    }

                    if (val == "") {
                        continue;
                    }

                    // if input has be re-assigned, kill it
                    for (auto LHS : input) {
                        if (isa<Instruction> (&*inst)) {
                            if (getValueName((Value *) domain[LHS]) == val) {
                                kill.insert(LHS);
                            }
                        }      
                    }

                    // some values are killed in the same Basic Block, so sad
                    auto tmp = gen.end();
                    for (auto i = gen.begin(); i != gen.end(); ++i) {
                        if (tmp != gen.end()) {
                            gen.erase(tmp);
                            tmp = gen.end();
                        }
                        if (getValueName((Value *) domain[*i]) == val) {
                            tmp = i;
                        }
                    }

                    Index i = domainIndex(&*inst);
                    if (i != INDEX_NOT_FOUND) {
                        gen.insert(i);
                    }
                }

                // (input - kill) U gen  
                VSet tmp = substractSet(input, kill);
                output.transfer = unionSet(tmp, gen);
                return output;
            }
        };
    };

    // register pass
    char Reaching::ID = 0;
    RegisterPass<Reaching> X("reaching", "Reaching Definition");
}