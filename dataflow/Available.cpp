#include "Dataflow.h"

using namespace llvm;
namespace {
    class Available : public FunctionPass {
        public:
        static char ID;

        virtual void getAnalysisUsage (AnalysisUsage& AU) const {
            AU.setPreservesAll();
        }

        Available() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F) {
            outs() << "Function: " << F.getName() << "\n";
            DataFlowResult result;
            Domain domain;

            // we only evaluate Expressions
            for (auto I = inst_begin(F); I != inst_end(F); ++I) {
                if (isa<BinaryOperator> (*I)) {
                    domain.push_back(new Expression(&*I));
                }
            }

            Analysis analysis = Analysis(Direction::FORWARD, MeetOp::INTERSECT,  domain);
            result = analysis.run(F, VSet(), VSet());

            // output the result
            int index = 0;
            for (auto &BB : F) {
                outs() << "\n<" << BB.getName() << ">\n";
                VSet gen = result.result[&BB].in;
                for (auto &I : BB) {
                    outs() << index << ": " << I;

                    std::string val = getValueName(&I);
                    
                    // killed redefined expressions
                    auto tmp = gen.end();
                    for (auto i = gen.begin(); i != gen.end(); ++i) {
                        if (tmp != gen.end()) {
                            gen.erase(tmp);
                            tmp = gen.end();
                        }
                        Expression* exp = (Expression *) domain[*i];
                        std::string left = getValueName(exp->v1), right = getValueName(exp->v2);                        
                        if (left == val || right == val) {
                            tmp = i;
                        }
                    }
                    
                    // insert expressions
                    if (isa<BinaryOperator> (&I)) {
                        Expression exp = Expression(&I);
                        int i = 0;
                        for (auto e : domain) {
                            if (exp == *(Expression *)(e)) {
                                break;
                            }
                            i++;
                        }
                        gen.insert(i);
                    }
                    
                    // pretty print
                    outs() << "\t{";
                    for (auto i : gen) {
                        outs() << ((Expression *) domain[i])->toString() << ", ";
                    }
                    outs() << "}\n";

                    index++;
                }    
            }
            // clean up
            for (auto exp : domain) {
                delete (Expression *) exp;
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
                    std::string val;
                    if (isa<Instruction> (&*inst)) {
                        val = getValueName(&*inst);
                    }

                    if (auto load_ins = dyn_cast<StoreInst>(&*inst)) {
                        val = getValueName(load_ins->getPointerOperand());
                    }
                    
                    // skip if it's not a value
                    if (val == "") {
                        continue;
                    }

                    // if input has be re-assigned, kill it
                    for (auto LHS : input) {
                        Expression* exp = (Expression *) domain[LHS];
                        std::string left = getValueName(exp->v1), right = getValueName(exp->v2);
                        if (left == val || right == val) {
                            kill.insert(LHS);
                        }    
                    }

                    // some values are killed in the same Basic Block, so sad
                    auto tmp = gen.end();
                    for (auto i = gen.begin(); i != gen.end(); ++i) {
                        if (tmp != gen.end()) {
                            gen.erase(tmp);
                            tmp = gen.end();
                        }
                        Expression* exp = (Expression *) domain[*i];
                        std::string left = getValueName(exp->v1), right = getValueName(exp->v2);                        
                        if (left == val || right == val) {
                            tmp = i;
                        }
                    }

                    // insert to generate set
                    if (isa<BinaryOperator> (&*inst)) {
                        Expression exp = Expression(&*inst);
                        int i = 0;
                        for (auto e : domain) {
                            if (exp == *(Expression *)(e)) {
                                break;
                            }
                            i++;
                        }

                        if (i < domain.size()) {
                            gen.insert(i);
                        }
                    }
                }

                auto tmp = substractSet(input, kill);
                output.transfer = unionSet(tmp, gen);
                return output;
            }
        };
    };

    char Available::ID = 0;
    RegisterPass<Available> X("available", "Available Expression");
}