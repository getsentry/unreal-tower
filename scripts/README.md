# Setting up the Sentry plugin with a script

[`setup-sentry-sdk.ps1`](setup-sentry-sdk.ps1) installs the [Sentry Unreal SDK](https://github.com/getsentry/sentry-unreal) plugin into `Plugins/Sentry`. CI and the console repos use it to build the demo.

## Prerequisites

* [PowerShell 7](https://learn.microsoft.com/powershell/scripting/install/installing-powershell)
* The [GitHub CLI](https://cli.github.com), logged in with `gh auth login` (not needed for `-LocalPath`)

## Usage

Run from the repository root:

```
pwsh scripts/setup-sentry-sdk.ps1
pwsh scripts/setup-sentry-sdk.ps1 -Ref <branch-or-commit>
pwsh scripts/setup-sentry-sdk.ps1 -LocalPath <path-to-sentry-unreal>
```

* **No arguments:** installs the release pinned in [`sentry-unreal.properties`](../sentry-unreal.properties). Run it again whenever the pinned version changes.
* **`-Ref`:** installs plugin sources from a sentry-unreal branch or commit, with the binaries of the pinned release. This is for trying unreleased plugin changes. The script warns when the ref pins different native/Java/Cocoa SDK versions than the release, because the plugin may then not compile.
* **`-LocalPath`:** links `Plugins/Sentry` to a local sentry-unreal checkout, for working on the plugin and the game together. Set up the checkout with its own init script first, so it has the SDK binaries.

Close the Unreal Editor before running the script: every run replaces the whole `Plugins/Sentry` directory, including its compiled `Binaries` and `Intermediate` folders. If something fails, the existing plugin is left in place.

To change the release the game builds with, edit `version` in `sentry-unreal.properties`.
