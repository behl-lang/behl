#include "common/print.hpp"

#include "state.hpp"

namespace behl
{

    namespace detail
    {
        void print(State* S, std::string_view msg)
        {
            if (S->print_handler == nullptr)
            {
                return;
            }

            S->print_handler(S, msg);
        }
    } // namespace detail

} // namespace behl
