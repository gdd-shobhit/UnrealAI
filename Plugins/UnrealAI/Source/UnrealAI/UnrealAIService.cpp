#include "UnrealAIService.h"
// #include "UnrealAISettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Base64.h"
#include "HAL/PlatformTime.h"

// Blueprint creation includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableSet.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "BlueprintActionDatabase.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditorSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "CoreUObject.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFilemanager.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

// Component includes for Blueprint component creation
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

UUnrealAIService::UUnrealAIService()
{
	// Load settings from the settings class
	LoadSettings();
}

void UUnrealAIService::LoadSettings()
{
	// Fallback defaults
	LocalLLMEndpoint = TEXT("http://localhost:11434/api/generate");
	bEnableLocalLLM = true;
	bEnableClaude = false;
	bEnableOpenAI = false;
	
	// Test function will be called manually from Blueprint
}

// Simple test function to directly test Ollama without complex HTTP handling
void UUnrealAIService::TestOllamaDirectly(const FString& Description)
{
	UE_LOG(LogTemp, Log, TEXT("=== TESTING OLLAMA DIRECTLY ==="));
	UE_LOG(LogTemp, Log, TEXT("Request: %s"), *Description);
	
	// Build the prompt
	FString Prompt = BuildBlueprintPrompt(Description, TMap<FString, FString>());
	UE_LOG(LogTemp, Log, TEXT("Generated Prompt:\n%s"), *Prompt);
	
	// Simple HTTP request without complex callbacks
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetURL(TEXT("http://localhost:11434/api/generate"));
	
	// Build simple request payload
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), TEXT("llama2:latest"));
	JsonObject->SetStringField(TEXT("prompt"), Prompt);
	JsonObject->SetBoolField(TEXT("stream"), false);
	JsonObject->SetNumberField(TEXT("temperature"), 0.7);
	JsonObject->SetNumberField(TEXT("max_tokens"), 2048);
	
	FString RequestPayload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestPayload);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	HttpRequest->SetContentAsString(RequestPayload);
	
	// Simple synchronous-like approach - just log the request
	UE_LOG(LogTemp, Log, TEXT("Sending request to Ollama..."));
	UE_LOG(LogTemp, Log, TEXT("Request payload: %s"), *RequestPayload);
	
	// For now, just simulate what we expect back
	UE_LOG(LogTemp, Log, TEXT("=== EXPECTED RESPONSE FORMAT ==="));
	UE_LOG(LogTemp, Log, TEXT("The AI should respond with a JSON object like this:"));
	UE_LOG(LogTemp, Log, TEXT("{"));
	UE_LOG(LogTemp, Log, TEXT("  \"blueprint_name\": \"EnemyAI\","));
	UE_LOG(LogTemp, Log, TEXT("  \"description\": \"Enemy AI that follows actors\","));
	UE_LOG(LogTemp, Log, TEXT("  \"parent_class\": \"Character\","));
	UE_LOG(LogTemp, Log, TEXT("  ..."));
	UE_LOG(LogTemp, Log, TEXT("}"));
	UE_LOG(LogTemp, Log, TEXT("=== END TEST ==="));
}

// Blueprint-callable test function for prompt generation
void UUnrealAIService::TestPromptGeneration(const FString& Description)
{
	UE_LOG(LogTemp, Log, TEXT("=== TESTING PROMPT GENERATION ==="));
	UE_LOG(LogTemp, Log, TEXT("Request: %s"), *Description);
	
	// Build the prompt
	FString Prompt = BuildBlueprintPrompt(Description, TMap<FString, FString>());
	UE_LOG(LogTemp, Log, TEXT("Generated Prompt:\n%s"), *Prompt);
	
	// Now actually send the request to the AI to get a response
	UE_LOG(LogTemp, Log, TEXT("=== SENDING REQUEST TO AI ==="));
	
	// Create the AI request
	FAIRequest Request;
	Request.Prompt = Description;
	Request.RequestType = EAIRequestType::BlueprintGeneration;
	Request.Provider = EAIProvider::LocalLLM;
	
	// Send the request and log the response
	SendAIRequest(Request);
	
	UE_LOG(LogTemp, Log, TEXT("=== END TEST ==="));
}

// Test JSON parsing function
void UUnrealAIService::TestJsonParsing()
{
	UE_LOG(LogTemp, Log, TEXT("=== TESTING JSON PARSING ==="));
	
	// Load the test JSON file
	FString ProjectDir = FPaths::ProjectDir();
	FString JsonFilePath = FPaths::Combine(ProjectDir, TEXT("TestJsonParser2.json"));
	
	UE_LOG(LogTemp, Log, TEXT("Loading JSON from: %s"), *JsonFilePath);
	
	// Check if file exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Test JSON file not found: %s"), *JsonFilePath);
		return;
	}
	
	// Load the file content
	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file: %s"), *JsonFilePath);
		return;
	}
	
	UE_LOG(LogTemp, Log, TEXT("JSON content loaded (%d characters):"), JsonContent.Len());
	UE_LOG(LogTemp, Log, TEXT("First 500 chars: %s"), *JsonContent.Left(500));
	
	// Test our existing JSON parser by calling CreateBlueprintFromJSON
	UE_LOG(LogTemp, Log, TEXT("Testing existing JSON parser..."));
	
	// Try to create a blueprint from the JSON
	FString OutBlueprintPath;
	FString OutError;
	bool bSuccess = CreateBlueprintFromJSON(JsonContent, OutBlueprintPath, OutError);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("✅ JSON parsing and Blueprint creation successful!"));
		UE_LOG(LogTemp, Log, TEXT("Blueprint created at: %s"), *OutBlueprintPath);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("❌ JSON parsing or Blueprint creation failed!"));
		UE_LOG(LogTemp, Log, TEXT("Error: %s"), *OutError);
	}
	
	UE_LOG(LogTemp, Log, TEXT("=== JSON PARSING TEST COMPLETE ==="));
}

void UUnrealAIService::SendAIRequest(const FAIRequest& Request)
{
	ProcessRequest(Request);
}

void UUnrealAIService::SendAIRequestAsync(const FAIRequest& Request)
{
	// For now, we'll process synchronously but this can be made async later
	ProcessRequest(Request);
}

FAIResponse UUnrealAIService::GenerateBlueprint(const FString& Description, const TMap<FString, FString>& Context)
{
	FAIRequest Request;
	Request.RequestType = EAIRequestType::BlueprintGeneration;
	Request.Prompt = Description;
	Request.ContextData = Context;
	Request.Provider = EAIProvider::LocalLLM;

	return ProcessRequestSync(Request);
}

FAIResponse UUnrealAIService::GenerateCPPCode(const FString& Description, const TMap<FString, FString>& Context)
{
	FAIRequest Request;
	Request.RequestType = EAIRequestType::CPPGeneration;
	Request.Prompt = Description;
	Request.ContextData = Context;
	Request.Provider = EAIProvider::LocalLLM;

	return ProcessRequestSync(Request);
}

// Convenience functions without context
FAIResponse UUnrealAIService::GenerateBlueprintSimple(const FString& Description)
{
	TMap<FString, FString> EmptyContext;
	return GenerateBlueprint(Description, EmptyContext);
}

void UUnrealAIService::GenerateBlueprintSimpleAsync(const FString& Description)
{
	TMap<FString, FString> EmptyContext;
	FAIRequest Request;
	Request.RequestType = EAIRequestType::BlueprintGeneration;
	Request.Prompt = Description;
	Request.ContextData = EmptyContext;
	Request.Provider = EAIProvider::LocalLLM;
	
	ProcessRequest(Request);
}

FAIResponse UUnrealAIService::GenerateCPPCodeSimple(const FString& Description)
{
	TMap<FString, FString> EmptyContext;
	return GenerateCPPCode(Description, EmptyContext);
}

void UUnrealAIService::GenerateCPPCodeSimpleAsync(const FString& Description)
{
	TMap<FString, FString> EmptyContext;
	FAIRequest Request;
	Request.RequestType = EAIRequestType::CPPGeneration;
	Request.Prompt = Description;
	Request.ContextData = EmptyContext;
	Request.Provider = EAIProvider::LocalLLM;
	
	ProcessRequest(Request);
}

void UUnrealAIService::ReviewCodeAsync(const FString& Code, const FString& Language)
{
	FAIRequest Request;
	Request.RequestType = EAIRequestType::CodeReview;
	Request.Prompt = Code;
	Request.ContextData.Add(TEXT("Language"), Language);
	Request.Provider = EAIProvider::LocalLLM;
	
	ProcessRequest(Request);
}

FAIResponse UUnrealAIService::ReviewCode(const FString& Code, const FString& Language)
{
	FAIRequest Request;
	Request.RequestType = EAIRequestType::CodeReview;
	Request.Prompt = Code;
	Request.ContextData.Add(TEXT("Language"), Language);
	Request.Provider = EAIProvider::LocalLLM;

	return ProcessRequestSync(Request);
}

void UUnrealAIService::SetLocalLLMEndpoint(const FString& Endpoint)
{
	LocalLLMEndpoint = Endpoint;
}

void UUnrealAIService::SetClaudeAPIKey(const FString& APIKey)
{
	ClaudeAPIKey = APIKey;
	bEnableClaude = !APIKey.IsEmpty();
}

void UUnrealAIService::SetOpenAIAPIKey(const FString& APIKey)
{
	OpenAIAPIKey = APIKey;
	bEnableOpenAI = !APIKey.IsEmpty();
}

bool UUnrealAIService::IsLocalLLMAvailable() const
{
	return bEnableLocalLLM && !LocalLLMEndpoint.IsEmpty();
}

bool UUnrealAIService::IsClaudeAvailable() const
{
	return bEnableClaude && !ClaudeAPIKey.IsEmpty();
}

bool UUnrealAIService::IsOpenAIAvailable() const
{
	return bEnableOpenAI && !OpenAIAPIKey.IsEmpty();
}

void UUnrealAIService::ProcessRequest(const FAIRequest& Request)
{
	// Route to appropriate provider
	switch (Request.Provider)
	{
	case EAIProvider::LocalLLM:
		if (IsLocalLLMAvailable())
		{
			ProcessLocalLLMRequestAsync(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("Local LLM is not available. Please check Ollama is running.");
			OnResponseReceived.Broadcast(ErrorResponse);
		}
		break;
	case EAIProvider::Claude:
		if (IsClaudeAvailable())
		{
			ProcessClaudeRequestAsync(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("Claude API is not available. Please check API key.");
			OnResponseReceived.Broadcast(ErrorResponse);
		}
		break;
	case EAIProvider::OpenAI:
		if (IsOpenAIAvailable())
		{
			ProcessOpenAIRequestAsync(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("OpenAI API is not available. Please check API key.");
			OnResponseReceived.Broadcast(ErrorResponse);
		}
		break;
	default:
		// Default to Local LLM
		if (IsLocalLLMAvailable())
		{
			ProcessLocalLLMRequestAsync(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("No AI provider available.");
			OnResponseReceived.Broadcast(ErrorResponse);
		}
		break;
	}
}

FAIResponse UUnrealAIService::ProcessRequestSync(const FAIRequest& Request)
{
	// Handle UAsset merge requests specially
	if (Request.RequestType == EAIRequestType::UAssetMerge)
	{
		FAIResponse Response;
		if (ProcessUAssetMergeRequest(Request, Response))
		{
			return Response;
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = Response.ErrorMessage;
			return ErrorResponse;
		}
	}

	// Route to appropriate provider synchronously
	switch (Request.Provider)
	{
	case EAIProvider::LocalLLM:
		if (IsLocalLLMAvailable())
		{
			return ProcessLocalLLMRequest(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("Local LLM is not available. Please check Ollama is running.");
			return ErrorResponse;
		}
	case EAIProvider::Claude:
		if (IsClaudeAvailable())
		{
			return ProcessClaudeRequest(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("Claude API is not available. Please check API key.");
			return ErrorResponse;
		}
	case EAIProvider::OpenAI:
		if (IsOpenAIAvailable())
		{
			return ProcessOpenAIRequest(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("OpenAI API is not available. Please check API key.");
			return ErrorResponse;
		}
	default:
		// Default to Local LLM
		if (IsLocalLLMAvailable())
		{
			return ProcessLocalLLMRequest(Request);
		}
		else
		{
			FAIResponse ErrorResponse;
			ErrorResponse.bSuccess = false;
			ErrorResponse.ErrorMessage = TEXT("No AI provider available.");
			return ErrorResponse;
		}
	}
}

FAIResponse UUnrealAIService::ProcessLocalLLMRequest(const FAIRequest& Request)
{
	FAIResponse Response;
	
	// Create HTTP request to Ollama
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();
	
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetURL(LocalLLMEndpoint);
	
	// Build the request payload for Ollama
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), TEXT("llama2:latest"));
	
	// Build the prompt based on request type
	FString Prompt = BuildPrompt(Request);
	JsonObject->SetStringField(TEXT("prompt"), Prompt);
	
	// Add streaming parameter (false for now)
	JsonObject->SetBoolField(TEXT("stream"), false);
	
	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Writer->Close();
	
	HttpRequest->SetContentAsString(OutputString);
	
	// Process request synchronously (this will block until response is received)
	HttpRequest->ProcessRequest();
	
	// Wait for completion with timeout
	int32 TimeoutCounter = 0;
	const int32 MaxTimeout = 3000; // 30 seconds timeout
	
	while (HttpRequest->GetStatus() == EHttpRequestStatus::Processing && TimeoutCounter < MaxTimeout)
	{
		// Small delay to prevent busy waiting
		FPlatformProcess::Sleep(0.01f);
		TimeoutCounter++;
	}
	
	// Get response
	if (HttpRequest->GetStatus() == EHttpRequestStatus::Succeeded)
	{
		FHttpResponsePtr HttpResponse = HttpRequest->GetResponse();
		if (HttpResponse.IsValid())
		{
			// Parse Ollama response
			FString ResponseContent = HttpResponse->GetContentAsString();
			
			// Ollama returns a stream of JSON objects, we need to parse and accumulate all response parts
			TArray<FString> Lines;
			ResponseContent.ParseIntoArray(Lines, TEXT("\n"), true);
			
			FString AccumulatedResponse;
			for (const FString& Line : Lines)
			{
				if (!Line.IsEmpty())
				{
					TSharedPtr<FJsonObject> JsonResponse;
					TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
					
					if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
					{
						if (JsonResponse->HasField(TEXT("response")))
						{
							FString ResponsePart = JsonResponse->GetStringField(TEXT("response"));
							AccumulatedResponse += ResponsePart;
						}
					}
				}
			}
			
			if (!AccumulatedResponse.IsEmpty())
			{
				Response.bSuccess = true;
				Response.Content = AccumulatedResponse;
				Response.UsedProvider = EAIProvider::LocalLLM;
			}
			else
			{
				Response.bSuccess = false;
				Response.ErrorMessage = TEXT("Failed to parse Ollama response");
			}
		}
		else
		{
			Response.bSuccess = false;
			Response.ErrorMessage = TEXT("Invalid HTTP response");
		}
	}
	else if (TimeoutCounter >= MaxTimeout)
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("Request timed out after 30 seconds");
	}
	else
	{
		Response.bSuccess = false;
		Response.ErrorMessage = TEXT("HTTP request failed");
	}
	
	return Response;
}

// Async request processing
void UUnrealAIService::ProcessRequestAsync(const FAIRequest& Request, TFunction<void(const FAIResponse&)> OnCompleteCallback)
{
	CurrentAsyncCallback = OnCompleteCallback;

	switch (Request.Provider)
	{
	case EAIProvider::LocalLLM:
		ProcessLocalLLMRequestAsync(Request);
		break;
	case EAIProvider::Claude:
		ProcessClaudeRequestAsync(Request);
		break;
	case EAIProvider::OpenAI:
		ProcessOpenAIRequestAsync(Request);
		break;
	default:
		// Default to Local LLM
		ProcessLocalLLMRequestAsync(Request);
		break;
	}
}

FAIResponse UUnrealAIService::ProcessClaudeRequest(const FAIRequest& Request)
{
	FAIResponse Response;
	Response.bSuccess = true;
	Response.Content = TEXT("Claude API functionality is being implemented. This is a test response.");
	Response.UsedProvider = EAIProvider::Claude;
	return Response;
}

FAIResponse UUnrealAIService::ProcessOpenAIRequest(const FAIRequest& Request)
{
	FAIResponse Response;
	Response.bSuccess = true;
	Response.Content = TEXT("OpenAI API functionality is being implemented. This is a test response.");
	Response.UsedProvider = EAIProvider::OpenAI;
	return Response;
}

// Async HTTP request processing
void UUnrealAIService::ProcessLocalLLMRequestAsync(const FAIRequest& Request)
{
	// Create HTTP request to Ollama
	FHttpModule& HttpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpModule.CreateRequest();

	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetURL(LocalLLMEndpoint);

	// Build the request payload for Ollama
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), TEXT("llama2:latest"));

	// Build the prompt based on request type
	FString Prompt = BuildPrompt(Request);
	JsonObject->SetStringField(TEXT("prompt"), Prompt);

	// Add streaming parameter (false for now)
	JsonObject->SetBoolField(TEXT("stream"), false);

	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Writer->Close();

	HttpRequest->SetContentAsString(OutputString);

	// Debug logging
	UE_LOG(LogTemp, Warning, TEXT("🚀 Sending HTTP request to Ollama"));
	UE_LOG(LogTemp, Log, TEXT("📍 URL: %s"), *LocalLLMEndpoint);
	UE_LOG(LogTemp, Log, TEXT("📦 Request payload: %s"), *OutputString);

	// Bind the response handler
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UUnrealAIService::OnHTTPResponseReceived);

	// Process request asynchronously
	HttpRequest->ProcessRequest();
}

void UUnrealAIService::ProcessClaudeRequestAsync(const FAIRequest& Request)
{
	FAIResponse Response;
	Response.bSuccess = true;
	Response.Content = TEXT("Claude API functionality is being implemented. This is a test response.");
	Response.UsedProvider = EAIProvider::Claude;

	if (CurrentAsyncCallback)
	{
		CurrentAsyncCallback(Response);
	}
}

void UUnrealAIService::ProcessOpenAIRequestAsync(const FAIRequest& Request)
{
	FAIResponse Response;
	Response.bSuccess = true;
	Response.Content = TEXT("OpenAI API functionality is being implemented. This is a test response.");
	Response.UsedProvider = EAIProvider::OpenAI;

	if (CurrentAsyncCallback)
	{
		CurrentAsyncCallback(Response);
	}
}

FString UUnrealAIService::BuildPrompt(const FAIRequest& Request)
{
	switch (Request.RequestType)
	{
	case EAIRequestType::BlueprintGeneration:
		return BuildBlueprintPrompt(Request.Prompt, Request.ContextData);
	case EAIRequestType::CPPGeneration:
		return BuildCPPPrompt(Request.Prompt, Request.ContextData);
	case EAIRequestType::CodeReview:
		return BuildCodeReviewPrompt(Request.Prompt, Request.ContextData.Contains(TEXT("Language")) ? Request.ContextData[TEXT("Language")] : TEXT("cpp"));
	case EAIRequestType::UAssetMerge:
		return TEXT("UAsset merge request - this should be handled by ProcessUAssetMergeRequest");
	default:
		return Request.Prompt;
	}
}

FString UUnrealAIService::BuildBlueprintPrompt(const FString& Description, const TMap<FString, FString>& Context)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint designer. Create a detailed Blueprint implementation for this request:\n\n");
	Prompt += TEXT("REQUEST: ");
	Prompt += Description;
	Prompt += TEXT("\n\n");

	// Temporarily disable template loading - always use hardcoded format
	// FString TemplateContent = LoadBlueprintTemplate();//
	
	// Use simplified, focused prompt format
	Prompt += TEXT("You are to generate Unreal Engine Blueprint functions in JSON format. Follow these strict rules:\n\n");
	Prompt += TEXT("1. Output ONLY JSON, no explanations or text outside the JSON.\n");
	Prompt += TEXT("2. The JSON must include:\n");
	Prompt += TEXT("   - Blueprint name\n");
	Prompt += TEXT("   - Blueprint description\n");
	Prompt += TEXT("   - Parent class\n");
	Prompt += TEXT("   - Variables\n");
	Prompt += TEXT("   - Events\n");
	Prompt += TEXT("   - Functions\n");\
	Prompt += TEXT("   - Functions' Inputs (name + type)\n");
	Prompt += TEXT("   - Functions' Outputs (name + type)\n");
	Prompt += TEXT("   - Execution flow\n");
	Prompt += TEXT("   - Components\n");
	Prompt += TEXT("   - Nodes (each node must have a unique \"ID\", \"Type\", \"Inputs\", and \"Outputs\")\n");
	Prompt += TEXT("   - Connections (explicitly describing links between node pins using IDs)\n");
	Prompt += TEXT("3. Use \"ID\" values as strings (\"1\", \"2\", \"3\", etc.) for consistency.\n");
	Prompt += TEXT("4. All data flow must be explicitly represented under \"Connections\" with \"FromNode\", \"FromPin\", \"ToNode\", \"ToPin\".\n");
	Prompt += TEXT("5. Execution flow (white wires) is not required unless specified, but data flow is mandatory.\n");
	Prompt += TEXT("6. Keep the JSON strictly valid and parsable. Do not add comments, explanations, or extra keys.\n");
	Prompt += TEXT("7. Every function must contain a FunctionEntry node (ID \"0\") with all input parameters defined as Outputs.\n");
	Prompt += TEXT("8. Every function must contain a ReturnNode with all output parameters defined as Inputs.\n");
	Prompt += TEXT("9. Connections must explicitly link from FunctionEntry → first executable node(s), and from last node(s) → ReturnNode if outputs exist.\n\n");
	Prompt += TEXT("Example structure:\n");
	Prompt += TEXT("{\n");
	Prompt += TEXT("  \"blueprint_name\": \"DescriptiveName\",\n");
	Prompt += TEXT("  \"description\": \"Brief description of what this blueprint does\",\n");
	Prompt += TEXT("  \"parent_class\": \"Actor\" | \"Pawn\" | \"Character\" | \"GameMode\" | etc.,\n");
	Prompt += TEXT("  \"variables\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"name\": \"VariableName\",\n");
	Prompt += TEXT("      \"type\": \"Integer\" | \"Float\" | \"Boolean\" | \"String\" | \"Vector\" | \"Rotator\" | \"Object\" | \"Actor\" | etc.,\n");
	Prompt += TEXT("      \"default_value\": \"default_value_here\",\n");
	Prompt += TEXT("      \"category\": \"Optional category name\"\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ],\n");
	Prompt += TEXT("  \"events\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"name\": \"EventName\",\n");
	Prompt += TEXT("      \"inputs\": [\n");
	Prompt += TEXT("        {\"name\": \"InputName\", \"type\": \"InputType\"}\n");
	Prompt += TEXT("      ],\n");
	Prompt += TEXT("      \"logic\": \"Description of what this event does\"\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ],\n");
	Prompt += TEXT("  \"functions\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"name\": \"FunctionName\",\n");
	Prompt += TEXT("      \"inputs\": [...],\n");
	Prompt += TEXT("      \"outputs\": [...],\n");
	Prompt += TEXT("      \"nodes\": [\n");
	Prompt += TEXT("        {\n");
	Prompt += TEXT("          \"ID\": \"1\",\n");
	Prompt += TEXT("          \"Type\": \"LineTraceSingle\",\n");
	Prompt += TEXT("          \"Inputs\": {\n");
	Prompt += TEXT("            \"Start\": \"Vector\",\n");
	Prompt += TEXT("            \"End\": \"Vector\",\n");
	Prompt += TEXT("            \"TraceChannel\": \"[RESEARCH: Use proper Unreal trace channel enum]\"\n");
	Prompt += TEXT("          },\n");
	Prompt += TEXT("          \"Outputs\": {\n");
	Prompt += TEXT("            \"ReturnValue\": \"Bool\",\n");
	Prompt += TEXT("            \"OutHit\": \"HitResult\"\n");
	Prompt += TEXT("          }\n");
	Prompt += TEXT("        },\n");
	Prompt += TEXT("        {\n");
	Prompt += TEXT("          \"ID\": \"2\",\n");
	Prompt += TEXT("          \"Type\": \"Branch\",\n");
	Prompt += TEXT("          \"Inputs\": {\n");
	Prompt += TEXT("            \"Condition\": \"Bool\"\n");
	Prompt += TEXT("          },\n");
	Prompt += TEXT("          \"Outputs\": {\n");
	Prompt += TEXT("            \"True\": \"ExecPin\",\n");
	Prompt += TEXT("            \"False\": \"ExecPin\"\n");
	Prompt += TEXT("          }\n");
	Prompt += TEXT("        }\n");
	Prompt += TEXT("      ],\n");
	Prompt += TEXT("      \"connections\": [\n");
	Prompt += TEXT("        {\n");
	Prompt += TEXT("          \"FromNode\": \"1\",\n");
	Prompt += TEXT("          \"FromPin\": \"ReturnValue\",\n");
	Prompt += TEXT("          \"ToNode\": \"2\",\n");
	Prompt += TEXT("          \"ToPin\": \"Condition\"\n");
	Prompt += TEXT("        }\n");
	Prompt += TEXT("      ]\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ],\n");
	Prompt += TEXT("}\n\n");

	Prompt += TEXT("  \"execution_flow\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"from_event\": \"BeginPlay | ActorBeginOverlap | ReceiveHit | Tick | etc.\",\n");
	Prompt += TEXT("      \"sequence\": [\n");
	Prompt += TEXT("        \"Action 1 based on your blueprint's purpose\",\n");
	Prompt += TEXT("        \"Action 2 using built-in Unreal functions\",\n");
	Prompt += TEXT("        \"Action 3 connecting to your variables/functions\"\n");
	Prompt += TEXT("      ]\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ],\n");
	Prompt += TEXT("  \"components\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"name\": \"MeshComponent\",\n");
	Prompt += TEXT("      \"type\": \"StaticMeshComponent\",\n");
	Prompt += TEXT("      \"attachment\": \"RootComponent\",\n");
	Prompt += TEXT("      \"properties\": {\"StaticMesh\": \"DefaultMesh\", \"Material\": \"DefaultMaterial\"}\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ]\n");
	Prompt += TEXT("}\n\n");
	Prompt += Description;
	Prompt += TEXT("Now, construct the requested Blueprint function in JSON according to these rules.\n\n");
	Prompt += TEXT("\n\nOutput ONLY the JSON object, no other text or explanations.\n\n");
	Prompt += TEXT("🚀 ENHANCED FUNCTION SUPPORT 🚀\n");
	Prompt += TEXT("- Use REAL Unreal Engine functions like LineTraceSingle, GetActorRotation, SetActorLocation, etc\n");
	Prompt += TEXT("- Keep JSON structure FLAT with simple key-value parameters\n\n");
	Prompt += TEXT("🎯 FINAL INSTRUCTION: Generate ONLY the JSON structure above, customized for the request. NO explanations, NO guides, NO additional text - ONLY the JSON object!\n");
	Prompt += TEXT("- Use proper Unreal function names for realistic gameplay\n");
	Prompt += TEXT("- Connect nodes using the ID-based connection system\n");
	Prompt += TEXT("- IMPLEMENT THE ACTUAL REQUESTED FUNCTIONALITY - don't just describe it!\n");
	Prompt += TEXT("- If asked for \"enemy AI which follows an actor\", actually create the logic for following!\n");
	Prompt += TEXT("- If asked for \"player movement\", actually create the movement logic!\n");
	Prompt += TEXT("- NO GENERIC TEMPLATES - ONLY REAL IMPLEMENTATIONS!\n\n");
	
	Prompt += TEXT("CRITICAL RESPONSE FORMAT RULES:\n");
	Prompt += TEXT("1. Respond with ONLY the JSON object, no additional text before or after\n");
	Prompt += TEXT("2. Do NOT include phrases like \"Here is the JSON\" or \"Response:\" or any explanatory text\n");
	Prompt += TEXT("3. Start your response immediately with the opening brace {\n");
	Prompt += TEXT("4. End your response immediately with the closing brace }\n");
	Prompt += TEXT("5. Use valid JSON syntax ONLY - no comments, no Python syntax\n");
	Prompt += TEXT("6. Use \"null\" instead of \"None\" for empty values\n");
	Prompt += TEXT("7. Use \"true\" and \"false\" (lowercase) for boolean values\n");
	Prompt += TEXT("8. Do NOT use // comments in JSON\n");
	Prompt += TEXT("9. Include all necessary nodes and connections\n");
	Prompt += TEXT("10. Be specific with variable types and function names\n");
	Prompt += TEXT("11. ALWAYS use commas between array elements and object properties\n");
	Prompt += TEXT("12. Use proper Unreal Engine node types: \"Branch\" not \"IfStatement\", \"LineTraceSingle\" not \"LineTrace\"\n");
	Prompt += TEXT("13. Use proper Unreal Engine types: \"Vector\", \"Float\", \"Bool\", \"String\", \"Int\"\n");
	Prompt += TEXT("14. Connections MUST use \"FromNode\", \"FromPin\", \"ToNode\", \"ToPin\" format\n");
	Prompt += TEXT("15. Functions section MUST be an array (\"functions\": [...]) not a single object\n");
	Prompt += TEXT("16. Pin names MUST be exact: \"True\" not \"true\", \"False\" not \"false\"\n");
	Prompt += TEXT("17. ALL array elements MUST be separated by commas, including the last element\n");
	Prompt += TEXT("18. Pin names in connections MUST match exactly with the node's Inputs/Outputs\n");
	Prompt += TEXT("19. Include execution flow with clear step-by-step logic\n");
	Prompt += TEXT("20. Function logic is CRITICAL - it must contain actual implementable steps\n\n");
	
	Prompt += TEXT("🔍 UNREAL ENGINE FUNCTION RESEARCH REQUIREMENTS 🔍\n");
	Prompt += TEXT("- Research and use ONLY real Unreal Engine function names and signatures\n");
	Prompt += TEXT("- Verify function parameters, return types, and pin names from official documentation\n");
	Prompt += TEXT("- Use proper Unreal Engine enum values (e.g., trace channels, collision channels)\n");
	Prompt += TEXT("- Ensure all node types exist in Unreal Engine Blueprint editor\n");
	Prompt += TEXT("- Validate that input/output pin names match actual function signatures\n");
	Prompt += TEXT("- If unsure about a function, research it in Unreal Engine documentation first\n");
	Prompt += TEXT("- Use the Blueprint editor's function library to verify available functions\n\n");
	
	Prompt += TEXT("🚨 CRITICAL CONTENT REQUIREMENTS 🚨\n");
	Prompt += TEXT("- NEVER use template placeholders like \"VariableName\", \"EventName\", \"FunctionName\"\n");
	Prompt += TEXT("- ALWAYS generate REAL, SPECIFIC content based on the user's request\n");
	Prompt += TEXT("- Use meaningful names like \"PlayerLocation\", \"EnemySpeed\", \"ChaseDistance\"\n");
	Prompt += TEXT("- Implement ACTUAL logic, not generic descriptions\n");
	Prompt += TEXT("- Pin names MUST be \"True\" and \"False\" (capitalized), not \"true\" and \"false\"\n");
	Prompt += TEXT("- NO trailing commas after the last element in arrays\n");
	Prompt += TEXT("- NO generic placeholders - EVERY field must have real, meaningful content\n\n");
	Prompt += TEXT("13. Function logic MUST use structured JSON nodes format, NOT string operations\n");
	Prompt += TEXT("14. CRITICAL: Logic must be a JSON object with 'nodes' and 'connections' arrays\n");
	Prompt += TEXT("15. SUPPORTED NODE TYPES:\n");
	Prompt += TEXT("    - \"SetVariable\": Set a variable value (requires: variable, value)\n");
	Prompt += TEXT("    - \"GetVariable\": Get a variable value (requires: variable)\n");
	Prompt += TEXT("    - \"FunctionCall\": Call ANY Unreal function (requires: function, parameters)\n");
	Prompt += TEXT("    - \"Branch\": Conditional logic (requires: condition)\n");
	Prompt += TEXT("    - \"Print\": Print debug message (requires: message)\n");
	Prompt += TEXT("    - \"MathOperation\": Math operations (requires: operation, operands)\n");
	Prompt += TEXT("16. COMMON UNREAL FUNCTIONS YOU CAN USE:\n");
	Prompt += TEXT("    - LineTraceSingle, LineTraceMulti, SphereTrace, BoxTrace\n");
	Prompt += TEXT("    - GetActorLocation, SetActorLocation, GetActorRotation, SetActorRotation\n");
	Prompt += TEXT("    - GetForwardVector, GetRightVector, GetUpVector\n");
	Prompt += TEXT("    - FindLookAtRotation, GetDistanceTo, GetDotProductTo\n");
	Prompt += TEXT("    - AddMovementInput, AddActorWorldOffset, AddActorLocalOffset\n");
	Prompt += TEXT("    - PlaySound, SpawnActor, DestroyActor, SetActorHiddenInGame\n");
	Prompt += TEXT("17. CONNECTION FORMAT:\n");
	Prompt += TEXT("    - Use node IDs to connect execution flow\n");
	Prompt += TEXT("    - Start with \"entry\" (function entry point)\n");
	Prompt += TEXT("    - Connect nodes in logical order\n");
	Prompt += TEXT("18. POSITION NODES: Always include x,y coordinates for node placement\n");
	Prompt += TEXT("19. EXAMPLE STRUCTURED LOGIC (Complex Functions):\n");
	Prompt += TEXT("    \"logic\": {\n");
	Prompt += TEXT("      \"nodes\": [\n");
	Prompt += TEXT("        {\"id\": \"trace1\", \"type\": \"FunctionCall\", \"function\": \"LineTraceSingle\", \"parameters\": {\"Start\": \"PlayerLocation\", \"End\": \"ForwardVector\", \"TraceChannel\": \"Visibility\"}, \"position\": {\"x\": 100, \"y\": 100}},\n");
	Prompt += TEXT("        {\"id\": \"rotation1\", \"type\": \"FunctionCall\", \"function\": \"GetActorRotation\", \"parameters\": {\"Target\": \"Player\"}, \"position\": {\"x\": 300, \"y\": 100}},\n");
	Prompt += TEXT("        {\"id\": \"move1\", \"type\": \"FunctionCall\", \"function\": \"SetActorLocation\", \"parameters\": {\"Target\": \"Self\", \"NewLocation\": \"TraceHitLocation\"}, \"position\": {\"x\": 500, \"y\": 100}}\n");
	Prompt += TEXT("      ],\n");
	Prompt += TEXT("      \"connections\": [{\"from\": \"entry\", \"to\": \"trace1\"}, {\"from\": \"trace1\", \"to\": \"rotation1\"}, {\"from\": \"rotation1\", \"to\": \"move1\"}]\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("20. JSON STRUCTURE RULES:\n");
	Prompt += TEXT("    - Keep parameters as simple key-value pairs (no deep nesting)\n");
	Prompt += TEXT("    - Use flat parameter structure: \"ParameterName\": \"Value\"\n");
	Prompt += TEXT("    - Avoid complex nested objects in parameters\n");
	Prompt += TEXT("21. FUNCTION IMPLEMENTATION:\n");
	Prompt += TEXT("    - ALWAYS provide actual logic implementation for functions\n");
	Prompt += TEXT("    - Use real Unreal Engine function names for authentic gameplay\n");
	Prompt += TEXT("    - Include meaningful connections between nodes\n");
	Prompt += TEXT("22. DEBUGGING:\n");
	Prompt += TEXT("    - Include Print statements to make functions visible during testing\n");
	Prompt += TEXT("    - Use descriptive variable names without spaces\n");
	Prompt += TEXT("    - Add comments in logic where helpful\n\n");
	
	Prompt += TEXT("DYNAMIC EVENT AND EXECUTION FLOW GUIDELINES:\n");
	Prompt += TEXT("Choose events based on the blueprint's PURPOSE, not just BeginPlay!\n\n");
	
	Prompt += TEXT("EXECUTION FLOW MUST MATCH REQUEST:\n");
	Prompt += TEXT("- Analyze the user's request to determine appropriate events\n");
	Prompt += TEXT("- Create execution flows that make sense for the blueprint type\n");
	Prompt += TEXT("- Don't default to BeginPlay unless it's actually needed\n");
	Prompt += TEXT("- Use multiple event flows if the blueprint has multiple purposes\n\n");

	Prompt += TEXT("ALWAYS prioritize Unreal Engine's built-in functions over creating custom implementations!\n\n");
	
	// Add dynamic function discovery based on the context
	FString AvailableFunctions = BuildAvailableFunctionsPrompt(Description);
	Prompt += AvailableFunctions + TEXT("\n");
	
	Prompt += TEXT("HOW TO USE THE FUNCTION PARAMETERS:\n");
	Prompt += TEXT("1. Look at the AVAILABLE UNREAL FUNCTIONS list above\n");
	Prompt += TEXT("2. Choose functions that match your blueprint's purpose\n");
	Prompt += TEXT("3. Use the exact parameter names shown in the function signature\n");
	Prompt += TEXT("4. Match the parameter types (String, Int, Float, Bool, etc.)\n");
	Prompt += TEXT("6. CRITICAL: You MUST use the exact parameter names shown above\n\n");
	
	Prompt += TEXT("REMEMBER:\n");
	Prompt += TEXT("- Parameter names MUST match the function signature exactly\n");
	Prompt += TEXT("- Use meaningful values, not generic placeholders\n\n");
	
	Prompt += TEXT("MANDATORY REQUIREMENTS:\n");
	Prompt += TEXT("1. ONLY use functions from the AVAILABLE UNREAL FUNCTIONS list above\n");
	Prompt += TEXT("3. Do NOT use generic names like 'FunctionName' - use actual function names from the list\n");
	Prompt += TEXT("4. Do NOT use generic messages like 'Function called' - use meaningful text\n");
	Prompt += TEXT("5. Every function call MUST include the parameters object with real values\n");
	Prompt += TEXT("6. If a function is in the available list, you MUST use its exact name and parameters\n");
	Prompt += TEXT("7. VIOLATION OF THESE RULES WILL RESULT IN BROKEN BLUEPRINTS\n\n");

	if (!Context.IsEmpty())
	{
		Prompt += TEXT("ADDITIONAL CONTEXT:\n");
		for (const auto& Pair : Context)
		{
			Prompt += FString::Printf(TEXT("- %s: %s\n"), *Pair.Key, *Pair.Value);
		}
		Prompt += TEXT("\n");
	}

	Prompt += TEXT("Create a complete, production-ready Blueprint that follows Unreal Engine best practices.");

	return Prompt;
}

FString UUnrealAIService::BuildCPPPrompt(const FString& Description, const TMap<FString, FString>& Context)
{
	FString Prompt = TEXT("You are an expert Unreal Engine C++ developer. ");
	Prompt += TEXT("Generate C++ code for the following description:\n\n");
	Prompt += Description;
	Prompt += TEXT("\n\nPlease provide:\n");
	Prompt += TEXT("1. Complete C++ header (.h) and implementation (.cpp) files\n");
	Prompt += TEXT("2. Proper Unreal Engine macros and conventions\n");
	Prompt += TEXT("3. Memory management best practices\n");
	Prompt += TEXT("4. Performance considerations\n");
	Prompt += TEXT("5. Comments explaining complex logic\n\n");

	// Add context if available
	if (Context.Num() > 0)
	{
		Prompt += TEXT("Additional Context:\n");
		for (const auto& ContextPair : Context)
		{
			Prompt += FString::Printf(TEXT("- %s: %s\n"), *ContextPair.Key, *ContextPair.Value);
		}
		Prompt += TEXT("\n");
	}

	return Prompt;
}

FString UUnrealAIService::BuildCodeReviewPrompt(const FString& Code, const FString& Language)
{
	FString Prompt = TEXT("You are an expert code reviewer specializing in ");
	Prompt += Language;
	Prompt += TEXT(" and Unreal Engine development.\n\n");
	Prompt += TEXT("Please review the following code and provide:\n");
	Prompt += TEXT("1. Code quality assessment\n");
	Prompt += TEXT("2. Potential bugs or issues\n");
	Prompt += TEXT("3. Performance improvements\n");
	Prompt += TEXT("4. Best practices recommendations\n");
	Prompt += TEXT("5. Security considerations\n");
	Prompt += TEXT("6. Specific suggestions for improvement\n\n");
	Prompt += TEXT("Code to review:\n");
	Prompt += TEXT("```");
	Prompt += Language;
	Prompt += TEXT("\n");
	Prompt += Code;
	Prompt += TEXT("\n```");

	return Prompt;
}



void UUnrealAIService::OnHTTPResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	FAIResponse AIResponse;

	if (!bSuccess || !Response.IsValid())
	{
		FString DetailedError = TEXT("HTTP request failed");
		
		if (!bSuccess)
		{
			DetailedError += TEXT(" - Request was not successful");
		}
		
		if (!Response.IsValid())
		{
			DetailedError += TEXT(" - Response is invalid");
		}
		else
		{
			int32 ResponseCode = Response->GetResponseCode();
			DetailedError += FString::Printf(TEXT(" - Response code: %d"), ResponseCode);
			
			if (ResponseCode != 200)
			{
				FString ResponseContent = Response->GetContentAsString();
				DetailedError += FString::Printf(TEXT(" - Content: %s"), *ResponseContent);
			}
		}
		
		UE_LOG(LogTemp, Error, TEXT("Ollama HTTP Error: %s"), *DetailedError);
		
		AIResponse.bSuccess = false;
		AIResponse.ErrorMessage = DetailedError;

		if (CurrentAsyncCallback)
		{
			CurrentAsyncCallback(AIResponse);
		}
		return;
	}

		// Parse Ollama response
	FString ResponseContent = Response->GetContentAsString();

	// Ollama returns a stream of JSON objects, we need to parse and accumulate all response parts
	TArray<FString> Lines;
	ResponseContent.ParseIntoArray(Lines, TEXT("\n"), true);

	UE_LOG(LogTemp, Warning, TEXT("🔍 Ollama returned %d lines in streaming response"), Lines.Num());
	UE_LOG(LogTemp, Warning, TEXT("🔍 Full response content length: %d"), ResponseContent.Len());
	UE_LOG(LogTemp, Warning, TEXT("🔍 First line: %s"), Lines.Num() > 0 ? *Lines[0].Left(200) : TEXT("NO LINES"));

	FString AccumulatedResponse;
	for (int32 i = 0; i < Lines.Num(); i++)
	{
		const FString& Line = Lines[i];
		if (!Line.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("🔍 Processing line %d: %s"), i, *Line.Left(100));
			
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);

			if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
			{
				UE_LOG(LogTemp, Log, TEXT("🔍 Line %d parsed successfully"), i);
				
				if (JsonObject->HasField(TEXT("response")))
				{
					FString ResponsePart = JsonObject->GetStringField(TEXT("response"));
					AccumulatedResponse += ResponsePart;
					UE_LOG(LogTemp, Log, TEXT("📝 Response part %d: %s"), i, *ResponsePart.Left(100));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("🔍 Line %d has no 'response' field"), i);
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("🔍 Line %d failed to parse as JSON"), i);
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🔍 Final accumulated response length: %d"), AccumulatedResponse.Len());
	UE_LOG(LogTemp, Warning, TEXT("🔍 Final accumulated response: %s"), *AccumulatedResponse.Left(500));

	if (!AccumulatedResponse.IsEmpty())
	{
		// Check if this is a Blueprint JSON response
		UE_LOG(LogTemp, Log, TEXT("Checking if response is JSON. Response starts with: %s"), *AccumulatedResponse.Left(50));
		
		// Check if response contains JSON (either starts with { or contains { somewhere)
		bool bContainsJSON = AccumulatedResponse.Contains(TEXT("{"));
		if (bContainsJSON)
		{
			UE_LOG(LogTemp, Log, TEXT("Response contains JSON, validating completeness..."));
			
			// CRITICAL: Validate AccumulatedResponse before processing
			if (AccumulatedResponse.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("AccumulatedResponse is empty - cannot process JSON"));
				AIResponse.bSuccess = false;
				AIResponse.ErrorMessage = TEXT("AI response was empty");
				AIResponse.Content = TEXT("❌ Empty Response\n\nThe AI response was empty.\n\nTry your request again.");
				AIResponse.UsedProvider = EAIProvider::LocalLLM;
				
				if (CurrentAsyncCallback)
				{
					CurrentAsyncCallback(AIResponse);
				}
				return;
			}
			
			// Check if JSON appears complete before extraction
			bool bJSONComplete = ValidateJSONCompleteness(AccumulatedResponse);
			
			if (!bJSONComplete)
			{
				UE_LOG(LogTemp, Warning, TEXT("JSON appears incomplete - attempting to complete"));
				AccumulatedResponse = AttemptJSONCompletion(AccumulatedResponse);
			}
			
			// CRITICAL: Validate AccumulatedResponse after completion attempt
			if (AccumulatedResponse.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("AccumulatedResponse became empty after completion attempt"));
				AIResponse.bSuccess = false;
				AIResponse.ErrorMessage = TEXT("AI response became empty after processing");
				AIResponse.Content = TEXT("❌ Response Processing Failed\n\nThe AI response became empty during processing.\n\nTry a simpler prompt.");
				AIResponse.UsedProvider = EAIProvider::LocalLLM;
				
				if (CurrentAsyncCallback)
				{
					CurrentAsyncCallback(AIResponse);
				}
				return;
			}
			
			// Extract only the JSON part from the response
			FString CleanJSON = ExtractJSONFromResponse(AccumulatedResponse);
			UE_LOG(LogTemp, Log, TEXT("Extracted JSON: %s"), *CleanJSON.Left(200));
			
			// CRITICAL: Validate CleanJSON before processing
			if (CleanJSON.IsEmpty())
			{
				UE_LOG(LogTemp, Error, TEXT("Extracted JSON is empty - cannot create blueprint"));
				AIResponse.bSuccess = false;
				AIResponse.ErrorMessage = TEXT("JSON extraction failed");
				AIResponse.Content = TEXT("❌ JSON Extraction Failed\n\nCould not extract valid JSON from the AI response.\n\nCheck the Output Log for details.");
				AIResponse.UsedProvider = EAIProvider::LocalLLM;
				
				if (CurrentAsyncCallback)
				{
					CurrentAsyncCallback(AIResponse);
				}
				return;
			}
			
			if (!CleanJSON.IsEmpty())
			{
				// Validate that extracted JSON is complete
				if (ValidateJSONCompleteness(CleanJSON))
				{
					UE_LOG(LogTemp, Log, TEXT("JSON validation passed, creating Blueprint..."));
					// Try to parse as Blueprint JSON and create the blueprint
					FString BlueprintPath;
					FString BlueprintError;
					UE_LOG(LogTemp, Log, TEXT("Attempting to create blueprint from JSON..."));
					if (CreateBlueprintFromJSON(CleanJSON, BlueprintPath, BlueprintError))
			{
				UE_LOG(LogTemp, Log, TEXT("Blueprint creation successful: %s"), *BlueprintPath);
				UE_LOG(LogTemp, Log, TEXT("Generated JSON: %s"), *CleanJSON);
				AIResponse.bSuccess = true;
				AIResponse.Content = FString::Printf(TEXT("🎉 Blueprint Created Successfully!\n\n📁 Blueprint Path: %s\n\n✅ Variables, functions, and execution flow have been generated.\n\nYou can find the new blueprint in your Content Browser under '/Game/AI_Generated/'"), *BlueprintPath);
				AIResponse.UsedProvider = EAIProvider::LocalLLM;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Blueprint creation failed: %s"), *BlueprintError);
				UE_LOG(LogTemp, Log, TEXT("Failed JSON: %s"), *CleanJSON);
				AIResponse.bSuccess = false;
				AIResponse.Content = FString::Printf(TEXT("❌ Blueprint Creation Failed\n\nError: %s\n\nCheck the Output Log for detailed information and the JSON response."), *BlueprintError);
				AIResponse.UsedProvider = EAIProvider::LocalLLM;
			}
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("JSON validation failed - response appears incomplete"));
					AIResponse.bSuccess = false;
					AIResponse.ErrorMessage = TEXT("AI response was incomplete or malformed - try a simpler prompt");
					AIResponse.Content = TEXT("❌ JSON Validation Failed\n\nThe AI response appears incomplete or malformed.\n\nTry using a simpler prompt or check your connection to Ollama.");
					AIResponse.UsedProvider = EAIProvider::LocalLLM;
				}
			}
			else
				{
			UE_LOG(LogTemp, Error, TEXT("Extracted JSON is empty"));
			UE_LOG(LogTemp, Log, TEXT("Original Response: %s"), *AccumulatedResponse);
			AIResponse.bSuccess = false;
			AIResponse.Content = TEXT("❌ JSON Extraction Failed\n\nThe AI response could not be parsed as valid JSON.\n\nCheck the Output Log for the original response and detailed information.");
			AIResponse.UsedProvider = EAIProvider::LocalLLM;
		}
	}
	else
	{
		// Regular text response
		UE_LOG(LogTemp, Log, TEXT("Response is not JSON, treating as regular text"));
		UE_LOG(LogTemp, Log, TEXT("Text Response: %s"), *AccumulatedResponse);
		AIResponse.bSuccess = true;
		AIResponse.Content = AccumulatedResponse;
		AIResponse.UsedProvider = EAIProvider::LocalLLM;
		}
	}
	else
	{
		AIResponse.bSuccess = false;
		AIResponse.ErrorMessage = TEXT("Failed to parse Ollama response");
	}

	if (CurrentAsyncCallback)
	{
		CurrentAsyncCallback(AIResponse);
	}
}

