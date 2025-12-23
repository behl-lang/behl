#include "lsp_server.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>

// WASM bindings for JavaScript/Node.js
using namespace emscripten;

EMSCRIPTEN_BINDINGS(behl_lsp)
{
    class_<behl::lsp::LanguageServer>("LanguageServer")
        .constructor<>()
        .function("initialize", &behl::lsp::LanguageServer::initialize)
        .function("getDiagnostics",
            optional_override([](behl::lsp::LanguageServer& self, const std::string& source_code,
                                  const std::string& file_path) { return self.get_diagnostics(source_code, file_path); }))
        .function("getCompletions",
            optional_override(
                [](behl::lsp::LanguageServer& self, const std::string& source_code, int line, int character,
                    const std::string& file_path) { return self.get_completions(source_code, line, character, file_path); }))
        .function("getHoverInfo", &behl::lsp::LanguageServer::get_hover_info)
        .function("getSignatureHelp",
            optional_override([](behl::lsp::LanguageServer& self, const std::string& source_code, int line, int character,
                                  const std::string& file_path) {
                return self.get_signature_help(source_code, line, character, file_path);
            }));
}
