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
    L"Fan",
    L"Glide",
    L"Spring",
    L"Fade",
    L"None",
    L"Include folders",
    L"Show file extensions",
    L"Change folder\u2026",
    L"Exit FanFolder",
    L"Open: ",
    L"Select folder to watch",
    L"Folder",
    L"Downloads",
    L"Desktop",
    L"Documents",
    L"Recent files",
    L"Recent Office 365 documents",
    L"Browse\u2026",
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
    L"Vifte",
    L"Gli",
    L"Fjederkraft",
    L"Tone ind",
    L"Ingen",
    L"Inkluder mapper",
    L"Vis filtypenavne",
    L"Skift mappe\u2026",
    L"Afslut FanFolder",
    L"\u00c5bn: ",
    L"V\u00e6lg mappe at overv\u00e5ge",
    L"Mappe",
    L"Overf\u00f8rsler",
    L"Skrivebord",
    L"Dokumenter",
    L"Seneste filer",
    L"Seneste Office 365 dokumenter",
    L"Gennemse\u2026",
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
    L"Fl\u00e4kt",
    L"Glid",
    L"Fj\u00e4der",
    L"Tona in",
    L"Ingen",
    L"Inkludera mappar",
    L"Visa filnamnstill\u00e4gg",
    L"Byt mapp\u2026",
    L"Avsluta FanFolder",
    L"\u00d6ppna: ",
    L"V\u00e4lj mapp att \u00f6vervaka",
    L"Mapp",
    L"Nedladdningar",
    L"Skrivbord",
    L"Dokument",
    L"Senaste filer",
    L"Senaste Office 365-dokument",
    L"Bl\u00e4ddra\u2026",
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
    L"Vifte",
    L"Gli",
    L"Fj\u00e6r",
    L"Ton inn",
    L"Ingen",
    L"Inkluder mapper",
    L"Vis filendelser",
    L"Bytt mappe\u2026",
    L"Avslutt FanFolder",
    L"\u00c5pne: ",
    L"Velg mappe \u00e5 overv\u00e5ke",
    L"Mappe",
    L"Nedlastinger",
    L"Skrivebord",
    L"Dokumenter",
    L"Siste filer",
    L"Siste Office 365-dokumenter",
    L"Bla gjennom\u2026",
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
    L"F\u00e4cher",
    L"Gleiten",
    L"Feder",
    L"Einblenden",
    L"Keine",
    L"Ordner einschlie\u00dfen",
    L"Dateierweiterungen anzeigen",
    L"Ordner wechseln\u2026",
    L"FanFolder beenden",
    L"\u00d6ffnen: ",
    L"Ordner zum \u00dcberwachen ausw\u00e4hlen",
    L"Ordner",
    L"Downloads",
    L"Desktop",
    L"Dokumente",
    L"Zuletzt ge\u00f6ffnete Dateien",
    L"Zuletzt ge\u00f6ffnete Office 365-Dokumente",
    L"Durchsuchen\u2026",
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
    L"Waaier",
    L"Glijden",
    L"Veer",
    L"Vervagen",
    L"Geen",
    L"Mappen opnemen",
    L"Bestandsextensies weergeven",
    L"Map wijzigen\u2026",
    L"FanFolder afsluiten",
    L"Openen: ",
    L"Map selecteren om te bekijken",
    L"Map",
    L"Downloads",
    L"Bureaublad",
    L"Documenten",
    L"Recente bestanden",
    L"Recente Office 365-documenten",
    L"Bladeren\u2026",
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
    L"Wachlarz",
    L"\u015alizg",
    L"Spr\u0119\u017cyna",
    L"Zanikanie",
    L"Brak",
    L"Uwzgl\u0119dnij foldery",
    L"Poka\u017c rozszerzenia plik\u00f3w",
    L"Zmie\u0144 folder\u2026",
    L"Zamknij FanFolder",
    L"Otw\u00f3rz: ",
    L"Wybierz folder do \u015bledzenia",
    L"Folder",
    L"Pobrane",
    L"Pulpit",
    L"Dokumenty",
    L"Ostatnie pliki",
    L"Ostatnie dokumenty Office 365",
    L"Przegl\u0105daj\u2026",
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
    L"\u0645\u0631\u0648\u062d\u0629",
    L"\u0627\u0646\u0632\u0644\u0627\u0642",
    L"\u0646\u0627\u0628\u0636",
    L"\u062a\u0644\u0627\u0634\u064a",
    L"\u0644\u0627 \u0634\u064a\u0621",
    L"\u062a\u0636\u0645\u064a\u0646 \u0627\u0644\u0645\u062c\u0644\u062f\u0627\u062a",
    L"\u0625\u0638\u0647\u0627\u0631 \u0627\u0645\u062a\u062f\u0627\u062f\u0627\u062a \u0627\u0644\u0645\u0644\u0641\u0627\u062a",
    L"\u062a\u063a\u064a\u064a\u0631 \u0627\u0644\u0645\u062c\u0644\u062f\u2026",
    L"\u0625\u0646\u0647\u0627\u0621 FanFolder",
    L"\u0641\u062a\u062d: ",
    L"\u0627\u062e\u062a\u0631 \u0645\u062c\u0644\u062f\u064b\u0627 \u0644\u0644\u0645\u0631\u0627\u0642\u0628\u0629",
    L"\u0627\u0644\u0645\u062c\u0644\u062f",
    L"\u0627\u0644\u062a\u0646\u0632\u064a\u0644\u0627\u062a",
    L"\u0633\u0637\u062d \u0627\u0644\u0645\u0643\u062a\u0628",
    L"\u0627\u0644\u0645\u0633\u062a\u0646\u062f\u0627\u062a",
    L"\u0627\u0644\u0645\u0644\u0641\u0627\u062a \u0627\u0644\u0623\u062e\u064a\u0631\u0629",
    L"\u0645\u0633\u062a\u0646\u062f\u0627\u062a Office 365 \u0627\u0644\u0623\u062e\u064a\u0631\u0629",
    L"\u0627\u0633\u062a\u0639\u0631\u0627\u0636\u2026",
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
    L"\u6247\u5f62",
    L"\u6ed1\u52a8",
    L"\u5f39\u7c27",
    L"\u6de1\u5165",
    L"\u65e0",
    L"\u5305\u542b\u6587\u4ef6\u5939",
    L"\u663e\u793a\u6587\u4ef6\u6269\u5c55\u540d",
    L"\u66f4\u6539\u6587\u4ef6\u5939\u2026",
    L"\u9000\u51fa FanFolder",
    L"\u6253\u5f00\uff1a",
    L"\u9009\u62e9\u8981\u76d1\u89c6\u7684\u6587\u4ef6\u5939",
    L"\u6587\u4ef6\u5939",
    L"\u4e0b\u8f7d",
    L"\u684c\u9762",
    L"\u6587\u6863",
    L"\u6700\u8fd1\u7684\u6587\u4ef6",
    L"\u6700\u8fd1\u7684 Office 365 \u6587\u6863",
    L"\u6d4f\u89c8\u2026",
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
    L"\u30d5\u30a1\u30f3",
    L"\u30b0\u30e9\u30a4\u30c9",
    L"\u30b9\u30d7\u30ea\u30f3\u30b0",
    L"\u30d5\u30a7\u30fc\u30c9",
    L"\u306a\u3057",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u542b\u3081\u308b",
    L"\u62e1\u5f35\u5b50\u3092\u8868\u793a",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u5909\u66f4\u2026",
    L"FanFolder \u3092\u7d42\u4e86",
    L"\u958b\u304f\uff1a",
    L"\u76e3\u8996\u3059\u308b\u30d5\u30a9\u30eb\u30c0\u30fc\u3092\u9078\u629e",
    L"\u30d5\u30a9\u30eb\u30c0\u30fc",
    L"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9",
    L"\u30c7\u30b9\u30af\u30c8\u30c3\u30d7",
    L"\u30c9\u30ad\u30e5\u30e1\u30f3\u30c8",
    L"\u6700\u8fd1\u306e\u30d5\u30a1\u30a4\u30eb",
    L"\u6700\u8fd1\u306e Office 365 \u30c9\u30ad\u30e5\u30e1\u30f3\u30c8",
    L"\u53c2\u7167\u2026",
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
    L"\ud31c",
    L"\uae00\ub77c\uc774\ub4dc",
    L"\uc2a4\ud504\ub9c1",
    L"\ud398\uc774\ub4dc",
    L"\uc5c6\uc74c",
    L"\ud3f4\ub354 \ud3ec\ud568",
    L"\ud30c\uc77c \ud655\uc7a5\uc790 \ud45c\uc2dc",
    L"\ud3f4\ub354 \ubcc0\uacbd\u2026",
    L"FanFolder \uc885\ub8cc",
    L"\uc5f4\uae30: ",
    L"\uac10\uc2dc\ud560 \ud3f4\ub354 \uc120\ud0dd",
    L"\ud3f4\ub354",
    L"\ub2e4\uc6b4\ub85c\ub4dc",
    L"\ubc14\ud0d5 \ud654\uba74",
    L"\ubb38\uc11c",
    L"\ucd5c\uadfc \ud30c\uc77c",
    L"\ucd5c\uadfc Office 365 \ubb38\uc11c",
    L"\ucc3e\uc544\ubcf4\uae30\u2026",
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
    L"Shabiki",
    L"Teleza",
    L"Chemchemi",
    L"Fifia",
    L"Hakuna",
    L"Jumuisha folda",
    L"Onyesha viongezeo vya faili",
    L"Badilisha folda\u2026",
    L"Funga FanFolder",
    L"Fungua: ",
    L"Chagua folda ya kufuatilia",
    L"Folda",
    L"Vipakuliwa",
    L"Eneo-kazi",
    L"Nyaraka",
    L"Faili za hivi karibuni",
    L"Hati za hivi karibuni za Office 365",
    L"Vinjari\u2026",
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
    L"\u00c9ventail",
    L"Glissement",
    L"Ressort",
    L"Fondu",
    L"Aucune",
    L"Inclure les dossiers",
    L"Afficher les extensions",
    L"Changer de dossier\u2026",
    L"Quitter FanFolder",
    L"Ouvrir : ",
    L"S\u00e9lectionner un dossier \u00e0 surveiller",
    L"Dossier",
    L"T\u00e9l\u00e9chargements",
    L"Bureau",
    L"Documents",
    L"Fichiers r\u00e9cents",
    L"Documents r\u00e9cents Office 365",
    L"Parcourir\u2026",
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
    L"Ventaglio",
    L"Scorrimento",
    L"Molla",
    L"Dissolvenza",
    L"Nessuna",
    L"Includi cartelle",
    L"Mostra estensioni file",
    L"Cambia cartella\u2026",
    L"Esci da FanFolder",
    L"Apri: ",
    L"Seleziona cartella da monitorare",
    L"Cartella",
    L"Download",
    L"Desktop",
    L"Documenti",
    L"File recenti",
    L"Documenti recenti di Office 365",
    L"Sfoglia\u2026",
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
    L"Abanico",
    L"Deslizamiento",
    L"Resorte",
    L"Fundido",
    L"Ninguna",
    L"Incluir carpetas",
    L"Mostrar extensiones",
    L"Cambiar carpeta\u2026",
    L"Salir de FanFolder",
    L"Abrir: ",
    L"Seleccionar carpeta a vigilar",
    L"Carpeta",
    L"Descargas",
    L"Escritorio",
    L"Documentos",
    L"Archivos recientes",
    L"Documentos recientes de Office 365",
    L"Examinar\u2026",
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
    L"Leque",
    L"Deslizar",
    L"Mola",
    L"Efeito",
    L"Nenhuma",
    L"Incluir pastas",
    L"Mostrar extens\u00f5es",
    L"Alterar pasta\u2026",
    L"Sair do FanFolder",
    L"Abrir: ",
    L"Selecionar pasta para monitorar",
    L"Pasta",
    L"Downloads",
    L"Ambiente de trabalho",
    L"Documentos",
    L"Ficheiros recentes",
    L"Documentos recentes do Office 365",
    L"Procurar\u2026",
    L"Abrir no Explorador",
};

