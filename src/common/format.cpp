#include "format.hpp"

namespace behl
{
    std::string vformat(std::string_view fmt, const std::vector<format_arg>& args)
    {
        std::string result;
        size_t i = 0;
        size_t arg_index = 0;

        while (i < fmt.size())
        {
            if (fmt[i] == '{')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '{')
                {
                    result += '{';
                    i += 2;
                    continue;
                }

                size_t spec_start = i + 1;
                size_t brace_pos = fmt.find('}', spec_start);

                if (brace_pos == std::string_view::npos)
                {
                    throw std::runtime_error("unmatched '{' in format string");
                }

                std::string_view spec_str = fmt.substr(spec_start, brace_pos - spec_start);

                // Parse optional argument index (e.g., "0" in "{0:.2f}")
                size_t actual_arg_index = arg_index;
                size_t colon_pos = spec_str.find(':');

                if (colon_pos != std::string_view::npos || !spec_str.empty())
                {
                    std::string_view index_part = (colon_pos != std::string_view::npos) ? spec_str.substr(0, colon_pos)
                                                                                        : spec_str;

                    // Check if it's a numeric index
                    if (!index_part.empty() && index_part[0] >= '0' && index_part[0] <= '9')
                    {
                        size_t parsed_index = 0;
                        for (char c : index_part)
                        {
                            if (c >= '0' && c <= '9')
                            {
                                parsed_index = parsed_index * 10 + static_cast<size_t>(c - '0');
                            }
                            else
                            {
                                break;
                            }
                        }
                        actual_arg_index = parsed_index;

                        // Extract format spec after colon (if any)
                        if (colon_pos != std::string_view::npos && colon_pos + 1 < spec_str.size())
                        {
                            spec_str = spec_str.substr(colon_pos + 1);
                        }
                        else
                        {
                            spec_str = std::string_view();
                        }
                    }
                    else if (colon_pos != std::string_view::npos)
                    {
                        // No index, just spec after colon (e.g., "{:.2f}")
                        spec_str = spec_str.substr(colon_pos + 1);
                        actual_arg_index = arg_index++;
                    }
                    else
                    {
                        // No colon, no numeric index - auto-increment
                        actual_arg_index = arg_index++;
                        spec_str = std::string_view();
                    }
                }
                else
                {
                    // Empty spec - auto-increment
                    actual_arg_index = arg_index++;
                }

                format_spec spec = parse_format_spec(spec_str);

                if (actual_arg_index >= args.size())
                {
                    throw std::runtime_error("not enough arguments for format string");
                }

                result += format_value(args[actual_arg_index], spec);

                i = brace_pos + 1;
            }
            else if (fmt[i] == '}')
            {
                if (i + 1 < fmt.size() && fmt[i + 1] == '}')
                {
                    result += '}';
                    i += 2;
                    continue;
                }
                throw std::runtime_error("unmatched '}' in format string");
            }
            else
            {
                result += fmt[i++];
            }
        }

        return result;
    }

} // namespace behl
