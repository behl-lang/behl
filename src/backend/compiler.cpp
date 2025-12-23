#include "backend/compiler.hpp"

#include "ast/ast.hpp"
#include "behl.hpp"
#include "common/format.hpp"
#include "common/hash_map.hpp"
#include "common/vector.hpp"
#include "config_internal.hpp"
#include "gc/gc.hpp"
#include "gc/gc_object.hpp"
#include "gc/gco_proto.hpp"
#include "gc/gco_string.hpp"
#include "state.hpp"
#include "vm/bytecode.hpp"

#include <behl/exceptions.hpp>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace behl
{
    constexpr uint32_t kInvalidUpvalue = UINT32_MAX;
    constexpr int32_t kInvalidLocal = -1;

    struct Local
    {
        std::string_view name;
        int32_t start_pc;
        Reg reg;
        bool is_const;
    };

    struct UpvalueInfo
    {
        uint8_t index;
        bool is_local;
        bool is_const;
    };

    struct LoopContext
    {
        AutoVector<size_t> break_list;    // Positions of break JMP instructions to patch
        AutoVector<size_t> continue_list; // Positions of continue JMP instructions to patch

        LoopContext(State* S)
            : break_list(S)
            , continue_list(S)
        {
        }
    };

    struct DeferInfo
    {
        const AstNode* defer_body; // The body of the defer statement
        int32_t scope_level;       // Track which scope this defer belongs to

        DeferInfo(const AstNode* body, int32_t level)
            : defer_body(body)
            , scope_level(level)
        {
        }
    };

    struct CompilerState
    {
        State* S = nullptr;
        GCProto* current_proto = nullptr;
        CompilerState* parent = nullptr;
        AutoVector<Local> locals;
        AutoVector<AutoVector<Local>> scopes;
        AutoVector<UpvalueInfo> upvalues;
        AutoVector<LoopContext> loop_stack; // Stack of loop contexts for break/continue
        AutoVector<DeferInfo> defer_stack;  // Stack of defer statements (LIFO order)
        AutoHashMap<std::string_view, size_t, StringHash, StringEq> upvalue_indices;
        int32_t lastline = 1;
        int32_t lastcolumn = 1;
        uint8_t freereg = 0;
        uint8_t min_freereg = 0;
        bool is_module = false;

        CompilerState(State* state, CompilerState* parent_state, GCProto* proto)
            : S(state)
            , current_proto(proto)
            , parent(parent_state)
            , locals(state)
            , scopes(state)
            , upvalues(state)
            , loop_stack(state)
            , defer_stack(state)
            , upvalue_indices(state)
        {
        }
    };

    template<typename Container>
    static ConstIndex add_constant_impl(CompilerState& C, Container& container, Value v)
    {
        for (ConstIndex i = 0; i < container.size(); ++i)
        {
            if (container[i] == v)
            {
                return i;
            }
        }

        container.push_back(C.S, v);

        return static_cast<ConstIndex>(container.size() - 1);
    }

    ConstIndex add_integer_constant(CompilerState& C, Integer val)
    {
        return add_constant_impl(C, C.current_proto->int_constants, Value(val));
    }

    ConstIndex add_fp_constant(CompilerState& C, FP val)
    {
        return add_constant_impl(C, C.current_proto->fp_constants, Value(val));
    }

    static ConstIndex add_string_constant(CompilerState& C, std::string_view str)
    {
        for (ConstIndex i = 0; i < C.current_proto->str_constants.size(); ++i)
        {
            auto& const_val = C.current_proto->str_constants[i];
            assert(const_val.is_string());

            auto* const_str = const_val.get_string();
            if (const_str->view() == str)
            {
                return i;
            }
        }

        auto* string = gc_new_string(C.S, str);
        C.current_proto->str_constants.push_back(C.S, Value(string));

        return static_cast<ConstIndex>(C.current_proto->str_constants.size() - 1);
    }

    static SourceLocation get_location(const CompilerState& C)
    {
        std::string_view filename = C.current_proto->source_name ? C.current_proto->source_name->view() : "";
        return SourceLocation(filename, C.lastline, C.lastcolumn);
    }

    static Reg alloc_reg(CompilerState& C)
    {
        if (C.freereg >= kMaxRegisters)
        {
            throw RuntimeError("Register overflow", get_location(C));
        }
        Reg reg = C.freereg++;

        if (C.freereg > C.current_proto->max_stack_size)
        {
            C.current_proto->max_stack_size = C.freereg;
        }
        return reg;
    }

    static void free_reg(CompilerState& C, Reg reg)
    {
        if (reg == C.freereg - 1)
        {
            if (C.freereg > C.min_freereg)
            {
                --C.freereg;
            }
        }
    }

    static void emit(CompilerState& C, Instruction instr, int32_t line = -1, int32_t column = -1)
    {
        C.current_proto->code.push_back(C.S, std::move(instr));
        C.current_proto->line_info.push_back(C.S, line >= 0 ? line : C.lastline);
        C.current_proto->column_info.push_back(C.S, column >= 0 ? column : C.lastcolumn);
    }

    static void enter_scope(CompilerState& C)
    {
        C.scopes.emplace_back(AutoVector<Local>(C.S));
    }

    static void leave_scope(CompilerState& C)
    {
        auto& scope_locals = C.scopes.back();

        uint8_t scope_min_reg = kMaxRegisters;
        for (const auto& local : scope_locals)
        {
            if (local.reg < scope_min_reg)
            {
                scope_min_reg = local.reg;
            }
        }

        for (auto it = scope_locals.rbegin(); it != scope_locals.rend(); ++it)
        {
            if (C.freereg > it->reg)
            {
                C.freereg = it->reg;
            }
        }

        if (scope_min_reg < kMaxRegisters && C.min_freereg > scope_min_reg)
        {
            C.min_freereg = scope_min_reg;
        }

        C.scopes.pop_back();

        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }
    }

    struct VisitorAdapter;
    static void emit_defers_for_scope(CompilerState& C, VisitorAdapter& visitor, int32_t target_scope_level);

    static int32_t current_scope_level(const CompilerState& C)
    {
        return static_cast<int>(C.scopes.size());
    }

    static int32_t resolve_local(CompilerState& C, std::string_view name)
    {
        // Search from innermost to outermost scope
        for (auto scope_it = C.scopes.rbegin(); scope_it != C.scopes.rend(); ++scope_it)
        {
            // Search locals in reverse order within the scope
            for (auto local_it = scope_it->rbegin(); local_it != scope_it->rend(); ++local_it)
            {
                if (local_it->name == name)
                {
                    return local_it->reg;
                }
            }
        }
        return kInvalidLocal;
    }

    static bool is_local_const(CompilerState& C, std::string_view name)
    {
        // Search from innermost to outermost scope
        for (auto scope_it = C.scopes.rbegin(); scope_it != C.scopes.rend(); ++scope_it)
        {
            // Search locals in reverse order within the scope
            for (auto local_it = scope_it->rbegin(); local_it != scope_it->rend(); ++local_it)
            {
                if (local_it->name == name)
                {
                    return local_it->is_const;
                }
            }
        }
        return false;
    }

    static bool is_upvalue_const(CompilerState& C, std::string_view name)
    {
        const auto& upvalues = C.upvalues;

        auto it = C.upvalue_indices.find(name);
        if (it != C.upvalue_indices.end())
        {
            size_t idx = it->second;
            if (idx < upvalues.size())
            {
                return upvalues[idx].is_const;
            }
        }
        return false;
    }

    static uint32_t resolve_upvalue(CompilerState& C, std::string_view name)
    {
        auto it = C.upvalue_indices.find(name);
        if (it != C.upvalue_indices.end())
        {
            return static_cast<uint32_t>(it->second);
        }
        if (!C.parent)
        {
            return kInvalidUpvalue;
        }
        auto& upvalues = C.upvalues;
        int32_t loc = resolve_local(*C.parent, name);
        if (loc >= 0)
        {
            bool is_const = is_local_const(*C.parent, name);
            uint32_t idx = static_cast<uint32_t>(upvalues.size());
            upvalues.push_back(UpvalueInfo{ static_cast<uint8_t>(loc), true, is_const });
            C.upvalue_indices.insert_or_assign(name, idx);
            C.current_proto->upvalue_names.push_back(C.S, gc_new_string(C.S, name));
            C.current_proto->has_upvalues = true;
            // Parent function needs to close this upvalue when it returns
            C.parent->current_proto->has_upvalues = true;
            return idx;
        }
        uint32_t up = resolve_upvalue(*C.parent, name);
        if (up != kInvalidUpvalue)
        {
            bool is_const = (up < static_cast<uint32_t>(C.parent->upvalues.size()))
                ? C.parent->upvalues[static_cast<size_t>(up)].is_const
                : false;
            uint32_t idx = static_cast<uint32_t>(upvalues.size());
            upvalues.push_back(UpvalueInfo{ static_cast<uint8_t>(up), false, is_const });
            C.upvalue_indices.insert_or_assign(name, idx);
            C.current_proto->upvalue_names.push_back(C.S, gc_new_string(C.S, name));
            C.current_proto->has_upvalues = true;
            // Upvalue chain - parent also needs to track upvalues
            C.parent->current_proto->has_upvalues = true;
            return idx;
        }
        return kInvalidUpvalue;
    }

    struct VisitorAdapter : AstVisitor
    {
        CompilerState& C;
        std::optional<Reg> target_reg;
        bool compile_for_jump = false;
        size_t* jump_patch_location = nullptr;

        explicit VisitorAdapter(CompilerState& compiler)
            : C(compiler)
            , target_reg(std::nullopt)
            , compile_for_jump(false)
            , jump_patch_location(nullptr)
        {
        }

        void compile_call(const AstFuncCall& node, uint8_t nresults);

        bool last_instruction_is_terminal() const
        {
            if (C.current_proto->code.empty())
            {
                return false;
            }
            const auto& last = C.current_proto->code.back();
            return last.op() == OpCode::kOpReturn || last.op() == OpCode::kOpTailCall;
        }

        Reg get_target_reg()
        {
            if (target_reg.has_value())
            {
                Reg reg = target_reg.value();
                target_reg = std::nullopt;
                return reg;
            }
            return alloc_reg(C);
        }

        std::pair<Reg, bool> try_get_reg(const AstNode* expr)
        {
            if (auto* ident = expr->try_as<AstIdent>())
            {
                int32_t loc = resolve_local(C, ident->name->view());
                if (loc >= 0)
                {
                    return { static_cast<uint8_t>(loc), false };
                }
            }

            expr->accept(*this);
            Reg reg = static_cast<Reg>(C.freereg - 1);
            return { reg, true };
        }

        std::pair<Reg, bool> try_get_rk(const AstNode* expr)
        {
            if (auto* ident = expr->try_as<AstIdent>())
            {
                int32_t loc = resolve_local(C, ident->name->view());
                if (loc >= 0)
                {
                    return { static_cast<uint8_t>(loc), false };
                }
            }

            if (auto* int_node = expr->try_as<AstInt>())
            {
                const auto reg = get_target_reg();
                // Use immediate encoding for small integers (-65536 to 65535)
                if (int_node->value >= -65536 && int_node->value <= 65535)
                {
                    emit(C, make_op_loadimm(reg, static_cast<int32_t>(int_node->value)), C.lastline);
                }
                else
                {
                    const auto k = add_integer_constant(C, int_node->value);
                    emit(C, make_op_loadi(reg, k), C.lastline);
                }
                return { reg, true };
            }
            if (auto* float_node = expr->try_as<AstFP>())
            {
                const auto reg = get_target_reg();
                const auto k = add_fp_constant(C, float_node->value);
                emit(C, make_op_loadf(reg, k), C.lastline);
                return { reg, true };
            }
            if (auto* str_node = expr->try_as<AstString>())
            {
                const auto reg = get_target_reg();
                const auto k = add_string_constant(C, str_node->view());
                emit(C, make_op_loads(reg, k), C.lastline);
                return { reg, true };
            }

            expr->accept(*this);
            Reg reg = static_cast<Reg>(C.freereg - 1);
            return { reg, true };
        }

        void visit(const AstNil&) override;
        void visit(const AstBool&) override;
        void visit(const AstInt&) override;
        void visit(const AstFP&) override;
        void visit(const AstString&) override;
        void visit(const AstVararg&) override;
        void visit(const AstIdent&) override;
        void visit(const AstBinOp&) override;
        void visit(const AstUnOp&) override;
        void visit(const AstTernary&) override;
        void visit(const AstFuncCall&) override;
        void visit(const AstTableCtor&) override;
        void visit(const AstIndex&) override;
        void visit(const AstMember&) override;
        void visit(const AstFuncDef&) override;

        void visit(const AstAssign&) override;
        void visit(const AstAssignLocal&) override;
        void visit(const AstAssignGlobal&) override;
        void visit(const AstAssignUpvalue&) override;
        void visit(const AstCompoundAssign&) override;
        void visit(const AstCompoundLocal&) override;
        void visit(const AstCompoundGlobal&) override;
        void visit(const AstCompoundUpvalue&) override;
        void visit(const AstIncrement&) override;
        void visit(const AstIncLocal&) override;
        void visit(const AstIncGlobal&) override;
        void visit(const AstIncUpvalue&) override;
        void visit(const AstDecrement&) override;
        void visit(const AstDecLocal&) override;
        void visit(const AstDecGlobal&) override;
        void visit(const AstDecUpvalue&) override;
        void visit(const AstLocalDecl&) override;
        void visit(const AstIf&) override;
        void visit(const AstWhile&) override;
        void visit(const AstForNum&) override;
        void visit(const AstForIn&) override;
        void visit(const AstForC&) override;
        void visit(const AstForCNumeric&) override;
        void visit(const AstFuncDefStat&) override;
        void visit(const AstReturn&) override;
        void visit(const AstBreak&) override;
        void visit(const AstContinue&) override;
        void visit(const AstDefer&) override;
        void visit(const AstScope&) override;
        void visit(const AstExprStat&) override;

        void visit(const AstBlock&) override;
        void visit(const AstProgram&) override;
        void visit(const AstModuleDecl&) override;
        void visit(const AstExportDecl&) override;
        void visit(const AstExportList&) override;
    };

    void VisitorAdapter::visit(const AstNil&)
    {
        const auto reg = get_target_reg();
        emit(C, make_op_loadnil(reg, 0), C.lastline);
    }

    void VisitorAdapter::visit(const AstBool& node)
    {
        const auto reg = get_target_reg();
        emit(C, make_op_loadbool(reg, node.value, false), C.lastline);

        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, reg);
        }
    }

    void VisitorAdapter::visit(const AstInt& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        const auto reg = get_target_reg();
        // Use immediate encoding for small integers (-256 to 255)
        if (node.value >= -256 && node.value <= 255)
        {
            emit(C, make_op_loadimm(reg, static_cast<int32_t>(node.value)), C.lastline);
        }
        else
        {
            const auto k = add_integer_constant(C, node.value);
            emit(C, make_op_loadi(reg, k), C.lastline);
        }
    }

    void VisitorAdapter::visit(const AstFP& node)
    {
        const auto reg = get_target_reg();
        const auto k = add_fp_constant(C, node.value);
        emit(C, make_op_loadf(reg, k), C.lastline);
    }

    void VisitorAdapter::visit(const AstString& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        const auto reg = get_target_reg();

        const auto k = add_string_constant(C, node.view());
        emit(C, make_op_loads(reg, k), C.lastline);
    }

    void VisitorAdapter::visit(const AstVararg&)
    {
        const auto reg = get_target_reg();
        emit(C, make_op_vararg(reg, 0), C.lastline);
    }

    void VisitorAdapter::visit(const AstIdent& node)
    {
        const auto reg = get_target_reg();
        int32_t loc = resolve_local(C, node.name->view());
        if (loc >= 0)
        {
            if (reg != static_cast<uint8_t>(loc))
            {
                emit(C, make_op_move(reg, static_cast<uint8_t>(loc)), C.lastline);
            }
        }
        else
        {
            uint32_t up = resolve_upvalue(C, node.name->view());
            if (up != kInvalidUpvalue)
            {
                emit(C, make_op_getupval(reg, static_cast<uint8_t>(up)), C.lastline);
            }
            else
            {
                const auto k = add_string_constant(C, node.name->view());
                emit(C, make_op_getglobal(reg, k), C.lastline);
            }
        }

        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, reg);
        }
    }

    void VisitorAdapter::visit(const AstBinOp& node)
    {
        // Save the line/column of the binary operation itself before visiting children
        const int32_t binop_line = node.line;
        const int32_t binop_column = node.column;

        C.lastline = node.line;
        C.lastcolumn = node.column;

        if (node.op == TokenType::kAndOp)
        {
            Reg result_reg = get_target_reg();
            bool saved_compile_for_jump = compile_for_jump;
            size_t* saved_jump_patch = jump_patch_location;
            compile_for_jump = false;
            jump_patch_location = nullptr;

            node.left->accept(*this);
            Reg left_reg = static_cast<Reg>(C.freereg - 1);

            if (left_reg != result_reg)
            {
                emit(C, make_op_move(result_reg, left_reg), C.lastline);
            }

            emit(C, make_op_test(result_reg, true), C.lastline);
            size_t jmp_pos = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);

            free_reg(C, left_reg);

            target_reg = result_reg;
            node.right->accept(*this);

            C.current_proto->code[jmp_pos] = make_op_jmp(static_cast<int32_t>(C.current_proto->code.size() - jmp_pos - 1));

            if (saved_compile_for_jump && saved_jump_patch)
            {
                emit(C, make_op_test(result_reg, true), C.lastline);
                *saved_jump_patch = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);
                free_reg(C, result_reg);
            }

            return;
        }

        if (node.op == TokenType::kOrOp)
        {
            Reg result_reg = get_target_reg();
            bool saved_compile_for_jump = compile_for_jump;
            size_t* saved_jump_patch = jump_patch_location;
            compile_for_jump = false;
            jump_patch_location = nullptr;

            node.left->accept(*this);
            Reg left_reg = static_cast<Reg>(C.freereg - 1);

            if (left_reg != result_reg)
            {
                emit(C, make_op_move(result_reg, left_reg), C.lastline);
            }

            emit(C, make_op_test(result_reg, false), C.lastline);
            size_t jmp_pos = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);

            free_reg(C, left_reg);

            target_reg = result_reg;
            node.right->accept(*this);

            C.current_proto->code[jmp_pos] = make_op_jmp(static_cast<int32_t>(C.current_proto->code.size() - jmp_pos - 1));

            if (saved_compile_for_jump && saved_jump_patch)
            {
                emit(C, make_op_test(result_reg, true), C.lastline);
                *saved_jump_patch = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);
                free_reg(C, result_reg);
            }

            return;
        }

        if (compile_for_jump && jump_patch_location
            && (node.op == TokenType::kEq || node.op == TokenType::kNe || node.op == TokenType::kLt || node.op == TokenType::kGt
                || node.op == TokenType::kLe || node.op == TokenType::kGe))
        {
            // Check for constant right operand in equality comparisons
            if (node.right->try_as<AstInt>() && (node.op == TokenType::kEq || node.op == TokenType::kNe))
            {
                auto* rhs_int = node.right->try_as<AstInt>();
                compile_for_jump = false;
                auto [lreg, l_free] = try_get_rk(node.left);
                compile_for_jump = true;

                // Use immediate opcode if the value fits in 17-bit signed range
                if (rhs_int->value >= -65536 && rhs_int->value <= 65535)
                {
                    if (node.op == TokenType::kEq)
                    {
                        emit(C, make_op_neimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                    else if (node.op == TokenType::kNe)
                    {
                        emit(C, make_op_eqimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                }
                else
                {
                    // Fallback to regular EQ with register
                    compile_for_jump = false;
                    auto [rreg, r_free] = try_get_rk(node.right);
                    compile_for_jump = true;

                    if (node.op == TokenType::kEq)
                    {
                        emit(C, make_op_ne(lreg, rreg), C.lastline);
                    }
                    else if (node.op == TokenType::kNe)
                    {
                        emit(C, make_op_eq(lreg, rreg), C.lastline);
                    }

                    if (r_free)
                    {
                        free_reg(C, rreg);
                    }
                }

                *jump_patch_location = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);

                if (l_free)
                {
                    free_reg(C, lreg);
                }
                return;
            }

            // Check for constant right operand in comparisons
            if (node.right->try_as<AstInt>()
                && (node.op == TokenType::kLt || node.op == TokenType::kLe || node.op == TokenType::kGt
                    || node.op == TokenType::kGe))
            {
                auto* rhs_int = node.right->try_as<AstInt>();
                compile_for_jump = false;
                auto [lreg, l_free] = try_get_rk(node.left);
                compile_for_jump = true;

                // Use immediate opcodes if the value fits in 17-bit signed range
                if (rhs_int->value >= -65536 && rhs_int->value <= 65535)
                {
                    if (node.op == TokenType::kLt)
                    {
                        emit(C, make_op_geimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                    else if (node.op == TokenType::kLe)
                    {
                        emit(C, make_op_gtimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                    else if (node.op == TokenType::kGt)
                    {
                        emit(C, make_op_leimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                    else if (node.op == TokenType::kGe)
                    {
                        emit(C, make_op_ltimm(lreg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                    }
                }
                else
                {
                    const auto k = add_integer_constant(C, rhs_int->value);

                    if (node.op == TokenType::kLt)
                    {
                        emit(C, make_op_gei(lreg, k), C.lastline);
                    }
                    else if (node.op == TokenType::kLe)
                    {
                        emit(C, make_op_gti(lreg, k), C.lastline);
                    }
                    else if (node.op == TokenType::kGt)
                    {
                        emit(C, make_op_lei(lreg, k), C.lastline);
                    }
                    else if (node.op == TokenType::kGe)
                    {
                        emit(C, make_op_lti(lreg, k), C.lastline);
                    }
                }

                *jump_patch_location = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);

                if (l_free)
                {
                    free_reg(C, lreg);
                }
                return;
            }

            if (node.right->try_as<AstFP>()
                && (node.op == TokenType::kLt || node.op == TokenType::kLe || node.op == TokenType::kGt
                    || node.op == TokenType::kGe))
            {
                auto* rhs_fp = node.right->try_as<AstFP>();
                compile_for_jump = false;
                auto [lreg, l_free] = try_get_rk(node.left);
                compile_for_jump = true;

                auto const k = add_fp_constant(C, rhs_fp->value);

                if (node.op == TokenType::kLt)
                {
                    emit(C, make_op_gef(lreg, k), C.lastline);
                }
                else if (node.op == TokenType::kLe)
                {
                    emit(C, make_op_gtf(lreg, k), C.lastline);
                }
                else if (node.op == TokenType::kGt)
                {
                    emit(C, make_op_lef(lreg, k), C.lastline);
                }
                else if (node.op == TokenType::kGe)
                {
                    emit(C, make_op_ltf(lreg, k), C.lastline);
                }

                *jump_patch_location = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);

                if (l_free)
                {
                    free_reg(C, lreg);
                }
                return;
            }

            compile_for_jump = false;
            auto [lreg, l_free] = try_get_rk(node.left);
            auto [rreg, r_free] = try_get_rk(node.right);
            compile_for_jump = true;

            if (node.op == TokenType::kEq)
            {
                emit(C, make_op_ne(lreg, rreg), C.lastline);
            }
            else if (node.op == TokenType::kNe)
            {
                emit(C, make_op_eq(lreg, rreg), C.lastline);
            }
            else if (node.op == TokenType::kLt)
            {
                emit(C, make_op_ge(lreg, rreg), C.lastline);
            }
            else if (node.op == TokenType::kLe)
            {
                emit(C, make_op_gt(lreg, rreg), C.lastline);
            }
            else if (node.op == TokenType::kGt)
            {
                emit(C, make_op_le(lreg, rreg), C.lastline);
            }
            else if (node.op == TokenType::kGe)
            {
                emit(C, make_op_lt(lreg, rreg), C.lastline);
            }

            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);

            if (r_free)
            {
                free_reg(C, rreg);
            }
            if (l_free)
            {
                free_reg(C, lreg);
            }
            return;
        }

        if (node.op == TokenType::kPlus && node.right->try_as<AstInt>())
        {
            auto* rhs_int = node.right->try_as<AstInt>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();
            // Use immediate encoding for small integers (-256 to 255)
            if (rhs_int->value >= -256 && rhs_int->value <= 255)
            {
                emit(C, make_op_addimm(result_reg, left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
            }
            else
            {
                const auto k = add_integer_constant(C, rhs_int->value);
                emit(C, make_op_addki(result_reg, left_reg, k), C.lastline);
            }
            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }
        if (node.op == TokenType::kPlus && node.right->try_as<AstFP>())
        {
            auto* rhs_fp = node.right->try_as<AstFP>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();
            const auto k = add_fp_constant(C, rhs_fp->value);
            emit(C, make_op_addkf(result_reg, left_reg, k), C.lastline);
            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }
        if (node.op == TokenType::kMinus && node.right->try_as<AstInt>())
        {
            auto* rhs_int = node.right->try_as<AstInt>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();
            // Use immediate encoding for small integers (-256 to 255)
            if (rhs_int->value >= -256 && rhs_int->value <= 255)
            {
                emit(C, make_op_subimm(result_reg, left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
            }
            else
            {
                const auto k = add_integer_constant(C, rhs_int->value);
                emit(C, make_op_subki(result_reg, left_reg, k), C.lastline);
            }
            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }
        if (node.op == TokenType::kMinus && node.right->try_as<AstFP>())
        {
            auto* rhs_fp = node.right->try_as<AstFP>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();
            const auto k = add_fp_constant(C, rhs_fp->value);
            emit(C, make_op_subkf(result_reg, left_reg, k), C.lastline);
            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }

        // Optimize equality comparisons with constant right operands (i == 0, x != 1, etc)
        if (!compile_for_jump && node.right->try_as<AstInt>() && (node.op == TokenType::kEq || node.op == TokenType::kNe))
        {
            auto* rhs_int = node.right->try_as<AstInt>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();

            // Use immediate opcode if the value fits in 17-bit signed range
            if (rhs_int->value >= -65536 && rhs_int->value <= 65535)
            {
                if (node.op == TokenType::kEq)
                {
                    emit(C, make_op_eqimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
                else if (node.op == TokenType::kNe)
                {
                    emit(C, make_op_neimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
            }
            else
            {
                // Fallback to regular EQ with register
                const auto k = add_integer_constant(C, rhs_int->value);
                const Reg rreg = alloc_reg(C);
                emit(C, make_op_loadi(rreg, k), C.lastline);

                if (node.op == TokenType::kEq)
                {
                    emit(C, make_op_eq(left_reg, rreg), C.lastline);
                }
                else if (node.op == TokenType::kNe)
                {
                    emit(C, make_op_ne(left_reg, rreg), C.lastline);
                }

                free_reg(C, rreg);
            }

            emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
            emit(C, make_op_loadbool(result_reg, false, false), C.lastline);

            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }

        // Optimize comparisons with constant right operands (i < 10, x <= 5.5, etc)
        if (!compile_for_jump && node.right->try_as<AstInt>()
            && (node.op == TokenType::kLt || node.op == TokenType::kLe || node.op == TokenType::kGt
                || node.op == TokenType::kGe))
        {
            auto* rhs_int = node.right->try_as<AstInt>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();

            // Use immediate opcodes if the value fits in 17-bit signed range
            if (rhs_int->value >= -65536 && rhs_int->value <= 65535)
            {
                if (node.op == TokenType::kLt)
                {
                    emit(C, make_op_ltimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
                else if (node.op == TokenType::kLe)
                {
                    emit(C, make_op_leimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
                else if (node.op == TokenType::kGt)
                {
                    emit(C, make_op_gtimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
                else if (node.op == TokenType::kGe)
                {
                    emit(C, make_op_geimm(left_reg, static_cast<int32_t>(rhs_int->value)), C.lastline);
                }
            }
            else
            {
                const auto k = add_integer_constant(C, rhs_int->value);

                if (node.op == TokenType::kLt)
                {
                    emit(C, make_op_lti(left_reg, k), C.lastline);
                }
                else if (node.op == TokenType::kLe)
                {
                    emit(C, make_op_lei(left_reg, k), C.lastline);
                }
                else if (node.op == TokenType::kGt)
                {
                    emit(C, make_op_gti(left_reg, k), C.lastline);
                }
                else if (node.op == TokenType::kGe)
                {
                    emit(C, make_op_gei(left_reg, k), C.lastline);
                }
            }

            emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
            emit(C, make_op_loadbool(result_reg, false, false), C.lastline);

            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }

        if (!compile_for_jump && node.right->try_as<AstFP>()
            && (node.op == TokenType::kLt || node.op == TokenType::kLe || node.op == TokenType::kGt
                || node.op == TokenType::kGe))
        {
            auto* rhs_fp = node.right->try_as<AstFP>();
            auto saved_target = target_reg;
            target_reg = std::nullopt;
            auto [left_reg, left_free] = try_get_rk(node.left);
            target_reg = saved_target;
            Reg result_reg = get_target_reg();

            const auto k = add_fp_constant(C, rhs_fp->value);

            if (node.op == TokenType::kLt)
            {
                emit(C, make_op_ltf(left_reg, k), C.lastline);
            }
            else if (node.op == TokenType::kLe)
            {
                emit(C, make_op_lef(left_reg, k), C.lastline);
            }
            else if (node.op == TokenType::kGt)
            {
                emit(C, make_op_gtf(left_reg, k), C.lastline);
            }
            else if (node.op == TokenType::kGe)
            {
                emit(C, make_op_gef(left_reg, k), C.lastline);
            }

            emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
            emit(C, make_op_loadbool(result_reg, false, false), C.lastline);

            if (left_free)
            {
                free_reg(C, left_reg);
            }
            return;
        }

        auto saved_target = target_reg;
        target_reg = std::nullopt;

        auto [left_reg, left_free] = try_get_rk(node.left);
        node.right->accept(*this);
        Reg right_reg = C.freereg - 1;

        // Allocate result register: reuse left if it's temporary, else use saved target or allocate new
        Reg result_reg;
        if (left_free && !saved_target.has_value())
        {
            // Left is a temporary expression result - reuse its register
            result_reg = left_reg;
        }
        else
        {
            // Left is a local/upvalue, or we have a target - allocate new register
            target_reg = saved_target;
            result_reg = get_target_reg();
        }

        Instruction instr{};
        bool emitted_sequence = false;

        switch (node.op)
        {
            case TokenType::kPlus:
                instr = make_op_add(result_reg, left_reg, right_reg);
                break;
            case TokenType::kMinus:
                instr = make_op_sub(result_reg, left_reg, right_reg);
                break;
            case TokenType::kStar:
                instr = make_op_mul(result_reg, left_reg, right_reg);
                break;
            case TokenType::kPower:
                instr = make_op_pow(result_reg, left_reg, right_reg);
                break;
            case TokenType::kSlash:
                instr = make_op_div(result_reg, left_reg, right_reg);
                break;
            case TokenType::kPercent:
                instr = make_op_mod(result_reg, left_reg, right_reg);
                break;
            case TokenType::kBAnd:
                instr = make_op_band(result_reg, left_reg, right_reg);
                break;
            case TokenType::kBOr:
                instr = make_op_bor(result_reg, left_reg, right_reg);
                break;
            case TokenType::kBXor:
                instr = make_op_bxor(result_reg, left_reg, right_reg);
                break;
            case TokenType::kBShl:
                instr = make_op_shl(result_reg, left_reg, right_reg);
                break;
            case TokenType::kBShr:
                instr = make_op_shr(result_reg, left_reg, right_reg);
                break;
            case TokenType::kEq:

                emit(C, make_op_eq(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            case TokenType::kNe:

                emit(C, make_op_ne(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            case TokenType::kLt:
                emit(C, make_op_lt(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            case TokenType::kLe:
                emit(C, make_op_le(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            case TokenType::kGt:

                emit(C, make_op_gt(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            case TokenType::kGe:

                emit(C, make_op_ge(left_reg, right_reg), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                emitted_sequence = true;
                break;
            default:
                throw TypeError("Unsupported binary operator", get_location(C));
        }
        if (!emitted_sequence)
        {
            emit(C, std::move(instr), binop_line, binop_column);
        }
        free_reg(C, right_reg);
        if (left_free && result_reg != left_reg)
        {
            // Only free left if we didn't reuse it as result
            free_reg(C, left_reg);
        }

        // Ensure freereg is positioned correctly so result is at freereg-1
        // This is critical because callers expect the result at freereg-1
        C.freereg = result_reg + 1;
        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }
        if (C.freereg > C.current_proto->max_stack_size)
        {
            C.current_proto->max_stack_size = C.freereg;
        }
    }

    void VisitorAdapter::visit(const AstUnOp& node)
    {
        Reg result_reg = get_target_reg();
        node.expr->accept(*this);
        Reg expr_reg = C.freereg - 1;
        Instruction instr;
        switch (node.op)
        {
            case TokenType::kMinus:
                instr = make_op_unm(result_reg, expr_reg);
                break;
            case TokenType::kNotOp:

                emit(C, make_op_test(expr_reg, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, true, true), C.lastline);
                emit(C, make_op_loadbool(result_reg, false, false), C.lastline);
                free_reg(C, expr_reg);

                if (compile_for_jump && jump_patch_location)
                {
                    emit(C, make_op_test(result_reg, true), C.lastline);
                    *jump_patch_location = C.current_proto->code.size();
                    emit(C, make_op_jmp(0), C.lastline);
                    free_reg(C, result_reg);
                }
                return;
            case TokenType::kHash:
                instr = make_op_len(result_reg, expr_reg);
                break;
            case TokenType::kBNot:
                instr = make_op_bnot(result_reg, expr_reg);
                break;
            default:
                throw TypeError("Unsupported unary operator", get_location(C));
        }
        emit(C, std::move(instr), C.lastline);
        free_reg(C, expr_reg);

        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(result_reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, result_reg);
        }
    }

    void VisitorAdapter::visit(const AstTernary& node)
    {
        Reg result_reg = get_target_reg();

        // Compile condition
        auto saved_target = target_reg;
        target_reg = std::nullopt;
        node.condition->accept(*this);
        Reg cond_reg = C.freereg - 1;

        // Test condition: if false, jump to false branch
        emit(C, make_op_test(cond_reg, true), C.lastline);
        size_t jmp_to_false = C.current_proto->code.size();
        emit(C, make_op_jmp(0), C.lastline);
        free_reg(C, cond_reg);

        // True branch: evaluate true_expr into result_reg
        target_reg = result_reg;
        node.true_expr->accept(*this);
        // Free any temporary registers used by true_expr
        while (C.freereg > result_reg + 1)
        {
            free_reg(C, C.freereg - 1);
        }
        target_reg = saved_target;

        // Jump over false branch
        size_t jmp_to_end = C.current_proto->code.size();
        emit(C, make_op_jmp(0), C.lastline);

        // Patch jump to false branch
        const auto jmp_false_offset = static_cast<int32_t>(C.current_proto->code.size() - jmp_to_false - 1);
        C.current_proto->code[jmp_to_false] = make_op_jmp(jmp_false_offset);

        // False branch: evaluate false_expr into result_reg
        target_reg = result_reg;
        node.false_expr->accept(*this);
        // Free any temporary registers used by false_expr
        while (C.freereg > result_reg + 1)
        {
            free_reg(C, C.freereg - 1);
        }
        target_reg = saved_target;

        // Patch jump to end
        const auto jmp_end_offset = static_cast<int32_t>(C.current_proto->code.size() - jmp_to_end - 1);
        C.current_proto->code[jmp_to_end] = make_op_jmp(jmp_end_offset);

        // Result is already in result_reg from either branch
        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(result_reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, result_reg);
        }
    }

    void VisitorAdapter::compile_call(const AstFuncCall& node, uint8_t nresults)
    {
        // Defensive: ensure freereg respects min_freereg at function entry
        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }

        // Check if this is a call to tostring/tonumber builtin with exactly one argument
        if (const auto* ident = node.func->try_as<AstIdent>())
        {
            const bool is_tostring = ident->name->view() == "tostring";
            const bool is_tonumber = ident->name->view() == "tonumber";

            // Check if exactly one arg (first_arg != nullptr and first_arg->next_child == nullptr)
            if ((is_tostring || is_tonumber) && node.first_arg && !node.first_arg->next_child && !node.is_self_call)
            {
                // Emit kOpToString bytecode instead of a function call
                auto saved_target = target_reg;
                target_reg = std::nullopt;

                // Ensure freereg is at least min_freereg before allocating registers
                if (C.freereg < C.min_freereg)
                {
                    C.freereg = C.min_freereg;
                }

                // We need to mimic what a normal call does:
                // 1. Function goes in a register (but we skip this since it's builtin)
                // 2. Args go in consecutive registers after function
                // 3. Result comes back in function register

                // Allocate a register for where the result will go (simulating func_reg)
                Reg func_reg = C.freereg;
                C.freereg++;

                // Compile the argument into the next register
                node.first_arg->accept(*this);
                Reg arg_reg = C.freereg - 1;

                // Emit TOSTRING or TONUMBER: result goes to func_reg, reads from arg_reg
                if (is_tostring)
                {
                    emit(C, make_op_tostring(func_reg, arg_reg), node.line);
                }
                else
                {
                    emit(C, make_op_tonumber(func_reg, arg_reg), node.line);
                }

                // Match the behavior of a normal call: set freereg based on nresults
                // tostring/tonumber ALWAYS produce exactly 1 result (not variadic)
                // So we set freereg to func_reg + 1, regardless of what nresults was requested
                C.freereg = func_reg + 1;

                if (C.freereg < C.min_freereg)
                {
                    C.freereg = C.min_freereg;
                }

                // Handle target_reg if specified (move result to target)
                if (saved_target.has_value() && nresults == 1)
                {
                    Reg target = saved_target.value();
                    if (func_reg != target)
                    {
                        emit(C, make_op_move(target, func_reg), C.lastline);
                        C.freereg = target + 1;
                        if (C.freereg < C.min_freereg)
                        {
                            C.freereg = C.min_freereg;
                        }
                    }
                }

                target_reg = saved_target;
                return;
            }
        }

        auto saved_target = target_reg;
        target_reg = std::nullopt;

        bool saved_compile_for_jump = compile_for_jump;
        size_t* saved_jump_patch = jump_patch_location;
        compile_for_jump = false;
        jump_patch_location = nullptr;

        // Save the call site location for error reporting
        int32_t call_line = node.line;
        int32_t call_column = node.column;

        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }

        node.func->accept(*this);
        Reg func_reg = C.freereg - 1;
        Reg arg_base = func_reg + 1;

        // Count args and find last arg
        size_t args_count = 0;
        AstNode* last_arg = nullptr;
        for (AstNode* arg = node.first_arg; arg; arg = arg->next_child)
        {
            args_count++;
            last_arg = arg;
        }

        bool last_is_call = false;
        bool last_is_vararg = false;
        size_t regular_args = args_count;
        if (last_arg)
        {
            const auto* last_arg_call = last_arg->try_as<AstFuncCall>();
            const auto* last_arg_vararg = last_arg->try_as<AstVararg>();
            if (last_arg_call)
            {
                last_is_call = true;
                regular_args = args_count - 1;
            }
            else if (last_arg_vararg)
            {
                last_is_vararg = true;
                regular_args = args_count - 1;
            }
        }

        // Compile regular args
        size_t i = 0;
        for (AstNode* arg = node.first_arg; arg && i < regular_args; arg = arg->next_child, ++i)
        {
            arg->accept(*this);
            Reg arg_reg = C.freereg - 1;
            Reg dest_reg = arg_base + static_cast<Reg>(i);
            if (arg_reg != dest_reg)
            {
                emit(C, make_op_move(dest_reg, arg_reg), C.lastline);
            }
            C.freereg = static_cast<Reg>(arg_base + i + 1);
            if (C.freereg < C.min_freereg)
            {
                C.freereg = C.min_freereg;
            }
        }

        if (last_is_call)
        {
            const auto* last_call = last_arg->try_as<AstFuncCall>();
            compile_call(*last_call, static_cast<uint8_t>(kMultRet));

            emit(C, make_op_call(func_reg, static_cast<uint8_t>(kMultArgs), nresults, node.is_self_call), call_line,
                call_column);
        }
        else if (last_is_vararg)
        {
            // Use VARARG to push varargs onto stack starting after regular args
            Reg vararg_dest = arg_base + static_cast<Reg>(regular_args);
            emit(C, make_op_vararg(vararg_dest, 0), C.lastline); // 0 means "all remaining varargs"

            // Call with kMultArgs (variable number of args from stack)
            emit(C, make_op_call(func_reg, static_cast<uint8_t>(kMultArgs), nresults, node.is_self_call), call_line,
                call_column);
        }
        else
        {
            uint8_t num_args = static_cast<uint8_t>(args_count + 1);
            emit(C, make_op_call(func_reg, num_args, nresults, node.is_self_call), call_line, call_column);
        }

        if (nresults == static_cast<uint8_t>(kMultRet))
        {
            // kMultRet means variadic returns - we don't know how many, so assume at least 1
            C.freereg = func_reg + 1;
            if (C.freereg < C.min_freereg)
            {
                C.freereg = C.min_freereg;
            }
        }
        else
        {
            C.freereg = func_reg + nresults;
            if (C.freereg < C.min_freereg)
            {
                C.freereg = C.min_freereg;
            }
        }

        if (saved_target.has_value() && nresults == 1)
        {
            Reg target = saved_target.value();
            if (func_reg != target)
            {
                emit(C, make_op_move(target, func_reg), C.lastline);
                C.freereg = target + 1;
                if (C.freereg < C.min_freereg)
                {
                    C.freereg = C.min_freereg;
                }
            }
        }

        compile_for_jump = saved_compile_for_jump;
        jump_patch_location = saved_jump_patch;
    }

    void VisitorAdapter::visit(const AstFuncCall& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        compile_call(node, 1);

        C.freereg = C.freereg - 1 + 1;

        if (compile_for_jump && jump_patch_location)
        {
            Reg reg = C.freereg - 1;
            emit(C, make_op_test(reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, reg);
        }
    }

    void VisitorAdapter::visit(const AstTableCtor& node)
    {
        const auto reg = get_target_reg();
        uint8_t array_hint = 0;
        uint8_t hash_hint = 0;
        // Count array vs hash fields
        for (AstNode* n = node.first_field; n; n = n->next_child)
        {
            auto* field = static_cast<TableField*>(n);
            if (!field->key)
            {
                ++array_hint;
            }
            else
            {
                ++hash_hint;
            }
        }
        emit(C, make_op_newtable(reg, array_hint, hash_hint), C.lastline);
        Reg temp_reg = alloc_reg(C);
        uint32_t array_idx = 0; // Start from 0
        for (AstNode* n = node.first_field; n; n = n->next_child)
        {
            auto* field = static_cast<TableField*>(n);

            if (field->key)
            {
                // Hash field
                field->value->accept(*this);
                Reg val_reg = C.freereg - 1;
                Reg key_reg;
                if (auto* id = field->key->try_as<AstIdent>())
                {
                    key_reg = alloc_reg(C);
                    const auto k = add_string_constant(C, id->name->view());
                    emit(C, make_op_loads(key_reg, k), C.lastline);
                    emit(C, make_op_setfield(reg, key_reg, val_reg), C.lastline);
                    free_reg(C, key_reg);
                }
                else
                {
                    field->key->accept(*this);
                    key_reg = C.freereg - 1;
                    emit(C, make_op_setfield(reg, key_reg, val_reg), C.lastline);
                    free_reg(C, key_reg);
                }
                free_reg(C, val_reg);
            }
            else
            {
                // Array field - check for varargs
                if (field->value->try_as<AstVararg>())
                {
                    // Check if there are any fields after this vararg
                    if (n->next_child)
                    {
                        throw SyntaxError(
                            "Table constructor: vararg expansion (...) must be the last element", get_location(C));
                    }

                    // Use VARARGEXPAND to copy varargs directly into table
                    emit(C, make_op_varargexpand(reg, static_cast<uint8_t>(array_idx)), C.lastline);
                    break;
                }
                else
                {
                    // Regular array value - use SETFIELD with integer key
                    field->value->accept(*this);
                    Reg val_reg = C.freereg - 1;
                    Reg key_reg = alloc_reg(C);
                    const auto k = add_integer_constant(C, array_idx++);
                    emit(C, make_op_loadi(key_reg, k), C.lastline);
                    emit(C, make_op_setfield(reg, key_reg, val_reg), C.lastline);
                    free_reg(C, key_reg);
                    free_reg(C, val_reg);
                }
            }
        }
        free_reg(C, temp_reg);
        C.freereg = reg + 1;
        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }
    }

    void VisitorAdapter::visit(const AstIndex& node)
    {
        Reg result_reg = get_target_reg();

        auto [table_reg, table_free] = try_get_reg(node.table);

        // Check if key is a constant integer in range [0, 511] for GETFIELDI optimization
        if (auto* int_node = dynamic_cast<const AstInt*>(node.key))
        {
            if (int_node->value >= 0 && int_node->value <= 511)
            {
                emit(C, make_op_getfieldi(result_reg, table_reg, static_cast<int32_t>(int_node->value)), C.lastline);

                if (table_free)
                {
                    free_reg(C, table_reg);
                }

                if (compile_for_jump && jump_patch_location)
                {
                    emit(C, make_op_test(result_reg, true), C.lastline);
                    *jump_patch_location = C.current_proto->code.size();
                    emit(C, make_op_jmp(0), C.lastline);
                    free_reg(C, result_reg);
                }
                return;
            }
        }

        // Check if key is a string constant in range [0, 511] for GETFIELDS optimization
        if (auto* str_node = dynamic_cast<const AstString*>(node.key))
        {
            ConstIndex k = add_string_constant(C, str_node->view());
            if (k <= 511)
            {
                emit(C, make_op_getfields(result_reg, table_reg, k), C.lastline);

                if (table_free)
                {
                    free_reg(C, table_reg);
                }

                if (compile_for_jump && jump_patch_location)
                {
                    emit(C, make_op_test(result_reg, true), C.lastline);
                    *jump_patch_location = C.current_proto->code.size();
                    emit(C, make_op_jmp(0), C.lastline);
                    free_reg(C, result_reg);
                }
                return;
            }
        }

        auto [key_reg, key_free] = try_get_rk(node.key);

        emit(C, make_op_getfield(result_reg, table_reg, key_reg), C.lastline);

        if (key_free)
        {
            free_reg(C, key_reg);
        }
        if (table_free)
        {
            free_reg(C, table_reg);
        }

        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(result_reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, result_reg);
        }
    }

    void VisitorAdapter::visit(const AstMember& node)
    {
        Reg result_reg = get_target_reg();
        node.table->accept(*this);
        Reg table_reg = C.freereg - 1;
        const auto k = add_string_constant(C, node.name->view());

        // Use GETFIELDS if the constant index fits in 9 bits
        if (k <= 511)
        {
            emit(C, make_op_getfields(result_reg, table_reg, k), C.lastline);
        }
        else
        {
            // Fallback to LOADS + GETFIELD for large constant indices
            Reg key_reg = alloc_reg(C);
            emit(C, make_op_loads(key_reg, k), C.lastline);
            emit(C, make_op_getfield(result_reg, table_reg, key_reg), C.lastline);
            free_reg(C, key_reg);
        }
        free_reg(C, table_reg);

        if (compile_for_jump && jump_patch_location)
        {
            emit(C, make_op_test(result_reg, true), C.lastline);
            *jump_patch_location = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            free_reg(C, result_reg);
        }
    }

    void VisitorAdapter::visit(const AstFuncDef& node)
    {
        auto* child_proto = gc_new_proto(C.S);
        child_proto->source_name = C.current_proto->source_name;
        // Count params
        uint32_t param_count = 0;
        for (AstNode* p = node.first_param; p; p = p->next_child)
        {
            param_count++;
        }
        child_proto->num_params = param_count;
        child_proto->is_vararg = node.is_vararg;

        // Set function name for debugging
        if (!node.self_name.empty())
        {
            child_proto->name = gc_new_string(C.S, node.self_name);
        }
        else
        {
            child_proto->name = gc_new_string(C.S, "<anonymous>");
        }

        C.current_proto->protos.push_back(C.S, child_proto);
        const auto proto_idx = static_cast<ProtoIndex>(C.current_proto->protos.size() - 1);

        CompilerState child(C.S, &C, C.current_proto->protos[proto_idx]);

        // Emit VARARGPREP as first instruction if this is a vararg function
        if (node.is_vararg)
        {
            emit(child, make_op_varargprep(static_cast<uint8_t>(param_count)), C.lastline);
        }

        enter_scope(child);

        uint8_t first_param_reg = 0;

        if (node.is_method)
        {
            Local self_loc{ "self", static_cast<int>(child.current_proto->code.size()), 0, false };
            child.scopes.back().push_back(self_loc);
            first_param_reg = 1;
        }
        else
        {
            first_param_reg = 1;

            if (!node.self_name.empty())
            {
                Local self_ref_loc{ node.self_name, static_cast<int>(child.current_proto->code.size()), 0, false };
                child.scopes.back().push_back(self_ref_loc);
            }
        }

        uint8_t param_idx = 0;
        for (AstNode* p = node.first_param; p; p = p->next_child)
        {
            auto* param_str = static_cast<AstString*>(p);
            Reg param_reg = static_cast<Reg>(first_param_reg + param_idx);
            Local param_loc{ param_str->view(), static_cast<int>(child.current_proto->code.size()), param_reg, false };
            child.scopes.back().push_back(param_loc);
            param_idx++;
        }

        child.freereg = first_param_reg + param_idx;
        child.min_freereg = child.freereg;

        VisitorAdapter child_visitor(child);
        if (node.block)
        {
            child_visitor.visit(*node.block);
        }

        // Only add implicit return if the last instruction isn't already a return
        if (child.current_proto->code.empty() || child.current_proto->code.back().op() != OpCode::kOpReturn)
        {
            // Implicit return: return 0 values (nil-padding happens at call site if needed)
            // Emit with line 0 to indicate this is not user-visible and shouldn't trigger breakpoints
            emit(child, make_op_return(0, 0), 0, 0);
        }

        child.current_proto->num_params = param_count;

        leave_scope(child);

        // If child has upvalues, parent needs to close them when returning
        if (child.current_proto->has_upvalues)
        {
            C.current_proto->has_upvalues = true;
        }

        const auto reg = get_target_reg();
        emit(C, make_op_closure(reg, proto_idx), C.lastline);
        for (const auto& up : child.upvalues)
        {
            if (up.is_local)
            {
                emit(C, make_op_move(0, up.index), C.lastline);
            }
            else
            {
                emit(C, make_op_getupval(0, up.index), C.lastline);
            }
        }
    }

    void VisitorAdapter::visit(const AstAssign& node)
    {
        // Count vars and exprs
        size_t var_count = 0, expr_count = 0;
        for (AstNode* v = node.first_var; v; v = v->next_child)
        {
            ++var_count;
        }
        for (AstNode* e = node.first_expr; e; e = e->next_child)
        {
            ++expr_count;
        }

        if (var_count == 1 && expr_count == 1)
        {
            if (auto* idx = node.first_var->try_as<AstIndex>())
            {
                // Check if key is a constant integer in range [0, 511] for SETFIELDI optimization
                if (auto* int_node = dynamic_cast<const AstInt*>(idx->key))
                {
                    if (int_node->value >= 0 && int_node->value <= 511)
                    {
                        auto [table_reg, table_needs_free] = try_get_rk(idx->table);
                        auto [val_reg, val_needs_free] = try_get_rk(node.first_expr);

                        emit(C, make_op_setfieldi(table_reg, val_reg, static_cast<int32_t>(int_node->value)), C.lastline);

                        if (val_needs_free)
                        {
                            free_reg(C, val_reg);
                        }
                        if (table_needs_free)
                        {
                            free_reg(C, table_reg);
                        }

                        return;
                    }
                }

                // Check if key is a string constant in range [0, 511] for SETFIELDS optimization
                if (auto* str_node = dynamic_cast<const AstString*>(idx->key))
                {
                    ConstIndex k = add_string_constant(C, str_node->view());
                    if (k <= 511)
                    {
                        auto [table_reg, table_needs_free] = try_get_rk(idx->table);
                        auto [val_reg, val_needs_free] = try_get_rk(node.first_expr);

                        emit(C, make_op_setfields(table_reg, val_reg, k), C.lastline);

                        if (val_needs_free)
                        {
                            free_reg(C, val_reg);
                        }
                        if (table_needs_free)
                        {
                            free_reg(C, table_reg);
                        }

                        return;
                    }
                }

                auto [table_reg, table_needs_free] = try_get_rk(idx->table);
                auto [key_reg, key_needs_free] = try_get_rk(idx->key);
                auto [val_reg, val_needs_free] = try_get_rk(node.first_expr);

                emit(C, make_op_setfield(table_reg, key_reg, val_reg), C.lastline);

                if (val_needs_free)
                {
                    free_reg(C, val_reg);
                }
                if (key_needs_free)
                {
                    free_reg(C, key_reg);
                }
                if (table_needs_free)
                {
                    free_reg(C, table_reg);
                }

                return;
            }
        }

        Reg expr_base = C.freereg;
        size_t num_exprs = expr_count;

        // Handle multi-return from single function call (like let a, b, c = func())
        if (expr_count == 1 && var_count > 1 && node.first_expr)
        {
            const auto* call_expr = node.first_expr->try_as<AstFuncCall>();
            if (call_expr)
            {
                compile_call(*call_expr, static_cast<uint8_t>(var_count));
                Reg call_result_base = C.freereg - static_cast<uint8_t>(var_count);

                // Assign results to variables
                size_t i = 0;
                for (AstNode* v = node.first_var; v; v = v->next_child, ++i)
                {
                    Reg val_reg = static_cast<Reg>(call_result_base + i);
                    if (auto* id = v->try_as<AstIdent>())
                    {
                        auto id_name = id->name->view();
                        int32_t loc = resolve_local(C, id_name);
                        if (loc >= 0)
                        {
                            if (is_local_const(C, id_name))
                            {
                                throw SemanticError(
                                    behl::format("Cannot assign to const variable '{}'", id_name), get_location(C));
                            }
                            emit(C, make_op_move(static_cast<uint8_t>(loc), val_reg), C.lastline);
                        }
                        else
                        {
                            uint32_t up = resolve_upvalue(C, id_name);
                            if (up != kInvalidUpvalue)
                            {
                                if (is_upvalue_const(C, id_name))
                                {
                                    throw SemanticError(
                                        behl::format("Cannot assign to const upvalue '{}'", id_name), get_location(C));
                                }
                                emit(C, make_op_setupval(val_reg, static_cast<uint8_t>(up)), C.lastline);
                            }
                            else
                            {
                                const auto k = add_string_constant(C, id->name->view());
                                emit(C, make_op_setglobal(val_reg, k), C.lastline);
                            }
                        }
                    }
                    else if (auto* idx = v->try_as<AstIndex>())
                    {
                        idx->table->accept(*this);
                        Reg table_reg = C.freereg - 1;
                        auto [key_reg, key_needs_free] = try_get_rk(idx->key);
                        emit(C, make_op_setfield(table_reg, key_reg, val_reg), C.lastline);
                        if (key_needs_free)
                        {
                            free_reg(C, key_reg);
                        }
                        free_reg(C, table_reg);
                    }
                    else if (auto* mem = v->try_as<AstMember>())
                    {
                        mem->table->accept(*this);
                        Reg table_reg = C.freereg - 1;
                        const auto k = add_string_constant(C, mem->name->view());
                        if (k <= 511)
                        {
                            emit(C, make_op_setfields(table_reg, val_reg, k), C.lastline);
                        }
                        else
                        {
                            Reg key_reg = alloc_reg(C);
                            emit(C, make_op_loads(key_reg, k), C.lastline);
                            emit(C, make_op_setfield(table_reg, key_reg, val_reg), C.lastline);
                            free_reg(C, key_reg);
                        }
                        free_reg(C, table_reg);
                    }
                }

                // Free registers used for call results
                C.freereg = expr_base;
                return;
            }
        }

        for (AstNode* e = node.first_expr; e; e = e->next_child)
        {
            e->accept(*this);
        }
        if (num_exprs < var_count)
        {
            for (size_t i = num_exprs; i < var_count; ++i)
            {
                Reg nil_reg = alloc_reg(C);
                emit(C, make_op_loadnil(nil_reg, 0), C.lastline);
            }
            num_exprs = var_count;
        }
        size_t i = 0;
        for (AstNode* v = node.first_var; v; v = v->next_child, ++i)
        {
            Reg val_reg = C.freereg - static_cast<Reg>(num_exprs - i);
            if (auto* id = v->try_as<AstIdent>())
            {
                auto id_name = id->name->view();
                int32_t loc = resolve_local(C, id_name);
                if (loc >= 0)
                {
                    if (is_local_const(C, id_name))
                    {
                        throw SemanticError(behl::format("Cannot assign to const variable '{}'", id_name), get_location(C));
                    }
                    emit(C, make_op_move(static_cast<uint8_t>(loc), val_reg), C.lastline);
                }
                else
                {
                    uint32_t up = resolve_upvalue(C, id_name);
                    if (up != kInvalidUpvalue)
                    {
                        if (is_upvalue_const(C, id_name))
                        {
                            throw SemanticError(behl::format("Cannot assign to const upvalue '{}'", id_name), get_location(C));
                        }
                        emit(C, make_op_setupval(val_reg, static_cast<uint8_t>(up)), C.lastline);
                    }
                    else
                    {
                        const auto k = add_string_constant(C, id->name->view());
                        emit(C, make_op_setglobal(val_reg, k), C.lastline);
                    }
                }
            }
            else if (auto* idx = v->try_as<AstIndex>())
            {
                // Check if key is a constant integer in range [0, 511] for SETFIELDI optimization
                if (auto* int_node = dynamic_cast<const AstInt*>(idx->key))
                {
                    if (int_node->value >= 0 && int_node->value <= 511)
                    {
                        idx->table->accept(*this);
                        Reg table_reg = C.freereg - 1;

                        emit(C, make_op_setfieldi(table_reg, val_reg, static_cast<int32_t>(int_node->value)), C.lastline);

                        free_reg(C, table_reg);
                        continue;
                    }
                }

                // Check if key is a string constant in range [0, 511] for SETFIELDS optimization
                if (auto* str_node = dynamic_cast<const AstString*>(idx->key))
                {
                    ConstIndex k = add_string_constant(C, str_node->view());
                    if (k <= 511)
                    {
                        idx->table->accept(*this);
                        Reg table_reg = C.freereg - 1;

                        emit(C, make_op_setfields(table_reg, val_reg, k), C.lastline);

                        free_reg(C, table_reg);
                        continue;
                    }
                }

                idx->table->accept(*this);
                Reg table_reg = C.freereg - 1;
                auto [key_reg, key_needs_free] = try_get_rk(idx->key);

                emit(C, make_op_setfield(table_reg, key_reg, val_reg), C.lastline);

                if (key_needs_free)
                {
                    free_reg(C, key_reg);
                }
                free_reg(C, table_reg);
            }
            else if (auto* mem = v->try_as<AstMember>())
            {
                mem->table->accept(*this);
                Reg table_reg = C.freereg - 1;
                const auto k = add_string_constant(C, mem->name->view());

                // Use SETFIELDS if the constant index fits in 9 bits
                if (k <= 511)
                {
                    emit(C, make_op_setfields(table_reg, val_reg, k), C.lastline);
                    free_reg(C, table_reg);
                }
                else
                {
                    // Fallback to LOADS + SETFIELD for large constant indices
                    Reg key_reg = alloc_reg(C);
                    emit(C, make_op_loads(key_reg, k), C.lastline);
                    emit(C, make_op_setfield(table_reg, key_reg, val_reg), C.lastline);
                    free_reg(C, key_reg);
                    free_reg(C, table_reg);
                }
            }
            else
            {
                throw SyntaxError("Invalid assignment target", get_location(C));
            }
        }
        C.freereg = expr_base;
    }

    void VisitorAdapter::visit(const AstAssignLocal& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        int32_t loc = resolve_local(C, node.name->view());
        if (loc < 0)
        {
            throw ReferenceError(behl::format("Local variable '{}' not found", node.name->view()), get_location(C));
        }

        if (is_local_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot assign to const variable '{}'", node.name->view()), get_location(C));
        }

        target_reg = static_cast<uint8_t>(loc);
        node.expr->accept(*this);
    }

    void VisitorAdapter::visit(const AstAssignGlobal& node)
    {
        node.expr->accept(*this);
        Reg val_reg = C.freereg - 1;

        const auto k = add_string_constant(C, node.name->view());
        emit(C, make_op_setglobal(val_reg, k), C.lastline);

        free_reg(C, val_reg);
    }

    void VisitorAdapter::visit(const AstAssignUpvalue& node)
    {
        node.expr->accept(*this);
        Reg val_reg = C.freereg - 1;

        uint32_t up = resolve_upvalue(C, node.name->view());
        if (up == kInvalidUpvalue)
        {
            throw ReferenceError(behl::format("Upvalue '{}' not found", node.name->view()));
        }

        if (is_upvalue_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot assign to const upvalue '{}'", node.name->view()), get_location(C));
        }

        emit(C, make_op_setupval(val_reg, static_cast<uint8_t>(up)), C.lastline);
        free_reg(C, val_reg);
    }

    void VisitorAdapter::visit(const AstCompoundAssign&)
    {
        throw SemanticError("Unresolved AstCompoundAssign - semantic analyzer should have transformed this", get_location(C));
    }

    void VisitorAdapter::visit(const AstCompoundLocal& node)
    {
        // Save the line/column of the compound assignment itself
        const int32_t compound_line = node.line;
        const int32_t compound_column = node.column;

        C.lastline = node.line;
        C.lastcolumn = node.column;

        int32_t loc = resolve_local(C, node.name->view());
        if (loc < 0)
        {
            throw ReferenceError(behl::format("Local variable '{}' not found", node.name->view()), get_location(C));
        }

        if (is_local_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot assign to const variable '{}'", node.name->view()), get_location(C));
        }

        if (node.op == TokenType::kPlus)
        {
            if (auto* rhs_ident = node.expr->try_as<AstIdent>())
            {
                int32_t rhs_loc = resolve_local(C, rhs_ident->name->view());
                if (rhs_loc >= 0)
                {
                    emit(C, make_op_addlocal(static_cast<uint8_t>(loc), static_cast<uint8_t>(rhs_loc)), compound_line,
                        compound_column);
                    return;
                }
            }

            if (auto* rhs_int = node.expr->try_as<AstInt>())
            {
                // Use immediate encoding for small integers (-256 to 255)
                if (rhs_int->value >= -256 && rhs_int->value <= 255)
                {
                    emit(C,
                        make_op_addimm(
                            static_cast<uint8_t>(loc), static_cast<uint8_t>(loc), static_cast<int32_t>(rhs_int->value)),
                        compound_line, compound_column);
                }
                else
                {
                    const auto k = add_integer_constant(C, rhs_int->value);
                    emit(C, make_op_addki(static_cast<uint8_t>(loc), static_cast<uint8_t>(loc), k), compound_line,
                        compound_column);
                }
                return;
            }
            if (auto* rhs_fp = node.expr->try_as<AstFP>())
            {
                const auto k = add_fp_constant(C, rhs_fp->value);
                emit(C, make_op_addkf(static_cast<uint8_t>(loc), static_cast<uint8_t>(loc), k), compound_line, compound_column);
                return;
            }
        }

        Reg lhs_reg = static_cast<uint8_t>(loc);

        node.expr->accept(*this);
        Reg rhs_reg = C.freereg - 1;

        if (node.op == TokenType::kPlus)
        {
            emit(C, make_op_add(static_cast<uint8_t>(loc), lhs_reg, rhs_reg), compound_line, compound_column);
        }
        else if (node.op == TokenType::kMinus)
        {
            emit(C, make_op_sub(static_cast<uint8_t>(loc), lhs_reg, rhs_reg), compound_line, compound_column);
        }
        else if (node.op == TokenType::kStar)
        {
            emit(C, make_op_mul(static_cast<uint8_t>(loc), lhs_reg, rhs_reg), compound_line, compound_column);
        }
        else if (node.op == TokenType::kSlash)
        {
            emit(C, make_op_div(static_cast<uint8_t>(loc), lhs_reg, rhs_reg), compound_line, compound_column);
        }
        else if (node.op == TokenType::kPercent)
        {
            emit(C, make_op_mod(static_cast<uint8_t>(loc), lhs_reg, rhs_reg), compound_line, compound_column);
        }

        free_reg(C, rhs_reg);
    }

    void VisitorAdapter::visit(const AstCompoundGlobal& node)
    {
        const auto k = add_string_constant(C, node.name->view());

        Reg lhs_reg = alloc_reg(C);
        emit(C, make_op_getglobal(lhs_reg, k), C.lastline);

        node.expr->accept(*this);
        Reg rhs_reg = C.freereg - 1;

        if (node.op == TokenType::kPlus)
        {
            emit(C, make_op_add(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kMinus)
        {
            emit(C, make_op_sub(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kStar)
        {
            emit(C, make_op_mul(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kSlash)
        {
            emit(C, make_op_div(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kPercent)
        {
            emit(C, make_op_mod(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }

        emit(C, make_op_setglobal(lhs_reg, k), C.lastline);

        free_reg(C, rhs_reg);
        free_reg(C, lhs_reg);
    }

    void VisitorAdapter::visit(const AstCompoundUpvalue& node)
    {
        uint32_t up = resolve_upvalue(C, node.name->view());
        if (up == kInvalidUpvalue)
        {
            throw ReferenceError(behl::format("Upvalue '{}' not found", node.name->view()));
        }

        if (is_upvalue_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot modify const upvalue '{}'", node.name->view()));
        }

        Reg lhs_reg = alloc_reg(C);
        emit(C, make_op_getupval(lhs_reg, static_cast<uint8_t>(up)), C.lastline);

        node.expr->accept(*this);
        Reg rhs_reg = C.freereg - 1;

        if (node.op == TokenType::kPlus)
        {
            emit(C, make_op_add(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kMinus)
        {
            emit(C, make_op_sub(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kStar)
        {
            emit(C, make_op_mul(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kSlash)
        {
            emit(C, make_op_div(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }
        else if (node.op == TokenType::kPercent)
        {
            emit(C, make_op_mod(lhs_reg, lhs_reg, rhs_reg), C.lastline);
        }

        emit(C, make_op_setupval(lhs_reg, static_cast<uint8_t>(up)), C.lastline);

        free_reg(C, rhs_reg);
        free_reg(C, lhs_reg);
    }

    void VisitorAdapter::visit(const AstIncrement&)
    {
        throw SemanticError("Unresolved AstIncrement - semantic analyzer should have transformed this", get_location(C));
    }

    void VisitorAdapter::visit(const AstIncLocal& node)
    {
        int32_t loc = resolve_local(C, node.name->view());
        if (loc < 0)
        {
            throw ReferenceError(behl::format("Local variable '{}' not found", node.name->view()), get_location(C));
        }

        if (is_local_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot modify const variable '{}'", node.name->view()), get_location(C));
        }

        emit(C, make_op_inclocal(static_cast<uint8_t>(loc)), C.lastline);
    }

    void VisitorAdapter::visit(const AstIncGlobal& node)
    {
        const auto k = add_string_constant(C, node.name->view());

        emit(C, make_op_incglobal(k), C.lastline);
    }

    void VisitorAdapter::visit(const AstIncUpvalue& node)
    {
        uint32_t up = resolve_upvalue(C, node.name->view());
        if (up == kInvalidUpvalue)
        {
            throw ReferenceError(behl::format("Upvalue '{}' not found", node.name->view()), get_location(C));
        }

        if (is_upvalue_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot modify const upvalue '{}'", node.name->view()), get_location(C));
        }

        emit(C, make_op_incupvalue(static_cast<uint8_t>(up)), C.lastline);
    }

    void VisitorAdapter::visit(const AstDecrement&)
    {
        throw SemanticError("Unresolved AstDecrement - semantic analyzer should have transformed this", get_location(C));
    }

    void VisitorAdapter::visit(const AstDecLocal& node)
    {
        int32_t loc = resolve_local(C, node.name->view());
        if (loc < 0)
        {
            throw ReferenceError(behl::format("Local variable '{}' not found", node.name->view()), get_location(C));
        }

        if (is_local_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot modify const variable '{}'", node.name->view()), get_location(C));
        }

        emit(C, make_op_declocal(static_cast<uint8_t>(loc)), C.lastline);
    }

    void VisitorAdapter::visit(const AstDecGlobal& node)
    {
        const auto k = add_string_constant(C, node.name->view());

        emit(C, make_op_decglobal(k), C.lastline);
    }

    void VisitorAdapter::visit(const AstDecUpvalue& node)
    {
        uint32_t up = resolve_upvalue(C, node.name->view());
        if (up == kInvalidUpvalue)
        {
            throw ReferenceError(behl::format("Upvalue '{}' not found", node.name->view()), get_location(C));
        }

        if (is_upvalue_const(C, node.name->view()))
        {
            throw SemanticError(behl::format("Cannot modify const upvalue '{}'", node.name->view()), get_location(C));
        }

        emit(C, make_op_decupvalue(static_cast<uint8_t>(up)), C.lastline);
    }

    void VisitorAdapter::visit(const AstLocalDecl& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        // Count names
        size_t name_count = 0;
        for (AstNode* n = reinterpret_cast<AstNode*>(node.first_name); n; n = n->next_child)
        {
            ++name_count;
        }

        AutoVector<Local> new_locals(C.S);
        new_locals.reserve(name_count);

        auto& scopes = C.scopes;
        for (AstNode* n = reinterpret_cast<AstNode*>(node.first_name); n; n = n->next_child)
        {
            auto* name = static_cast<AstString*>(n);
            Local loc;
            loc.name = name->view();
            loc.is_const = node.is_const;
            loc.reg = alloc_reg(C);
            loc.start_pc = static_cast<int>(C.current_proto->code.size());
            // In module mode, top-level consts are module-scoped (added to first scope)
            if (C.is_module && node.is_const && scopes.size() <= 2)
            {
                scopes.front().push_back(loc);
            }
            else
            {
                scopes.back().push_back(loc);
            }
            new_locals.push_back(loc);
        }

        Reg locals_end = C.freereg;

        if (locals_end > C.min_freereg)
        {
            C.min_freereg = locals_end;
        }

        // Count inits
        size_t init_count = 0;
        for (AstNode* init = node.first_init; init; init = init->next_child)
        {
            ++init_count;
        }

        if (init_count == 1 && new_locals.size() > 1 && node.first_init)
        {
            const auto* call_expr = node.first_init->try_as<AstFuncCall>();
            if (call_expr)
            {
                compile_call(*call_expr, static_cast<uint8_t>(new_locals.size()));
                Reg call_result_reg = C.freereg - static_cast<uint8_t>(new_locals.size());

                for (size_t i = 0; i < new_locals.size(); ++i)
                {
                    emit(C, make_op_move(new_locals[i].reg, static_cast<uint8_t>(call_result_reg + i)), C.lastline);
                }

                C.freereg = locals_end;
                if (C.freereg < C.min_freereg)
                {
                    C.freereg = C.min_freereg;
                }

                return;
            }
        }

        size_t i = 0;
        AstNode* init = node.first_init;
        for (; init && i < new_locals.size(); ++i, init = init->next_child)
        {
            target_reg = new_locals[i].reg;
            init->accept(*this);
        }
        target_reg = std::nullopt; // Clear target_reg after processing initializers

        C.freereg = locals_end;

        if (C.freereg < C.min_freereg)
        {
            C.freereg = C.min_freereg;
        }

        for (; i < new_locals.size(); ++i)
        {
            Reg r = new_locals[i].reg;
            emit(C, make_op_loadnil(r, 0), C.lastline);
        }
    }

    void VisitorAdapter::visit(const AstIf& node)
    {
        AutoVector<size_t> jmp_false_pcs(C.S);
        AutoVector<size_t> jmp_end_pcs(C.S);

        size_t placeholder = 0;
        compile_for_jump = true;
        jump_patch_location = &placeholder;
        node.cond->accept(*this);
        compile_for_jump = false;
        jump_patch_location = nullptr;
        jmp_false_pcs.push_back(placeholder);

        if (node.then_block)
        {
            node.then_block->accept(*this);
        }

        bool has_branches = node.first_elseif != nullptr || node.else_block != nullptr;
        if (has_branches && !last_instruction_is_terminal())
        {
            placeholder = C.current_proto->code.size();
            emit(C, make_op_jmp(0), C.lastline);
            jmp_end_pcs.push_back(placeholder);
        }

        const auto jmp_false_offset = static_cast<int32_t>(C.current_proto->code.size() - jmp_false_pcs.back() - 1);
        C.current_proto->code[jmp_false_pcs.back()] = make_op_jmp(jmp_false_offset);

        jmp_false_pcs.pop_back();

        for (ElseIf* elseif = node.first_elseif; elseif != nullptr; elseif = static_cast<ElseIf*>(elseif->next_child))
        {
            compile_for_jump = true;
            jump_patch_location = &placeholder;
            if (elseif->cond)
            {
                elseif->cond->accept(*this);
            }
            compile_for_jump = false;
            jump_patch_location = nullptr;
            jmp_false_pcs.push_back(placeholder);

            if (elseif->block)
            {
                elseif->block->accept(*this);
            }

            if (!last_instruction_is_terminal())
            {
                placeholder = C.current_proto->code.size();
                emit(C, make_op_jmp(0), C.lastline);
                jmp_end_pcs.push_back(placeholder);
            }
            C.current_proto->code[jmp_false_pcs.back()] = make_op_jmp(
                static_cast<int32_t>(C.current_proto->code.size() - jmp_false_pcs.back() - 1));
            jmp_false_pcs.pop_back();
        }

        if (node.else_block)
        {
            node.else_block->accept(*this);
        }

        int32_t end = static_cast<int>(C.current_proto->code.size());
        for (auto pc : jmp_end_pcs)
        {
            C.current_proto->code[pc] = make_op_jmp(end - static_cast<int>(pc) - 1);
        }
    }

    void VisitorAdapter::visit(const AstWhile& node)
    {
        size_t start_pc = C.current_proto->code.size();
        size_t exit_placeholder = 0;
        compile_for_jump = true;
        jump_patch_location = &exit_placeholder;
        node.cond->accept(*this);
        compile_for_jump = false;
        jump_patch_location = nullptr;

        // Push loop context for break/continue tracking
        C.loop_stack.emplace_back(C.S);

        if (node.block)
        {
            node.block->accept(*this);
        }

        // Continue jumps should jump back to start_pc (before condition)
        size_t continue_target = start_pc;
        for (size_t pos : C.loop_stack.back().continue_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(continue_target - pos - 1));
        }

        emit(C, make_op_jmp(static_cast<int32_t>(start_pc - C.current_proto->code.size() - 1)), C.lastline);
        C.current_proto->code[exit_placeholder] = make_op_jmp(
            static_cast<int32_t>(C.current_proto->code.size() - exit_placeholder - 1));

        // Break jumps should jump to after the loop (current position)
        size_t break_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().break_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(break_target - pos - 1));
        }

        // Pop loop context
        C.loop_stack.pop_back();
    }

    void VisitorAdapter::visit(const AstForNum& node)
    {
        enter_scope(C);
        Reg base = alloc_reg(C);
        Local var_loc{ node.var->view(), static_cast<int>(C.current_proto->code.size()), base, false };
        C.scopes.back().push_back(var_loc);
        Reg limit_reg = alloc_reg(C);
        Reg step_reg = alloc_reg(C);
        Reg internal_reg = alloc_reg(C);
        node.start->accept(*this);
        emit(C, make_op_move(base, static_cast<Reg>(C.freereg - 1)), C.lastline);
        free_reg(C, C.freereg - 1);
        node.end->accept(*this);
        emit(C, make_op_move(limit_reg, static_cast<Reg>(C.freereg - 1)), C.lastline);
        free_reg(C, C.freereg - 1);
        if (node.step)
        {
            node.step->accept(*this);
            emit(C, make_op_move(step_reg, static_cast<Reg>(C.freereg - 1)), C.lastline);
            free_reg(C, C.freereg - 1);
        }
        else
        {
            const auto k = add_integer_constant(C, 1);
            emit(C, make_op_loadi(step_reg, k), C.lastline);
        }
        size_t prep_pc = C.current_proto->code.size();
        emit(C, make_op_forprep(base, 0), C.lastline);

        // Push loop context for break/continue tracking
        C.loop_stack.emplace_back(C.S);

        if (node.block)
        {
            node.block->accept(*this);
        }

        // Continue jumps should jump to the FORLOOP instruction (which will be at loop_pc)
        size_t loop_pc = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().continue_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(loop_pc - pos - 1));
        }

        emit(C, make_op_forloop(base, 0), C.lastline);
        C.current_proto->code[prep_pc] = make_op_forprep(base, static_cast<int32_t>(loop_pc - (prep_pc + 1)));
        C.current_proto->code[loop_pc] = make_op_forloop(base, static_cast<int32_t>((prep_pc + 1) - loop_pc));

        // Break jumps should jump to after the loop (current position)
        size_t break_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().break_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(break_target - pos - 1));
        }

        // Pop loop context
        C.loop_stack.pop_back();

        leave_scope(C);
        free_reg(C, internal_reg);
        free_reg(C, step_reg);
        free_reg(C, limit_reg);
        free_reg(C, base);
    }

    void VisitorAdapter::visit(const AstForIn& node)
    {
        enter_scope(C);

        // Iterator protocol: for(names in expr) calls expr which should return (iter_fn, state, init_key)
        // Each iteration calls iter_fn(state, prev_key) which returns (next_key, value) or nil

        // Allocate persistent loop control registers FIRST
        Reg iter_fn_reg = alloc_reg(C); // Iterator function
        Reg state_reg = alloc_reg(C);   // State (usually the table)
        Reg key_reg = alloc_reg(C);     // Current key

        // Call the expression to get (iter_fn, state, init_key)
        // The expression is responsible for returning the iterator triple
        // We need to handle function calls specially to get 3 return values
        const auto* call_expr = node.first_expr ? node.first_expr->try_as<AstFuncCall>() : nullptr;
        if (call_expr)
        {
            // Compile the call requesting 3 results
            compile_call(*call_expr, 3);
            // Results are at C.freereg - 3, C.freereg - 2, C.freereg - 1
            Reg result_base = C.freereg - 3;
            emit(C, make_op_move(iter_fn_reg, result_base), C.lastline);
            emit(C, make_op_move(state_reg, static_cast<Reg>(result_base + 1)), C.lastline);
            emit(C, make_op_move(key_reg, static_cast<Reg>(result_base + 2)), C.lastline);
            // Free the temporary result registers
            free_reg(C, static_cast<Reg>(result_base + 2));
            free_reg(C, static_cast<Reg>(result_base + 1));
            free_reg(C, result_base);
        }
        else if (node.first_expr)
        {
            // Non-call expression - evaluate it normally
            node.first_expr->accept(*this);
            Reg expr_result = C.freereg - 1;
            // Assume it somehow provides 3 values (might be a variable holding an iterator triple?)
            // This is a fallback - most for-in loops should use pairs() or similar functions
            emit(C, make_op_move(iter_fn_reg, expr_result), C.lastline);
            emit(C, make_op_move(state_reg, static_cast<Reg>(expr_result + 1)), C.lastline);
            emit(C, make_op_move(key_reg, static_cast<Reg>(expr_result + 2)), C.lastline);
            free_reg(C, expr_result);
        }

        // Count names
        size_t name_count = 0;
        for (const AstNode* name = node.first_name; name != nullptr; name = name->next_child)
        {
            name_count++;
        }

        // Handle loop variables - either declare them inline or resolve existing ones
        struct VarInfo
        {
            int32_t local_idx;  // INVALID_LOCAL if not local
            uint32_t upval_idx; // INVALID_UPVALUE if not upvalue
        };

        AutoVector<VarInfo> var_info(C.S, name_count);

        if (node.declares_variables)
        {
            // Variables declared inline with 'let' - create new locals
            for (const AstNode* name = node.first_name; name != nullptr; name = name->next_child)
            {
                auto* name_str = static_cast<const AstString*>(name);
                Reg var_reg = alloc_reg(C);
                Local var_loc{ name_str->view(), static_cast<int>(C.current_proto->code.size()), var_reg, false };
                C.scopes.back().push_back(var_loc);
                var_info.push_back({ static_cast<int>(var_reg), kInvalidUpvalue });
            }
        }
        else
        {
            // Variables must already exist (local or upvalue)
            for (const AstNode* name = node.first_name; name != nullptr; name = name->next_child)
            {
                auto* name_str = static_cast<const AstString*>(name);
                int32_t loc = resolve_local(C, name_str->view());
                uint32_t up = kInvalidUpvalue;

                if (loc < 0)
                {
                    // Not found as local, try upvalue
                    up = resolve_upvalue(C, name_str->view());
                    if (up == kInvalidUpvalue)
                    {
                        // Variable not found
                        throw ReferenceError(
                            behl::format(
                                "For-in loop variable '{}' is not declared. Variables must be declared before use in for-in "
                                "loops.",
                                name_str->view()),
                            get_location(C));
                    }
                }

                var_info.push_back({ loc, up });
            }
        }

        // Protect loop control registers from being reused
        if (C.freereg > C.min_freereg)
        {
            C.min_freereg = C.freereg;
        }

        size_t loop_start = C.current_proto->code.size();

        // Push loop context for break/continue tracking
        C.loop_stack.emplace_back(C.S);

        // Save freereg before calling iterator
        Reg loop_body_freereg = C.freereg;

        // Call iter_fn(state, key) to get (next_key, value1, value2, ...)
        // Number of results = number of loop variables (key gets first result, rest get remaining results)
        // Push arguments for the call
        Reg call_base = alloc_reg(C);
        emit(C, make_op_move(call_base, iter_fn_reg), C.lastline);
        Reg arg1 = alloc_reg(C);
        emit(C, make_op_move(arg1, state_reg), C.lastline);
        Reg arg2 = alloc_reg(C);
        emit(C, make_op_move(arg2, key_reg), C.lastline);

        // Request as many results as we have loop variables (minimum 1 for next_key)
        uint8_t num_results = static_cast<uint8_t>(std::max<size_t>(1, name_count));

        // Call with 2 args, expecting num_results results, not a self call
        // num_args includes the function, so 2 args = 3 total (func + arg1 + arg2)
        emit(C, make_op_call(call_base, 3, num_results, false), C.lastline);

        // Results are now at call_base, call_base+1, call_base+2, ...
        // First result is always next_key (used for iteration control)
        Reg next_key_reg = call_base;

        // Check if next_key is nil (end of iteration)
        // OP_TEST with invert=true: skip next instruction if truthy, execute if falsy
        // So TEST(true) + JMP means: if nil, execute JMP to exit
        size_t exit_jmp_placeholder = 0;
        emit(C, make_op_test(next_key_reg, true), C.lastline); // invert=true: skip JMP if truthy
        exit_jmp_placeholder = C.current_proto->code.size();
        emit(C, make_op_jmp(0), C.lastline); // Jump to exit if nil

        // Update key_reg with next_key for next iteration
        emit(C, make_op_move(key_reg, next_key_reg), C.lastline);

        // Assign values to loop variables
        // Single variable: gets second return value (first is key for iteration control)
        // Multiple variables: first gets next_key, rest get remaining return values
        size_t var_idx = 0;
        for (const AstNode* name = node.first_name; name != nullptr; name = name->next_child, ++var_idx)
        {
            Reg source_reg;
            if (name_count == 1)
            {
                // Single variable gets value (second result)
                source_reg = static_cast<Reg>(call_base + 1);
            }
            else
            {
                // Multiple variables: first gets key, rest get subsequent values
                source_reg = static_cast<Reg>(call_base + var_idx);
            }

            if (var_info[var_idx].local_idx >= 0)
            {
                emit(C, make_op_move(static_cast<Reg>(var_info[var_idx].local_idx), source_reg), C.lastline);
            }
            else if (var_info[var_idx].upval_idx != kInvalidUpvalue)
            {
                emit(C, make_op_setupval(source_reg, static_cast<uint8_t>(var_info[var_idx].upval_idx)), C.lastline);
            }
        }

        // Free call result registers
        C.freereg = loop_body_freereg;

        // Execute loop body
        if (node.block)
        {
            node.block->accept(*this);
        }

        // Restore freereg
        C.freereg = loop_body_freereg;

        // Continue jumps should jump back to loop start
        for (size_t pos : C.loop_stack.back().continue_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(loop_start - pos - 1));
        }

        // Jump back to loop start
        emit(C, make_op_jmp(static_cast<int32_t>(loop_start - C.current_proto->code.size() - 1)), C.lastline);

        // Patch exit jump
        C.current_proto->code[exit_jmp_placeholder] = make_op_jmp(
            static_cast<int32_t>(C.current_proto->code.size() - exit_jmp_placeholder - 1));

        // Break jumps should jump to after the loop (current position)
        size_t break_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().break_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(break_target - pos - 1));
        }

        // Pop loop context
        C.loop_stack.pop_back();

        leave_scope(C);
    }

    void VisitorAdapter::visit(const AstForC& node)
    {
        enter_scope(C);

        if (node.init)
        {
            node.init->accept(*this);
        }

        size_t loop_start = C.current_proto->code.size();

        size_t exit_jmp = 0;
        if (node.condition)
        {
            compile_for_jump = true;
            jump_patch_location = &exit_jmp;
            node.condition->accept(*this);
            compile_for_jump = false;
            jump_patch_location = nullptr;
        }

        // Push loop context for break/continue tracking
        C.loop_stack.emplace_back(C.S);

        if (node.block)
        {
            node.block->accept(*this);
        }

        // Continue jumps should jump to the update statement (or loop start if no update)
        size_t continue_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().continue_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(continue_target - pos - 1));
        }

        if (node.update)
        {
            Reg before_freereg = C.freereg;
            node.update->accept(*this);

            if (node.update->type != AstNodeType::kAssign && C.freereg > before_freereg)
            {
                C.freereg = before_freereg;
            }
        }

        emit(C, make_op_jmp(static_cast<int32_t>(loop_start - C.current_proto->code.size() - 1)), C.lastline);

        if (node.condition)
        {
            C.current_proto->code[exit_jmp] = make_op_jmp(static_cast<int32_t>(C.current_proto->code.size() - exit_jmp - 1));
        }

        // Break jumps should jump to after the loop (current position)
        size_t break_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().break_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(break_target - pos - 1));
        }

        // Pop loop context
        C.loop_stack.pop_back();

        leave_scope(C);
    }

    void VisitorAdapter::visit(const AstForCNumeric& node)
    {
        // Compile optimized numeric C-style for loop using FORPREP/FORLOOP
        // This is similar to ForNum but handles C-style comparison operators and step directions

        enter_scope(C);
        Reg base = alloc_reg(C);
        Local var_loc{ node.var->view(), static_cast<int>(C.current_proto->code.size()), base, false };
        C.scopes.back().push_back(var_loc);
        Reg limit_reg = alloc_reg(C);
        Reg step_reg = alloc_reg(C);
        Reg internal_reg = alloc_reg(C);

        // Compile start value
        node.start->accept(*this);
        emit(C, make_op_move(base, static_cast<Reg>(C.freereg - 1)), C.lastline);
        free_reg(C, C.freereg - 1);

        // Compile end value
        // For non-inclusive comparisons (<, >), adjust the limit by 1
        if (node.inclusive)
        {
            // For <= or >=, use the end value directly
            node.end->accept(*this);
            emit(C, make_op_move(limit_reg, static_cast<Reg>(C.freereg - 1)), C.lastline);
            free_reg(C, C.freereg - 1);
        }
        else
        {
            // For < or >, adjust the limit by +/-1 to make it inclusive for FORLOOP
            node.end->accept(*this);
            Reg end_reg = C.freereg - 1;
            if (node.ascending)
            {
                // For i < end, convert to i <= (end - 1)
                emit(C, make_op_subimm(limit_reg, end_reg, 1), C.lastline);
            }
            else
            {
                // For i > end, convert to i >= (end + 1)
                emit(C, make_op_addimm(limit_reg, end_reg, 1), C.lastline);
            }
            free_reg(C, end_reg);
        }

        // Compile step value
        if (node.step)
        {
            node.step->accept(*this);
            Reg step_value_reg = C.freereg - 1;

            // If descending, negate the step value
            if (!node.ascending)
            {
                emit(C, make_op_unm(step_reg, step_value_reg), C.lastline);
            }
            else
            {
                emit(C, make_op_move(step_reg, step_value_reg), C.lastline);
            }
            free_reg(C, step_value_reg);
        }
        else
        {
            // Implicit step: 1 for ascending, -1 for descending
            const auto k = add_integer_constant(C, node.ascending ? 1 : -1);
            emit(C, make_op_loadi(step_reg, k), C.lastline);
        }

        size_t prep_pc = C.current_proto->code.size();
        emit(C, make_op_forprep(base, 0), C.lastline);

        // Push loop context for break/continue tracking
        C.loop_stack.emplace_back(C.S);

        if (node.block)
        {
            node.block->accept(*this);
        }

        // Continue jumps should jump to the FORLOOP instruction
        size_t loop_pc = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().continue_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(loop_pc - pos - 1));
        }

        emit(C, make_op_forloop(base, 0), C.lastline);
        C.current_proto->code[prep_pc] = make_op_forprep(base, static_cast<int32_t>(loop_pc - (prep_pc + 1)));
        C.current_proto->code[loop_pc] = make_op_forloop(base, static_cast<int32_t>((prep_pc + 1) - loop_pc));

        // Break jumps should jump to after the loop
        size_t break_target = C.current_proto->code.size();
        for (size_t pos : C.loop_stack.back().break_list)
        {
            C.current_proto->code[pos] = make_op_jmp(static_cast<int32_t>(break_target - pos - 1));
        }

        // Pop loop context
        C.loop_stack.pop_back();

        leave_scope(C);
        free_reg(C, internal_reg);
        free_reg(C, step_reg);
        free_reg(C, limit_reg);
        free_reg(C, base);
    }

    void VisitorAdapter::visit(const AstFuncDefStat& node)
    {
        // Count name parts and check if simple
        size_t name_part_count = 0;
        const AstNode* last_name_part = nullptr;
        for (const AstNode* part = node.first_name_part; part != nullptr; part = part->next_child)
        {
            name_part_count++;
            last_name_part = part;
        }
        bool is_simple_name = (name_part_count == 1);

        // In module mode, all non-local functions at module level are treated as locals
        bool treat_as_local = node.is_local || (C.is_module && is_simple_name && C.scopes.size() <= 2);

        if (treat_as_local)
        {
            if (!is_simple_name)
            {
                throw SyntaxError("local function must use a simple name", get_location(C));
            }
            auto* first_name = static_cast<const AstString*>(node.first_name_part);
            Local loc;
            loc.name = first_name->view();
            loc.is_const = false;
            loc.reg = alloc_reg(C);
            loc.start_pc = static_cast<int>(C.current_proto->code.size());
            // In module mode, add to first (module) scope so it survives
            if (C.is_module && !node.is_local)
            {
                C.scopes.front().push_back(loc);
            }
            else
            {
                C.scopes.back().push_back(loc);
            }
        }

        AstFuncDef func_expr;
        func_expr.first_param = node.first_param;
        func_expr.is_vararg = node.is_vararg;
        func_expr.is_method = node.is_method;

        if (is_simple_name)
        {
            auto* first_name = static_cast<const AstString*>(node.first_name_part);
            func_expr.self_name = first_name->view();
        }

        func_expr.block = node.block;
        func_expr.accept(*this);
        Reg func_reg = C.freereg - 1;

        if (treat_as_local)
        {
            auto* first_name = static_cast<const AstString*>(node.first_name_part);
            int32_t loc = resolve_local(C, first_name->view());
            if (loc >= 0)
            {
                emit(C, make_op_move(static_cast<uint8_t>(loc), func_reg), C.lastline);
                free_reg(C, func_reg);
                return;
            }
            throw ReferenceError(behl::format("Local function '{}' not found", first_name->view()), get_location(C));
        }

        uint8_t dest_reg = alloc_reg(C);
        if (name_part_count == 1)
        {
            auto* first_name = static_cast<const AstString*>(node.first_name_part);
            const auto k = add_string_constant(C, first_name->view());
            emit(C, make_op_setglobal(func_reg, k), C.lastline);
        }
        else
        {
            auto* base_name_node = static_cast<const AstString*>(node.first_name_part);
            const auto base_name = base_name_node->view();
            int32_t loc = resolve_local(C, base_name);
            if (loc >= 0)
            {
                emit(C, make_op_move(dest_reg, static_cast<uint8_t>(loc)), C.lastline);
            }
            else
            {
                uint32_t up = resolve_upvalue(C, base_name);
                if (up != kInvalidUpvalue)
                {
                    emit(C, make_op_getupval(dest_reg, static_cast<uint8_t>(up)), C.lastline);
                }
                else
                {
                    const auto k = add_string_constant(C, base_name);
                    emit(C, make_op_getglobal(dest_reg, k), C.lastline);
                }
            }

            // Navigate through intermediate parts
            const AstNode* part = node.first_name_part->next_child;
            for (size_t i = 1; i < name_part_count - 1; ++i, part = part->next_child)
            {
                auto* part_name = static_cast<const AstString*>(part);
                Reg next_reg = alloc_reg(C);
                Reg key_reg = alloc_reg(C);
                const auto k = add_string_constant(C, part_name->view());
                emit(C, make_op_loads(key_reg, k), C.lastline);
                emit(C, make_op_getfield(next_reg, dest_reg, key_reg), C.lastline);
                free_reg(C, key_reg);
                free_reg(C, dest_reg);
                dest_reg = next_reg;
            }

            // Set the final field
            auto* last_name = static_cast<const AstString*>(last_name_part);
            Reg key_reg = alloc_reg(C);
            const auto k = add_string_constant(C, last_name->view());
            emit(C, make_op_loads(key_reg, k), C.lastline);
            emit(C, make_op_setfield(dest_reg, key_reg, func_reg), C.lastline);
            free_reg(C, key_reg);
        }

        free_reg(C, dest_reg);
        free_reg(C, func_reg);
    }

    void VisitorAdapter::visit(const AstReturn& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        // Emit all defer statements before returning (scope level 1 = function scope)
        emit_defers_for_scope(C, *this, 1);

        // Check if single expr that is a function call
        if (node.first_expr && !node.first_expr->next_child && node.first_expr->type == AstNodeType::kFuncCall)
        {
            const auto& call_node = static_cast<const AstFuncCall&>(*node.first_expr);

            // Check if this is a tail call to tostring/tonumber builtin - if so, use compile_call optimization
            if (const auto* ident = call_node.func->try_as<AstIdent>())
            {
                const bool is_builtin = (ident->name->view() == "tostring" || ident->name->view() == "tonumber");
                // Check exactly one arg: first_arg != nullptr && first_arg->next_child == nullptr
                if (is_builtin && call_node.first_arg && !call_node.first_arg->next_child && !call_node.is_self_call)
                {
                    // Use the standard compile_call which will optimize tostring
                    compile_call(call_node, 0);
                    Reg result_reg = C.freereg - 1;
                    emit(C, make_op_return(result_reg, 1), C.lastline);
                    return;
                }
            }

            // Save call site location for tail call error reporting
            int32_t call_line = call_node.line;
            int32_t call_column = call_node.column;

            if (C.freereg < C.min_freereg)
            {
                C.freereg = C.min_freereg;
            }

            call_node.func->accept(*this);
            Reg func_reg = C.freereg - 1;

            AutoVector<uint8_t> arg_regs(C.S);

            size_t arg_idx = 0;
            size_t total_args = 0;
            [[maybe_unused]] AstNode* last_arg_node = nullptr;

            // Count args
            for (AstNode* arg = call_node.first_arg; arg; arg = arg->next_child)
            {
                total_args++;
                last_arg_node = arg;
            }

            for (AstNode* arg = call_node.first_arg; arg; arg = arg->next_child, ++arg_idx)
            {
                const bool is_last = (arg_idx == total_args - 1);
                const auto* arg_call = arg->try_as<AstFuncCall>();
                const auto* arg_vararg = arg->try_as<AstVararg>();

                if (is_last && arg_call)
                {
                    compile_call(*arg_call, static_cast<uint8_t>(kMultRet));
                    uint8_t arg_result = C.freereg - 1;

                    uint8_t dest_reg = static_cast<uint8_t>(func_reg + 1 + arg_regs.size());
                    if (arg_result != dest_reg)
                    {
                        if (dest_reg >= C.freereg)
                        {
                            C.freereg = dest_reg + 1;
                        }
                        emit(C, make_op_move(dest_reg, arg_result), C.lastline);
                    }

                    emit(C, make_op_tailcall(func_reg, static_cast<uint8_t>(kMultArgs), call_node.is_self_call), call_line,
                        call_column);

                    C.freereg = 0;

                    return;
                }
                else if (is_last && arg_vararg)
                {
                    // Emit VARARG to push varargs onto stack
                    Reg vararg_dest = static_cast<Reg>(func_reg + 1 + arg_regs.size());
                    emit(C, make_op_vararg(vararg_dest, 0), C.lastline);

                    // Tailcall with kMultArgs (variable args from stack)
                    emit(C, make_op_tailcall(func_reg, static_cast<uint8_t>(kMultArgs), call_node.is_self_call), call_line,
                        call_column);

                    C.freereg = 0;

                    return;
                }
                else
                {
                    arg->accept(*this);
                    uint8_t arg_result = C.freereg - 1;

                    Reg dest_reg = static_cast<Reg>(func_reg + 1 + arg_regs.size());

                    if (arg_result != dest_reg)
                    {
                        if (dest_reg >= C.freereg)
                        {
                            C.freereg = dest_reg + 1;
                        }
                        emit(C, make_op_move(dest_reg, arg_result), C.lastline);
                    }

                    arg_regs.push_back(dest_reg);
                }
            }

            uint8_t num_args = static_cast<uint8_t>(arg_regs.size() + 1);

            emit(C, make_op_tailcall(func_reg, num_args, call_node.is_self_call), call_line, call_column);
            C.freereg = 0;

            return;
        }

        if (!node.first_expr)
        {
            // Explicit empty return: return 0 values
            emit(C, make_op_return(0, 0), C.lastline);
        }
        else if (!node.first_expr->next_child)
        {
            // Single return expression
            const auto* call_expr = node.first_expr->try_as<AstFuncCall>();
            if (call_expr)
            {
                compile_call(*call_expr, static_cast<uint8_t>(kMultRet));
                Reg call_reg = C.freereg - 1;
                emit(C, make_op_return(call_reg, static_cast<uint8_t>(kMultRet)), C.lastline);
            }
            else
            {
                if (const auto* ident = node.first_expr->try_as<AstIdent>())
                {
                    int32_t loc = resolve_local(C, ident->name->view());
                    if (loc >= 0)
                    {
                        emit(C, make_op_return(static_cast<uint8_t>(loc), 1), C.lastline);
                        return;
                    }
                    uint32_t up = resolve_upvalue(C, ident->name->view());
                    if (up != kInvalidUpvalue)
                    {
                        Reg reg = alloc_reg(C);
                        emit(C, make_op_getupval(reg, static_cast<uint8_t>(up)), C.lastline);
                        emit(C, make_op_return(reg, 1), C.lastline);
                        return;
                    }
                }

                node.first_expr->accept(*this);
                Reg result_reg = C.freereg - 1;
                emit(C, make_op_return(result_reg, 1), C.lastline);
            }
        }
        else
        {
            // Multiple return expressions - find last one and check if it's a call
            AstNode* last_expr = nullptr;
            for (AstNode* expr = node.first_expr; expr; expr = expr->next_child)
            {
                last_expr = expr;
            }
            const auto* last_call = last_expr ? last_expr->try_as<AstFuncCall>() : nullptr;

            AutoVector<Reg> result_regs(C.S);

            // Compile all but last expression
            for (AstNode* expr = node.first_expr; expr && expr != last_expr; expr = expr->next_child)
            {
                expr->accept(*this);
                result_regs.push_back(C.freereg - 1);
            }

            if (last_call)
            {
                compile_call(*last_call, static_cast<uint8_t>(kMultRet));
                result_regs.push_back(C.freereg - 1);

                Reg first_return_reg = result_regs.front();
                for (size_t i = 1; i < result_regs.size() - 1; i++)
                {
                    Reg expected_reg = first_return_reg + static_cast<Reg>(i);
                    if (result_regs[i] != expected_reg)
                    {
                        emit(C, make_op_move(expected_reg, result_regs[i]), C.lastline);
                    }
                }

                uint8_t call_pos = first_return_reg + static_cast<uint8_t>(result_regs.size() - 1);
                if (result_regs.back() != call_pos)
                {
                    emit(C, make_op_move(call_pos, result_regs.back()), C.lastline);
                }

                emit(C, make_op_return(first_return_reg, static_cast<uint8_t>(kMultRet)), C.lastline);
            }
            else
            {
                if (last_expr)
                {
                    last_expr->accept(*this);
                    result_regs.push_back(C.freereg - 1);
                }

                uint8_t first_return_reg = result_regs.front();
                // Count total exprs
                size_t expr_count = 0;
                for (AstNode* expr = node.first_expr; expr; expr = expr->next_child)
                {
                    expr_count++;
                }
                for (size_t i = 1; i < result_regs.size(); i++)
                {
                    Reg expected_reg = first_return_reg + static_cast<uint8_t>(i);
                    if (result_regs[i] != expected_reg)
                    {
                        emit(C, make_op_move(expected_reg, result_regs[i]), C.lastline);
                    }
                }

                emit(C, make_op_return(first_return_reg, static_cast<uint8_t>(expr_count)), C.lastline);
            }
        }
        C.freereg = 0;
    }

    void VisitorAdapter::visit(const AstBreak&)
    {
        if (C.loop_stack.empty())
        {
            throw SemanticError("break statement outside of loop", get_location(C));
        }
        size_t jmp_pos = C.current_proto->code.size();
        emit(C, make_op_jmp(0), C.lastline);
        C.loop_stack.back().break_list.push_back(jmp_pos);
    }

    void VisitorAdapter::visit(const AstContinue&)
    {
        if (C.loop_stack.empty())
        {
            throw SemanticError("continue statement outside of loop", get_location(C));
        }
        size_t jmp_pos = C.current_proto->code.size();
        emit(C, make_op_jmp(0), C.lastline);
        C.loop_stack.back().continue_list.push_back(jmp_pos);
    }

    void VisitorAdapter::visit(const AstDefer& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        // Store the defer statement in the defer stack
        // It will be executed in LIFO order when scope exits
        int32_t scope_level = current_scope_level(C);
        C.defer_stack.emplace_back(node.body, scope_level);
    }

    void VisitorAdapter::visit(const AstScope& node)
    {
        enter_scope(C);
        int32_t scope_level = current_scope_level(C);
        if (node.block)
        {
            node.block->accept(*this);
        }
        // Emit defers for this scope before leaving
        emit_defers_for_scope(C, *this, scope_level);
        leave_scope(C);
    }

    void VisitorAdapter::visit(const AstExprStat& node)
    {
        C.lastline = node.line;
        C.lastcolumn = node.column;

        node.expr->accept(*this);

        C.freereg = C.min_freereg;
    }

    void VisitorAdapter::visit(const AstBlock& node)
    {
        enter_scope(C);
        int32_t scope_level = current_scope_level(C);
        for (const AstNode* stat = node.first_stat; stat != nullptr; stat = stat->next_child)
        {
            stat->accept(*this);
        }
        // Emit defers for this scope before leaving
        emit_defers_for_scope(C, *this, scope_level);
        leave_scope(C);
    }

    void VisitorAdapter::visit(const AstProgram& node)
    {
        if (node.block)
        {
            node.block->accept(*this);
        }
    }

    void VisitorAdapter::visit(const AstModuleDecl&)
    {
        // Module declaration is handled during parsing/semantics
    }

    void VisitorAdapter::visit(const AstExportDecl& node)
    {
        // Just compile the inner declaration (function or const)
        node.declaration->accept(*this);
    }

    void VisitorAdapter::visit(const AstExportList&)
    {
        // Export list will be handled by export transform pass
    }

    // Emit all defer statements from the current scope level and above (in LIFO order)
    static void emit_defers_for_scope(CompilerState& C, VisitorAdapter& visitor, int32_t target_scope_level)
    {
        // Save the current freereg to restore later
        uint8_t saved_freereg = C.freereg;

        // Iterate backwards through defer stack (LIFO)
        for (auto it = C.defer_stack.rbegin(); it != C.defer_stack.rend();)
        {
            if (it->scope_level >= target_scope_level)
            {
                // Execute this defer statement
                if (it->defer_body)
                {
                    it->defer_body->accept(visitor);
                }

                // Remove from the defer stack (we've executed it)
                // Convert reverse iterator to forward iterator for erase
                auto forward_it = std::next(it).base();
                C.defer_stack.erase(forward_it);

                // After erase, rbegin() gives us a new iterator
                it = C.defer_stack.rbegin();
            }
            else
            {
                ++it;
            }
        }

        // Restore freereg
        C.freereg = saved_freereg;
    }

    GCProto* compile(State* state, const AstProgram* program, std::string_view source_name)
    {
        GCPauseGuard gc_pause(state);

        auto* proto = gc_new_proto(state);

        CompilerState C(state, nullptr, proto);

        C.current_proto->source_name = gc_new_string(state, source_name);
        C.current_proto->source_path = gc_new_string(state, source_name);
        C.current_proto->name = gc_new_string(state, "<main chunk>");
        C.is_module = program->is_module;
        C.parent = nullptr;
        C.freereg = 1;
        C.min_freereg = 1;

        enter_scope(C);
        VisitorAdapter V(C);
        program->accept(V);
        leave_scope(C);

        // Return nothing (0 values) - export transform pass will add explicit return if module
        emit(C, make_op_return(0, 0), 0, 0);

        return proto;
    }

} // namespace behl
