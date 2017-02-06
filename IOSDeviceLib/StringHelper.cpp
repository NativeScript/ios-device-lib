#include "StringHelper.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <cctype>
#include <vector>

std::string to_hex(const std::string & s)
{
	std::ostringstream ret;

	for (size_t i = 0; i < s.length(); ++i)
		ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase << (int)s[i];

	return ret.str();
}

bool contains(const std::string& text, const std::string& search_string, int start_position)
{
	std::string::size_type index_of_documents = text.find(search_string, start_position);
	return index_of_documents != std::string::npos;
}

bool starts_with(const std::string& str, const std::string& prefix)
{
	return !str.compare(0, prefix.size(), prefix);
}

std::string trim_end(std::string &str)
{
    str.erase(std::find_if(str.rbegin(), str.rend(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
    return str;
}

void split(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim))
    {
        elems.push_back(item);
    }
}

std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

void replace_all(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}
