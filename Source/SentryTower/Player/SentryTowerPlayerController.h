// Copyright (c) 2024 Sentry. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SentryTowerPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;

struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShootDelegate, AActor*, TargetActor, const FVector&, TargetLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOpenMenuDelegate);

/**
 * Player controller
 */
UCLASS()
class SENTRYTOWER_API ASentryTowerPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void SetupInputComponent() override;

	/** Input handlers. */
	void OnOpenMenuStarted();
	void OnTriggerGpuCrashStarted();
	void OnTriggerGpuPageFault();

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputMappingContext* DefaultMappingContext;

	/** Open game menu input action (touch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* OpenMenuAction;

	/**Trigger GPU crash input action (touch) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Input, meta=(AllowPrivateAccess = "true"))
	UInputAction* TriggerGpuCrashAction;

	UPROPERTY(BlueprintAssignable)
	FOnOpenMenuDelegate OnOpenMenu;

	UPROPERTY(BlueprintAssignable)
	FOnOpenMenuDelegate OnTriggerGpuCrash;

private:
	bool AllowShooting;
};
