#pragma once
#include <vector>
#include <string>

struct FileInfo {
	long size;
	std::vector<char> contents;
};

bool exists(std::string& path);
FileInfo get_file_info(std::string& path, bool get_contents);
std::string base64_encode(const char *path, unsigned int len);
