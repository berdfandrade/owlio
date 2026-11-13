# Code signing policy

Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).

Owlio release builds for Windows (`owlio.exe` and the `Owlio-Setup-<version>.exe` installer) are signed through the SignPath Foundation program for open source projects. Only files built by this repository's GitHub Actions workflow ([`.github/workflows/windows.yml`](../.github/workflows/windows.yml)) from a `v*` tag are submitted for signing.

## Team roles

| Role | Members |
|---|---|
| Committers and reviewers | [berdfandrade](https://github.com/berdfandrade) |
| Approvers | [berdfandrade](https://github.com/berdfandrade) |

Every signing request is approved by hand in SignPath before it is signed. All team members use multi-factor authentication on GitHub and SignPath.

## Privacy policy

This program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

## License

Owlio is licensed under the [GNU General Public License v3.0](../LICENSE).

---

## Setting it up (maintainers)

1. Apply at <https://signpath.org/apply> with this repository and this page as the code signing policy. Wait for approval.
2. In SignPath, under the project **owlio**:
   - connect the GitHub.com trusted build system to this repository;
   - create the artifact configuration **exe** with the contents of [`packaging/signpath/exe.xml`](../packaging/signpath/exe.xml);
   - use the signing policy **release-signing** (manual approval).
3. In the GitHub repository settings:
   - secret `SIGNPATH_API_TOKEN`: an API token of a SignPath CI user with submitter rights on the project;
   - variable `SIGNPATH_ORGANIZATION_ID`: the organization ID shown in SignPath.
4. Push a tag such as `v0.2`. The workflow stops at **Sign owlio.exe** and then at **Sign installer** until each request is approved in SignPath (up to one hour each), checks both signatures and publishes the signed installer.

Without the secret, the workflow builds and publishes an unsigned installer, as before. Pull requests are never signed.

SmartScreen builds reputation per certificate, so the first signed releases can still show a warning for a while; it fades as people download and run them.
