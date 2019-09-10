#include "llvm/Support/raw_ostream.h"
#include "Dataflow.h"

#include <queue>

namespace llvm {
    Index Dataflow::domainIndex (void* ptr) {
        Domain &D = this->domain;
        auto it = std::find(D.begin(), D.end(), ptr);
        Index i = std::distance(D.begin(), it);
        if (i >= this->domain.size() || i < 0) {
            i = INDEX_NOT_FOUND;
        }
        return i;
    }

    void error (std::string err) {
        errs() << "\n[!] " << err << "\n";
        exit(-1);
    }

    // store it to a cutomize type
    Expression::Expression (Instruction *ins) {
        if (auto bin_op = dyn_cast<BinaryOperator> (ins)) {
            this->op = bin_op->getOpcode();
            this->v1 = bin_op->getOperand(0);
            this->v2 = bin_op->getOperand(1);
        } else {
            error ("BinaryOperator Instruction Only");
        }
    }

    bool Expression::operator== (const Expression &exp) {
        return (exp.op == this->op && exp.v1 == this->v1 
            && exp.v2 == this->v2);
    }

    // code from https://github.com/jarulraj/llvm/ , the find name is too trivial...
    std::string Expression::toString () {
        std::string op = "?";
        switch (this->op) {
        case Instruction::Add:
        case Instruction::FAdd:
            op = "+";
            break;
        case Instruction::Sub:
        case Instruction::FSub:
            op = "-";
            break;
        case Instruction::Mul:
        case Instruction::FMul:
            op = "*";
            break;
        case Instruction::UDiv:
        case Instruction::FDiv:
        case Instruction::SDiv:
            op = "/";
            break;
        case Instruction::URem:
        case Instruction::FRem:
        case Instruction::SRem:
            op = "%";
            break;
        case Instruction::Shl:
            op = "<<";
            break;
        case Instruction::AShr:
        case Instruction::LShr:
            op = ">>";
            break;
        case Instruction::And:
            op = "&";
            break;
        case Instruction::Or:
            op = "|";
            break;
        case Instruction::Xor:
            op = "xor";
            break;
        default:
            op = "op";
            break;
        }
        return getValueName(v1) + " " + op + " " + getValueName(v2);
    }

    // gmerate meet opeartion for in/out
    VSet Dataflow::applyMeet (VList input) {
        VSet result;

        if (input.empty()) {
            return result;
        }

        for (auto o : input) {
            // UNION: simply insert all number to set
            if (meetop == MeetOp::UNION) {
                result.insert(o.begin(), o.end());

            // INTERSECTION: remove value occurs more than once, otherwise insert it
            } else if (meetop == MeetOp::INTERSECT) {
                if (result.empty()) {
                    result.insert(o.begin(), o.end());
                    continue;
                }

                VSet tmp;
                for (auto value : o) {
                    auto redundant = result.find(value);
                    if (redundant != result.end()) {
                        tmp.insert(value);
                    }
                }
                result = tmp;
            }
        }

        return result;         
    }

