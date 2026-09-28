#include "format.hpp"

namespace behl
{
    std::string vformat(std::string_view fmt, const std::vector<format_arg>& args)
    {
        std::string result;
        result.reserve(fmt.size());
        vformat_to(result, fmt, args.size(),
            [&args](std::string& out, size_t index, const format_spec& spec) { format_value_to(out, args[index], spec); });
        return result;
    }

} // namespace behl