// ---------------------------------------------------------------------------
// Russian (Русский)  —  LANG_RUSSIAN 0x19
// ---------------------------------------------------------------------------
static constexpr Strings kRu = {
    L"\u0421\u043e\u0440\u0442\u0438\u0440\u043e\u0432\u0430\u0442\u044c \u043f\u043e",
    L"\u0414\u0430\u0442\u0430 \u0438\u0437\u043c\u0435\u043d\u0435\u043d\u0438\u044f \u2193 (\u0441\u043d\u0430\u0447\u0430\u043b\u0430 \u043d\u043e\u0432\u044b\u0435)",
    L"\u0414\u0430\u0442\u0430 \u0438\u0437\u043c\u0435\u043d\u0435\u043d\u0438\u044f \u2191 (\u0441\u043d\u0430\u0447\u0430\u043b\u0430 \u0441\u0442\u0430\u0440\u044b\u0435)",
    L"\u0414\u0430\u0442\u0430 \u0441\u043e\u0437\u0434\u0430\u043d\u0438\u044f \u2193 (\u0441\u043d\u0430\u0447\u0430\u043b\u0430 \u043d\u043e\u0432\u044b\u0435)",
    L"\u0414\u0430\u0442\u0430 \u0441\u043e\u0437\u0434\u0430\u043d\u0438\u044f \u2191 (\u0441\u043d\u0430\u0447\u0430\u043b\u0430 \u0441\u0442\u0430\u0440\u044b\u0435)",
    L"\u0418\u043c\u044f \u0410 \u2192 \u042f",
    L"\u0418\u043c\u044f \u042f \u2192 \u0410",
    L"\u041c\u0430\u043a\u0441. \u044d\u043b\u0435\u043c\u0435\u043d\u0442\u043e\u0432",
    L"\u0410\u043d\u0438\u043c\u0430\u0446\u0438\u044f",
    L"\u0412\u0435\u0435\u0440",
    L"\u0421\u043a\u043e\u043b\u044c\u0436\u0435\u043d\u0438\u0435",
    L"\u041f\u0440\u0443\u0436\u0438\u043d\u0430",
    L"\u0417\u0430\u0442\u0443\u0445\u0430\u043d\u0438\u0435",
    L"\u041d\u0435\u0442",
    L"\u0412\u043a\u043b\u044e\u0447\u0438\u0442\u044c \u043f\u0430\u043f\u043a\u0438",
    L"\u041f\u043e\u043a\u0430\u0437\u044b\u0432\u0430\u0442\u044c \u0440\u0430\u0441\u0448\u0438\u0440\u0435\u043d\u0438\u044f",
    L"\u0421\u043c\u0435\u043d\u0438\u0442\u044c \u043f\u0430\u043f\u043a\u0443\u2026",
    L"\u0412\u044b\u0439\u0442\u0438 \u0438\u0437 FanFolder",
    L"\u041e\u0442\u043a\u0440\u044b\u0442\u044c: ",
    L"\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u043f\u0430\u043f\u043a\u0443 \u0434\u043b\u044f \u043e\u0442\u0441\u043b\u0435\u0436\u0438\u0432\u0430\u043d\u0438\u044f",
    L"\u041f\u0430\u043f\u043a\u0430",
    L"\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0438",
    L"\u0420\u0430\u0431\u043e\u0447\u0438\u0439 \u0441\u0442\u043e\u043b",
    L"\u0414\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u044b",
    L"\u041d\u0435\u0434\u0430\u0432\u043d\u0438\u0435 \u0444\u0430\u0439\u043b\u044b",
    L"\u041d\u0435\u0434\u0430\u0432\u043d\u0438\u0435 \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u044b Office 365",
    L"\u041e\u0431\u0437\u043e\u0440\u2026",
    L"\u041e\u0442\u043a\u0440\u044b\u0442\u044c \u0432 \u041f\u0440\u043e\u0432\u043e\u0434\u043d\u0438\u043a\u0435",
};

