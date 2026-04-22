#pragma once

#include "CoreMinimal.h"
#include "BlueprintMergeToolAPI.h"
#include "MergePlanner.h"

/**
 * LLM Adapter that integrates with the UnrealAI plugin
 * Provides conflict resolution using the existing AI service
 */
class BLUEPRINTMERGETOOL_API FUnrealAILLMAdapter : public ILLMAdapter
{
public:
	FUnrealAILLMAdapter();
	virtual ~FUnrealAILLMAdapter() = default;

	// ILLMAdapter interface
	virtual bool ResolveConflicts(
		const TArray<FMergeConflict>& Conflicts,
		const FString& Context,
		TArray<FMergeOperation>& OutResolvedOperations
	) override;

	virtual bool IsAvailable() const override;

	/**
	 * Set the AI service endpoint
	 * @param Endpoint URL for the AI service
	 */
	void SetAIEndpoint(const FString& Endpoint);

	/**
	 * Enable or disable the adapter
	 * @param bEnabled Whether the adapter should be enabled
	 */
	void SetEnabled(bool bEnabled);

private:
	/**
	 * Send a request to the AI service
	 * @param Prompt Prompt to send
	 * @param OutResponse Response from AI
	 * @return True if successful
	 */
	bool SendAIRequest(const FString& Prompt, FString& OutResponse);

	/**
	 * Build a conflict resolution prompt
	 * @param Conflicts Conflicts to resolve
	 * @param Context Additional context
	 * @return Formatted prompt
	 */
	FString BuildConflictResolutionPrompt(
		const TArray<FMergeConflict>& Conflicts,
		const FString& Context
	);

	/**
	 * Parse AI response into merge operations
	 * @param AIResponse Response from AI
	 * @param OutOperations Parsed operations
	 * @return True if parsing was successful
	 */
	bool ParseAIResponse(const FString& AIResponse, TArray<FMergeOperation>& OutOperations);

private:
	FString AIEndpoint;
	bool bIsEnabled;
	float RequestTimeoutSeconds;
};