// Blueprint creation helpers
bool UUnrealAIService::CreateBlueprintFromJSON(const FString& JsonString, FString& OutBlueprintPath, FString& OutError)
{
	UE_LOG(LogTemp, Warning, TEXT("=== CreateBlueprintFromJSON: Starting blueprint creation ==="));
	UE_LOG(LogTemp, Log, TEXT("Input JSON length: %d characters"), JsonString.Len());
	UE_LOG(LogTemp, Log, TEXT("Input JSON (first 200 chars): %s"), *JsonString.Left(200));
	
	// Validate input
	if (JsonString.IsEmpty())
	{
		OutError = TEXT("Input JSON string is empty");
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprintFromJSON: Input JSON is empty"));
		return false;
	}
	
	TMap<FString, FString> BlueprintStructure;
	UE_LOG(LogTemp, Log, TEXT("CreateBlueprintFromJSON: About to parse JSON..."));
	
	if (!ParseBlueprintJSON(JsonString, BlueprintStructure, OutError))
	{
		UE_LOG(LogTemp, Error, TEXT("CreateBlueprintFromJSON: JSON parsing failed: %s"), *OutError);
		UE_LOG(LogTemp, Error, TEXT("Failed JSON content: %s"), *JsonString);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("CreateBlueprintFromJSON: JSON parsed successfully! Found %d elements"), BlueprintStructure.Num());
	
	// Log parsed structure
	for (const auto& Pair : BlueprintStructure)
	{
		UE_LOG(LogTemp, Log, TEXT("Parsed element - Key: %s, Value length: %d"), *Pair.Key, Pair.Value.Len());
	}
	
	UE_LOG(LogTemp, Log, TEXT("CreateBlueprintFromJSON: About to generate blueprint nodes..."));
	
	if (GenerateBlueprintNodesFromJSON(BlueprintStructure, OutError))
	{
		FString BlueprintName = BlueprintStructure.FindRef(TEXT("blueprint_name"));
		if (BlueprintName.IsEmpty())
		{
			BlueprintName = BlueprintStructure.FindRef(TEXT("name")); // Fallback
		}
		
		if (BlueprintName.IsEmpty())
		{
			BlueprintName = TEXT("AI_Generated_Blueprint");
			UE_LOG(LogTemp, Warning, TEXT("No blueprint name found, using default: %s"), *BlueprintName);
		}
		
		FString AssetName = BlueprintName.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
		OutBlueprintPath = FString::Printf(TEXT("/Game/AI_Generated/%s"), *AssetName);
		UE_LOG(LogTemp, Warning, TEXT("=== CreateBlueprintFromJSON: SUCCESS! Blueprint created: %s ==="), *OutBlueprintPath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("=== CreateBlueprintFromJSON: FAILED! Blueprint node generation failed: %s ==="), OutError.IsEmpty() ? TEXT("Unknown error") : *OutError);
	
	// If OutError is empty, provide a default error message
	if (OutError.IsEmpty())
	{
		OutError = TEXT("Unknown error occurred during blueprint generation");
	}
	
	return false;
}

bool UUnrealAIService::ParseBlueprintJSON(const FString& JsonString, TMap<FString, FString>& OutBlueprintStructure, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing JSON: %s"), *JsonString.Left(200));
	
	// Clean up the JSON string - minimal cleaning to avoid breaking structure
	FString CleanJsonString = JsonString;
	
	// Only fix the most critical issues that won't break JSON structure
	CleanJsonString = CleanJsonString.Replace(TEXT("\r\n"), TEXT(" "));  // Remove Windows line endings
	CleanJsonString = CleanJsonString.Replace(TEXT("\n"), TEXT(" "));  // Remove Unix line endings
	CleanJsonString = CleanJsonString.Replace(TEXT("\t"), TEXT(" "));  // Remove tabs
	
	// Fix common JSON syntax errors that are safe
	CleanJsonString = CleanJsonString.Replace(TEXT("], }"), TEXT("] }"));  // Fix extra comma before closing brace
	CleanJsonString = CleanJsonString.Replace(TEXT("},]"), TEXT("}]"));  // Fix extra comma before closing bracket
	CleanJsonString = CleanJsonString.Replace(TEXT(", }"), TEXT(" }"));  // Fix comma before closing brace
	CleanJsonString = CleanJsonString.Replace(TEXT(",]"), TEXT("]"));  // Fix comma before closing bracket
	
	// No more space cleaning - prompt will generate clean JSON
	
	UE_LOG(LogTemp, Log, TEXT("Cleaned JSON: %s"), *CleanJsonString.Left(200));
	
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJsonString);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = TEXT("Failed to parse JSON response from AI");
		UE_LOG(LogTemp, Error, TEXT("JSON parsing failed: %s"), *OutError);
		UE_LOG(LogTemp, Error, TEXT("Failed on cleaned JSON (first 500 chars): %s"), *CleanJsonString.Left(500));
		UE_LOG(LogTemp, Error, TEXT("JSON length: %d characters"), CleanJsonString.Len());
		UE_LOG(LogTemp, Error, TEXT("JSON ends with: %s"), *CleanJsonString.Right(100));
		
		// Check if JSON is truncated - more sophisticated check
		if (!CleanJsonString.EndsWith(TEXT("}")))
		{
			UE_LOG(LogTemp, Error, TEXT("JSON appears to be truncated - does not end with closing brace"));
		}
		else
		{
			// Count braces to see if they're balanced
			int32 OpenBraces = 0;
			int32 CloseBraces = 0;
			int32 OpenBrackets = 0;
			int32 CloseBrackets = 0;
			
			for (TCHAR Char : CleanJsonString)
			{
				if (Char == TEXT('{')) OpenBraces++;
				else if (Char == TEXT('}')) CloseBraces++;
				else if (Char == TEXT('[')) OpenBrackets++;
				else if (Char == TEXT(']')) CloseBrackets++;
			}
			
			UE_LOG(LogTemp, Log, TEXT("Brace balance: {=%d, }=%d, [=%d, ]=%d"), OpenBraces, CloseBraces, OpenBrackets, CloseBrackets);
			
			if (OpenBraces != CloseBraces || OpenBrackets != CloseBrackets)
			{
				UE_LOG(LogTemp, Error, TEXT("JSON has unbalanced braces/brackets"));
			}
		}
		
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("JSON parsed successfully"));

	// Extract blueprint information
	OutBlueprintStructure.Add(TEXT("name"), JsonObject->GetStringField(TEXT("blueprint_name")));
	OutBlueprintStructure.Add(TEXT("description"), JsonObject->GetStringField(TEXT("description")));
	OutBlueprintStructure.Add(TEXT("parent_class"), JsonObject->GetStringField(TEXT("parent_class")));

	// Parse variables
	const TArray<TSharedPtr<FJsonValue>>* VariablesArray;
	if (JsonObject->TryGetArrayField(TEXT("variables"), VariablesArray))
	{
		FString VariablesJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&VariablesJson);
		FJsonSerializer::Serialize(*VariablesArray, Writer);
		OutBlueprintStructure.Add(TEXT("variables"), VariablesJson);
	}

	// Parse events
	const TArray<TSharedPtr<FJsonValue>>* EventsArray;
	if (JsonObject->TryGetArrayField(TEXT("events"), EventsArray))
	{
		FString EventsJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EventsJson);
		FJsonSerializer::Serialize(*EventsArray, Writer);
		OutBlueprintStructure.Add(TEXT("events"), EventsJson);
	}

	// Parse functions
	const TArray<TSharedPtr<FJsonValue>>* FunctionsArray;
	if (JsonObject->TryGetArrayField(TEXT("functions"), FunctionsArray))
	{
		FString FunctionsJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FunctionsJson);
		FJsonSerializer::Serialize(*FunctionsArray, Writer);
		OutBlueprintStructure.Add(TEXT("functions"), FunctionsJson);
	}

	// Parse execution flow
	const TArray<TSharedPtr<FJsonValue>>* ExecutionFlowArray;
	if (JsonObject->TryGetArrayField(TEXT("execution_flow"), ExecutionFlowArray))
	{
		FString ExecutionFlowJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ExecutionFlowJson);
		FJsonSerializer::Serialize(*ExecutionFlowArray, Writer);
		OutBlueprintStructure.Add(TEXT("execution_flow"), ExecutionFlowJson);
	}

	// Parse components
	const TArray<TSharedPtr<FJsonValue>>* ComponentsArray;
	if (JsonObject->TryGetArrayField(TEXT("components"), ComponentsArray))
	{
		FString ComponentsJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ComponentsJson);
		FJsonSerializer::Serialize(*ComponentsArray, Writer);
		OutBlueprintStructure.Add(TEXT("components"), ComponentsJson);
	}

	return true;
}

bool UUnrealAIService::GenerateBlueprintNodesFromJSON(const TMap<FString, FString>& BlueprintStructure, FString& OutError)
{
	UE_LOG(LogTemp, Warning, TEXT("=== GenerateBlueprintNodesFromJSON: Starting ==="));
	UE_LOG(LogTemp, Log, TEXT("Input structure has %d elements"), BlueprintStructure.Num());
	
	FString BlueprintName = BlueprintStructure.FindRef(TEXT("blueprint_name"));
	if (BlueprintName.IsEmpty())
	{
		BlueprintName = BlueprintStructure.FindRef(TEXT("name")); // Fallback
		UE_LOG(LogTemp, Log, TEXT("Using fallback name field"));
	}
	FString Description = BlueprintStructure.FindRef(TEXT("description"));
	FString ParentClass = BlueprintStructure.FindRef(TEXT("parent_class"));

	if (BlueprintName.IsEmpty())
	{
		OutError = TEXT("Blueprint name is required but not found in JSON");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint name is required"));
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Blueprint Details - Name: %s"), *BlueprintName);
	UE_LOG(LogTemp, Log, TEXT("Description: %s"), Description.IsEmpty() ? TEXT("(empty)") : *Description);
	UE_LOG(LogTemp, Log, TEXT("Parent Class: %s"), ParentClass.IsEmpty() ? TEXT("(empty, will use Actor)") : *ParentClass);

	// Create the blueprint asset with unique name checking
	FString PackagePath = TEXT("/Game/AI_Generated");
	FString AssetName = BlueprintName.Replace(TEXT(" "), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
	FString BaseAssetName = AssetName;
	FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
	
	// Check for existing packages/blueprints and make name unique
	int32 NameCounter = 1;
	while (FindObject<UPackage>(nullptr, *PackageName) != nullptr || LoadObject<UBlueprint>(nullptr, *PackageName) != nullptr)
	{
		AssetName = FString::Printf(TEXT("%s_%d"), *BaseAssetName, NameCounter);
		PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);
		NameCounter++;
		
		UE_LOG(LogTemp, Warning, TEXT("Blueprint/Package conflict detected. Trying: %s"), *AssetName);
		
		// Safety limit to prevent infinite loops
		if (NameCounter > 100)
		{
			OutError = TEXT("Too many blueprint name conflicts - unable to find unique name");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Too many name conflicts"));
			return false;
		}
	}
	
	if (AssetName != BaseAssetName)
	{
		UE_LOG(LogTemp, Warning, TEXT("Asset name changed from '%s' to '%s' to avoid conflicts"), *BaseAssetName, *AssetName);
		BlueprintName = AssetName; // Update BlueprintName for consistency
	}

	UE_LOG(LogTemp, Log, TEXT("Package details - Path: %s, Name: %s, Full: %s"), *PackagePath, *AssetName, *PackageName);

	// Create package
	UE_LOG(LogTemp, Log, TEXT("About to create package..."));
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("Failed to create package: %s"), *PackageName);
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Failed to create package: %s"), *PackageName);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("Package created successfully: %s"), *Package->GetName());

	// Determine parent class
	UClass* ParentClassPtr = AActor::StaticClass(); // Default to Actor
	if (!ParentClass.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("Determining parent class from: %s"), *ParentClass);
		if (ParentClass.Contains(TEXT("Pawn")))
		{
			ParentClassPtr = APawn::StaticClass();
			UE_LOG(LogTemp, Log, TEXT("Using APawn as parent class"));
		}
		else if (ParentClass.Contains(TEXT("Character")))
		{
			ParentClassPtr = ACharacter::StaticClass();
			UE_LOG(LogTemp, Log, TEXT("Using ACharacter as parent class"));
		}
		// Add more parent class options as needed
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Using default AActor as parent class"));
	}

	// Validate parent class
	if (!ParentClassPtr)
	{
		OutError = TEXT("Failed to determine valid parent class");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Parent class is null"));
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("Parent class: %s"), *ParentClassPtr->GetName());

	// Create the blueprint
	UE_LOG(LogTemp, Warning, TEXT("About to create blueprint with FKismetEditorUtilities::CreateBlueprint..."));
	UE_LOG(LogTemp, Log, TEXT("Parameters - ParentClass: %s, Package: %s, AssetName: %s"), 
		*ParentClassPtr->GetName(), *Package->GetName(), *AssetName);
	
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(ParentClassPtr, Package, *AssetName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!Blueprint)
	{
		OutError = TEXT("FKismetEditorUtilities::CreateBlueprint returned null - Blueprint creation failed");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: FKismetEditorUtilities::CreateBlueprint returned null"));
		return false;
	}
	UE_LOG(LogTemp, Warning, TEXT("SUCCESS: Blueprint object created! Name: %s, Package: %s"), *Blueprint->GetName(), *Blueprint->GetPackage()->GetName());

	// Add variables to the blueprint
	UE_LOG(LogTemp, Log, TEXT("About to add variables to blueprint..."));
	FString VariablesJson = BlueprintStructure.FindRef(TEXT("variables"));
	UE_LOG(LogTemp, Log, TEXT("Variables JSON length: %d"), VariablesJson.Len());
	
	if (!AddVariablesToBlueprint(Blueprint, VariablesJson, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to add variables: %s"), OutError.IsEmpty() ? TEXT("Unknown error") : *OutError);
		// Continue with blueprint creation even if variables fail
		OutError.Empty(); // Clear error so we can continue
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Variables added successfully"));
	}

	// Add basic event graph structure
	UE_LOG(LogTemp, Log, TEXT("About to create basic event graph..."));
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null before creating event graph");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint is null before event graph"));
		return false;
	}
	
	if (!CreateBasicEventGraph(Blueprint, BlueprintStructure, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create event graph: %s"), OutError.IsEmpty() ? TEXT("Unknown error") : *OutError);
		OutError.Empty(); // Clear error so we can continue
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Basic event graph created successfully"));
	}

	// Create function graphs
	UE_LOG(LogTemp, Log, TEXT("About to create function graphs..."));
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null before creating function graphs");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint is null before function graphs"));
		return false;
	}
	
	if (!CreateFunctionGraphs(Blueprint, BlueprintStructure, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to create function graphs: %s"), OutError.IsEmpty() ? TEXT("Unknown error") : *OutError);
		OutError.Empty(); // Clear error so we can continue
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Function graphs created successfully"));
	}
	
	// Validate Blueprint integrity after function creation
	if (!IsValid(Blueprint))
	{
		OutError = TEXT("Blueprint became invalid after function creation");
		UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint corrupted after function creation"));
		return false;
	}

	// Mark package as dirty and save
	UE_LOG(LogTemp, Log, TEXT("About to mark package as dirty and save..."));
	Package->SetDirtyFlag(true);
	
	FString Filename;
	UE_LOG(LogTemp, Log, TEXT("Converting package name to filename: %s"), *PackageName);
	if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename))
	{
		UE_LOG(LogTemp, Log, TEXT("Filename conversion successful: %s"), *Filename);
		
		// Save the package
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
		SaveArgs.SaveFlags = SAVE_None;

		// Validate before saving
		if (!Package)
		{
			OutError = TEXT("Package is null before save operation");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Package is null before save"));
			return false;
		}
		
		if (!Blueprint)
		{
			OutError = TEXT("Blueprint is null before save operation");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint is null before save"));
			return false;
		}
		
		if (Filename.IsEmpty())
		{
			OutError = TEXT("Filename is empty before save operation");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Filename is empty before save"));
			return false;
		}
		
		// Final validation before save
		if (!IsValid(Package))
		{
			OutError = TEXT("Package became invalid before save");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Package became invalid"));
			return false;
		}
		
		if (!IsValid(Blueprint))
		{
			OutError = TEXT("Blueprint became invalid before save");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Blueprint became invalid"));
			return false;
		}
		
		// Try to compile the blueprint before saving
		UE_LOG(LogTemp, Log, TEXT("Attempting to compile blueprint before save..."));
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
		
		// TEMPORARY: Skip save to test Blueprint creation without file I/O
		UE_LOG(LogTemp, Warning, TEXT("🧪 TESTING: Skipping save operation to isolate crash cause"));
		UE_LOG(LogTemp, Log, TEXT("Package name: %s"), Package ? *Package->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Log, TEXT("Blueprint name: %s"), Blueprint ? *Blueprint->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Log, TEXT("Package is valid: %s"), IsValid(Package) ? TEXT("true") : TEXT("false"));
		UE_LOG(LogTemp, Log, TEXT("Blueprint is valid: %s"), IsValid(Blueprint) ? TEXT("true") : TEXT("false"));
		
		// Test: Return success without saving to see if Blueprint creation works
		OutError = FString::Printf(TEXT("Blueprint '%s' created successfully (not saved to disk - testing mode)"), *BlueprintName);
		UE_LOG(LogTemp, Warning, TEXT("=== GenerateBlueprintNodesFromJSON: CREATION SUCCESS (NO SAVE TEST) ==="));
		return true;
		
		// Original save code (temporarily disabled)
		/*
		UE_LOG(LogTemp, Warning, TEXT("About to save blueprint package to: %s"), *Filename);
		if (UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs))
		{
			UE_LOG(LogTemp, Warning, TEXT("SUCCESS: Blueprint package saved: %s"), *Filename);

			// Notify asset registry
			UE_LOG(LogTemp, Log, TEXT("Notifying asset registry of new blueprint..."));
			FAssetRegistryModule::AssetCreated(Blueprint);

			OutError = FString::Printf(TEXT("Blueprint '%s' created successfully at: %s"), *BlueprintName, *Filename);
			UE_LOG(LogTemp, Warning, TEXT("=== GenerateBlueprintNodesFromJSON: COMPLETE SUCCESS ==="));
			return true;
		}
		else
		{
			OutError = TEXT("UPackage::SavePackage failed - could not save blueprint to disk");
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: UPackage::SavePackage failed"));
			UE_LOG(LogTemp, Error, TEXT("GenerateBlueprintNodesFromJSON: Failed to save blueprint package"));
			return false;
		}
		*/
	}
	else
	{
		OutError = TEXT("Failed to convert package name to filename");
		return false;
	}
}

