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

## How to run locally

### 1. Install Unreal Engine
* Get the [Epic Games Launcher](https://store.epicgames.com/en-US/download)
* Navigate to Unreal Engine -> Library and then click on "Engine Versions +" - Select Engine version 5.8.X

### 2. Add the Sentry plugin
The [Sentry Unreal SDK](https://github.com/getsentry/sentry-unreal) plugin is not included in this repo.
* Open the [sentry-unreal releases page](https://github.com/getsentry/sentry-unreal/releases) and find the release with the version listed in [`sentry-unreal.properties`](sentry-unreal.properties)
* Download `sentry-unreal-<version>-engine5.8.zip` from its assets
* Extract it into a `Plugins/Sentry` folder in this project, so that the file `Plugins/Sentry/Sentry.uplugin` exists

To install the plugin from the command line instead, or to try unreleased plugin changes, see [Setting up the Sentry plugin with a script](scripts/README.md).

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
