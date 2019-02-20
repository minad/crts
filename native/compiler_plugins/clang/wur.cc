/*
 * By Noam Lewis
 * https://github.com/sinelaw/elfs-clang-plugins
 * MIT License
 */

#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/MacroInfo.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/AST/ASTContext.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <set>
#include <iostream>
#include <string>
#include <cinttypes>

using namespace clang;
using namespace clang::ast_matchers;

class WarnUnusedResultMatchCallback : public MatchFinder::MatchCallback
{
private:
    bool m_static_only;

    static void emit_warn(DiagnosticsEngine &diagEngine, SourceLocation loc) {
        unsigned diagID = diagEngine.getCustomDiagID(diagEngine.getWarningsAsErrors()
                                                     ? DiagnosticsEngine::Error
                                                     : DiagnosticsEngine::Warning,
                                                     "missing attribute warn_unused_result");
        DiagnosticBuilder DB = diagEngine.Report(loc, diagID);
    }

public:
    WarnUnusedResultMatchCallback(bool static_only) : m_static_only(static_only) { }

    virtual void run(const MatchFinder::MatchResult &Result) {
        ASTContext *context = Result.Context;
        DiagnosticsEngine &diagEngine = context->getDiagnostics();
        const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("badFunc");
        const bool is_static = func->getStorageClass() == clang::StorageClass::SC_Static;
        if (!is_static && m_static_only) return;
        emit_warn(diagEngine, func->getLocation());
    }
};

class WarnUnusedResultAction : public PluginASTAction {

    bool m_static_only;


public:
    WarnUnusedResultAction() {
        m_static_only = false;
    }

protected:

    DeclarationMatcher missingWarnUnusedResultMatcher =
        functionDecl(isExpansionInMainFile(),
                     unless(isImplicit()),
                     unless(hasName("main")),
                     unless(returns(asString("void"))),
                     unless(hasAttr(clang::attr::Kind::WarnUnusedResult))).bind("badFunc");

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   llvm::StringRef) override {
        MatchFinder *const find = new MatchFinder();
        find->addMatcher(missingWarnUnusedResultMatcher, new WarnUnusedResultMatchCallback(m_static_only));
        return find->newASTConsumer();
    }

    const std::string param_static_only = "--static-only";

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) override {
        if (args.size() == 0)
            return true;

        if ((args.size() == 1) && (args.front() == param_static_only)) {
            m_static_only = true;
            return true;
        }
        DiagnosticsEngine &D = CI.getDiagnostics();
        unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                            "warn_unused_result plugin: invalid arguments");
        D.Report(DiagID);
        llvm::errs() << "warn_unused_result plugin: Accepted arguments: [" << param_static_only << "]\n";
        return false;
    }
};

static FrontendPluginRegistry::Add<WarnUnusedResultAction> registerWur("wur", "find missing warn_unused_result");
