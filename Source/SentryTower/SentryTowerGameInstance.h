// Copyright (c) 2024 Sentry. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "SentryTowerGameInstance.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnBuyComplete, bool, IsSuccessfull);

UCLASS()
class SENTRYTOWER_API USentryTowerGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Meta = (AutoCreateRefTerm = "OnBuyComplete"))
	void BuyUpgrade(const FOnBuyComplete& OnBuyComplete);

private:
	TSharedPtr<FJsonObject> BuildCheckoutRequestJson();
};
