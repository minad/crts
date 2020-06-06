#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Pass.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#pragma GCC diagnostic push

namespace {

using namespace llvm;

struct TailPrefixPass : public PassInfoMixin<TailPrefixPass> {
    PreservedAnalyses run(Module&, ModuleAnalysisManager&);
};

static void filterUsedList(Module &M, StringRef name) {
    auto GV = M.getGlobalVariable("llvm.used");
    SmallVector<Constant*, 16> Init;
    if (GV) {
        auto *CA = cast<ConstantArray>(GV->getInitializer());
        for (auto &Op : CA->operands()) {
            Constant *C = cast_or_null<Constant>(Op);
            if (C->getOperand(0)->getName() != name)
                Init.push_back(C);
        }
        GV->eraseFromParent();
    }

    if (Init.empty())
        return;

    auto Int8PtrTy = llvm::Type::getInt8PtrTy(M.getContext());
    auto ATy = ArrayType::get(Int8PtrTy, Init.size());
    GV = new llvm::GlobalVariable(M, ATy, false, GlobalValue::AppendingLinkage,
                                  ConstantArray::get(ATy, Init), "llvm.used");
    GV->setSection("llvm.metadata");

}

PreservedAnalyses TailPrefixPass::run(Module& module, ModuleAnalysisManager& mam) {
    CallingConv::ID callconv = module.getTargetTriple().find("x86_64") != std::string::npos
        ? CallingConv::GHC : CallingConv::Fast;

    auto globalAnns = module.getNamedGlobal("llvm.global.annotations");
    if (globalAnns) {
        // Taken from blog post by Brandon Holt
        auto ann = cast<ConstantArray>(globalAnns->getInitializer());
        for (size_t i = 0; i < ann->getNumOperands(); ++i) {
            auto expr = cast<ConstantStruct>(ann->getOperand(i));
            auto fn = dyn_cast<Function>(expr->getOperand(0)->getOperand(0));
            if (fn) {
                auto attr = cast<ConstantDataArray>(cast<GlobalVariable>(expr->getOperand(1)->getOperand(0))->getOperand(0))->getAsCString();
                if (attr == "__tailcallable__") {
                    fn->addFnAttr(Attribute::NoReturn);
                    fn->setCallingConv(callconv);
                }

                auto pos = attr.find("__prefix_data__ ");
                if (!pos) {
                    auto prefixDataName = attr.substr(strlen("__prefix_data__ "));
                    auto prefixData = module.getNamedGlobal(prefixDataName);
                    if (!prefixData) {
                        errs() << "Prefix data " << prefixDataName << " not found\n";
                        report_fatal_error("FATAL ERROR");
                    }

                    auto prefixDataConst = prefixData->getInitializer();
                    if (!prefixDataConst) {
                        errs() << "Prefix data " << prefixDataName << " is not initialized\n";
                        report_fatal_error("FATAL ERROR");
                    }

                    fn->setPrefixData(prefixDataConst);
                    filterUsedList(module, prefixDataName);
                    prefixData->eraseFromParent();
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
                    if (calledFn && calledFn->getCallingConv() == callconv)
                        callInsn->setCallingConv(callconv);
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
                            tailCallInsn->setCallingConv(callconv);
                            tailCallInsn->setDoesNotReturn();
                            ReplaceInstWithInst(callInsn, ReturnInst::Create(module.getContext(), tailCallInsn));
                            unreachableInsn->eraseFromParent();
                            break;
                        }
                    }
                }
            }
        }
    }

    verifyModule(module, &errs());
    //outs() << module;

    return PreservedAnalyses::none();
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return { LLVM_PLUGIN_API_VERSION,
             "tailcall_prefix_data",
             LLVM_VERSION_STRING,
             [](PassBuilder &pb) {
                 pb.registerPipelineStartEPCallback([](ModulePassManager &mpm) { mpm.addPass(TailPrefixPass()); });
             }
    };
}

} // anonymous namespace
