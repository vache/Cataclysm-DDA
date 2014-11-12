#ifndef HELPER_H
#define HELPER_H

#include <string>
#include <sstream>

namespace helper
{
    int to_int(std::string str);
    std::string to_string_int(int i);
    std::string to_string_long(long i);
    std::string space_to_underscore(std::string str);
    std::string underscore_to_space(std::string str);
    std::string swap_char(std::string input, char c1, char c2);
}

#endif
