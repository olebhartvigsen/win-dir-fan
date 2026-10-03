# Store listing notes

Import-ready listing data lives in `FanFolderListing.csv` in this folder. That CSV is the source of
truth for the listing text; the Partner Center portal is only where it lands.

## Editing without the CSV

Two options, both valid:

- **Partner Center UI directly.** Fastest for a one-off change. Store listing > pick language > edit the
  field > save. Nothing to keep in sync beyond copying the new text back into the CSV afterwards.
- **The Microsoft Store submission API** (`https://api.store.microsoft.com`, scope
  `https://api.store.microsoft.com/.default`). Programmatic, and it can drive the whole submission
  including packages. It needs a Microsoft Entra ID app associated with the Partner Center account
  (Manager role) plus a client secret. Not worth setting up for occasional listing tweaks.

Whichever route you use, put the final text back in `FanFolderListing.csv` so the next submission starts
from the current copy instead of someone re-typing it.

## Hard limit: seven search terms

Store Policy 10.1.3 states search terms must "not exceed seven unique terms or phrases". They must also
be relevant, must not include pricing terms, and must not use other products' titles.

This file currently uses exactly seven. An earlier pass added five more on top, which would have been
rejected at certification. Check the policy before adding an eighth:

https://learn.microsoft.com/en-us/windows/apps/publish/store-policies#1013-distinct-function--value-accurate-representation

Current terms: `FanFolder`, `Windows taskbar`, `recent files`, `quick launch Windows 11`, `macOS Stacks`,
`jump list`, `mac dock for Windows`.

## Other field limits worth knowing

- Description: 10,000 characters max, must not be empty.
- Applicable license terms: required, 10,000 characters max.
- Screenshots: 1 to 10 per listing. Store logo 1:1 is required for every language.
- Description and ShortDescription are both indexed for Store search, so the keyword belongs in both.
- Do not rename or remove a language column in the CSV. Export a fresh copy if the language list changed.