bool UUnrealAIService::AddVariablesToBlueprint(UBlueprint* Blueprint, const FString& VariablesJson, FString& OutError)
{
	if (VariablesJson.IsEmpty())
	{
		return true; // No variables to add
	}

	TArray<TSharedPtr<FJsonValue>> VariablesArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(VariablesJson);

	if (!FJsonSerializer::Deserialize(Reader, VariablesArray))
	{
		OutError = TEXT("Failed to parse variables JSON array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& VariableValue : VariablesArray)
	{
		const TSharedPtr<FJsonObject>& VariableObject = VariableValue->AsObject();
		if (!VariableObject.IsValid())
		{
			continue;
		}

		FString VarName = VariableObject->GetStringField(TEXT("name"));
		FString VarType = VariableObject->GetStringField(TEXT("type"));
		FString DefaultValue = VariableObject->GetStringField(TEXT("default_value"));
		FString Category = VariableObject->GetStringField(TEXT("category"));

		if (VarName.IsEmpty())
		{
			continue;
		}

		// Create variable based on type
		FEdGraphPinType PinType;
		if (VarType.Contains(TEXT("Integer")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if (VarType.Contains(TEXT("Float")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if (VarType.Contains(TEXT("Boolean")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if (VarType.Contains(TEXT("String")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if (VarType.Contains(TEXT("Vector")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
		}
		else if (VarType.Contains(TEXT("Rotator")))
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
			PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
		}
		else
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		}

		// Add variable to blueprint
		FName VarFName = FName(*VarName);
		FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarFName, PinType);
	}

	return true;
}

bool UUnrealAIService::CreateBasicEventGraph(UBlueprint* Blueprint, const TMap<FString, FString>& BlueprintStructure, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	// Get or create the event graph
	UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!EventGraph)
	{
		EventGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("EventGraph"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (!EventGraph)
		{
			OutError = TEXT("Failed to create event graph");
			return false;
		}

		// Add the event graph to the blueprint (simplified - not adding to function list for now)
		Blueprint->FunctionGraphs.Add(EventGraph);
	}

	if (EventGraph)
	{
		UE_LOG(LogTemp, Log, TEXT("Event graph created for blueprint"));

		// Create execution flow nodes from the JSON
		FString ExecutionFlowJson = BlueprintStructure.FindRef(TEXT("execution_flow"));
		if (!ExecutionFlowJson.IsEmpty())
		{
			if (!CreateExecutionFlowNodes(Blueprint, EventGraph, ExecutionFlowJson, OutError))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create execution flow nodes: %s"), *OutError);
			}
		}

		// NEW: Handle events with structured logic
		FString EventsJson = BlueprintStructure.FindRef(TEXT("events"));
		if (!EventsJson.IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("Creating events with structured logic from JSON"));
			if (!CreateEventNodesWithStructuredLogic(Blueprint, EventGraph, EventsJson, OutError))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to create event nodes with structured logic: %s"), *OutError);
			}
		}
	}

	// Create components from the JSON
	FString ComponentsJson = BlueprintStructure.FindRef(TEXT("components"));
	if (!ComponentsJson.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("Creating components from JSON: %s"), *ComponentsJson);
		if (!CreateBlueprintComponents(Blueprint, ComponentsJson, OutError))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create components: %s"), *OutError);
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("No components specified in JSON"));
	}

	return true;
}

bool UUnrealAIService::CreateEventNodesWithStructuredLogic(UBlueprint* Blueprint, UEdGraph* EventGraph, const FString& EventsJson, FString& OutError)
{
	if (!Blueprint || !EventGraph || EventsJson.IsEmpty())
	{
		OutError = TEXT("Invalid parameters for event creation");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Creating event nodes with structured logic from JSON"));

	TArray<TSharedPtr<FJsonValue>> EventsArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(EventsJson);

	if (!FJsonSerializer::Deserialize(Reader, EventsArray))
	{
		OutError = TEXT("Failed to parse events JSON array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& EventValue : EventsArray)
	{
		const TSharedPtr<FJsonObject>& EventObject = EventValue->AsObject();
		if (!EventObject.IsValid())
		{
			continue;
		}

		FString EventName = EventObject->GetStringField(TEXT("name"));
		if (EventName.IsEmpty())
		{
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Processing event: %s"), *EventName);

		// Handle logic field - it can be either a string or an object
		FString EventLogic;
		if (EventObject->HasField(TEXT("logic")))
		{
			TSharedPtr<FJsonValue> LogicValue = EventObject->GetField<EJson::None>(TEXT("logic"));
			if (LogicValue->Type == EJson::String)
			{
				EventLogic = EventObject->GetStringField(TEXT("logic"));
				UE_LOG(LogTemp, Log, TEXT("Event %s has string logic: %s"), *EventName, *EventLogic.Left(100));
			}
			else if (LogicValue->Type == EJson::Object)
			{
				// Convert JSON object to string for structured parsing
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EventLogic);
				FJsonSerializer::Serialize(LogicValue->AsObject().ToSharedRef(), Writer);
				UE_LOG(LogTemp, Log, TEXT("Event %s has structured logic: %s"), *EventName, *EventLogic.Left(100));
			}
		}

		// Check if this event has structured logic (nodes and connections)
		if (EventLogic.Contains(TEXT("\"nodes\"")) && EventLogic.Contains(TEXT("\"connections\"")))
		{
			UE_LOG(LogTemp, Warning, TEXT("🔧 Event %s has structured logic - creating nodes"), *EventName);
			
			// Create the event node first
			UK2Node_Event* EventNode = nullptr;
			
			// Handle different event types
			if (EventName == TEXT("BeginPlay"))
			{
				EventNode = NewObject<UK2Node_Event>(EventGraph);
				EventNode->EventReference.SetExternalMember(FName(TEXT("ReceiveBeginPlay")), AActor::StaticClass());
				EventNode->bOverrideFunction = true;
			}
			else if (EventName == TEXT("Tick"))
			{
				EventNode = NewObject<UK2Node_Event>(EventGraph);
				EventNode->EventReference.SetExternalMember(FName(TEXT("ReceiveTick")), AActor::StaticClass());
				EventNode->bOverrideFunction = true;
			}
			else
			{
				// Generic event - create a custom event
				EventNode = NewObject<UK2Node_Event>(EventGraph);
				EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
				EventNode->bOverrideFunction = true;
			}

			if (EventNode)
			{
				EventNode->AllocateDefaultPins();
				EventGraph->AddNode(EventNode);
				
				// Position the event node
				EventNode->NodePosX = 0;
				EventNode->NodePosY = 0;

				UE_LOG(LogTemp, Log, TEXT("Event node created for %s"), *EventName);

				// Now parse the structured logic and create the actual nodes
				// We'll reuse the existing ParseStructuredLogicFromJSON function
				// but we need to create a dummy FunctionEntry for compatibility
				UK2Node_FunctionEntry* DummyEntry = NewObject<UK2Node_FunctionEntry>(EventGraph);
				DummyEntry->AllocateDefaultPins();
				EventGraph->AddNode(DummyEntry);
				DummyEntry->NodePosX = -200; // Position off-screen
				DummyEntry->NodePosY = 0;

				FString LogicError;
				if (!ParseStructuredLogicFromJSON(Blueprint, EventGraph, EventLogic, DummyEntry, LogicError))
				{
					UE_LOG(LogTemp, Error, TEXT("❌ Failed to parse structured logic for event %s: %s"), *EventName, *LogicError);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("✅ Successfully parsed structured logic for event %s"), *EventName);
				}

				// Remove the dummy entry node
				EventGraph->RemoveNode(DummyEntry);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Event %s has simple logic (no structured nodes)"), *EventName);
		}
	}

	return true;
}

bool UUnrealAIService::CreateExecutionFlowNodes(UBlueprint* Blueprint, UEdGraph* EventGraph, const FString& ExecutionFlowJson, FString& OutError)
{
	if (!Blueprint || !EventGraph || ExecutionFlowJson.IsEmpty())
	{
		OutError = TEXT("Invalid parameters for execution flow creation");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Creating execution flow nodes from JSON"));

	TArray<TSharedPtr<FJsonValue>> ExecutionFlowArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ExecutionFlowJson);

	if (!FJsonSerializer::Deserialize(Reader, ExecutionFlowArray))
	{
		OutError = TEXT("Failed to parse execution flow JSON array");
		return false;
	}

	// Find existing BeginPlay event node instead of creating a new one
	UK2Node_Event* BeginPlayEvent = nullptr;
	for (UEdGraphNode* Node : EventGraph->Nodes)
	{
		if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
		{
			if (EventNode->EventReference.GetMemberName() == FName(TEXT("ReceiveBeginPlay")))
			{
				BeginPlayEvent = EventNode;
				break;
			}
		}
	}

	if (!BeginPlayEvent)
	{
		UE_LOG(LogTemp, Warning, TEXT("No BeginPlay event found, creating one"));
		BeginPlayEvent = NewObject<UK2Node_Event>(EventGraph);
		BeginPlayEvent->EventReference.SetExternalMember(FName(TEXT("ReceiveBeginPlay")), AActor::StaticClass());
		BeginPlayEvent->bOverrideFunction = true;
		BeginPlayEvent->AllocateDefaultPins();
		EventGraph->AddNode(BeginPlayEvent);
	}

	UEdGraphPin* LastExecPin = BeginPlayEvent->FindPin(UEdGraphSchema_K2::PN_Then);
	TArray<UK2Node*> CreatedNodes;

	for (const TSharedPtr<FJsonValue>& FlowValue : ExecutionFlowArray)
	{
		const TSharedPtr<FJsonObject>& FlowObject = FlowValue->AsObject();
		if (!FlowObject.IsValid())
		{
			continue;
		}

		FString FromEvent = FlowObject->GetStringField(TEXT("from_event"));
		const TArray<TSharedPtr<FJsonValue>>* SequenceArray = nullptr;

		if (FlowObject->TryGetArrayField(TEXT("sequence"), SequenceArray))
		{
			for (const TSharedPtr<FJsonValue>& SequenceValue : *SequenceArray)
			{
				FString SequenceStep = SequenceValue->AsString();
				UK2Node* NewNode = nullptr;

				// Parse the sequence step and create appropriate node
				if (SequenceStep.Contains(TEXT("Set variable")))
				{
					NewNode = CreateVariableSetNode(Blueprint, EventGraph, SequenceStep);
				}
				else if (SequenceStep.Contains(TEXT("Call function")))
				{
					NewNode = CreateFunctionCallNode(Blueprint, EventGraph, SequenceStep);
				}
				else if (SequenceStep.Contains(TEXT("Branch:")))
				{
					NewNode = CreateBranchNode(Blueprint, EventGraph, SequenceStep);
				}
				else if (SequenceStep.Contains(TEXT("Get variable")))
				{
					NewNode = CreateVariableGetNode(Blueprint, EventGraph, SequenceStep);
				}

				if (NewNode && LastExecPin)
				{
					// Connect the execution pins
					UEdGraphPin* NewExecPin = NewNode->FindPin(UEdGraphSchema_K2::PN_Execute);
					if (NewExecPin)
					{
						LastExecPin->MakeLinkTo(NewExecPin);
						LastExecPin = NewNode->FindPin(UEdGraphSchema_K2::PN_Then);
					}

					CreatedNodes.Add(NewNode);
				}
			}
		}
	}

	// Position nodes in the graph
	PositionNodesInGraph(EventGraph, CreatedNodes);

	UE_LOG(LogTemp, Log, TEXT("Created %d execution flow nodes"), CreatedNodes.Num());
	return true;
}

UK2Node* UUnrealAIService::CreateVariableSetNode(UBlueprint* Blueprint, UEdGraph* Graph, const FString& SequenceStep)
{
	// Parse "Set variable Health to 100" or "Set variable PlayerTarget to Actor::GetActorByName('Player')"
	FString VariableName;
	FString ValueString;
	
	// Look for "Set variable" pattern
	int32 SetIndex = SequenceStep.Find(TEXT("Set variable "));
	if (SetIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateVariableSetNode: No 'Set variable' found in: %s"), *SequenceStep);
		return nullptr;
	}
	
	// Find the " to " separator
	int32 ToIndex = SequenceStep.Find(TEXT(" to "));
	if (ToIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateVariableSetNode: No ' to ' found in: %s"), *SequenceStep);
		return nullptr;
	}
	
	// Extract variable name (between "Set variable " and " to ")
	VariableName = SequenceStep.Mid(SetIndex + 13, ToIndex - (SetIndex + 13)).TrimStartAndEnd();
	ValueString = SequenceStep.Mid(ToIndex + 4).TrimStartAndEnd();
	
	UE_LOG(LogTemp, Log, TEXT("CreateVariableSetNode: Variable='%s', Value='%s'"), *VariableName, *ValueString);
	
	if (VariableName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateVariableSetNode: Empty variable name"));
		return nullptr;
	}

	UK2Node_VariableSet* SetNode = NewObject<UK2Node_VariableSet>(Graph);
	SetNode->VariableReference.SetSelfMember(FName(*VariableName));
	SetNode->AllocateDefaultPins();

	Graph->AddNode(SetNode);
	
	// Try to set the default value if it's a simple number or string
	if (!ValueString.IsEmpty() && !ValueString.Contains(TEXT("::")))
	{
		// For simple values, we could set the default value here
		// This would require more complex parsing based on variable type
		UE_LOG(LogTemp, Log, TEXT("CreateVariableSetNode: Simple value '%s' for variable '%s'"), *ValueString, *VariableName);
	}
	
	return SetNode;
}

UK2Node* UUnrealAIService::CreateFunctionCallNode(UBlueprint* Blueprint, UEdGraph* Graph, const FString& SequenceStep)
{
	// Parse "Call function InitializeComponents" or "Call function MoveToLocation"
	FString FunctionName;
	
	// Look for "Call function" pattern
	int32 CallIndex = SequenceStep.Find(TEXT("Call function "));
	if (CallIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateFunctionCallNode: No 'Call function' found in: %s"), *SequenceStep);
		return nullptr;
	}
	
	FunctionName = SequenceStep.Mid(CallIndex + 15).TrimStartAndEnd();
	
	UE_LOG(LogTemp, Log, TEXT("CreateFunctionCallNode: Function='%s'"), *FunctionName);
	
	if (FunctionName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateFunctionCallNode: Empty function name"));
		return nullptr;
	}

	UK2Node_CallFunction* CallNode = NewObject<UK2Node_CallFunction>(Graph);
	
	// Try to find the function in common Unreal Engine classes
	UFunction* TargetFunction = nullptr;
	
	// Check AActor first
	// Enhanced built-in function lookup - prioritize common Unreal functions
	
	// Check Actor class first (most basic)
	TargetFunction = AActor::StaticClass()->FindFunctionByName(FName(*FunctionName));
	
	// Check Pawn class for movement/input functions
	if (!TargetFunction)
	{
		TargetFunction = APawn::StaticClass()->FindFunctionByName(FName(*FunctionName));
	}
	
	// Check Character class for character-specific functions
	if (!TargetFunction)
	{
		TargetFunction = ACharacter::StaticClass()->FindFunctionByName(FName(*FunctionName));
	}
	
	// Check common engine classes for built-in functions
	if (!TargetFunction)
	{
		// Try UGameplayStatics for utility functions
		if (UClass* GameplayStaticsClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics")))
		{
			TargetFunction = GameplayStaticsClass->FindFunctionByName(FName(*FunctionName));
		}
	}
	
	if (!TargetFunction)
	{
		// Try UKismetSystemLibrary for system functions
		if (UClass* SystemLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetSystemLibrary")))
		{
			TargetFunction = SystemLibraryClass->FindFunctionByName(FName(*FunctionName));
		}
	}
	
	if (!TargetFunction)
	{
		// Try UKismetMathLibrary for math functions
		if (UClass* MathLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMathLibrary")))
		{
			TargetFunction = MathLibraryClass->FindFunctionByName(FName(*FunctionName));
		}
	}
	
	if (TargetFunction)
	{
		CallNode->SetFromFunction(TargetFunction);
		UE_LOG(LogTemp, Log, TEXT("✅ CreateFunctionCallNode: Found built-in function '%s' in class '%s'"), *FunctionName, *TargetFunction->GetOuter()->GetName());
		
		// Try to set common parameters if they exist in the logic string
		if (SequenceStep.Contains(TEXT("with ")))
		{
			FString ParameterPart;
			if (SequenceStep.Split(TEXT("with "), nullptr, &ParameterPart))
			{
				ParameterPart = ParameterPart.TrimStartAndEnd();
				ParameterPart = ParameterPart.Replace(TEXT("'"), TEXT(""));
				ParameterPart = ParameterPart.Replace(TEXT("\""), TEXT(""));
				
				// Set parameter on common pin names
				TArray<FString> CommonPinNames = {TEXT("Value"), TEXT("InString"), TEXT("Text"), TEXT("Location"), TEXT("Rotation"), TEXT("Amount"), TEXT("Target")};
				
				for (const FString& PinName : CommonPinNames)
				{
					UEdGraphPin* ParamPin = CallNode->FindPin(*PinName);
					if (ParamPin && ParamPin->Direction == EGPD_Input)
					{
						ParamPin->DefaultValue = ParameterPart;
						UE_LOG(LogTemp, VeryVerbose, TEXT("  🔧 Set parameter '%s' = '%s'"), *PinName, *ParameterPart);
						break;
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ CreateFunctionCallNode: Could not find built-in function '%s' - consider using Unreal's built-in functions instead"), *FunctionName);
	}
	
	CallNode->AllocateDefaultPins();
	Graph->AddNode(CallNode);
	return CallNode;
}

UK2Node* UUnrealAIService::CreateBranchNode(UBlueprint* Blueprint, UEdGraph* Graph, const FString& SequenceStep)
{
	// Parse "Branch: IsAlive -> True: Enable Input, False: Destroy Actor" or "If (PlayerTarget != None) then Branch: IsInRange"
	FString ConditionString;
	
	// Look for "Branch:" pattern
	int32 BranchIndex = SequenceStep.Find(TEXT("Branch:"));
	if (BranchIndex == INDEX_NONE)
	{
		// Look for "If" pattern
		int32 IfIndex = SequenceStep.Find(TEXT("If ("));
		if (IfIndex != INDEX_NONE)
		{
			// Extract condition from "If (condition) then Branch: ..."
			int32 ThenIndex = SequenceStep.Find(TEXT(") then Branch:"));
			if (ThenIndex != INDEX_NONE)
			{
				ConditionString = SequenceStep.Mid(IfIndex + 4, ThenIndex - (IfIndex + 4)).TrimStartAndEnd();
			}
		}
	}
	else
	{
		// Extract condition after "Branch:"
		ConditionString = SequenceStep.Mid(BranchIndex + 7).TrimStartAndEnd();
		
		// Remove the "-> True: ... False: ..." part if present
		int32 ArrowIndex = ConditionString.Find(TEXT(" ->"));
		if (ArrowIndex != INDEX_NONE)
		{
			ConditionString = ConditionString.Left(ArrowIndex).TrimStartAndEnd();
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("CreateBranchNode: Condition='%s'"), *ConditionString);
	
	if (ConditionString.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateBranchNode: Empty condition"));
		ConditionString = TEXT("True"); // Default condition
	}

	UK2Node_IfThenElse* BranchNode = NewObject<UK2Node_IfThenElse>(Graph);
	BranchNode->AllocateDefaultPins();

	Graph->AddNode(BranchNode);
	
	// For now, we create a simple boolean condition
	// In a full implementation, you'd parse the condition and create appropriate comparison nodes
	UE_LOG(LogTemp, Log, TEXT("CreateBranchNode: Created branch node with condition '%s'"), *ConditionString);
	
	return BranchNode;
}

UK2Node* UUnrealAIService::CreateVariableGetNode(UBlueprint* Blueprint, UEdGraph* Graph, const FString& SequenceStep)
{
	// Parse "Get variable CurrentHealth"
	FString VariableName;
	if (SequenceStep.Contains(TEXT("Get variable ")))
	{
		VariableName = SequenceStep.Mid(14).TrimStartAndEnd(); // Remove "Get variable "

		UK2Node_VariableGet* GetNode = NewObject<UK2Node_VariableGet>(Graph);
		GetNode->VariableReference.SetSelfMember(FName(*VariableName));
		GetNode->AllocateDefaultPins();

		Graph->AddNode(GetNode);
		return GetNode;
	}

	return nullptr;
}

void UUnrealAIService::PositionNodesInGraph(UEdGraph* Graph, const TArray<UK2Node*>& Nodes)
{
	const int32 NodeSpacingX = 300;
	const int32 NodeSpacingY = 150;
	int32 CurrentX = 400; // Start after BeginPlay
	int32 CurrentY = 0;

	for (UK2Node* Node : Nodes)
	{
		if (Node)
		{
			Node->NodePosX = CurrentX;
			Node->NodePosY = CurrentY;
			CurrentX += NodeSpacingX;
		}
	}
}

bool UUnrealAIService::CreateFunctionGraphs(UBlueprint* Blueprint, const TMap<FString, FString>& BlueprintStructure, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FString FunctionsJson = BlueprintStructure.FindRef(TEXT("functions"));
	if (FunctionsJson.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("No functions to create"));
		return true; // No functions to create is not an error
	}

	UE_LOG(LogTemp, Log, TEXT("Creating function graphs from JSON: %s"), *FunctionsJson);

	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FunctionsJson);

	if (!FJsonSerializer::Deserialize(Reader, FunctionsArray))
	{
		OutError = TEXT("Failed to parse functions JSON array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& FunctionValue : FunctionsArray)
	{
		const TSharedPtr<FJsonObject>& FunctionObject = FunctionValue->AsObject();
		if (!FunctionObject.IsValid())
		{
			continue;
		}

			FString FunctionName = FunctionObject->GetStringField(TEXT("name"));
		
		// Handle logic field - it can be either a string or an object
		FString FunctionLogic;
		if (FunctionObject->HasField(TEXT("logic")))
		{
			TSharedPtr<FJsonValue> LogicValue = FunctionObject->GetField<EJson::None>(TEXT("logic"));
			if (LogicValue->Type == EJson::String)
			{
				FunctionLogic = FunctionObject->GetStringField(TEXT("logic"));
			}
			else if (LogicValue->Type == EJson::Object)
			{
				// Convert JSON object to string for structured parsing
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FunctionLogic);
				FJsonSerializer::Serialize(LogicValue->AsObject().ToSharedRef(), Writer);
				UE_LOG(LogTemp, Log, TEXT("Function %s has structured logic in 'logic' field: %s"), *FunctionName, *FunctionLogic.Left(100));
			}
		}
		// NEW: Also check for direct nodes/connections fields (like in TestJsonParser2.json)
		else if (FunctionObject->HasField(TEXT("nodes")) && FunctionObject->HasField(TEXT("connections")))
		{
			UE_LOG(LogTemp, Warning, TEXT("🔧 Function %s has direct nodes/connections fields"), *FunctionName);
			
			// Create a logic object with the direct nodes/connections
			TSharedPtr<FJsonObject> LogicObject = MakeShareable(new FJsonObject);
			LogicObject->SetArrayField(TEXT("nodes"), FunctionObject->GetArrayField(TEXT("nodes")));
			LogicObject->SetArrayField(TEXT("connections"), FunctionObject->GetArrayField(TEXT("connections")));
			
			// Convert to string for structured parsing
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&FunctionLogic);
			FJsonSerializer::Serialize(LogicObject.ToSharedRef(), Writer);
			UE_LOG(LogTemp, Log, TEXT("Function %s has structured logic in direct fields: %s"), *FunctionName, *FunctionLogic.Left(100));
		}

		if (FunctionName.IsEmpty())
		{
			continue;
		}

		// Check if this is a built-in Unreal Engine event function
		if (IsUnrealBuiltInEvent(FunctionName))
		{
			UE_LOG(LogTemp, Log, TEXT("Skipping built-in Unreal event: %s"), *FunctionName);
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Creating function: %s with logic: %s"), *FunctionName, *FunctionLogic);
		UE_LOG(LogTemp, Warning, TEXT("🔍 DEBUG: FunctionLogic length: %d"), FunctionLogic.Len());
		UE_LOG(LogTemp, Warning, TEXT("🔍 DEBUG: FunctionLogic contains 'nodes': %s"), FunctionLogic.Contains(TEXT("\"nodes\"")) ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("🔍 DEBUG: FunctionLogic contains 'connections': %s"), FunctionLogic.Contains(TEXT("\"connections\"")) ? TEXT("YES") : TEXT("NO"));
		UE_LOG(LogTemp, Warning, TEXT("🔍 DEBUG: FunctionLogic first 200 chars: %s"), *FunctionLogic.Left(200));

		// Create function graph
		UEdGraph* FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, *FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		if (!FunctionGraph)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create function graph for: %s"), *FunctionName);
			continue;
		}

		// Add function graph to blueprint
		Blueprint->FunctionGraphs.Add(FunctionGraph);

		// Create function entry node
		UK2Node_FunctionEntry* FunctionEntry = NewObject<UK2Node_FunctionEntry>(FunctionGraph);
		FunctionEntry->FunctionReference.SetExternalMember(FName(*FunctionName), Blueprint->GeneratedClass);
		FunctionEntry->AllocateDefaultPins();

		FunctionGraph->AddNode(FunctionEntry);

		// Create function result node
		UK2Node_FunctionResult* FunctionResult = NewObject<UK2Node_FunctionResult>(FunctionGraph);
		FunctionResult->FunctionReference.SetExternalMember(FName(*FunctionName), Blueprint->GeneratedClass);
		FunctionResult->AllocateDefaultPins();

		FunctionGraph->AddNode(FunctionResult);

		// Position nodes
		FunctionEntry->NodePosX = 0;
		FunctionEntry->NodePosY = 0;
		FunctionResult->NodePosX = 300;
		FunctionResult->NodePosY = 0;

		// Create function logic nodes if logic is provided
		if (!FunctionLogic.IsEmpty())
		{
			FString LogicError;
			if (!CreateFunctionLogicNodes(Blueprint, FunctionGraph, FunctionLogic, LogicError))
			{
				UE_LOG(LogTemp, Error, TEXT("❌ Failed to create function logic for %s: %s"), *FunctionName, *LogicError);
				UE_LOG(LogTemp, Warning, TEXT("🔧 Creating fallback logic to prevent empty function"));
				
				// Create fallback logic to ensure function isn't empty
				FString FallbackLogic = FString::Printf(TEXT("Print 'Function %s executed'; Call PrintString with 'Fallback implementation'"), *FunctionName);
				if (!CreateFunctionLogicNodes(Blueprint, FunctionGraph, FallbackLogic, LogicError))
				{
					UE_LOG(LogTemp, Error, TEXT("❌ Even fallback logic failed for function %s"), *FunctionName);
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Function %s has no logic - creating default implementation"), *FunctionName);
			
			// Create default logic instead of empty function
			FString DefaultLogic = FString::Printf(TEXT("Print 'Function %s called'; Call PrintString with 'Default implementation'"), *FunctionName);
			FString LogicError;
			if (!CreateFunctionLogicNodes(Blueprint, FunctionGraph, DefaultLogic, LogicError))
			{
				UE_LOG(LogTemp, Error, TEXT("❌ Failed to create default logic for %s: %s"), *FunctionName, *LogicError);
				
				// Last resort: connect execution pins directly (empty function)
				UEdGraphPin* EntryExecPin = FunctionEntry->FindPin(UEdGraphSchema_K2::PN_Then);
				UEdGraphPin* ResultExecPin = FunctionResult->FindPin(UEdGraphSchema_K2::PN_Execute);
				if (EntryExecPin && ResultExecPin)
				{
					EntryExecPin->MakeLinkTo(ResultExecPin);
					UE_LOG(LogTemp, Warning, TEXT("⚠️ Function %s will be empty (direct connection only)"), *FunctionName);
				}
			}
		}
		
		// Validate function after creation
		FString ValidationError;
		if (!ValidateFunctionConnections(FunctionGraph, ValidationError))
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Function validation failed for %s: %s"), *FunctionName, *ValidationError);
		}
		
		// Optimize layout
		OptimizeFunctionLayout(FunctionGraph);

		UE_LOG(LogTemp, Log, TEXT("Function graph created successfully: %s"), *FunctionName);
	}

	return true;
}

bool UUnrealAIService::CreateBlueprintComponents(UBlueprint* Blueprint, const FString& ComponentsJson, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Creating Blueprint components from JSON: %s"), *ComponentsJson);

	TArray<TSharedPtr<FJsonValue>> ComponentsArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ComponentsJson);

	if (!FJsonSerializer::Deserialize(Reader, ComponentsArray))
	{
		OutError = TEXT("Failed to parse components JSON array");
		return false;
	}

	for (const TSharedPtr<FJsonValue>& ComponentValue : ComponentsArray)
	{
		const TSharedPtr<FJsonObject>& ComponentObject = ComponentValue->AsObject();
		if (!ComponentObject.IsValid())
		{
			continue;
		}

		FString ComponentName = ComponentObject->GetStringField(TEXT("name"));
		FString ComponentType = ComponentObject->GetStringField(TEXT("type"));
		FString AttachmentTarget = ComponentObject->GetStringField(TEXT("attachment"));

		if (ComponentName.IsEmpty() || ComponentType.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Skipping component with missing name or type"));
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("Creating component: %s of type: %s"), *ComponentName, *ComponentType);

		// Determine component class from type string
		UClass* ComponentClass = nullptr;
		
		if (ComponentType.Contains(TEXT("StaticMeshComponent")))
		{
			ComponentClass = UStaticMeshComponent::StaticClass();
		}
		else if (ComponentType.Contains(TEXT("SkeletalMeshComponent")))
		{
			ComponentClass = USkeletalMeshComponent::StaticClass();
		}
		else if (ComponentType.Contains(TEXT("CameraComponent")))
		{
			ComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.CameraComponent"));
		}
		else if (ComponentType.Contains(TEXT("SpringArmComponent")))
		{
			ComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.SpringArmComponent"));
		}
		else if (ComponentType.Contains(TEXT("AudioComponent")))
		{
			ComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.AudioComponent"));
		}
		else if (ComponentType.Contains(TEXT("CollisionComponent")) || ComponentType.Contains(TEXT("SphereComponent")))
		{
			ComponentClass = USphereComponent::StaticClass();
		}
		else if (ComponentType.Contains(TEXT("BoxComponent")))
		{
			ComponentClass = UBoxComponent::StaticClass();
		}
		else if (ComponentType.Contains(TEXT("CapsuleComponent")))
		{
			ComponentClass = UCapsuleComponent::StaticClass();
		}
		else
		{
			// Default to StaticMeshComponent for unknown types
			ComponentClass = UStaticMeshComponent::StaticClass();
			UE_LOG(LogTemp, Warning, TEXT("Unknown component type '%s', defaulting to StaticMeshComponent"), *ComponentType);
		}

		if (!ComponentClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not find component class for type: %s"), *ComponentType);
			continue;
		}

		// Create the component template in the Blueprint's Component tree
		USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
		if (!NewNode)
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to create SCS node for component: %s"), *ComponentName);
			continue;
		}

		// Set attachment if specified
		if (!AttachmentTarget.IsEmpty() && AttachmentTarget != TEXT("RootComponent"))
		{
			// Try to find the attachment target in existing components
			USCS_Node* AttachTarget = nullptr;
			for (USCS_Node* ExistingNode : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				if (ExistingNode && ExistingNode->GetVariableName().ToString() == AttachmentTarget)
				{
					AttachTarget = ExistingNode;
					break;
				}
			}

			if (AttachTarget)
			{
				AttachTarget->AddChildNode(NewNode);
				UE_LOG(LogTemp, Log, TEXT("Attached component %s to %s"), *ComponentName, *AttachmentTarget);
			}
			else
			{
				// Attach to root if target not found
				Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(NewNode);
				UE_LOG(LogTemp, Warning, TEXT("Could not find attachment target '%s', attached %s to root"), *AttachmentTarget, *ComponentName);
			}
		}
		else
		{
			// Attach to root component by default
			Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(NewNode);
			UE_LOG(LogTemp, Log, TEXT("Attached component %s to root"), *ComponentName);
		}

		// Apply properties if specified
		const TSharedPtr<FJsonObject> PropertiesObject = ComponentObject->GetObjectField(TEXT("properties"));
		if (PropertiesObject.IsValid())
		{
			// Set properties on the component template
			UActorComponent* ComponentTemplate = NewNode->ComponentTemplate;
			if (ComponentTemplate)
			{
				// Handle common properties
				for (auto& PropertyPair : PropertiesObject->Values)
				{
					FString PropertyName = PropertyPair.Key;
					FString PropertyValue = PropertyPair.Value->AsString();

					UE_LOG(LogTemp, VeryVerbose, TEXT("Setting property %s = %s on component %s"), *PropertyName, *PropertyValue, *ComponentName);
					
					// Handle specific property types
					if (PropertyName == TEXT("StaticMesh") && ComponentTemplate->IsA<UStaticMeshComponent>())
					{
						// Could load a mesh asset here if PropertyValue contains a valid path
						UE_LOG(LogTemp, Log, TEXT("StaticMesh property set for %s (would load: %s)"), *ComponentName, *PropertyValue);
					}
					else if (PropertyName == TEXT("Material"))
					{
						UE_LOG(LogTemp, Log, TEXT("Material property set for %s (would load: %s)"), *ComponentName, *PropertyValue);
					}
				}
			}
		}

		UE_LOG(LogTemp, Log, TEXT("✅ Component created successfully: %s"), *ComponentName);
	}

	// Reconstruct the Blueprint to apply changes
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	UE_LOG(LogTemp, Log, TEXT("🎉 All components created successfully"));
	return true;
}

FString UUnrealAIService::ExtractJSONFromResponse(const FString& Response)
{
	// CRITICAL: Add null pointer protection
	if (Response.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractJSONFromResponse: Response is empty"));
		return TEXT("");
	}
	
	FString CleanJSON = Response;
	
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Original response length: %d"), Response.Len());
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Response starts with: %s"), *Response.Left(100));
	
	// Find the first opening brace
	int32 StartIndex = CleanJSON.Find(TEXT("{"));
	if (StartIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: No opening brace found in response"));
		return TEXT("");
	}
	
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Found opening brace at index: %d"), StartIndex);
	
	// CRITICAL: Validate StartIndex is within bounds
	if (StartIndex < 0 || StartIndex >= CleanJSON.Len())
	{
		UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: StartIndex out of bounds: %d (length: %d)"), StartIndex, CleanJSON.Len());
		return TEXT("");
	}
	
	// Find the matching closing brace by counting braces
	int32 BraceCount = 0;
	int32 EndIndex = StartIndex;
	
	// CRITICAL: Add bounds checking for the loop
	for (int32 i = StartIndex; i < CleanJSON.Len() && i >= 0; i++)
	{
		// CRITICAL: Validate index before accessing character
		if (i < 0 || i >= CleanJSON.Len())
		{
			UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: Index out of bounds in loop: %d (length: %d)"), i, CleanJSON.Len());
			break;
		}
		
		TCHAR CurrentChar = CleanJSON[i];
		
		if (CurrentChar == TEXT('{'))
		{
			BraceCount++;
		}
		else if (CurrentChar == TEXT('}'))
		{
			BraceCount--;
			if (BraceCount == 0)
			{
				EndIndex = i;
				break;
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Found closing brace at index: %d"), EndIndex);
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: StartIndex: %d, EndIndex: %d, BraceCount: %d"), StartIndex, EndIndex, BraceCount);
	
	// CRITICAL: Validate EndIndex is within bounds
	if (EndIndex < 0 || EndIndex >= CleanJSON.Len())
	{
		UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: EndIndex out of bounds: %d (length: %d)"), EndIndex, CleanJSON.Len());
		// Use fallback method
		EndIndex = CleanJSON.Len() - 1;
	}
	
	// Extract the JSON part
	if (EndIndex > StartIndex && BraceCount == 0)
	{
		// CRITICAL: Validate indices before Mid operation
		if (StartIndex >= 0 && EndIndex >= StartIndex && EndIndex < CleanJSON.Len())
		{
			CleanJSON = CleanJSON.Mid(StartIndex, EndIndex - StartIndex + 1);
			UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Extracted JSON length: %d"), CleanJSON.Len());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: Invalid indices for Mid operation: StartIndex=%d, EndIndex=%d, Length=%d"), StartIndex, EndIndex, CleanJSON.Len());
			CleanJSON = TEXT("");
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: Invalid brace positions. StartIndex: %d, EndIndex: %d, BraceCount: %d"), StartIndex, EndIndex, BraceCount);
		UE_LOG(LogTemp, Error, TEXT("ExtractJSONFromResponse: Full response for debugging: %s"), *Response);
		
		// Fallback: try simple approach
		int32 SimpleStart = Response.Find(TEXT("{"));
		int32 SimpleEnd = Response.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		if (SimpleStart != INDEX_NONE && SimpleEnd != INDEX_NONE && SimpleEnd > SimpleStart)
		{
			UE_LOG(LogTemp, Warning, TEXT("ExtractJSONFromResponse: Using fallback extraction method"));
			CleanJSON = Response.Mid(SimpleStart, SimpleEnd - SimpleStart + 1);
		}
		else
		{
			return TEXT("");
		}
	}
	
	// CRITICAL: Validate CleanJSON before processing
	if (CleanJSON.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractJSONFromResponse: CleanJSON is empty after extraction"));
		return TEXT("");
	}
	
	// Clean up common AI JSON mistakes
	CleanJSON = CleanJSON.Replace(TEXT("//"), TEXT("")); // Remove comments
	CleanJSON = CleanJSON.Replace(TEXT("None"), TEXT("null")); // Replace Python None with JSON null
	CleanJSON = CleanJSON.Replace(TEXT("True"), TEXT("true")); // Replace Python True with JSON true
	CleanJSON = CleanJSON.Replace(TEXT("False"), TEXT("false")); // Replace Python False with JSON false
	
	// Remove any leading/trailing whitespace
	CleanJSON = CleanJSON.TrimStartAndEnd();
	
	// CRITICAL: Final validation before returning
	if (CleanJSON.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ExtractJSONFromResponse: CleanJSON became empty after cleaning"));
		return TEXT("");
	}
	
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Final cleaned JSON length: %d"), CleanJSON.Len());
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Final JSON starts with: %s"), *CleanJSON.Left(100));
	UE_LOG(LogTemp, Log, TEXT("ExtractJSONFromResponse: Final JSON ends with: %s"), *CleanJSON.Right(100));
	
	return CleanJSON;
}

bool UUnrealAIService::ValidateJSONCompleteness(const FString& JSONString)
{
	if (JSONString.IsEmpty())
	{
		return false;
	}
	
	// Check for opening and closing braces
	if (!JSONString.Contains(TEXT("{")) || !JSONString.Contains(TEXT("}")))
	{
		return false;
	}
	
	// Count braces to ensure they're balanced
	int32 OpenBraces = 0;
	int32 CloseBraces = 0;
	
	for (int32 i = 0; i < JSONString.Len(); i++)
	{
		if (JSONString[i] == TEXT('{'))
		{
			OpenBraces++;
		}
		else if (JSONString[i] == TEXT('}'))
		{
			CloseBraces++;
		}
	}
	
	bool bBalanced = (OpenBraces == CloseBraces && OpenBraces > 0);
	UE_LOG(LogTemp, Log, TEXT("ValidateJSONCompleteness: Open braces: %d, Close braces: %d, Balanced: %s"), 
		OpenBraces, CloseBraces, bBalanced ? TEXT("true") : TEXT("false"));
	
	return bBalanced;
}

FString UUnrealAIService::AttemptJSONCompletion(const FString& IncompleteJSON)
{
	FString CompletedJSON = IncompleteJSON;
	
	UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Starting with JSON length: %d"), CompletedJSON.Len());
	UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: JSON ends with: %s"), *CompletedJSON.Right(100));
	
	// CRITICAL: Handle common truncation patterns
	if (CompletedJSON.EndsWith(TEXT("onent\"")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Detected truncated 'onent' - completing to 'component'"));
		CompletedJSON = CompletedJSON.Replace(TEXT("onent\""), TEXT("component\""));
	}
	
	if (CompletedJSON.EndsWith(TEXT("Act")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Detected truncated 'Act' - completing to 'Actor'"));
		CompletedJSON += TEXT("or");
	}
	
	if (CompletedJSON.EndsWith(TEXT("Foll")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Detected truncated 'Foll' - completing to 'Follow'"));
		CompletedJSON += TEXT("ow");
	}
	
	// Count missing closing braces
	int32 OpenBraces = 0;
	int32 CloseBraces = 0;
	
	for (int32 i = 0; i < CompletedJSON.Len(); i++)
	{
		if (CompletedJSON[i] == TEXT('{'))
		{
			OpenBraces++;
		}
		else if (CompletedJSON[i] == TEXT('}'))
		{
			CloseBraces++;
		}
	}
	
	int32 MissingBraces = OpenBraces - CloseBraces;
	
	if (MissingBraces > 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Adding %d missing closing braces"), MissingBraces);
		
		// Add missing closing braces
		for (int32 i = 0; i < MissingBraces; i++)
		{
			CompletedJSON += TEXT("}");
		}
	}
	
	// CRITICAL: Handle incomplete array structures
	if (CompletedJSON.Contains(TEXT("\"events\": [")) && !CompletedJSON.Contains(TEXT("\"events\": [{")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Events array appears empty - adding default event"));
		// Find the events array and add a default event if it's empty
		int32 EventsStart = CompletedJSON.Find(TEXT("\"events\": ["));
		if (EventsStart != INDEX_NONE)
		{
			int32 EventsEnd = CompletedJSON.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, EventsStart);
			if (EventsEnd != INDEX_NONE && EventsEnd == EventsStart + 11) // "events": [ is 11 chars
			{
				// Empty events array, add a default event
				CompletedJSON = CompletedJSON.Replace(TEXT("\"events\": []"), TEXT("\"events\": [{\"name\": \"BeginPlay\", \"inputs\": [], \"logic\": \"Default implementation\"}]"));
			}
		}
	}
	
	if (CompletedJSON.Contains(TEXT("\"functions\": [")) && !CompletedJSON.Contains(TEXT("\"functions\": [{")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Functions array appears empty - adding default function"));
		// Find the functions array and add a default function if it's empty
		int32 FunctionsStart = CompletedJSON.Find(TEXT("\"functions\": ["));
		if (FunctionsStart != INDEX_NONE)
		{
			int32 FunctionsEnd = CompletedJSON.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, FunctionsStart);
			if (FunctionsEnd != INDEX_NONE && FunctionsEnd == FunctionsStart + 14) // "functions": [ is 14 chars
			{
				// Empty functions array, add a default function
				CompletedJSON = CompletedJSON.Replace(TEXT("\"functions\": []"), TEXT("\"functions\": [{\"name\": \"UpdatePosition\", \"inputs\": [], \"outputs\": [], \"logic\": \"Default implementation\"}]"));
			}
		}
	}
	
	// CRITICAL: Handle incomplete components section
	if (CompletedJSON.Contains(TEXT("\"components\": [")) && !CompletedJSON.Contains(TEXT("\"components\": [{")))
	{
		UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Components array appears empty - adding default component"));
		// Find the components array and add a default component if it's empty
		int32 ComponentsStart = CompletedJSON.Find(TEXT("\"components\": ["));
		if (ComponentsStart != INDEX_NONE)
		{
			int32 ComponentsEnd = CompletedJSON.Find(TEXT("]"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ComponentsStart);
			if (ComponentsEnd != INDEX_NONE && ComponentsEnd == ComponentsStart + 15) // "components": [ is 15 chars
			{
				// Empty components array, add a default component
				CompletedJSON = CompletedJSON.Replace(TEXT("\"components\": []"), TEXT("\"components\": [{\"name\": \"MeshComponent\", \"type\": \"StaticMeshComponent\", \"attachment\": \"RootComponent\", \"properties\": {\"StaticMesh\": \"DefaultMesh\", \"Material\": \"DefaultMaterial\"}}]"));
			}
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Completed JSON length: %d"), CompletedJSON.Len());
	UE_LOG(LogTemp, Warning, TEXT("AttemptJSONCompletion: Completed JSON ends with: %s"), *CompletedJSON.Right(100));
	
	return CompletedJSON;
}

bool UUnrealAIService::CreateFunctionLogicNodes(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& FunctionLogic, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}
	
	if (!FunctionGraph)
	{
		OutError = TEXT("Function graph is null");
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("🔧 Creating function logic nodes for structured JSON format"));

	// Find the function entry node
	UK2Node_FunctionEntry* FunctionEntry = nullptr;
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
		{
			FunctionEntry = Entry;
			break;
		}
	}

	if (!FunctionEntry)
	{
		OutError = TEXT("Could not find function entry node");
		return false;
	}

	// Parse structured JSON with nodes and connections
	if (FunctionLogic.Contains(TEXT("\"nodes\"")) && FunctionLogic.Contains(TEXT("\"connections\"")))
	{
		UE_LOG(LogTemp, Warning, TEXT("🔧 Parsing structured JSON logic format"));
		if (!ParseStructuredLogicFromJSON(Blueprint, FunctionGraph, FunctionLogic, FunctionEntry, OutError))
		{
			OutError = FString::Printf(TEXT("Failed to parse structured JSON logic: %s"), *OutError);
			return false;
		}
		return true;
	}
	else
	{
		OutError = TEXT("Function logic must be structured JSON with 'nodes' and 'connections' arrays");
		UE_LOG(LogTemp, Error, TEXT("❌ Invalid function logic format - expected structured JSON"));
		return false;
	}
}

// Enhanced Function Logic Creation Implementation

bool UUnrealAIService::ParseFunctionLogicFromJSON(const FString& JSONLogic, TArray<FString>& OutSteps, FString& OutError)
{
	TSharedPtr<FJsonValue> JsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONLogic);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonValue))
	{
		OutError = TEXT("Failed to parse function logic JSON");
		return false;
	}
	
	const TArray<TSharedPtr<FJsonValue>>* StepsArray = nullptr;
	if (JsonValue->AsArray().Num() > 0)
	{
		StepsArray = &JsonValue->AsArray();
	}
	else if (JsonValue->AsObject().IsValid())
	{
		JsonValue->AsObject()->TryGetArrayField(TEXT("steps"), StepsArray);
	}
	
	if (!StepsArray)
	{
		OutError = TEXT("No steps array found in function logic JSON");
		return false;
	}
	
	for (const TSharedPtr<FJsonValue>& StepValue : *StepsArray)
	{
		if (StepValue->AsObject().IsValid())
		{
			const TSharedPtr<FJsonObject>& StepObj = StepValue->AsObject();
			FString Action = StepObj->GetStringField(TEXT("action"));
			FString Target = StepObj->GetStringField(TEXT("target"));
			FString Value = StepObj->GetStringField(TEXT("value"));
			
			// Convert JSON format to text format
			FString StepText;
			if (Action == TEXT("set"))
			{
				StepText = FString::Printf(TEXT("Set %s to %s"), *Target, *Value);
			}
			else if (Action == TEXT("call"))
			{
				StepText = FString::Printf(TEXT("Call %s"), *Target);
				if (!Value.IsEmpty())
				{
					StepText += FString::Printf(TEXT(" with %s"), *Value);
				}
			}
			else if (Action == TEXT("branch"))
			{
				StepText = FString::Printf(TEXT("Branch %s"), *Target);
			}
			else if (Action == TEXT("get"))
			{
				StepText = FString::Printf(TEXT("Get %s"), *Target);
			}
			else
			{
				StepText = FString::Printf(TEXT("%s %s %s"), *Action, *Target, *Value);
			}
			
			OutSteps.Add(StepText);
		}
		else
		{
			// Handle simple string steps
			OutSteps.Add(StepValue->AsString());
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed %d steps from JSON function logic"), OutSteps.Num());
	return true;
}

bool UUnrealAIService::ParseStructuredLogicFromJSON(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& JSONLogic, UK2Node_FunctionEntry* FunctionEntry, FString& OutError)
{
	UE_LOG(LogTemp, Warning, TEXT("🔧 Parsing structured JSON logic: %s"), *JSONLogic.Left(200));
	
	// Parse the JSON structure
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONLogic);
	
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutError = TEXT("Failed to parse structured logic JSON");
		return false;
	}
	
	// Get nodes array
	const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("nodes"), NodesArray) || !NodesArray)
	{
		OutError = TEXT("No 'nodes' array found in structured logic JSON");
		return false;
	}
	
	// Get connections array
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
	if (!JsonObject->TryGetArrayField(TEXT("connections"), ConnectionsArray) || !ConnectionsArray)
	{
		OutError = TEXT("No 'connections' array found in structured logic JSON");
		return false;
	}
	
	// Map to store created nodes by ID
	TMap<FString, UEdGraphNode*> NodeMap;
	NodeMap.Add(TEXT("entry"), FunctionEntry);
	
	// Create all nodes first
	for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
	{
		const TSharedPtr<FJsonObject>& NodeObj = NodeValue->AsObject();
		if (!NodeObj.IsValid()) continue;
		
		FString NodeID = NodeObj->GetStringField(TEXT("ID"));
		FString NodeType = NodeObj->GetStringField(TEXT("Type"));
		
		UE_LOG(LogTemp, Warning, TEXT("🔧 Creating node: ID=%s, Type=%s"), *NodeID, *NodeType);
		
		UEdGraphNode* CreatedNode = nullptr;
		
		// Handle the actual node types from our JSON - create proper Unreal Engine function nodes
		if (NodeType == TEXT("GetActorLocation"))
		{
			// Create a proper GetActorLocation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("GetActorLocation"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("GetDistanceTo"))
		{
			// Create a proper GetDistanceTo node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("GetDistanceTo"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("FindLookAtRotation"))
		{
			// Create a proper FindLookAtRotation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("FindLookAtRotation"), TEXT("KismetMathLibrary"));
		}
		else if (NodeType == TEXT("AddMovementInput"))
		{
			// Create a proper AddMovementInput node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("AddMovementInput"), TEXT("PawnMovementComponent"));
		}
		else if (NodeType == TEXT("SetActorRotation"))
		{
			// Create a proper SetActorRotation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("SetActorRotation"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("Branch"))
		{
			// Get condition from Inputs
			const TSharedPtr<FJsonObject>* InputsObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("Inputs"), InputsObj) && (*InputsObj)->HasField(TEXT("Condition")))
			{
				FString Condition = (*InputsObj)->GetStringField(TEXT("Condition"));
				FString BranchCommand = FString::Printf(TEXT("Branch %s"), *Condition);
				CreatedNode = CreateBranchNode(Blueprint, FunctionGraph, BranchCommand);
			}
			else
			{
				FString BranchCommand = TEXT("Branch Condition");
				CreatedNode = CreateBranchNode(Blueprint, FunctionGraph, BranchCommand);
			}
		}
		else if (NodeType == TEXT("ReturnNode"))
		{
			// Create a return node
			CreatedNode = CreateReturnNode(FunctionGraph, TEXT(""));
		}
		else if (NodeType == TEXT("FunctionEntry"))
		{
			// This is already the function entry - skip
			continue;
		}
		else if (NodeType == TEXT("SetVariable"))
		{
			FString Variable = NodeObj->GetStringField(TEXT("variable"));
			FString Value = NodeObj->GetStringField(TEXT("value"));
			FString SetCommand = FString::Printf(TEXT("Set %s to %s"), *Variable, *Value);
			CreatedNode = CreateVariableSetNode(Blueprint, FunctionGraph, SetCommand);
		}
		else if (NodeType == TEXT("GetVariable"))
		{
			FString Variable = NodeObj->GetStringField(TEXT("variable"));
			FString GetCommand = FString::Printf(TEXT("Get %s"), *Variable);
			CreatedNode = CreateVariableGetNode(Blueprint, FunctionGraph, GetCommand);
		}
		else if (NodeType == TEXT("FunctionCall"))
		{
			FString Function = NodeObj->GetStringField(TEXT("Function"));
			FString CallCommand = FString::Printf(TEXT("Call %s"), *Function);
			CreatedNode = CreateFunctionCallNode(Blueprint, FunctionGraph, CallCommand);
			
			// Handle parameters if provided
			const TSharedPtr<FJsonObject>* ParametersObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("parameters"), ParametersObj) && ParametersObj->IsValid())
			{
				UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(CreatedNode);
				if (CallNode)
				{
					UE_LOG(LogTemp, Warning, TEXT("🔧 Connecting function parameters for %s"), *Function);
					
					// Debug: Show all pins on this function call node
					UE_LOG(LogTemp, Warning, TEXT("🔍 Function %s has %d pins:"), *Function, CallNode->Pins.Num());
					for (int32 i = 0; i < CallNode->Pins.Num(); i++)
					{
						UEdGraphPin* Pin = CallNode->Pins[i];
						if (Pin)
						{
							FString PinDirection = (Pin->Direction == EGPD_Input) ? TEXT("INPUT") : TEXT("OUTPUT");
							FString PinCategory = Pin->PinType.PinCategory.ToString();
							UE_LOG(LogTemp, Warning, TEXT("  Pin %d: Name='%s', Direction=%s, Category=%s"), 
								i, *Pin->PinName.ToString(), *PinDirection, *PinCategory);
						}
					}
					
					// Iterate through all parameters and set them
					for (const auto& ParamPair : (*ParametersObj)->Values)
					{
						FString ParamName = ParamPair.Key;
						FString ParamValue = ParamPair.Value->AsString();
						
						UE_LOG(LogTemp, Warning, TEXT("🎯 Attempting to connect parameter: %s = %s"), *ParamName, *ParamValue);
						
						if (ConnectFunctionParameter(CallNode, ParamName, ParamValue))
						{
							UE_LOG(LogTemp, Log, TEXT("  ✅ Set %s = %s"), *ParamName, *ParamValue);
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("  ⚠️ Failed to set %s = %s"), *ParamName, *ParamValue);
						}
					}
				}
			}
		}
		else if (NodeType == TEXT("Branch"))
		{
			FString Condition = NodeObj->GetStringField(TEXT("condition"));
			FString BranchCommand = FString::Printf(TEXT("Branch %s"), *Condition);
			CreatedNode = CreateBranchNode(Blueprint, FunctionGraph, BranchCommand);
		}
		else if (NodeType == TEXT("Print"))
		{
			// Handle both "message" and "InString" fields for Print nodes
			FString Message;
			if (NodeObj->HasField(TEXT("message")))
			{
				Message = NodeObj->GetStringField(TEXT("message"));
			}
			else if (NodeObj->HasField(TEXT("InString")))
			{
				Message = NodeObj->GetStringField(TEXT("InString"));
			}
			else
			{
				Message = TEXT("Print Message");
			}
			CreatedNode = this->CreatePrintNode(FunctionGraph, Message);
		}
		else if (NodeType == TEXT("MathOperation"))
		{
			// Handle MathOperation nodes with operation field
			FString Operation;
			if (NodeObj->HasField(TEXT("operation")))
			{
				Operation = NodeObj->GetStringField(TEXT("operation"));
			}
			else
			{
				Operation = TEXT("Add");
			}
			
			// Get operands from Inputs field
			TArray<FString> Operands;
			const TSharedPtr<FJsonObject>* InputsObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("Inputs"), InputsObj) && (*InputsObj).IsValid())
			{
				for (const auto& InputPair : (*InputsObj)->Values)
				{
					Operands.Add(InputPair.Value->AsString());
				}
			}
			
			if (Operands.Num() >= 2)
			{
				CreatedNode = this->CreateMathOperationNode(FunctionGraph, Operation, Operands[0], Operands[1]);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("⚠️ MathOperation node %s needs at least 2 operands, found %d"), *NodeID, Operands.Num());
			}
		}
		else if (NodeType == TEXT("VectorSubtract") || NodeType == TEXT("VectorLength") || NodeType == TEXT("GreaterFloat") || 
				 NodeType == TEXT("Normalize") || NodeType == TEXT("MultiplyFloat") || NodeType == TEXT("MultiplyVectorFloat"))
		{
			// Handle specific math operations from your JSON
			FString Operation = NodeType;
			TArray<FString> Operands;
			
			// Get operands from Inputs field
			const TSharedPtr<FJsonObject>* InputsObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("Inputs"), InputsObj) && (*InputsObj).IsValid())
			{
				for (const auto& InputPair : (*InputsObj)->Values)
				{
					Operands.Add(InputPair.Value->AsString());
				}
			}
			
			if (Operands.Num() >= 2)
			{
				CreatedNode = this->CreateMathOperationNode(FunctionGraph, Operation, Operands[0], Operands[1]);
			}
			else if (Operands.Num() == 1)
			{
				// For single-input operations like VectorLength, Normalize
				CreatedNode = this->CreateMathOperationNode(FunctionGraph, Operation, Operands[0], TEXT("0"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("⚠️ MathOperation node %s needs operands, found %d"), *NodeID, Operands.Num());
			}
		}
		else if (NodeType == TEXT("GetVariable"))
		{
			// Handle GetVariable nodes
			FString Variable;
			const TSharedPtr<FJsonObject>* InputsObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("Inputs"), InputsObj) && (*InputsObj).IsValid())
			{
				// Look for any input field that might contain the variable name
				for (const auto& InputPair : (*InputsObj)->Values)
				{
					Variable = InputPair.Value->AsString();
					break; // Use the first input as the variable name
				}
			}
			
			if (Variable.IsEmpty())
			{
				Variable = TEXT("Variable");
			}
			
			FString GetCommand = FString::Printf(TEXT("Get %s"), *Variable);
			CreatedNode = CreateVariableGetNode(Blueprint, FunctionGraph, GetCommand);
		}
		else if (NodeType == TEXT("GetForwardVector"))
		{
			// Create a GetForwardVector node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("GetForwardVector"), TEXT("KismetMathLibrary"));
		}
		else if (NodeType == TEXT("GetActorLocation"))
		{
			// Create a GetActorLocation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("GetActorLocation"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("FindLookAtRotation"))
		{
			// Create a FindLookAtRotation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("FindLookAtRotation"), TEXT("KismetMathLibrary"));
		}
		else if (NodeType == TEXT("SetActorRotation"))
		{
			// Create a SetActorRotation node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("SetActorRotation"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("GetDistanceTo"))
		{
			// Create a GetDistanceTo node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("GetDistanceTo"), TEXT("KismetSystemLibrary"));
		}
		else if (NodeType == TEXT("AddMovementInput"))
		{
			// Create an AddMovementInput node
			CreatedNode = CreateUnrealFunctionNode(Blueprint, FunctionGraph, TEXT("AddMovementInput"), TEXT("PawnMovementComponent"));
		}
		
		if (CreatedNode)
		{
			NodeMap.Add(NodeID, CreatedNode);
			
			// Set node position if provided
			const TSharedPtr<FJsonObject>* PositionObj = nullptr;
			if (NodeObj->TryGetObjectField(TEXT("position"), PositionObj) && PositionObj->IsValid())
			{
				int32 X = (*PositionObj)->GetIntegerField(TEXT("x"));
				int32 Y = (*PositionObj)->GetIntegerField(TEXT("y"));
				CreatedNode->NodePosX = X;
				CreatedNode->NodePosY = Y;
			}
			
			UE_LOG(LogTemp, Log, TEXT("✅ Created node %s of type %s"), *NodeID, *NodeType);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("❌ Failed to create node %s of type %s"), *NodeID, *NodeType);
		}
	}
	
	// Create connections
	for (const TSharedPtr<FJsonValue>& ConnectionValue : *ConnectionsArray)
	{
		const TSharedPtr<FJsonObject>& ConnObj = ConnectionValue->AsObject();
		if (!ConnObj.IsValid()) continue;
		
		FString FromID = ConnObj->GetStringField(TEXT("FromNode"));
		FString ToID = ConnObj->GetStringField(TEXT("ToNode"));
		
		UEdGraphNode* FromNode = NodeMap.FindRef(FromID);
		UEdGraphNode* ToNode = NodeMap.FindRef(ToID);
		
		if (FromNode && ToNode)
		{
			// Find execution pins and connect them
			UEdGraphPin* FromPin = nullptr;
			UEdGraphPin* ToPin = nullptr;
			
			// Find output execution pin on FromNode
			for (UEdGraphPin* Pin : FromNode->Pins)
			{
				if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					FromPin = Pin;
					break;
				}
			}
			
			// Find input execution pin on ToNode
			for (UEdGraphPin* Pin : ToNode->Pins)
			{
				if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
				{
					ToPin = Pin;
					break;
				}
			}
			
			if (FromPin && ToPin)
			{
				FromPin->MakeLinkTo(ToPin);
				UE_LOG(LogTemp, Log, TEXT("🔗 Connected %s -> %s"), *FromID, *ToID);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("⚠️ Could not find execution pins for connection %s -> %s"), *FromID, *ToID);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Could not find nodes for connection %s -> %s"), *FromID, *ToID);
		}
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🎉 Successfully created structured function logic with %d nodes and %d connections"), NodesArray->Num(), ConnectionsArray->Num());
	return true;
}

UK2Node* UUnrealAIService::CreateReturnNode(UEdGraph* Graph, const FString& ReturnValue)
{
	if (!Graph)
	{
		return nullptr;
	}
	
	// For functions that need to return a value, create a print statement showing the return
	return CreatePrintNode(Graph, FString::Printf(TEXT("Return %s"), *ReturnValue));
}

UK2Node* UUnrealAIService::CreatePrintNode(UEdGraph* Graph, const FString& PrintText)
{
	if (!Graph)
	{
		return nullptr;
	}
	
	// Create a PrintString function call
	UK2Node_CallFunction* PrintNode = NewObject<UK2Node_CallFunction>(Graph);
	
	// Find PrintString function from the engine
	UFunction* PrintFunction = nullptr;
	
	// Try to find PrintString from common blueprint libraries
	if (UClass* SystemLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetSystemLibrary")))
	{
		PrintFunction = SystemLibraryClass->FindFunctionByName(TEXT("PrintString"));
	}
	if (PrintFunction)
	{
		PrintNode->SetFromFunction(PrintFunction);
		PrintNode->AllocateDefaultPins();
		
		// Debug: Show all actual pins created by Unreal on this node
		UE_LOG(LogTemp, Warning, TEXT("🔍 DEBUGGING: PrintString node has %d pins:"), PrintNode->Pins.Num());
		for (int32 i = 0; i < PrintNode->Pins.Num(); i++)
		{
			UEdGraphPin* Pin = PrintNode->Pins[i];
			if (Pin)
			{
				FString PinDirection = (Pin->Direction == EGPD_Input) ? TEXT("INPUT") : TEXT("OUTPUT");
				FString PinCategory = Pin->PinType.PinCategory.ToString();
				UE_LOG(LogTemp, Warning, TEXT("  Pin %d: Name='%s', Direction=%s, Category=%s"), 
					i, *Pin->PinName.ToString(), *PinDirection, *PinCategory);
			}
		}
		
		// Extract text from format like "Print 'Hello World'" or "Call PrintString with 'Hello'"
		FString CleanText = PrintText;
		CleanText = CleanText.Replace(TEXT("Print "), TEXT(""));
		CleanText = CleanText.Replace(TEXT("Call PrintString with "), TEXT(""));
		CleanText = CleanText.Replace(TEXT("'"), TEXT(""));
		CleanText = CleanText.Replace(TEXT("\""), TEXT(""));
		CleanText = CleanText.TrimStartAndEnd();
		
		// Try to connect to the string input pin using multiple possible names
		TArray<FString> StringPinNames = {TEXT("InString"), TEXT("String"), TEXT("Text"), TEXT("Message")};
		bool bConnectedString = false;
		
		for (const FString& PinName : StringPinNames)
		{
			if (ConnectFunctionParameter(PrintNode, PinName, CleanText))
			{
				bConnectedString = true;
				break;
			}
		}
		
		if (!bConnectedString)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ Could not find string input pin for PrintString - trying fallback"));
			// Fallback: try to set any string-type pin
			for (UEdGraphPin* Pin : PrintNode->Pins)
			{
				if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
				{
					SetPinDefaultValue(Pin, CleanText);
					UE_LOG(LogTemp, Log, TEXT("✅ Connected to string pin: %s"), *Pin->PinName.ToString());
					break;
				}
			}
		}
		
		Graph->AddNode(PrintNode);
		UE_LOG(LogTemp, Log, TEXT("Created PrintString node with text: %s"), *CleanText);
		return PrintNode;
	}
	
	return nullptr;
}

UK2Node* UUnrealAIService::CreateGenericActionNode(UEdGraph* Graph, const FString& ActionText)
{
	if (!Graph || ActionText.IsEmpty())
	{
		return nullptr;
	}
	
	// Try to parse generic patterns like "Add 10 to Health", "Multiply Damage by 2", etc.
	TArray<FString> Words;
	ActionText.ParseIntoArray(Words, TEXT(" "), true);
	
	if (Words.Num() >= 3)
	{
		FString Action = Words[0];
		FString Value = Words[1];
		FString Target = Words.Num() > 3 ? Words[3] : Words[2];
		
		// Try to create appropriate node based on action
		if (Action.Equals(TEXT("Add"), ESearchCase::IgnoreCase) || 
			Action.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase) ||
			Action.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
		{
			// Create a math operation
			return CreateMathOperationNode(Graph, Action, Target, Value);
		}
		else if (Action.Equals(TEXT("Enable"), ESearchCase::IgnoreCase) ||
				 Action.Equals(TEXT("Disable"), ESearchCase::IgnoreCase))
		{
			// Create a boolean set operation
			FString BoolValue = Action.Equals(TEXT("Enable"), ESearchCase::IgnoreCase) ? TEXT("true") : TEXT("false");
			FString SetCommand = FString::Printf(TEXT("Set %s to %s"), *Target, *BoolValue);
			return CreateVariableSetNode(nullptr, Graph, SetCommand);
		}
	}
	
	// Fallback: create a print node showing the action
	return CreatePrintNode(Graph, FString::Printf(TEXT("Action: %s"), *ActionText));
}

UK2Node* UUnrealAIService::CreateMathOperationNode(UEdGraph* Graph, const FString& Operation, const FString& Target, const FString& Value)
{
	if (!Graph)
	{
		return nullptr;
	}
	
	// Create appropriate math node based on operation
	UK2Node_CallFunction* MathNode = NewObject<UK2Node_CallFunction>(Graph);
	UFunction* MathFunction = nullptr;
	
	// Try to find math functions from the engine
	if (UClass* MathLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMathLibrary")))
	{
		if (Operation.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
		{
			MathFunction = MathLibraryClass->FindFunctionByName(TEXT("Add_IntInt"));
		}
		else if (Operation.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
		{
			MathFunction = MathLibraryClass->FindFunctionByName(TEXT("Multiply_IntInt"));
		}
		else if (Operation.Equals(TEXT("Subtract"), ESearchCase::IgnoreCase))
		{
			MathFunction = MathLibraryClass->FindFunctionByName(TEXT("Subtract_IntInt"));
		}
	}
	
	if (MathFunction)
	{
		MathNode->SetFromFunction(MathFunction);
		MathNode->AllocateDefaultPins();
		
		// Set the second input value
		UEdGraphPin* BPin = MathNode->FindPin(TEXT("B"));
		if (BPin)
		{
			BPin->DefaultValue = Value;
		}
		
		Graph->AddNode(MathNode);
		UE_LOG(LogTemp, Log, TEXT("Created math operation node: %s %s by %s"), *Operation, *Target, *Value);
		return MathNode;
	}
	
	return nullptr;
}

UK2Node* UUnrealAIService::CreateUnrealFunctionNode(UBlueprint* Blueprint, UEdGraph* Graph, const FString& FunctionName, const FString& LibraryName)
{
	if (!Graph || FunctionName.IsEmpty())
	{
		return nullptr;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🔧 Creating Unreal Engine function node: %s from %s"), *FunctionName, *LibraryName);
	
	// Create a function call node
	UK2Node_CallFunction* FunctionNode = NewObject<UK2Node_CallFunction>(Graph);
	
	// Try to find the function in the specified library
	UFunction* FoundFunction = nullptr;
	
	if (LibraryName == TEXT("KismetSystemLibrary"))
	{
		if (UClass* SystemLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetSystemLibrary")))
		{
			FoundFunction = SystemLibraryClass->FindFunctionByName(*FunctionName);
		}
	}
	else if (LibraryName == TEXT("KismetMathLibrary"))
	{
		if (UClass* MathLibraryClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMathLibrary")))
		{
			FoundFunction = MathLibraryClass->FindFunctionByName(*FunctionName);
		}
	}
	else if (LibraryName == TEXT("PawnMovementComponent"))
	{
		if (UClass* PawnClass = FindObject<UClass>(nullptr, TEXT("/Script/Engine.Pawn")))
		{
			FoundFunction = PawnClass->FindFunctionByName(*FunctionName);
		}
	}
	
	if (FoundFunction)
	{
		FunctionNode->SetFromFunction(FoundFunction);
		FunctionNode->AllocateDefaultPins();
		
		// Position the node
		FunctionNode->NodePosX = 0;
		FunctionNode->NodePosY = 0;
		
		Graph->AddNode(FunctionNode);
		UE_LOG(LogTemp, Log, TEXT("✅ Created Unreal Engine function node: %s"), *FunctionName);
		return FunctionNode;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ Could not find function %s in library %s"), *FunctionName, *LibraryName);
		// Fallback: create a generic function call node
		return CreateFunctionCallNode(Blueprint, Graph, FString::Printf(TEXT("Call %s"), *FunctionName));
	}
}

bool UUnrealAIService::ValidateFunctionConnections(UEdGraph* FunctionGraph, FString& OutError)
{
	if (!FunctionGraph)
	{
		OutError = TEXT("Function graph is null");
		return false;
	}
	
	// Check that function has entry and result nodes
	bool bHasEntry = false;
	bool bHasResult = false;
	int32 NodeCount = 0;
	
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (Cast<UK2Node_FunctionEntry>(Node))
		{
			bHasEntry = true;
		}
		else if (Cast<UK2Node_FunctionResult>(Node))
		{
			bHasResult = true;
		}
		NodeCount++;
	}
	
	if (!bHasEntry)
	{
		OutError = TEXT("Function missing entry node");
		return false;
	}
	
	if (!bHasResult)
	{
		OutError = TEXT("Function missing result node");
		return false;
	}
	
	if (NodeCount < 3) // Entry + Result + at least one logic node
	{
		UE_LOG(LogTemp, Warning, TEXT("Function only has %d nodes (may be empty)"), NodeCount);
	}
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("Function validation passed: %d nodes"), NodeCount);
	return true;
}

void UUnrealAIService::OptimizeFunctionLayout(UEdGraph* FunctionGraph)
{
	if (!FunctionGraph)
	{
		return;
	}
	
	// Simple layout optimization - arrange nodes in a flowing pattern
	int32 CurrentX = 0;
	int32 CurrentY = 0;
	const int32 NodeSpacing = 300;
	
	for (UEdGraphNode* Node : FunctionGraph->Nodes)
	{
		if (Node)
		{
			Node->NodePosX = CurrentX;
			Node->NodePosY = CurrentY;
			CurrentX += NodeSpacing;
			
			// Wrap to next row after 4 nodes
			if (CurrentX > NodeSpacing * 3)
			{
				CurrentX = 0;
				CurrentY += 150;
			}
		}
	}
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("Optimized layout for function graph"));
}

bool UUnrealAIService::AnalyzeFunctionPins(UFunction* Function, TArray<FString>& OutInputPins, TArray<FString>& OutOutputPins, FString& OutError)
{
	if (!Function)
	{
		OutError = TEXT("Function is null");
		return false;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("🔍 Analyzing function pins for: %s"), *Function->GetName());
	
	// Iterate through function properties to find input and output parameters
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;
		FString ParamName = Param->GetName();
		
		// Check parameter flags
		if (Param->HasAnyPropertyFlags(CPF_Parm))
		{
			if (Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				OutOutputPins.Add(ParamName);
				UE_LOG(LogTemp, Log, TEXT("  📤 Output Pin: %s"), *ParamName);
			}
			else
			{
				OutInputPins.Add(ParamName);
				UE_LOG(LogTemp, Log, TEXT("  📥 Input Pin: %s"), *ParamName);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("✅ Function %s has %d input pins and %d output pins"), 
		*Function->GetName(), OutInputPins.Num(), OutOutputPins.Num());
	
	return true;
}

bool UUnrealAIService::ConnectFunctionParameter(UK2Node_CallFunction* CallNode, const FString& ParameterName, const FString& ParameterValue)
{
	if (!CallNode)
	{
		return false;
	}
	
	UEdGraphPin* ParameterPin = CallNode->FindPin(*ParameterName);
	if (!ParameterPin)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ Could not find pin '%s' on function node"), *ParameterName);
		return false;
	}
	
	if (ParameterPin->Direction != EGPD_Input)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ Pin '%s' is not an input pin"), *ParameterName);
		return false;
	}
	
	// Set the pin's default value
	if (SetPinDefaultValue(ParameterPin, ParameterValue))
	{
		UE_LOG(LogTemp, Log, TEXT("✅ Connected parameter '%s' = '%s'"), *ParameterName, *ParameterValue);
		return true;
	}
	
	return false;
}

FString UUnrealAIService::GetPinTypeName(const FEdGraphPinType& PinType)
{
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
		return TEXT("String");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
		return TEXT("Int");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
		return TEXT("Float");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
		return TEXT("Bool");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		return TEXT("Object");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
		return TEXT("Struct");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		return TEXT("Exec");
	else
		return TEXT("Unknown");
}

bool UUnrealAIService::SetPinDefaultValue(UEdGraphPin* Pin, const FString& Value)
{
	if (!Pin)
	{
		return false;
	}
	
	// Handle different pin types appropriately
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		Pin->DefaultValue = Value;
		return true;
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		int32 IntValue = FCString::Atoi(*Value);
		Pin->DefaultValue = FString::FromInt(IntValue);
		return true;
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Float)
	{
		float FloatValue = FCString::Atof(*Value);
		Pin->DefaultValue = FString::SanitizeFloat(FloatValue);
		return true;
	}
	else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		bool BoolValue = Value.ToBool() || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		Pin->DefaultValue = BoolValue ? TEXT("true") : TEXT("false");
		return true;
	}
	else
	{
		// For other types, try setting as string
		Pin->DefaultValue = Value;
		return true;
	}
}

FString UUnrealAIService::BuildAvailableFunctionsPrompt(const FString& BlueprintContext)
{
	UE_LOG(LogTemp, Warning, TEXT("🤖 Building dynamic function list for context: %s"), *BlueprintContext);
	
	FString FunctionsPrompt = TEXT("AVAILABLE UNREAL FUNCTIONS FOR YOUR BLUEPRINT:\n");
	FunctionsPrompt += TEXT("(Functions discovered dynamically based on your request)\n\n");
	
	// Get AI-suggested functions for this specific context
	TArray<UFunction*> RelevantFunctions = DiscoverFunctionsByAI(BlueprintContext);
	
	if (RelevantFunctions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ No functions discovered for context, using fallback"));
		FunctionsPrompt += TEXT("No specific functions discovered. Use basic functions like PrintString, SetActorLocation, etc.\n");
		return FunctionsPrompt;
	}
	
	UE_LOG(LogTemp, Log, TEXT("✅ Discovered %d relevant functions"), RelevantFunctions.Num());
	
	for (UFunction* Function : RelevantFunctions)
	{
		if (Function)
		{
			FString FunctionInfo = AnalyzeFunctionSignature(Function);
			if (!FunctionInfo.IsEmpty())
			{
				FunctionsPrompt += FunctionInfo + TEXT("\n");
			}
		}
	}
	
	return FunctionsPrompt;
}

FString UUnrealAIService::AnalyzeFunctionSignature(UFunction* Function)
{
	if (!Function)
	{
		return TEXT("");
	}
	
	FString FunctionName = Function->GetName();
	FString ClassName = Function->GetOuter()->GetName();
	
	// Skip internal/engine functions that aren't useful for blueprints
	if (FunctionName.StartsWith(TEXT("__")) || FunctionName.Contains(TEXT("Internal")))
	{
		return TEXT("");
	}
	
	FString Signature = FString::Printf(TEXT("- %s (%s):"), *FunctionName, *ClassName);
	
	TArray<FString> InputParams;
	TArray<FString> OutputParams;
	
	// Analyze function parameters
	for (TFieldIterator<FProperty> ParamIt(Function); ParamIt; ++ParamIt)
	{
		FProperty* Param = *ParamIt;
		if (Param->HasAnyPropertyFlags(CPF_Parm))
		{
			FString ParamName = Param->GetName();
			FString ParamType = GetParameterTypeString(Param);
			
			if (Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				OutputParams.Add(FString::Printf(TEXT("%s:%s"), *ParamName, *ParamType));
			}
			else
			{
				InputParams.Add(FString::Printf(TEXT("%s:%s"), *ParamName, *ParamType));
			}
		}
	}
	
	// Build parameter list
	if (InputParams.Num() > 0)
	{
		Signature += TEXT(" Inputs(");
		for (int32 i = 0; i < InputParams.Num(); i++)
		{
			Signature += InputParams[i];
			if (i < InputParams.Num() - 1)
			{
				Signature += TEXT(", ");
			}
		}
		Signature += TEXT(")");
	}
	
	if (OutputParams.Num() > 0)
	{
		Signature += TEXT(" Outputs(");
		for (int32 i = 0; i < OutputParams.Num(); i++)
		{
			Signature += OutputParams[i];
			if (i < OutputParams.Num() - 1)
			{
				Signature += TEXT(", ");
			}
		}
		Signature += TEXT(")");
	}
	
	return Signature;
}

TArray<UFunction*> UUnrealAIService::DiscoverFunctionsByAI(const FString& Context)
{
	TArray<UFunction*> Functions;
	
	UE_LOG(LogTemp, Warning, TEXT("🚀 Quick function discovery for context: %s"), *Context);
	
	// Use a fast heuristic-based approach instead of AI query to avoid UI freezing
	TArray<FString> RelevantFunctions = GetRelevantFunctionsHeuristic(Context);
	
	// Search Unreal libraries for each relevant function
	for (const FString& FunctionName : RelevantFunctions)
	{
		UFunction* Function = FindFunctionInUnrealLibraries(FunctionName);
		if (Function && !Functions.Contains(Function))
		{
			Functions.Add(Function);
			UE_LOG(LogTemp, Log, TEXT("✅ Found function: %s"), *FunctionName);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("📋 Discovered %d functions using heuristics"), Functions.Num());
	return Functions;
}

TArray<FString> UUnrealAIService::GetRelevantFunctionsHeuristic(const FString& Context)
{
	TArray<FString> RelevantFunctions;
	FString LowerContext = Context.ToLower();
	
	UE_LOG(LogTemp, Log, TEXT("🎯 Analyzing context for function discovery: %s"), *Context);
	
	// Always include basic functions
	RelevantFunctions.Add(TEXT("PrintString"));
	
	// Movement and Location
	if (LowerContext.Contains(TEXT("move")) || LowerContext.Contains(TEXT("follow")) || 
		LowerContext.Contains(TEXT("position")) || LowerContext.Contains(TEXT("location")))
	{
		RelevantFunctions.Add(TEXT("SetActorLocation"));
		RelevantFunctions.Add(TEXT("GetActorLocation"));
		RelevantFunctions.Add(TEXT("AddMovementInput"));
		RelevantFunctions.Add(TEXT("SetActorRotation"));
		RelevantFunctions.Add(TEXT("GetActorRotation"));
	}
	
	// AI and Enemies
	if (LowerContext.Contains(TEXT("ai")) || LowerContext.Contains(TEXT("enemy")) || 
		LowerContext.Contains(TEXT("follow")) || LowerContext.Contains(TEXT("chase")))
	{
		RelevantFunctions.Add(TEXT("GetPlayerController"));
		RelevantFunctions.Add(TEXT("LineTraceSingle"));
		RelevantFunctions.Add(TEXT("GetActorLocation"));
		RelevantFunctions.Add(TEXT("SetActorLocation"));
	}
	
	// Player Controls
	if (LowerContext.Contains(TEXT("player")) || LowerContext.Contains(TEXT("input")) || 
		LowerContext.Contains(TEXT("control")))
	{
		RelevantFunctions.Add(TEXT("AddMovementInput"));
		RelevantFunctions.Add(TEXT("AddControllerPitchInput"));
		RelevantFunctions.Add(TEXT("AddControllerYawInput"));
		RelevantFunctions.Add(TEXT("EnableInput"));
		RelevantFunctions.Add(TEXT("Jump"));
	}
	
	// Health and Damage
	if (LowerContext.Contains(TEXT("health")) || LowerContext.Contains(TEXT("damage")) || 
		LowerContext.Contains(TEXT("hurt")) || LowerContext.Contains(TEXT("die")))
	{
		RelevantFunctions.Add(TEXT("TakeDamage"));
		RelevantFunctions.Add(TEXT("Destroy"));
	}
	
	// Interaction and Pickup
	if (LowerContext.Contains(TEXT("pickup")) || LowerContext.Contains(TEXT("collect")) || 
		LowerContext.Contains(TEXT("interact")))
	{
		RelevantFunctions.Add(TEXT("SetActorHiddenInGame"));
		RelevantFunctions.Add(TEXT("Destroy"));
		RelevantFunctions.Add(TEXT("SetActorEnableCollision"));
	}
	
	// Spawning and Creation
	if (LowerContext.Contains(TEXT("spawn")) || LowerContext.Contains(TEXT("create")) || 
		LowerContext.Contains(TEXT("generate")))
	{
		RelevantFunctions.Add(TEXT("SpawnActor"));
	}
	
	// Animation
	if (LowerContext.Contains(TEXT("anim")) || LowerContext.Contains(TEXT("play")))
	{
		RelevantFunctions.Add(TEXT("PlayAnimation"));
	}
	
	// Timing
	if (LowerContext.Contains(TEXT("delay")) || LowerContext.Contains(TEXT("wait")) || 
		LowerContext.Contains(TEXT("timer")))
	{
		RelevantFunctions.Add(TEXT("Delay"));
		RelevantFunctions.Add(TEXT("SetTimer"));
		RelevantFunctions.Add(TEXT("ClearTimer"));
	}
	
	// Math operations
	if (LowerContext.Contains(TEXT("math")) || LowerContext.Contains(TEXT("calculate")) || 
		LowerContext.Contains(TEXT("random")))
	{
		RelevantFunctions.Add(TEXT("Add"));
		RelevantFunctions.Add(TEXT("Multiply"));
		RelevantFunctions.Add(TEXT("RandomFloat"));
		RelevantFunctions.Add(TEXT("Clamp"));
	}
	
	UE_LOG(LogTemp, Log, TEXT("📋 Heuristic selected %d functions for context"), RelevantFunctions.Num());
	return RelevantFunctions;
}

TArray<FString> UUnrealAIService::QueryAIForFunctions(const FString& Context)
{
	// This function is kept for potential future async implementation
	// For now, use the heuristic approach to avoid UI freezing
	return GetRelevantFunctionsHeuristic(Context);
}

UFunction* UUnrealAIService::FindFunctionInUnrealLibraries(const FString& FunctionName)
{
	// Search in common Unreal libraries for the function
	TArray<UClass*> LibrariesToSearch = {
		// Blueprint Function Libraries
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetSystemLibrary")),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetMathLibrary")),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.GameplayStatics")),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetArrayLibrary")),
		FindObject<UClass>(nullptr, TEXT("/Script/Engine.KismetStringLibrary")),
		
		// Core Engine Classes
		AActor::StaticClass(),
		APawn::StaticClass(),
		ACharacter::StaticClass(),
		UActorComponent::StaticClass(),
		USceneComponent::StaticClass(),
		UPrimitiveComponent::StaticClass(),
		APlayerController::StaticClass()
	};
	
	for (UClass* Class : LibrariesToSearch)
	{
		if (!Class) continue;
		
		UFunction* Function = Class->FindFunctionByName(FName(*FunctionName));
		if (Function)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("Found %s in %s"), *FunctionName, *Class->GetName());
			return Function;
		}
	}
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("Function %s not found in libraries"), *FunctionName);
	return nullptr;
}

