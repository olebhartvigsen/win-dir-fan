// Copyright (c) 2026 Ole Bülow Hartvigsen. All rights reserved.
#include "pch.h"
#include "Localization.h"

// ---------------------------------------------------------------------------
// English (default / fallback)
// ---------------------------------------------------------------------------
static constexpr Strings kEn = {
    L"Sort by",
    L"Date modified \u2193 (newest first)",
    L"Date modified \u2191 (oldest first)",
    L"Date created \u2193 (newest first)",
    L"Date created \u2191 (oldest first)",
    L"Name A \u2192 Z",
    L"Name Z \u2192 A",
    L"Max items",
    L"Animation",
    L"Fan", L"Glide", L"Spring", L"Fade", L"None",
    L"Include folders",
    L"Show file extensions",
    L"Change folder\u2026",
    L"Exit FanFolder",
    L"Open: ",
    L"Select folder to watch",
    L"Folder",
    L"Downloads", L"Recent (Explorer)", L"Desktop", L"Documents", L"Recent files", L"Seneste", L"Recent Office 365 documents", L"Browse\u2026",
    L"Open in Explorer",
};

// ---------------------------------------------------------------------------
// Danish (Dansk)  —  LANG_DANISH 0x06
// ---------------------------------------------------------------------------
static constexpr Strings kDa = {
    L"Sorter efter",
    L"Dato \u00e6ndret \u2193 (nyeste f\u00f8rst)",
    L"Dato \u00e6ndret \u2191 (\u00e6ldste f\u00f8rst)",
    L"Dato oprettet \u2193 (nyeste f\u00f8rst)",
    L"Dato oprettet \u2191 (\u00e6ldste f\u00f8rst)",
    L"Navn A \u2192 Z",
    L"Navn Z \u2192 A",
    L"Maks. elementer",
    L"Animation",
    L"Vifte", L"Gli", L"Fjederkraft", L"Tone ind", L"Ingen",
    L"Inkluder mapper",
    L"Vis filtypenavne",
    L"Skift mappe\u2026",
    L"Afslut FanFolder",
    L"\u00c5bn: ",
    L"V\u00e6lg mappe at overv\u00e5ge",
    L"Mappe",
    L"Overf\u00f8rsler", L"Seneste", L"Skrivebord", L"Dokumenter", L"Seneste filer", L"Seneste", L"Seneste Office 365 dokumenter", L"Gennemse\u2026",
    L"\u00c5bn i Stifinder",
};

// ---------------------------------------------------------------------------
// Swedish (Svenska)  —  LANG_SWEDISH 0x1D
// ---------------------------------------------------------------------------
static constexpr Strings kSv = {
    L"Sortera efter",
    L"\u00c4ndringstid \u2193 (nyast f\u00f6rst)",
    L"\u00c4ndringstid \u2191 (\u00e4ldst f\u00f6rst)",
    L"Skapandetid \u2193 (nyast f\u00f6rst)",
    L"Skapandetid \u2191 (\u00e4ldst f\u00f6rst)",
    L"Namn A \u2192 Z",
    L"Namn Z \u2192 A",
    L"Max antal",
    L"Animation",
    L"Fl\u00e4kt", L"Glid", L"Fj\u00e4der", L"Tona in", L"Ingen",
    L"Inkludera mappar",
    L"Visa filnamnstill\u00e4gg",
    L"Byt mapp\u2026",
    L"Avsluta FanFolder",
    L"\u00d6ppna: ",
    L"V\u00e4lj mapp att \u00f6vervaka",
    L"Mapp",
    L"Nedladdningar", L"Senaste (Utforskaren)", L"Skrivbord", L"Dokument", L"Senaste filer", L"Seneste", L"Senaste Office 365-dokument", L"Bl\u00e4ddra\u2026",
    L"\u00d6ppna i Utforskaren",
};

