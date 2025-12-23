#pragma once

#include "common/vector.hpp"
#include "gc/gco_string.hpp"
#include "vm/bytecode.hpp"
#include "vm/value.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace behl
{
    struct State;
    struct CallFrame;
    struct GCProto;

    void dump_proto(const GCProto& proto, int indent = 0);

} // namespace behl
