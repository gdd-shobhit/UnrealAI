#include "UnrealAIWidget.h"
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
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/SlateColor.h"
#include "Engine/Font.h"

UUnrealAIWidget::UUnrealAIWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsProcessing = false;
}

void UUnrealAIWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Create AI Service instance
	AIService = NewObject<UUnrealAIService>(this);
	
	// Bind AI Service events
	if (AIService)
	{
		AIService->OnResponseReceived.AddDynamic(this, &UUnrealAIWidget::OnAIResponseReceived);
		AIService->OnBlueprintGenerated.AddDynamic(this, &UUnrealAIWidget::OnBlueprintGenerated);
		AIService->OnCPPGenerated.AddDynamic(this, &UUnrealAIWidget::OnCPPGenerated);
		AIService->OnCodeReviewed.AddDynamic(this, &UUnrealAIWidget::OnCodeReviewed);
	}

	InitializeUI();
}

void UUnrealAIWidget::NativeDestruct()
{
	// Unbind events
	if (AIService)
	{
		AIService->OnResponseReceived.RemoveAll(this);
		AIService->OnBlueprintGenerated.RemoveAll(this);
		AIService->OnCPPGenerated.RemoveAll(this);
		AIService->OnCodeReviewed.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UUnrealAIWidget::InitializeUI()
{
	// Set title
	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("🤖 UnrealAI Assistant")));
	}

	// Initialize request type combo
	if (RequestTypeCombo)
	{
		RequestTypeCombo->ClearOptions();
		RequestTypeCombo->AddOption(TEXT("General Query"));
		RequestTypeCombo->AddOption(TEXT("Blueprint Generation"));
		RequestTypeCombo->AddOption(TEXT("C++ Generation"));
		RequestTypeCombo->AddOption(TEXT("Code Review"));
		RequestTypeCombo->AddOption(TEXT("Documentation"));
		RequestTypeCombo->SetSelectedIndex(0);
		RequestTypeCombo->OnSelectionChanged.AddDynamic(this, &UUnrealAIWidget::OnRequestTypeChanged);
	}

	// Initialize provider combo
	if (ProviderCombo)
	{
		ProviderCombo->ClearOptions();
		ProviderCombo->AddOption(TEXT("Local LLM"));
		ProviderCombo->AddOption(TEXT("Claude API"));
		ProviderCombo->AddOption(TEXT("OpenAI API"));
		ProviderCombo->SetSelectedIndex(0);
		ProviderCombo->OnSelectionChanged.AddDynamic(this, &UUnrealAIWidget::OnProviderChanged);
	}

	// Initialize advanced options
	if (EnableAdvancedOptions)
	{
		EnableAdvancedOptions->OnCheckStateChanged.AddDynamic(this, &UUnrealAIWidget::OnEnableAdvancedOptionsChanged);
		EnableAdvancedOptions->SetCheckedState(ECheckBoxState::Unchecked);
	}

	// Initialize temperature slider
	if (TemperatureSlider)
	{
		TemperatureSlider->SetValue(0.7f);
		TemperatureSlider->OnValueChanged.AddDynamic(this, &UUnrealAIWidget::OnTemperatureChanged);
		UpdateTemperatureDisplay(0.7f);
	}

	// Initialize max tokens spinbox
	if (MaxTokensSpinBox)
	{
		MaxTokensSpinBox->SetValue(2048.0f);
		MaxTokensSpinBox->OnValueChanged.AddDynamic(this, &UUnrealAIWidget::OnMaxTokensChanged);
		MaxTokensSpinBox->SetMinValue(100.0f);
		MaxTokensSpinBox->SetMaxValue(8192.0f);
		MaxTokensSpinBox->SetMinSliderValue(100.0f);
		MaxTokensSpinBox->SetMaxSliderValue(8192.0f);
	}

	// Bind button events
	if (GenerateBlueprintButton)
	{
		GenerateBlueprintButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnGenerateBlueprintButtonClicked);
	}

	if (GenerateCPPButton)
	{
		GenerateCPPButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnGenerateCPPButtonClicked);
	}

	if (AnalyzeCodeButton)
	{
		AnalyzeCodeButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnAnalyzeCodeButtonClicked);
	}

	if (GeneralQueryButton)
	{
		GeneralQueryButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnGeneralQueryButtonClicked);
	}

	if (ClearButton)
	{
		ClearButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnClearButtonClicked);
	}

	if (SettingsButton)
	{
		SettingsButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnSettingsButtonClicked);
	}

	// TestPromptButton will be created programmatically below

	if (CopyResponseButton)
	{
		CopyResponseButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnCopyResponseButtonClicked);
	}

	if (SaveResponseButton)
	{
		SaveResponseButton->OnClicked.AddDynamic(this, &UUnrealAIWidget::OnSaveResponseButtonClicked);
	}

	// Initialize UI state
	UpdateUIState();
	UpdateAdvancedOptionsVisibility();
	UpdateStatusText(TEXT("Ready"));
	UpdateStatusIcon(true);
}