// ---------------------------------------------------------------------------
// Norwegian (Norsk)  —  LANG_NORWEGIAN 0x14
// ---------------------------------------------------------------------------
static constexpr Strings kNo = {
    L"Sorter etter",
    L"Dato endret \u2193 (nyeste f\u00f8rst)",
    L"Dato endret \u2191 (eldste f\u00f8rst)",
    L"Dato opprettet \u2193 (nyeste f\u00f8rst)",
    L"Dato opprettet \u2191 (eldste f\u00f8rst)",
    L"Navn A \u2192 Z",
    L"Navn Z \u2192 A",
    L"Maks. elementer",
    L"Animasjon",
    L"Vifte", L"Gli", L"Fj\u00e6r", L"Ton inn", L"Ingen",
    L"Inkluder mapper",
    L"Vis filendelser",
    L"Bytt mappe\u2026",
    L"Avslutt FanFolder",
    L"\u00c5pne: ",
    L"Velg mappe \u00e5 overv\u00e5ke",
    L"Mappe",
    L"Nedlastinger", L"Siste (Utforsker)", L"Skrivebord", L"Dokumenter", L"Siste filer", L"Seneste", L"Siste Office 365-dokumenter", L"Bla gjennom\u2026",
    L"\u00c5pne i Utforsker",
};

// ---------------------------------------------------------------------------
// German (Deutsch)  —  LANG_GERMAN 0x07
// ---------------------------------------------------------------------------
static constexpr Strings kDe = {
    L"Sortieren nach",
    L"\u00c4nderungsdatum \u2193 (neueste zuerst)",
    L"\u00c4nderungsdatum \u2191 (\u00e4lteste zuerst)",
    L"Erstellungsdatum \u2193 (neueste zuerst)",
    L"Erstellungsdatum \u2191 (\u00e4lteste zuerst)",
    L"Name A \u2192 Z",
    L"Name Z \u2192 A",
    L"Max. Eintr\u00e4ge",
    L"Animation",
    L"F\u00e4cher", L"Gleiten", L"Feder", L"Einblenden", L"Keine",
    L"Ordner einschlie\u00dfen",
    L"Dateierweiterungen anzeigen",
    L"Ordner wechseln\u2026",
    L"FanFolder beenden",
    L"\u00d6ffnen: ",
    L"Ordner zum \u00dcberwachen ausw\u00e4hlen",
    L"Ordner",
    L"Downloads", L"Zuletzt (Explorer)", L"Desktop", L"Dokumente", L"Zuletzt ge\u00f6ffnete Dateien", L"Zuletzt", L"Zuletzt ge\u00f6ffnete Office 365-Dokumente", L"Durchsuchen\u2026",
    L"Im Explorer \u00f6ffnen",
};

// ---------------------------------------------------------------------------
// Dutch (Nederlands)  —  LANG_DUTCH 0x13
// ---------------------------------------------------------------------------
static constexpr Strings kNl = {
    L"Sorteren op",
    L"Datum gewijzigd \u2193 (nieuwste eerst)",
    L"Datum gewijzigd \u2191 (oudste eerst)",
    L"Datum gemaakt \u2193 (nieuwste eerst)",
    L"Datum gemaakt \u2191 (oudste eerst)",
    L"Naam A \u2192 Z",
    L"Naam Z \u2192 A",
    L"Max. items",
    L"Animatie",
    L"Waaier", L"Glijden", L"Veer", L"Vervagen", L"Geen",
    L"Mappen opnemen",
    L"Bestandsextensies weergeven",
    L"Map wijzigen\u2026",
    L"FanFolder afsluiten",
    L"Openen: ",
    L"Map selecteren om te bekijken",
    L"Map",
    L"Downloads", L"Recent (Verkenner)", L"Bureaublad", L"Documenten", L"Recente bestanden", L"Recent", L"Recente Office 365-documenten", L"Bladeren\u2026",
    L"Openen in Verkenner",
};

