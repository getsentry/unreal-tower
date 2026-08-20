// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerGameInstance.h"

#include "Containers/Ticker.h"
#include "HttpModule.h"
#include "SentryLibrary.h"
#include "SentrySettings.h"
#include "SentrySpan.h"
#include "SentrySubsystem.h"
#include "SentryTransaction.h"
#include "SentryTransactionContext.h"
#include "Interfaces/IHttpResponse.h"

#if PLATFORM_WINDOWS || PLATFORM_XSX || PLATFORM_XB1
#include "SentryShaders/HeavyComputeLoop/HeavyComputeLoop.h"
#endif

void USentryTowerGameInstance::Init()
{
	Super::Init();

#if PLATFORM_WINDOWS || PLATFORM_XSX || PLATFORM_XB1
	if (FParse::Param(FCommandLine::Get(), TEXT("crash-gpu")))
	{
		// Deferred so the RHI and global shader map are ready before triggering a GPU crash.
		// Two staged attempts so the run's wall-clock attributes the crash: HeavyComputeLoop
		// first, then (if the title is still alive) the engine's GPUDebugCrash command.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
		{
			static float ElapsedTime = 0.0f;
			static bool bDispatchedShader = false;
			static bool bDispatchedEngineCrash = false;
			ElapsedTime += DeltaTime;

			if (!bDispatchedShader && ElapsedTime >= 5.0f)
			{
				bDispatchedShader = true;

				UE_LOG(LogTemp, Warning, TEXT("[gpu-crash] Stage 1: dispatching HeavyComputeLoop"));
				if (USentrySubsystem* Sentry = GEngine->GetEngineSubsystem<USentrySubsystem>())
				{
					Sentry->SetTag(TEXT("test.gpu_crash"), TEXT("xsx"));
					Sentry->CaptureMessage(TEXT("GPU crash test: dispatching HeavyComputeLoop"));
				}

				FHeavyComputeLoopDispatchParams Params(1, 1, 1);
				Params.Input[0] = 111;
				Params.Input[1] = 222;
				FHeavyComputeLoopInterface::Dispatch(Params, [](int OutputVal)
				{
					// Only reached if the GPU did NOT hang (shader ran to completion)
					UE_LOG(LogTemp, Warning, TEXT("[gpu-crash] HeavyComputeLoop COMPLETED without hang (output=%d)"), OutputVal);
					if (USentrySubsystem* Sentry = GEngine->GetEngineSubsystem<USentrySubsystem>())
					{
						Sentry->CaptureMessage(TEXT("GPU crash test: HeavyComputeLoop COMPLETED without hang"));
					}
				});

				UE_LOG(LogTemp, Warning, TEXT("[gpu-crash] Stage 1: HeavyComputeLoop dispatch call returned"));
				return true;
			}

			if (bDispatchedShader && !bDispatchedEngineCrash && ElapsedTime >= 15.0f)
			{
				bDispatchedEngineCrash = true;

				UE_LOG(LogTemp, Warning, TEXT("[gpu-crash] Stage 2: still alive; issuing engine 'GPUDebugCrash hang'"));
				if (USentrySubsystem* Sentry = GEngine->GetEngineSubsystem<USentrySubsystem>())
				{
					Sentry->CaptureMessage(TEXT("GPU crash test: issuing engine GPUDebugCrash hang"));
				}

				if (GEngine)
				{
					GEngine->Exec(nullptr, TEXT("GPUDebugCrash hang"));
				}

				UE_LOG(LogTemp, Warning, TEXT("[gpu-crash] Stage 2: GPUDebugCrash exec returned"));
				return false;
			}

			return true;
		}));

		return;
	}
#endif

	if (FParse::Param(FCommandLine::Get(), TEXT("upload-only")))
	{
		// For upload-only mode (second run after crash on platform with in-proc crash handler, e.g. Xbox) exit after Sentry initializes
		// Short delay allows pending crash reports to be uploaded before shutdown
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float DeltaTime) -> bool
		{
			static float ElapsedTime = 0.0f;
			ElapsedTime += DeltaTime;

			if (ElapsedTime >= 3.0f)
			{
				FPlatformMisc::RequestExit(false);
				return false;
			}

			return true;
		}));

		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		// For CI simulation (no RHI available) copy pre-made screenshot to dest where Unreal SDK can pick it up during crash handling
		const FString FakeScreenshotPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Resources"), TEXT("screenshot.png"));
		
		// Add screenshot attachment to Sentry
		if (USentrySubsystem* SentrySubsystem = GEngine->GetEngineSubsystem<USentrySubsystem>())
		{
			// Create the attachment
			USentryAttachment* ScreenshotAttachment = USentryLibrary::CreateSentryAttachmentWithPath(
				FakeScreenshotPath,
				TEXT("screenshot.png"),
				TEXT("image/png")
			);

			// Add to subsystem scope (will be included with all events)
			if (ScreenshotAttachment)
			{
				SentrySubsystem->AddAttachment(ScreenshotAttachment);
			}
		}
	}
}

