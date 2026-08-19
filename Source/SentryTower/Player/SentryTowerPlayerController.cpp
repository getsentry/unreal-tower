// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "SentryTowerPawn.h"
#include "SentryTowerTurret.h"

#if PLATFORM_WINDOWS || PLATFORM_XSX || PLATFORM_XB1
#include "SentryShaders/HeavyComputeLoop/HeavyComputeLoop.h"
#endif

#include "SentryTower/Enemy/SentryTowerEnemyBase.h"

void ASentryTowerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Add Input Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ASentryTowerPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ASentryTowerPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Setup keyboard events
		EnhancedInputComponent->BindAction(OpenMenuAction, ETriggerEvent::Started, this, &ASentryTowerPlayerController::OnOpenMenuStarted);
		EnhancedInputComponent->BindAction(TriggerGpuCrashAction, ETriggerEvent::Started, this, &ASentryTowerPlayerController::OnTriggerGpuCrashStarted);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ASentryTowerPlayerController: Failed to find an Enhanced Input Component!"));
	}
}

void ASentryTowerPlayerController::OnOpenMenuStarted()
{
	UE_LOG(LogTemp, Log, TEXT("ASentryTowerPlayerController: Open menu!"));

	OnOpenMenu.Broadcast();
}

void ASentryTowerPlayerController::OnTriggerGpuCrashStarted()
{
	UE_LOG(LogTemp, Log, TEXT("ASentryTowerPlayerController: Trigger GPU crash!"));

#if PLATFORM_WINDOWS || PLATFORM_XSX || PLATFORM_XB1

	FHeavyComputeLoopDispatchParams Params(1, 1, 1);
	Params.Input[0] = 111;
	Params.Input[1] = 222;

	FHeavyComputeLoopInterface::Dispatch(Params, [](int OutputVal)
	{
		UE_LOG(LogTemp, Log, TEXT("ASentryTowerPlayerController: Shader output - %d"), OutputVal);
	});

#endif

	OnTriggerGpuCrash.Broadcast();
}
