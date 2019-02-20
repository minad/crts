/*
 * Clang typechecker plugin which checks our custom format strings.
 * Inspired by rspamd printf_check plugin by Vsevolod Stakhov.
 * https://github.com/rspamd/rspamd/blob/master/clang-plugin/printf_check.cc
 */
#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Expr.h>
#include <clang/AST/Attr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendPluginRegistry.h>
#include <clang/Sema/Sema.h>

using llvm::APInt;
using namespace clang;

class FmtCheckVisitor : public RecursiveASTVisitor<FmtCheckVisitor> {
    ASTContext& ctx;
    CompilerInstance& ci;

    void warn(Expr*, const std::string&);
    void checkArgs(Expr*, const StringRef&, Expr**, unsigned);

public:
    FmtCheckVisitor(ASTContext& ctx, CompilerInstance &ci)
        : ctx(ctx), ci(ci) {
    }

    bool VisitCallExpr(CallExpr*);
};

class FmtASTConsumer : public ASTConsumer {
    CompilerInstance &ci;
public:

    explicit FmtASTConsumer(CompilerInstance& ci)
        : ci(ci) {
    }

    void HandleTranslationUnit(ASTContext& ctx) override {
        FmtCheckVisitor(ctx, ci).TraverseDecl(ctx.getTranslationUnitDecl());
    }
};

class FmtASTAction : public PluginASTAction {
protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& ci, StringRef) override {
        return llvm::make_unique<FmtASTConsumer>(ci);
    }

    bool ParseArgs(const CompilerInstance& ci, const std::vector <std::string> &args) override {
        return true;
    }
};

void FmtCheckVisitor::warn(Expr* current, const std::string &err) {
    auto loc = current->getExprLoc();
    auto &diag = ci.getDiagnostics();
    auto id = diag.getCustomDiagID(DiagnosticsEngine::Warning, "format string: %0");
    diag.Report(loc, id) << err;
}

