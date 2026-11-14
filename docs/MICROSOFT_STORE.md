# Publishing Owlio on the Microsoft Store

Apps installed from the Microsoft Store are signed by Microsoft, so Windows never shows the SmartScreen "unknown publisher" warning for them. The Windows workflow ([`.github/workflows/windows.yml`](../.github/workflows/windows.yml)) builds an MSIX package for the Store on every run, next to the Inno Setup installer.

The package is unsigned on purpose: the Store signs it when a submission is certified. It is uploaded as a workflow artifact (`Owlio-<version>-msix`) and is **not** attached to GitHub releases, because an unsigned MSIX cannot be installed outside the Store.

## What is in the package

- Everything `cmake --install` puts in `dist/` (Owlio plus the Qt files), `LICENSE.txt`, and the logos in [`packaging/msix/Assets`](../packaging/msix/Assets).
- The manifest, [`packaging/msix/AppxManifest.xml.in`](../packaging/msix/AppxManifest.xml.in). It declares a full-trust desktop app (`runFullTrust`) that opens `.fountain` and `.fdx` files.
- The version comes from `CMakeLists.txt`, or from the tag on a release, as four numbers with the last one set to zero (1.1 → 1.1.0.0). The Store requires that last zero.

Settings, autosave copies and backups go to the user's AppData folder, which works the same way in a packaged app.

## One-time setup

1. **Create a developer account** at [Partner Center](https://partner.microsoft.com/dashboard) (Apps and games). Individual accounts are free.
2. **Reserve the name**: Apps and games → New product → MSIX or PWA app → `Owlio`.
3. **Copy the package identity**: in the new product, open Product management → Product identity. Then, in GitHub, go to Settings → Secrets and variables → Actions → Variables and create three repository variables:

   | Variable | Value from Product identity |
   |---|---|
   | `MSIX_IDENTITY_NAME` | Package/Identity/Name |
   | `MSIX_PUBLISHER` | Package/Identity/Publisher (starts with `CN=`) |
   | `MSIX_PUBLISHER_NAME` | Package/Properties/PublisherDisplayName |

   Until these exist, the workflow uses test values (`Owlio.Development`, `CN=Owlio Development`). That package builds and is checked, but the Store will reject it.

## Each release

1. Tag the release as usual (`v1.2`). The workflow builds the installer for GitHub and the MSIX.
2. Download the `Owlio-<version>-msix` artifact from the workflow run and unzip it to get `Owlio-<version>.msix`.
3. In Partner Center, open Owlio → Start submission (or Update for later versions):
   - **Packages**: upload the `.msix`.
   - **Pricing and availability**: Free.
   - **Properties**: category Productivity. Owlio does not collect personal data. Partner Center may still ask for a privacy policy URL; a short page on the website saying Owlio collects no data is enough.
   - **Age ratings**: fill in the questionnaire (no user-generated content shared online, no purchases).
   - **Store listings**, in English and Brazilian Portuguese: description, screenshots (the ones in [`docs/screenshots`](screenshots) work) and the 300×300 logo, if asked.
   - **Restricted capability** `runFullTrust`: explain that Owlio is a desktop app built with Qt that runs as a full-trust Win32 process.
4. Submit. Certification usually takes from a few hours to a few days. After that, the Store listing link can go on the website next to the download button.
