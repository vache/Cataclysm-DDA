#include "helper.h"
namespace helper
{
    int to_int(std::string str)
    {
        std::istringstream buffer(str);
        int value;
        buffer >> value;
        if(str != to_string(value)) //if the converted value is different then return 0 (similar to atoi)
        {
            return 0;
        }
        return value;
    }
    std::string to_string(int i)
    {
        std::stringstream sstr;
        sstr << i;
        std::string str1 = sstr.str();
        return str1;
    }
    std::string space_to_underscore(std::string input)
    {
        std::string output = input; // Replace " " with "_"
        size_t found = output.find(" ");
        while (found != std::string::npos)
        {
            output.replace(found, 1, "_");
            found = output.find(" ");
        }
        return output;
    }
    std::string underscore_to_space(std::string input)
    {
        std::string output = input; // Replace "_" with " "
        size_t found = output.find("_");
        while (found != std::string::npos)
        {
            output.replace(found, 1, " ");
            found = output.find("_");
        }
        return output;
    }
}