void UUnrealAIWidget::OnGenerateBlueprintButtonClicked()
{
	if (!AIService || bIsProcessing)
	{
		return;
	}

	FString Description = PromptInput ? PromptInput->GetText().ToString() : TEXT("");
	if (Description.IsEmpty())
	{
		ShowError(TEXT("Please enter a description for the Blueprint"));
		return;
	}

	SetProcessingState(true);
	UpdateStatusText(TEXT("Generating Blueprint..."));
	UpdateStatusIcon(false);

	FAIRequest Request = CreateAIRequest(EAIRequestType::BlueprintGeneration);
	SendAIRequest(Request);
}

void UUnrealAIWidget::OnGenerateCPPButtonClicked()
{
	if (!AIService || bIsProcessing)
	{
		return;
	}

	FString Description = PromptInput ? PromptInput->GetText().ToString() : TEXT("");
	if (Description.IsEmpty())
	{
		ShowError(TEXT("Please enter a description for the C++ code"));
		return;
	}

	SetProcessingState(true);
	UpdateStatusText(TEXT("Generating C++ code..."));
	UpdateStatusIcon(false);

	FAIRequest Request = CreateAIRequest(EAIRequestType::CPPGeneration);
	SendAIRequest(Request);
}

void UUnrealAIWidget::OnAnalyzeCodeButtonClicked()
{
	if (!AIService || bIsProcessing)
	{
		return;
	}

	FString Code = PromptInput ? PromptInput->GetText().ToString() : TEXT("");
	if (Code.IsEmpty())
	{
		ShowError(TEXT("Please enter code to analyze"));
		return;
	}

	SetProcessingState(true);
	UpdateStatusText(TEXT("Analyzing code..."));
	UpdateStatusIcon(false);

	FAIRequest Request = CreateAIRequest(EAIRequestType::CodeReview);
	SendAIRequest(Request);
}

void UUnrealAIWidget::OnGeneralQueryButtonClicked()
{
	if (!AIService || bIsProcessing)
	{
		return;
	}

	FString Query = PromptInput ? PromptInput->GetText().ToString() : TEXT("");
	if (Query.IsEmpty())
	{
		ShowError(TEXT("Please enter your question"));
		return;
	}

	SetProcessingState(true);
	UpdateStatusText(TEXT("Processing query..."));
	UpdateStatusIcon(false);

	FAIRequest Request = CreateAIRequest(EAIRequestType::General);
	SendAIRequest(Request);
}

void UUnrealAIWidget::OnClearButtonClicked()
{
	ClearResponse();
	if (PromptInput)
	{
		PromptInput->SetText(FText::GetEmpty());
	}
}

void UUnrealAIWidget::OnSettingsButtonClicked()
{
	ShowResponseDialog(TEXT("Settings functionality will be implemented in the next version.\n\nFor now, you can:\n• Use the default settings\n• Adjust temperature and max tokens in advanced options\n• Configure AI providers in the dropdown"), TEXT("Settings"));
}

