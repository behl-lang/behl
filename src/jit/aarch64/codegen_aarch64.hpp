#pragma once

#include "jit/jit.hpp"

#if BEHL_JIT_AARCH64

#    include "jit/aarch64/emitter_aarch64.hpp"
#    include "jit/jit_compiler.hpp"

#    include "common/vector.hpp"

#    include <cstdint>
#    include <utility>

namespace behl
{
    class CodegenAArch64
    {
    public:
        explicit CodegenAArch64(State* state)
            : e_(state)
            , last_pos_(state)
            , var_reg_(state)
            , var_f64_(state)
            , slots_(state)
            , label_states_(state)
            , guard_slots_(state)
            , state_(state)
        {
        }

        JitEntry generate(State* S, const CgProgram& program);

    private:
        struct SlotState
        {
            uint8_t reg;
            bool is_f64;
            uint8_t tag;
        };

        struct LabelState
        {
            bool recorded;
            AutoVector<std::pair<int32_t, SlotState>> entries;

            explicit LabelState(State* state)
                : recorded(false)
                , entries(state)
            {
            }
        };

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

        void cache_reset();
        void cache_drop_all();
        void cache_drop_slot(int32_t slot);
        void discard_payload(int32_t slot);
        uint32_t next_slot_use(int32_t slot, uint32_t from) const;
        int32_t pick_victim(bool want_f64) const;
        bool evict_one(bool want_f64);
        void record_label_state(uint32_t label);
        void restore_label_state(uint32_t label);
        void take_ownership(int32_t slot, uint32_t var, bool is_f64);
        bool slot_in_reg(int32_t slot, bool want_f64) const;

        A64Reg gp(uint32_t var) const;
        A64Vec fp(uint32_t var) const;
        A64Label label(uint32_t id) const noexcept;

        A64Emitter e_;
        AutoVector<uint32_t> last_pos_;
        AutoVector<uint8_t> var_reg_;
        AutoVector<bool> var_f64_;
        AutoVector<SlotState> slots_;
        AutoVector<LabelState> label_states_;
        AutoVector<AutoVector<int32_t>> guard_slots_;
        State* state_{};
        const CgProgram* program_{};
        uint32_t cur_index_{};
        bool cache_enabled_{};
        uint32_t gp_used_{};
        uint32_t fp_used_{};
        bool base_valid_{};
        bool failed_{};
    };

} // namespace behl

#endif