// ---------------------------------------------------------------------------
// Hindi (हिन्दी)  —  LANG_HINDI 0x39
// ---------------------------------------------------------------------------
static constexpr Strings kHi = {
    L"\u0915\u094d\u0930\u092e\u092c\u0926\u094d\u0927 \u0915\u0930\u0947\u0902",
    L"\u0938\u0902\u0936\u094b\u0927\u0928 \u0924\u093f\u0925\u093f \u2193 (\u0928\u0935\u0940\u0928\u0924\u092e \u092a\u0939\u0932\u0947)",
    L"\u0938\u0902\u0936\u094b\u0927\u0928 \u0924\u093f\u0925\u093f \u2191 (\u092a\u0941\u0930\u093e\u0928\u0940 \u092a\u0939\u0932\u0947)",
    L"\u0928\u093f\u0930\u094d\u092e\u093e\u0923 \u0924\u093f\u0925\u093f \u2193 (\u0928\u0935\u0940\u0928\u0924\u092e \u092a\u0939\u0932\u0947)",
    L"\u0928\u093f\u0930\u094d\u092e\u093e\u0923 \u0924\u093f\u0925\u093f \u2191 (\u092a\u0941\u0930\u093e\u0928\u0940 \u092a\u0939\u0932\u0947)",
    L"\u0928\u093e\u092e A \u2192 Z",
    L"\u0928\u093e\u092e Z \u2192 A",
    L"\u0905\u0927\u093f\u0915\u0924\u092e \u0906\u0907\u091f\u092e",
    L"\u090f\u0928\u093f\u092e\u0947\u0936\u0928",
    L"\u092a\u0902\u0916\u093e",
    L"\u0917\u094d\u0932\u093e\u0907\u0921",
    L"\u0938\u094d\u092a\u094d\u0930\u093f\u0902\u0917",
    L"\u092b\u0940\u0915\u093e",
    L"\u0915\u094b\u0908 \u0928\u0939\u0940\u0902",
    L"\u095e\u093c\u094b\u0932\u094d\u0921\u0930 \u0936\u093e\u092e\u093f\u0932 \u0915\u0930\u0947\u0902",
    L"\u095e\u093c\u093e\u0907\u0932 \u090f\u0915\u094d\u0938\u091f\u0947\u0902\u0936\u0928 \u0926\u093f\u0916\u093e\u090f\u0902",
    L"\u095e\u093c\u094b\u0932\u094d\u0921\u0930 \u092c\u0926\u0932\u0947\u0902\u2026",
    L"FanFolder \u0938\u0947 \u092c\u093e\u0939\u0930 \u0928\u093f\u0915\u0932\u0947\u0902",
    L"\u0916\u094b\u0932\u0947\u0902: ",
    L"\u0926\u0947\u0916\u0928\u0947 \u0915\u0947 \u0932\u093f\u090f \u095e\u093c\u094b\u0932\u094d\u0921\u0930 \u091a\u0941\u0928\u0947\u0902",
    L"\u095e\u093c\u094b\u0932\u094d\u0921\u0930",
    L"\u0921\u093e\u0909\u0928\u0932\u094b\u0921",
    L"\u0921\u0947\u0938\u094d\u0915\u091f\u0949\u092a",
    L"\u0926\u0938\u094d\u0924\u093e\u0935\u0947\u095b\u093c",
    L"\u0939\u093e\u0932 \u0915\u0940 \u095e\u093c\u093e\u0907\u0932\u0947\u0902",
    L"\u0939\u093e\u0932 \u0915\u0947 Office 365 \u0926\u0938\u094d\u0924\u093e\u0935\u0947\u095b\u093c",
    L"\u092c\u094d\u0930\u093e\u0909\u095b\u093c\u2026",
    L"Explorer \u092e\u0947\u0902 \u0916\u094b\u0932\u0947\u0902",
};

