#include "vm/bytecode.hpp"
#include "vm/vm.hpp"

#include <algorithm>
#include <behl/behl.hpp>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <vector>

#ifdef _WIN32
#    include <windows.h>
#else
#    include <csignal>
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif

    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    return result;
}
