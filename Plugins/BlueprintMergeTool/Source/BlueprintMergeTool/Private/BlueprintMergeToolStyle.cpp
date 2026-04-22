#include "../Public/BlueprintMergeToolStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FBlueprintMergeToolStyle::StyleInstance = nullptr;

void FBlueprintMergeToolStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FBlueprintMergeToolStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FBlueprintMergeToolStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("BlueprintMergeToolStyle"));
	return StyleSetName;
}

const ISlateStyle& FBlueprintMergeToolStyle::Get()
{
	return *StyleInstance;
}

void FBlueprintMergeToolStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

TSharedRef<FSlateStyleSet> FBlueprintMergeToolStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet("BlueprintMergeToolStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("BlueprintMergeTool")->GetBaseDir() / TEXT("Resources"));

	Style->Set("BlueprintMergeTool.OpenPluginWindow", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), CoreStyleConstants::Icon16x16));

	return Style;
}

#undef RootToContentDir