// ---------------------------------------------------------------------------
// Polish (Polski)  —  LANG_POLISH 0x15
// ---------------------------------------------------------------------------
static constexpr Strings kPl = {
    L"Sortuj wed\u0142ug",
    L"Data modyfikacji \u2193 (najnowsze)",
    L"Data modyfikacji \u2191 (najstarsze)",
    L"Data utworzenia \u2193 (najnowsze)",
    L"Data utworzenia \u2191 (najstarsze)",
    L"Nazwa A \u2192 Z",
    L"Nazwa Z \u2192 A",
    L"Maks. element\u00f3w",
    L"Animacja",
    L"Wachlarz", L"\u015blizg", L"Spr\u0119\u017cyna", L"Zanikanie", L"Brak",
    L"Uwzgl\u0119dnij foldery",
    L"Poka\u017c rozszerzenia plik\u00f3w",
    L"Zmie\u0144 folder\u2026",
    L"Zamknij FanFolder",
    L"Otw\u00f3rz: ",
    L"Wybierz folder do \u015bledzenia",
    L"Folder",
    L"Pobrane", L"Ostatnie (Eksplorator)", L"Pulpit", L"Dokumenty", L"Ostatnie pliki", L"Ostatnie", L"Ostatnie dokumenty Office 365", L"Przegl\u0105daj\u2026",
    L"Otw\u00f3rz w Eksploratorze",
};

// ---------------------------------------------------------------------------
// Arabic (عربي)  —  LANG_ARABIC 0x01
// ---------------------------------------------------------------------------
static constexpr Strings kAr = {
    L"\u062a\u0631\u062a\u064a\u0628 \u062d\u0633\u0628",
    L"\u062a\u0627\u0631\u064a\u062e \u0627\u0644\u062a\u0639\u062f\u064a\u0644 \u2193 (\u0627\u0644\u0623\u062d\u062f\u062b)",
    L"\u062a\u0627\u0631\u064a\u062e \u0627\u0644\u062a\u0639\u062f\u064a\u0644 \u2191 (\u0627\u0644\u0623\u0642\u062f\u0645)",
    L"\u062a\u0627\u0631\u064a\u062e \u0627\u0644\u0625\u0646\u0634\u0627\u0621 \u2193 (\u0627\u0644\u0623\u062d\u062f\u062b)",
    L"\u062a\u0627\u0631\u064a\u062e \u0627\u0644\u0625\u0646\u0634\u0627\u0621 \u2191 (\u0627\u0644\u0623\u0642\u062f\u0645)",
    L"\u0627\u0644\u0627\u0633\u0645 \u0623 \u2192 \u064a",
    L"\u0627\u0644\u0627\u0633\u0645 \u064a \u2192 \u0623",
    L"\u0627\u0644\u062d\u062f \u0627\u0644\u0623\u0642\u0635\u0649 \u0644\u0644\u0639\u0646\u0627\u0635\u0631",
    L"\u0627\u0644\u0631\u0633\u0648\u0645 \u0627\u0644\u0645\u062a\u062d\u0631\u0643\u0629",
    L"\u0645\u0631\u0648\u062d\u0629", L"\u0627\u0646\u0632\u0644\u0627\u0642", L"\u0646\u0627\u0628\u0636", L"\u062a\u0644\u0627\u0634\u064a", L"\u0644\u0627 \u0634\u064a\u0621",
    L"\u062a\u0636\u0645\u064a\u0646 \u0627\u0644\u0645\u062c\u0644\u062f\u0627\u062a",
    L"\u0625\u0638\u0647\u0627\u0631 \u0627\u0645\u062a\u062f\u0627\u062f\u0627\u062a \u0627\u0644\u0645\u0644\u0641\u0627\u062a",
    L"\u062a\u063a\u064a\u064a\u0631 \u0627\u0644\u0645\u062c\u0644\u062f\u2026",
    L"\u0625\u0646\u0647\u0627\u0621 FanFolder",
    L"\u0641\u062a\u062d: ",
    L"\u0627\u062e\u062a\u0631 \u0645\u062c\u0644\u062f\u064b\u0627 \u0644\u0644\u0645\u0631\u0627\u0642\u0628\u0629",
    L"\u0627\u0644\u0645\u062c\u0644\u062f",
    L"\u0627\u0644\u062a\u0646\u0632\u064a\u0644\u0627\u062a", L"\u0627\u0644\u0645\u0644\u0641\u0627\u062a \u0627\u0644\u0623\u062e\u064a\u0631\u0629", L"\u0633\u0637\u062d \u0627\u0644\u0645\u0643\u062a\u0628", L"\u0627\u0644\u0645\u0633\u062a\u0646\u062f\u0627\u062a", L"\u0627\u0644\u0645\u0644\u0641\u0627\u062a \u0627\u0644\u0623\u062e\u064a\u0631\u0629", L"\u0627\u0644\u0623\u062e\u064a\u0631\u0629", L"\u0645\u0633\u062a\u0646\u062f\u0627\u062a Office 365 \u0627\u0644\u0623\u062e\u064a\u0631\u0629", L"\u0627\u0633\u062a\u0639\u0631\u0627\u0636\u2026",
    L"\u0641\u062a\u062d \u0641\u064a \u0627\u0644\u0645\u0633\u062a\u0643\u0634\u0641",
};