// ---------------------------------------------------------------------------
// Turkish (Türkçe)  —  LANG_TURKISH 0x1F
// ---------------------------------------------------------------------------
static constexpr Strings kTr = {
    L"S\u0131rala",
    L"De\u011fi\u015ftirilme tarihi \u2193 (en yeni \u00f6nce)",
    L"De\u011fi\u015ftirilme tarihi \u2191 (en eski \u00f6nce)",
    L"Olu\u015fturulma tarihi \u2193 (en yeni \u00f6nce)",
    L"Olu\u015fturulma tarihi \u2191 (en eski \u00f6nce)",
    L"Ad A \u2192 Z",
    L"Ad Z \u2192 A",
    L"Maks. \u00f6\u011fe",
    L"Animasyon",
    L"Yelpaze",
    L"Kayma",
    L"Yay",
    L"Solma",
    L"Yok",
    L"Klas\u00f6rleri dahil et",
    L"Dosya uzant\u0131lar\u0131n\u0131 g\u00f6ster",
    L"Klas\u00f6r\u00fc de\u011fi\u015ftir\u2026",
    L"FanFolder\u2019dan \u00e7\u0131k",
    L"A\u00e7: ",
    L"\u0130zlenecek klas\u00f6r\u00fc se\u00e7",
    L"Klas\u00f6r",
    L"\u0130ndirilenler",
    L"Masa\u00fcst\u00fc",
    L"Belgeler",
    L"Son kullan\u0131lan dosyalar",
    L"Son kullan\u0131lan Office 365 belgeleri",
    L"G\u00f6zat\u2026",
    L"Gezgin\u2019de a\u00e7",
};

// ---------------------------------------------------------------------------
// Hebrew (עברית)  —  LANG_HEBREW 0x0D
// ---------------------------------------------------------------------------
static constexpr Strings kHe = {
    L"\u05de\u05d9\u05d9\u05df \u05dc\u05e4\u05d9",
    L"\u05ea\u05d0\u05e8\u05d9\u05da \u05e9\u05d9\u05e0\u05d5\u05d9 \u2193 (\u05d4\u05d7\u05d3\u05e9 \u05d1\u05d9\u05d5\u05ea\u05e8)",
    L"\u05ea\u05d0\u05e8\u05d9\u05da \u05e9\u05d9\u05e0\u05d5\u05d9 \u2191 (\u05d4\u05d9\u05e9\u05df \u05d1\u05d9\u05d5\u05ea\u05e8)",
    L"\u05ea\u05d0\u05e8\u05d9\u05da \u05d9\u05e6\u05d9\u05e8\u05d4 \u2193 (\u05d4\u05d7\u05d3\u05e9 \u05d1\u05d9\u05d5\u05ea\u05e8)",
    L"\u05ea\u05d0\u05e8\u05d9\u05da \u05d9\u05e6\u05d9\u05e8\u05d4 \u2191 (\u05d4\u05d9\u05e9\u05df \u05d1\u05d9\u05d5\u05ea\u05e8)",
    L"\u05e9\u05dd \u05d0 \u2192 \u05ea",
    L"\u05e9\u05dd \u05ea \u2192 \u05d0",
    L"\u05de\u05e7\u05e1. \u05e4\u05e8\u05d9\u05d8\u05d9\u05dd",
    L"\u05d0\u05e0\u05d9\u05de\u05e6\u05d9\u05d4",
    L"\u05de\u05e0\u05d9\u05e4\u05d4",
    L"\u05d4\u05d7\u05dc\u05e7\u05d4",
    L"\u05e7\u05e4\u05d9\u05e5",
    L"\u05d4\u05ea\u05e2\u05de\u05e2\u05de\u05d5\u05ea",
    L"\u05dc\u05dc\u05d0",
    L"\u05db\u05dc\u05d5\u05dc \u05ea\u05d9\u05e7\u05d9\u05d5\u05ea",
    L"\u05d4\u05e6\u05d2 \u05e1\u05d9\u05d5\u05de\u05d5\u05ea \u05e7\u05d1\u05e6\u05d9\u05dd",
    L"\u05d4\u05d7\u05dc\u05e3 \u05ea\u05d9\u05e7\u05d9\u05d4\u2026",
    L"\u05e6\u05d0 \u05de-FanFolder",
    L"\u05e4\u05ea\u05d7: ",
    L"\u05d1\u05d7\u05e8 \u05ea\u05d9\u05e7\u05d9\u05d4 \u05dc\u05de\u05e2\u05e7\u05d1",
    L"\u05ea\u05d9\u05e7\u05d9\u05d4",
    L"\u05d4\u05d5\u05e8\u05d3\u05d5\u05ea",
    L"\u05e9\u05d5\u05dc\u05d7\u05df \u05e2\u05d1\u05d5\u05d3\u05d4",
    L"\u05de\u05e1\u05de\u05db\u05d9\u05dd",
    L"\u05e7\u05d1\u05e6\u05d9\u05dd \u05d0\u05d7\u05e8\u05d5\u05e0\u05d9\u05dd",
    L"\u05de\u05e1\u05de\u05db\u05d9 Office 365 \u05d0\u05d7\u05e8\u05d5\u05e0\u05d9\u05dd",
    L"\u05e2\u05d9\u05d5\u05df\u2026",
    L"\u05e4\u05ea\u05d7 \u05d1\u05e1\u05d9\u05d9\u05e8",
};

