#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "UnrealBridgeClient.generated.h"

/** Result of a single poll: new log lines and optional tool call to execute */
USTRUCT(BlueprintType)
struct FBridgePollResult
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FString> LogLines;

	UPROPERTY()
	bool bDone = false;

	UPROPERTY()
	FString ToolCallId;

	UPROPERTY()
	FString ToolName;

	UPROPERTY()
	FString ToolArgsJson;

	UPROPERTY()
	FString Error;
};

/** Client that connects to the Python MCP bridge. Sends prompt, polls for log lines and tool calls, sends tool results. */
UCLASS()
class UNREALAI_API UUnrealBridgeClient : public UObject
{
	GENERATED_BODY()

public:
	UUnrealBridgeClient();

	/** Configure bridge URL (e.g. http://127.0.0.1:8765). Call before SendRunPrompt. */
	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void SetBridgeUrl(const FString& BaseUrl);

	/** Start a run: send prompt and provider to bridge. Returns session id on success, empty on failure. */
	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void SendRunPrompt(const FString& Prompt, const FString& Provider, TFunction<void(bool bSuccess, const FString& SessionId)> Callback);

	/** Poll for log lines and optional tool call. Call repeatedly until bDone. Execute tool when ToolName is set, then call SendToolResult. */
	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void Poll(const FString& SessionId, TFunction<void(const FBridgePollResult& Result)> Callback);

	/** Send result of tool execution. Call after executing tool from Poll result. */
	UFUNCTION(BlueprintCallable, Category = "UnrealAI")
	void SendToolResult(const FString& SessionId, const FString& ToolCallId, const FString& Result, TFunction<void(bool bSuccess)> Callback);

	/** Get bridge URL from config (DefaultUnrealAI.ini [UnrealAI] BridgeUrl). */
	static FString GetDefaultBridgeUrl();

private:
	FString BridgeBaseUrl;

	void OnRunResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(bool, const FString&)> Callback);
	void OnPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(const FBridgePollResult&)> Callback);
	void OnToolResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(bool)> Callback);
};