void USentryTowerGameInstance::BuyUpgrade(const FOnBuyComplete& OnBuyComplete)
{
	USentrySubsystem* Sentry = GEngine->GetEngineSubsystem<USentrySubsystem>();

	USentryTransaction* CheckoutTransaction = Sentry->StartTransaction(TEXT("checkout"), TEXT("http.client"), true);

	USentrySpan* ProcessSpan = CheckoutTransaction->StartChildSpan(TEXT("task"), TEXT("process_upgrade_data"), true);

	TSharedPtr<FJsonObject> UpgradeDataJsonObject = BuildCheckoutRequestJson();

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	if (!FJsonSerializer::Serialize(UpgradeDataJsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize JSON object"));
	}

	FPlatformProcess::Sleep(0.1f);

	ProcessSpan->Finish();

	USentrySpan* CheckoutSpan = CheckoutTransaction->StartChildSpan(TEXT("task"), TEXT("checkout_request"), true);
	FString Domain = TEXT("https://flask.empower-plant.com");
	FString Endpoint = TEXT("/checkout");
	FString CheckoutURL = Domain + Endpoint;

	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = Http->CreateRequest();

	HttpRequest->SetURL(CheckoutURL);
	HttpRequest->SetVerb("POST");
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	FString TraceKey;
	FString TraceValue;
	CheckoutSpan->GetTrace(TraceKey, TraceValue);

	UE_LOG(LogTemp, Log, TEXT("TraceValue - %s"), *TraceValue);

	HttpRequest->SetHeader(TraceKey, TraceValue);

	HttpRequest->SetContentAsString(JsonString);

	HttpRequest->OnProcessRequestComplete().BindLambda([=](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		CheckoutSpan->Finish();

		USentrySpan* ResponseSpan = CheckoutTransaction->StartChildSpan(TEXT("task"), TEXT("process_checkout_response"), true);
		ensureMsgf(bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200, TEXT("Checkout HTTP request failed"));

		if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
		{
			UE_LOG(LogTemp, Log, TEXT("Checkout completed"));
			OnBuyComplete.ExecuteIfBound(true);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Checkout failed"));
			OnBuyComplete.ExecuteIfBound(false);
		}

		ResponseSpan->Finish();
		CheckoutTransaction->Finish();
	});

	HttpRequest->ProcessRequest();
}

TSharedPtr<FJsonObject> USentryTowerGameInstance::BuildCheckoutRequestJson()
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	// Build cart object
	TSharedPtr<FJsonObject> CartObject = MakeShareable(new FJsonObject());

	// Build items array
	TArray<TSharedPtr<FJsonValue>> ItemsArray;
	TSharedPtr<FJsonObject> ItemObject = MakeShareable(new FJsonObject());
	ItemObject->SetStringField(TEXT("description"), TEXT("The mood ring for plants."));
	ItemObject->SetStringField(TEXT("descriptionfull"), TEXT("This is an example of what you can do with just a few things, a little imagination and a happy dream in your heart. I'm a water fanatic. I love water. There's not a thing in the world wrong with washing your brush. Everybody needs a friend. Here we're limited by the time we have."));
	ItemObject->SetNumberField(TEXT("id"), 3);
	ItemObject->SetStringField(TEXT("img"), TEXT("https://storage.googleapis.com/application-monitoring/mood-planter.jpg"));
	ItemObject->SetStringField(TEXT("imgcropped"), TEXT("https://storage.googleapis.com/application-monitoring/mood-planter-cropped.jpg"));
	ItemObject->SetNumberField(TEXT("price"), 155);
	ItemObject->SetArrayField(TEXT("reviews"), TArray<TSharedPtr<FJsonValue>>());
	ItemObject->SetStringField(TEXT("title"), TEXT("Plant Mood"));
	ItemsArray.Add(MakeShareable(new FJsonValueObject(ItemObject)));
	CartObject->SetArrayField(TEXT("items"), ItemsArray);

	// Build quantities object
	TSharedPtr<FJsonObject> QuantitiesObject = MakeShareable(new FJsonObject());
	QuantitiesObject->SetNumberField(TEXT("3"), 3);
	CartObject->SetObjectField(TEXT("quantities"), QuantitiesObject);
	CartObject->SetNumberField(TEXT("total"), 465);

	// Build form object
	TSharedPtr<FJsonObject> FormObject = MakeShareable(new FJsonObject());
	FormObject->SetStringField(TEXT("address"), TEXT(""));
	FormObject->SetStringField(TEXT("city"), TEXT(""));
	FormObject->SetStringField(TEXT("country"), TEXT(""));
	FormObject->SetStringField(TEXT("email"), TEXT("sampleEmail@email.com"));
	FormObject->SetStringField(TEXT("firstName"), TEXT(""));
	FormObject->SetStringField(TEXT("lastName"), TEXT(""));
	FormObject->SetStringField(TEXT("state"), TEXT(""));
	FormObject->SetStringField(TEXT("subscribe"), TEXT(""));
	FormObject->SetStringField(TEXT("zipCode"), TEXT(""));

	// Set all objects to main JSON
	JsonObject->SetObjectField(TEXT("cart"), CartObject);
	JsonObject->SetObjectField(TEXT("form"), FormObject);
	JsonObject->SetStringField(TEXT("validate_inventory"), TEXT("true"));

	return JsonObject;
}
