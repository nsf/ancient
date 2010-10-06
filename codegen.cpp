#include <tr1/unordered_map>
#include <string>
#include <cstdarg>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include "grammar.h"
#include "parser.h"

using std::tr1::unordered_map;
using std::string;

extern "C" {
	LLVMModuleRef codegen(struct stmts *stmts);
}

struct Scope {
	unordered_map<string, llvm::Value*> values;

	llvm::Value *get(const char *name)
	{
		auto it = values.find(name);
		if (it == values.end())
			return 0;
		return it->second;
	}

	void add(const char *name, llvm::Value* value)
	{
		values[name] = value;
	}
};

//-------------------------------------------------------------------------
// Helpers and shortcurs
//-------------------------------------------------------------------------
static llvm::Value *errorv(const char *msg ...)
{
	va_list args;
	va_start(args, msg);
	vfprintf(stderr, msg, args);
	fputs("\n", stderr);
	va_end(args);
	return 0;
}

static const llvm::Type *type_double() { return llvm::Type::getDoubleTy(llvm::getGlobalContext()); }
static llvm::Value *const_double(double num) { return llvm::ConstantFP::get(type_double(), num); }
static llvm::StringRef to_ref(struct expr *ident) { return llvm::StringRef(ident->ident.beg, ident->ident.len); }
static std::string to_string(struct expr *ident) { return std::string(ident->ident.beg, ident->ident.len); }

//-------------------------------------------------------------------------
// Codegen
//-------------------------------------------------------------------------

struct CodegenContext {
	llvm::Module *module;
	llvm::IRBuilder<> *builder;
	Scope scope;
	llvm::Function *F;
};

static llvm::Value *codegen_expr(CodegenContext *ctx, struct expr *e)
{
	switch (e->type) {
	case EXPR_NUM:
	{
		return const_double(e->num);
	}
	case EXPR_IDENT:
	{
		// if it's in a scope, then it is a variable
		auto v = ctx->scope.get(to_string(e).c_str());
		if (v)
			return ctx->builder->CreateLoad(v, "loadtmp");

		// try function otherwise
		auto F = ctx->module->getFunction(to_ref(e));
		if (!F)
			return errorv("Cannot resolve entity: %s", to_string(e).c_str());

		return ctx->builder->CreateCall(F, "calltmp");
	}
	case EXPR_BIN:
	{
		auto L = codegen_expr(ctx, e->bin.lhs);
		auto R = codegen_expr(ctx, e->bin.rhs);
		if (L == 0 || R == 0)
			return errorv("Failed to codegen lhs or rhs for binaryop");

		switch (e->bin.tok) {
		case PLUS: return ctx->builder->CreateFAdd(L, R, "addtmp");
		case MINUS: return ctx->builder->CreateFSub(L, R, "subtmp");
		case TIMES: return ctx->builder->CreateFMul(L, R, "multmp");
		case DIVIDE: return ctx->builder->CreateFDiv(L, R, "divtmp");
		case LESS:
			L = ctx->builder->CreateFCmpULT(L, R, "cmptmp");
			return ctx->builder->CreateUIToFP(L, type_double(), "casttmp");
		}
	}
	case EXPR_CALL:
	{
		auto F = ctx->module->getFunction(to_ref(e->call.ident));
		if (!F)
			return errorv("Cannot resolve function: %s", to_string(e).c_str());

		int numargs = F->arg_size();
		if (e->call.args->v_n != numargs)
			return errorv("Invalid number of arguments for a function call: %s", to_string(e).c_str());

		std::vector<llvm::Value*> args;
		args.resize(numargs);
		for (int i = 0; i < numargs; i++)
			args[i] = codegen_expr(ctx, e->call.args->v[i]);

		return ctx->builder->CreateCall(F, args.begin(), args.end(), "calltmp");
	}
	default:
		return errorv("Unknown expression type");
	}
}

static llvm::Value *codegen_entry_alloca(llvm::Function *F, llvm::StringRef name)
{
	llvm::IRBuilder<> builder(&F->getEntryBlock(), F->getEntryBlock().begin());
	return builder.CreateAlloca(type_double(), 0, name);
}

static int codegen_statements(CodegenContext *ctx, struct stmts *ss);

static void codegen_func(CodegenContext *ctx, struct stmt *s)
{
	int numargs = s->func.args ? s->func.args->v_n : 0;

	auto name = to_string(s->func.ident);
	if (name == "main")
		name = "_anc_main";

	std::vector<const llvm::Type*> types(numargs, type_double());
	auto FT = llvm::FunctionType::get(type_double(), types, false);
	auto F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, name, ctx->module);
	if (s->func.block == 0)
		return;

	auto entry = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", F);
	llvm::IRBuilder<> builder(llvm::getGlobalContext());
	builder.SetInsertPoint(entry);

	int i = 0;
	for (auto it = F->arg_begin(); it != F->arg_end(); it++, i++) {
		auto name = to_string(s->func.args->v[i]);
		it->setName(name);
		if (ctx->scope.get(name.c_str())) {
			errorv("Redeclaration of a variable: %s\n", name.c_str());
		} else {
			auto store = codegen_entry_alloca(F, name);
			builder.CreateStore(it, store);
			ctx->scope.add(name.c_str(), store);
		}
	}

	auto savebuilder = ctx->builder;
	ctx->builder = &builder;
	ctx->F = F;

	int terminated = codegen_statements(ctx, s->func.block->block);
	if (!terminated)
		ctx->builder->CreateRet(const_double(0));

	ctx->builder = savebuilder;
	ctx->F = 0;
}

