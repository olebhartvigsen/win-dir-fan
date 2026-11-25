// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
// Table test for the filter ranking. Portable: no Win32, no test framework.
#include "../src/MatchScore.h"

#include <cstdio>
#include <iterator>
#include <string>

namespace {

struct Case {
    const wchar_t* name;
    const wchar_t* relativeDir;
    const wchar_t* query;
    int            expected;
};

const Case kCases[] = {
    // exact on the visible name (extension hidden)
    {L"rapport.docx",   L"",          L"rapport",  FanFolderMatch::kExact},
    {L"rapport.docx",   L"",          L"rapp",     FanFolderMatch::kPrefix},
    {L"rapport",        L"",          L"rapport",  FanFolderMatch::kExact},
    // prefix
    {L"rapport.docx",   L"",          L"rap",      FanFolderMatch::kPrefix},
    // word boundary after '-'
    {L"min-rapport.docx", L"",        L"rapp",     FanFolderMatch::kWordBoundary},
    // extension after a dot is a boundary too
    {L"rapport.docx",   L"",          L"docx",     FanFolderMatch::kWordBoundary},
    // plain substring, no boundary
    {L"skrapport.docx", L"",          L"rapp",     FanFolderMatch::kSubstring},
    // only the folder matches
    {L"rapport.docx",   L"Projekt A", L"projekt",  FanFolderMatch::kFolderOnly},
    // no match anywhere
    {L"rapport.docx",   L"Projekt A", L"zzz",      FanFolderMatch::kNoMatch},
    // empty query never matches
    {L"rapport.docx",   L"",          L"",        FanFolderMatch::kNoMatch},
};

} // namespace

int main() {
    int failures = 0;

    for (const Case& c : kCases) {
        const std::wstring query = FanFolderMatch::Lowercase(c.query);
        const int got = FanFolderMatch::Score(c.name, c.relativeDir, query);
        if (got != c.expected) {
            ++failures;
            std::wprintf(L"FAIL: name=\"%ls\" dir=\"%ls\" query=\"%ls\" expected %d got %d\n",
                         c.name, c.relativeDir, c.query, c.expected, got);
        }
    }

    // Case folding is applied by the caller, so verify it directly.
    if (FanFolderMatch::Lowercase(L"RAPPORT.Docx") != L"rapport.docx") {
        ++failures;
        std::wprintf(L"FAIL: Lowercase did not fold mixed case\n");
    }

    // A null folder path must not crash (root-level items pass an empty one).
    if (FanFolderMatch::Score(L"alpha.txt", nullptr, L"al") != FanFolderMatch::kPrefix) {
        ++failures;
        std::wprintf(L"FAIL: null relativeDir was not tolerated\n");
    }

    // The index scores pre-folded strings. It must rank exactly like the
    // folding path, otherwise results depend on whether the index is warm.
    for (const Case& c : kCases) {
        const std::wstring lowerName = FanFolderMatch::Lowercase(c.name);
        const std::wstring lowerDir  = FanFolderMatch::Lowercase(c.relativeDir);
        const std::wstring query     = FanFolderMatch::Lowercase(c.query);
        const int lowered = FanFolderMatch::ScoreLowered(lowerName, lowerDir, query);
        if (lowered != c.expected) {
            ++failures;
            std::wprintf(L"FAIL: ScoreLowered name=\"%ls\" expected %d got %d\n",
                         c.name, c.expected, lowered);
        }
    }

    if (failures == 0) {
        std::wprintf(L"All %zu match cases passed\n", std::size(kCases));
        return 0;
    }
    std::wprintf(L"%d failure(s)\n", failures);
    return 1;
}