    DataFlowResult Dataflow::run (Function &F, VSet boundary, VSet interior) {
        std::map<BasicBlock*, BlockResult> result;
        Domain &domain = this->domain;
        VSet base;

        // initialize the first Block we need to iterate accoring to direction
        BBList initList, traverseList;
        switch (direction) {
            case Direction::FORWARD:
                initList.push_back(&F.front());
                break;
            case Direction::BACKWARD:
                for (auto &BB : F) {
                    if(isa<ReturnInst> (BB.getTerminator())) {
                        initList.push_back(&BB);
                    }
                }
                break;
            default:
                error("Unknown Direction");
                break;
        }

        // push other blocks into the list
        std::map<BasicBlock*, BBList> neighbors;
        switch (direction) {
            case Direction::FORWARD:
                for (auto &BB : F) {
                    for (auto pred_BB = pred_begin(&BB); pred_BB != pred_end(&BB); ++pred_BB) {
                        neighbors[&BB].push_back(*pred_BB);
                    }
                }
                break;
            case Direction::BACKWARD:
                for (auto &BB : F) {
                    for (auto succ_BB = succ_begin(&BB); succ_BB != succ_end(&BB); ++succ_BB) {
                        neighbors[&BB].push_back(*succ_BB);
                    }
                }
                break;
            default:
                error("Unknown Direction");
                break;
        }

        // initialize boudary set value
        BlockResult boundaryRes = BlockResult();
        if (direction == Direction::FORWARD) {
            boundaryRes.in = boundary;
            base = boundary;
        } else {
            boundaryRes.out = boundary;
        }

        for (auto BB : initList) {
            result.insert(std::pair<BasicBlock*, BlockResult>(BB, boundaryRes));
        }

        // initalize interior set value
        BlockResult interiorRes = BlockResult();
        if (direction == Direction::FORWARD) {
            interiorRes.out = interior;
        } else {
            interiorRes.in = interior;
            base = interior;
        }

        for (auto &BB : F) {
            if (result.find(&BB) == result.end()) {
                result.insert(std::pair<BasicBlock*, BlockResult>(&BB, interiorRes));
            }
        }

        // Use BFS to determine traverse order
        std::set<BasicBlock*> visited;
        while (!initList.empty())
        {
            BasicBlock *currBB = initList[0];
            traverseList.push_back(currBB);
            initList.erase(initList.begin());
            visited.insert(currBB);

            switch (direction)
            {
            case FORWARD:
                for (auto succ_BB = succ_begin(currBB); succ_BB != succ_end(currBB); ++succ_BB)
                {
                    if (visited.find(*succ_BB) == visited.end())
                    {
                        initList.push_back(*succ_BB);
                    }
                }
                break;
            case BACKWARD:
                for (auto prev_BB = pred_begin(currBB); prev_BB != pred_end(currBB); ++prev_BB)
                {
                    if (visited.find(*prev_BB) == visited.end())
                    {
                        initList.push_back(*prev_BB);
                    }
                }
                break;
            default:
                error("Unknown Direction");
                break;
            }
        }

        // fixed point algorithm, iterate until it does not change
        bool converged = false;
        while (!converged) {
            converged = true;

            for (auto currBB : traverseList) {
                // we use calculate meet value first
                VList meetInput;

                // if we have to initialize with some values
                if (direction == BACKWARD && isa<ReturnInst>(currBB->getTerminator())) {
                    meetInput.push_back(base);
                }

                if (direction == FORWARD && currBB == &F.front()) {
                    meetInput.push_back(base);
                }
                
                for (auto n : neighbors[currBB]) {
                    VSet value;
                    if (direction == Direction::FORWARD) {
                        value = result[n].out;
                    } else {
                        value = result[n].in;
                    }
                    meetInput.push_back(value);
                }
                
                // then is transfer value
                VSet meetResult = applyMeet(meetInput);
                if (direction == Direction::FORWARD) {
                    result[currBB].in = meetResult;
                } else {
                    result[currBB].out = meetResult;
                }

                VSet *blockInput = (direction == Direction::FORWARD) 
                    ? &result[currBB].in : &result[currBB].out;
                TransferOutput transferRes = transferFn (*blockInput, currBB);
                VSet *blockOutput = (direction == Direction::FORWARD) 
                    ? &result[currBB].out : &result[currBB].in; 

                // check if previous result and the transfer result are the same
                if (converged) {
                    if (transferRes.transfer != *blockOutput ||
                        result[currBB].transferOutput.neighbor.size() != transferRes.neighbor.size()) {
                        converged = false;
                    }
                }

                // update value
                *blockOutput = transferRes.transfer;
                result[currBB].transferOutput.neighbor = transferRes.neighbor;
            }
        }

        DataFlowResult analysis;
        analysis.result = result;

        return analysis;
    }

    // code from https://github.com/jarulraj/llvm/ , the find name is too trivial...
    std::string getValueName (Value *v) {
        // If we can get name directly
        if (v->getName().str().length() > 0) {
            return "%" + v->getName().str();
        } else if (isa<Instruction>(v)) {
            std::string s = "";
            raw_string_ostream *strm = new raw_string_ostream(s);
            v->print(*strm);
            std::string inst = strm->str();
            size_t idx1 = inst.find("%");
            size_t idx2 = inst.find(" ", idx1);
            if (idx1 != std::string::npos && idx2 != std::string::npos && idx1 == 2) {
                return inst.substr(idx1, idx2 - idx1);
            } else {
                // nothing match
                return "";
            }
        } else if (ConstantInt *cint = dyn_cast<ConstantInt>(v)) {
            std::string s = "";
            raw_string_ostream *strm = new raw_string_ostream(s);
            cint->getValue().print(*strm, true);
            return strm->str();
        } else {
            std::string s = "";
            raw_string_ostream *strm = new raw_string_ostream(s);
            v->print(*strm);
            std::string inst = strm->str();
            return "\"" + inst + "\"";
        }
    }
};