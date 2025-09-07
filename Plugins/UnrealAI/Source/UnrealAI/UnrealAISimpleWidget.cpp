#include "UnrealAISimpleWidget.h"
#include "UnrealAIService.h"
// #include "UnrealAISettings.h"
#include "Misc/MessageDialog.h"

UUnrealAISimpleWidget::UUnrealAISimpleWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUnrealAISimpleWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create AI Service instance
	AIService = NewObject<UUnrealAIService>(this);
	
	// Bind AI Service events
	if (AIService)
	{
		AIService->OnResponseReceived.AddDynamic(this, &UUnrealAISimpleWidget::OnAIResponseReceived);
	}

	ShowStatus(TEXT("UnrealAI Plugin Ready!"));
}

void UUnrealAISimpleWidget::NativeDestruct()
{
	// Unbind events
	if (AIService)
	{
		AIService->OnResponseReceived.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UUnrealAISimpleWidget::TestLocalLLM()
{
	if (!AIService)
	{
		ShowStatus(TEXT("Error: AI Service not available"));
		return;
	}

	ShowStatus(TEXT("Testing Local LLM..."));

	FAIRequest Request;
	Request.Prompt = TEXT("Hello! Can you help me with Unreal Engine development?");
	Request.RequestType = EAIRequestType::General;
	Request.Provider = EAIProvider::LocalLLM;

	AIService->SendAIRequest(Request);
}

void UUnrealAISimpleWidget::TestBlueprintGeneration()
{
	if (!AIService)
	{
		ShowStatus(TEXT("Error: AI Service not available"));
		return;
	}

	ShowStatus(TEXT("Testing Blueprint Generation..."));

	FString Description = TEXT("Create a simple health system Blueprint that has a max health of 100 and can take damage");
	AIService->GenerateBlueprintSimple(Description);
}

void UUnrealAISimpleWidget::TestCPPGeneration()
{
	if (!AIService)
	{
		ShowStatus(TEXT("Error: AI Service not available"));
		return;
	}

	ShowStatus(TEXT("Testing C++ Generation..."));

	FString Description = TEXT("Create a C++ class for a weapon system that inherits from AActor");
	AIService->GenerateCPPCodeSimple(Description);
}

void UUnrealAISimpleWidget::TestCodeReview()
{
	if (!AIService)
	{
		ShowStatus(TEXT("Error: AI Service not available"));
		return;
	}

	ShowStatus(TEXT("Testing Code Review..."));

	FString Code = TEXT("void AMyActor::Tick(float DeltaTime)\n{\n    Super::Tick(DeltaTime);\n    // Add some logic here\n}");
	AIService->ReviewCode(Code, TEXT("cpp"));
}

void UUnrealAISimpleWidget::OnAIResponseReceived(const FAIResponse& Response)
{
	DisplayResponse(Response);
}

void UUnrealAISimpleWidget::DisplayResponse(const FAIResponse& Response)
{
	if (Response.bSuccess)
	{
		FString Status = FString::Printf(TEXT("Response received in %.2f seconds"), Response.ProcessingTime);
		ShowStatus(Status);
		
		// Show the response in a dialog for now
		FText DialogText = FText::FromString(Response.Content);
		FMessageDialog::Open(EAppMsgType::Ok, DialogText);
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Error: %s"), *Response.ErrorMessage);
		ShowStatus(ErrorMsg);
		
		// Show error in dialog
		FText DialogText = FText::FromString(ErrorMsg);
		FMessageDialog::Open(EAppMsgType::Ok, DialogText);
	}
}

void UUnrealAISimpleWidget::ShowStatus(const FString& Status)
{
	// For now, just log the status
	UE_LOG(LogTemp, Log, TEXT("UnrealAI Status: %s"), *Status);
}