// ---------------------------------------------------------------------------
// Chinese Simplified (中文简体)  —  LANG_CHINESE 0x04
// ---------------------------------------------------------------------------
static constexpr Strings kZh = {
    L"\u6392\u5e8f\u65b9\u5f0f",
    L"\u4fee\u6539\u65e5\u671f \u2193\uff08\u6700\u65b0\uff09",
    L"\u4fee\u6539\u65e5\u671f \u2191\uff08\u6700\u65e7\uff09",
    L"\u521b\u5efa\u65e5\u671f \u2193\uff08\u6700\u65b0\uff09",
    L"\u521b\u5efa\u65e5\u671f \u2191\uff08\u6700\u65e7\uff09",
    L"\u540d\u79f0 A \u2192 Z",
    L"\u540d\u79f0 Z \u2192 A",
    L"\u6700\u591a\u9879\u76ee\u6570",
    L"\u52a8\u753b",
    L"\u6247\u5f62", L"\u6ed1\u52a8", L"\u5f39\u7c27", L"\u6de1\u5165", L"\u65e0",
    L"\u5305\u542b\u6587\u4ef6\u5939",
    L"\u663e\u793a\u6587\u4ef6\u6269\u5c55\u540d",
    L"\u66f4\u6539\u6587\u4ef6\u5939\u2026",
    L"\u9000\u51fa FanFolder",
    L"\u6253\u5f00\uff1a",
    L"\u9009\u62e9\u8981\u76d1\u89c6\u7684\u6587\u4ef6\u5939",
    L"\u6587\u4ef6\u5939",
    L"\u4e0b\u8f7d", L"\u6700\u8fd1\uff08\u6587\u4ef6\u8d44\u6e90\u7ba1\u7406\u5668\uff09", L"\u684c\u9762", L"\u6587\u6863", L"\u6700\u8fd1\u7684\u6587\u4ef6", L"\u6700\u8fd1", L"\u6700\u8fd1\u7684 Office 365 \u6587\u6863", L"\u6d4f\u89c8\u2026",
    L"\u5728\u6587\u4ef6\u8d44\u6e90\u7ba1\u7406\u5668\u4e2d\u6253\u5f00",
};

