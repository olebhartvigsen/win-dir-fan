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

// towlower is a locale call per character, and a deep-folder search folds the
// name of every file on every keystroke. ASCII needs no locale, and Windows
// compares file names ordinally, so folding A-Z here is also the faithful rule.
inline wchar_t Fold(wchar_t c) {
    if (c >= L'A' && c <= L'Z') return static_cast<wchar_t>(c + 32);
    return (c < 128) ? c : static_cast<wchar_t>(towlower(c));
}

// Writes the folded copy into `out`, reusing its buffer. `out` grows to the
// longest name seen and then stops allocating.
void LowercaseInto(std::wstring_view text, std::wstring& out) {
    out.resize(text.size());
    for (size_t i = 0; i < text.size(); ++i) out[i] = Fold(text[i]);
}

} // namespace

namespace FanFolderMatch {

std::wstring Lowercase(std::wstring_view text) {
    std::wstring out;
    LowercaseInto(text, out);
    return out;
}

int ScoreLowered(std::wstring_view lowerName, std::wstring_view lowerDir,
                 const std::wstring& lowerQuery) {
    if (lowerName.empty() || lowerQuery.empty()) return kNoMatch;

    if (lowerName == lowerQuery) return kExact;

    if (lowerName.substr(0, lowerQuery.size()) == lowerQuery &&
        lowerName.size() >= lowerQuery.size()) {
        // "rapport" against "rapport.docx" is an exact hit on the name the
        // user sees when extensions are hidden.
        if (lowerName.size() > lowerQuery.size() &&
            lowerName[lowerQuery.size()] == L'.')
            return kExact;
        return kPrefix;
    }

    const size_t hit = lowerName.find(lowerQuery);
    if (hit != std::wstring_view::npos) {
        return (hit > 0 && IsBoundary(lowerName[hit - 1])) ? kWordBoundary
                                                           : kSubstring;
    }

    if (!lowerDir.empty() && lowerDir.find(lowerQuery) != std::wstring_view::npos)
        return kFolderOnly;

    return kNoMatch;
}

int Score(const wchar_t* name, const wchar_t* relativeDir,
          const std::wstring& lowerQuery) {
    if (!name || !name[0] || lowerQuery.empty()) return kNoMatch;

    // Per-thread scratch: the search scores every file in the tree, so a heap
    // allocation per candidate is pure overhead.
    thread_local std::wstring lowerName;
    thread_local std::wstring lowerDir;
    LowercaseInto(name, lowerName);
    if (relativeDir && relativeDir[0]) {
        LowercaseInto(relativeDir, lowerDir);
        return ScoreLowered(lowerName, lowerDir, lowerQuery);
    }
    return ScoreLowered(lowerName, std::wstring_view(), lowerQuery);
}

} // namespace FanFolderMatch
