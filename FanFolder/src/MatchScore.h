// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#pragma once
#include <string>
#include <string_view>

// Ranking for the type-to-filter pass. Pure string logic with no Win32
// dependency, so the rules can be unit tested on any platform.
namespace FanFolderMatch {

constexpr int kNoMatch      = 0;
constexpr int kFolderOnly   = 1;  // match only in the relative folder path
constexpr int kSubstring    = 2;  // substring inside the file name
constexpr int kWordBoundary = 3;  // substring that starts after a separator
constexpr int kPrefix       = 4;  // file name starts with the query
constexpr int kExact        = 5;  // query is the whole name, or the name stem

// Locale-aware lowercase copy, used for both the query and each candidate so
// the comparison runs once per item.
std::wstring Lowercase(std::wstring_view text);

// Returns one of the constants above. A zero query is never a match.
// relativeDir may be null or empty for root-level items.
int Score(const wchar_t* name, const wchar_t* relativeDir,
          const std::wstring& lowerQuery);

} // namespace FanFolderMatch