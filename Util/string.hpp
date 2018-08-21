//
// Created by Spencer Michaels on 8/19/18.
//

#ifndef XENDBG_UTIL_STRING_HPP
#define XENDBG_UTIL_STRING_HPP

#include <string>

namespace xd::util::string {

  using StrConstIt = std::string::const_iterator;

  StrConstIt expect(const std::string& target, StrConstIt begin, StrConstIt end);
  StrConstIt next_char(StrConstIt begin, StrConstIt end, char c);
  StrConstIt next_not_char(StrConstIt begin, StrConstIt end, char c);
  StrConstIt next_whitespace(StrConstIt begin, StrConstIt end);
  StrConstIt skip_whitespace(StrConstIt begin, StrConstIt end);

}

#endif //XENDBG_STRING_HPP
