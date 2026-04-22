#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Slate style for Blueprint Merge Tool
 */
class FBlueprintMergeToolStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static void ReloadTextures();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedRef<FSlateStyleSet> Create();

private:
	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