// ---------------------------------------------------------------------------
// Japanese (日本語)  —  LANG_JAPANESE 0x11
// ---------------------------------------------------------------------------
static constexpr Strings kJa = {
    L"\u4e26\u3073\u66ff\u3048",
    L"\u66f4\u65b0\u65e5\u6642 \u2193\uff08\u65b0\u3057\u3044\u9806\uff09",
    L"\u66f4\u65b0\u65e5\u6642 \u2191\uff08\u53e4\u3044\u9806\uff09",
    L"\u4f5c\u6210\u65e5\u6642 \u2193\uff08\u65b0\u3057\u3044\u9806\uff09",
    L"\u4f5c\u6210\u65e5\u6642 \u2191\uff08\u53e4\u3044\u9806\uff09",
    L"\u540d\u524d A \u2192 Z",
    L"\u540d\u524d Z \u2192 A",
    L"\u8868\u793a\u6570\u306e\u4e0a\u9650",
    L"\u30a2\u30cb\u30e1\u30fc\u30b7\u30e7\u30f3",
    L"\u30d5\u30a1\u30f3", L"\u30b0\u30e9\u30a4\u30c9", L"\u30b9\u30d7\u30ea\u30f3\u30b0", L"\u30d5\u30a7\u30fc\u30c9", L"\u306a\u3057",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u542b\u3081\u308b",
    L"\u62e1\u5f35\u5b50\u3092\u8868\u793a",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u5909\u66f4\u2026",
    L"FanFolder \u3092\u7d42\u4e86",
    L"\u958b\u304f\uff1a",
    L"\u76e3\u8996\u3059\u308b\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u9078\u629e",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc",
    L"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9", L"\u6700\u8fd1\uff08\u30a8\u30af\u30b9\u30d7\u30ed\u30fc\u30e9\u30fc\uff09", L"\u30c7\u30b9\u30af\u30c8\u30c3\u30d7", L"\u30c9\u30ad\u30e5\u30e1\u30f3\u30c8", L"\u6700\u8fd1\u306e\u30d5\u30a1\u30a4\u30eb", L"\u6700\u8fd1", L"\u6700\u8fd1\u306e Office 365 \u30c9\u30ad\u30e5\u30e1\u30f3\u30c8", L"\u53c2\u7167\u2026",
    L"\u30a8\u30af\u30b9\u30d7\u30ed\u30fc\u30e9\u30fc\u3067\u958b\u304f",
};

// ---------------------------------------------------------------------------
// Korean (한국어)  —  LANG_KOREAN 0x12
// ---------------------------------------------------------------------------
static constexpr Strings kKo = {
    L"\uc815\ub82c \uae30\uc900",
    L"\uc218\uc815 \ub0a0\uc9dc \u2193 (\ucd5c\uc2e0\uc21c)",
    L"\uc218\uc815 \ub0a0\uc9dc \u2191 (\uc624\ub798\ub41c\uc21c)",
    L"\ub9cc\ub4e0 \ub0a0\uc9dc \u2193 (\ucd5c\uc2e0\uc21c)",
    L"\ub9cc\ub4e0 \ub0a0\uc9dc \u2191 (\uc624\ub798\ub41c\uc21c)",
    L"\uc774\ub984 A \u2192 Z",
    L"\uc774\ub984 Z \u2192 A",
    L"\ucd5c\ub300 \ud56d\ubaa9 \uc218",
    L"\uc560\ub2c8\uba54\uc774\uc158",
    L"\ud31c", L"\uae00\ub77c\uc774\ub4dc", L"\uc2a4\ud504\ub9c1", L"\ud398\uc774\ub4dc", L"\uc5c6\uc74c",
    L"\ud3f4\ub354 \ud3ec\ud568",
    L"\ud30c\uc77c \ud655\uc7a5\uc790 \ud45c\uc2dc",
    L"\ud3f4\ub354 \ubcc0\uacbd\u2026",
    L"FanFolder \uc885\ub8cc",
    L"\uc5f4\uae30: ",
    L"\uac10\uc2dc\ud560 \ud3f4\ub354 \uc120\ud0dd",
    L"\ud3f4\ub354",
    L"\ub2e4\uc6b4\ub85c\ub4dc", L"\uc904\uadfc \ud30c\uc77c (\ud0d0\uc0c9\uae30)", L"\ubc14\ud0d5 \ud654\uba74", L"\ubb38\uc11c", L"\ucd5c\uadfc \ud30c\uc77c", L"\ucd5c\uadfc", L"\ucd5c\uadfc Office 365 \ubb38\uc11c", L"\ucc3e\uc544\ubcf4\uae30\u2026",
    L"\ud0d0\uc0c9\uae30\uc5d0\uc11c \uc5f4\uae30",
};

