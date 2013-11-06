#ifndef _HELPER_H_
#define _HELPER_H_
#include <string>
#include <sstream>
namespace helper
{
    int to_int(std::string str);
    std::string to_string(int i);
    std::string space_to_underscore(std::string str);
    std::string underscore_to_space(std::string str);
}
#endif
