#include "StringHelper.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <sstream>
#include <vector>

std::string to_hex(const std::string &s) {
  std::ostringstream ret;

  for (size_t i = 0; i < s.length(); ++i)
    ret << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase
        << (int)s[i];

  return ret.str();
}

bool contains(const std::string &text, const std::string &search_string,
              int start_position) {
  std::string::size_type index_of_documents =
      text.find(search_string, start_position);
  return index_of_documents != std::string::npos;
}

bool starts_with(const std::string &str, const std::string &prefix) {
  return !str.compare(0, prefix.size(), prefix);
}

std::string trim_end(std::string &str) {
  str.erase(std::find_if(str.rbegin(), str.rend(),
                         [](unsigned char ch) { return !std::isspace(ch); })
                .base(),
            str.end());
  return str;
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss;
  ss.str(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
}

std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> elems;
  split(s, delim, elems);
  return elems;
}

void replace_all(std::string &str, const std::string &from,
                 const std::string &to) {
  if (from.empty())
    return;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length(); // In case 'to' contains 'from', like replacing
                              // 'x' with 'yx'
  }
}

std::string
url_encode_without_forward_slash_and_colon(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (std::string::const_iterator i = value.begin(), n = value.end(); i != n;
       ++i) {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    // NB! Do not encode / and : because they're part of the file:/// prefix
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' ||
        c == '/' || c == ':') {
      escaped << c;
      continue;
    }

    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char)c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}
