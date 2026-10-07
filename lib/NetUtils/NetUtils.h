#pragma once

#include <Arduino.h>
#include <vector>

// Small string helpers shared by the network-facing modules.
namespace NetUtils {

// Percent-encode everything except RFC 3986 unreserved characters.
inline String urlEncode(const String &in) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  out.reserve(in.length() * 3);
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 0x0F];
    }
  }
  return out;
}

// Escape text for safe use inside HTML attributes and element bodies.
inline String htmlEscape(const String &in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&#39;";
      break;
    default:
      out += c;
    }
  }
  return out;
}

// Split a separated list, trimming entries and skipping empty ones.
inline std::vector<String> splitList(const String &in, char sep,
                                     size_t maxItems) {
  std::vector<String> items;
  int start = 0;
  while (start < (int)in.length()) {
    int idx = in.indexOf(sep, start);
    if (idx == -1)
      idx = in.length();
    String s = in.substring(start, idx);
    s.trim();
    if (s.length() > 0)
      items.push_back(s);
    start = idx + 1;
    if (items.size() >= maxItems)
      break;
  }
  return items;
}

} // namespace NetUtils
