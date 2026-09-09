#pragma once

#include "jit/jit.hpp"

#if BEHL_JIT_X86

#    include "jit/jit_compiler.hpp"
#    include "jit/x86/emitter_x86.hpp"

#    include <cstdint>
#    include <utility>
#    include <vector>

namespace behl
{
    class CodegenX86
    {
    public:
        CodegenX86() = default;

        JitEntry generate(State* S, const CgProgram& program);

    private:
        struct SlotState
        {
            uint8_t reg;
            bool is_f64;
            bool dirty;
            uint8_t tag;
            bool tag_dirty;
        };

        struct LabelState
        {
            bool recorded;
            std::vector<std::pair<int32_t, SlotState>> entries;
        };

        void compute_liveness(const CgProgram& program);
        void lower(const CgOp& op, uint32_t index);
        void release_dead(const CgOp& op, uint32_t index);

        void cache_reset();
        void cache_flush_dirty();
        void cache_drop_all();
        void cache_drop_slot(int32_t slot);
        void discard_payload(int32_t slot);
        void cache_forget_tags();
        void flush_slot(int32_t slot);
        uint32_t next_slot_use(int32_t slot, uint32_t from) const;
        int32_t pick_victim(bool want_f64) const;
        bool evict_one(bool want_f64);
        void record_label_state(uint32_t label);
        void restore_label_state(uint32_t label);
        void take_ownership(int32_t slot, uint32_t var, bool is_f64);
        bool slot_in_reg(int32_t slot, bool want_f64) const;
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
        std::vector<SlotState> slots_;
        std::vector<LabelState> label_states_;
        std::vector<std::vector<int32_t>> guard_slots_;
        const CgProgram* program_{};
        uint32_t cur_index_{};
        bool cache_enabled_{};
        uint32_t gp_used_{};
        uint32_t xmm_used_{};
        bool base_valid_{};
        bool failed_{};
    };

} // namespace behl

#endif
