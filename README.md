# Sentry Tower Defense - Built with Unreal Engine

A Sentry-themed tower defence game written for Unreal Engine in C++/Blueprints featuring:
* Non-stop Idle shooting action!
* 3 turret power-ups with their own unique behavior
* 3 different enemies

![](/Media/gameplay.gif?raw=true)

## Gameplay

* Destroy enemies to pick up XP
* Select attack modifiers for each level
* Survive as long as you can!

## System requirements

* Windows
* Unreal Engine 5.8
* [PowerShell 7](https://learn.microsoft.com/powershell/scripting/install/installing-powershell) and the [GitHub CLI](https://cli.github.com), logged in with `gh auth login` (to set up the Sentry plugin)

## How to run locally

### 1. Install Unreal Engine
* Get the [Epic Games Launcher](https://store.epicgames.com/en-US/download)
* Navigate to Unreal Engine -> Library and then click on "Engine Versions +" - Select Engine version 5.8.X

### 2. Set up the Sentry plugin
The [Sentry Unreal SDK](https://github.com/getsentry/sentry-unreal) plugin is not committed to this repo. Its version is pinned in [`sentry-unreal.properties`](sentry-unreal.properties). Install it into `Plugins/Sentry` with:

```
pwsh scripts/setup-sentry-sdk.ps1
```

Run it again whenever the pinned version changes. Close the editor first: the script replaces the whole `Plugins/Sentry` directory.

Without the script, download `sentry-unreal-<version>.zip` (the one without an engine version in its name) for the pinned version from the [sentry-unreal releases](https://github.com/getsentry/sentry-unreal/releases) and extract it into `Plugins/Sentry`.

To try unreleased plugin changes:
* `pwsh scripts/setup-sentry-sdk.ps1 -Ref <branch-or-commit>` installs plugin sources from a sentry-unreal branch or commit with the binaries of the pinned release.
* `pwsh scripts/setup-sentry-sdk.ps1 -LocalPath <path-to-sentry-unreal>` links `Plugins/Sentry` to a local sentry-unreal checkout. Set up the checkout with its own init script first, so it has the SDK binaries.

### 3. Run the game
* Open `SentryTower.uproject`, or pick it in the Epic Games Launcher under "My Projects"
* Once in the Editor, click the green Play icon to get the demo running

## Features/Errors
*  https://github.com/sentry-demos/unreal/blob/main/CONTRIBUTING.md


## Third-party assets

* 3D models - [Space Kit](https://kenney.nl/assets/space-kit) by @KenneyNL
* Materials - [Stylized Egypt](https://www.unrealengine.com/marketplace/en-US/product/stylized-egypt) permanently free UE Marketplace collection
* VFX - [FX Variety Pack](https://www.unrealengine.com/marketplace/en-US/product/a36bac8b05004e999dd4b1d332501f49) permanently free UE Marketplace collection
* Sounds - [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds) by @KenneyNL