// ---------------------------------------------------------------------------
// Czech (Čeština)  —  LANG_CZECH 0x05
// ---------------------------------------------------------------------------
static constexpr Strings kCs = {
    L"Se\u0159adit podle",
    L"Datum zm\u011bny \u2193 (nejnov\u011bj\u0161\u00ed)",
    L"Datum zm\u011bny \u2191 (nejstar\u0161\u00ed)",
    L"Datum vytvo\u0159en\u00ed \u2193 (nejnov\u011bj\u0161\u00ed)",
    L"Datum vytvo\u0159en\u00ed \u2191 (nejstar\u0161\u00ed)",
    L"N\u00e1zev A \u2192 Z",
    L"N\u00e1zev Z \u2192 A",
    L"Max. polo\u017eek",
    L"Animace",
    L"V\u011bj\u00ed\u0159",
    L"Klouz\u00e1n\u00ed",
    L"Pru\u017eina",
    L"Prol\u00edn\u00e1n\u00ed",
    L"\u017d\u00e1dn\u00e1",
    L"Zahrnout slo\u017eky",
    L"Zobrazit p\u0159\u00edpony soubor\u016f",
    L"Zm\u011bnit slo\u017eku\u2026",
    L"Ukon\u010dit FanFolder",
    L"Otev\u0159\u00edt: ",
    L"Vyberte slo\u017eku ke sledov\u00e1n\u00ed",
    L"Slo\u017eka",
    L"Sta\u017een\u00e9",
    L"Plocha",
    L"Dokumenty",
    L"Ned\u00e1vn\u00e9 soubory",
    L"Ned\u00e1vn\u00e9 dokumenty Office 365",
    L"Proch\u00e1zet\u2026",
    L"Otev\u0159\u00edt v Pr\u016fzkumn\u00edku",
};

// ---------------------------------------------------------------------------
// Finnish (Suomi)  —  LANG_FINNISH 0x0B
// ---------------------------------------------------------------------------
static constexpr Strings kFi = {
    L"Lajitteluperuste",
    L"Muokkausp\u00e4iv\u00e4 \u2193 (uusin ensin)",
    L"Muokkausp\u00e4iv\u00e4 \u2191 (vanhin ensin)",
    L"Luontip\u00e4iv\u00e4 \u2193 (uusin ensin)",
    L"Luontip\u00e4iv\u00e4 \u2191 (vanhin ensin)",
    L"Nimi A \u2192 \u00d6",
    L"Nimi \u00d6 \u2192 A",
    L"Enimm\u00e4ism\u00e4\u00e4r\u00e4",
    L"Animaatio",
    L"Viuhka",
    L"Liuku",
    L"Jousi",
    L"H\u00e4ivytys",
    L"Ei mit\u00e4\u00e4n",
    L"Sis\u00e4llyt\u00e4 kansiot",
    L"N\u00e4yt\u00e4 tiedostotunnisteet",
    L"Vaihda kansio\u2026",
    L"Sulje FanFolder",
    L"Avaa: ",
    L"Valitse seurattava kansio",
    L"Kansio",
    L"Lataukset",
    L"Ty\u00f6p\u00f6yt\u00e4",
    L"Tiedostot",
    L"Viimeisimm\u00e4t tiedostot",
    L"Viimeisimm\u00e4t Office 365 -asiakirjat",
    L"Selaa\u2026",
    L"Avaa Resurssienhallinnassa",
};

// ---------------------------------------------------------------------------
// Hungarian (Magyar)  —  LANG_HUNGARIAN 0x0E
// ---------------------------------------------------------------------------
static constexpr Strings kHu = {
    L"Rendez\u00e9s",
    L"M\u00f3dos\u00edt\u00e1s d\u00e1tuma \u2193 (legfrissebb el\u0151re)",
    L"M\u00f3dos\u00edt\u00e1s d\u00e1tuma \u2191 (legr\u00e9gebbi el\u0151re)",
    L"L\u00e9trehoz\u00e1s d\u00e1tuma \u2193 (legfrissebb el\u0151re)",
    L"L\u00e9trehoz\u00e1s d\u00e1tuma \u2191 (legr\u00e9gebbi el\u0151re)",
    L"N\u00e9v A \u2192 Z",
    L"N\u00e9v Z \u2192 A",
    L"Max. elemek",
    L"Anim\u00e1ci\u00f3",
    L"Legyez\u0151",
    L"Cs\u00fasz\u00e1s",
    L"Rug\u00f3",
    L"\u00c1tt\u0171n\u00e9s",
    L"Nincs",
    L"Mapp\u00e1k felv\u00e9tele",
    L"F\u00e1jlkiterjeszt\u00e9sek megjelen\u00edt\u00e9se",
    L"Mappa m\u00f3dos\u00edt\u00e1sa\u2026",
    L"Kil\u00e9p\u00e9s a FanFolderb\u0151l",
    L"Megnyit\u00e1s: ",
    L"V\u00e1lassza ki a figyelend\u0151 mapp\u00e1t",
    L"Mappa",
    L"Let\u00f6lt\u00e9sek",
    L"Asztal",
    L"Dokumentumok",
    L"Legut\u00f3bbi f\u00e1jlok",
    L"Legut\u00f3bbi Office 365 dokumentumok",
    L"Tall\u00f3z\u00e1s\u2026",
    L"Megnyit\u00e1s Int\u00e9z\u0151ben",
};

