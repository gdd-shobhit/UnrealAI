#include "../Public/UnrealAILLMAdapter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FUnrealAILLMAdapter::FUnrealAILLMAdapter()
	: AIEndpoint(TEXT("http://localhost:11434/api/generate"))
	, bIsEnabled(false)
	, RequestTimeoutSeconds(30.0f)
{
}

bool FUnrealAILLMAdapter::ResolveConflicts(
	const TArray<FMergeConflict>& Conflicts,
	const FString& Context,
	TArray<FMergeOperation>& OutResolvedOperations)
{
	if (!IsAvailable() || Conflicts.Num() == 0)
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealAILLMAdapter: Resolving %d conflicts"), Conflicts.Num());

	// Build prompt
	FString Prompt = BuildConflictResolutionPrompt(Conflicts, Context);

	// Send request to AI
	FString AIResponse;
	if (!SendAIRequest(Prompt, AIResponse))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealAILLMAdapter: Failed to get AI response"));
		return false;
	}

	// Parse response
	if (!ParseAIResponse(AIResponse, OutResolvedOperations))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealAILLMAdapter: Failed to parse AI response"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("UnrealAILLMAdapter: Successfully resolved %d operations"), OutResolvedOperations.Num());
	return true;
}

bool FUnrealAILLMAdapter::IsAvailable() const
{
	return bIsEnabled && !AIEndpoint.IsEmpty();
}

void FUnrealAILLMAdapter::SetAIEndpoint(const FString& Endpoint)
{
	AIEndpoint = Endpoint;
}

void FUnrealAILLMAdapter::SetEnabled(bool bEnabled)
{
	bIsEnabled = bEnabled;
}

bool FUnrealAILLMAdapter::SendAIRequest(const FString& Prompt, FString& OutResponse)
{
	if (AIEndpoint.IsEmpty())
	{
		return false;
	}

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetURL(AIEndpoint);

	// Build request payload for Ollama
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
	JsonObject->SetStringField(TEXT("model"), TEXT("llama2:latest"));
	JsonObject->SetStringField(TEXT("prompt"), Prompt);
	JsonObject->SetBoolField(TEXT("stream"), false);
	JsonObject->SetNumberField(TEXT("temperature"), 0.3); // Lower temperature for more consistent results

	FString RequestPayload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestPayload);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	HttpRequest->SetContentAsString(RequestPayload);

	// Send request synchronously (blocking)
	HttpRequest->ProcessRequest();

	// Wait for completion with timeout
	int32 TimeoutCounter = 0;
	const int32 MaxTimeout = RequestTimeoutSeconds * 100; // 100 checks per second

	while (HttpRequest->GetStatus() == EHttpRequestStatus::Processing && TimeoutCounter < MaxTimeout)
	{
		FPlatformProcess::Sleep(0.01f);
		TimeoutCounter++;
	}

	// Process response
	if (HttpRequest->GetStatus() == EHttpRequestStatus::Succeeded)
	{
		FHttpResponsePtr HttpResponse = HttpRequest->GetResponse();
		if (HttpResponse.IsValid())
		{
			FString ResponseContent = HttpResponse->GetContentAsString();
			
			// Parse Ollama response format
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
				OutResponse = AccumulatedResponse;
				return true;
			}
		}
	}

	return false;
}