void UUnrealAIWidget::OnTestPromptButtonClicked()
{
	if (!AIService)
	{
		ShowError(TEXT("AI Service not available"));
		return;
	}

	// Test the prompt generation with a sample request
	AIService->TestPromptGeneration(TEXT("make enemy ai which follows an actor"));
	
	// Show success message
	ShowSuccess(TEXT("Test prompt generated! Check the Output Log for details."));
	UpdateStatusText(TEXT("Test completed - check Output Log"));
}

void UUnrealAIWidget::OnCopyResponseButtonClicked()
{
	if (!LastResponse.IsEmpty())
	{
		CopyToClipboard(LastResponse);
		ShowSuccess(TEXT("Response copied to clipboard!"));
	}
}

void UUnrealAIWidget::OnSaveResponseButtonClicked()
{
	if (!LastResponse.IsEmpty())
	{
		SaveResponseToFile(LastResponse);
	}
}

void UUnrealAIWidget::OnRequestTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateUIState();
}

void UUnrealAIWidget::OnProviderChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateUIState();
}

void UUnrealAIWidget::OnEnableAdvancedOptionsChanged(bool bIsChecked)
{
	UpdateAdvancedOptionsVisibility();
}

void UUnrealAIWidget::OnTemperatureChanged(float Value)
{
	UpdateTemperatureDisplay(Value);
}

void UUnrealAIWidget::OnMaxTokensChanged(float Value)
{
	UpdateMaxTokensDisplay(Value);
}

void UUnrealAIWidget::OnAIResponseReceived(const FAIResponse& Response)
{
	SetProcessingState(false);
	DisplayResponse(Response);
}

void UUnrealAIWidget::OnBlueprintGenerated(const FAIResponse& Response)
{
	SetProcessingState(false);
	DisplayResponse(Response);
	
	if (Response.bSuccess)
	{
		ShowSuccess(TEXT("Blueprint generated successfully!"));
		UpdateStatusIcon(true);
	}
}

void UUnrealAIWidget::OnCPPGenerated(const FAIResponse& Response)
{
	SetProcessingState(false);
	DisplayResponse(Response);
	
	if (Response.bSuccess)
	{
		ShowSuccess(TEXT("C++ code generated successfully!"));
		UpdateStatusIcon(true);
	}
}

void UUnrealAIWidget::OnCodeReviewed(const FAIResponse& Response)
{
	SetProcessingState(false);
	DisplayResponse(Response);
	
	if (Response.bSuccess)
	{
		ShowSuccess(TEXT("Code analysis completed!"));
		UpdateStatusIcon(true);
	}
}

void UUnrealAIWidget::UpdateUIState()
{
	UpdateButtonStates();
	
	// Update status based on provider availability
	EAIProvider SelectedProvider = static_cast<EAIProvider>(ProviderCombo ? ProviderCombo->GetSelectedIndex() : 0);
	
	FString Status;
	switch (SelectedProvider)
	{
	case EAIProvider::LocalLLM:
		Status = AIService && AIService->IsLocalLLMAvailable() ? TEXT("Local LLM Available") : TEXT("Local LLM Not Available");
		break;
	case EAIProvider::Claude:
		Status = AIService && AIService->IsClaudeAvailable() ? TEXT("Claude API Available") : TEXT("Claude API Not Available");
		break;
	case EAIProvider::OpenAI:
		Status = AIService && AIService->IsOpenAIAvailable() ? TEXT("OpenAI API Available") : TEXT("OpenAI API Not Available");
		break;
	}
	
	UpdateStatusText(Status);
}