// ---------------------------------------------------------------------------
// Greek (Ελληνικά)  —  LANG_GREEK 0x08
// ---------------------------------------------------------------------------
static constexpr Strings kEl = {
    L"\u03a4\u03b1\u03be\u03b9\u03bd\u03cc\u03bc\u03b7\u03c3\u03b7 \u03ba\u03b1\u03c4\u03ac",
    L"\u0397\u03bc. \u03c4\u03c1\u03bf\u03c0\u03bf\u03c0\u03bf\u03af\u03b7\u03c3\u03b7\u03c2 \u2193 (\u03bd\u03b5\u03cc\u03c4\u03b5\u03c1\u03b1 \u03c0\u03c1\u03ce\u03c4\u03b1)",
    L"\u0397\u03bc. \u03c4\u03c1\u03bf\u03c0\u03bf\u03c0\u03bf\u03af\u03b7\u03c3\u03b7\u03c2 \u2191 (\u03c0\u03b1\u03bb\u03b1\u03b9\u03cc\u03c4\u03b5\u03c1\u03b1 \u03c0\u03c1\u03ce\u03c4\u03b1)",
    L"\u0397\u03bc. \u03b4\u03b7\u03bc\u03b9\u03bf\u03c5\u03c1\u03b3\u03af\u03b1\u03c2 \u2193 (\u03bd\u03b5\u03cc\u03c4\u03b5\u03c1\u03b1 \u03c0\u03c1\u03ce\u03c4\u03b1)",
    L"\u0397\u03bc. \u03b4\u03b7\u03bc\u03b9\u03bf\u03c5\u03c1\u03b3\u03af\u03b1\u03c2 \u2191 (\u03c0\u03b1\u03bb\u03b1\u03b9\u03cc\u03c4\u03b5\u03c1\u03b1 \u03c0\u03c1\u03ce\u03c4\u03b1)",
    L"\u038c\u03bd\u03bf\u03bc\u03b1 \u0391 \u2192 \u03a9",
    L"\u038c\u03bd\u03bf\u03bc\u03b1 \u03a9 \u2192 \u0391",
    L"\u039c\u03ad\u03b3. \u03c3\u03c4\u03bf\u03b9\u03c7\u03b5\u03af\u03b1",
    L"\u039a\u03b9\u03bd\u03bf\u03cd\u03bc\u03b5\u03bd\u03b7 \u03b5\u03b9\u03ba\u03cc\u03bd\u03b1",
    L"\u0392\u03b5\u03bd\u03c4\u03ac\u03bb\u03b9\u03b1",
    L"\u039f\u03bb\u03af\u03c3\u03b8\u03b7\u03c3\u03b7",
    L"\u0395\u03bb\u03b1\u03c4\u03ae\u03c1\u03b9\u03bf",
    L"\u0395\u03be\u03b1\u03c3\u03b8\u03ad\u03bd\u03b7\u03c3\u03b7",
    L"\u039a\u03b1\u03bc\u03af\u03b1",
    L"\u03a3\u03c5\u03bc\u03c0\u03b5\u03c1\u03af\u03bb\u03b7\u03c8\u03b7 \u03c6\u03b1\u03ba\u03ad\u03bb\u03c9\u03bd",
    L"\u0395\u03bc\u03c6\u03ac\u03bd\u03b9\u03c3\u03b7 \u03b5\u03c0\u03b5\u03ba\u03c4\u03ac\u03c3\u03b5\u03c9\u03bd \u03b1\u03c1\u03c7\u03b5\u03af\u03c9\u03bd",
    L"\u0391\u03bb\u03bb\u03b1\u03b3\u03ae \u03c6\u03b1\u03ba\u03ad\u03bb\u03bf\u03c5\u2026",
    L"\u0388\u03be\u03bf\u03b4\u03bf\u03c2 \u03b1\u03c0\u03cc \u03c4\u03bf FanFolder",
    L"\u0386\u03bd\u03bf\u03b9\u03b3\u03bc\u03b1: ",
    L"\u0395\u03c0\u03b9\u03bb\u03ad\u03be\u03c4\u03b5 \u03c6\u03ac\u03ba\u03b5\u03bb\u03bf \u03b3\u03b9\u03b1 \u03c0\u03b1\u03c1\u03b1\u03ba\u03bf\u03bb\u03bf\u03cd\u03b8\u03b7\u03c3\u03b7",
    L"\u03a6\u03ac\u03ba\u03b5\u03bb\u03bf\u03c2",
    L"\u039b\u03ae\u03c8\u03b5\u03b9\u03c2",
    L"\u0395\u03c0\u03b9\u03c6\u03ac\u03bd\u03b5\u03b9\u03b1 \u03b5\u03c1\u03b3\u03b1\u03c3\u03af\u03b1\u03c2",
    L"\u0388\u03b3\u03b3\u03c1\u03b1\u03c6\u03b1",
    L"\u03a0\u03c1\u03cc\u03c3\u03c6\u03b1\u03c4\u03b1 \u03b1\u03c1\u03c7\u03b5\u03af\u03b1",
    L"\u03a0\u03c1\u03cc\u03c3\u03c6\u03b1\u03c4\u03b1 \u03ad\u03b3\u03b3\u03c1\u03b1\u03c6\u03b1 Office 365",
    L"\u0391\u03bd\u03b1\u03b6\u03ae\u03c4\u03b7\u03c3\u03b7\u2026",
    L"\u0386\u03bd\u03bf\u03b9\u03b3\u03bc\u03b1 \u03c3\u03c4\u03b7\u03bd \u0395\u03be\u03b5\u03c1\u03b5\u03cd\u03bd\u03b7\u03c3\u03b7",
};

// ---------------------------------------------------------------------------
// Vietnamese (Tiếng Việt)  —  LANG_VIETNAMESE 0x2A
// ---------------------------------------------------------------------------
static constexpr Strings kVi = {
    L"S\u1eafp x\u1ebfp theo",
    L"Ng\u00e0y s\u1eeda \u0111\u1ed5i \u2193 (m\u1edbi nh\u1ea5t tr\u01b0\u1edbc)",
    L"Ng\u00e0y s\u1eeda \u0111\u1ed5i \u2191 (c\u0169 nh\u1ea5t tr\u01b0\u1edbc)",
    L"Ng\u00e0y t\u1ea1o \u2193 (m\u1edbi nh\u1ea5t tr\u01b0\u1edbc)",
    L"Ng\u00e0y t\u1ea1o \u2191 (c\u0169 nh\u1ea5t tr\u01b0\u1edbc)",
    L"T\u00ean A \u2192 Z",
    L"T\u00ean Z \u2192 A",
    L"S\u1ed1 m\u1ee5c t\u1ed1i \u0111a",
    L"Ho\u1ea1t \u1ea3nh",
    L"Qu\u1ea1t",
    L"L\u01b0\u1edbt",
    L"L\u00f2 xo",
    L"M\u1edd d\u1ea7n",
    L"Kh\u00f4ng",
    L"Bao g\u1ed3m th\u01b0 m\u1ee5c",
    L"Hi\u1ec3n th\u1ecb ph\u1ea7n m\u1edf r\u1ed9ng",
    L"\u0110\u1ed5i th\u01b0 m\u1ee5c\u2026",
    L"Tho\u00e1t FanFolder",
    L"M\u1edf: ",
    L"Ch\u1ecdn th\u01b0 m\u1ee5c \u0111\u1ec3 theo d\u00f5i",
    L"Th\u01b0 m\u1ee5c",
    L"T\u1ea3i xu\u1ed1ng",
    L"M\u00e0n h\u00ecnh n\u1ec1n",
    L"T\u00e0i li\u1ec7u",
    L"T\u1ec7p g\u1ea7n \u0111\u00e2y",
    L"T\u00e0i li\u1ec7u Office 365 g\u1ea7n \u0111\u00e2y",
    L"Duy\u1ec7t\u2026",
    L"M\u1edf trong Explorer",
};