// ---------------------------------------------------------------------------
// Swahili (Kiswahili)  —  LANG_SWAHILI 0x41
// ---------------------------------------------------------------------------
static constexpr Strings kSw = {
    L"Panga kwa",
    L"Tarehe iliyobadilishwa \u2193 (mpya kwanza)",
    L"Tarehe iliyobadilishwa \u2191 (kongwe kwanza)",
    L"Tarehe ya uundaji \u2193 (mpya kwanza)",
    L"Tarehe ya uundaji \u2191 (kongwe kwanza)",
    L"Jina A \u2192 Z",
    L"Jina Z \u2192 A",
    L"Vitu vingi zaidi",
    L"Uhuishaji",
    L"Shabiki", L"Teleza", L"Chemchemi", L"Fifia", L"Hakuna",
    L"Jumuisha folda",
    L"Onyesha viongezeo vya faili",
    L"Badilisha folda\u2026",
    L"Funga FanFolder",
    L"Fungua: ",
    L"Chagua folda ya kufuatilia",
    L"Folda",
    L"Vipakuliwa", L"Hivi karibuni (Kichunguzi)", L"Eneo-kazi", L"Nyaraka", L"Faili za hivi karibuni", L"Hivi karibuni", L"Hati za hivi karibuni za Office 365", L"Vinjari\u2026",
    L"Fungua katika Kichunguzi",
};

// ---------------------------------------------------------------------------
// French (Français)  —  LANG_FRENCH 0x0C
// ---------------------------------------------------------------------------
static constexpr Strings kFr = {
    L"Trier par",
    L"Date de modification \u2193 (plus r\u00e9cent)",
    L"Date de modification \u2191 (plus ancien)",
    L"Date de cr\u00e9ation \u2193 (plus r\u00e9cent)",
    L"Date de cr\u00e9ation \u2191 (plus ancien)",
    L"Nom A \u2192 Z",
    L"Nom Z \u2192 A",
    L"Nb max.",
    L"Animation",
    L"\u00c9ventail", L"Glissement", L"Ressort", L"Fondu", L"Aucune",
    L"Inclure les dossiers",
    L"Afficher les extensions",
    L"Changer de dossier\u2026",
    L"Quitter FanFolder",
    L"Ouvrir : ",
    L"S\u00e9lectionner un dossier \u00e0 surveiller",
    L"Dossier",
    L"T\u00e9l\u00e9chargements", L"R\u00e9cent (Explorateur)", L"Bureau", L"Documents", L"Fichiers r\u00e9cents", L"R\u00e9cent", L"Documents r\u00e9cents Office 365", L"Parcourir\u2026",
    L"Ouvrir dans l\u2019Explorateur",
};

// ---------------------------------------------------------------------------
// Italian (Italiano)  —  LANG_ITALIAN 0x10
// ---------------------------------------------------------------------------
static constexpr Strings kIt = {
    L"Ordina per",
    L"Data modifica \u2193 (pi\u00f9 recente)",
    L"Data modifica \u2191 (pi\u00f9 vecchio)",
    L"Data creazione \u2193 (pi\u00f9 recente)",
    L"Data creazione \u2191 (pi\u00f9 vecchio)",
    L"Nome A \u2192 Z",
    L"Nome Z \u2192 A",
    L"Elementi max",
    L"Animazione",
    L"Ventaglio", L"Scorrimento", L"Molla", L"Dissolvenza", L"Nessuna",
    L"Includi cartelle",
    L"Mostra estensioni file",
    L"Cambia cartella\u2026",
    L"Esci da FanFolder",
    L"Apri: ",
    L"Seleziona cartella da monitorare",
    L"Cartella",
    L"Download", L"Recenti (Esplora risorse)", L"Desktop", L"Documenti", L"File recenti", L"Recenti", L"Documenti recenti di Office 365", L"Sfoglia\u2026",
    L"Apri in Esplora risorse",
};

