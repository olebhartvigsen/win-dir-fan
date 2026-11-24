// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "MatchScore.h"

#include <cwctype>

namespace {

// Separators that make the following character the start of a word. The
// backslash is here so a match at the start of a folder segment in
// relativeDir reads the same way as one after "Projekt -".
bool IsBoundary(wchar_t c) {
    return c == L' ' || c == L'-' || c == L'_' || c == L'.' ||
           c == L'(' || c == L'[' || c == L'\\' || c == L'/';
}

} // namespace

namespace FanFolderMatch {

std::wstring Lowercase(std::wstring_view text) {
    std::wstring out(text);
    for (auto& c : out) c = static_cast<wchar_t>(towlower(c));
    return out;
}

int Score(const wchar_t* name, const wchar_t* relativeDir,
          const std::wstring& lowerQuery) {
    if (!name || !name[0] || lowerQuery.empty()) return kNoMatch;

    const std::wstring lowerName = Lowercase(name);
    if (lowerName == lowerQuery) return kExact;

    if (lowerName.rfind(lowerQuery, 0) == 0) {
        // "rapport" against "rapport.docx" is an exact hit on the name the
        // user sees when extensions are hidden.
        if (lowerName.size() > lowerQuery.size() &&
            lowerName[lowerQuery.size()] == L'.')
            return kExact;
        return kPrefix;
    }

    const size_t hit = lowerName.find(lowerQuery);
    if (hit != std::wstring::npos) {
        return (hit > 0 && IsBoundary(lowerName[hit - 1])) ? kWordBoundary
                                                           : kSubstring;
    }

    if (relativeDir && relativeDir[0]) {
        const std::wstring lowerDir = Lowercase(relativeDir);
        if (lowerDir.find(lowerQuery) != std::wstring::npos) return kFolderOnly;
    }

    return kNoMatch;
}

} // namespace FanFolderMatch