// ---------------------------------------------------------------------------
// Indonesian (Bahasa Indonesia)  —  LANG_INDONESIAN 0x21
// ---------------------------------------------------------------------------
static constexpr Strings kId = {
    L"Urutkan menurut",
    L"Tanggal diubah \u2193 (terbaru dahulu)",
    L"Tanggal diubah \u2191 (terlama dahulu)",
    L"Tanggal dibuat \u2193 (terbaru dahulu)",
    L"Tanggal dibuat \u2191 (terlama dahulu)",
    L"Nama A \u2192 Z",
    L"Nama Z \u2192 A",
    L"Maks. item",
    L"Animasi",
    L"Kipas",
    L"Meluncur",
    L"Pegas",
    L"Pudar",
    L"Tidak ada",
    L"Sertakan folder",
    L"Tampilkan ekstensi file",
    L"Ubah folder\u2026",
    L"Keluar dari FanFolder",
    L"Buka: ",
    L"Pilih folder untuk dipantau",
    L"Folder",
    L"Unduhan",
    L"Desktop",
    L"Dokumen",
    L"File terbaru",
    L"Dokumen Office 365 terbaru",
    L"Telusuri\u2026",
    L"Buka di Explorer",
};

// ---------------------------------------------------------------------------
// Ukrainian (Українська)  —  LANG_UKRAINIAN 0x22
// ---------------------------------------------------------------------------
static constexpr Strings kUk = {
    L"\u0421\u043e\u0440\u0442\u0443\u0432\u0430\u0442\u0438 \u0437\u0430",
    L"\u0414\u0430\u0442\u0430 \u0437\u043c\u0456\u043d\u0435\u043d\u043d\u044f \u2193 (\u043d\u043e\u0432\u0456\u0448\u0456 \u0441\u043f\u043e\u0447\u0430\u0442\u043a\u0443)",
    L"\u0414\u0430\u0442\u0430 \u0437\u043c\u0456\u043d\u0435\u043d\u043d\u044f \u2191 (\u0441\u0442\u0430\u0440\u0456\u0448\u0456 \u0441\u043f\u043e\u0447\u0430\u0442\u043a\u0443)",
    L"\u0414\u0430\u0442\u0430 \u0441\u0442\u0432\u043e\u0440\u0435\u043d\u043d\u044f \u2193 (\u043d\u043e\u0432\u0456\u0448\u0456 \u0441\u043f\u043e\u0447\u0430\u0442\u043a\u0443)",
    L"\u0414\u0430\u0442\u0430 \u0441\u0442\u0432\u043e\u0440\u0435\u043d\u043d\u044f \u2191 (\u0441\u0442\u0430\u0440\u0456\u0448\u0456 \u0441\u043f\u043e\u0447\u0430\u0442\u043a\u0443)",
    L"\u0406\u043c\u02bc\u044f \u0410 \u2192 \u042f",
    L"\u0406\u043c\u02bc\u044f \u042f \u2192 \u0410",
    L"\u041c\u0430\u043a\u0441. \u0435\u043b\u0435\u043c\u0435\u043d\u0442\u0456\u0432",
    L"\u0410\u043d\u0456\u043c\u0430\u0446\u0456\u044f",
    L"\u0412\u0456\u044f\u043b\u043e",
    L"\u041a\u043e\u0432\u0437\u0430\u043d\u043d\u044f",
    L"\u041f\u0440\u0443\u0436\u0438\u043d\u0430",
    L"\u0417\u0433\u0430\u0441\u0430\u043d\u043d\u044f",
    L"\u041d\u0435\u043c\u0430\u0454",
    L"\u0412\u043a\u043b\u044e\u0447\u0438\u0442\u0438 \u043f\u0430\u043f\u043a\u0438",
    L"\u041f\u043e\u043a\u0430\u0437\u0443\u0432\u0430\u0442\u0438 \u0440\u043e\u0437\u0448\u0438\u0440\u0435\u043d\u043d\u044f",
    L"\u0417\u043c\u0456\u043d\u0438\u0442\u0438 \u043f\u0430\u043f\u043a\u0443\u2026",
    L"\u0412\u0438\u0439\u0442\u0438 \u0437 FanFolder",
    L"\u0412\u0456\u0434\u043a\u0440\u0438\u0442\u0438: ",
    L"\u0412\u0438\u0431\u0435\u0440\u0456\u0442\u044c \u043f\u0430\u043f\u043a\u0443 \u0434\u043b\u044f \u0441\u0442\u0435\u0436\u0435\u043d\u043d\u044f",
    L"\u041f\u0430\u043f\u043a\u0430",
    L"\u0417\u0430\u0432\u0430\u043d\u0442\u0430\u0436\u0435\u043d\u043d\u044f",
    L"\u0420\u043e\u0431\u043e\u0447\u0438\u0439 \u0441\u0442\u0456\u043b",
    L"\u0414\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0438",
    L"\u041e\u0441\u0442\u0430\u043d\u043d\u0456 \u0444\u0430\u0439\u043b\u0438",
    L"\u041e\u0441\u0442\u0430\u043d\u043d\u0456 \u0434\u043e\u043a\u0443\u043c\u0435\u043d\u0442\u0438 Office 365",
    L"\u041e\u0433\u043b\u044f\u0434\u2026",
    L"\u0412\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u0432 \u041f\u0440\u043e\u0432\u0456\u0434\u043d\u0438\u043a\u0443",
};

// ---------------------------------------------------------------------------
// Romanian (Română)  —  LANG_ROMANIAN 0x18
// ---------------------------------------------------------------------------
static constexpr Strings kRo = {
    L"Sortare dup\u0103",
    L"Data modific\u0103rii \u2193 (cele mai noi)",
    L"Data modific\u0103rii \u2191 (cele mai vechi)",
    L"Data cre\u0103rii \u2193 (cele mai noi)",
    L"Data cre\u0103rii \u2191 (cele mai vechi)",
    L"Nume A \u2192 Z",
    L"Nume Z \u2192 A",
    L"Max. elemente",
    L"Anima\u021bie",
    L"Evantai",
    L"Glisare",
    L"Resort",
    L"Estompare",
    L"Niciuna",
    L"Includere foldere",
    L"Afi\u0219are extensii fi\u0219iere",
    L"Schimbare folder\u2026",
    L"Ie\u0219ire din FanFolder",
    L"Deschidere: ",
    L"Selecta\u021bi folderul de monitorizat",
    L"Folder",
    L"Desc\u0103rc\u0103ri",
    L"Desktop",
    L"Documente",
    L"Fi\u0219iere recente",
    L"Documente Office 365 recente",
    L"R\u0103sfoire\u2026",
    L"Deschidere \u00een Explorer",
};