void UUnrealAIWidget::SetProcessingState(bool bProcessing)
{
	bIsProcessing = bProcessing;
	
	if (ProgressBar)
	{
		ProgressBar->SetVisibility(bProcessing ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	
	EnableInputs(!bProcessing);
	UpdateButtonStates();
}

void UUnrealAIWidget::DisplayResponse(const FAIResponse& Response)
{
	LastResponse = Response.Content;
	
	if (!ResponseText)
	{
		return;
	}

	if (Response.bSuccess)
	{
		ResponseText->SetText(FText::FromString(Response.Content));
		UpdateStatusText(FString::Printf(TEXT("Response received in %.2f seconds"), Response.ProcessingTime));
		UpdateStatusIcon(true);
	}
	else
	{
		ResponseText->SetText(FText::FromString(FString::Printf(TEXT("Error: %s"), *Response.ErrorMessage)));
		UpdateStatusText(TEXT("Request failed"));
		UpdateStatusIcon(false);
	}

	// Scroll to top
	if (ResponseScrollBox)
	{
		ResponseScrollBox->ScrollToStart();
	}
}

void UUnrealAIWidget::ShowError(const FString& ErrorMessage)
{
	UpdateStatusText(FString::Printf(TEXT("Error: %s"), *ErrorMessage));
	UpdateStatusIcon(false);
}

void UUnrealAIWidget::ShowSuccess(const FString& Message)
{
	UpdateStatusText(Message);
	UpdateStatusIcon(true);
}

void UUnrealAIWidget::ClearResponse()
{
	LastResponse.Empty();
	if (ResponseText)
	{
		ResponseText->SetText(FText::GetEmpty());
	}
	UpdateStatusText(TEXT("Ready"));
	UpdateStatusIcon(true);
}

void UUnrealAIWidget::UpdateStatusText(const FString& Status)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Status));
	}
}

void UUnrealAIWidget::UpdateStatusIcon(bool bSuccess)
{
	if (StatusIcon)
	{
		// Set icon color based on success state
		FLinearColor IconColor = bSuccess ? FLinearColor(0.0f, 1.0f, 0.0f, 1.0f) : FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		StatusIcon->SetColorAndOpacity(IconColor);
	}
}

void UUnrealAIWidget::EnableInputs(bool bEnable)
{
	if (PromptInput)
	{
		PromptInput->SetIsEnabled(bEnable);
	}
	
	if (RequestTypeCombo)
	{
		RequestTypeCombo->SetIsEnabled(bEnable);
	}
	
	if (ProviderCombo)
	{
		ProviderCombo->SetIsEnabled(bEnable);
	}

	if (EnableAdvancedOptions)
	{
		EnableAdvancedOptions->SetIsEnabled(bEnable);
	}

	if (TemperatureSlider)
	{
		TemperatureSlider->SetIsEnabled(bEnable);
	}

	if (MaxTokensSpinBox)
	{
		MaxTokensSpinBox->SetIsEnabled(bEnable);
	}
}

void UUnrealAIWidget::UpdateButtonStates()
{
	bool bCanSend = !bIsProcessing && AIService;
	
	if (GenerateBlueprintButton)
	{
		GenerateBlueprintButton->SetIsEnabled(bCanSend);
	}
	
	if (GenerateCPPButton)
	{
		GenerateCPPButton->SetIsEnabled(bCanSend);
	}
	
	if (AnalyzeCodeButton)
	{
		AnalyzeCodeButton->SetIsEnabled(bCanSend);
	}

	if (GeneralQueryButton)
	{
		GeneralQueryButton->SetIsEnabled(bCanSend);
	}
	
	if (ClearButton)
	{
		ClearButton->SetIsEnabled(!bIsProcessing);
	}
	
	if (SettingsButton)
	{
		SettingsButton->SetIsEnabled(!bIsProcessing);
	}
	// TestPromptButton is managed separately

	if (CopyResponseButton)
	{
		CopyResponseButton->SetIsEnabled(!LastResponse.IsEmpty());
	}

	if (SaveResponseButton)
	{
		SaveResponseButton->SetIsEnabled(!LastResponse.IsEmpty());
	}
}

void UUnrealAIWidget::UpdateAdvancedOptionsVisibility()
{
	bool bShowAdvanced = EnableAdvancedOptions ? EnableAdvancedOptions->IsChecked() : false;
	
	if (AdvancedOptionsSection)
	{
		AdvancedOptionsSection->SetVisibility(bShowAdvanced ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UUnrealAIWidget::UpdateTemperatureDisplay(float Value)
{
	if (TemperatureValue)
	{
		TemperatureValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), Value)));
	}
}

