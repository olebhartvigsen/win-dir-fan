# Partner Center Import Folder

This folder is ready for manual upload to Microsoft Partner Center as part of 
the first FanFolder v1.3.1 Store submission.

## How to import

1. Open Partner Center → your app overview page
2. In the **Store listings** section, click **Import listing**
3. Choose **Import folder** (not "Import .csv")
4. Browse to this folder and click **Select folder**
5. Wait for validation — if errors appear, click "View errors" to fix
6. Once imported without errors, the listing data and images are saved to 
   your submission draft

## Contents

```
import-folder/
├── Store listing FanFolder.csv     ← Main listing data (30 languages)
├── store-logo-1x1.png               ← 1:1 store logo (300×300)
└── screenshot-1.png                 ← Desktop screenshot
```

## CSV structure

- **Columns**: `Field`, `Type`, then one column per language (30 total)
- **Languages**: English, Arabic, Chinese (Simplified), Czech, Danish, Dutch,
  Finnish, French, German, Greek, Hebrew, Hindi, Hungarian, Indonesian, Italian,
  Japanese, Korean, Norwegian, Polish, Portuguese, Romanian, Russian, Spanish,
  Kiswahili, Swedish, Thai, Turkish, Ukrainian, Vietnamese
- **Rows**: ProductName, Description, WhatsNew, ProductFeatures1-20,
  ShortDescription, SearchTerms1-7, Applicable license terms, Copyright,
  DevelopedBy, RequirementsMinimum1-11, RequirementsRecommended1-11,
  StoreLogos1-2, Screenshots1-10, HeroArts, Trailers

## Fields in this export

| Field | Status |
|---|---|
| ProductName | FanFolder (all languages) |
| Description | ✅ All 30 languages translated |
| WhatsNew | ✅ All 30 languages — v1.3.1 release notes |
| ProductFeatures1-10 | ✅ All 30 languages translated |
| ProductFeatures11-20 | Empty (unused) |
| ShortDescription | ✅ All 30 languages translated |
| SearchTerms1-7 | ✅ All 30 languages translated |
| Applicable license terms | ✅ All 30 languages translated |
| Copyright | ✅ All 30 languages — "© 2026 Ole Bulow Hartvigsen" |
| DevelopedBy | ✅ All 30 languages — "Ole Bulow Hartvigsen" |
| RequirementsMinimum1-4 | ✅ OS, Windows 11, CPU, disk space |
| RequirementsRecommended1-2 | ✅ Local folder, visible taskbar |
| StoreLogos1 | store-logo-1x1.png |
| Screenshots1 | screenshot-1.png |

## Notes

- The CSV is encoded as **UTF-8 with BOM** (required by Partner Center)
- Image paths in the CSV are relative filenames (images are in the same folder)
- After import, image paths are converted to Partner Center URLs — subsequent
  exports will show URLs instead of filenames
- **Only one .csv file** can be in the folder during import
