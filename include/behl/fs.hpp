#pragma once

#include "behl.hpp"

namespace behl
{
    /**
     * Load the filesystem library into the behl state.
     * Registers the 'fs' module with file and directory operations.
     *
     * @param S The behl state
     *
     * Usage:
     *   behl::load_fslib(S);
     *   behl::get_global(S, "fs");
     *   // fs module is now available
     */
    void load_fslib(State* S);

} // namespace behl
