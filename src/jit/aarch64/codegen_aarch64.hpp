#pragma once

#include "jit/jit.hpp"

#if BEHL_JIT_AARCH64

#    include "jit/aarch64/emitter_aarch64.hpp"
#    include "jit/jit_compiler.hpp"

#    include <cstdint>
#    include <vector>

namespace behl
{
    class CodegenAArch64
    {
    public:
        CodegenAArch64() = default;

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
        void emit_cmp_imm(A64Reg reg, int64_t imm);
        void emit_add_imm(A64Reg reg, int64_t imm);
        void alloc_i64(uint32_t var);
        void alloc_f64(uint32_t var);
        void alloc_result(uint32_t var);
        void release_var(uint32_t var);
        A64Reg gp(uint32_t var) const;
        A64Vec fp(uint32_t var) const;
        A64Label label(uint32_t id) const noexcept;

        A64Emitter e_;
        std::vector<uint32_t> last_pos_;
        std::vector<uint8_t> var_reg_;
        std::vector<bool> var_f64_;
        uint32_t gp_used_{};
        uint32_t fp_used_{};
        bool base_valid_{};
        bool failed_{};
    };

} // namespace behl

#endif