void FmtCheckVisitor::checkArgs(Expr* current, const StringRef& fmt, Expr** args, unsigned numArgs) {
    unsigned arg = 0;
    for (auto i = fmt.begin(); i != fmt.end(); ++i) {
        if (*i != '%')
            continue;

        ++i;
        if (i == fmt.end()) {
            warn(current, "Unexpected end in format string");
            return;
        }

        //bool leftAlign = false;
        if (i != fmt.end() && *i == '-') {
            //leftAlign = true;
            ++i;
        }

        bool zeroPad = false;
        if (i != fmt.end() && *i == '0') {
            zeroPad = true;
            ++i;
        }

        bool width = false;
        if (i != fmt.end() && *i == '*') {
            width = true;
            if (arg >= numArgs) {
                warn(current, "Missing width argument");
                return;
            }
            auto type = args[arg]->getType().split().Ty;
            auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
            if (!builtinType || (builtinType->getKind() != BuiltinType::Kind::UInt && builtinType->getKind() != BuiltinType::Kind::Int)) {
                warn(current, "Width argument must be int");
                return;
            }
            ++arg;
            ++i;
        } else {
            while (i != fmt.end() && *i >= '0' && *i <= '9') {
                width = true;
                ++i;
            }
        }

        bool prec = false;
        if (i != fmt.end() && *i == '.') {
            prec = true;
            ++i;

            if (i != fmt.end() && *i == '*') {
                if (arg >= numArgs) {
                    warn(current, "Missing precision argument");
                    return;
                }
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType || (builtinType->getKind() != BuiltinType::Kind::UInt && builtinType->getKind() != BuiltinType::Kind::Int)) {
                    warn(current, "Precision argument must be int");
                    return;
                }
                ++arg;
                ++i;
            } else {
                while (i != fmt.end() && *i >= '0' && *i <= '9')
                    ++i;
            }
        }

        char quote = 0;
        if (i != fmt.end() && (*i == 'q' || *i == 'h')) {
            quote = *i;
            ++i;
            if (width) {
                warn(current, "Width must not be set for quoted fields");
                return;
            }
        }

        char len = 0;
        if (i != fmt.end() && (*i == 'l' || *i == 'j' || *i == 'z')) {
            len = *i;
            ++i;
        }

        if (i == fmt.end()) {
            warn(current, "Unexpected end in format string");
            return;
        }

        if (*i != 'd' && *i != 'u' && *i != 'x') {
            if (zeroPad) {
                warn(current, std::string("Zero pad is not allowed for %") + *i);
                return;
            }
            if (len) {
                warn(current, std::string("Length specifier is not allowed for %") + *i);
                return;
            }
        }

        if (prec && *i != 'f' && *i != 'e' && *i != 't') {
            warn(current, std::string("Precision is not allowed for %") + *i);
            return;
        }

        if (quote == 'h' && *i != 's' && *i != 'S' && *i != 'b') {
            warn(current, std::string("Hexadecimal quotation is not allowed for %") + *i);
            return;
        }

        if (*i != 'w' && *i != '%' && *i != 'm' && arg >= numArgs) {
            warn(current, "Missing argument");
            return;
        }

        switch (*i) {
        case 'm':
        case 'w':
        case '%':
            break;
        case 'c':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType || (builtinType->getKind() != BuiltinType::Kind::Int && builtinType->getKind() != BuiltinType::Kind::SChar)) {
                    warn(current, "Expected char, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
            break;
        case 's':
            {
                auto type = args[arg]->getType().split().Ty;
                if (!type->isPointerType() || !type->getPointeeType().split().Ty->isCharType()) {
                    warn(current, "Expected string, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'b':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType || builtinType->getKind() != BuiltinType::Kind::UInt) {
                    warn(current, "Expected unsigned int, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;

                if (arg >= numArgs) {
                    warn(current, "Missing argument");
                    return;
                }

                type = args[arg]->getType().split().Ty;
                if (!type->isPointerType()) {
                    warn(current, "Expected pointer to buffer, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'C':
            {
                if (args[arg]->getType().getAsString() != "Chili") {
                    warn(current, "Expected Chili, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'L':
            {
                auto type = args[arg]->getType().split().Ty;
                if (!type->isPointerType() || type->getPointeeType().getAsString() != "ChiLocInfo") {
                    warn(current, "Expected ChiLocInfo*, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'S':
            {
                if (args[arg]->getType().getAsString() != "ChiStringRef") {
                    warn(current, "Expected ChiStringRef, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 't':
            {
                if (args[arg]->getType().getAsString() != "ChiNanos") {
                    warn(current, "Expected ChiNanos, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'p':
            {
                auto type = args[arg]->getType().split().Ty;
                if (!type->isPointerType()) {
                    warn(current, "Expected pointer, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'd':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType) {
                    warn(current, "Expected integer type, got " + args[arg]->getType().getAsString());
                    return;
                }
                if (len == 'l') {
                    if (builtinType->getKind() != BuiltinType::Kind::Long) {
                        warn(current, "Expected long, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else if (len == 'j') {
                    if (builtinType->getKind() != BuiltinType::Kind::LongLong &&
                        !(sizeof (long) == sizeof (long long) && builtinType->getKind() == BuiltinType::Kind::Long)) {
                        warn(current, "Expected long long, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else if (len == 'z') {
                    if (!(sizeof (int) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::Int) &&
                        !(sizeof (long) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::Long)) {
                        warn(current, "Expected size_t, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else {
                    if (builtinType->getKind() != BuiltinType::Kind::Int) {
                        warn(current, "Expected int, got " + args[arg]->getType().getAsString());
                        return;
                    }
                }
                ++arg;
                break;
            }
        case 'x':
        case 'u':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType) {
                    warn(current, "Expected integer type, got " + args[arg]->getType().getAsString());
                    return;
                }
                if (len == 'l') {
                    if (builtinType->getKind() != BuiltinType::Kind::ULong) {
                        warn(current, "Expected unsigned long, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else if (len == 'j') {
                    if (builtinType->getKind() != BuiltinType::Kind::ULongLong &&
                        !(sizeof (long) == sizeof (long long) && builtinType->getKind() == BuiltinType::Kind::ULong)) {
                        warn(current, "Expected unsigned long long, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else if (len == 'z') {
                    if (!(sizeof (int) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::UInt) &&
                        !(sizeof (long) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::ULong)) {
                        warn(current, "Expected size_t, got " + args[arg]->getType().getAsString());
                        return;
                    }
                } else {
                    if (builtinType->getKind() != BuiltinType::Kind::UInt) {
                        warn(current, "Expected unsigned int, got " + args[arg]->getType().getAsString());
                        return;
                    }
                }
                ++arg;
                break;
            }
        case 'f':
        case 'e':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType || builtinType->getKind() != BuiltinType::Kind::Double) {
                    warn(current, "Expected double, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        case 'Z':
            {
                auto type = args[arg]->getType().split().Ty;
                auto builtinType = dyn_cast<BuiltinType>(type->getUnqualifiedDesugaredType());
                if (!builtinType ||
                    (!(sizeof (int) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::UInt) &&
                     !(sizeof (long) == sizeof (size_t) && builtinType->getKind() == BuiltinType::Kind::ULong))) {
                    warn(current, "Expected size_t, got " + args[arg]->getType().getAsString());
                    return;
                }
                ++arg;
                break;
            }
        default:
            warn(current, std::string("Unexpected format character %") + *i);
            return;
        }
    }

    if (arg < numArgs) {
        warn(current, "Too many arguments");
        return;
    }
}

bool FmtCheckVisitor::VisitCallExpr(CallExpr* current) {
    auto callee = current->getCalleeDecl();
    if (!callee)
        return true;

    auto attr = callee->getAttr<AnnotateAttr>();
    if (!attr)
        return true;

    StringRef s = attr->getAnnotation();
    if (!s.consume_front("__fmt__ "))
        return true;

    auto idx = s.split(' ');
    APInt fmtIdx, argIdx;
    idx.first.getAsInteger(10, fmtIdx);
    idx.second.getAsInteger(10, argIdx);

    const auto args = current->getArgs();
    auto fmt = args[fmtIdx.getLimitedValue() - 1];

    if (!fmt->isEvaluatable(ctx)) {
        warn(current, "Format string cannot be evaluated");
        return true;
    }

    Expr::EvalResult r;
    if (!fmt->EvaluateAsRValue(r, ctx)) {
        warn(current, "Format string cannot be evaluated");
        return true;
    }

    auto fmtStr = dyn_cast<StringLiteral>(r.Val.getLValueBase().get<const Expr*>());
    if (!fmtStr) {
        warn(current, "Format string is not a string literal");
        return true;
    }

    auto n = argIdx.getLimitedValue() - 1;
    checkArgs(current, fmtStr->getString(), args + n, current->getNumArgs() - n);

    return true;
}

static FrontendPluginRegistry::Add<FmtASTAction> registerFmt("fmt", "format string checker");