static void codegen_var(CodegenContext *ctx, struct stmt *s)
{
	auto ref = to_ref(s->var.ident);
	auto str = to_string(s->var.ident);
	if (ctx->scope.get(str.c_str())) {
		errorv("Redeclaration of a variable: %s\n", str.c_str());
	} else {
		auto store = ctx->builder->CreateAlloca(type_double(), 0, ref);
		if (s->var.init) {
			auto init = codegen_expr(ctx, s->var.init);
			ctx->builder->CreateStore(init, store);
		} else
			ctx->builder->CreateStore(const_double(0), store);

		ctx->scope.add(str.c_str(), store);
	}
}

static void codegen_assign(CodegenContext *ctx, struct stmt *s)
{
	auto ref = to_ref(s->assign.ident);
	auto str = to_string(s->assign.ident);
	auto store = ctx->scope.get(str.c_str());
	if (!store) {
		errorv("Cannot resolve variable: %s\n", str.c_str());
	} else {
		auto rhs = codegen_expr(ctx, s->assign.rhs);
		if (!rhs) {
			errorv("Can't evaluate rhs expr in an assign stmt");
			return;
		}
		ctx->builder->CreateStore(rhs, store);
	}
}

static void codegen_ifelse(CodegenContext *ctx, struct stmt *s)
{
	auto cond = codegen_expr(ctx, s->ifelse.cond);
	if (!cond) {
		errorv("Can't evaluate condition inside if statement");
		return;
	}

	auto iftrue = llvm::BasicBlock::Create(llvm::getGlobalContext(), "iftrue", ctx->F);
	llvm::BasicBlock *iffalse = 0;
	if (s->ifelse.elseblock)
		iffalse = llvm::BasicBlock::Create(llvm::getGlobalContext(), "iffalse", ctx->F);
	auto end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "ifend", ctx->F);

	auto ifcond = ctx->builder->CreateFCmpONE(cond, const_double(0), "ifcond");
	ctx->builder->CreateCondBr(ifcond, iftrue, iffalse);

	// true
	ctx->builder->SetInsertPoint(iftrue);
	int terminated = codegen_statements(ctx, s->ifelse.block->block);
	if (!terminated)
		ctx->builder->CreateBr(end);

	// false
	if (s->ifelse.elseblock) {
		ctx->builder->SetInsertPoint(iffalse);
		int terminated = codegen_statements(ctx, s->ifelse.elseblock->block);
		if (!terminated)
			ctx->builder->CreateBr(end);
	}

	ctx->builder->SetInsertPoint(end);
}

static void codegen_return(CodegenContext *ctx, struct stmt *s)
{
	if (s->ret) {
		auto v = codegen_expr(ctx, s->ret);
		ctx->builder->CreateRet(v);
	} else
		ctx->builder->CreateRet(const_double(0));
}

static void codegen_forloop(CodegenContext *ctx, struct stmt *s)
{
	auto loopdecide = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loopdecide", ctx->F);
	auto loop = llvm::BasicBlock::Create(llvm::getGlobalContext(), "loop", ctx->F);
	auto end = llvm::BasicBlock::Create(llvm::getGlobalContext(), "endloop", ctx->F);

	ctx->builder->CreateBr(loopdecide);

	// loopdecide
	ctx->builder->SetInsertPoint(loopdecide);
	auto cond = codegen_expr(ctx, s->forloop.cond);
	if (!cond) {
		errorv("Cannot evaluate condition inside for statement");
		return;
	}

	auto loopcond = ctx->builder->CreateFCmpONE(cond, const_double(0), "loopcond");
	ctx->builder->CreateCondBr(loopcond, loop, end);

	// loop
	ctx->builder->SetInsertPoint(loop);
	int terminated = codegen_statements(ctx, s->forloop.block->block);
	if (!terminated)
		ctx->builder->CreateBr(loopdecide);

	// end
	ctx->builder->SetInsertPoint(end);
}

static int codegen_statements(CodegenContext *ctx, struct stmts *ss)
{
	for (int i = 0; i < ss->v_n; i++) {
		struct stmt *s = ss->v[i];
		switch (s->type) {
		case STMT_EXPR:
			codegen_expr(ctx, s->expr);
			break;
		case STMT_FUNC:
			ctx->scope.values.clear();
			codegen_func(ctx, s);
			break;
		case STMT_RETURN:
			codegen_return(ctx, s);
			return 1; // terminated
			break;
		case STMT_VAR:
			codegen_var(ctx, s);
			break;
		case STMT_ASSIGN:
			codegen_assign(ctx, s);
			break;
		case STMT_IFELSE:
			codegen_ifelse(ctx, s);
			break;
		case STMT_FOR:
			codegen_forloop(ctx, s);
			break;
		}
	}
	return 0;
}

extern "C" LLVMModuleRef codegen(struct stmts *stmts)
{
	CodegenContext ctx;
	llvm::IRBuilder<> builder(llvm::getGlobalContext());
	ctx.module = new llvm::Module("main", llvm::getGlobalContext());
	ctx.builder = &builder;
	ctx.F = 0;

	codegen_statements(&ctx, stmts);
	return wrap(ctx.module);
}
