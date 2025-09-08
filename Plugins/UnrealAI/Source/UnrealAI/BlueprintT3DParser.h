#pragma once

#include "CoreMinimal.h"

struct FT3DPinInfo
{
	FString PinId;
	FString PinName;
	TMap<FString, FString> Attributes;
};

struct FT3DNodeInfo
{
	FString ClassName;
	FString Name;
	FString NodeGuid;
	FString FunctionReference;
	FString VarGuid;
	TArray<FT3DPinInfo> Pins;
	TMap<FString, FString> Properties;
};

struct FT3DGraphInfo
{
	FString ClassName;
	FString Name;
	TArray<FT3DNodeInfo> Nodes;
	TMap<FString, FString> Properties;
};

struct FT3DAssetInfo
{
	FString ParentClass;
	TArray<FString> NewVariables; 
	TArray<FString> FunctionGraphs; 
	TArray<FString> NodesList; 
	TArray<FT3DGraphInfo> Graphs;
	TArray<FT3DNodeInfo> TopLevelNodes;
	TMap<FString, FString> TopLevelProperties;
};

class FBlueprintT3DParser
{
public:
	static bool Parse(const FString& Text, FT3DAssetInfo& OutInfo);
	static void LogSummary(const FT3DAssetInfo& Info);
	static TSharedPtr<class FJsonObject> ToJson(const FT3DAssetInfo& Info);

private:
	struct FObjectContext
	{
		FString ClassName;
		FString Name;
		TArray<FString> PropertyLines;
	};

	static bool ParseBeginObjectHeader(const FString& Line, FString& OutClass, FString& OutName);
	static void ParseTopLevelLine(const FString& Line, FT3DAssetInfo& OutInfo);
	static void ParseObjectBlock(const FObjectContext& Ctx, FT3DAssetInfo& OutInfo);
	static void ParseNodeProperties(const TArray<FString>& Lines, FT3DNodeInfo& OutNode);
	static void ParsePinsFromCustomProperties(const FString& Line, TArray<FT3DPinInfo>& OutPins);
	static void SplitTopLevelCommaPairs(const FString& ParenContent, TArray<FString>& OutPairs);
	static void ParseKeyValue(const FString& Pair, FString& OutKey, FString& OutVal);
};