// ---------------------------------------------------------------------------
// Thai (ไทย)  —  LANG_THAI 0x1E
// ---------------------------------------------------------------------------
static constexpr Strings kTh = {
    L"\u0e40\u0e23\u0e35\u0e22\u0e07\u0e15\u0e32\u0e21",
    L"\u0e27\u0e31\u0e19\u0e17\u0e35\u0e48\u0e41\u0e01\u0e49\u0e44\u0e02 \u2193 (\u0e43\u0e2b\u0e21\u0e48\u0e2a\u0e38\u0e14\u0e01\u0e48\u0e2d\u0e19)",
    L"\u0e27\u0e31\u0e19\u0e17\u0e35\u0e48\u0e41\u0e01\u0e49\u0e44\u0e02 \u2191 (\u0e40\u0e01\u0e48\u0e32\u0e2a\u0e38\u0e14\u0e01\u0e48\u0e2d\u0e19)",
    L"\u0e27\u0e31\u0e19\u0e17\u0e35\u0e48\u0e2a\u0e23\u0e49\u0e32\u0e07 \u2193 (\u0e43\u0e2b\u0e21\u0e48\u0e2a\u0e38\u0e14\u0e01\u0e48\u0e2d\u0e19)",
    L"\u0e27\u0e31\u0e19\u0e17\u0e35\u0e48\u0e2a\u0e23\u0e49\u0e32\u0e07 \u2191 (\u0e40\u0e01\u0e48\u0e32\u0e2a\u0e38\u0e14\u0e01\u0e48\u0e2d\u0e19)",
    L"\u0e0a\u0e37\u0e48\u0e2d A \u2192 Z",
    L"\u0e0a\u0e37\u0e48\u0e2d Z \u2192 A",
    L"\u0e08\u0e33\u0e19\u0e27\u0e19\u0e2a\u0e39\u0e07\u0e2a\u0e38\u0e14",
    L"\u0e20\u0e32\u0e1e\u0e40\u0e04\u0e25\u0e37\u0e48\u0e2d\u0e19\u0e44\u0e2b\u0e27",
    L"\u0e1e\u0e31\u0e14",
    L"\u0e23\u0e48\u0e2d\u0e19",
    L"\u0e2a\u0e1b\u0e23\u0e34\u0e07",
    L"\u0e40\u0e25\u0e37\u0e2d\u0e19\u0e2b\u0e32\u0e22",
    L"\u0e44\u0e21\u0e48\u0e21\u0e35",
    L"\u0e23\u0e27\u0e21\u0e42\u0e1f\u0e25\u0e40\u0e14\u0e2d\u0e23\u0e4c",
    L"\u0e41\u0e2a\u0e14\u0e07\u0e19\u0e32\u0e21\u0e2a\u0e01\u0e38\u0e25\u0e44\u0e1f\u0e25\u0e4c",
    L"\u0e40\u0e1b\u0e25\u0e35\u0e48\u0e22\u0e19\u0e42\u0e1f\u0e25\u0e40\u0e14\u0e2d\u0e23\u0e4c\u2026",
    L"\u0e2d\u0e2d\u0e01\u0e08\u0e32\u0e01 FanFolder",
    L"\u0e40\u0e1b\u0e34\u0e14: ",
    L"\u0e40\u0e25\u0e37\u0e2d\u0e01\u0e42\u0e1f\u0e25\u0e40\u0e14\u0e2d\u0e23\u0e4c\u0e40\u0e1e\u0e37\u0e48\u0e2d\u0e15\u0e23\u0e27\u0e08\u0e2a\u0e2d\u0e1a",
    L"\u0e42\u0e1f\u0e25\u0e40\u0e14\u0e2d\u0e23\u0e4c",
    L"\u0e14\u0e32\u0e27\u0e19\u0e4c\u0e42\u0e2b\u0e25\u0e14",
    L"\u0e40\u0e14\u0e2a\u0e01\u0e4c\u0e17\u0e47\u0e2d\u0e1b",
    L"\u0e40\u0e2d\u0e01\u0e2a\u0e32\u0e23",
    L"\u0e44\u0e1f\u0e25\u0e4c\u0e25\u0e48\u0e32\u0e2a\u0e38\u0e14",
    L"\u0e40\u0e2d\u0e01\u0e2a\u0e32\u0e23 Office 365 \u0e25\u0e48\u0e32\u0e2a\u0e38\u0e14",
    L"\u0e40\u0e23\u0e35\u0e22\u0e01\u0e14\u0e39\u2026",
    L"\u0e40\u0e1b\u0e34\u0e14\u0e43\u0e19 Explorer",
};

// ---------------------------------------------------------------------------
// Language lookup
// ---------------------------------------------------------------------------
const Strings& GetStrings() {
    const LANGID lang = PRIMARYLANGID(GetUserDefaultUILanguage());
    switch (lang) {
    case 0x06: return kDa;   // Danish (Dansk)
    case 0x1D: return kSv;   // Swedish (Svenska)
    case 0x14: return kNo;   // Norwegian (Norsk)
    case 0x07: return kDe;   // German (Deutsch)
    case 0x13: return kNl;   // Dutch (Nederlands)
    case 0x15: return kPl;   // Polish (Polski)
    case 0x01: return kAr;   // Arabic (عربي)
    case 0x04: return kZh;   // Chinese Simplified (中文简体)
    case 0x11: return kJa;   // Japanese (日本語)
    case 0x12: return kKo;   // Korean (한국어)
    case 0x41: return kSw;   // Swahili (Kiswahili)
    case 0x0C: return kFr;   // French (Français)
    case 0x10: return kIt;   // Italian (Italiano)
    case 0x0A: return kEs;   // Spanish (Español)
    case 0x16: return kPt;   // Portuguese (Português)
    case 0x19: return kRu;   // Russian (Русский)
    case 0x39: return kHi;   // Hindi (हिन्दी)
    case 0x1F: return kTr;   // Turkish (Türkçe)
    case 0x0D: return kHe;   // Hebrew (עברית)
    case 0x05: return kCs;   // Czech (Čeština)
    case 0x0B: return kFi;   // Finnish (Suomi)
    case 0x0E: return kHu;   // Hungarian (Magyar)
    case 0x08: return kEl;   // Greek (Ελληνικά)
    case 0x2A: return kVi;   // Vietnamese (Tiếng Việt)
    case 0x21: return kId;   // Indonesian (Bahasa Indonesia)
    case 0x22: return kUk;   // Ukrainian (Українська)
    case 0x18: return kRo;   // Romanian (Română)
    case 0x1E: return kTh;   // Thai (ไทย)
    default:   return kEn;   // English (fallback)
    }
}
