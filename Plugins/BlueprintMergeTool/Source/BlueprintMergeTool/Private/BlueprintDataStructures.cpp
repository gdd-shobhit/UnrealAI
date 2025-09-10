#include "../Public/BlueprintDataStructures.h"
#include "../Public/SnapshotManager.h"

void FBlueprintMergeData::CaptureGraphs(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return;
	}

	// Capture graphs
	TArray<UEdGraph*> AllGraphs;
	FSnapshotManager::GetAllBlueprintGraphs(Blueprint, AllGraphs);
	
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph)
		{
			Graphs.Add(FBlueprintMergeGraphData(Graph));
		}
	}
}
