#include "BlueprintDiffService.h"

#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Exporters/Exporter.h"
#include "Misc/OutputDevice.h"
#include "UnrealExporter.h"

UBlueprint* FBlueprintDiffService::FindBlueprintByName(const FString& BlueprintName)
{
	if (BlueprintName.IsEmpty())
	{
		return nullptr;
	}

	// Try direct path in a common location first
	{
		const FString AssetPath = FString::Printf(TEXT("/Game/FirstPerson/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
		if (UBlueprint* Direct = LoadObject<UBlueprint>(nullptr, *AssetPath))
		{
			return Direct;
		}
	}

	// Search common package paths
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FString> PackagePaths = {
		TEXT("/Game"),
		TEXT("/Game/FirstPerson"),
		TEXT("/Game/FirstPerson/Blueprints"),
		TEXT("/Game/FirstPerson/Character"),
		TEXT("/Game/FirstPerson/Input"),
		TEXT("/Game/FirstPerson/Maps")
	};

	for (const FString& PackagePath : PackagePaths)
	{
		FARFilter Filter;
		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.PackagePaths.Add(FName(*PackagePath));

		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
		for (const FAssetData& AssetData : AssetDataList)
		{
			if (AssetData.AssetName.ToString() == BlueprintName)
			{
				return Cast<UBlueprint>(AssetData.GetAsset());
			}
		}
	}

	return nullptr;
}

UBlueprint* FBlueprintDiffService::FindAnyBlueprint()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

	TArray<FAssetData> AllBlueprints;
	AssetRegistryModule.Get().GetAssets(Filter, AllBlueprints);
	for (const FAssetData& AssetData : AllBlueprints)
	{
		if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
		{
			return BP;
		}
	}
	return nullptr;
}

bool FBlueprintDiffService::ExportBlueprintT3D(UBlueprint* Blueprint, FString& OutText)
{
	if (!Blueprint)
	{
		return false;
	}
	FStringOutputDevice Out;
	const uint32 PortFlags = PPF_ExportsNotFullyQualified | PPF_Copy | PPF_IncludeTransient;
	UExporter::ExportToOutputDevice(nullptr, Blueprint, nullptr, Out, TEXT("T3D"), 0, PortFlags);
	OutText = Out;
	return true;
}


