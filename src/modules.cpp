#include "modules.hpp"

#include "behl.hpp"
#include "common/format.hpp"
#include "common/print.hpp"
#include "state.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace behl
{

    std::string resolve_module_path(State* S, std::string_view module_name, std::string_view importing_file)
    {
        std::string filename{ module_name };

        // Add .behl extension if not present
        if (!std::string_view(filename).ends_with(".behl"))
        {
            filename += ".behl";
        }

        // Relative imports (starts with ./ or ../)
        bool is_relative = module_name.starts_with("./") || module_name.starts_with("../");
        if (is_relative)
        {
            try
            {
                std::filesystem::path importer_dir = std::filesystem::path(importing_file).parent_path();
                std::filesystem::path resolved = importer_dir / filename;

                if (std::filesystem::exists(resolved))
                {
                    return std::filesystem::canonical(resolved).string();
                }
            }
            catch (const std::filesystem::filesystem_error&)
            {
                return std::string(); // Return empty string on error
            }

            return std::string(); // Not found
        }

        // Module path search - first try relative to importing file
        try
        {
            // Normalize path separators for cross-platform compatibility
            std::string normalized_path(importing_file);
            std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');

            std::filesystem::path importer_path(normalized_path);
            std::filesystem::path importer_dir = importer_path.parent_path();

            // Try in same directory as importer, then in its modules/ subdirectory
            const std::filesystem::path candidates[] = {
                importer_dir / filename,
                importer_dir / "modules" / filename,
            };

            for (const auto& candidate : candidates)
            {
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec) && !ec)
                {
                    std::error_code canonical_ec;
                    std::filesystem::path canonical = std::filesystem::canonical(candidate, canonical_ec);
                    if (!canonical_ec)
                    {
                        return canonical.string();
                    }
                }

#if defined(__EMSCRIPTEN__)
                if (!candidate.is_absolute())
                {
                    const std::filesystem::path rooted = std::filesystem::path("/") / candidate;
                    std::error_code vfs_ec;
                    if (std::filesystem::exists(rooted, vfs_ec) && !vfs_ec)
                    {
                        std::error_code canonical_ec;
                        std::filesystem::path canonical = std::filesystem::canonical(rooted, canonical_ec);
                        if (!canonical_ec)
                        {
                            return canonical.string();
                        }
                    }
                }
#endif
            }
        }
        catch (...)
        {
        }

        // Fallback: Module path search relative to CWD
        for (const auto& search_path : S->module_paths)
        {
            try
            {
                std::filesystem::path full = std::filesystem::path(search_path->view()) / filename;

                if (std::filesystem::exists(full))
                {
                    return std::filesystem::canonical(full).string();
                }
            }
            catch (const std::filesystem::filesystem_error&)
            {
                continue;
            }
        }

        return std::string(); // Not found
    }

} // namespace behl
