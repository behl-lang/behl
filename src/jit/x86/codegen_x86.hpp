#pragma once

#include "jit/jit.hpp"

#if BEHL_JIT_X86

#    include "jit/jit_compiler.hpp"
#    include "jit/x86/emitter_x86.hpp"

#    include <cstdint>
#    include <vector>

namespace behl
{
    class CodegenX86
    {
    public:
        CodegenX86() = default;

        JitEntry generate(State* S, const CgProgram& program);

    private:
        void compute_liveness(const CgProgram& program);
        void lower(const CgOp& op, uint32_t index);
        void release_dead(const CgOp& op, uint32_t index);
        void ensure_base();
        void emit_base_refresh();
        void emit_prologue();
        void emit_epilogue(uint32_t result_code);
        void emit_helper_call(const CgOp& op);
        void emit_branch_i64_imm(const CgOp& op);
        void emit_const_f64(const CgOp& op);
        void alloc_i64(uint32_t var);
        void alloc_f64(uint32_t var);
        void alloc_result(uint32_t var);
        uint8_t alloc_gp_slot();
        void release_var(uint32_t var);
        GpReg gp(uint32_t var) const;
        GpReg gp_hi(uint32_t var) const;
        XmmReg xmm(uint32_t var) const;
        Label label(uint32_t id) const noexcept;

        X86Emitter e_;
        std::vector<uint32_t> last_pos_;
        std::vector<bool> loop_header_;
        std::vector<uint8_t> var_reg_;
        std::vector<uint8_t> var_reg2_;
        std::vector<bool> var_f64_;
        uint32_t gp_used_{};
        uint32_t xmm_used_{};
        bool base_valid_{};
        bool failed_{};
    };

} // namespace behl

#endif
