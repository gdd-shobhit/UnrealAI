#pragma once

#include "CoreMinimal.h"
#include "BlueprintT3DParser.h"

class UBlueprint;

class FBlueprintDiffService
{
public:
	// Finds a blueprint by asset name (without path). Returns nullptr if not found.
	static UBlueprint* FindBlueprintByName(const FString& BlueprintName);

	// Returns the first blueprint found in the project or nullptr.
	static UBlueprint* FindAnyBlueprint();

	// Exports the blueprint to T3D-style text using Unreal's exporter. Returns true if successful.
	static bool ExportBlueprintT3D(UBlueprint* Blueprint, FString& OutText);

	// Parse T3D text into structured asset info
	static bool ParseT3D(const FString& Text, FT3DAssetInfo& OutInfo) { return FBlueprintT3DParser::Parse(Text, OutInfo); }
	static TSharedPtr<class FJsonObject> ToJson(const FT3DAssetInfo& Info) { return FBlueprintT3DParser::ToJson(Info); }
	static void LogSummary(const FT3DAssetInfo& Info) { FBlueprintT3DParser::LogSummary(Info); }
};


