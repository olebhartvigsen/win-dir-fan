# FanFolder v1.2.2

Date: 2026-07-23

Changelog since v1.2.1:
- Fixed a blank fan window on machines where the configured folder (or the default recent-documents list) is empty: the fan now shows a "This folder is empty" message instead of an empty frame with a loading spinner.
- Removed the unused Microsoft Graph integration and its winhttp / nlohmann_json dependencies, shrinking the build and keeping the app free of runtime network dependencies.
- Documentation fixes: corrected the default animation style, the MaxItems range and the default folder path in the README.
