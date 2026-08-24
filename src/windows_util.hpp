// Internal Windows helpers (UTF-8 conversion, error text). Not installed.
#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>

namespace topology_fabric {
namespace winutil {

inline std::string widen_to_utf8(const wchar_t* w) {
  if (!w) return {};
  int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) return {};
  std::string out(static_cast<size_t>(len - 1), '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
  return out;
}

inline std::string widen_to_utf8(const std::wstring& w) {
  return widen_to_utf8(w.c_str());
}

inline std::wstring utf8_to_widen(const std::string& s) {
  if (s.empty()) return {};
  int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
  if (len <= 0) return {};
  std::wstring out(static_cast<size_t>(len), L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
  return out;
}

inline std::string last_error(const char* api) {
  DWORD code = ::GetLastError();
  LPWSTR buf = nullptr;
  ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                   reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
  std::string msg = buf ? widen_to_utf8(buf) : std::string("<no message>");
  if (buf) ::LocalFree(buf);
  return std::string(api) + " failed (error " + std::to_string(code) + "): " + msg;
}

}  // namespace winutil
}  // namespace topology_fabric
#endif