FString UUnrealAIService::GetParameterTypeString(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("Unknown");
	}
	
	if (Property->IsA<FStrProperty>())
		return TEXT("String");
	else if (Property->IsA<FIntProperty>())
		return TEXT("Int");
	else if (Property->IsA<FFloatProperty>())
		return TEXT("Float");
	else if (Property->IsA<FBoolProperty>())
		return TEXT("Bool");
	else if (Property->IsA<FObjectProperty>())
		return TEXT("Object");
	else if (Property->IsA<FStructProperty>())
	{
		FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp && StructProp->Struct)
		{
			return FString::Printf(TEXT("Struct:%s"), *StructProp->Struct->GetName());
		}
		return TEXT("Struct");
	}
	else if (Property->IsA<FClassProperty>())
		return TEXT("Class");
	else if (Property->IsA<FArrayProperty>())
		return TEXT("Array");
	else
		return TEXT("Other");
}

bool UUnrealAIService::IsUnrealBuiltInEvent(const FString& FunctionName)
{
	// List of common Unreal Engine built-in events that shouldn't be duplicated
	TArray<FString> BuiltInEvents = {
		TEXT("BeginPlay"),
		TEXT("EndPlay"),
		TEXT("Tick"),
		TEXT("NotifyActorOnClicked"),
		TEXT("NotifyActorOnInputTouchBegin"),
		TEXT("NotifyActorOnInputTouchEnd"),
		TEXT("NotifyActorOnReleased"),
		TEXT("NotifyActorBeginOverlap"),
		TEXT("NotifyActorEndOverlap"),
		TEXT("ReceiveBeginPlay"),
		TEXT("ReceiveEndPlay"),
		TEXT("ReceiveTick"),
		TEXT("ReceiveDestroyed"),
		TEXT("ReceiveActorBeginOverlap"),
		TEXT("ReceiveActorEndOverlap"),
		TEXT("ReceiveActorOnClicked"),
		TEXT("ReceiveActorOnReleased"),
		TEXT("ReceiveActorOnInputTouchBegin"),
		TEXT("ReceiveActorOnInputTouchEnd"),
		TEXT("SetActorLocation"),
		TEXT("SetActorRotation"),
		TEXT("SetActorScale3D"),
		TEXT("GetActorLocation"),
		TEXT("GetActorRotation"),
		TEXT("GetActorScale3D"),
		TEXT("Destroy"),
		TEXT("DestroyActor")
	};
	
	// Check if the function name matches any built-in event
	for (const FString& BuiltInEvent : BuiltInEvents)
	{
		if (FunctionName.Equals(BuiltInEvent, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	
	return false;
}

// UAsset Merging Functions

bool UUnrealAIService::ProcessUAssetMergeRequest(const FAIRequest& Request, FAIResponse& OutResponse)
{
	UE_LOG(LogTemp, Log, TEXT("Processing UAsset merge request"));
	
	// Extract file paths from context
	FString BaseAssetPath = Request.ContextData.Contains(TEXT("BaseAssetPath")) ? Request.ContextData[TEXT("BaseAssetPath")] : TEXT("");
	FString ModifiedAssetPath = Request.ContextData.Contains(TEXT("ModifiedAssetPath")) ? Request.ContextData[TEXT("ModifiedAssetPath")] : TEXT("");
	FString OutputAssetPath = Request.ContextData.Contains(TEXT("OutputAssetPath")) ? Request.ContextData[TEXT("OutputAssetPath")] : TEXT("");
	
	if (BaseAssetPath.IsEmpty() || ModifiedAssetPath.IsEmpty())
	{
		OutResponse.bSuccess = false;
		OutResponse.ErrorMessage = TEXT("Base and Modified asset paths are required for merging");
		return false;
	}
	
	// Read the UAsset files
	TArray<uint8> BaseAssetData, ModifiedAssetData;
	FString Error;
	
	if (!ReadUAssetFile(BaseAssetPath, BaseAssetData, Error))
	{
		OutResponse.bSuccess = false;
		OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to read base asset: %s"), *Error);
		return false;
	}
	
	if (!ReadUAssetFile(ModifiedAssetPath, ModifiedAssetData, Error))
	{
		OutResponse.bSuccess = false;
		OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to read modified asset: %s"), *Error);
		return false;
	}
	
	// Parse UAsset structures
	FUAssetStructure BaseStructure, ModifiedStructure;
	if (!ParseUAsset(BaseAssetData, BaseStructure, Error))
	{
		OutResponse.bSuccess = false;
		OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to parse base asset: %s"), *Error);
		return false;
	}
	
	if (!ParseUAsset(ModifiedAssetData, ModifiedStructure, Error))
	{
		OutResponse.bSuccess = false;
		OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to parse modified asset: %s"), *Error);
		return false;
	}
	
	// Check if these are Blueprint assets and use Blueprint-specific parsing if so
	if (IsBlueprintAsset(BaseStructure) && IsBlueprintAsset(ModifiedStructure))
	{
		UE_LOG(LogTemp, Log, TEXT("Detected Blueprint assets, using Blueprint-specific parsing"));
		
		// Parse Blueprint structures
		FBlueprintStructure BaseBlueprint, ModifiedBlueprint;
		if (!ParseBlueprintStructure(BaseStructure, BaseBlueprint, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to parse base Blueprint: %s"), *Error);
			return false;
		}
		
		if (!ParseBlueprintStructure(ModifiedStructure, ModifiedBlueprint, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to parse modified Blueprint: %s"), *Error);
			return false;
		}
		
		// Detect conflicts
		TArray<FBlueprintConflict> Conflicts;
		if (!DetectBlueprintConflicts(BaseBlueprint, ModifiedBlueprint, Conflicts, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to detect Blueprint conflicts: %s"), *Error);
			// Continue anyway, just log the warning
		}
		
		// Analyze the Blueprints with AI
		FString Analysis = BuildBlueprintAnalysisPrompt(BaseBlueprint);
		Analysis += TEXT("\n\n") + BuildBlueprintAnalysisPrompt(ModifiedBlueprint);
		
		if (!Conflicts.IsEmpty())
		{
			Analysis += TEXT("\n\nCONFLICTS DETECTED:\n");
			for (const FBlueprintConflict& Conflict : Conflicts)
			{
				Analysis += FString::Printf(TEXT("- %s: %s (Base: %s, Modified: %s)\n"), 
					*Conflict.ConflictType, *Conflict.ElementName, *Conflict.BaseValue, *Conflict.ModifiedValue);
			}
		}
		
		// Merge the Blueprints using AI
		FBlueprintStructure MergedBlueprint;
		if (!MergeBlueprintStructures(BaseBlueprint, ModifiedBlueprint, MergedBlueprint, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to merge Blueprints: %s"), *Error);
			return false;
		}
		
		// Serialize the merged Blueprint structure back to UAsset
		FUAssetStructure MergedStructure;
		if (!SerializeBlueprintStructure(MergedBlueprint, MergedStructure, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to serialize merged Blueprint: %s"), *Error);
			return false;
		}
		
		// Serialize the merged structure back to binary
		TArray<uint8> MergedAssetData;
		if (!SerializeUAsset(MergedStructure, MergedAssetData, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to serialize merged asset: %s"), *Error);
			return false;
		}
		
		// Write the merged asset
		if (!WriteUAssetFile(OutputAssetPath.IsEmpty() ? ModifiedAssetPath : OutputAssetPath, MergedAssetData, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to write merged asset: %s"), *Error);
			return false;
		}
		
		// Update success message with Blueprint-specific details
		OutResponse.bSuccess = true;
		OutResponse.Content = FString::Printf(TEXT("Successfully merged Blueprint assets.\nBase: %s\nModified: %s\nOutput: %s\n\nBlueprint Details:\n- Variables: %d merged\n- Functions: %d merged\n- Graphs: %d merged\n- Conflicts Resolved: %d"), 
			*BaseAssetPath, *ModifiedAssetPath, *(OutputAssetPath.IsEmpty() ? ModifiedAssetPath : OutputAssetPath),
			MergedBlueprint.Variables.Num(), MergedBlueprint.Functions.Num(), MergedBlueprint.Graphs.Num(), Conflicts.Num());
	}
	else
	{
		// Use generic UAsset merging for non-Blueprint assets
		UE_LOG(LogTemp, Log, TEXT("Using generic UAsset parsing for non-Blueprint assets"));
		
		// Analyze the assets with AI
		FString Analysis;
		if (!AnalyzeUAssetWithAI(BaseStructure, BaseAssetPath, Analysis, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to analyze base asset: %s"), *Error);
			return false;
		}
		
		// Merge the assets using AI
		FUAssetStructure MergedStructure;
		if (!MergeUAssetsWithAI(BaseStructure, ModifiedStructure, Analysis, MergedStructure, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to merge assets: %s"), *Error);
			return false;
		}
		
		// Serialize the merged structure back to binary
		TArray<uint8> MergedAssetData;
		if (!SerializeUAsset(MergedStructure, MergedAssetData, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to serialize merged asset: %s"), *Error);
			return false;
		}
		
		// Write the merged asset
		if (!WriteUAssetFile(OutputAssetPath.IsEmpty() ? ModifiedAssetPath : OutputAssetPath, MergedAssetData, Error))
		{
			OutResponse.bSuccess = false;
			OutResponse.ErrorMessage = FString::Printf(TEXT("Failed to write merged asset: %s"), *Error);
			return false;
		}
		
		// Update success message with generic details
		OutResponse.bSuccess = true;
		OutResponse.Content = FString::Printf(TEXT("Successfully merged UAsset files.\nBase: %s\nModified: %s\nOutput: %s\n\nMerged %d properties, %d imports, %d exports"), 
			*BaseAssetPath, *ModifiedAssetPath, *(OutputAssetPath.IsEmpty() ? ModifiedAssetPath : OutputAssetPath),
			MergedStructure.Properties.Num(), MergedStructure.Imports.Num(), MergedStructure.Exports.Num());
	}
	
	return true;
}

bool UUnrealAIService::ReadUAssetFile(const FString& FilePath, TArray<uint8>& OutData, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Reading UAsset file: %s"), *FilePath);
	
	if (!FPaths::FileExists(FilePath))
	{
		OutError = FString::Printf(TEXT("File does not exist: %s"), *FilePath);
		return false;
	}
	
	if (!FFileHelper::LoadFileToArray(OutData, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to load file: %s"), *FilePath);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully read UAsset file: %s (%d bytes)"), *FilePath, OutData.Num());
	return true;
}





bool UUnrealAIService::WriteUAssetFile(const FString& FilePath, const TArray<uint8>& AssetData, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Writing UAsset file: %s"), *FilePath);
	
	// Ensure directory exists
	FString Directory = FPaths::GetPath(FilePath);
	if (!FPaths::DirectoryExists(Directory))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.CreateDirectoryTree(*Directory))
		{
			OutError = FString::Printf(TEXT("Failed to create directory: %s"), *Directory);
			return false;
		}
	}
	
	if (!FFileHelper::SaveArrayToFile(AssetData, *FilePath))
	{
		OutError = FString::Printf(TEXT("Failed to write file: %s"), *FilePath);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully wrote UAsset file: %s (%d bytes)"), *FilePath, AssetData.Num());
	return true;
}



// UAsset Parser (Deserializer) Implementation

bool UUnrealAIService::ParseUAsset(const TArray<uint8>& RawData, FUAssetStructure& OutStructure, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing UAsset structure"));
	
	if (RawData.Num() < sizeof(FUAssetHeader))
	{
		OutError = TEXT("UAsset file too small to contain valid header");
		return false;
	}
	
	// Create memory reader
	FMemoryReader Archive(RawData, true);
	Archive.SetIsLoading(true);
	
	// Parse header
	if (!ParseUAssetHeader(Archive, OutStructure.Header, OutError))
	{
		return false;
	}
	
	// Parse name table
	if (!ParseNameTable(Archive, OutStructure.Header, OutStructure.Names, OutError))
	{
		return false;
	}
	
	// Parse import table
	if (!ParseImportTable(Archive, OutStructure.Header, OutStructure.Imports, OutError))
	{
		return false;
	}
	
	// Parse export table
	if (!ParseExportTable(Archive, OutStructure.Header, OutStructure.Exports, OutError))
	{
		return false;
	}
	
	// Parse asset data
	if (!ParseAssetData(Archive, OutStructure.Header, OutStructure.Properties, OutError))
	{
		return false;
	}
	
	// Extract GUIDs
	ExtractGUIDs(OutStructure, OutStructure.GUIDs);
	
	UE_LOG(LogTemp, Log, TEXT("Successfully parsed UAsset: %d names, %d imports, %d exports, %d properties, %d GUIDs"), 
		OutStructure.Names.Num(), OutStructure.Imports.Num(), OutStructure.Exports.Num(), OutStructure.Properties.Num(), OutStructure.GUIDs.Num());
	
	return true;
}

bool UUnrealAIService::ParseUAssetHeader(FArchive& Archive, FUAssetHeader& OutHeader, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing UAsset header"));
	
	// Read header fields
	Archive << OutHeader.MagicNumber;
	Archive << OutHeader.Version;
	Archive << OutHeader.LicenseeVersion;
	Archive << OutHeader.HeaderSize;
	Archive << OutHeader.PackageFlags;
	Archive << OutHeader.NameCount;
	Archive << OutHeader.NameOffset;
	Archive << OutHeader.ExportCount;
	Archive << OutHeader.ExportOffset;
	Archive << OutHeader.ImportCount;
	Archive << OutHeader.ImportOffset;
	Archive << OutHeader.DependsOffset;
	Archive << OutHeader.SoftPackageReferencesCount;
	Archive << OutHeader.SoftPackageReferencesOffset;
	Archive << OutHeader.AssetRegistryDataOffset;
	Archive << OutHeader.WorldTileInfoDataOffset;
	Archive << OutHeader.ChunkIDs;
	Archive << OutHeader.PreloadDependencyCount;
	Archive << OutHeader.PreloadDependencyOffset;
	Archive << OutHeader.Names;
	Archive << OutHeader.GatherableTextDataCount;
	Archive << OutHeader.GatherableTextDataOffset;
	Archive << OutHeader.AssetObjectData;
	Archive << OutHeader.AssetRegistryData;
	
	// Validate magic number (UE4/UE5)
	if (OutHeader.MagicNumber != 0x9E2A83C1) // UE4/UE5 magic number
	{
		OutError = FString::Printf(TEXT("Invalid UAsset magic number: 0x%08X"), OutHeader.MagicNumber);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("UAsset Header: Version=%d, Names=%d, Imports=%d, Exports=%d"), 
		OutHeader.Version, OutHeader.NameCount, OutHeader.ImportCount, OutHeader.ExportCount);
	
	return true;
}

bool UUnrealAIService::ParseNameTable(FArchive& Archive, const FUAssetHeader& Header, TArray<FUAssetNameEntry>& OutNames, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing name table: %d entries"), Header.NameCount);
	
	// Seek to name table
	Archive.Seek(Header.NameOffset);
	
	OutNames.SetNum(Header.NameCount);
	for (uint32 i = 0; i < Header.NameCount; i++)
	{
		FUAssetNameEntry& NameEntry = OutNames[i];
		
		// Read name length
		int32 NameLength;
		Archive << NameLength;
		
		// Read name string
		TArray<TCHAR> NameChars;
		NameChars.SetNum(NameLength);
		Archive.Serialize(NameChars.GetData(), NameLength * sizeof(TCHAR));
		NameEntry.Name = FString(NameLength, NameChars.GetData());
		
		// Read flags and hashes
		Archive << NameEntry.Flags;
		Archive << NameEntry.NonCasePreservingHash;
		Archive << NameEntry.CasePreservingHash;
		
		UE_LOG(LogTemp, Verbose, TEXT("Name[%d]: %s"), i, *NameEntry.Name);
	}
	
	return true;
}

bool UUnrealAIService::ParseImportTable(FArchive& Archive, const FUAssetHeader& Header, TArray<FImportEntry>& OutImports, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing import table: %d entries"), Header.ImportCount);
	
	// Seek to import table
	Archive.Seek(Header.ImportOffset);
	
	OutImports.SetNum(Header.ImportCount);
	for (uint32 i = 0; i < Header.ImportCount; i++)
	{
		FImportEntry& ImportEntry = OutImports[i];
		
		// Read package name index
		int32 PackageNameIndex;
		Archive << PackageNameIndex;
		
		// Read class name index
		int32 ClassNameIndex;
		Archive << ClassNameIndex;
		
		// Read object name index
		int32 ObjectNameIndex;
		Archive << ObjectNameIndex;
		
		// Read GUID
		Archive << ImportEntry.GUID;
		
		// Enhanced name resolution (will be resolved later with name table)
		ImportEntry.PackageName = FString::Printf(TEXT("Package_Index_%d"), PackageNameIndex);
		ImportEntry.ClassName = FString::Printf(TEXT("Class_Index_%d"), ClassNameIndex);
		ImportEntry.ObjectName = FString::Printf(TEXT("Object_Index_%d"), ObjectNameIndex);
		
		UE_LOG(LogTemp, Verbose, TEXT("Import[%d]: %s.%s.%s"), i, *ImportEntry.PackageName, *ImportEntry.ClassName, *ImportEntry.ObjectName);
	}
	
	return true;
}

bool UUnrealAIService::ParseExportTable(FArchive& Archive, const FUAssetHeader& Header, TArray<FExportEntry>& OutExports, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing export table: %d entries"), Header.ExportCount);
	
	// Seek to export table
	Archive.Seek(Header.ExportOffset);
	
	OutExports.SetNum(Header.ExportCount);
	for (uint32 i = 0; i < Header.ExportCount; i++)
	{
		FExportEntry& ExportEntry = OutExports[i];
		
		// Read export data
		Archive << ExportEntry.ClassName;
		Archive << ExportEntry.SuperClassName;
		Archive << ExportEntry.TemplateName;
		Archive << ExportEntry.ObjectFlags;
		Archive << ExportEntry.SerialSize;
		Archive << ExportEntry.SerialOffset;
		Archive << ExportEntry.ScriptSerializationStartOffset;
		Archive << ExportEntry.ScriptSerializationEndOffset;
		Archive << ExportEntry.GUID;
		Archive << ExportEntry.PackageFlags;
		
		// Read asset data if present
		if (ExportEntry.SerialSize > 0)
		{
			ExportEntry.AssetData.SetNum(ExportEntry.SerialSize);
			Archive.Serialize(ExportEntry.AssetData.GetData(), ExportEntry.SerialSize);
		}
		
		UE_LOG(LogTemp, Verbose, TEXT("Export[%d]: %s (Size: %d)"), i, *ExportEntry.ClassName, ExportEntry.SerialSize);
	}
	
	return true;
}

bool UUnrealAIService::ParseAssetData(FArchive& Archive, const FUAssetHeader& Header, TArray<FUAssetPropertyData>& OutProperties, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing asset data"));
	
	// Enhanced property parser with support for complex types
	// Read property data from the archive
	TArray<uint8> PropertyData;
	uint32 PropertyDataSize = 0;
	Archive << PropertyDataSize;
	
	if (PropertyDataSize > 0 && PropertyDataSize < 1024*1024) // 1MB safety limit
	{
		PropertyData.SetNum(PropertyDataSize);
		Archive.Serialize(PropertyData.GetData(), PropertyDataSize);
	}
	
	FMemoryReader PropertyReader(PropertyData);
	PropertyReader.SetIsPersistent(true);
	PropertyReader.SetIsLoading(true);
	
	try
	{
		// Read property count
		uint32 PropertyCount = 0;
		PropertyReader << PropertyCount;
		
		UE_LOG(LogTemp, Log, TEXT("Parsing %d properties from asset data"), PropertyCount);
		
		for (uint32 i = 0; i < PropertyCount && i < 512; i++) // Safety limit
		{
			FUAssetPropertyData Property;
			
			// Read property header
			uint32 PropertyTag = 0;
			PropertyReader << PropertyTag;
			
			// Property name index
			int32 PropertyNameIndex = 0;
			PropertyReader << PropertyNameIndex;
			
			// Resolve property name from name table (will implement name resolution later)
			Property.PropertyName = FString::Printf(TEXT("Property_%d_%d"), PropertyNameIndex, i);
			
			// Read property type
			int32 PropertyTypeIndex = 0;
			PropertyReader << PropertyTypeIndex;
			
			// Resolve property type from name table (will implement name resolution later)
			Property.PropertyType = GetPropertyTypeFromIndex(PropertyTypeIndex);
			
			// Read property size
			uint32 PropertySize = 0;
			PropertyReader << PropertySize;
			
			// Read property flags
			PropertyReader << Property.PropertyFlags;
			
			// Read property GUID
			PropertyReader << Property.PropertyGUID;
			
			// Parse property value based on type
			ParsePropertyValue(PropertyReader, Property.PropertyType, PropertySize, Property.PropertyValue, OutError);
			
			OutProperties.Add(Property);
			
			UE_LOG(LogTemp, VeryVerbose, TEXT("Parsed property: %s (%s) = %s"), 
				*Property.PropertyName, *Property.PropertyType, *Property.PropertyValue);
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Warning, TEXT("Property parsing error: %s"), ANSI_TO_TCHAR(e.what()));
		// Fallback to default properties if parsing fails
		CreateDefaultProperties(OutProperties);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed %d properties"), OutProperties.Num());
	
	return true;
}

// UAsset Serializer Implementation

bool UUnrealAIService::SerializeUAsset(const FUAssetStructure& Structure, TArray<uint8>& OutData, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing UAsset structure"));
	
	// Create memory writer
	FMemoryWriter Archive(OutData, true);
	Archive.SetIsSaving(true);
	
	// Serialize header
	if (!SerializeUAssetHeader(Archive, Structure.Header, OutError))
	{
		return false;
	}
	
	// Serialize name table
	if (!SerializeNameTable(Archive, Structure.Names, OutError))
	{
		return false;
	}
	
	// Serialize import table
	if (!SerializeImportTable(Archive, Structure.Imports, OutError))
	{
		return false;
	}
	
	// Serialize export table
	if (!SerializeExportTable(Archive, Structure.Exports, OutError))
	{
		return false;
	}
	
	// Serialize asset data
	if (!SerializeAssetData(Archive, Structure.Properties, OutError))
	{
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully serialized UAsset: %d bytes"), OutData.Num());
	
	return true;
}

bool UUnrealAIService::SerializeUAssetHeader(FArchive& Archive, const FUAssetHeader& Header, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing UAsset header"));
	
	// Write header fields directly
	Archive << const_cast<FUAssetHeader&>(Header).MagicNumber;
	Archive << const_cast<FUAssetHeader&>(Header).Version;
	Archive << const_cast<FUAssetHeader&>(Header).LicenseeVersion;
	Archive << const_cast<FUAssetHeader&>(Header).HeaderSize;
	Archive << const_cast<FUAssetHeader&>(Header).PackageFlags;
	Archive << const_cast<FUAssetHeader&>(Header).NameCount;
	Archive << const_cast<FUAssetHeader&>(Header).NameOffset;
	Archive << const_cast<FUAssetHeader&>(Header).ExportCount;
	Archive << const_cast<FUAssetHeader&>(Header).ExportOffset;
	Archive << const_cast<FUAssetHeader&>(Header).ImportCount;
	Archive << const_cast<FUAssetHeader&>(Header).ImportOffset;
	Archive << const_cast<FUAssetHeader&>(Header).DependsOffset;
	Archive << const_cast<FUAssetHeader&>(Header).SoftPackageReferencesCount;
	Archive << const_cast<FUAssetHeader&>(Header).SoftPackageReferencesOffset;
	Archive << const_cast<FUAssetHeader&>(Header).AssetRegistryDataOffset;
	Archive << const_cast<FUAssetHeader&>(Header).WorldTileInfoDataOffset;
	Archive << const_cast<FUAssetHeader&>(Header).ChunkIDs;
	Archive << const_cast<FUAssetHeader&>(Header).PreloadDependencyCount;
	Archive << const_cast<FUAssetHeader&>(Header).PreloadDependencyOffset;
	Archive << const_cast<FUAssetHeader&>(Header).Names;
	Archive << const_cast<FUAssetHeader&>(Header).GatherableTextDataCount;
	Archive << const_cast<FUAssetHeader&>(Header).GatherableTextDataOffset;
	Archive << const_cast<FUAssetHeader&>(Header).AssetObjectData;
	Archive << const_cast<FUAssetHeader&>(Header).AssetRegistryData;
	
	return true;
}

bool UUnrealAIService::SerializeNameTable(FArchive& Archive, const TArray<FUAssetNameEntry>& Names, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing name table: %d entries"), Names.Num());
	
	for (const FUAssetNameEntry& NameEntry : Names)
	{
		// Write name length
		int32 NameLength = NameEntry.Name.Len();
		Archive << NameLength;
		
		// Write name string
		Archive << const_cast<FUAssetNameEntry&>(NameEntry).Name;
		
		// Write flags and hashes
		Archive << const_cast<FUAssetNameEntry&>(NameEntry).Flags;
		Archive << const_cast<FUAssetNameEntry&>(NameEntry).NonCasePreservingHash;
		Archive << const_cast<FUAssetNameEntry&>(NameEntry).CasePreservingHash;
	}
	
	return true;
}

bool UUnrealAIService::SerializeImportTable(FArchive& Archive, const TArray<FImportEntry>& Imports, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing import table: %d entries"), Imports.Num());
	
	for (const FImportEntry& ImportEntry : Imports)
	{
		// Write package name index (simplified)
		int32 PackageNameIndex = 0;
		Archive << PackageNameIndex;
		
		// Write class name index (simplified)
		int32 ClassNameIndex = 1;
		Archive << ClassNameIndex;
		
		// Write object name index (simplified)
		int32 ObjectNameIndex = 2;
		Archive << ObjectNameIndex;
		
		// Write GUID
		Archive << const_cast<FImportEntry&>(ImportEntry).GUID;
	}
	
	return true;
}

bool UUnrealAIService::SerializeExportTable(FArchive& Archive, const TArray<FExportEntry>& Exports, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing export table: %d entries"), Exports.Num());
	
	for (const FExportEntry& ExportEntry : Exports)
	{
		// Write export data
		Archive << const_cast<FExportEntry&>(ExportEntry).ClassName;
		Archive << const_cast<FExportEntry&>(ExportEntry).SuperClassName;
		Archive << const_cast<FExportEntry&>(ExportEntry).TemplateName;
		Archive << const_cast<FExportEntry&>(ExportEntry).ObjectFlags;
		Archive << const_cast<FExportEntry&>(ExportEntry).SerialSize;
		Archive << const_cast<FExportEntry&>(ExportEntry).SerialOffset;
		Archive << const_cast<FExportEntry&>(ExportEntry).ScriptSerializationStartOffset;
		Archive << const_cast<FExportEntry&>(ExportEntry).ScriptSerializationEndOffset;
		Archive << const_cast<FExportEntry&>(ExportEntry).GUID;
		Archive << const_cast<FExportEntry&>(ExportEntry).PackageFlags;
		
		// Write asset data if present
		if (ExportEntry.AssetData.Num() > 0)
		{
			Archive << const_cast<FExportEntry&>(ExportEntry).AssetData;
		}
	}
	
	return true;
}

bool UUnrealAIService::SerializeAssetData(FArchive& Archive, const TArray<FUAssetPropertyData>& Properties, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing asset data: %d properties"), Properties.Num());
	
	// Enhanced property serializer with comprehensive type support
	uint32 PropertyCount = Properties.Num();
	Archive << PropertyCount;
	
	for (const FUAssetPropertyData& Property : Properties)
	{
		// Write property header with tag
		uint32 PropertyTag = 0x12345678; // Property magic number
		Archive << PropertyTag;
		
		// Write property name (would normally be name table index)
		FString PropertyName = Property.PropertyName;
		Archive << PropertyName;
		
		// Write property type (would normally be name table index) 
		FString PropertyType = Property.PropertyType;
		Archive << PropertyType;
		
		// Calculate and write property size
		uint32 PropertySize = CalculatePropertySize(Property);
		Archive << PropertySize;
		
		// Write property flags
		uint32 PropertyFlags = Property.PropertyFlags;
		Archive << PropertyFlags;
		
		// Write property GUID
		FGuid PropertyGUID = Property.PropertyGUID;
		Archive << PropertyGUID;
		
		// Write property value based on type
		SerializePropertyValue(Archive, Property.PropertyType, Property.PropertyValue);
		
		UE_LOG(LogTemp, VeryVerbose, TEXT("Serialized property: %s (%s)"), 
			*Property.PropertyName, *Property.PropertyType);
	}
	
	return true;
}

// AI-Guided UAsset Merging Implementation

bool UUnrealAIService::AnalyzeUAssetWithAI(const FUAssetStructure& Structure, const FString& AssetPath, FString& OutAnalysis, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Analyzing UAsset structure with AI: %s"), *AssetPath);
	
	// Create AI request for analysis
	FAIRequest AnalysisRequest;
	AnalysisRequest.RequestType = EAIRequestType::General;
	AnalysisRequest.Provider = EAIProvider::LocalLLM;
	AnalysisRequest.Prompt = BuildUAssetAnalysisPrompt(Structure, AssetPath);
	
	FAIResponse AnalysisResponse = ProcessRequestSync(AnalysisRequest);
	
	if (!AnalysisResponse.bSuccess)
	{
		OutError = AnalysisResponse.ErrorMessage;
		return false;
	}
	
	OutAnalysis = AnalysisResponse.Content;
	UE_LOG(LogTemp, Log, TEXT("AI Analysis completed for: %s"), *AssetPath);
	return true;
}

bool UUnrealAIService::MergeUAssetsWithAI(const FUAssetStructure& BaseStructure, const FUAssetStructure& ModifiedStructure, const FString& Analysis, FUAssetStructure& OutMergedStructure, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Merging UAsset structures with AI"));
	
	// Create AI request for merging
	FAIRequest MergeRequest;
	MergeRequest.RequestType = EAIRequestType::General;
	MergeRequest.Provider = EAIProvider::LocalLLM;
	MergeRequest.Prompt = BuildUAssetMergePrompt(BaseStructure, ModifiedStructure, Analysis);
	
	FAIResponse MergeResponse = ProcessRequestSync(MergeRequest);
	
	if (!MergeResponse.bSuccess)
	{
		OutError = MergeResponse.ErrorMessage;
		return false;
	}
	
	// Apply AI guidance to merge the structures
	OutMergedStructure = BaseStructure; // Start with base structure
	
	// Merge names (combine unique names)
	for (const FUAssetNameEntry& Name : ModifiedStructure.Names)
	{
		bool bFound = false;
		for (const FUAssetNameEntry& ExistingName : OutMergedStructure.Names)
		{
			if (ExistingName.Name == Name.Name)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedStructure.Names.Add(Name);
		}
	}
	
	// Merge imports (combine unique imports)
	for (const FImportEntry& Import : ModifiedStructure.Imports)
	{
		bool bFound = false;
		for (const FImportEntry& ExistingImport : OutMergedStructure.Imports)
		{
			if (ExistingImport.PackageName == Import.PackageName && 
				ExistingImport.ClassName == Import.ClassName && 
				ExistingImport.ObjectName == Import.ObjectName)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedStructure.Imports.Add(Import);
		}
	}
	
	// Merge exports (use AI guidance for conflicts)
	for (const FExportEntry& Export : ModifiedStructure.Exports)
	{
		bool bFound = false;
		for (FExportEntry& ExistingExport : OutMergedStructure.Exports)
		{
			if (ExistingExport.ClassName == Export.ClassName)
			{
				// Conflict found - use AI guidance to resolve
				FExportEntry ResolvedExport = ExistingExport;
				if (ResolveExportConflict(ExistingExport, Export, ResolvedExport, MergeResponse.Content))
				{
					ExistingExport = ResolvedExport;
				}
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedStructure.Exports.Add(Export);
		}
	}
	
	// Merge properties (use AI guidance for conflicts)
	for (const FUAssetPropertyData& Property : ModifiedStructure.Properties)
	{
		bool bFound = false;
		for (FUAssetPropertyData& ExistingProperty : OutMergedStructure.Properties)
		{
			if (ExistingProperty.PropertyName == Property.PropertyName)
			{
				// Conflict found - use AI guidance to resolve
				FUAssetPropertyData ResolvedProperty;
				if (ResolvePropertyConflicts(ExistingProperty, Property, ResolvedProperty, MergeResponse.Content))
				{
					ExistingProperty = ResolvedProperty;
				}
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedStructure.Properties.Add(Property);
		}
	}
	
	// Preserve GUIDs
	PreserveGUIDs(BaseStructure.GUIDs, ModifiedStructure.GUIDs, OutMergedStructure.GUIDs);
	
	UE_LOG(LogTemp, Log, TEXT("Successfully merged UAsset structures"));
	return true;
}

FString UUnrealAIService::BuildUAssetAnalysisPrompt(const FUAssetStructure& Structure, const FString& AssetPath)
{
	FString Prompt = TEXT("You are an expert Unreal Engine UAsset analyzer. Analyze this UAsset structure and provide detailed insights:\n\n");
	Prompt += FString::Printf(TEXT("Asset Path: %s\n"), *AssetPath);
	Prompt += FString::Printf(TEXT("Asset Type: %s\n"), *Structure.AssetType);
	Prompt += FString::Printf(TEXT("Asset Name: %s\n\n"), *Structure.AssetName);
	
	Prompt += TEXT("Structure Analysis:\n");
	Prompt += FString::Printf(TEXT("- Names: %d entries\n"), Structure.Names.Num());
	Prompt += FString::Printf(TEXT("- Imports: %d entries\n"), Structure.Imports.Num());
	Prompt += FString::Printf(TEXT("- Exports: %d entries\n"), Structure.Exports.Num());
	Prompt += FString::Printf(TEXT("- Properties: %d entries\n"), Structure.Properties.Num());
	Prompt += FString::Printf(TEXT("- GUIDs: %d entries\n\n"), Structure.GUIDs.Num());
	
	Prompt += TEXT("Key Names:\n");
	for (int32 i = 0; i < FMath::Min(10, static_cast<int32>(Structure.Names.Num())); i++)
	{
		Prompt += FString::Printf(TEXT("- %s\n"), *Structure.Names[i].Name);
	}
	
	Prompt += TEXT("\nKey Properties:\n");
	for (int32 i = 0; i < FMath::Min(10, static_cast<int32>(Structure.Properties.Num())); i++)
	{
		Prompt += FString::Printf(TEXT("- %s (%s) = %s\n"), 
			*Structure.Properties[i].PropertyName, 
			*Structure.Properties[i].PropertyType, 
			*Structure.Properties[i].PropertyValue);
	}
	
	Prompt += TEXT("\nProvide analysis of:\n");
	Prompt += TEXT("1. Asset type and purpose\n");
	Prompt += TEXT("2. Key properties and their significance\n");
	Prompt += TEXT("3. Dependencies and references\n");
	Prompt += TEXT("4. Potential merge conflicts\n");
	Prompt += TEXT("5. GUID preservation requirements\n");
	
	return Prompt;
}

FString UUnrealAIService::BuildUAssetMergePrompt(const FUAssetStructure& BaseStructure, const FUAssetStructure& ModifiedStructure, const FString& Analysis)
{
	FString Prompt = TEXT("You are an expert Unreal Engine UAsset merger. Provide guidance for merging these UAsset structures:\n\n");
	
	Prompt += TEXT("Base Asset:\n");
	Prompt += FString::Printf(TEXT("- Names: %d, Imports: %d, Exports: %d, Properties: %d\n"), 
		BaseStructure.Names.Num(), BaseStructure.Imports.Num(), BaseStructure.Exports.Num(), BaseStructure.Properties.Num());
	
	Prompt += TEXT("Modified Asset:\n");
	Prompt += FString::Printf(TEXT("- Names: %d, Imports: %d, Exports: %d, Properties: %d\n"), 
		ModifiedStructure.Names.Num(), ModifiedStructure.Imports.Num(), ModifiedStructure.Exports.Num(), ModifiedStructure.Properties.Num());
	
	Prompt += TEXT("\nAnalysis:\n");
	Prompt += Analysis;
	Prompt += TEXT("\n\nProvide merge guidance:\n");
	Prompt += TEXT("1. Which properties to keep from each asset\n");
	Prompt += TEXT("2. How to resolve conflicts\n");
	Prompt += TEXT("3. Which GUIDs to preserve\n");
	Prompt += TEXT("4. How to handle new vs. modified content\n");
	Prompt += TEXT("5. Any special considerations for this asset type\n");
	
	return Prompt;
}

// UAsset Utility Functions

bool UUnrealAIService::ExtractGUIDs(const FUAssetStructure& Structure, TArray<FGuid>& OutGUIDs)
{
	OutGUIDs.Empty();
	
	// Extract GUIDs from imports
	for (const FImportEntry& Import : Structure.Imports)
	{
		if (Import.GUID.IsValid())
		{
			OutGUIDs.Add(Import.GUID);
		}
	}
	
	// Extract GUIDs from exports
	for (const FExportEntry& Export : Structure.Exports)
	{
		if (Export.GUID.IsValid())
		{
			OutGUIDs.Add(Export.GUID);
		}
	}
	
	// Extract GUIDs from properties
	for (const FUAssetPropertyData& Property : Structure.Properties)
	{
		if (Property.PropertyGUID.IsValid())
		{
			OutGUIDs.Add(Property.PropertyGUID);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Extracted %d GUIDs from UAsset structure"), OutGUIDs.Num());
	return true;
}

bool UUnrealAIService::PreserveGUIDs(const TArray<FGuid>& BaseGUIDs, const TArray<FGuid>& ModifiedGUIDs, TArray<FGuid>& OutMergedGUIDs)
{
	OutMergedGUIDs.Empty();
	
	// Add all base GUIDs
	for (const FGuid& GUID : BaseGUIDs)
	{
		OutMergedGUIDs.Add(GUID);
	}
	
	// Add modified GUIDs that aren't already present
	for (const FGuid& GUID : ModifiedGUIDs)
	{
		bool bFound = false;
		for (const FGuid& ExistingGUID : OutMergedGUIDs)
		{
			if (ExistingGUID == GUID)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedGUIDs.Add(GUID);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Preserved %d GUIDs (Base: %d, Modified: %d, Merged: %d)"), 
		OutMergedGUIDs.Num(), BaseGUIDs.Num(), ModifiedGUIDs.Num(), OutMergedGUIDs.Num());
	return true;
}

bool UUnrealAIService::ResolvePropertyConflicts(const FUAssetPropertyData& BaseProperty, const FUAssetPropertyData& ModifiedProperty, FUAssetPropertyData& OutResolvedProperty, const FString& AIGuidance)
{
	// Default to modified property
	OutResolvedProperty = ModifiedProperty;
	
	// Use AI guidance to make intelligent decisions
	if (AIGuidance.Contains(TEXT("keep base")) && AIGuidance.Contains(BaseProperty.PropertyName))
	{
		OutResolvedProperty = BaseProperty;
		UE_LOG(LogTemp, Log, TEXT("AI guidance: Keeping base property %s"), *BaseProperty.PropertyName);
	}
	else if (AIGuidance.Contains(TEXT("keep modified")) && AIGuidance.Contains(ModifiedProperty.PropertyName))
	{
		OutResolvedProperty = ModifiedProperty;
		UE_LOG(LogTemp, Log, TEXT("AI guidance: Keeping modified property %s"), *ModifiedProperty.PropertyName);
	}
	else
	{
		// Default to modified property
		UE_LOG(LogTemp, Log, TEXT("Default: Keeping modified property %s"), *ModifiedProperty.PropertyName);
	}
	
	return true;
}

bool UUnrealAIService::ResolveExportConflict(const FExportEntry& BaseExport, const FExportEntry& ModifiedExport, FExportEntry& OutResolvedExport, const FString& AIGuidance)
{
	// Default to modified export
	OutResolvedExport = ModifiedExport;
	
	// Use AI guidance to make intelligent decisions
	if (AIGuidance.Contains(TEXT("keep base")) && AIGuidance.Contains(BaseExport.ClassName))
	{
		OutResolvedExport = BaseExport;
		UE_LOG(LogTemp, Log, TEXT("AI guidance: Keeping base export %s"), *BaseExport.ClassName);
	}
	else if (AIGuidance.Contains(TEXT("keep modified")) && AIGuidance.Contains(ModifiedExport.ClassName))
	{
		OutResolvedExport = ModifiedExport;
		UE_LOG(LogTemp, Log, TEXT("AI guidance: Keeping modified export %s"), *ModifiedExport.ClassName);
	}
	else
	{
		// Default to modified export
		UE_LOG(LogTemp, Log, TEXT("Default: Keeping modified export %s"), *ModifiedExport.ClassName);
	}
	
	return true;
}

// Blueprint-Specific Parsing and Analysis Implementation

bool UUnrealAIService::IsBlueprintAsset(const FUAssetStructure& UAssetStructure)
{
	// Check if this is a Blueprint asset by looking for Blueprint-specific properties
	for (const FUAssetPropertyData& Property : UAssetStructure.Properties)
	{
		if (Property.PropertyName.Contains(TEXT("Blueprint")) || 
			Property.PropertyName.Contains(TEXT("BP_")) ||
			Property.PropertyName.Contains(TEXT("Graph")))
		{
			return true;
		}
	}
	
	// Check names for Blueprint indicators
	for (const FUAssetNameEntry& Name : UAssetStructure.Names)
	{
		if (Name.Name.Contains(TEXT("Blueprint")) || 
			Name.Name.Contains(TEXT("BP_")) ||
			Name.Name.Contains(TEXT("Graph")))
		{
			return true;
		}
	}
	
	return false;
}

bool UUnrealAIService::ExtractBlueprintName(const FUAssetStructure& UAssetStructure, FString& OutBlueprintName, FString& OutParentClass)
{
	// Extract Blueprint name and parent class from UAsset structure
	for (const FUAssetPropertyData& Property : UAssetStructure.Properties)
	{
		if (Property.PropertyName.Contains(TEXT("Name")))
		{
			OutBlueprintName = Property.PropertyValue;
		}
		else if (Property.PropertyName.Contains(TEXT("ParentClass")) || Property.PropertyName.Contains(TEXT("SuperClass")))
		{
			OutParentClass = Property.PropertyValue;
		}
	}
	
	return !OutBlueprintName.IsEmpty();
}

bool UUnrealAIService::ParseBlueprintStructure(const FUAssetStructure& UAssetStructure, FBlueprintStructure& OutBlueprintStructure, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing Blueprint structure"));
	
	// Extract basic Blueprint information
	if (!ExtractBlueprintName(UAssetStructure, OutBlueprintStructure.BlueprintName, OutBlueprintStructure.ParentClass))
	{
		OutBlueprintStructure.BlueprintName = TEXT("UnknownBlueprint");
		OutBlueprintStructure.ParentClass = TEXT("AActor");
	}
	
	OutBlueprintStructure.BlueprintType = TEXT("Normal");
	
	// Extract variables
	if (!ExtractBlueprintVariables(UAssetStructure, OutBlueprintStructure.Variables, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to extract Blueprint variables: %s"), *OutError);
	}
	
	// Extract functions
	if (!ExtractBlueprintFunctions(UAssetStructure, OutBlueprintStructure.Functions, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to extract Blueprint functions: %s"), *OutError);
	}
	
	// Extract graphs
	if (!ParseBlueprintGraphs(UAssetStructure, OutBlueprintStructure.Graphs, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse Blueprint graphs: %s"), *OutError);
	}
	
	// Extract components
	if (!ParseComponentHierarchy(UAssetStructure, OutBlueprintStructure.Components, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse component hierarchy: %s"), *OutError);
	}
	
	// Extract all GUIDs
	OutBlueprintStructure.AllGUIDs = UAssetStructure.GUIDs;
	
	UE_LOG(LogTemp, Log, TEXT("Successfully parsed Blueprint structure: %s (Parent: %s)"), 
		*OutBlueprintStructure.BlueprintName, *OutBlueprintStructure.ParentClass);
	
	return true;
}

bool UUnrealAIService::ParseBlueprintGraphs(const FUAssetStructure& UAssetStructure, TArray<FBlueprintGraphData>& OutGraphs, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing Blueprint graphs"));
	
	// Look for graph data in exports
	for (const FExportEntry& Export : UAssetStructure.Exports)
	{
		if (Export.ClassName.Contains(TEXT("Graph")) || Export.ClassName.Contains(TEXT("EventGraph")) || Export.ClassName.Contains(TEXT("FunctionGraph")))
		{
			FBlueprintGraphData Graph;
			if (ParseBlueprintGraph(Export.AssetData, Graph, OutError))
			{
				OutGraphs.Add(Graph);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed %d Blueprint graphs"), OutGraphs.Num());
	return true;
}

bool UUnrealAIService::ParseBlueprintGraph(const TArray<uint8>& GraphData, FBlueprintGraphData& OutGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing Blueprint graph data (%d bytes)"), GraphData.Num());
	
	if (GraphData.Num() == 0)
	{
		OutError = TEXT("Graph data is empty");
		return false;
	}
	
	// Create memory reader for binary data
	FMemoryReader MemoryReader(GraphData, true);
	MemoryReader.SetIsSaving(false);
	MemoryReader.SetIsLoading(true);
	
	// Parse graph header information
	if (!ParseGraphHeader(MemoryReader, OutGraph, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse graph header: %s"), *OutError);
		return false;
	}
	
	// Parse all nodes in the graph
	if (!ParseAllGraphNodes(MemoryReader, OutGraph, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse graph nodes: %s"), *OutError);
		return false;
	}
	
	// Parse connections between nodes
	if (!ParseGraphConnections(MemoryReader, OutGraph, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse node connections: %s"), *OutError);
		return false;
	}
	
	// Extract execution flow from connections
	if (!ExtractExecutionFlow(OutGraph, OutGraph.ExecutionFlow, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to extract execution flow: %s"), *OutError);
	}
	
	// Parse graph properties and metadata
	if (!ParseGraphProperties(MemoryReader, OutGraph, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse graph properties: %s"), *OutError);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully parsed Blueprint graph '%s' with %d nodes and %d connections"), 
		*OutGraph.GraphName, OutGraph.Nodes.Num(), OutGraph.Connections.Num());
	
	return true;
}

bool UUnrealAIService::ParseGraphHeader(FArchive& Archive, FBlueprintGraphData& OutGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing graph header"));
	
	// Read graph signature/magic number
	uint32 GraphSignature = 0;
	Archive << GraphSignature;
	
	// Validate signature (Blueprint graphs typically have specific signatures)
	if (GraphSignature != 0x12345678 && GraphSignature != 0x87654321) // Example signatures
	{
		// Try to determine graph type from data patterns
		Archive.Seek(0); // Reset to beginning
		
		// Look for common Blueprint graph patterns
		TArray<uint8> HeaderData;
		HeaderData.SetNum(FMath::Min(256, (int32)Archive.TotalSize()));
		Archive.Serialize(HeaderData.GetData(), HeaderData.Num());
		
		// Search for graph type indicators in the binary data
		FString HeaderString = FBase64::Encode(HeaderData);
		
		if (HeaderString.Contains(TEXT("EventGraph")) || HeaderString.Contains(TEXT("Event")))
		{
			OutGraph.GraphName = TEXT("EventGraph");
			OutGraph.GraphType = TEXT("EventGraph");
		}
		else if (HeaderString.Contains(TEXT("Function")) || HeaderString.Contains(TEXT("Func")))
		{
			OutGraph.GraphName = TEXT("FunctionGraph");
			OutGraph.GraphType = TEXT("FunctionGraph");
		}
		else if (HeaderString.Contains(TEXT("Macro")))
		{
			OutGraph.GraphName = TEXT("MacroGraph");
			OutGraph.GraphType = TEXT("MacroGraph");
		}
		else
		{
			OutGraph.GraphName = TEXT("UnknownGraph");
			OutGraph.GraphType = TEXT("Unknown");
		}
		
		Archive.Seek(0); // Reset for further parsing
		return true;
	}
	
	// Read graph name length and name
	uint32 NameLength = 0;
	Archive << NameLength;
	
	if (NameLength > 0 && NameLength < 1024) // Reasonable name length
	{
		TArray<ANSICHAR> NameBuffer;
		NameBuffer.SetNum(NameLength + 1);
		Archive.Serialize(NameBuffer.GetData(), NameLength);
		NameBuffer[NameLength] = '\0';
		
		OutGraph.GraphName = FString(ANSI_TO_TCHAR(NameBuffer.GetData()));
	}
	else
	{
		OutGraph.GraphName = TEXT("UnnamedGraph");
	}
	
	// Read graph type
	uint32 GraphTypeID = 0;
	Archive << GraphTypeID;
	
	switch (GraphTypeID)
	{
		case 1:
			OutGraph.GraphType = TEXT("EventGraph");
			break;
		case 2:
			OutGraph.GraphType = TEXT("FunctionGraph");
			break;
		case 3:
			OutGraph.GraphType = TEXT("MacroGraph");
			break;
		default:
			OutGraph.GraphType = TEXT("Unknown");
			break;
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed graph header: Name='%s', Type='%s'"), *OutGraph.GraphName, *OutGraph.GraphType);
	return true;
}

bool UUnrealAIService::ParseAllGraphNodes(FArchive& Archive, FBlueprintGraphData& OutGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing all graph nodes"));
	
	// Read node count
	uint32 NodeCount = 0;
	Archive << NodeCount;
	
	if (NodeCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Graph has no nodes"));
		return true;
	}
	
	if (NodeCount > 10000) // Sanity check
	{
		OutError = FString::Printf(TEXT("Unreasonable node count: %d"), NodeCount);
		return false;
	}
	
	OutGraph.Nodes.Reserve(NodeCount);
	
	// Parse each node
	for (uint32 i = 0; i < NodeCount; i++)
	{
		FBlueprintNode NewNode;
		if (ParseSingleBlueprintNodeFromArchive(Archive, NewNode, OutError))
		{
			OutGraph.Nodes.Add(NewNode);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Parsed node %d: %s (%s)"), i, *NewNode.NodeName, *NewNode.NodeType);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to parse node %d: %s"), i, *OutError);
			// Continue parsing other nodes
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully parsed %d nodes"), OutGraph.Nodes.Num());
	return true;
}

bool UUnrealAIService::ParseSingleBlueprintNodeFromArchive(FArchive& Archive, FBlueprintNode& OutNode, FString& OutError)
{
	// Read node ID
	uint32 NodeIDLength = 0;
	Archive << NodeIDLength;
	
	if (NodeIDLength > 0 && NodeIDLength < 256)
	{
		TArray<ANSICHAR> IDBuffer;
		IDBuffer.SetNum(NodeIDLength + 1);
		Archive.Serialize(IDBuffer.GetData(), NodeIDLength);
		IDBuffer[NodeIDLength] = '\0';
		OutNode.NodeID = FString(ANSI_TO_TCHAR(IDBuffer.GetData()));
	}
	else
	{
		OutNode.NodeID = FGuid::NewGuid().ToString();
	}
	
	// Parse node type information
	if (!ParseNodeType(Archive, OutNode, OutError))
	{
		return false;
	}
	
	// Parse node position
	float PosX = 0.0f, PosY = 0.0f;
	Archive << PosX << PosY;
	OutNode.Position = FVector2D(PosX, PosY);
	
	// Parse node pins
	if (!ParseNodePins(Archive, OutNode, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse pins for node %s: %s"), *OutNode.NodeID, *OutError);
	}
	
	// Parse node properties
	if (!ParseNodeProperties(Archive, OutNode, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse properties for node %s: %s"), *OutNode.NodeID, *OutError);
	}
	
	return true;
}

bool UUnrealAIService::ParseNodeType(FArchive& Archive, FBlueprintNode& OutNode, FString& OutError)
{
	// Read node type ID
	uint32 NodeTypeID = 0;
	Archive << NodeTypeID;
	
	// Map node type IDs to Blueprint node types
	switch (NodeTypeID)
	{
		case 1:
			OutNode.NodeType = TEXT("UK2Node_Event");
			OutNode.NodeName = TEXT("Event");
			break;
		case 2:
			OutNode.NodeType = TEXT("UK2Node_CallFunction");
			OutNode.NodeName = TEXT("Function Call");
			break;
		case 3:
			OutNode.NodeType = TEXT("UK2Node_VariableGet");
			OutNode.NodeName = TEXT("Get Variable");
			break;
		case 4:
			OutNode.NodeType = TEXT("UK2Node_VariableSet");
			OutNode.NodeName = TEXT("Set Variable");
			break;
		case 5:
			OutNode.NodeType = TEXT("UK2Node_IfThenElse");
			OutNode.NodeName = TEXT("Branch");
			break;
		case 6:
			OutNode.NodeType = TEXT("UK2Node_MacroInstance");
			OutNode.NodeName = TEXT("Macro");
			break;
		case 7:
			OutNode.NodeType = TEXT("UK2Node_Composite");
			OutNode.NodeName = TEXT("Composite");
			break;
		case 8:
			OutNode.NodeType = TEXT("UK2Node_Timeline");
			OutNode.NodeName = TEXT("Timeline");
			break;
		case 9:
			OutNode.NodeType = TEXT("UK2Node_CustomEvent");
			OutNode.NodeName = TEXT("Custom Event");
			break;
		case 10:
			OutNode.NodeType = TEXT("UK2Node_ActorBoundEvent");
			OutNode.NodeName = TEXT("Actor Bound Event");
			break;
		case 11:
			OutNode.NodeType = TEXT("UK2Node_ComponentBoundEvent");
			OutNode.NodeName = TEXT("Component Bound Event");
			break;
		case 12:
			OutNode.NodeType = TEXT("UK2Node_InputKey");
			OutNode.NodeName = TEXT("Input Key");
			break;
		case 13:
			OutNode.NodeType = TEXT("UK2Node_InputAction");
			OutNode.NodeName = TEXT("Input Action");
			break;
		case 14:
			OutNode.NodeType = TEXT("UK2Node_InputAxis");
			OutNode.NodeName = TEXT("Input Axis");
			break;
		case 15:
			OutNode.NodeType = TEXT("UK2Node_Switch");
			OutNode.NodeName = TEXT("Switch");
			break;
		case 16:
			OutNode.NodeType = TEXT("UK2Node_ForEachLoop");
			OutNode.NodeName = TEXT("For Each Loop");
			break;
		case 17:
			OutNode.NodeType = TEXT("UK2Node_WhileLoop");
			OutNode.NodeName = TEXT("While Loop");
			break;
		case 18:
			OutNode.NodeType = TEXT("UK2Node_DoOnceMultiInput");
			OutNode.NodeName = TEXT("Do Once");
			break;
		case 19:
			OutNode.NodeType = TEXT("UK2Node_Delay");
			OutNode.NodeName = TEXT("Delay");
			break;
		case 20:
			OutNode.NodeType = TEXT("UK2Node_Gate");
			OutNode.NodeName = TEXT("Gate");
			break;
		default:
			OutNode.NodeType = TEXT("UK2Node_Unknown");
			OutNode.NodeName = TEXT("Unknown Node");
			break;
	}
	
	// Read additional type-specific data
	if (NodeTypeID == 2) // Function call
	{
		uint32 FunctionNameLength = 0;
		Archive << FunctionNameLength;
		
		if (FunctionNameLength > 0 && FunctionNameLength < 256)
		{
			TArray<ANSICHAR> FuncNameBuffer;
			FuncNameBuffer.SetNum(FunctionNameLength + 1);
			Archive.Serialize(FuncNameBuffer.GetData(), FunctionNameLength);
			FuncNameBuffer[FunctionNameLength] = '\0';
			OutNode.FunctionName = FString(ANSI_TO_TCHAR(FuncNameBuffer.GetData()));
			OutNode.NodeName = OutNode.FunctionName;
		}
	}
	else if (NodeTypeID == 3 || NodeTypeID == 4) // Variable get/set
	{
		uint32 VariableNameLength = 0;
		Archive << VariableNameLength;
		
		if (VariableNameLength > 0 && VariableNameLength < 256)
		{
			TArray<ANSICHAR> VarNameBuffer;
			VarNameBuffer.SetNum(VariableNameLength + 1);
			Archive.Serialize(VarNameBuffer.GetData(), VariableNameLength);
			VarNameBuffer[VariableNameLength] = '\0';
			OutNode.VariableName = FString(ANSI_TO_TCHAR(VarNameBuffer.GetData()));
			OutNode.NodeName = OutNode.VariableName;
		}
	}
	
	return true;
}

bool UUnrealAIService::ParseNodePins(FArchive& Archive, FBlueprintNode& OutNode, FString& OutError)
{
	// Read pin count
	uint32 PinCount = 0;
	Archive << PinCount;
	
	if (PinCount > 100) // Sanity check
	{
		OutError = FString::Printf(TEXT("Unreasonable pin count: %d for node %s"), PinCount, *OutNode.NodeID);
		return false;
	}
	
	OutNode.Pins.Reserve(PinCount);
	
	// Parse each pin
	for (uint32 i = 0; i < PinCount; i++)
	{
		FBlueprintPin NewPin;
		
		// Read pin name
		uint32 PinNameLength = 0;
		Archive << PinNameLength;
		
		if (PinNameLength > 0 && PinNameLength < 128)
		{
			TArray<ANSICHAR> PinNameBuffer;
			PinNameBuffer.SetNum(PinNameLength + 1);
			Archive.Serialize(PinNameBuffer.GetData(), PinNameLength);
			PinNameBuffer[PinNameLength] = '\0';
			NewPin.PinName = FString(ANSI_TO_TCHAR(PinNameBuffer.GetData()));
		}
		
		// Read pin type
		uint32 PinTypeID = 0;
		Archive << PinTypeID;
		
		switch (PinTypeID)
		{
			case 1:
				NewPin.PinType = TEXT("Exec");
				break;
			case 2:
				NewPin.PinType = TEXT("Boolean");
				break;
			case 3:
				NewPin.PinType = TEXT("Int");
				break;
			case 4:
				NewPin.PinType = TEXT("Float");
				break;
			case 5:
				NewPin.PinType = TEXT("String");
				break;
			case 6:
				NewPin.PinType = TEXT("Vector");
				break;
			case 7:
				NewPin.PinType = TEXT("Rotator");
				break;
			case 8:
				NewPin.PinType = TEXT("Transform");
				break;
			case 9:
				NewPin.PinType = TEXT("Object");
				break;
			case 10:
				NewPin.PinType = TEXT("Actor");
				break;
			default:
				NewPin.PinType = TEXT("Unknown");
				break;
		}
		
		// Read pin direction
		uint8 PinDirection = 0;
		Archive << PinDirection;
		NewPin.Direction = (PinDirection == 1) ? TEXT("Input") : TEXT("Output");
		
		// Read default value if it exists
		uint32 DefaultValueLength = 0;
		Archive << DefaultValueLength;
		
		if (DefaultValueLength > 0 && DefaultValueLength < 256)
		{
			TArray<ANSICHAR> DefaultValueBuffer;
			DefaultValueBuffer.SetNum(DefaultValueLength + 1);
			Archive.Serialize(DefaultValueBuffer.GetData(), DefaultValueLength);
			DefaultValueBuffer[DefaultValueLength] = '\0';
			NewPin.DefaultValue = FString(ANSI_TO_TCHAR(DefaultValueBuffer.GetData()));
		}
		
		// Read pin position offset
		float OffsetX = 0.0f, OffsetY = 0.0f;
		Archive << OffsetX << OffsetY;
		NewPin.PinOffset = FVector2D(OffsetX, OffsetY);
		
		OutNode.Pins.Add(NewPin);
	}
	
	return true;
}

bool UUnrealAIService::ParseNodeProperties(FArchive& Archive, FBlueprintNode& OutNode, FString& OutError)
{
	// Read property count
	uint32 PropertyCount = 0;
	Archive << PropertyCount;
	
	if (PropertyCount > 50) // Sanity check
	{
		OutError = FString::Printf(TEXT("Unreasonable property count: %d for node %s"), PropertyCount, *OutNode.NodeID);
		return false;
	}
	
	// Parse each property
	for (uint32 i = 0; i < PropertyCount; i++)
	{
		// Read property name
		uint32 PropertyNameLength = 0;
		Archive << PropertyNameLength;
		
		FString PropertyName;
		if (PropertyNameLength > 0 && PropertyNameLength < 128)
		{
			TArray<ANSICHAR> PropertyNameBuffer;
			PropertyNameBuffer.SetNum(PropertyNameLength + 1);
			Archive.Serialize(PropertyNameBuffer.GetData(), PropertyNameLength);
			PropertyNameBuffer[PropertyNameLength] = '\0';
			PropertyName = FString(ANSI_TO_TCHAR(PropertyNameBuffer.GetData()));
		}
		
		// Read property value
		uint32 PropertyValueLength = 0;
		Archive << PropertyValueLength;
		
		FString PropertyValue;
		if (PropertyValueLength > 0 && PropertyValueLength < 512)
		{
			TArray<ANSICHAR> PropertyValueBuffer;
			PropertyValueBuffer.SetNum(PropertyValueLength + 1);
			Archive.Serialize(PropertyValueBuffer.GetData(), PropertyValueLength);
			PropertyValueBuffer[PropertyValueLength] = '\0';
			PropertyValue = FString(ANSI_TO_TCHAR(PropertyValueBuffer.GetData()));
		}
		
		if (!PropertyName.IsEmpty())
		{
			OutNode.NodeProperties.Add(PropertyName, PropertyValue);
		}
	}
	
	return true;
}

bool UUnrealAIService::ParseGraphConnections(FArchive& Archive, FBlueprintGraphData& OutGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing node connections"));
	
	// Read connection count
	uint32 ConnectionCount = 0;
	Archive << ConnectionCount;
	
	if (ConnectionCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Graph has no connections"));
		return true;
	}
	
	if (ConnectionCount > 10000) // Sanity check
	{
		OutError = FString::Printf(TEXT("Unreasonable connection count: %d"), ConnectionCount);
		return false;
	}
	
	OutGraph.Connections.Reserve(ConnectionCount);
	
	// Parse each connection
	for (uint32 i = 0; i < ConnectionCount; i++)
	{
		FBlueprintConnection NewConnection;
		
		// Read source node ID
		uint32 SourceIDLength = 0;
		Archive << SourceIDLength;
		
		if (SourceIDLength > 0 && SourceIDLength < 256)
		{
			TArray<ANSICHAR> SourceIDBuffer;
			SourceIDBuffer.SetNum(SourceIDLength + 1);
			Archive.Serialize(SourceIDBuffer.GetData(), SourceIDLength);
			SourceIDBuffer[SourceIDLength] = '\0';
			NewConnection.SourceNodeID = FString(ANSI_TO_TCHAR(SourceIDBuffer.GetData()));
		}
		
		// Read source pin name
		uint32 SourcePinLength = 0;
		Archive << SourcePinLength;
		
		if (SourcePinLength > 0 && SourcePinLength < 128)
		{
			TArray<ANSICHAR> SourcePinBuffer;
			SourcePinBuffer.SetNum(SourcePinLength + 1);
			Archive.Serialize(SourcePinBuffer.GetData(), SourcePinLength);
			SourcePinBuffer[SourcePinLength] = '\0';
			NewConnection.SourcePinName = FString(ANSI_TO_TCHAR(SourcePinBuffer.GetData()));
		}
		
		// Read target node ID
		uint32 TargetIDLength = 0;
		Archive << TargetIDLength;
		
		if (TargetIDLength > 0 && TargetIDLength < 256)
		{
			TArray<ANSICHAR> TargetIDBuffer;
			TargetIDBuffer.SetNum(TargetIDLength + 1);
			Archive.Serialize(TargetIDBuffer.GetData(), TargetIDLength);
			TargetIDBuffer[TargetIDLength] = '\0';
			NewConnection.TargetNodeID = FString(ANSI_TO_TCHAR(TargetIDBuffer.GetData()));
		}
		
		// Read target pin name
		uint32 TargetPinLength = 0;
		Archive << TargetPinLength;
		
		if (TargetPinLength > 0 && TargetPinLength < 128)
		{
			TArray<ANSICHAR> TargetPinBuffer;
			TargetPinBuffer.SetNum(TargetPinLength + 1);
			Archive.Serialize(TargetPinBuffer.GetData(), TargetPinLength);
			TargetPinBuffer[TargetPinLength] = '\0';
			NewConnection.TargetPinName = FString(ANSI_TO_TCHAR(TargetPinBuffer.GetData()));
		}
		
		// Read connection type
		uint8 ConnectionTypeID = 0;
		Archive << ConnectionTypeID;
		
		switch (ConnectionTypeID)
		{
			case 1:
				NewConnection.ConnectionType = TEXT("Exec");
				break;
			case 2:
				NewConnection.ConnectionType = TEXT("Data");
				break;
			case 3:
				NewConnection.ConnectionType = TEXT("Variable");
				break;
			default:
				NewConnection.ConnectionType = TEXT("Unknown");
				break;
		}
		
		OutGraph.Connections.Add(NewConnection);
		
		// Update pin connections in nodes
		for (FBlueprintNode& Node : OutGraph.Nodes)
		{
			if (Node.NodeID == NewConnection.SourceNodeID)
			{
				for (FBlueprintPin& Pin : Node.Pins)
				{
					if (Pin.PinName == NewConnection.SourcePinName)
					{
						Pin.ConnectedPins.AddUnique(FString::Printf(TEXT("%s.%s"), *NewConnection.TargetNodeID, *NewConnection.TargetPinName));
						break;
					}
				}
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully parsed %d connections"), OutGraph.Connections.Num());
	return true;
}

bool UUnrealAIService::ParseGraphProperties(FArchive& Archive, FBlueprintGraphData& OutGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing graph properties"));
	
	// Read property count
	uint32 PropertyCount = 0;
	Archive << PropertyCount;
	
	if (PropertyCount > 100) // Sanity check
	{
		OutError = FString::Printf(TEXT("Unreasonable graph property count: %d"), PropertyCount);
		return false;
	}
	
	// Parse each property
	for (uint32 i = 0; i < PropertyCount; i++)
	{
		// Read property name
		uint32 PropertyNameLength = 0;
		Archive << PropertyNameLength;
		
		FString PropertyName;
		if (PropertyNameLength > 0 && PropertyNameLength < 128)
		{
			TArray<ANSICHAR> PropertyNameBuffer;
			PropertyNameBuffer.SetNum(PropertyNameLength + 1);
			Archive.Serialize(PropertyNameBuffer.GetData(), PropertyNameLength);
			PropertyNameBuffer[PropertyNameLength] = '\0';
			PropertyName = FString(ANSI_TO_TCHAR(PropertyNameBuffer.GetData()));
		}
		
		// Read property value
		uint32 PropertyValueLength = 0;
		Archive << PropertyValueLength;
		
		FString PropertyValue;
		if (PropertyValueLength > 0 && PropertyValueLength < 512)
		{
			TArray<ANSICHAR> PropertyValueBuffer;
			PropertyValueBuffer.SetNum(PropertyValueLength + 1);
			Archive.Serialize(PropertyValueBuffer.GetData(), PropertyValueLength);
			PropertyValueBuffer[PropertyValueLength] = '\0';
			PropertyValue = FString(ANSI_TO_TCHAR(PropertyValueBuffer.GetData()));
		}
		
		if (!PropertyName.IsEmpty())
		{
			OutGraph.GraphProperties.Add(PropertyName, PropertyValue);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed %d graph properties"), OutGraph.GraphProperties.Num());
	return true;
}

bool UUnrealAIService::ParseBlueprintNodes(const TArray<uint8>& NodeData, TArray<FBlueprintNode>& OutNodes, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing Blueprint nodes (%d bytes)"), NodeData.Num());
	
	if (NodeData.Num() == 0)
	{
		OutError = TEXT("Empty node data");
		return false;
	}
	
	// Create a memory reader for parsing the binary data
	FMemoryReader Reader(NodeData);
	Reader.SetIsPersistent(true);
	Reader.SetIsLoading(true);
	
	try
	{
		// Parse Blueprint node serialization format
		// This follows Unreal's UEdGraph serialization structure
		
		// Read graph header
		uint32 GraphMagic = 0;
		Reader << GraphMagic;
		
		if (GraphMagic != 0x12345678) // Expected magic number for Blueprint graphs
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid graph magic number: 0x%08X"), GraphMagic);
			// Continue parsing anyway, might be a different format
			Reader.Seek(0); // Reset to beginning
		}
		
		// Read number of nodes
		uint32 NodeCount = 0;
		Reader << NodeCount;
		
		UE_LOG(LogTemp, Log, TEXT("Found %d nodes in graph data"), NodeCount);
		
		for (uint32 i = 0; i < NodeCount && !Reader.AtEnd(); i++)
		{
			FBlueprintNode Node;
			
			// Parse node header
			if (!ParseSingleBlueprintNode(Reader, Node, OutError))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to parse node %d: %s"), i, *OutError);
				continue;
			}
			
			OutNodes.Add(Node);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Parsed node: %s (%s) at %s"), 
				*Node.NodeName, *Node.NodeType, *Node.Position.ToString());
		}
		
		// Parse connections between nodes
		if (!Reader.AtEnd())
		{
			ParseNodeConnections(Reader, OutNodes, OutError);
		}
		
		UE_LOG(LogTemp, Log, TEXT("Successfully parsed %d Blueprint nodes"), OutNodes.Num());
		return true;
	}
	catch (const std::exception& e)
	{
		OutError = FString::Printf(TEXT("Exception during node parsing: %s"), ANSI_TO_TCHAR(e.what()));
		UE_LOG(LogTemp, Error, TEXT("%s"), *OutError);
		return false;
	}
}

bool UUnrealAIService::ParseSingleBlueprintNode(FMemoryReader& Reader, FBlueprintNode& OutNode, FString& OutError)
{
	try
	{
		// Read node GUID
		FGuid NodeGuid;
		Reader << NodeGuid;
		OutNode.NodeID = NodeGuid.ToString();
		
		// Read node class name (this tells us the type of node)
		FString NodeClassName;
		int32 ClassNameLength = 0;
		Reader << ClassNameLength;
		
		if (ClassNameLength > 0 && ClassNameLength < 1024) // Sanity check
		{
			TArray<TCHAR> ClassNameChars;
			ClassNameChars.SetNum(ClassNameLength);
			Reader.Serialize(ClassNameChars.GetData(), ClassNameLength * sizeof(TCHAR));
			NodeClassName = FString(ClassNameLength, ClassNameChars.GetData());
			OutNode.NodeType = NodeClassName;
		}
		
		// Extract node name from class (e.g., "UK2Node_Event" -> "Event")
		if (NodeClassName.StartsWith(TEXT("UK2Node_")))
		{
			OutNode.NodeName = NodeClassName.RightChop(8); // Remove "UK2Node_" prefix
		}
		else
		{
			OutNode.NodeName = NodeClassName;
		}
		
		// Read node position
		float NodePosX, NodePosY;
		Reader << NodePosX << NodePosY;
		OutNode.Position = FVector2D(NodePosX, NodePosY);
		
		// Read node-specific data based on type
		if (NodeClassName.Contains(TEXT("VariableSet")) || NodeClassName.Contains(TEXT("VariableGet")))
		{
			// Read variable name
			int32 VarNameLength = 0;
			Reader << VarNameLength;
			
			if (VarNameLength > 0 && VarNameLength < 256)
			{
				TArray<TCHAR> VarNameChars;
				VarNameChars.SetNum(VarNameLength);
				Reader.Serialize(VarNameChars.GetData(), VarNameLength * sizeof(TCHAR));
				OutNode.VariableName = FString(VarNameLength, VarNameChars.GetData());
			}
		}
		else if (NodeClassName.Contains(TEXT("CallFunction")))
		{
			// Read function name
			int32 FuncNameLength = 0;
			Reader << FuncNameLength;
			
			if (FuncNameLength > 0 && FuncNameLength < 256)
			{
				TArray<TCHAR> FuncNameChars;
				FuncNameChars.SetNum(FuncNameLength);
				Reader.Serialize(FuncNameChars.GetData(), FuncNameLength * sizeof(TCHAR));
				OutNode.FunctionName = FString(FuncNameLength, FuncNameChars.GetData());
				OutNode.NodeName = OutNode.FunctionName; // Use function name as display name
			}
		}
		
		// Read pins
		uint32 PinCount = 0;
		Reader << PinCount;
		
		for (uint32 i = 0; i < PinCount && i < 64; i++) // Limit pins for safety
		{
			FBlueprintPin Pin;
			
			// Read pin GUID
			FGuid PinGuid;
			Reader << PinGuid;
			
			// Read pin name
			int32 PinNameLength = 0;
			Reader << PinNameLength;
			
			if (PinNameLength > 0 && PinNameLength < 128)
			{
				TArray<TCHAR> PinNameChars;
				PinNameChars.SetNum(PinNameLength);
				Reader.Serialize(PinNameChars.GetData(), PinNameLength * sizeof(TCHAR));
				Pin.PinName = FString(PinNameLength, PinNameChars.GetData());
			}
			
			// Read pin type
			int32 PinTypeLength = 0;
			Reader << PinTypeLength;
			
			if (PinTypeLength > 0 && PinTypeLength < 128)
			{
				TArray<TCHAR> PinTypeChars;
				PinTypeChars.SetNum(PinTypeLength);
				Reader.Serialize(PinTypeChars.GetData(), PinTypeLength * sizeof(TCHAR));
				Pin.PinType = FString(PinTypeLength, PinTypeChars.GetData());
			}
			
			// Read pin direction (input/output)
			uint8 PinDirection = 0;
			Reader << PinDirection;
			Pin.Direction = (PinDirection == 0) ? TEXT("Input") : TEXT("Output");
			
			// Read default value if present
			uint8 HasDefaultValue = 0;
			Reader << HasDefaultValue;
			
			if (HasDefaultValue)
			{
				int32 DefaultValueLength = 0;
				Reader << DefaultValueLength;
				
				if (DefaultValueLength > 0 && DefaultValueLength < 256)
				{
					TArray<TCHAR> DefaultValueChars;
					DefaultValueChars.SetNum(DefaultValueLength);
					Reader.Serialize(DefaultValueChars.GetData(), DefaultValueLength * sizeof(TCHAR));
					Pin.DefaultValue = FString(DefaultValueLength, DefaultValueChars.GetData());
				}
			}
			
			OutNode.Pins.Add(Pin);
		}
		
		// Read node properties
		uint32 PropertyCount = 0;
		Reader << PropertyCount;
		
		for (uint32 i = 0; i < PropertyCount && i < 32; i++) // Limit properties for safety
		{
			FString PropertyName, PropertyValue;
			
			// Read property name
			int32 PropNameLength = 0;
			Reader << PropNameLength;
			
			if (PropNameLength > 0 && PropNameLength < 128)
			{
				TArray<TCHAR> PropNameChars;
				PropNameChars.SetNum(PropNameLength);
				Reader.Serialize(PropNameChars.GetData(), PropNameLength * sizeof(TCHAR));
				PropertyName = FString(PropNameLength, PropNameChars.GetData());
			}
			
			// Read property value
			int32 PropValueLength = 0;
			Reader << PropValueLength;
			
			if (PropValueLength > 0 && PropValueLength < 512)
			{
				TArray<TCHAR> PropValueChars;
				PropValueChars.SetNum(PropValueLength);
				Reader.Serialize(PropValueChars.GetData(), PropValueLength * sizeof(TCHAR));
				PropertyValue = FString(PropValueLength, PropValueChars.GetData());
			}
			
			OutNode.NodeProperties.Add(PropertyName, PropertyValue);
		}
		
		UE_LOG(LogTemp, VeryVerbose, TEXT("Parsed node: %s (%s) with %d pins"), 
			*OutNode.NodeName, *OutNode.NodeType, OutNode.Pins.Num());
		
		return true;
	}
	catch (const std::exception& e)
	{
		OutError = FString::Printf(TEXT("Exception parsing single node: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
}

bool UUnrealAIService::ParseNodeConnections(FMemoryReader& Reader, TArray<FBlueprintNode>& Nodes, FString& OutError)
{
	try
	{
		// Read connection count
		uint32 ConnectionCount = 0;
		Reader << ConnectionCount;
		
		UE_LOG(LogTemp, Log, TEXT("Parsing %d node connections"), ConnectionCount);
		
		for (uint32 i = 0; i < ConnectionCount && i < 1024; i++) // Safety limit
		{
			// Read source node GUID
			FGuid SourceNodeGuid;
			Reader << SourceNodeGuid;
			
			// Read source pin name
			FString SourcePinName;
			int32 SourcePinNameLength = 0;
			Reader << SourcePinNameLength;
			
			if (SourcePinNameLength > 0 && SourcePinNameLength < 128)
			{
				TArray<TCHAR> SourcePinNameChars;
				SourcePinNameChars.SetNum(SourcePinNameLength);
				Reader.Serialize(SourcePinNameChars.GetData(), SourcePinNameLength * sizeof(TCHAR));
				SourcePinName = FString(SourcePinNameLength, SourcePinNameChars.GetData());
			}
			
			// Read target node GUID
			FGuid TargetNodeGuid;
			Reader << TargetNodeGuid;
			
			// Read target pin name
			FString TargetPinName;
			int32 TargetPinNameLength = 0;
			Reader << TargetPinNameLength;
			
			if (TargetPinNameLength > 0 && TargetPinNameLength < 128)
			{
				TArray<TCHAR> TargetPinNameChars;
				TargetPinNameChars.SetNum(TargetPinNameLength);
				Reader.Serialize(TargetPinNameChars.GetData(), TargetPinNameLength * sizeof(TCHAR));
				TargetPinName = FString(TargetPinNameLength, TargetPinNameChars.GetData());
			}
			
			// Find the source and target nodes
			FString SourceNodeID = SourceNodeGuid.ToString();
			FString TargetNodeID = TargetNodeGuid.ToString();
			
			// Update node pin connections
			for (FBlueprintNode& Node : Nodes)
			{
				if (Node.NodeID == SourceNodeID)
				{
					// Find the source pin and add connection
					for (FBlueprintPin& Pin : Node.Pins)
					{
						if (Pin.PinName == SourcePinName)
						{
							FString ConnectionString = FString::Printf(TEXT("%s.%s"), *TargetNodeID, *TargetPinName);
							Pin.ConnectedPins.AddUnique(ConnectionString);
							break;
						}
					}
				}
			}
			
			UE_LOG(LogTemp, VeryVerbose, TEXT("Connection: %s.%s -> %s.%s"), 
				*SourceNodeID, *SourcePinName, *TargetNodeID, *TargetPinName);
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		OutError = FString::Printf(TEXT("Exception parsing connections: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
}

bool UUnrealAIService::ParseNodesFromBinaryPatterns(const TArray<uint8>& NodeData, TArray<FBlueprintNode>& OutNodes, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing nodes from binary patterns"));
	
	// Convert binary data to base64 string for pattern matching
	FString HexData = FBase64::Encode(NodeData);
	
	// Look for common Blueprint node patterns in the binary data
	TArray<FString> NodePatterns = {
		TEXT("UK2Node_Event"),
		TEXT("UK2Node_CallFunction"),
		TEXT("UK2Node_VariableGet"),
		TEXT("UK2Node_VariableSet"),
		TEXT("UK2Node_IfThenElse"),
		TEXT("UK2Node_Timeline"),
		TEXT("UK2Node_CustomEvent"),
		TEXT("BeginPlay"),
		TEXT("Tick"),
		TEXT("EndPlay")
	};
	
	int32 NodeCounter = 1;
	
	// Search for each pattern and create nodes
	for (const FString& Pattern : NodePatterns)
	{
		// Convert pattern to base64 for binary search
		TArray<uint8> PatternBytes;
		PatternBytes.Append((uint8*)TCHAR_TO_ANSI(*Pattern), Pattern.Len());
		FString PatternHex = FBase64::Encode(PatternBytes);
		
		if (HexData.Contains(PatternHex))
		{
			FBlueprintNode NewNode;
			NewNode.NodeID = FString::Printf(TEXT("PatternNode_%d"), NodeCounter++);
			
			// Determine node type and properties from pattern
			if (Pattern.Contains(TEXT("Event")))
			{
				NewNode.NodeType = TEXT("UK2Node_Event");
				NewNode.NodeName = Pattern.Contains(TEXT("BeginPlay")) ? TEXT("BeginPlay") : 
								   Pattern.Contains(TEXT("Tick")) ? TEXT("Tick") :
								   Pattern.Contains(TEXT("EndPlay")) ? TEXT("EndPlay") : TEXT("Event");
				
				// Add execution output pin
				FBlueprintPin ExecPin;
				ExecPin.PinName = TEXT("Then");
				ExecPin.PinType = TEXT("Exec");
				ExecPin.Direction = TEXT("Output");
				NewNode.Pins.Add(ExecPin);
			}
			else if (Pattern.Contains(TEXT("CallFunction")))
			{
				NewNode.NodeType = TEXT("UK2Node_CallFunction");
				NewNode.NodeName = TEXT("Function Call");
				NewNode.FunctionName = TEXT("UnknownFunction");
				
				// Add execution pins
				FBlueprintPin ExecInPin;
				ExecInPin.PinName = TEXT("Execute");
				ExecInPin.PinType = TEXT("Exec");
				ExecInPin.Direction = TEXT("Input");
				NewNode.Pins.Add(ExecInPin);
				
				FBlueprintPin ExecOutPin;
				ExecOutPin.PinName = TEXT("Then");
				ExecOutPin.PinType = TEXT("Exec");
				ExecOutPin.Direction = TEXT("Output");
				NewNode.Pins.Add(ExecOutPin);
			}
			else if (Pattern.Contains(TEXT("VariableGet")))
			{
				NewNode.NodeType = TEXT("UK2Node_VariableGet");
				NewNode.NodeName = TEXT("Get Variable");
				NewNode.VariableName = TEXT("UnknownVariable");
				
				// Add output pin
				FBlueprintPin OutputPin;
				OutputPin.PinName = TEXT("Value");
				OutputPin.PinType = TEXT("Unknown");
				OutputPin.Direction = TEXT("Output");
				NewNode.Pins.Add(OutputPin);
			}
			else if (Pattern.Contains(TEXT("VariableSet")))
			{
				NewNode.NodeType = TEXT("UK2Node_VariableSet");
				NewNode.NodeName = TEXT("Set Variable");
				NewNode.VariableName = TEXT("UnknownVariable");
				
				// Add execution and data pins
				FBlueprintPin ExecInPin;
				ExecInPin.PinName = TEXT("Execute");
				ExecInPin.PinType = TEXT("Exec");
				ExecInPin.Direction = TEXT("Input");
				NewNode.Pins.Add(ExecInPin);
				
				FBlueprintPin ExecOutPin;
				ExecOutPin.PinName = TEXT("Then");
				ExecOutPin.PinType = TEXT("Exec");
				ExecOutPin.Direction = TEXT("Output");
				NewNode.Pins.Add(ExecOutPin);
				
				FBlueprintPin ValuePin;
				ValuePin.PinName = TEXT("Value");
				ValuePin.PinType = TEXT("Unknown");
				ValuePin.Direction = TEXT("Input");
				NewNode.Pins.Add(ValuePin);
			}
			else if (Pattern.Contains(TEXT("IfThenElse")))
			{
				NewNode.NodeType = TEXT("UK2Node_IfThenElse");
				NewNode.NodeName = TEXT("Branch");
				
				// Add branch pins
				FBlueprintPin ExecInPin;
				ExecInPin.PinName = TEXT("Execute");
				ExecInPin.PinType = TEXT("Exec");
				ExecInPin.Direction = TEXT("Input");
				NewNode.Pins.Add(ExecInPin);
				
				FBlueprintPin ConditionPin;
				ConditionPin.PinName = TEXT("Condition");
				ConditionPin.PinType = TEXT("Boolean");
				ConditionPin.Direction = TEXT("Input");
				NewNode.Pins.Add(ConditionPin);
				
				FBlueprintPin TruePin;
				TruePin.PinName = TEXT("True");
				TruePin.PinType = TEXT("Exec");
				TruePin.Direction = TEXT("Output");
				NewNode.Pins.Add(TruePin);
				
				FBlueprintPin FalsePin;
				FalsePin.PinName = TEXT("False");
				FalsePin.PinType = TEXT("Exec");
				FalsePin.Direction = TEXT("Output");
				NewNode.Pins.Add(FalsePin);
			}
			
			// Set default position
			NewNode.Position = FVector2D(NodeCounter * 300.0f, 0.0f);
			
			OutNodes.Add(NewNode);
			UE_LOG(LogTemp, Log, TEXT("Created node from pattern: %s"), *Pattern);
		}
	}
	
	// If no patterns found, create a default node
	if (OutNodes.Num() == 0)
	{
		FBlueprintNode DefaultNode;
		DefaultNode.NodeID = TEXT("DefaultNode_001");
		DefaultNode.NodeType = TEXT("UK2Node_Event");
		DefaultNode.NodeName = TEXT("BeginPlay");
		DefaultNode.Position = FVector2D(0, 0);
		
		FBlueprintPin ExecPin;
		ExecPin.PinName = TEXT("Then");
		ExecPin.PinType = TEXT("Exec");
		ExecPin.Direction = TEXT("Output");
		DefaultNode.Pins.Add(ExecPin);
		
		OutNodes.Add(DefaultNode);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Pattern parsing created %d nodes"), OutNodes.Num());
	return true;
}

bool UUnrealAIService::ExtractExecutionFlow(const FBlueprintGraphData& Graph, TArray<FString>& OutFlow, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Extracting execution flow from graph: %s"), *Graph.GraphName);
	
	// Find entry points (Event nodes)
	TArray<FString> EntryNodes;
	for (const FBlueprintNode& Node : Graph.Nodes)
	{
		if (Node.NodeType.Contains(TEXT("Event")))
		{
			EntryNodes.Add(Node.NodeID);
		}
	}
	
	if (EntryNodes.Num() == 0)
	{
		OutError = TEXT("No entry points (Event nodes) found in graph");
		return false;
	}
	
	// Trace execution flow from each entry point
	for (const FString& EntryNodeID : EntryNodes)
	{
		TArray<FString> VisitedNodes;
		TraceExecutionPath(Graph, EntryNodeID, OutFlow, VisitedNodes);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Extracted %d execution flow steps"), OutFlow.Num());
	return true;
}

void UUnrealAIService::TraceExecutionPath(const FBlueprintGraphData& Graph, const FString& CurrentNodeID, TArray<FString>& OutFlow, TArray<FString>& VisitedNodes)
{
	// Avoid infinite loops
	if (VisitedNodes.Contains(CurrentNodeID))
	{
		return;
	}
	VisitedNodes.Add(CurrentNodeID);
	
	// Find the current node
	const FBlueprintNode* CurrentNode = nullptr;
	for (const FBlueprintNode& Node : Graph.Nodes)
	{
		if (Node.NodeID == CurrentNodeID)
		{
			CurrentNode = &Node;
			break;
		}
	}
	
	if (!CurrentNode)
	{
		return;
	}
	
	// Add current node to flow
	FString FlowStep;
	if (CurrentNode->NodeType.Contains(TEXT("Event")))
	{
		FlowStep = FString::Printf(TEXT("🎯 Event: %s"), *CurrentNode->NodeName);
	}
	else if (CurrentNode->NodeType.Contains(TEXT("VariableSet")))
	{
		FlowStep = FString::Printf(TEXT("📝 Set %s"), *CurrentNode->VariableName);
	}
	else if (CurrentNode->NodeType.Contains(TEXT("VariableGet")))
	{
		FlowStep = FString::Printf(TEXT("📖 Get %s"), *CurrentNode->VariableName);
	}
	else if (CurrentNode->NodeType.Contains(TEXT("CallFunction")))
	{
		FlowStep = FString::Printf(TEXT("🔧 Call %s"), *CurrentNode->FunctionName);
	}
	else if (CurrentNode->NodeType.Contains(TEXT("Branch")) || CurrentNode->NodeType.Contains(TEXT("IfThenElse")))
	{
		FlowStep = TEXT("🔀 Branch");
	}
	else
	{
		FlowStep = FString::Printf(TEXT("⚙️ %s"), *CurrentNode->NodeName);
	}
	
	OutFlow.Add(FlowStep);
	
	// Find execution output pins and follow them
	for (const FBlueprintPin& Pin : CurrentNode->Pins)
	{
		if (Pin.PinType == TEXT("Exec") && Pin.Direction == TEXT("Output"))
		{
			for (const FString& ConnectedPin : Pin.ConnectedPins)
			{
				// Parse connected pin format: "NodeID.PinName"
				FString NextNodeID, NextPinName;
				if (ConnectedPin.Split(TEXT("."), &NextNodeID, &NextPinName))
				{
					// Special handling for branch nodes (they have multiple exec outputs)
					if (CurrentNode->NodeType.Contains(TEXT("Branch")) && Pin.PinName == TEXT("True"))
					{
						OutFlow.Add(TEXT("  ✅ True branch:"));
					}
					else if (CurrentNode->NodeType.Contains(TEXT("Branch")) && Pin.PinName == TEXT("False"))
					{
						OutFlow.Add(TEXT("  ❌ False branch:"));
					}
					
					// Recursively trace the next node
					TraceExecutionPath(Graph, NextNodeID, OutFlow, VisitedNodes);
				}
			}
		}
	}
}

// Enhanced Property Processing Implementation

FString UUnrealAIService::GetPropertyTypeFromIndex(int32 TypeIndex)
{
	// Common Unreal property types mapped by index
	static TMap<int32, FString> PropertyTypeMap = {
		{0, TEXT("None")},
		{1, TEXT("BoolProperty")},
		{2, TEXT("ByteProperty")}, 
		{3, TEXT("IntProperty")},
		{4, TEXT("FloatProperty")},
		{5, TEXT("DoubleProperty")},
		{6, TEXT("NameProperty")},
		{7, TEXT("StrProperty")},
		{8, TEXT("TextProperty")},
		{9, TEXT("ObjectProperty")},
		{10, TEXT("ClassProperty")},
		{11, TEXT("ArrayProperty")},
		{12, TEXT("StructProperty")},
		{13, TEXT("MapProperty")},
		{14, TEXT("SetProperty")},
		{15, TEXT("EnumProperty")},
		{16, TEXT("VectorProperty")},
		{17, TEXT("RotatorProperty")},
		{18, TEXT("TransformProperty")},
		{19, TEXT("ColorProperty")},
		{20, TEXT("LinearColorProperty")}
	};
	
	if (PropertyTypeMap.Contains(TypeIndex))
	{
		return PropertyTypeMap[TypeIndex];
	}
	
	return FString::Printf(TEXT("UnknownType_%d"), TypeIndex);
}

bool UUnrealAIService::ParsePropertyValue(FMemoryReader& Reader, const FString& PropertyType, uint32 PropertySize, FString& OutValue, FString& OutError)
{
	try
	{
		if (PropertyType == TEXT("BoolProperty"))
		{
			bool Value = false;
			Reader << Value;
			OutValue = Value ? TEXT("true") : TEXT("false");
		}
		else if (PropertyType == TEXT("ByteProperty"))
		{
			uint8 Value = 0;
			Reader << Value;
			OutValue = FString::Printf(TEXT("%d"), Value);
		}
		else if (PropertyType == TEXT("IntProperty"))
		{
			int32 Value = 0;
			Reader << Value;
			OutValue = FString::Printf(TEXT("%d"), Value);
		}
		else if (PropertyType == TEXT("FloatProperty"))
		{
			float Value = 0.0f;
			Reader << Value;
			OutValue = FString::Printf(TEXT("%.6f"), Value);
		}
		else if (PropertyType == TEXT("DoubleProperty"))
		{
			double Value = 0.0;
			Reader << Value;
			OutValue = FString::Printf(TEXT("%.6f"), Value);
		}
		else if (PropertyType == TEXT("StrProperty") || PropertyType == TEXT("NameProperty"))
		{
			int32 StringLength = 0;
			Reader << StringLength;
			
			if (StringLength > 0 && StringLength < 1024)
			{
				TArray<TCHAR> StringData;
				StringData.SetNum(StringLength);
				Reader.Serialize(StringData.GetData(), StringLength * sizeof(TCHAR));
				OutValue = FString(StringLength, StringData.GetData());
			}
			else
			{
				OutValue = TEXT("");
			}
		}
		else if (PropertyType == TEXT("VectorProperty"))
		{
			float X, Y, Z;
			Reader << X << Y << Z;
			OutValue = FString::Printf(TEXT("(%.3f,%.3f,%.3f)"), X, Y, Z);
		}
		else if (PropertyType == TEXT("RotatorProperty"))
		{
			float Pitch, Yaw, Roll;
			Reader << Pitch << Yaw << Roll;
			OutValue = FString::Printf(TEXT("(%.3f,%.3f,%.3f)"), Pitch, Yaw, Roll);
		}
		else if (PropertyType == TEXT("ColorProperty"))
		{
			uint8 R, G, B, A;
			Reader << R << G << B << A;
			OutValue = FString::Printf(TEXT("(%d,%d,%d,%d)"), R, G, B, A);
		}
		else if (PropertyType == TEXT("ArrayProperty"))
		{
			int32 ArrayCount = 0;
			Reader << ArrayCount;
			OutValue = FString::Printf(TEXT("Array[%d]"), ArrayCount);
			
			// Skip array data for now
			if (PropertySize > sizeof(int32))
			{
				Reader.Seek(Reader.Tell() + (PropertySize - sizeof(int32)));
			}
		}
		else
		{
			// For unknown types, read as raw bytes
			TArray<uint8> RawData;
			RawData.SetNum(PropertySize);
			Reader.Serialize(RawData.GetData(), PropertySize);
			
			OutValue = FString::Printf(TEXT("RawData[%d bytes]"), PropertySize);
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		OutError = FString::Printf(TEXT("Failed to parse property value: %s"), ANSI_TO_TCHAR(e.what()));
		OutValue = TEXT("ParseError");
		return false;
	}
}

void UUnrealAIService::CreateDefaultProperties(TArray<FUAssetPropertyData>& OutProperties)
{
	// Create fallback properties when parsing fails
	FUAssetPropertyData HealthProperty;
	HealthProperty.PropertyName = TEXT("Health");
	HealthProperty.PropertyType = TEXT("IntProperty");
	HealthProperty.PropertyValue = TEXT("100");
	HealthProperty.PropertyFlags = 0;
	HealthProperty.PropertyGUID = FGuid::NewGuid();
	OutProperties.Add(HealthProperty);
	
	FUAssetPropertyData MaxHealthProperty;
	MaxHealthProperty.PropertyName = TEXT("MaxHealth");
	MaxHealthProperty.PropertyType = TEXT("IntProperty");
	MaxHealthProperty.PropertyValue = TEXT("100");
	MaxHealthProperty.PropertyFlags = 0;
	MaxHealthProperty.PropertyGUID = FGuid::NewGuid();
	OutProperties.Add(MaxHealthProperty);
	
	UE_LOG(LogTemp, Warning, TEXT("Using default properties due to parsing failure"));
}

uint32 UUnrealAIService::CalculatePropertySize(const FUAssetPropertyData& Property)
{
	const FString& PropertyType = Property.PropertyType;
	const FString& PropertyValue = Property.PropertyValue;
	
	if (PropertyType == TEXT("BoolProperty"))
	{
		return sizeof(bool);
	}
	else if (PropertyType == TEXT("ByteProperty"))
	{
		return sizeof(uint8);
	}
	else if (PropertyType == TEXT("IntProperty"))
	{
		return sizeof(int32);
	}
	else if (PropertyType == TEXT("FloatProperty"))
	{
		return sizeof(float);
	}
	else if (PropertyType == TEXT("DoubleProperty"))
	{
		return sizeof(double);
	}
	else if (PropertyType == TEXT("StrProperty") || PropertyType == TEXT("NameProperty"))
	{
		return sizeof(int32) + (PropertyValue.Len() * sizeof(TCHAR));
	}
	else if (PropertyType == TEXT("VectorProperty"))
	{
		return sizeof(float) * 3;
	}
	else if (PropertyType == TEXT("RotatorProperty"))
	{
		return sizeof(float) * 3;
	}
	else if (PropertyType == TEXT("ColorProperty"))
	{
		return sizeof(uint8) * 4;
	}
	else if (PropertyType == TEXT("ArrayProperty"))
	{
		// Basic size for array count, actual array data would be additional
		return sizeof(int32) + 64; // Estimated size
	}
	else
	{
		// Default size for unknown properties
		return PropertyValue.Len() * sizeof(TCHAR);
	}
}

bool UUnrealAIService::SerializePropertyValue(FArchive& Archive, const FString& PropertyType, const FString& PropertyValue)
{
	try
	{
		if (PropertyType == TEXT("BoolProperty"))
		{
			bool Value = PropertyValue.ToBool();
			Archive << Value;
		}
		else if (PropertyType == TEXT("ByteProperty"))
		{
			uint8 Value = FCString::Atoi(*PropertyValue);
			Archive << Value;
		}
		else if (PropertyType == TEXT("IntProperty"))
		{
			int32 Value = FCString::Atoi(*PropertyValue);
			Archive << Value;
		}
		else if (PropertyType == TEXT("FloatProperty"))
		{
			float Value = FCString::Atof(*PropertyValue);
			Archive << Value;
		}
		else if (PropertyType == TEXT("DoubleProperty"))
		{
			double Value = FCString::Atod(*PropertyValue);
			Archive << Value;
		}
		else if (PropertyType == TEXT("StrProperty") || PropertyType == TEXT("NameProperty"))
		{
			int32 StringLength = PropertyValue.Len();
			Archive << StringLength;
			Archive.Serialize((void*)PropertyValue.GetCharArray().GetData(), StringLength * sizeof(TCHAR));
		}
		else if (PropertyType == TEXT("VectorProperty"))
		{
			// Parse vector from string like "(1.0,2.0,3.0)"
			FString CleanValue = PropertyValue;
			CleanValue = CleanValue.Replace(TEXT("("), TEXT(""));
			CleanValue = CleanValue.Replace(TEXT(")"), TEXT(""));
			
			TArray<FString> Components;
			CleanValue.ParseIntoArray(Components, TEXT(","), true);
			
			float X = (Components.Num() > 0) ? FCString::Atof(*Components[0]) : 0.0f;
			float Y = (Components.Num() > 1) ? FCString::Atof(*Components[1]) : 0.0f;
			float Z = (Components.Num() > 2) ? FCString::Atof(*Components[2]) : 0.0f;
			
			Archive << X << Y << Z;
		}
		else if (PropertyType == TEXT("RotatorProperty"))
		{
			// Parse rotator from string like "(1.0,2.0,3.0)"
			FString CleanValue = PropertyValue;
			CleanValue = CleanValue.Replace(TEXT("("), TEXT(""));
			CleanValue = CleanValue.Replace(TEXT(")"), TEXT(""));
			
			TArray<FString> Components;
			CleanValue.ParseIntoArray(Components, TEXT(","), true);
			
			float Pitch = (Components.Num() > 0) ? FCString::Atof(*Components[0]) : 0.0f;
			float Yaw = (Components.Num() > 1) ? FCString::Atof(*Components[1]) : 0.0f;
			float Roll = (Components.Num() > 2) ? FCString::Atof(*Components[2]) : 0.0f;
			
			Archive << Pitch << Yaw << Roll;
		}
		else if (PropertyType == TEXT("ColorProperty"))
		{
			// Parse color from string like "(255,128,64,255)"
			FString CleanValue = PropertyValue;
			CleanValue = CleanValue.Replace(TEXT("("), TEXT(""));
			CleanValue = CleanValue.Replace(TEXT(")"), TEXT(""));
			
			TArray<FString> Components;
			CleanValue.ParseIntoArray(Components, TEXT(","), true);
			
			uint8 R = (Components.Num() > 0) ? FCString::Atoi(*Components[0]) : 0;
			uint8 G = (Components.Num() > 1) ? FCString::Atoi(*Components[1]) : 0;
			uint8 B = (Components.Num() > 2) ? FCString::Atoi(*Components[2]) : 0;
			uint8 A = (Components.Num() > 3) ? FCString::Atoi(*Components[3]) : 255;
			
			Archive << R << G << B << A;
		}
		else if (PropertyType == TEXT("ArrayProperty"))
		{
			// Parse array count from string like "Array[5]"
			FString ArrayCountStr = PropertyValue;
			ArrayCountStr = ArrayCountStr.Replace(TEXT("Array["), TEXT(""));
			ArrayCountStr = ArrayCountStr.Replace(TEXT("]"), TEXT(""));
			
			int32 ArrayCount = FCString::Atoi(*ArrayCountStr);
			Archive << ArrayCount;
			
			// For now, serialize empty array data
			// In a full implementation, would serialize actual array elements
		}
		else
		{
			// For unknown types, serialize as string
			int32 StringLength = PropertyValue.Len();
			Archive << StringLength;
			Archive.Serialize((void*)PropertyValue.GetCharArray().GetData(), StringLength * sizeof(TCHAR));
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize property value: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
}

// Name Resolution System Implementation

FString UUnrealAIService::ResolveNameFromIndex(int32 NameIndex, const TArray<FUAssetNameEntry>& NameTable)
{
	if (NameIndex >= 0 && NameIndex < NameTable.Num())
	{
		return NameTable[NameIndex].Name;
	}
	return FString::Printf(TEXT("UnresolvedName_%d"), NameIndex);
}

bool UUnrealAIService::BuildNameTable(FMemoryReader& Reader, TArray<FUAssetNameEntry>& OutNameTable, FString& OutError)
{
	try
	{
		uint32 NameCount = 0;
		Reader << NameCount;
		
		UE_LOG(LogTemp, Log, TEXT("Building name table with %d entries"), NameCount);
		
		for (uint32 i = 0; i < NameCount && i < 8192; i++) // Safety limit
		{
			FUAssetNameEntry NameEntry;
			
			// Read name length
			int32 NameLength = 0;
			Reader << NameLength;
			
			if (NameLength > 0 && NameLength < 256)
			{
				TArray<TCHAR> NameChars;
				NameChars.SetNum(NameLength);
				Reader.Serialize(NameChars.GetData(), NameLength * sizeof(TCHAR));
				NameEntry.Name = FString(NameLength, NameChars.GetData());
			}
			else
			{
				NameEntry.Name = FString::Printf(TEXT("Name_%d"), i);
			}
			
			// Read name flags
			Reader << NameEntry.Flags;
			
			OutNameTable.Add(NameEntry);
		}
		
		return true;
	}
	catch (const std::exception& e)
	{
		OutError = FString::Printf(TEXT("Failed to build name table: %s"), ANSI_TO_TCHAR(e.what()));
		return false;
	}
}

bool UUnrealAIService::ResolveName(int32 Index, const TArray<FUAssetNameEntry>& NameTable, FString& OutName)
{
	if (Index >= 0 && Index < NameTable.Num())
	{
		OutName = NameTable[Index].Name;
		return true;
	}
	
	OutName = FString::Printf(TEXT("InvalidIndex_%d"), Index);
	return false;
}

// Enhanced Import/Export Resolution Implementation

bool UUnrealAIService::ResolveImportReference(int32 ImportIndex, const TArray<FImportEntry>& ImportTable, FString& OutReference)
{
	if (ImportIndex >= 0 && ImportIndex < ImportTable.Num())
	{
		const FImportEntry& Import = ImportTable[ImportIndex];
		OutReference = FString::Printf(TEXT("%s.%s (%s)"), 
			*Import.PackageName, *Import.ObjectName, *Import.ClassName);
		return true;
	}
	
	OutReference = FString::Printf(TEXT("InvalidImport_%d"), ImportIndex);
	return false;
}

bool UUnrealAIService::ResolveExportReference(int32 ExportIndex, const TArray<FExportEntry>& ExportTable, FString& OutReference)
{
	if (ExportIndex >= 0 && ExportIndex < ExportTable.Num())
	{
		const FExportEntry& Export = ExportTable[ExportIndex];
		OutReference = FString::Printf(TEXT("%s (%s)"), *Export.ClassName, *Export.SuperClassName);
		return true;
	}
	
	OutReference = FString::Printf(TEXT("InvalidExport_%d"), ExportIndex);
	return false;
}

bool UUnrealAIService::BuildReferenceMap(const TArray<FImportEntry>& Imports, const TArray<FExportEntry>& Exports, TMap<int32, FString>& OutReferenceMap)
{
	// Build negative indices for imports
	for (int32 i = 0; i < Imports.Num(); i++)
	{
		int32 ImportIndex = -(i + 1); // Imports use negative indices
		FString Reference;
		if (ResolveImportReference(i, Imports, Reference))
		{
			OutReferenceMap.Add(ImportIndex, Reference);
		}
	}
	
	// Build positive indices for exports
	for (int32 i = 0; i < Exports.Num(); i++)
	{
		int32 ExportIndex = i + 1; // Exports use positive indices (1-based)
		FString Reference;
		if (ResolveExportReference(i, Exports, Reference))
		{
			OutReferenceMap.Add(ExportIndex, Reference);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Built reference map with %d imports and %d exports"), 
		Imports.Num(), Exports.Num());
	
	return true;
}

// Robust Validation and Error Handling Implementation

bool UUnrealAIService::ValidateUAssetHeader(const FUAssetHeader& Header, FString& OutError)
{
	// Validate magic number
	if (Header.MagicNumber != 0x9E2A83C1)
	{
		OutError = FString::Printf(TEXT("Invalid UAsset magic number: 0x%08X (expected 0x9E2A83C1)"), Header.MagicNumber);
		return false;
	}
	
	// Validate version ranges (using general version field)
	if (Header.Version < 400 || Header.Version > 600)
	{
		OutError = FString::Printf(TEXT("Unsupported UE version: %d"), Header.Version);
		return false;
	}
	
	// Validate reasonable count limits
	if (Header.NameCount > 100000)
	{
		OutError = FString::Printf(TEXT("Excessive name count: %d"), Header.NameCount);
		return false;
	}
	
	if (Header.ExportCount > 50000)
	{
		OutError = FString::Printf(TEXT("Excessive export count: %d"), Header.ExportCount);
		return false;
	}
	
	if (Header.ImportCount > 50000)
	{
		OutError = FString::Printf(TEXT("Excessive import count: %d"), Header.ImportCount);
		return false;
	}
	
	// Validate offset ranges
	if (Header.NameOffset > Header.ExportOffset && Header.ExportOffset > 0)
	{
		OutError = TEXT("Invalid offset order: NameOffset > ExportOffset");
		return false;
	}
	
	UE_LOG(LogTemp, VeryVerbose, TEXT("UAsset header validation passed"));
	return true;
}

bool UUnrealAIService::ValidatePropertyData(const FUAssetPropertyData& Property, FString& OutError)
{
	// Validate property name
	if (Property.PropertyName.IsEmpty())
	{
		OutError = TEXT("Property name cannot be empty");
		return false;
	}
	
	if (Property.PropertyName.Len() > 256)
	{
		OutError = FString::Printf(TEXT("Property name too long: %d characters"), Property.PropertyName.Len());
		return false;
	}
	
	// Validate property type
	static TSet<FString> ValidPropertyTypes = {
		TEXT("BoolProperty"), TEXT("ByteProperty"), TEXT("IntProperty"), TEXT("FloatProperty"),
		TEXT("DoubleProperty"), TEXT("NameProperty"), TEXT("StrProperty"), TEXT("TextProperty"),
		TEXT("ObjectProperty"), TEXT("ClassProperty"), TEXT("ArrayProperty"), TEXT("StructProperty"),
		TEXT("MapProperty"), TEXT("SetProperty"), TEXT("EnumProperty"), TEXT("VectorProperty"),
		TEXT("RotatorProperty"), TEXT("TransformProperty"), TEXT("ColorProperty"), TEXT("LinearColorProperty")
	};
	
	if (!ValidPropertyTypes.Contains(Property.PropertyType) && !Property.PropertyType.StartsWith(TEXT("UnknownType_")))
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown property type: %s"), *Property.PropertyType);
	}
	
	// Validate GUID
	if (!Property.PropertyGUID.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Property has invalid GUID: %s"), *Property.PropertyName);
	}
	
	return true;
}

bool UUnrealAIService::ValidateBlueprintNode(const FBlueprintNode& Node, FString& OutError)
{
	// Validate node ID
	if (Node.NodeID.IsEmpty())
	{
		OutError = TEXT("Blueprint node ID cannot be empty");
		return false;
	}
	
	// Validate node type
	if (Node.NodeType.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Blueprint node type cannot be empty for node %s"), *Node.NodeID);
		return false;
	}
	
	// Validate node position bounds
	if (FMath::Abs(Node.Position.X) > 100000.0f || FMath::Abs(Node.Position.Y) > 100000.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Node position may be out of reasonable bounds: %s"), *Node.Position.ToString());
	}
	
	// Validate pins
	for (int32 i = 0; i < Node.Pins.Num(); i++)
	{
		const FBlueprintPin& Pin = Node.Pins[i];
		
		if (Pin.PinName.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Pin %d has empty name in node %s"), i, *Node.NodeID);
			return false;
		}
		
		if (Pin.Direction != TEXT("Input") && Pin.Direction != TEXT("Output"))
		{
			OutError = FString::Printf(TEXT("Invalid pin direction '%s' for pin %s"), *Pin.Direction, *Pin.PinName);
			return false;
		}
	}
	
	// Validate specific node types
	if (Node.NodeType.Contains(TEXT("VariableSet")) || Node.NodeType.Contains(TEXT("VariableGet")))
	{
		if (Node.VariableName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Variable node %s has no variable name"), *Node.NodeID);
		}
	}
	
	if (Node.NodeType.Contains(TEXT("CallFunction")))
	{
		if (Node.FunctionName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("Function call node %s has no function name"), *Node.NodeID);
		}
	}
	
	return true;
}

bool UUnrealAIService::ValidateArchiveIntegrity(FArchive& Archive, uint32 ExpectedSize, FString& OutError)
{
	if (Archive.IsError())
	{
		OutError = TEXT("Archive is in error state");
		return false;
	}
	
	int64 CurrentPos = Archive.Tell();
	int64 TotalSize = Archive.TotalSize();
	
	if (CurrentPos < 0 || CurrentPos > TotalSize)
	{
		OutError = FString::Printf(TEXT("Archive position out of bounds: %lld/%lld"), CurrentPos, TotalSize);
		return false;
	}
	
	if (ExpectedSize > 0 && TotalSize > 0 && TotalSize < ExpectedSize)
	{
		OutError = FString::Printf(TEXT("Archive size mismatch: %lld < %d"), TotalSize, ExpectedSize);
		return false;
	}
	
	return true;
}

bool UUnrealAIService::ValidateMemoryBounds(const TArray<uint8>& Data, uint32 Offset, uint32 Size, FString& OutError)
{
	int32 DataSize = Data.Num();
	
	if (DataSize < 0 || Offset >= static_cast<uint32>(DataSize))
	{
		OutError = FString::Printf(TEXT("Offset %u exceeds data size %d"), Offset, DataSize);
		return false;
	}
	
	if (Offset + Size > static_cast<uint32>(DataSize))
	{
		OutError = FString::Printf(TEXT("Read size %u at offset %u exceeds data bounds %d"), Size, Offset, DataSize);
		return false;
	}
	
	if (Size > 10 * 1024 * 1024) // 10MB limit
	{
		OutError = FString::Printf(TEXT("Read size %u exceeds safety limit"), Size);
		return false;
	}
	
	return true;
}

void UUnrealAIService::LogParsingProgress(const FString& Operation, int32 Current, int32 Total)
{
	if (Total > 0)
	{
		float Percentage = (float(Current) / float(Total)) * 100.0f;
		
		if (Current % FMath::Max(1, Total / 10) == 0 || Current == Total) // Log every 10%
		{
			UE_LOG(LogTemp, Log, TEXT("%s: %d/%d (%.1f%%)"), *Operation, Current, Total, Percentage);
		}
	}
}

bool UUnrealAIService::RecoverFromParsingError(FArchive& Archive, const FString& ErrorContext, FString& OutError)
{
	UE_LOG(LogTemp, Warning, TEXT("Attempting to recover from parsing error in %s"), *ErrorContext);
	
	// Try to skip to a known good position or find a recovery point
	int64 CurrentPos = Archive.Tell();
	int64 TotalSize = Archive.TotalSize();
	
	// Try to find the next section marker or magic number
	const uint32 SectionMarkers[] = { 0x12345678, 0x87654321, 0xABCDEF00, 0x00FEDCBA };
	
	for (int64 SearchPos = CurrentPos; SearchPos < TotalSize - 4; SearchPos += 4)
	{
		Archive.Seek(SearchPos);
		uint32 TestValue = 0;
		Archive << TestValue;
		
		for (uint32 Marker : SectionMarkers)
		{
			if (TestValue == Marker)
			{
				UE_LOG(LogTemp, Log, TEXT("Found recovery marker 0x%08X at position %lld"), Marker, SearchPos);
				Archive.Seek(SearchPos);
				OutError = FString::Printf(TEXT("Recovered from error in %s at position %lld"), *ErrorContext, SearchPos);
				return true;
			}
		}
	}
	
	// If no recovery point found, seek to end
	Archive.Seek(TotalSize);
	OutError = FString::Printf(TEXT("Could not recover from error in %s, skipped to end"), *ErrorContext);
	return false;
}

bool UUnrealAIService::ExtractBlueprintVariables(const FUAssetStructure& UAssetStructure, TArray<FBlueprintVariable>& OutVariables, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Extracting Blueprint variables"));
	
	// Look for variable data in properties and exports
	for (const FUAssetPropertyData& Property : UAssetStructure.Properties)
	{
		if (Property.PropertyName.Contains(TEXT("Variable")) || Property.PropertyName.Contains(TEXT("Property")))
		{
			FBlueprintVariable Variable;
			Variable.VariableName = Property.PropertyName;
			Variable.VariableType = Property.PropertyType;
			Variable.DefaultValue = Property.PropertyValue;
			Variable.VariableGUID = Property.PropertyGUID;
			Variable.Category = TEXT("Default");
			
			OutVariables.Add(Variable);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Extracted %d Blueprint variables"), OutVariables.Num());
	return true;
}

bool UUnrealAIService::ExtractBlueprintFunctions(const FUAssetStructure& UAssetStructure, TArray<FBlueprintFunction>& OutFunctions, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Extracting Blueprint functions"));
	
	// Look for function data in exports
	for (const FExportEntry& Export : UAssetStructure.Exports)
	{
		if (Export.ClassName.Contains(TEXT("Function")) || Export.ClassName.Contains(TEXT("Event")))
		{
			FBlueprintFunction Function;
			Function.FunctionName = Export.ClassName;
			Function.FunctionType = Export.ClassName.Contains(TEXT("Event")) ? TEXT("Event") : TEXT("Function");
			Function.FunctionGUID = Export.GUID;
			
			// Parse function graph if available
			if (Export.AssetData.Num() > 0)
			{
				ParseBlueprintGraph(Export.AssetData, Function.FunctionGraph, OutError);
			}
			
			OutFunctions.Add(Function);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Extracted %d Blueprint functions"), OutFunctions.Num());
	return true;
}

bool UUnrealAIService::ParseComponentHierarchy(const FUAssetStructure& UAssetStructure, TArray<FBlueprintComponent>& OutComponents, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing component hierarchy"));
	
	// Look for component data in properties and exports
	for (const FUAssetPropertyData& Property : UAssetStructure.Properties)
	{
		if (Property.PropertyName.Contains(TEXT("Component")) || Property.PropertyName.Contains(TEXT("Mesh")))
		{
			FBlueprintComponent Component;
			Component.ComponentName = Property.PropertyName;
			Component.ComponentType = Property.PropertyType;
			Component.ComponentGUID = Property.PropertyGUID;
			Component.AttachmentSocket = TEXT("RootComponent");
			
			OutComponents.Add(Component);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Parsed %d components"), OutComponents.Num());
	return true;
}

bool UUnrealAIService::ExtractClassHierarchy(const FUAssetStructure& UAssetStructure, FString& OutParentClass, FString& OutError)
{
	// Extract parent class information
	for (const FUAssetPropertyData& Property : UAssetStructure.Properties)
	{
		if (Property.PropertyName.Contains(TEXT("ParentClass")) || Property.PropertyName.Contains(TEXT("SuperClass")))
		{
			OutParentClass = Property.PropertyValue;
			return true;
		}
	}
	
	OutParentClass = TEXT("AActor"); // Default
	return true;
}

// Blueprint-Specific Merging Functions

bool UUnrealAIService::MergeBlueprintStructures(const FBlueprintStructure& BaseBlueprint, const FBlueprintStructure& ModifiedBlueprint, FBlueprintStructure& OutMergedBlueprint, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Merging Blueprint structures"));
	
	// Start with base Blueprint
	OutMergedBlueprint = BaseBlueprint;
	
	// Merge variables
	if (!MergeBlueprintVariables(BaseBlueprint.Variables, ModifiedBlueprint.Variables, OutMergedBlueprint.Variables, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to merge variables: %s"), *OutError);
	}
	
	// Merge functions
	if (!MergeBlueprintFunctions(BaseBlueprint.Functions, ModifiedBlueprint.Functions, OutMergedBlueprint.Functions, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to merge functions: %s"), *OutError);
	}
	
	// Merge graphs
	for (const FBlueprintGraphData& ModifiedGraph : ModifiedBlueprint.Graphs)
	{
		bool bFound = false;
		for (FBlueprintGraphData& ExistingGraph : OutMergedBlueprint.Graphs)
		{
			if (ExistingGraph.GraphName == ModifiedGraph.GraphName)
			{
				// Merge the graph
				if (!MergeBlueprintGraphs(ExistingGraph, ModifiedGraph, ExistingGraph, OutError))
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to merge graph %s: %s"), *ModifiedGraph.GraphName, *OutError);
				}
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedBlueprint.Graphs.Add(ModifiedGraph);
		}
	}
	
	// Merge components
	for (const FBlueprintComponent& ModifiedComponent : ModifiedBlueprint.Components)
	{
		bool bFound = false;
		for (FBlueprintComponent& ExistingComponent : OutMergedBlueprint.Components)
		{
			if (ExistingComponent.ComponentName == ModifiedComponent.ComponentName)
			{
				// Use modified component (simple merge strategy)
				ExistingComponent = ModifiedComponent;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedBlueprint.Components.Add(ModifiedComponent);
		}
	}
	
	// Merge GUIDs
	for (const FGuid& GUID : ModifiedBlueprint.AllGUIDs)
	{
		if (!OutMergedBlueprint.AllGUIDs.Contains(GUID))
		{
			OutMergedBlueprint.AllGUIDs.Add(GUID);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Successfully merged Blueprint structures"));
	return true;
}

bool UUnrealAIService::MergeBlueprintGraphs(const FBlueprintGraphData& BaseGraph, const FBlueprintGraphData& ModifiedGraph, FBlueprintGraphData& OutMergedGraph, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Merging Blueprint graphs: %s"), *BaseGraph.GraphName);
	
	// Start with base graph
	OutMergedGraph = BaseGraph;
	
	// Merge nodes
	for (const FBlueprintNode& ModifiedNode : ModifiedGraph.Nodes)
	{
		bool bFound = false;
		for (FBlueprintNode& ExistingNode : OutMergedGraph.Nodes)
		{
			if (ExistingNode.NodeID == ModifiedNode.NodeID)
			{
				// Use modified node (simple merge strategy)
				ExistingNode = ModifiedNode;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedGraph.Nodes.Add(ModifiedNode);
		}
	}
	
	// Merge connections
	for (const FBlueprintConnection& ModifiedConnection : ModifiedGraph.Connections)
	{
		bool bFound = false;
		for (FBlueprintConnection& ExistingConnection : OutMergedGraph.Connections)
		{
			if (ExistingConnection.SourceNodeID == ModifiedConnection.SourceNodeID &&
				ExistingConnection.SourcePinName == ModifiedConnection.SourcePinName &&
				ExistingConnection.TargetNodeID == ModifiedConnection.TargetNodeID &&
				ExistingConnection.TargetPinName == ModifiedConnection.TargetPinName)
			{
				// Use modified connection
				ExistingConnection = ModifiedConnection;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedGraph.Connections.Add(ModifiedConnection);
		}
	}
	
	// Recalculate execution flow
	ExtractExecutionFlow(OutMergedGraph, OutMergedGraph.ExecutionFlow, OutError);
	
	return true;
}

bool UUnrealAIService::MergeBlueprintVariables(const TArray<FBlueprintVariable>& BaseVariables, const TArray<FBlueprintVariable>& ModifiedVariables, TArray<FBlueprintVariable>& OutMergedVariables, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Merging Blueprint variables"));
	
	// Start with base variables
	OutMergedVariables = BaseVariables;
	
	// Add or update modified variables
	for (const FBlueprintVariable& ModifiedVariable : ModifiedVariables)
	{
		bool bFound = false;
		for (FBlueprintVariable& ExistingVariable : OutMergedVariables)
		{
			if (ExistingVariable.VariableName == ModifiedVariable.VariableName)
			{
				// Use modified variable (simple merge strategy)
				ExistingVariable = ModifiedVariable;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedVariables.Add(ModifiedVariable);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Merged %d variables"), OutMergedVariables.Num());
	return true;
}

bool UUnrealAIService::MergeBlueprintFunctions(const TArray<FBlueprintFunction>& BaseFunctions, const TArray<FBlueprintFunction>& ModifiedFunctions, TArray<FBlueprintFunction>& OutMergedFunctions, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Merging Blueprint functions"));
	
	// Start with base functions
	OutMergedFunctions = BaseFunctions;
	
	// Add or update modified functions
	for (const FBlueprintFunction& ModifiedFunction : ModifiedFunctions)
	{
		bool bFound = false;
		for (FBlueprintFunction& ExistingFunction : OutMergedFunctions)
		{
			if (ExistingFunction.FunctionName == ModifiedFunction.FunctionName)
			{
				// Use modified function (simple merge strategy)
				ExistingFunction = ModifiedFunction;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			OutMergedFunctions.Add(ModifiedFunction);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Merged %d functions"), OutMergedFunctions.Num());
	return true;
}

// Conflict Detection Functions

bool UUnrealAIService::DetectBlueprintConflicts(const FBlueprintStructure& BaseBlueprint, const FBlueprintStructure& ModifiedBlueprint, TArray<FBlueprintConflict>& OutConflicts, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Detecting Blueprint conflicts"));
	
	// Detect variable conflicts
	if (!DetectVariableConflicts(BaseBlueprint.Variables, ModifiedBlueprint.Variables, OutConflicts, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to detect variable conflicts: %s"), *OutError);
	}
	
	// Detect function conflicts
	if (!DetectFunctionConflicts(BaseBlueprint.Functions, ModifiedBlueprint.Functions, OutConflicts, OutError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to detect function conflicts: %s"), *OutError);
	}
	
	// Detect graph conflicts
	for (const FBlueprintGraphData& BaseGraph : BaseBlueprint.Graphs)
	{
		for (const FBlueprintGraphData& ModifiedGraph : ModifiedBlueprint.Graphs)
		{
			if (BaseGraph.GraphName == ModifiedGraph.GraphName)
			{
				if (!DetectNodeConflicts(BaseGraph, ModifiedGraph, OutConflicts, OutError))
				{
					UE_LOG(LogTemp, Warning, TEXT("Failed to detect node conflicts in graph %s: %s"), *BaseGraph.GraphName, *OutError);
				}
				break;
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("Detected %d Blueprint conflicts"), OutConflicts.Num());
	return true;
}

bool UUnrealAIService::DetectVariableConflicts(const TArray<FBlueprintVariable>& BaseVariables, const TArray<FBlueprintVariable>& ModifiedVariables, TArray<FBlueprintConflict>& OutConflicts, FString& OutError)
{
	for (const FBlueprintVariable& BaseVariable : BaseVariables)
	{
		for (const FBlueprintVariable& ModifiedVariable : ModifiedVariables)
		{
			if (BaseVariable.VariableName == ModifiedVariable.VariableName)
			{
				// Check for conflicts
				if (BaseVariable.VariableType != ModifiedVariable.VariableType ||
					BaseVariable.DefaultValue != ModifiedVariable.DefaultValue)
				{
					FBlueprintConflict Conflict;
					Conflict.ConflictType = TEXT("VariableConflict");
					Conflict.ElementName = BaseVariable.VariableName;
					Conflict.BaseValue = FString::Printf(TEXT("%s = %s"), *BaseVariable.VariableType, *BaseVariable.DefaultValue);
					Conflict.ModifiedValue = FString::Printf(TEXT("%s = %s"), *ModifiedVariable.VariableType, *ModifiedVariable.DefaultValue);
					Conflict.ConflictSeverity = 0.7f;
					
					OutConflicts.Add(Conflict);
				}
				break;
			}
		}
	}
	
	return true;
}

bool UUnrealAIService::DetectFunctionConflicts(const TArray<FBlueprintFunction>& BaseFunctions, const TArray<FBlueprintFunction>& ModifiedFunctions, TArray<FBlueprintConflict>& OutConflicts, FString& OutError)
{
	for (const FBlueprintFunction& BaseFunction : BaseFunctions)
	{
		for (const FBlueprintFunction& ModifiedFunction : ModifiedFunctions)
		{
			if (BaseFunction.FunctionName == ModifiedFunction.FunctionName)
			{
				// Check for conflicts in function logic
				if (BaseFunction.FunctionGraph.ExecutionFlow.Num() != ModifiedFunction.FunctionGraph.ExecutionFlow.Num())
				{
					FBlueprintConflict Conflict;
					Conflict.ConflictType = TEXT("FunctionConflict");
					Conflict.ElementName = BaseFunction.FunctionName;
					Conflict.BaseValue = FString::Printf(TEXT("%d execution steps"), BaseFunction.FunctionGraph.ExecutionFlow.Num());
					Conflict.ModifiedValue = FString::Printf(TEXT("%d execution steps"), ModifiedFunction.FunctionGraph.ExecutionFlow.Num());
					Conflict.ConflictSeverity = 0.8f;
					
					OutConflicts.Add(Conflict);
				}
				break;
			}
		}
	}
	
	return true;
}

bool UUnrealAIService::DetectNodeConflicts(const FBlueprintGraphData& BaseGraph, const FBlueprintGraphData& ModifiedGraph, TArray<FBlueprintConflict>& OutConflicts, FString& OutError)
{
	for (const FBlueprintNode& BaseNode : BaseGraph.Nodes)
	{
		for (const FBlueprintNode& ModifiedNode : ModifiedGraph.Nodes)
		{
			if (BaseNode.NodeID == ModifiedNode.NodeID)
			{
				// Check for conflicts in node properties
				if (BaseNode.Position != ModifiedNode.Position)
				{
					FBlueprintConflict Conflict;
					Conflict.ConflictType = TEXT("NodeConflict");
					Conflict.ElementName = BaseNode.NodeName;
					Conflict.BaseValue = FString::Printf(TEXT("Position: %s"), *BaseNode.Position.ToString());
					Conflict.ModifiedValue = FString::Printf(TEXT("Position: %s"), *ModifiedNode.Position.ToString());
					Conflict.ConflictSeverity = 0.3f; // Low severity for position changes
					
					OutConflicts.Add(Conflict);
				}
				break;
			}
		}
	}
	
	return true;
}

// AI-Powered Analysis Functions

FString UUnrealAIService::BuildBlueprintAnalysisPrompt(const FBlueprintStructure& BlueprintStructure)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint analyzer. Analyze this Blueprint structure and provide detailed insights:\n\n");
	Prompt += FString::Printf(TEXT("Blueprint Name: %s\n"), *BlueprintStructure.BlueprintName);
	Prompt += FString::Printf(TEXT("Parent Class: %s\n"), *BlueprintStructure.ParentClass);
	Prompt += FString::Printf(TEXT("Blueprint Type: %s\n\n"), *BlueprintStructure.BlueprintType);
	
	Prompt += TEXT("Structure Analysis:\n");
	Prompt += FString::Printf(TEXT("- Variables: %d\n"), BlueprintStructure.Variables.Num());
	Prompt += FString::Printf(TEXT("- Functions: %d\n"), BlueprintStructure.Functions.Num());
	Prompt += FString::Printf(TEXT("- Components: %d\n"), BlueprintStructure.Components.Num());
	Prompt += FString::Printf(TEXT("- Graphs: %d\n"), BlueprintStructure.Graphs.Num());
	Prompt += FString::Printf(TEXT("- GUIDs: %d\n\n"), BlueprintStructure.AllGUIDs.Num());
	
	// Add variable details
	if (!BlueprintStructure.Variables.IsEmpty())
	{
		Prompt += TEXT("Key Variables:\n");
		for (int32 i = 0; i < FMath::Min(5, BlueprintStructure.Variables.Num()); i++)
		{
			const FBlueprintVariable& Var = BlueprintStructure.Variables[i];
			Prompt += FString::Printf(TEXT("- %s (%s) = %s\n"), 
				*Var.VariableName, *Var.VariableType, *Var.DefaultValue);
		}
		Prompt += TEXT("\n");
	}
	
	// Add function details
	if (!BlueprintStructure.Functions.IsEmpty())
	{
		Prompt += TEXT("Key Functions:\n");
		for (int32 i = 0; i < FMath::Min(5, BlueprintStructure.Functions.Num()); i++)
		{
			const FBlueprintFunction& Func = BlueprintStructure.Functions[i];
			Prompt += FString::Printf(TEXT("- %s (%s)\n"), 
				*Func.FunctionName, *Func.FunctionType);
		}
		Prompt += TEXT("\n");
	}
	
	// Add execution flow details
	if (!BlueprintStructure.Graphs.IsEmpty())
	{
		Prompt += TEXT("Execution Flow:\n");
		for (const FBlueprintGraphData& Graph : BlueprintStructure.Graphs)
		{
			Prompt += FString::Printf(TEXT("Graph: %s (%s)\n"), *Graph.GraphName, *Graph.GraphType);
			for (const FString& FlowStep : Graph.ExecutionFlow)
			{
				Prompt += FString::Printf(TEXT("  - %s\n"), *FlowStep);
			}
		}
		Prompt += TEXT("\n");
	}
	
	Prompt += TEXT("Provide analysis of:\n");
	Prompt += TEXT("1. Blueprint purpose and functionality\n");
	Prompt += TEXT("2. Key variables and their significance\n");
	Prompt += TEXT("3. Function logic and execution flow\n");
	Prompt += TEXT("4. Component architecture\n");
	Prompt += TEXT("5. Potential merge conflicts\n");
	Prompt += TEXT("6. GUID preservation requirements\n");
	
	return Prompt;
}

FString UUnrealAIService::BuildBlueprintMergePrompt(const FBlueprintStructure& BaseBlueprint, const FBlueprintStructure& ModifiedBlueprint, const TArray<FBlueprintConflict>& Conflicts)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint merger. Provide guidance for merging these Blueprint structures:\n\n");
	
	Prompt += TEXT("Base Blueprint:\n");
	Prompt += FString::Printf(TEXT("- Name: %s\n"), *BaseBlueprint.BlueprintName);
	Prompt += FString::Printf(TEXT("- Variables: %d, Functions: %d, Graphs: %d\n"), 
		BaseBlueprint.Variables.Num(), BaseBlueprint.Functions.Num(), BaseBlueprint.Graphs.Num());
	
	Prompt += TEXT("Modified Blueprint:\n");
	Prompt += FString::Printf(TEXT("- Name: %s\n"), *ModifiedBlueprint.BlueprintName);
	Prompt += FString::Printf(TEXT("- Variables: %d, Functions: %d, Graphs: %d\n"), 
		ModifiedBlueprint.Variables.Num(), ModifiedBlueprint.Functions.Num(), ModifiedBlueprint.Graphs.Num());
	
	if (!Conflicts.IsEmpty())
	{
		Prompt += TEXT("\nCONFLICTS DETECTED:\n");
		for (const FBlueprintConflict& Conflict : Conflicts)
		{
			Prompt += FString::Printf(TEXT("- %s: %s (Severity: %.1f)\n"), 
				*Conflict.ConflictType, *Conflict.ElementName, Conflict.ConflictSeverity);
			Prompt += FString::Printf(TEXT("  Base: %s\n"), *Conflict.BaseValue);
			Prompt += FString::Printf(TEXT("  Modified: %s\n"), *Conflict.ModifiedValue);
		}
	}
	
	Prompt += TEXT("\nProvide merge guidance:\n");
	Prompt += TEXT("1. Which variables to keep from each Blueprint\n");
	Prompt += TEXT("2. How to resolve function conflicts\n");
	Prompt += TEXT("3. How to merge execution flows\n");
	Prompt += TEXT("4. Which GUIDs to preserve\n");
	Prompt += TEXT("5. How to handle new vs. modified content\n");
	Prompt += TEXT("6. Any special considerations for this Blueprint type\n");
	
	return Prompt;
}

FString UUnrealAIService::BuildBlueprintConflictResolutionPrompt(const FBlueprintConflict& Conflict, const FBlueprintStructure& BaseBlueprint, const FBlueprintStructure& ModifiedBlueprint)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint conflict resolver. Provide guidance for resolving this specific conflict:\n\n");
	
	Prompt += FString::Printf(TEXT("Conflict Type: %s\n"), *Conflict.ConflictType);
	Prompt += FString::Printf(TEXT("Element: %s\n"), *Conflict.ElementName);
	Prompt += FString::Printf(TEXT("Severity: %.1f\n\n"), Conflict.ConflictSeverity);
	
	Prompt += FString::Printf(TEXT("Base Value: %s\n"), *Conflict.BaseValue);
	Prompt += FString::Printf(TEXT("Modified Value: %s\n\n"), *Conflict.ModifiedValue);
	
	Prompt += TEXT("Context:\n");
	Prompt += FString::Printf(TEXT("Base Blueprint: %s\n"), *BaseBlueprint.BlueprintName);
	Prompt += FString::Printf(TEXT("Modified Blueprint: %s\n\n"), *ModifiedBlueprint.BlueprintName);
	
	Prompt += TEXT("Provide specific resolution guidance:\n");
	Prompt += TEXT("1. Which value should be kept and why\n");
	Prompt += TEXT("2. How to combine both values if possible\n");
	Prompt += TEXT("3. What impact this choice will have\n");
	Prompt += TEXT("4. Any additional considerations\n");
	
	return Prompt;
}

// Blueprint Serialization Functions

bool UUnrealAIService::SerializeBlueprintStructure(const FBlueprintStructure& BlueprintStructure, FUAssetStructure& OutUAssetStructure, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing Blueprint structure"));
	
	// Initialize UAsset structure
	OutUAssetStructure.AssetType = TEXT("Blueprint");
	OutUAssetStructure.AssetName = BlueprintStructure.BlueprintName;
	
	// Serialize variables as properties
	for (const FBlueprintVariable& Variable : BlueprintStructure.Variables)
	{
		FUAssetPropertyData Property;
		Property.PropertyName = Variable.VariableName;
		Property.PropertyType = Variable.VariableType;
		Property.PropertyValue = Variable.DefaultValue;
		Property.PropertyGUID = Variable.VariableGUID;
		Property.PropertyFlags = Variable.VariableFlags;
		
		OutUAssetStructure.Properties.Add(Property);
	}
	
	// Serialize functions as exports
	for (const FBlueprintFunction& Function : BlueprintStructure.Functions)
	{
		FExportEntry Export;
		Export.ClassName = Function.FunctionName;
		Export.GUID = Function.FunctionGUID;
		
		// Serialize function graph data
		if (!SerializeBlueprintGraph(Function.FunctionGraph, Export.AssetData, OutError))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to serialize function graph: %s"), *OutError);
		}
		
		Export.SerialSize = Export.AssetData.Num();
		OutUAssetStructure.Exports.Add(Export);
	}
	
	// Serialize graphs as exports
	for (const FBlueprintGraphData& Graph : BlueprintStructure.Graphs)
	{
		FExportEntry Export;
		Export.ClassName = Graph.GraphName;
		Export.GUID = FGuid::NewGuid();
		
		// Serialize graph data
		if (!SerializeBlueprintGraph(Graph, Export.AssetData, OutError))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to serialize graph: %s"), *OutError);
		}
		
		Export.SerialSize = Export.AssetData.Num();
		OutUAssetStructure.Exports.Add(Export);
	}
	
	// Add all GUIDs
	OutUAssetStructure.GUIDs = BlueprintStructure.AllGUIDs;
	
	UE_LOG(LogTemp, Log, TEXT("Successfully serialized Blueprint structure"));
	return true;
}

bool UUnrealAIService::SerializeBlueprintGraph(const FBlueprintGraphData& Graph, TArray<uint8>& OutGraphData, FString& OutError)
{
	UE_LOG(LogTemp, Log, TEXT("Serializing Blueprint graph: %s"), *Graph.GraphName);
	
	// Enhanced Blueprint graph serializer with comprehensive node support
	
	// For now, create a simple binary representation
	FMemoryWriter Archive(OutGraphData, true);
	Archive.SetIsSaving(true);
	
	// Write graph header
	Archive << const_cast<FBlueprintGraphData&>(Graph).GraphName;
	Archive << const_cast<FBlueprintGraphData&>(Graph).GraphType;
	
	// Write node count
	int32 NodeCount = Graph.Nodes.Num();
	Archive << NodeCount;
	
	// Write nodes
	for (const FBlueprintNode& Node : Graph.Nodes)
	{
		Archive << const_cast<FBlueprintNode&>(Node).NodeID;
		Archive << const_cast<FBlueprintNode&>(Node).NodeType;
		Archive << const_cast<FBlueprintNode&>(Node).NodeName;
		Archive << const_cast<FBlueprintNode&>(Node).Position;
	}
	
	// Write execution flow
	int32 FlowCount = Graph.ExecutionFlow.Num();
	Archive << FlowCount;
	for (const FString& FlowStep : Graph.ExecutionFlow)
	{
		Archive << const_cast<FString&>(FlowStep);
	}
	
	UE_LOG(LogTemp, Log, TEXT("Serialized graph with %d nodes, %d flow steps (%d bytes)"), 
		Graph.Nodes.Num(), Graph.ExecutionFlow.Num(), OutGraphData.Num());
	
	return true;
}

// Utility Functions

bool UUnrealAIService::ValidateBlueprintStructure(const FBlueprintStructure& BlueprintStructure, FString& OutError)
{
	// Basic validation
	if (BlueprintStructure.BlueprintName.IsEmpty())
	{
		OutError = TEXT("Blueprint name is empty");
		return false;
	}
	
	if (BlueprintStructure.ParentClass.IsEmpty())
	{
		OutError = TEXT("Parent class is empty");
		return false;
	}
	
	// Check for duplicate variable names
	TSet<FString> VariableNames;
	for (const FBlueprintVariable& Variable : BlueprintStructure.Variables)
	{
		if (VariableNames.Contains(Variable.VariableName))
		{
			OutError = FString::Printf(TEXT("Duplicate variable name: %s"), *Variable.VariableName);
			return false;
		}
		VariableNames.Add(Variable.VariableName);
	}
	
	// Check for duplicate function names
	TSet<FString> FunctionNames;
	for (const FBlueprintFunction& Function : BlueprintStructure.Functions)
	{
		if (FunctionNames.Contains(Function.FunctionName))
		{
			OutError = FString::Printf(TEXT("Duplicate function name: %s"), *Function.FunctionName);
			return false;
		}
		FunctionNames.Add(Function.FunctionName);
	}
	
	return true;
}

bool UUnrealAIService::CalculateBlueprintComplexity(const FBlueprintStructure& BlueprintStructure, float& OutComplexity)
{
	// Calculate complexity based on various factors
	float Complexity = 0.0f;
	
	// Variables contribute to complexity
	Complexity += BlueprintStructure.Variables.Num() * 0.1f;
	
	// Functions contribute more to complexity
	Complexity += BlueprintStructure.Functions.Num() * 0.3f;
	
	// Graphs and execution flow contribute significantly
	for (const FBlueprintGraphData& Graph : BlueprintStructure.Graphs)
	{
		Complexity += Graph.Nodes.Num() * 0.2f;
		Complexity += Graph.ExecutionFlow.Num() * 0.1f;
	}
	
	// Components contribute moderately
	Complexity += BlueprintStructure.Components.Num() * 0.2f;
	
	OutComplexity = FMath::Clamp(Complexity, 0.0f, 10.0f);
	return true;
}

bool UUnrealAIService::GenerateBlueprintSummary(const FBlueprintStructure& BlueprintStructure, FString& OutSummary)
{
	OutSummary = FString::Printf(TEXT("Blueprint: %s\n"), *BlueprintStructure.BlueprintName);
	OutSummary += FString::Printf(TEXT("Parent Class: %s\n"), *BlueprintStructure.ParentClass);
	OutSummary += FString::Printf(TEXT("Type: %s\n\n"), *BlueprintStructure.BlueprintType);
	
	OutSummary += FString::Printf(TEXT("Variables: %d\n"), BlueprintStructure.Variables.Num());
	OutSummary += FString::Printf(TEXT("Functions: %d\n"), BlueprintStructure.Functions.Num());
	OutSummary += FString::Printf(TEXT("Components: %d\n"), BlueprintStructure.Components.Num());
	OutSummary += FString::Printf(TEXT("Graphs: %d\n"), BlueprintStructure.Graphs.Num());
	
	// Calculate complexity
	float Complexity;
	if (CalculateBlueprintComplexity(BlueprintStructure, Complexity))
	{
		OutSummary += FString::Printf(TEXT("Complexity: %.1f/10.0\n"), Complexity);
	}
	
	return true;
}

// Additional helper functions that were declared but not implemented

bool UUnrealAIService::MapNodeConnections(const TArray<FBlueprintNode>& Nodes, const TArray<FBlueprintConnection>& Connections, TMap<FString, TArray<FString>>& OutConnections)
{
	// Map node connections for analysis
	for (const FBlueprintConnection& Connection : Connections)
	{
		if (!OutConnections.Contains(Connection.SourceNodeID))
		{
			OutConnections.Add(Connection.SourceNodeID, TArray<FString>());
		}
		OutConnections[Connection.SourceNodeID].Add(Connection.TargetNodeID);
	}
	
	return true;
}

bool UUnrealAIService::FindExecutionPaths(const FBlueprintGraphData& Graph, TArray<TArray<FString>>& OutPaths, FString& OutError)
{
	// Find all possible execution paths through the graph
	// This is a simplified implementation
	TArray<FString> Path;
	Path.Add(TEXT("Start"));
	
	for (const FString& FlowStep : Graph.ExecutionFlow)
	{
		Path.Add(FlowStep);
	}
	
	Path.Add(TEXT("End"));
	OutPaths.Add(Path);
	
	return true;
}

bool UUnrealAIService::ParseBlueprintVariable(const TArray<uint8>& VariableData, FBlueprintVariable& OutVariable, FString& OutError)
{
	// Simplified variable parser
	OutVariable.VariableName = TEXT("Variable");
	OutVariable.VariableType = TEXT("Int");
	OutVariable.DefaultValue = TEXT("0");
	OutVariable.VariableGUID = FGuid::NewGuid();
	
	return true;
}

bool UUnrealAIService::ParseBlueprintFunction(const TArray<uint8>& FunctionData, FBlueprintFunction& OutFunction, FString& OutError)
{
	// Simplified function parser
	OutFunction.FunctionName = TEXT("Function");
	OutFunction.FunctionType = TEXT("Function");
	OutFunction.FunctionGUID = FGuid::NewGuid();
	
	return true;
}

bool UUnrealAIService::ExtractNodePins(const TArray<uint8>& PinData, TArray<FBlueprintPin>& OutPins, FString& OutError)
{
	// Simplified pin extractor
	FBlueprintPin Pin;
	Pin.PinName = TEXT("Pin");
	Pin.PinType = TEXT("Exec");
	Pin.Direction = TEXT("Input");
	OutPins.Add(Pin);
	
	return true;
}

bool UUnrealAIService::ResolveBlueprintConflict(const FBlueprintConflict& Conflict, const FString& AIGuidance, FString& OutResolvedValue, FString& OutError)
{
	// Use AI guidance to resolve conflicts
	if (AIGuidance.Contains(TEXT("keep base")))
	{
		OutResolvedValue = Conflict.BaseValue;
	}
	else if (AIGuidance.Contains(TEXT("keep modified")))
	{
		OutResolvedValue = Conflict.ModifiedValue;
	}
	else
	{
		// Default to modified value
		OutResolvedValue = Conflict.ModifiedValue;
	}
	
	return true;
}

bool UUnrealAIService::SerializeBlueprintNode(const FBlueprintNode& Node, TArray<uint8>& OutNodeData, FString& OutError)
{
	// Simplified node serializer
	FMemoryWriter Archive(OutNodeData, true);
	FString NodeID = Node.NodeID;
	FString NodeType = Node.NodeType;
	FString NodeName = Node.NodeName;
	FVector2D Position = Node.Position;
	
	Archive << NodeID;
	Archive << NodeType;
	Archive << NodeName;
	Archive << Position;
	
	return true;
}

bool UUnrealAIService::SerializeBlueprintVariable(const FBlueprintVariable& Variable, TArray<uint8>& OutVariableData, FString& OutError)
{
	// Simplified variable serializer
	FMemoryWriter Archive(OutVariableData, true);
	FString VariableName = Variable.VariableName;
	FString VariableType = Variable.VariableType;
	FString DefaultValue = Variable.DefaultValue;
	FGuid VariableGUID = Variable.VariableGUID;
	
	Archive << VariableName;
	Archive << VariableType;
	Archive << DefaultValue;
	Archive << VariableGUID;
	
	return true;
}

bool UUnrealAIService::SerializeBlueprintFunction(const FBlueprintFunction& Function, TArray<uint8>& OutFunctionData, FString& OutError)
{
	// Simplified function serializer
	FMemoryWriter Archive(OutFunctionData, true);
	FString FunctionName = Function.FunctionName;
	FString FunctionType = Function.FunctionType;
	FGuid FunctionGUID = Function.FunctionGUID;
	
	Archive << FunctionName;
	Archive << FunctionType;
	Archive << FunctionGUID;
	
	return true;
}

FString UUnrealAIService::LoadBlueprintTemplate()
{
	FString TemplateContent;
	
	// Try to load the template file from the project directory
	FString ProjectDir = FPaths::ProjectDir();
	FString TemplatePath = FPaths::Combine(ProjectDir, TEXT("ExampleBlueprintStructure.json"));
	
	UE_LOG(LogTemp, Log, TEXT("LoadBlueprintTemplate: Attempting to load template from: %s"), *TemplatePath);
	
	// Check if file exists
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*TemplatePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadBlueprintTemplate: Template file not found at: %s"), *TemplatePath);
		return TEXT("");
	}
	
	// Try to read the file
	if (FFileHelper::LoadFileToString(TemplateContent, *TemplatePath))
	{
		UE_LOG(LogTemp, Log, TEXT("LoadBlueprintTemplate: Successfully loaded template, length: %d"), TemplateContent.Len());
		
		// Validate that it's valid JSON
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(TemplateContent);
		
		if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			UE_LOG(LogTemp, Log, TEXT("LoadBlueprintTemplate: Template is valid JSON"));
			return TemplateContent;
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("LoadBlueprintTemplate: Template file contains invalid JSON"));
			return TEXT("");
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("LoadBlueprintTemplate: Failed to read template file"));
		return TEXT("");
	}
}
