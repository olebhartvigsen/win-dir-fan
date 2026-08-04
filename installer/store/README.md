# Microsoft Store listing files

Store listing metadata for the FanFolder Win32 app (MSI/EXE submission type).

## Structure

Each `{lang}.json` file contains an `UpdateMetadataRequest`-compatible JSON object
with three top-level sections:

| Section | Purpose |
|---|---|
| `Listings` | Localized Store listing: description, features, search terms, screenshots captions |
| `Properties` | App-level properties: privacy URL, support contact, certification notes, category |
| `Availability` | Markets, pricing, discoverability (same across all languages) |

## Languages (30 files)

| Code | Language | Code | Language | Code | Language |
|---|---|---|---|---|---|
| `en` | English (base) | `de` | Deutsch | `ja` | 日本語 |
| `da` | Dansk | `pl` | Polski | `ko` | 한국어 |
| `sv` | Svenska | `fr` | Français | `zh-cn` | 中文(简体) |
| `nb` | Norsk Bokmål | `it` | Italiano | `zh-tw` | 中文(繁體) |
| `nl` | Nederlands | `es` | Español | `ar` | العربية |
| | | `pt` | Português | `tr` | Türkçe |
| | | `pt-br` | Português (Brasil) | `hi` | हिन्दी |
| `cs` | Čeština | `vi` | Tiếng Việt | `th` | ไทย |
| `fi` | Suomi | `id` | Bahasa Indonesia | `sw` | Kiswahili |
| `hu` | Magyar | `uk` | Українська | `el` | Ελληνικά |
| | | `ro` | Română | `ru` | Русский |

## API usage

The Win32 Store submission API uses `PUT /submission/v1/product/{productId}/metadata`
with the JSON body from these files. The `Listings` object is a **single** listing
(not an array); to update multiple languages, call the endpoint once per language
with the appropriate `Listings.Language` value.

```bash
# Example: update English listing
curl -X PUT \
  "https://api.store.microsoft.com/submission/v1/product/{productId}/metadata" \
  -H "Authorization: Bearer {token}" \
  -H "X-Seller-Account-Id: {sellerId}" \
  -H "Content-Type: application/json" \
  -d @listings/en.json
```

See the `microsoft-store-submission` Hermes skill for the full API workflow.

## Fields

| Field | Max | Section | Notes |
|---|---|---|---|
| `Description` | 10000 chars | Listings | Full Store description |
| `ShortDescription` | 500 chars | Listings | Short blurb |
| `WhatsNew` | 1500 chars | Listings | Release notes for this version |
| `ProductFeatures` | 20 items × 200 chars | Listings | Feature bullet list |
| `SearchTerms` | 7 items × 30 chars | Listings | Store search keywords |
| `Copyright` | 200 chars | Listings | |
| `DevelopedBy` | 255 chars | Listings | |
| `ContactInfo` | 2048 chars | Listings | Support URL or email |
| `PrivacyPolicyUrl` | 2048 chars | Properties | Required |
| `WebSite` | 2048 chars | Properties | Homepage URL |
| `CertificationNotes` | ~2000 chars | Properties | Notes to cert team (not public) |

## Source

- API models: `microsoft/msstore-cli` → `MSStore.API/Models/` (Listing.cs, Properties.cs, etc.)
- Field-length reference: `microsoft/StoreBroker/PDP/ProductDescription.xsd`
- Terminology: aligned with `FanFolder/src/Localization.cpp` UI strings per language
