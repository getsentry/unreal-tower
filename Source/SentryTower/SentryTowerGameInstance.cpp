// Copyright (c) 2024 Sentry. All Rights Reserved.

#include "SentryTowerGameInstance.h"

#include "HttpModule.h"
#include "SentryLibrary.h"
#include "SentrySettings.h"
#include "SentrySpan.h"
#include "SentrySubsystem.h"
#include "SentryTransaction.h"
#include "SentryTransactionContext.h"
#include "Interfaces/IHttpResponse.h"

void USentryTowerGameInstance::Init()
{
	Super::Init();

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
