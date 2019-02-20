#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Pass.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#pragma GCC diagnostic push

using namespace llvm;

namespace {

// TODO: We should use CallingConv::GHC on __x86_64__
// However it leads to crashes right now.
// Probably the code transformation performed by the
// plugin does something which leads to invalid code.
const CallingConv::ID CALLCONV = CallingConv::Fast;

struct TailCallPass : public ModulePass {
    static char ID;
    TailCallPass() : ModulePass(ID) {}
    bool runOnModule(Module&) override;
};

char TailCallPass::ID = 0;

bool TailCallPass::runOnModule(Module& module) {
    auto globalAnns = module.getNamedGlobal("llvm.global.annotations");
    if (globalAnns) {
        // Taken from blog post by Brandon Holt
        auto ann = cast<ConstantArray>(globalAnns->getOperand(0));
        for (size_t i = 0; i < ann->getNumOperands(); ++i) {
            auto expr = cast<ConstantStruct>(ann->getOperand(i));
            auto fn = dyn_cast<Function>(expr->getOperand(0)->getOperand(0));
            if (fn) {
                auto attr = cast<ConstantDataArray>(cast<GlobalVariable>(expr->getOperand(1)->getOperand(0))->getOperand(0))->getAsCString();
                if (attr == "__tailcallable__") {
                    fn->addFnAttr(attr);
                    fn->addFnAttr(Attribute::NoReturn);
                    fn->setCallingConv(CALLCONV);
                    //outs() << "tailcallable " << fn->getName() << "\n";
                }
            }
        }
    }

    for (Function& fn : module) {
        for (BasicBlock& bb : fn) {
            for (Instruction& insn : bb) {
                auto callInsn = dyn_cast<CallInst>(&insn);
                if (callInsn) {
                    auto calledFn = callInsn->getCalledFunction();
                    if (calledFn && calledFn->getCallingConv() == CALLCONV)
                        callInsn->setCallingConv(CALLCONV);
                    if (calledFn && calledFn->getName() == "llvm.annotation.i32") {
                        auto attr = cast<ConstantDataArray>(cast<GlobalVariable>(cast<ConstantExpr>(callInsn->getArgOperand(1))->getOperand(0))->getOperand(0))->getAsCString();
                        if (attr == "__tailcall__") {
                            auto tailCallInsn = dyn_cast<CallInst>(callInsn->getArgOperand(0));
                            auto unreachableInsn = dyn_cast<UnreachableInst>(callInsn->getNextNode());
                            if (!tailCallInsn || !unreachableInsn) {
                                errs() << "Invalid __tailcall__ annotation " << tailCallInsn << "\n";
                                report_fatal_error("FATAL ERROR");
                            }
                            tailCallInsn->setTailCallKind(CallInst::TCK_MustTail);
                            tailCallInsn->setCallingConv(CALLCONV);
                            tailCallInsn->setDoesNotReturn();
                            ReplaceInstWithInst(callInsn, ReturnInst::Create(module.getContext(), tailCallInsn));
                            unreachableInsn->eraseFromParent();
                            //outs() << *tailCallInsn << "\n";
                            break;
                        }
                    }
                }
            }
        }
    }

    verifyModule(module, &errs());
    //outs() << module;

    return true;
}

void registerTailCall(const PassManagerBuilder&, legacy::PassManagerBase& pm) {
    pm.add(new TailCallPass());
}

RegisterStandardPasses register0(PassManagerBuilder::EP_ModuleOptimizerEarly, registerTailCall);
RegisterStandardPasses register1(PassManagerBuilder::EP_EnabledOnOptLevel0, registerTailCall);

} // anonymous namespace
