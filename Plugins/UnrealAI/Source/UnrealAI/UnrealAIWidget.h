#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "UnrealAIService.h"
#include "UnrealAIWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class UNREALAI_API UUnrealAIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UUnrealAIWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// AI Service
	UPROPERTY()
	UUnrealAIService* AIService;

	// Main UI Components
	UPROPERTY(meta = (BindWidget))
	UBorder* MainBorder;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* MainVerticalBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StatusText;

	// Input Section
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* InputSection;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PromptLabel;

	UPROPERTY(meta = (BindWidget))
	UEditableTextBox* PromptInput;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* OptionsRow;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* RequestTypeCombo;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* ProviderCombo;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* EnableAdvancedOptions;

	// Advanced Options
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* AdvancedOptionsSection;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* TemperatureRow;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TemperatureLabel;

	UPROPERTY(meta = (BindWidget))
	USlider* TemperatureSlider;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TemperatureValue;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* MaxTokensRow;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MaxTokensLabel;

	UPROPERTY(meta = (BindWidget))
	USpinBox* MaxTokensSpinBox;

	// Action Buttons
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* ActionButtonsRow;

	UPROPERTY(meta = (BindWidget))
	UButton* GenerateBlueprintButton;

	UPROPERTY(meta = (BindWidget))
	UButton* GenerateCPPButton;

	UPROPERTY(meta = (BindWidget))
	UButton* AnalyzeCodeButton;

	UPROPERTY(meta = (BindWidget))
	UButton* GeneralQueryButton;

	UPROPERTY(meta = (BindWidget))
	UButton* BlueprintJsonParserButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ClearButton;

	UPROPERTY(meta = (BindWidget))
	UButton* SettingsButton;

	UPROPERTY()
	UButton* TestPromptButton;

	// Progress and Status
	UPROPERTY(meta = (BindWidget))
	UProgressBar* ProgressBar;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* StatusRow;

	UPROPERTY(meta = (BindWidget))
	UImage* StatusIcon;

	// Response Section
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* ResponseSection;

	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* ResponseHeaderRow;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ResponseLabel;

	UPROPERTY(meta = (BindWidget))
	UButton* CopyResponseButton;

	UPROPERTY(meta = (BindWidget))
	UButton* SaveResponseButton;

	UPROPERTY(meta = (BindWidget))
	UScrollBox* ResponseScrollBox;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ResponseText;

	// State
	UPROPERTY()
	bool bIsProcessing;

	UPROPERTY()
	FString LastResponse;

	// UI Event Handlers
	UFUNCTION()
	void OnGenerateBlueprintButtonClicked();

	UFUNCTION()
	void OnGenerateCPPButtonClicked();

	UFUNCTION()
	void OnAnalyzeCodeButtonClicked();

	UFUNCTION()
	void OnGeneralQueryButtonClicked();

	UFUNCTION()
	void OnClearButtonClicked();

	UFUNCTION()
	void OnSettingsButtonClicked();

	UFUNCTION()
	void OnTestPromptButtonClicked();

	UFUNCTION()
	void OnCopyResponseButtonClicked();

	UFUNCTION()
	void OnSaveResponseButtonClicked();

	UFUNCTION()
	void OnRequestTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnProviderChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void OnEnableAdvancedOptionsChanged(bool bIsChecked);

	UFUNCTION()
	void OnTemperatureChanged(float Value);

	UFUNCTION()
	void OnMaxTokensChanged(float Value);

	// AI Service Event Handlers
	UFUNCTION()
	void OnAIResponseReceived(const FAIResponse& Response);

	UFUNCTION()
	void OnBlueprintGenerated(const FAIResponse& Response);

	UFUNCTION()
	void OnCPPGenerated(const FAIResponse& Response);

	UFUNCTION()
	void OnCodeReviewed(const FAIResponse& Response);

	// UI Management Functions
	void InitializeUI();
	void UpdateUIState();
	void SetProcessingState(bool bProcessing);
	void DisplayResponse(const FAIResponse& Response);
	void ShowError(const FString& ErrorMessage);
	void ShowSuccess(const FString& Message);
	void ClearResponse();
	void UpdateStatusText(const FString& Status);
	void UpdateStatusIcon(bool bSuccess);
	void EnableInputs(bool bEnable);
	void UpdateButtonStates();
	void UpdateAdvancedOptionsVisibility();
	void UpdateTemperatureDisplay(float Value);
	void UpdateMaxTokensDisplay(float Value);

	// Utility Functions
	FAIRequest CreateAIRequest(EAIRequestType RequestType);
	void SendAIRequest(const FAIRequest& Request);
	void ShowResponseDialog(const FString& Content, const FString& Title);
	void CopyToClipboard(const FString& Text);
	void SaveResponseToFile(const FString& Content);
	FString GetRequestTypeDescription(EAIRequestType RequestType);
	FString GetProviderDescription(EAIProvider Provider);
};