void UUnrealAIWidget::UpdateMaxTokensDisplay(float Value)
{
	if (MaxTokensLabel)
	{
		MaxTokensLabel->SetText(FText::FromString(FString::Printf(TEXT("Max Tokens: %d"), FMath::RoundToInt(Value))));
	}
}

FAIRequest UUnrealAIWidget::CreateAIRequest(EAIRequestType RequestType)
{
	FAIRequest Request;
	Request.Prompt = PromptInput ? PromptInput->GetText().ToString() : TEXT("");
	Request.RequestType = RequestType;
	Request.Provider = static_cast<EAIProvider>(ProviderCombo ? ProviderCombo->GetSelectedIndex() : 0);
	
	// Get advanced options if enabled
	if (EnableAdvancedOptions && EnableAdvancedOptions->IsChecked())
	{
		if (TemperatureSlider)
		{
			Request.Temperature = TemperatureSlider->GetValue();
		}
		
		if (MaxTokensSpinBox)
		{
			Request.MaxTokens = FMath::RoundToInt(MaxTokensSpinBox->GetValue());
		}
	}
	else
	{
		// Use defaults
		Request.Temperature = 0.7f;
		Request.MaxTokens = 2048;
	}

	return Request;
}

void UUnrealAIWidget::SendAIRequest(const FAIRequest& Request)
{
	if (AIService)
	{
		AIService->SendAIRequest(Request);
	}
}

void UUnrealAIWidget::ShowResponseDialog(const FString& Content, const FString& Title)
{
	FText DialogText = FText::FromString(Content);
	FText DialogTitle = FText::FromString(Title);
	FMessageDialog::Open(EAppMsgType::Ok, DialogText, DialogTitle);
}

void UUnrealAIWidget::CopyToClipboard(const FString& Text)
{
	// For now, just show a message that copy functionality is available
	// In a full implementation, this would use the platform clipboard
	UE_LOG(LogTemp, Log, TEXT("Copy to clipboard requested for: %s"), *Text);
}

void UUnrealAIWidget::SaveResponseToFile(const FString& Content)
{
	// Create a default filename with timestamp
	FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	FString DefaultFilename = FString::Printf(TEXT("UnrealAI_Response_%s.txt"), *Timestamp);
	
	// Get the project's saved directory
	FString ProjectDir = FPaths::ProjectSavedDir();
	FString FilePath = FPaths::Combine(ProjectDir, TEXT("UnrealAI"), DefaultFilename);
	
	// Ensure directory exists
	FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree(*Directory);
	
	// Save the file
	if (FFileHelper::SaveStringToFile(Content, *FilePath))
	{
		ShowSuccess(FString::Printf(TEXT("Response saved to: %s"), *FilePath));
	}
	else
	{
		ShowError(TEXT("Failed to save response to file"));
	}
}

FString UUnrealAIWidget::GetRequestTypeDescription(EAIRequestType RequestType)
{
	switch (RequestType)
	{
	case EAIRequestType::General:
		return TEXT("General Query");
	case EAIRequestType::BlueprintGeneration:
		return TEXT("Blueprint Generation");
	case EAIRequestType::CPPGeneration:
		return TEXT("C++ Generation");
	case EAIRequestType::CodeReview:
		return TEXT("Code Review");
	case EAIRequestType::Documentation:
		return TEXT("Documentation");
	default:
		return TEXT("Unknown");
	}
}

FString UUnrealAIWidget::GetProviderDescription(EAIProvider Provider)
{
	switch (Provider)
	{
	case EAIProvider::LocalLLM:
		return TEXT("Local LLM");
	case EAIProvider::Claude:
		return TEXT("Claude API");
	case EAIProvider::OpenAI:
		return TEXT("OpenAI API");
	default:
		return TEXT("Unknown");
	}
}