FString FUnrealAILLMAdapter::BuildConflictResolutionPrompt(
	const TArray<FMergeConflict>& Conflicts,
	const FString& Context)
{
	FString Prompt = TEXT("You are an expert Unreal Engine Blueprint merge resolver. ");
	Prompt += TEXT("Analyze these Blueprint merge conflicts and provide resolution decisions in JSON format.\n\n");

	Prompt += TEXT("CONTEXT:\n");
	Prompt += Context;
	Prompt += TEXT("\n\nCONFLICTS TO RESOLVE:\n");

	for (int32 i = 0; i < Conflicts.Num(); i++)
	{
		const FMergeConflict& Conflict = Conflicts[i];
		Prompt += FString::Printf(TEXT("\nConflict %d:\n"), i + 1);
		Prompt += FString::Printf(TEXT("  ID: %s\n"), *Conflict.ConflictId);
		Prompt += FString::Printf(TEXT("  Type: %s\n"), *Conflict.ConflictType);
		Prompt += FString::Printf(TEXT("  Element: %s\n"), *Conflict.ElementName);
		Prompt += FString::Printf(TEXT("  Base: %s\n"), *Conflict.BaseValue);
		Prompt += FString::Printf(TEXT("  Local: %s\n"), *Conflict.LocalValue);
		Prompt += FString::Printf(TEXT("  Remote: %s\n"), *Conflict.RemoteValue);
	}

	Prompt += TEXT("\n\nRESOLUTION RULES:\n");
	Prompt += TEXT("1. Prefer non-destructive solutions\n");
	Prompt += TEXT("2. For variable conflicts, maintain type safety\n");
	Prompt += TEXT("3. For position conflicts, avoid overlaps\n");
	Prompt += TEXT("4. When in doubt, prefer local changes\n\n");

	Prompt += TEXT("OUTPUT FORMAT (JSON only, no explanations):\n");
	Prompt += TEXT("{\n");
	Prompt += TEXT("  \"merged_operations\": [\n");
	Prompt += TEXT("    {\n");
	Prompt += TEXT("      \"operation_type\": \"UpdateVariable\",\n");
	Prompt += TEXT("      \"target_id\": \"conflict_id_here\",\n");
	Prompt += TEXT("      \"resolution\": \"local\",\n");
	Prompt += TEXT("      \"reason\": \"explanation\"\n");
	Prompt += TEXT("    }\n");
	Prompt += TEXT("  ]\n");
	Prompt += TEXT("}\n\n");

	Prompt += TEXT("Respond with ONLY the JSON object, no additional text.");

	return Prompt;
}

bool FUnrealAILLMAdapter::ParseAIResponse(const FString& AIResponse, TArray<FMergeOperation>& OutOperations)
{
	// Extract JSON from response
	FString CleanJSON = AIResponse;
	
	// Find JSON boundaries
	int32 StartIndex = CleanJSON.Find(TEXT("{"));
	int32 EndIndex = CleanJSON.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (StartIndex != INDEX_NONE && EndIndex != INDEX_NONE && EndIndex > StartIndex)
	{
		CleanJSON = CleanJSON.Mid(StartIndex, EndIndex - StartIndex + 1);
	}

	// Parse JSON
	TSharedPtr<FJsonObject> ResponseObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanJSON);

	if (!FJsonSerializer::Deserialize(Reader, ResponseObject) || !ResponseObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealAILLMAdapter: Failed to parse JSON response"));
		return false;
	}

	// Extract operations
	const TArray<TSharedPtr<FJsonValue>>* OperationsArray = nullptr;
	if (!ResponseObject->TryGetArrayField(TEXT("merged_operations"), OperationsArray))
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealAILLMAdapter: No 'merged_operations' field found"));
		return false;
	}

	// Parse each operation
	for (const TSharedPtr<FJsonValue>& OpValue : *OperationsArray)
	{
		TSharedPtr<FJsonObject> OpObject = OpValue->AsObject();
		if (!OpObject.IsValid())
		{
			continue;
		}

		FMergeOperation Op;
		
		// Parse operation type
		FString OpTypeStr = OpObject->GetStringField(TEXT("operation_type"));
		if (OpTypeStr == TEXT("UpdateVariable"))
		{
			Op.OperationType = EMergeOperationType::UpdateVariable;
		}
		else if (OpTypeStr == TEXT("UpdateNode"))
		{
			Op.OperationType = EMergeOperationType::UpdateNodeProperty;
		}
		else if (OpTypeStr == TEXT("UpdateComponent"))
		{
			Op.OperationType = EMergeOperationType::UpdateComponent;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Unknown operation type: %s"), *OpTypeStr);
			continue;
		}

		Op.TargetId = OpObject->GetStringField(TEXT("target_id"));
		FString Resolution = OpObject->GetStringField(TEXT("resolution"));
		FString Reason = OpObject->GetStringField(TEXT("reason"));

		Op.AdditionalData.Add(TEXT("Resolution"), Resolution);
		Op.AdditionalData.Add(TEXT("Reason"), Reason);
		Op.AdditionalData.Add(TEXT("AIResolved"), TEXT("true"));

		OutOperations.Add(Op);
	}

	return OutOperations.Num() > 0;
}
