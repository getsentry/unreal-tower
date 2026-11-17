// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerFileDialogLibrary.h"

#if PLATFORM_WINDOWS
#include "Framework/Application/SlateApplication.h"
#include "Microsoft/COMPointer.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <shobjidl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

bool USentryTowerFileDialogLibrary::IsFileDialogSupported()
{
#if PLATFORM_WINDOWS
	return true;
#else
	return false;
#endif
}

bool USentryTowerFileDialogLibrary::OpenFileDialog(const FString& Title, TArray<FString>& OutFilePaths)
{
	OutFilePaths.Reset();

#if PLATFORM_WINDOWS
	check(IsInGameThread());

	const bool bComInitialized = FWindowsPlatformMisc::CoInitialize();

	{
		TComPtr<IFileOpenDialog> FileDialog;
		if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&FileDialog))))
		{
			DWORD Options = 0;
			FileDialog->GetOptions(&Options);
			FileDialog->SetOptions(Options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);

			if (!Title.IsEmpty())
			{
				FileDialog->SetTitle(*Title);
			}

			// Parent the dialog to the game window so it stays on top of it and is modal to it.
			HWND ParentWindow = nullptr;
			if (FSlateApplication::IsInitialized())
			{
				ParentWindow = (HWND)FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
			}

			if (SUCCEEDED(FileDialog->Show(ParentWindow)))
			{
				TComPtr<IShellItemArray> Results;
				if (SUCCEEDED(FileDialog->GetResults(&Results)))
				{
					DWORD NumResults = 0;
					Results->GetCount(&NumResults);

					for (DWORD Index = 0; Index < NumResults; ++Index)
					{
						TComPtr<IShellItem> Item;
						if (FAILED(Results->GetItemAt(Index, &Item)))
						{
							continue;
						}

						PWSTR FilePath = nullptr;
						if (SUCCEEDED(Item->GetDisplayName(SIGDN_FILESYSPATH, &FilePath)))
						{
							FString Path(FilePath);
							FPaths::NormalizeFilename(Path);
							OutFilePaths.Add(MoveTemp(Path));
							::CoTaskMemFree(FilePath);
						}
					}
				}
			}
		}
	}

	if (bComInitialized)
	{
		FWindowsPlatformMisc::CoUninitialize();
	}
#endif

	return OutFilePaths.Num() > 0;
}
