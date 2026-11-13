// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "SentryTowerPawn.h"
#include "SentryTowerTurret.h"

#if PLATFORM_WINDOWS
#include "SentryShaders/HeavyComputeLoop/HeavyComputeLoop.h"
#include "SentryShaders/PageFault/PageFault.h"
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

	// Launched with `-gpu-pagefault`: fault the GPU once the level has been running for a while.
	if (FParse::Param(FCommandLine::Get(), TEXT("gpu-pagefault")))
	{
		FTimerHandle GpuPageFaultTimer;
		GetWorldTimerManager().SetTimer(GpuPageFaultTimer, this, &ASentryTowerPlayerController::OnTriggerGpuPageFault, 20.0f, false);
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

void ASentryTowerPlayerController::OnTriggerGpuPageFault()
{
	UE_LOG(LogTemp, Log, TEXT("ASentryTowerPlayerController: Trigger GPU page fault!"));

#if PLATFORM_WINDOWS
	FPageFaultInterface::Dispatch();
#endif
}

void ASentryTowerPlayerController::OnTriggerGpuCrashStarted()
{
	UE_LOG(LogTemp, Log, TEXT("ASentryTowerPlayerController: Trigger GPU crash!"));

#if PLATFORM_WINDOWS

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
