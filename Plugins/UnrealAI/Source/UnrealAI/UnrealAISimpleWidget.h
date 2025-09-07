#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UnrealAIService.h"
#include "UnrealAISimpleWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class UNREALAI_API UUnrealAISimpleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUnrealAISimpleWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// AI Service instance
	UPROPERTY()
	UUnrealAIService* AIService;

	// AI Service callbacks
	UFUNCTION()
	void OnAIResponseReceived(const FAIResponse& Response);

	// Test functions
	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void TestLocalLLM();

	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void TestBlueprintGeneration();

	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void TestCPPGeneration();

	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void TestCodeReview();

	// Helper functions
	void DisplayResponse(const FAIResponse& Response);
	void ShowStatus(const FString& Status);
};
