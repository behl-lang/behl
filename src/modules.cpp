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
            std::cerr << "Resolving relative module import '" << module_name << "' from '" << importing_file << "'\n"
                      << std::flush;

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
            std::cerr << "Resolving module import '" << module_name << "' from '" << importing_file << "'\n" << std::flush;

            // Normalize path separators for cross-platform compatibility
            std::string normalized_path(importing_file);
            std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');

            // For VFS paths, we need to prepend / if working with relative paths
            std::filesystem::path importer_path(normalized_path);

            // If relative path, prepend / for VFS access
            std::string vfs_normalized = normalized_path;
            if (!vfs_normalized.empty() && vfs_normalized[0] != '/')
            {
                vfs_normalized = "/" + vfs_normalized;
            }

            std::cerr << "Importer path (relative): '" << normalized_path << "'\n" << std::flush;
            std::cerr << "Importer path (VFS): '" << vfs_normalized << "'\n" << std::flush;

            std::filesystem::path importer_dir = importer_path.parent_path();
            std::cerr << "Importer directory: '" << importer_dir.string() << "'\n" << std::flush;

            std::error_code ec;
            // Try in same directory as importer
            std::filesystem::path same_dir = importer_dir / filename;
            // Prepend / for VFS access
            std::string vfs_same_dir = same_dir.string();
            if (!vfs_same_dir.empty() && vfs_same_dir[0] != '/')
            {
                vfs_same_dir = "/" + vfs_same_dir;
            }
            std::cerr << "Trying module path (VFS): '" << vfs_same_dir << "'\n" << std::flush;
            bool exists1 = std::filesystem::exists(vfs_same_dir, ec);
            std::cerr << "  exists() returned: " << (exists1 ? "true" : "false") << "\n" << std::flush;
            if (ec)
            {
                std::cerr << "  error code: " << ec.message() << "\n" << std::flush;
            }
            if (exists1 && !ec)
            {
                std::cerr << "Found at: '" << vfs_same_dir << "'\n" << std::flush;
                return std::filesystem::canonical(vfs_same_dir).string();
            }

            // Try in modules/ subdirectory relative to importer
            std::filesystem::path modules_dir = importer_dir / "modules" / filename;
            // Prepend / for VFS access
            std::string vfs_modules_dir = modules_dir.string();
            if (!vfs_modules_dir.empty() && vfs_modules_dir[0] != '/')
            {
                vfs_modules_dir = "/" + vfs_modules_dir;
            }
            std::cerr << "Trying module path (VFS): '" << vfs_modules_dir << "'\n" << std::flush;
            bool exists2 = std::filesystem::exists(vfs_modules_dir, ec);
            std::cerr << "  exists() returned: " << (exists2 ? "true" : "false") << "\n" << std::flush;
            if (ec)
            {
                std::cerr << "  error code: " << ec.message() << "\n" << std::flush;
            }
            if (exists2 && !ec)
            {
                std::cerr << "Found at: '" << vfs_modules_dir << "'\n" << std::flush;
                return std::filesystem::canonical(vfs_modules_dir).string();
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Continue to CWD-relative search
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
