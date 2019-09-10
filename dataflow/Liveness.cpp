#include "Dataflow.h"

using namespace llvm;
namespace {
    class Liveness : public FunctionPass {
        public:
        static char ID;
        
        virtual void getAnalysisUsage (AnalysisUsage& AU) const {
            AU.setPreservesAll();
        }

        Liveness() : FunctionPass(ID) { }

        virtual bool runOnFunction (Function &F) {
            outs() << "Function: " << F.getName() << "\n";
            DataFlowResult result;
            Domain domain;

            // add all used values to the set
            for (auto I = inst_begin(F); I != inst_end(F); ++I) {
                if (Instruction* inst = dyn_cast<Instruction> (&*I)) {
                    for (auto OI = inst->op_begin(); OI != inst->op_end(); ++OI) {
                        Value *val = *OI;

                        if (isa<Instruction> (val) || isa<Argument> (val)) {
                            if (std::find(domain.begin(), domain.end(), val) == domain.end()) {
                                domain.push_back(val);
                            }
                        }
                    }
                }
            }

            // initialize analysis
            Analysis analysis  = Analysis(Direction::BACKWARD, MeetOp::UNION, domain);
            VSet boudary = VSet(), interior = VSet();
            result = analysis.run(F, boudary, interior);

            outs() << "Function: " << F.getName() << "\n";
            // We have got in/out for each block, now we need to analyze each instruction
            for (auto &BB : F) {
                outs() << "; " << BB.getName() << "\n";
                VSet live = result.result[&BB].out;
                std::vector<std::pair<Instruction *,std::string>> output;
                for (auto inst = BB.rbegin(); inst != BB.rend(); ++inst) {
                    Instruction *I = &*inst;
                    // PHINode is not a real node, so no need to add liveness behind it
                    if (auto phi = dyn_cast<PHINode>(&*inst)) {
                        Index i = analysis.domainIndex(phi);
                        auto LHS = live.find(i);
                        // if something has been redefines, killed it
                        if (LHS != live.end()) {
                            live.erase(LHS);
                        }
                        output.push_back(std::pair<Instruction*, std::string>(I, ""));
                    } else {
                        for (auto op = inst->op_begin(); op != inst->op_end(); ++op) {
                            Value *val = *op;
                            // find live varaible 
                            if (isa<Instruction>(val) || isa<Argument>(val)) {
                                Index i = analysis.domainIndex(val);
                                if (i != analysis.INDEX_NOT_FOUND) {
                                    live.insert(i);
                                }
                            }
                        }

                        // killed the redefined varaible
                        Index i = analysis.domainIndex(&*inst);
                        auto iter = live.find(i);
                        if (iter != live.end()) {
                            live.erase(iter);
                        }

                        // pretty print  
                        std::string s = "  {";
                        for (auto val : live) {
                            s += getValueName((Value *) analysis.domain[val]);
                            s += " ";
                        }
                        s += "}";
                        output.push_back(std::pair<Instruction*, std::string>(I, s));
                    }
                }

                for (auto o = output.rbegin(); o != output.rend(); ++o) {
                    outs() << *o->first << o->second << "\n";
                }

                outs() << "\n";
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

                VSet use, def;
                for (auto inst = curr->begin(); inst != curr->end(); ++inst) {
                    if (PHINode *phi = dyn_cast<PHINode> (&*inst)) {
                        for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
                            auto val = phi->getIncomingValue(i);
                            if (isa<Instruction>(val) || isa<Argument>(val)) {
                                // add instructions to neighbor
                                auto incomingBlock = phi->getIncomingBlock(i);
                                if (output.neighbor.find(incomingBlock) == output.neighbor.end()) {
                                    output.neighbor.insert(std::pair<BasicBlock*, VSet>(incomingBlock, VSet()));
                                }
                                
                                Index index = domainIndex(val);
                                output.neighbor[incomingBlock].insert(index);
                            }
                        }
                    } else {
                        for (auto op = inst->op_begin(); op != inst->op_end(); ++op) {
                            Value *val = *op;
                            if (isa<Instruction> (val) || isa<Argument> (val)) {
                                // if previous defined and used now, add to use set
                                Index index = domainIndex(val);
                                if (index != INDEX_NOT_FOUND) {
                                    if (def.find(index) == def.end()) {
                                        use.insert(index);
                                    }
                                }
                            }
                        }
                    }
                    
                    // insert to defined set
                    Index i = domainIndex(&*inst);
                    if (i != INDEX_NOT_FOUND) {
                        def.insert(i);
                    }
                }

                // (input - def) U use
                auto tmp = substractSet(input, def);
                output.transfer = unionSet(use, tmp);
                return output;
            }
        };
    };

    // register liveness analysis pass
    char Liveness::ID = 0;
    RegisterPass<Liveness> X("liveness", "Liveness Analysis");
}