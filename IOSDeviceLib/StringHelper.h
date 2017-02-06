#pragma once
#include <string>
#include <vector>

std::string to_hex(const std::string& s);
bool contains(const std::string& text, const std::string& search_string, int start_position = 0);
bool starts_with(const std::string& str, const std::string& prefix);
std::string trim_end(std::string &str);
void split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);
void replace_all(std::string& str, const std::string& from, const std::string& to);