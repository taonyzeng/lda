#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/CFG.h"

#include <vector>
#include <set>

namespace llvm {
	// cutomize errors for quiting
	void error(std::string);

	// Expression for storing BinaryInstruction, easier for comparasion
	class Expression {
		public:
		Value *v1, *v2;
		Instruction::BinaryOps op;
		Expression (Instruction *ins);
		bool operator== (const Expression &exp);
		std::string toString();
	};

	// ADT for storing BasicBlock and Instructions
	typedef int Index;
	typedef std::vector<BasicBlock*> BBList;
	typedef std::set<Index> VSet;
	typedef std::vector<VSet> VList;
	typedef std::vector<void*> Domain;

	// union two set
	template<typename T> std::set<T> unionSet (std::set<T> s1, std::set<T> s2) {
		std::set<T> result;
		result.insert(s1.begin(), s1.end());
		result.insert(s2.begin(), s2.end());

		return result;
	}

	// intersection of two sets
	template<typename T> std::set<T> substractSet (std::set<T> s1, std::set<T> s2) {
		std::set<T> result;
		result.insert(s1.begin(), s1.end());
		for (auto i = s2.begin(); i != s2.end(); ++i) {
			auto iter = result.find(*i);
			if (iter != result.end()) {
				result.erase(iter);
			}
		}

		return result;
	}

	// convert an expression to index ID
	Index domainIndex (Domain &D, void* ptr);

	// dataflow direction
	enum Direction {
		FORWARD,
		BACKWARD
	};

	// meet operations
	enum MeetOp {
		INTERSECT,
		UNION
	};

	// result for transfer function
	struct TransferOutput {
		VSet transfer;
		std::map<BasicBlock*, VSet> neighbor;
	};

	// in and out result for blocks
	struct BlockResult {
		VSet in, out;
		TransferOutput transferOutput;
	};

	// the final result
	struct DataFlowResult {
		std::map<BasicBlock*, BlockResult> result;
	};

	// dataflow framework
	class Dataflow {
		public:
		Dataflow (Direction direction, MeetOp meetop, Domain domain)
		: direction(direction), meetop(meetop), domain(domain)
		{

		};

		VSet applyMeet (VList input);
		DataFlowResult run (Function &F, VSet boudary, VSet interior);
		Index domainIndex (void* ptr);
		virtual TransferOutput transferFn (VSet input, BasicBlock *currentBlock) = 0;
		const Index INDEX_NOT_FOUND = -1;
		Domain domain;

		private:
		Direction direction;
		MeetOp meetop;
	};

	// convert LLVM value to corresponding std::string
	std::string getValueName (Value* v);
};
