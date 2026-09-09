#pragma once

#include "common/vector.hpp"
#include "gc_object.hpp"
#include "gco_string.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace behl
{

    struct State;

    inline constexpr uint32_t kMaxDeferBlocks = 32;

    struct DeferBlock
    {
        uint32_t entry_pc;
        Reg link_reg;
    };

    struct GCProto : GCObject
    {
        static constexpr auto kObjectType = GCType::kProto;

        mutable uint32_t (*jit_code)(State*){};
        mutable bool jit_declined{};

        Vector<Instruction> code;
        Vector<Value> str_constants;
        Vector<Value> int_constants;
        Vector<Value> fp_constants;
        Vector<GCProto*> protos;
        Vector<GCString*> upvalue_names;
        Vector<int> line_info;
        Vector<int> column_info;
        Vector<DeferBlock> defer_blocks;
        uint32_t defer_unwind_pc{};
        GCString* source_name;
        GCString* source_path; // Absolute path to source file for module resolution
        GCString* name;        // Function name for debugging
        uint32_t num_params{};
        uint32_t max_stack_size{};
        bool is_vararg{};
        bool has_upvalues{}; // True if function or any nested function uses upvalues
    };

} // namespace behl
