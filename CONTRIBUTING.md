# Sentry Tower Defense

To demonstrate the Unreal SDK capabilities the game can trigger different types of crashes based on user input. This allows the plugin to capture the corresponding reports and upload them to Sentry.

## Required configuration 

In `Project Settings > Plugins > Sentry` set `DSN` and configure [Debug Symbols Uploading](https://docs.sentry.io/platforms/unreal/configuration/debug-symbols/)

## In-game crashes/errors

### Null pointer dereference

Whenever a player levels up, a simple dialog appears (see `Content/UI/W_LevelUp.uasset`) allowing the player to select an attack modifier.
If the `WRATH` modifier is selected, the game will crash the next time the tower's turret attempts to spawn a projectile.
This occurs because the corresponding button handler passes `None` to the `ASentryTowerPawn::SetProjectileType` function.

```cpp
void ASentryTowerTurret::Shoot(AActor* TargetActor, const FVector& TargetLocation)
{
	...

    // Fail to spawn projectile as ProjectileType in `None`
	auto Projectile = 
		Cast<ASentryTowerProjectile>(GetWorld()->SpawnActor(ProjectileType, &SpawnLocation, &SpawnRotation));

    // Dereference nullptr
	Projectile->TargetToFollow = TargetActor;
	Projectile->TargetStationary = TargetLocation;

    ...
}
```

### GPU crash

Press the `G` key during the game to trigger a GPU crash.
This action dispatches a performance-heavy compute shader on the GPU causing it to hang and crash the game.
The shader source code is located in the project at `Shaders/SentryShaders/Private/HeavyComputeLoop/HeavyComputeLoop.usf`.

```cpp
[numthreads(1, 1, 1)]
void HeavyComputeLoop(uint3 DispatchThreadId : SV_DispatchThreadID, uint GroupIndex : SV_GroupIndex)
{
	float x = 0;

	for (int i = 0; i < 10000000000; ++i)
	{
		x += sin(i) * sin(Input[0]) * sin(Input[1]);
	}

	Output[0] = x;
}
```

In order to be able to capture GPU crashes, the game has to be built with Unreal Engine version that addops this [change](https://github.com/EpicGames/UnrealEngine/pull/12648) (starting from UE 5.7 it is available out of the box).

### HTTP request error

Open the in-game shop (button with a cart icon in the upper-right corner of the screen) to purchase the tower upgrade. After clicking the `Upgrade Tower` button the game will send an HTTP request to a dummy server which is expected to return an error. Also, this will trigger the `ensure` check in the corresponding request handler to fail (see `USentryTowerGameInstance::BuyUpgrade` for more details).

## Breadcrumbs

The game automatically adds breadcrumbs whenever a player levels up, a tower's attack modifier is changed, or the game is restarted from the "Game Over" popup.
Breadcrumbs are also added whenever an error message is printed to the logs.

## Attachments

All captured crash events should have the following attachments:

- Crash dump
- Screenshot (for non-GPU crashes)
- NVidia aftermath GPU crash dump (for GPU crashes)
- Game log (if logging is enabled, i.e. Development build)

## Self-playing mode

Launch the game with the `--idle` parameter to enable self-play mode (useful for running tests in a CI environment). In this mode, user input is not required. The game automatically responds to certain events (e.g., level-up, game over) by randomly selecting from a set of predefined actions.

## Console Controls

- To open the in-game shop, press the Y button on Xbox or the △ (triangle) button on PS5
- To open the game menu, press the B button on Xbox or the ○ (circle) button on PS5