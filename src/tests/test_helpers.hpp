#pragma once

#include "vm/vm_error.hpp"

#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

namespace behl_test
{
    inline std::string error_text(behl::State* S)
    {
        if (behl::get_top(S) == 0)
        {
            return "<no error value>";
        }
        if (behl::type(S, -1) == behl::Type::kString)
        {
            return std::string(behl::to_string(S, -1));
        }
        return "(error object is a " + std::string(behl::value_typename(S, -1)) + " value)";
    }

    inline std::string exception_text(const behl::Exception& e)
    {
        if (e.value().is_string())
        {
            return std::string(e.value().get_string()->view());
        }
        return e.what();
    }

    inline ::testing::AssertionResult status_ok(behl::State* S, int32_t status, std::string_view what)
    {
        if (status >= 0)
        {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << what << " failed (" << status << "): " << error_text(S);
    }

    inline ::testing::AssertionResult status_failed(int32_t status, std::string_view what)
    {
        if (status < 0)
        {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << what << " succeeded but was expected to fail";
    }

    inline ::testing::AssertionResult call_ok(behl::State* S, int32_t nargs, int32_t nresults)
    {
        return status_ok(S, behl::call(S, nargs, nresults), "call");
    }

    inline ::testing::AssertionResult call_fails(behl::State* S, int32_t nargs, int32_t nresults)
    {
        return status_failed(behl::call(S, nargs, nresults), "call");
    }

    inline ::testing::AssertionResult load_ok(behl::State* S, std::string_view code, bool optimize = true)
    {
        return status_ok(S, behl::load_string(S, code, optimize), "load_string");
    }

    inline ::testing::AssertionResult load_fails(behl::State* S, std::string_view code, bool optimize = true)
    {
        return status_failed(behl::load_string(S, code, optimize), "load_string");
    }

    inline ::testing::AssertionResult load_buffer_ok(
        behl::State* S, std::string_view code, std::string_view chunkname, bool optimize = true)
    {
        return status_ok(S, behl::load_buffer(S, code, chunkname, optimize), "load_buffer");
    }

    inline ::testing::AssertionResult load_buffer_fails(
        behl::State* S, std::string_view code, std::string_view chunkname, bool optimize = true)
    {
        return status_failed(behl::load_buffer(S, code, chunkname, optimize), "load_buffer");
    }

} // namespace behl_test
