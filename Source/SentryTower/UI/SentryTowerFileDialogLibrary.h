// Copyright (c) 2024 Sentry. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SentryTowerFileDialogLibrary.generated.h"

/**
 * Native OS file picker for runtime (non-editor) builds.
 *
 * UE's DesktopPlatform module is a Developer module and isn't available in Shipping builds,
 * so this talks to the platform API directly. Currently implemented for Windows only;
 * on every other platform IsFileDialogSupported() returns false and OpenFileDialog() is a no-op.
 */
UCLASS()
class SENTRYTOWER_API USentryTowerFileDialogLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Whether a native file picker is available on the current platform. */
	UFUNCTION(BlueprintPure, Category = "SentryTower|FileDialog")
	static bool IsFileDialogSupported();

	/**
	 * Shows a modal native "open file" dialog that allows selecting multiple files.
	 * Blocks the game thread until the dialog is closed.
	 *
	 * @param Title Dialog window title.
	 * @param OutFilePaths Absolute paths of the selected files (empty if cancelled).
	 * @return True if at least one file was selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "SentryTower|FileDialog")
	static bool OpenFileDialog(const FString& Title, TArray<FString>& OutFilePaths);
};
