#pragma once

#ifdef BEHL_SHARED_LIBRARY
#    ifdef _WIN32
#        ifdef BEHL_BUILDING_LIBRARY
#            define BEHL_API __declspec(dllexport)
#        else
#            define BEHL_API __declspec(dllimport)
#        endif
#    else
#        ifdef BEHL_BUILDING_LIBRARY
#            define BEHL_API __attribute__((visibility("default")))
#        else
#            define BEHL_API
#        endif
#    endif
#else
#    define BEHL_API
#endif