// ---------------------------------------------------------------------------
// Spanish (Español)  —  LANG_SPANISH 0x0A
// ---------------------------------------------------------------------------
static constexpr Strings kEs = {
    L"Ordenar por",
    L"Fecha modificaci\u00f3n \u2193 (m\u00e1s reciente)",
    L"Fecha modificaci\u00f3n \u2191 (m\u00e1s antigua)",
    L"Fecha creaci\u00f3n \u2193 (m\u00e1s reciente)",
    L"Fecha creaci\u00f3n \u2191 (m\u00e1s antigua)",
    L"Nombre A \u2192 Z",
    L"Nombre Z \u2192 A",
    L"M\u00e1x. elementos",
    L"Animaci\u00f3n",
    L"Abanico", L"Deslizamiento", L"Resorte", L"Fundido", L"Ninguna",
    L"Incluir carpetas",
    L"Mostrar extensiones",
    L"Cambiar carpeta\u2026",
    L"Salir de FanFolder",
    L"Abrir: ",
    L"Seleccionar carpeta a vigilar",
    L"Carpeta",
    L"Descargas", L"Reciente (Explorador)", L"Escritorio", L"Documentos", L"Archivos recientes", L"Reciente", L"Documentos recientes de Office 365", L"Examinar\u2026",
    L"Abrir en el Explorador",
};

// ---------------------------------------------------------------------------
// Portuguese (Português)  —  LANG_PORTUGUESE 0x16
// ---------------------------------------------------------------------------
static constexpr Strings kPt = {
    L"Ordenar por",
    L"Data modifica\u00e7\u00e3o \u2193 (mais recente)",
    L"Data modifica\u00e7\u00e3o \u2191 (mais antigo)",
    L"Data cria\u00e7\u00e3o \u2193 (mais recente)",
    L"Data cria\u00e7\u00e3o \u2191 (mais antigo)",
    L"Nome A \u2192 Z",
    L"Nome Z \u2192 A",
    L"M\u00e1x. itens",
    L"Anima\u00e7\u00e3o",
    L"Leque", L"Deslizar", L"Mola", L"Efeito", L"Nenhuma",
    L"Incluir pastas",
    L"Mostrar extens\u00f5es",
    L"Alterar pasta\u2026",
    L"Sair do FanFolder",
    L"Abrir: ",
    L"Selecionar pasta para monitorar",
    L"Pasta",
    L"Downloads", L"Recente (Explorador)", L"Ambiente de trabalho", L"Documentos", L"Ficheiros recentes", L"Recentes", L"Documentos recentes do Office 365", L"Procurar\u2026",
    L"Abrir no Explorador",
};

// ---------------------------------------------------------------------------
// Language lookup
// ---------------------------------------------------------------------------
const Strings& GetStrings() {
    const LANGID lang = PRIMARYLANGID(GetUserDefaultUILanguage());
    switch (lang) {
    case 0x06: return kDa;   // Danish
    case 0x1D: return kSv;   // Swedish
    case 0x14: return kNo;   // Norwegian
    case 0x07: return kDe;   // German
    case 0x13: return kNl;   // Dutch
    case 0x15: return kPl;   // Polish
    case 0x01: return kAr;   // Arabic
    case 0x04: return kZh;   // Chinese
    case 0x11: return kJa;   // Japanese
    case 0x12: return kKo;   // Korean
    case 0x41: return kSw;   // Swahili
    case 0x0C: return kFr;   // French
    case 0x10: return kIt;   // Italian
    case 0x0A: return kEs;   // Spanish
    case 0x16: return kPt;   // Portuguese
    default:   return kEn;   // English (fallback)
    }
}
