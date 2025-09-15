#include "../Public/ApplyEngine.h"
#include "../Public/SnapshotManager.h"
#include "../Public/BlueprintDataStructures.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Engine/Engine.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/SCS_Node.h"

bool FApplyEngine::ApplyMergePlan(
	UBlueprint* TargetBlueprint,
	const FMergePlan& MergePlan,
	FApplyResult& OutResult)
{
	if (!TargetBlueprint)
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is null");
		return false;
	}

	if (!IsValid(TargetBlueprint))
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is not valid");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Applying merge plan to Blueprint: %s"), *TargetBlueprint->GetName());
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: %d operations to apply"), MergePlan.AutoResolvedOperations.Num());

	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Must be called from game thread"));
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Operations must be applied from the game thread");
		return false;
	}

	// Create undo transaction
	TSharedPtr<FScopedTransaction> Transaction = CreateUndoTransaction(
		FString::Printf(TEXT("Apply Blueprint Merge (%d operations)"), MergePlan.AutoResolvedOperations.Num())
	);

	// Clear result
	OutResult = FApplyResult();
	OutResult.bSuccess = true;

	// Sort operations to ensure proper dependency order: Variables -> Graphs -> Nodes -> Connections
	TArray<FMergeOperation> SortedOperations = MergePlan.AutoResolvedOperations;
	SortedOperations.Sort([](const FMergeOperation& A, const FMergeOperation& B) {
		// Define operation priority (lower number = higher priority)
		auto GetOperationPriority = [](EMergeOperationType Type) -> int32 {
			switch (Type)
			{
			case EMergeOperationType::AddVariable:
			case EMergeOperationType::UpdateVariable:
			case EMergeOperationType::RemoveVariable:
				return 1; // Variables first
			case EMergeOperationType::AddGraph:
			case EMergeOperationType::RemoveGraph:
				return 2; // Graphs second
			case EMergeOperationType::AddNode:
			case EMergeOperationType::RemoveNode:
			case EMergeOperationType::UpdateNodeProperty:
				return 3; // Nodes third
			case EMergeOperationType::LinkPins:
			case EMergeOperationType::UnlinkPins:
				return 4; // Connections last
			default:
				return 5; // Everything else
			}
		};
		return GetOperationPriority(A.OperationType) < GetOperationPriority(B.OperationType);
	});

	// Apply each operation in sorted order
	for (int32 i = 0; i < SortedOperations.Num(); i++)
	{
		const FMergeOperation& Operation = SortedOperations[i];
		FString OperationError;

		bool bOperationSuccess = ApplyOperation(TargetBlueprint, Operation, OperationError);

		FString OperationName = FString::Printf(TEXT("Op %d: %s on %s"), 
			i, 
			*StaticEnum<EMergeOperationType>()->GetValueAsString(Operation.OperationType), 
			*Operation.TargetId);

		if (bOperationSuccess)
		{
			OutResult.AppliedOperations.Add(OperationName);
			LogOperationResult(Operation.OperationType, Operation.TargetId, true);
		}
		else
		{
			OutResult.FailedOperations.Add(FString::Printf(TEXT("%s: %s"), *OperationName, *OperationError));
			OutResult.Warnings.Add(OperationError);
			LogOperationResult(Operation.OperationType, Operation.TargetId, false, OperationError);

			// Continue with other operations even if one fails
		}
	}

	// Validate Blueprint integrity after all operations
	TArray<FString> ValidationErrors;
	if (!ValidateBlueprintIntegrity(TargetBlueprint, ValidationErrors))
	{
		for (const FString& ValidationError : ValidationErrors)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("Validation: %s"), *ValidationError));
		}
	}

	// Mark Blueprint as modified
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);

	// Compile and save
	if (OutResult.FailedOperations.Num() == 0) // Only compile if no critical failures
	{
		if (!CompileAndSaveBlueprint(TargetBlueprint, true, OutResult))
		{
			OutResult.bSuccess = false;
			OutResult.ErrorMessage = TEXT("Failed to compile or save Blueprint after applying operations");
		}
	}
	else
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = FString::Printf(TEXT("%d operations failed"), OutResult.FailedOperations.Num());
	}

	// Refresh editor if Blueprint is open
	RefreshBlueprintEditor(TargetBlueprint);

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Merge plan application completed. Success: %s, Applied: %d, Failed: %d"),
		OutResult.bSuccess ? TEXT("true") : TEXT("false"),
		OutResult.AppliedOperations.Num(),
		OutResult.FailedOperations.Num());

	return OutResult.bSuccess;
}

bool FApplyEngine::ApplyOperation(
	UBlueprint* TargetBlueprint,
	const FMergeOperation& Operation,
	FString& OutError)
{
	if (!TargetBlueprint)
	{
		OutError = TEXT("Target Blueprint is null");
		return false;
	}

	switch (Operation.OperationType)
	{
	case EMergeOperationType::AddVariable:
		{
			// Parse variable data from additional data
			FString VariableDataJson = Operation.AdditionalData.FindRef(TEXT("VariableData"));
			if (!VariableDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> VariableData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(VariableDataJson);
				if (FJsonSerializer::Deserialize(Reader, VariableData))
				{
					return AddVariable(TargetBlueprint, VariableData, OutError);
				}
			}
			OutError = TEXT("Invalid variable data for AddVariable operation");
			return false;
		}

	case EMergeOperationType::RemoveVariable:
		return RemoveVariable(TargetBlueprint, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateVariable:
		return UpdateVariable(TargetBlueprint, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	case EMergeOperationType::RemapVariableGuid:
		return RemapVariableGuid(TargetBlueprint, Operation.OldValue, Operation.NewValue, OutError);

	case EMergeOperationType::AddNode:
		{
			FString NodeDataJson = Operation.AdditionalData.FindRef(TEXT("NodeData"));
			if (!NodeDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> NodeData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(NodeDataJson);
				if (FJsonSerializer::Deserialize(Reader, NodeData))
				{
					return AddNode(TargetBlueprint, Operation.TargetGraph, NodeData, OutError);
				}
			}
			OutError = TEXT("Invalid node data for AddNode operation");
			return false;
		}

	case EMergeOperationType::AddGraph:
		{
			// One-shot graph creation using payload when Base lacks the function
			FString GraphDataJson = Operation.AdditionalData.FindRef(TEXT("GraphData"));
			if (GraphDataJson.IsEmpty())
			{
				// Backward compatibility: no payload -> treat as no-op for now
				OutError = TEXT("No GraphData payload for AddGraph");
				return false;
			}
			
			// Debug: Log the first 500 characters of the GraphData JSON
			UE_LOG(LogTemp, Log, TEXT("AddGraph: GraphData JSON (first 500 chars): %s"), *GraphDataJson.Left(500));
			
			TSharedPtr<FJsonObject> GraphDataObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GraphDataJson);
			if (!FJsonSerializer::Deserialize(Reader, GraphDataObj) || !GraphDataObj.IsValid())
			{
				OutError = TEXT("Failed to parse GraphData JSON");
				return false;
			}

			const FString GraphType = GraphDataObj->GetStringField(TEXT("GraphType"));
			const FString FunctionName = GraphDataObj->GetStringField(TEXT("FunctionName"));
			const FString GraphName = GraphDataObj->GetStringField(TEXT("GraphName"));

			if (GraphType == TEXT("Function"))
			{
				// Create or find function graph by name
				FName NewFuncName = !FunctionName.IsEmpty() ? FName(*FunctionName) : FName(*GraphName);
				UEdGraph* NewGraph = nullptr;
				if (UFunction* Existing = TargetBlueprint->SkeletonGeneratedClass ? TargetBlueprint->SkeletonGeneratedClass->FindFunctionByName(NewFuncName) : nullptr)
				{
					// Function exists; do nothing here (granular ops will handle differences)
					return true;
				}

				// First, let's check what function signature we need from the payload
				TArray<FBPVariableDescription> InputParams;
				TArray<FBPVariableDescription> OutputParams;
				
				// Parse function entry node to get input parameters
				const TArray<TSharedPtr<FJsonValue>>* NodesArray = nullptr;
				if (GraphDataObj->TryGetArrayField(TEXT("Nodes"), NodesArray) && NodesArray)
				{
					UE_LOG(LogTemp, Log, TEXT("AddGraph: Found %d nodes in payload"), NodesArray->Num());
					for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
					{
						if (NodeValue.IsValid() && NodeValue->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
							FString NodeClass = NodeObj->GetStringField(TEXT("NodeClass"));
							UE_LOG(LogTemp, Log, TEXT("AddGraph: Processing node class: %s"), *NodeClass);
							
							if (NodeClass.Contains(TEXT("K2Node_FunctionEntry")))
							{
								// Parse input parameters from function entry node
								const TArray<TSharedPtr<FJsonValue>>* PinsArray = nullptr;
								if (NodeObj->TryGetArrayField(TEXT("Pins"), PinsArray) && PinsArray)
								{
									for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
									{
										if (PinValue.IsValid() && PinValue->Type == EJson::Object)
										{
											TSharedPtr<FJsonObject> PinObj = PinValue->AsObject();
											FString PinName = PinObj->GetStringField(TEXT("PinName"));
											FString PinCategory = PinObj->GetStringField(TEXT("PinType")); // SnapshotManager uses "PinType"
											FString PinSubCategory = PinObj->GetStringField(TEXT("PinSubCategory")); // SnapshotManager captures this too
											// Only read PinSubCategoryObject if it exists (for struct/object pins)
											FString PinSubCategoryObject;
											if (PinObj->HasField(TEXT("PinSubCategoryObject")))
											{
												PinSubCategoryObject = PinObj->GetStringField(TEXT("PinSubCategoryObject"));
											}
											
											// Skip exec pins, only process data input pins
											if (!PinCategory.IsEmpty() && PinCategory != TEXT("exec"))
											{
												try
												{
													FBPVariableDescription InputParam;
													InputParam.VarName = FName(*PinName);
													InputParam.VarGuid = FGuid::NewGuid();
													
													FEdGraphPinType PinType;
													PinType.PinCategory = FName(*PinCategory);
													if (!PinSubCategory.IsEmpty())
													{
														PinType.PinSubCategory = FName(*PinSubCategory);
													}
													
													// Set PinSubCategoryObject for struct/object pins (like Vector2, Vector3, etc.)
													if (!PinSubCategoryObject.IsEmpty())
													{
														// Find the UObject by name using FindFirstObject (replaces ANY_PACKAGE behavior)
														UObject* SubCategoryObj = FindFirstObject<UObject>(*PinSubCategoryObject);
														
														if (SubCategoryObj)
														{
															PinType.PinSubCategoryObject = SubCategoryObj;
															UE_LOG(LogTemp, Log, TEXT("AddGraph: Found PinSubCategoryObject: %s"), *PinSubCategoryObject);
														}
														else
														{
															UE_LOG(LogTemp, Warning, TEXT("AddGraph: Could not find PinSubCategoryObject: %s"), *PinSubCategoryObject);
														}
													}
													
													InputParam.VarType = PinType;
													
													InputParams.Add(InputParam);
													UE_LOG(LogTemp, Log, TEXT("AddGraph: Found input parameter: %s (%s/%s/%s)"), *PinName, *PinCategory, *PinSubCategory, *PinSubCategoryObject);
												}
												catch (...)
												{
													UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to create input parameter: %s (%s/%s/%s) - caught exception"), *PinName, *PinCategory, *PinSubCategory, *PinSubCategoryObject);
												}
											}
											else
											{
												UE_LOG(LogTemp, Log, TEXT("AddGraph: Skipping pin '%s' - empty or exec category"), *PinName);
											}
										}
									}
								}
							}
							else if (NodeClass.Contains(TEXT("K2Node_FunctionResult")))
							{
								UE_LOG(LogTemp, Log, TEXT("AddGraph: Found K2Node_FunctionResult in payload, parsing pins..."));
								// Parse output parameters from function result node
								const TArray<TSharedPtr<FJsonValue>>* PinsArray = nullptr;
								if (NodeObj->TryGetArrayField(TEXT("Pins"), PinsArray) && PinsArray)
								{
									UE_LOG(LogTemp, Log, TEXT("AddGraph: Found %d pins in function result node"), PinsArray->Num());
									
									// Log all pins first to see what we have
									for (int32 i = 0; i < PinsArray->Num(); i++)
									{
										if (PinsArray->IsValidIndex(i) && (*PinsArray)[i].IsValid() && (*PinsArray)[i]->Type == EJson::Object)
										{
											TSharedPtr<FJsonObject> PinObj = (*PinsArray)[i]->AsObject();
											FString PinName = PinObj->GetStringField(TEXT("PinName"));
											FString PinType = PinObj->GetStringField(TEXT("PinType"));
											FString Direction = PinObj->GetStringField(TEXT("Direction"));
											UE_LOG(LogTemp, Log, TEXT("AddGraph: Pin %d - Name: '%s', Type: '%s', Direction: '%s'"), 
												i, *PinName, *PinType, *Direction);
										}
									}
									for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
									{
										if (PinValue.IsValid() && PinValue->Type == EJson::Object)
										{
											TSharedPtr<FJsonObject> PinObj = PinValue->AsObject();
											FString PinName = PinObj->GetStringField(TEXT("PinName"));
											FString PinCategory = PinObj->GetStringField(TEXT("PinType")); // SnapshotManager uses "PinType"
											FString PinSubCategory = PinObj->GetStringField(TEXT("PinSubCategory")); // SnapshotManager captures this too
											// Only read PinSubCategoryObject if it exists (for struct/object pins)
											FString PinSubCategoryObject;
											if (PinObj->HasField(TEXT("PinSubCategoryObject")))
											{
												PinSubCategoryObject = PinObj->GetStringField(TEXT("PinSubCategoryObject"));
											}
											FString PinDirection = PinObj->GetStringField(TEXT("Direction")); // SnapshotManager uses "Direction"
											
											UE_LOG(LogTemp, Log, TEXT("AddGraph: Function result pin - Name: '%s', Category: '%s', SubCategory: '%s', SubCategoryObject: '%s', Direction: '%s'"), 
												*PinName, *PinCategory, *PinSubCategory, *PinSubCategoryObject, *PinDirection);
											
											// Skip exec pins, only process data output pins
											if (!PinCategory.IsEmpty() && PinCategory != TEXT("exec"))
											{
												try
												{
													FBPVariableDescription OutputParam;
													OutputParam.VarName = FName(*PinName);
													OutputParam.VarGuid = FGuid::NewGuid();
													
													FEdGraphPinType PinType;
													PinType.PinCategory = FName(*PinCategory);
													if (!PinSubCategory.IsEmpty())
													{
														PinType.PinSubCategory = FName(*PinSubCategory);
													}
													
													// Set PinSubCategoryObject for struct/object pins (like Vector2, Vector3, etc.)
													if (!PinSubCategoryObject.IsEmpty())
													{
														// Find the UObject by name using FindFirstObject (replaces ANY_PACKAGE behavior)
														UObject* SubCategoryObj = FindFirstObject<UObject>(*PinSubCategoryObject);
														
														if (SubCategoryObj)
														{
															PinType.PinSubCategoryObject = SubCategoryObj;
															UE_LOG(LogTemp, Log, TEXT("AddGraph: Found PinSubCategoryObject: %s"), *PinSubCategoryObject);
														}
														else
														{
															UE_LOG(LogTemp, Warning, TEXT("AddGraph: Could not find PinSubCategoryObject: %s"), *PinSubCategoryObject);
														}
													}
													
													OutputParam.VarType = PinType;
													
													OutputParams.Add(OutputParam);
													UE_LOG(LogTemp, Log, TEXT("AddGraph: Found output parameter: %s (%s/%s/%s)"), *PinName, *PinCategory, *PinSubCategory, *PinSubCategoryObject);
												}
												catch (...)
												{
													UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to create output parameter: %s (%s/%s/%s) - caught exception"), *PinName, *PinCategory, *PinSubCategory, *PinSubCategoryObject);
												}
											}
											else
											{
												UE_LOG(LogTemp, Log, TEXT("AddGraph: Skipping output pin '%s' - empty or exec category"), *PinName);
											}
										}
									}
								}
								else
								{
									UE_LOG(LogTemp, Warning, TEXT("AddGraph: No pins array found in function result node"));
								}
							}
						}
					}
				}

				// Create a new function graph
				NewGraph = FBlueprintEditorUtils::CreateNewGraph(
					TargetBlueprint,
					NewFuncName,
					UEdGraph::StaticClass(),
					UEdGraphSchema_K2::StaticClass()
				);
				if (!NewGraph)
				{
					OutError = TEXT("Failed to create function graph");
					return false;
				}
				
				// Add the function graph with proper signature
				FBlueprintEditorUtils::AddFunctionGraph<UClass>(TargetBlueprint, NewGraph, /*bIsUserCreated=*/ true, nullptr);
				
				// Set up function signature with input/output parameters
				if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(NewGraph->Nodes[0]))
				{
					// Add input parameters to function entry
					for (const FBPVariableDescription& InputParam : InputParams)
					{
						EntryNode->CreateUserDefinedPin(InputParam.VarName, InputParam.VarType, EGPD_Output);
						UE_LOG(LogTemp, Log, TEXT("AddGraph: Added input parameter '%s' to function entry"), *InputParam.VarName.ToString());
					}
					EntryNode->ReconstructNode();
				}
				
				// Create function result node with output parameters
				UE_LOG(LogTemp, Log, TEXT("AddGraph: Found %d output parameters"), OutputParams.Num());
				
				// Use the original order from JSON payload (should match Blueprint creation order)
				
				// Find the function result node data from payload to get position
				FVector2D ResultNodePosition = FVector2D::ZeroVector;
				if (GraphDataObj->TryGetArrayField(TEXT("Nodes"), NodesArray) && NodesArray)
				{
					for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
					{
						if (NodeValue.IsValid() && NodeValue->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
							FString NodeClass = NodeObj->GetStringField(TEXT("NodeClass"));
							
							if (NodeClass.Contains(TEXT("K2Node_FunctionResult")))
							{
								// Get position from payload
								ResultNodePosition.X = NodeObj->GetNumberField(TEXT("NodePosX"));
								ResultNodePosition.Y = NodeObj->GetNumberField(TEXT("NodePosY"));
								UE_LOG(LogTemp, Log, TEXT("AddGraph: Found function result node position: X=%.0f, Y=%.0f"), ResultNodePosition.X, ResultNodePosition.Y);
								break;
							}
						}
					}
				}
				
				// Always create a function result node since connections expect it
				UK2Node_FunctionResult* ResultNode = NewObject<UK2Node_FunctionResult>(NewGraph);
				if (ResultNode)
				{
					NewGraph->AddNode(ResultNode);
					
					// Set the node position from payload
					ResultNode->NodePosX = ResultNodePosition.X;
					ResultNode->NodePosY = ResultNodePosition.Y;
					UE_LOG(LogTemp, Log, TEXT("AddGraph: Set function result node position: X=%.0f, Y=%.0f"), ResultNodePosition.X, ResultNodePosition.Y);
					
					if (OutputParams.Num() > 0)
					{
						// Add output parameters to function result in original order
						for (const FBPVariableDescription& OutputParam : OutputParams)
						{
							ResultNode->CreateUserDefinedPin(OutputParam.VarName, OutputParam.VarType, EGPD_Input);
							UE_LOG(LogTemp, Log, TEXT("AddGraph: Added output parameter '%s' to function result"), *OutputParam.VarName.ToString());
						}
						UE_LOG(LogTemp, Log, TEXT("AddGraph: Successfully created function result node with %d output parameters"), OutputParams.Num());
					}
					else
					{
						// Create a default "CanPrint" pin since that's what the connections expect
						FEdGraphPinType BoolPinType;
						BoolPinType.PinCategory = FName(TEXT("bool"));
						ResultNode->CreateUserDefinedPin(FName(TEXT("CanPrint")), BoolPinType, EGPD_Input);
						UE_LOG(LogTemp, Log, TEXT("AddGraph: Created function result node with default 'CanPrint' pin"));
					}
					
					ResultNode->ReconstructNode();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to create function result node"));
				}
				
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);


				// Recreate nodes from payload
				if (GraphDataObj->TryGetArrayField(TEXT("Nodes"), NodesArray) && NodesArray)
				{
					for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
					{
						if (NodeValue.IsValid() && NodeValue->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
							FString NodeClass = NodeObj->GetStringField(TEXT("NodeClass"));
							
							// Skip function entry nodes as they're automatically created by AddFunctionGraph
							if (NodeClass.Contains(TEXT("K2Node_FunctionEntry")))
							{
								UE_LOG(LogTemp, Log, TEXT("AddGraph: Skipping %s node (auto-created)"), *NodeClass);
								continue;
							}
							
							// Skip function result nodes as they're now created with proper signature
							if (NodeClass.Contains(TEXT("K2Node_FunctionResult")))
							{
								UE_LOG(LogTemp, Log, TEXT("AddGraph: Skipping %s node (created with function signature)"), *NodeClass);
								continue;
							}
							
							FString NodeError;
							if (!AddNode(TargetBlueprint, NewFuncName.ToString(), NodeObj, NodeError))
							{
								UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to add node to function %s: %s"), *NewFuncName.ToString(), *NodeError);
							}
						}
					}
				}

				// Log all nodes in the graph for debugging
				UE_LOG(LogTemp, Log, TEXT("AddGraph: Current nodes in graph '%s':"), *NewFuncName.ToString());
				for (UEdGraphNode* Node : NewGraph->Nodes)
				{
					if (Node)
					{
						FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
						UE_LOG(LogTemp, Log, TEXT("  Node: %s, Title: '%s'"), *Node->GetName(), *NodeTitle);
					}
				}

				// Recreate connections from payload (after all nodes exist)
				const TArray<TSharedPtr<FJsonValue>>* ConnsArray = nullptr;
				if (GraphDataObj->TryGetArrayField(TEXT("Connections"), ConnsArray) && ConnsArray)
				{
					// Helper to find node object in payload by GUID
					auto FindNodeJsonByGuid = [&](const FString& Guid) -> TSharedPtr<FJsonObject>
					{
						const TArray<TSharedPtr<FJsonValue>>* PayloadNodes = nullptr;
						if (GraphDataObj->TryGetArrayField(TEXT("Nodes"), PayloadNodes) && PayloadNodes)
						{
							for (const TSharedPtr<FJsonValue>& NodeValue : *PayloadNodes)
							{
								if (NodeValue.IsValid() && NodeValue->Type == EJson::Object)
								{
									TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
									if (NodeObj->GetStringField(TEXT("NodeGuid")) == Guid)
									{
										return NodeObj;
									}
								}
							}
						}
						return nullptr;
					};

					// Helper to resolve node by GUID first, then by NodeTitle if GUID lookup fails
					auto ResolveNodeByGuidOrTitle = [&](const FString& Guid, const FString& Title) -> UEdGraphNode*
					{
						UEdGraphNode* Found = FindNodeByGuid(NewGraph, Guid);
						if (Found)
						{
							UE_LOG(LogTemp, Log, TEXT("AddGraph: Found node by GUID: %s"), *Found->GetName());
							return Found;
						}
						
						// Fallback to title-based matching when GUIDs are unstable
						UE_LOG(LogTemp, Log, TEXT("AddGraph: GUID lookup failed for %s, trying title-based matching: '%s'"), *Guid, *Title);
						
						for (UEdGraphNode* Node : NewGraph->Nodes)
						{
							if (Node)
							{
								FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
								UE_LOG(LogTemp, Log, TEXT("AddGraph: Checking node '%s' with title '%s'"), *Node->GetName(), *NodeTitle);
								
								if (NodeTitle == Title)
								{
									UE_LOG(LogTemp, Log, TEXT("AddGraph: Found node by title: %s"), *Node->GetName());
									return Node;
								}
							}
						}
						
						UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to resolve node with GUID %s or title '%s'"), *Guid, *Title);
						return nullptr;
					};

					for (const TSharedPtr<FJsonValue>& ConnValue : *ConnsArray)
					{
						if (ConnValue.IsValid() && ConnValue->Type == EJson::Object)
						{
							TSharedPtr<FJsonObject> ConnObj = ConnValue->AsObject();
							FString SourceNodeGuid = ConnObj->GetStringField(TEXT("SourceNodeGuid"));
							FString SourcePinName = ConnObj->GetStringField(TEXT("SourcePinName"));
							FString TargetNodeGuid = ConnObj->GetStringField(TEXT("TargetNodeGuid"));
							FString TargetPinName = ConnObj->GetStringField(TEXT("TargetPinName"));

#if BPT_MERGE_USE_GUID_MATCHING
							FString LinkError;
							if (!LinkPins(TargetBlueprint, NewFuncName.ToString(), SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName, LinkError))
							{
								UE_LOG(LogTemp, Warning, TEXT("AddGraph: Failed to link pins in function %s: %s -> %s (%s)"), *NewFuncName.ToString(), *SourcePinName, *TargetPinName, *LinkError);
							}
#else
							// Name/semantic-based fallback
							TSharedPtr<FJsonObject> SrcNodeJson = FindNodeJsonByGuid(SourceNodeGuid);
							TSharedPtr<FJsonObject> DstNodeJson = FindNodeJsonByGuid(TargetNodeGuid);
							FString SrcTitle = SrcNodeJson.IsValid() ? SrcNodeJson->GetStringField(TEXT("NodeTitle")) : TEXT("");
							FString DstTitle = DstNodeJson.IsValid() ? DstNodeJson->GetStringField(TEXT("NodeTitle")) : TEXT("");

							UEdGraphNode* SrcNode = ResolveNodeByGuidOrTitle(SourceNodeGuid, SrcTitle);
							UEdGraphNode* DstNode = ResolveNodeByGuidOrTitle(TargetNodeGuid, DstTitle);

							if (!SrcNode || !DstNode)
							{
								UE_LOG(LogTemp, Warning, TEXT("AddGraph: Fallback link failed to resolve nodes. Src(%s:%s) Dst(%s:%s)"), *SourceNodeGuid, *SrcTitle, *TargetNodeGuid, *DstTitle);
								continue;
							}

							UEdGraphPin* SourcePin = FindPin(SrcNode, TEXT(""), SourcePinName);
							UEdGraphPin* TargetPin = FindPin(DstNode, TEXT(""), TargetPinName);

							if (!SourcePin || !TargetPin)
							{
								// Fallback: normalize names (remove spaces/underscores, lowercase) and retry
								auto Normalize = [](const FString& In) -> FString
								{
									FString S = In;
									S.ReplaceInline(TEXT(" "), TEXT(""));
									S.ReplaceInline(TEXT("_"), TEXT(""));
									return S.ToLower();
								};
								FString SrcNorm = Normalize(SourcePinName);
								FString DstNorm = Normalize(TargetPinName);

								auto FindPinNormalized = [&](UEdGraphNode* Node, const FString& WantNorm) -> UEdGraphPin*
								{
									for (UEdGraphPin* P : Node->Pins)
									{
										if (!P) { continue; }
										if (Normalize(P->PinName.ToString()) == WantNorm)
										{
											return P;
										}
									}
									return (UEdGraphPin*)nullptr;
								};

								if (!SourcePin)
								{
									SourcePin = FindPinNormalized(SrcNode, SrcNorm);
								}
								if (!TargetPin)
								{
									TargetPin = FindPinNormalized(DstNode, DstNorm);
								}

								if (!SourcePin || !TargetPin)
								{
									OutError = FString::Printf(TEXT("Source pin '%s' or target pin '%s' not found (after normalized fallback)"), *SourcePinName, *TargetPinName);
									return false;
								}
							}

							if (NewGraph->GetSchema()->CanCreateConnection(SourcePin, TargetPin).Response == CONNECT_RESPONSE_MAKE)
							{
								SourcePin->MakeLinkTo(TargetPin);
							}
							else
							{
								UE_LOG(LogTemp, Warning, TEXT("AddGraph: Fallback cannot connect pins %s -> %s (incompatible)"), *SourcePinName, *TargetPinName);
							}
#endif
						}
					}
				}

				return true;
			}

			// Non-function graphs not supported yet in one-shot path
			OutError = TEXT("AddGraph only supports Function graphs currently");
			return false;
		}

	case EMergeOperationType::RemoveNode:
		return RemoveNode(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateNodeProperty:
		return UpdateNodeProperty(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	case EMergeOperationType::MoveNode:
		{
			// Parse position from NewValue
			TArray<FString> Coords;
			Operation.NewValue.ParseIntoArray(Coords, TEXT(","), true);
			if (Coords.Num() >= 2)
			{
				float NewX = FCString::Atof(*Coords[0]);
				float NewY = FCString::Atof(*Coords[1]);
				return MoveNode(TargetBlueprint, Operation.TargetGraph, Operation.TargetId, NewX, NewY, OutError);
			}
			OutError = TEXT("Invalid position data for MoveNode operation");
			return false;
		}

	case EMergeOperationType::LinkPins:
		{
			// Parse connection data from TargetId (format: "SourceGuid.SourcePin->TargetGuid.TargetPin")
			FString SourcePart, TargetPart;
			if (Operation.TargetId.Split(TEXT("->"), &SourcePart, &TargetPart))
			{
				FString SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName;
				if (SourcePart.Split(TEXT("."), &SourceNodeGuid, &SourcePinName) &&
					TargetPart.Split(TEXT("."), &TargetNodeGuid, &TargetPinName))
				{
					return LinkPins(TargetBlueprint, Operation.TargetGraph, SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName, OutError);
				}
			}
			OutError = TEXT("Invalid connection format for LinkPins operation");
			return false;
		}

	case EMergeOperationType::UnlinkPins:
		{
			// Parse connection data similar to LinkPins
			FString SourcePart, TargetPart;
			if (Operation.TargetId.Split(TEXT("->"), &SourcePart, &TargetPart))
			{
				FString SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName;
				if (SourcePart.Split(TEXT("."), &SourceNodeGuid, &SourcePinName) &&
					TargetPart.Split(TEXT("."), &TargetNodeGuid, &TargetPinName))
				{
					return UnlinkPins(TargetBlueprint, Operation.TargetGraph, SourceNodeGuid, SourcePinName, TargetNodeGuid, TargetPinName, OutError);
				}
			}
			OutError = TEXT("Invalid connection format for UnlinkPins operation");
			return false;
		}

	case EMergeOperationType::AddComponent:
		{
			FString ComponentDataJson = Operation.AdditionalData.FindRef(TEXT("ComponentData"));
			if (!ComponentDataJson.IsEmpty())
			{
				TSharedPtr<FJsonObject> ComponentData;
				TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ComponentDataJson);
				if (FJsonSerializer::Deserialize(Reader, ComponentData))
				{
					return AddComponent(TargetBlueprint, ComponentData, OutError);
				}
			}
			OutError = TEXT("Invalid component data for AddComponent operation");
			return false;
		}

	case EMergeOperationType::RemoveComponent:
		return RemoveComponent(TargetBlueprint, Operation.TargetId, OutError);

	case EMergeOperationType::UpdateComponent:
		return UpdateComponent(TargetBlueprint, Operation.TargetId, Operation.PropertyName, Operation.NewValue, OutError);

	default:
		OutError = FString::Printf(TEXT("Unsupported operation type: %d"), (int32)Operation.OperationType);
		return false;
	}
}

bool FApplyEngine::ValidateBlueprintIntegrity(
	UBlueprint* Blueprint,
	TArray<FString>& OutValidationErrors)
{
	OutValidationErrors.Empty();

	if (!Blueprint)
	{
		OutValidationErrors.Add(TEXT("Blueprint is null"));
		return false;
	}

	// Check for duplicate variable names/GUIDs
	TSet<FString> VariableNames;
	TSet<FString> VariableGuids;

	for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		FString VarName = Variable.VarName.ToString();
		FString VarGuid = Variable.VarGuid.ToString();

		if (VariableNames.Contains(VarName))
		{
			OutValidationErrors.Add(FString::Printf(TEXT("Duplicate variable name: %s"), *VarName));
		}
		VariableNames.Add(VarName);

		if (!VarGuid.IsEmpty() && VariableGuids.Contains(VarGuid))
		{
			OutValidationErrors.Add(FString::Printf(TEXT("Duplicate variable GUID: %s"), *VarGuid));
		}
		VariableGuids.Add(VarGuid);
	}

	// Check graph integrity
	TArray<UEdGraph*> AllGraphs;
	FSnapshotManager::GetAllBlueprintGraphs(Blueprint, AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph)
		{
			OutValidationErrors.Add(TEXT("Found null graph in Blueprint"));
			continue;
		}

		// Check for orphaned nodes
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Found null node in graph: %s"), *Graph->GetName()));
				continue;
			}

			// Check for broken pin connections
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin)
				{
					OutValidationErrors.Add(FString::Printf(TEXT("Found null pin in node: %s"), *Node->GetName()));
					continue;
				}

				// Check if linked pins still exist
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (!LinkedPin || !IsValid(LinkedPin->GetOwningNode()))
					{
						OutValidationErrors.Add(FString::Printf(TEXT("Broken pin connection in node: %s, pin: %s"), 
							*Node->GetName(), *Pin->PinName.ToString()));
					}
				}
			}
		}
	}

	// Check component hierarchy integrity
	if (Blueprint->SimpleConstructionScript)
	{
		const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
		TSet<FString> ComponentNames;

		for (USCS_Node* Node : AllNodes)
		{
			if (!Node)
			{
				OutValidationErrors.Add(TEXT("Found null component node"));
				continue;
			}

			FString CompName = Node->GetVariableName().ToString();
			if (ComponentNames.Contains(CompName))
			{
				OutValidationErrors.Add(FString::Printf(TEXT("Duplicate component name: %s"), *CompName));
			}
			ComponentNames.Add(CompName);
		}
	}

	return OutValidationErrors.Num() == 0;
}

bool FApplyEngine::CompileAndSaveBlueprint(
	UBlueprint* Blueprint,
	bool bForceCompile,
	FApplyResult& OutResult)
{
	if (!Blueprint)
	{
		OutResult.ErrorMessage = TEXT("Blueprint is null");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Compiling and saving Blueprint: %s"), *Blueprint->GetName());

	// Compile the Blueprint
	if (bForceCompile || FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint))
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
		
		// Check for compilation errors
		if (Blueprint->Status == BS_Error)
		{
			OutResult.ErrorMessage = TEXT("Blueprint compilation failed with errors");
			OutResult.bBlueprintCompiled = false;
			UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Blueprint compilation failed"));
			return false;
		}
		else if (Blueprint->Status == BS_UpToDateWithWarnings)
		{
			OutResult.Warnings.Add(TEXT("Blueprint compiled with warnings"));
			UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Blueprint compiled with warnings"));
		}

		OutResult.bBlueprintCompiled = true;
	}

	// Save the Blueprint
	UPackage* Package = Blueprint->GetPackage();
	if (Package)
	{
		Package->SetDirtyFlag(true);
		
		// Use UEditorAssetSubsystem for proper saving
		if (UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>())
		{
			if (EditorAssetSubsystem->SaveAsset(Blueprint->GetPathName()))
			{
				OutResult.bBlueprintSaved = true;
				UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Blueprint saved successfully"));
			}
			else
			{
				OutResult.ErrorMessage = TEXT("Failed to save Blueprint");
				OutResult.bBlueprintSaved = false;
				UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Failed to save Blueprint"));
				return false;
			}
		}
		else
		{
			OutResult.ErrorMessage = TEXT("Could not get EditorAssetSubsystem");
			return false;
		}
	}
	else
	{
		OutResult.ErrorMessage = TEXT("Blueprint package is null");
		return false;
	}

	return true;
}

bool FApplyEngine::AddVariable(
	UBlueprint* Blueprint,
	TSharedPtr<FJsonObject> VariableData,
	FString& OutError)
{
	if (!Blueprint || !VariableData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddVariable");
		return false;
	}

	FString VarName = VariableData->GetStringField(TEXT("VariableName"));
	FString VarGuidStr = VariableData->GetStringField(TEXT("VariableGuid"));
	FString VarType = VariableData->GetStringField(TEXT("VarType"));

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: AddVariable - Name: %s, GUID: %s, Type: %s"), *VarName, *VarGuidStr, *VarType);

	if (VarName.IsEmpty())
	{
		OutError = TEXT("Variable name is empty");
		return false;
	}

	// Check if variable already exists (name-based conflict detection)
	for (const FBPVariableDescription& ExistingVar : Blueprint->NewVariables)
	{
		if (ExistingVar.VarName.ToString() == VarName)
		{
			OutError = FString::Printf(TEXT("Variable '%s' already exists"), *VarName);
			return false;
		}
	}

	// Create new variable description
	FBPVariableDescription NewVariable;
	NewVariable.VarName = FName(*VarName);
	
	// Parse GUID
	if (!VarGuidStr.IsEmpty())
	{
		FGuid::Parse(VarGuidStr, NewVariable.VarGuid);
	}
	if (!NewVariable.VarGuid.IsValid())
	{
		NewVariable.VarGuid = FGuid::NewGuid();
	}

	// Get subcategory from the captured data
	FString VarSubCategory = VariableData->GetStringField(TEXT("VarSubCategory"));
	
	// Only read VarSubCategoryObject if it exists (for struct/object variables)
	FString VarSubCategoryObject;
	if (VariableData->HasField(TEXT("VarSubCategoryObject")))
	{
		VarSubCategoryObject = VariableData->GetStringField(TEXT("VarSubCategoryObject"));
	}
	
	// Set variable type with proper subcategory
	// Handle both user-friendly names and Unreal Engine pin categories
	if (VarType == TEXT("int") || VarType == TEXT("Int") || VarType == TEXT("PC_Int"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Int;
		NewVariable.VarType.PinSubCategory = NAME_None;
	}
	else if (VarType == TEXT("float") || VarType == TEXT("Float") || VarType == TEXT("PC_Real"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Real;
		NewVariable.VarType.PinSubCategory = VarSubCategory == TEXT("PC_Float") ? UEdGraphSchema_K2::PC_Float : NAME_None;
	}
	else if (VarType == TEXT("bool") || VarType == TEXT("Boolean") || VarType == TEXT("PC_Boolean"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		NewVariable.VarType.PinSubCategory = NAME_None;
	}
	else if (VarType == TEXT("string") || VarType == TEXT("String") || VarType == TEXT("PC_String"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_String;
		NewVariable.VarType.PinSubCategory = NAME_None;
	}
	else if (VarType == TEXT("object") || VarType == TEXT("Object") || VarType == TEXT("PC_Object"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Object;
		NewVariable.VarType.PinSubCategory = NAME_None;
		// PinSubCategoryObject will be set separately if provided
	}
	else if (VarType == TEXT("class") || VarType == TEXT("Class") || VarType == TEXT("PC_Class"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Class;
		NewVariable.VarType.PinSubCategory = NAME_None;
		// PinSubCategoryObject will be set separately if provided
	}
	else if (VarType == TEXT("struct") || VarType == TEXT("Struct") || VarType == TEXT("PC_Struct"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		NewVariable.VarType.PinSubCategory = NAME_None;
		// PinSubCategoryObject will be set separately if provided
	}
	else if (VarType == TEXT("interface") || VarType == TEXT("Interface") || VarType == TEXT("PC_Interface"))
	{
		NewVariable.VarType.PinCategory = UEdGraphSchema_K2::PC_Interface;
		NewVariable.VarType.PinSubCategory = NAME_None;
		// PinSubCategoryObject will be set separately if provided
	}
	else
	{
		// Try to use the captured pin category directly
		NewVariable.VarType.PinCategory = FName(*VarType);
		NewVariable.VarType.PinSubCategory = VarSubCategory.IsEmpty() ? NAME_None : FName(*VarSubCategory);
		
		UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Using captured pin type - Category: %s, SubCategory: %s"), *VarType, *VarSubCategory);
	}

	// Set PinSubCategoryObject for struct/object/class/interface pins
	if (!VarSubCategoryObject.IsEmpty())
	{
		// Find the UObject by name using FindFirstObject (replaces ANY_PACKAGE behavior)
		UObject* SubCategoryObj = FindFirstObject<UObject>(*VarSubCategoryObject);
		
		if (SubCategoryObj)
		{
			NewVariable.VarType.PinSubCategoryObject = SubCategoryObj;
			UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Set PinSubCategoryObject to: %s"), *VarSubCategoryObject);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Could not find VarSubCategoryObject: %s"), *VarSubCategoryObject);
		}
	}

	// Set other properties
	NewVariable.Category = FText::FromString(VariableData->GetStringField(TEXT("Category")));
	NewVariable.FriendlyName = VariableData->GetStringField(TEXT("FriendlyName"));
	
	// Safely set tooltip metadata
	FString TooltipValue = VariableData->GetStringField(TEXT("Tooltip"));
	if (!TooltipValue.IsEmpty())
	{
		int32 TooltipIndex = NewVariable.FindMetaDataEntryIndexForKey(TEXT("Tooltip"));
		if (TooltipIndex == INDEX_NONE)
		{
			// Add new tooltip metadata entry
			FBPVariableMetaDataEntry TooltipEntry;
			TooltipEntry.DataKey = TEXT("Tooltip");
			TooltipEntry.DataValue = TooltipValue;
			NewVariable.MetaDataArray.Add(TooltipEntry);
		}
		else
		{
			// Update existing tooltip metadata entry
			NewVariable.MetaDataArray[TooltipIndex].DataValue = TooltipValue;
		}
	}
	
	NewVariable.DefaultValue = VariableData->GetStringField(TEXT("DefaultValue"));

	// Set flags
	if (VariableData->GetBoolField(TEXT("bExposeOnSpawn")))
		NewVariable.PropertyFlags |= CPF_ExposeOnSpawn;
	if (VariableData->GetBoolField(TEXT("bBlueprintReadOnly")))
		NewVariable.PropertyFlags |= CPF_BlueprintReadOnly;
	if (VariableData->GetBoolField(TEXT("bInstanceEditable")))
		NewVariable.PropertyFlags |= CPF_Edit;
	if (VariableData->GetBoolField(TEXT("bBlueprintVisible")))
		NewVariable.PropertyFlags |= CPF_BlueprintVisible;

	// Add the variable directly to the Blueprint's NewVariables array
	Blueprint->NewVariables.Add(NewVariable);
	
	// Mark the Blueprint as modified
	Blueprint->Modify();
	
	// Compile the Blueprint to ensure the variable is properly integrated
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Successfully added variable: %s (%s)"), *VarName, *VarType);
	return true;
}

bool FApplyEngine::RemoveVariable(
	UBlueprint* Blueprint,
	const FString& VariableGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FBPVariableDescription* Variable = FindVariableByGuid(Blueprint, VariableGuid);
	if (!Variable)
	{
		OutError = FString::Printf(TEXT("Variable with GUID %s not found"), *VariableGuid);
		return false;
	}

	FName VarName = Variable->VarName;

	// Remove the variable using Blueprint editor utilities
	FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VarName);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed variable: %s"), *VarName.ToString());
	return true;
}

bool FApplyEngine::UpdateVariable(
	UBlueprint* Blueprint,
	const FString& VariableGuid,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FBPVariableDescription* Variable = FindVariableByGuid(Blueprint, VariableGuid);
	if (!Variable)
	{
		OutError = FString::Printf(TEXT("Variable with GUID %s not found"), *VariableGuid);
		return false;
	}

	// Update the specified property
	if (PropertyName == TEXT("DefaultValue"))
	{
		Variable->DefaultValue = NewValue;
	}
	else if (PropertyName == TEXT("Category"))
	{
		Variable->Category = FText::FromString(NewValue);
	}
	else if (PropertyName == TEXT("FriendlyName"))
	{
		Variable->FriendlyName = NewValue;
	}
	else if (PropertyName == TEXT("Tooltip"))
	{
		Variable->MetaDataArray[Variable->FindMetaDataEntryIndexForKey(TEXT("Tooltip"))].DataValue = NewValue;
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown variable property: %s"), *PropertyName);
		return false;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated variable %s property %s = %s"), 
		*Variable->VarName.ToString(), *PropertyName, *NewValue);
	return true;
}

bool FApplyEngine::RemapVariableGuid(
	UBlueprint* Blueprint,
	const FString& OldGuid,
	const FString& NewGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FGuid OldGuidParsed, NewGuidParsed;
	if (!FGuid::Parse(OldGuid, OldGuidParsed) || !FGuid::Parse(NewGuid, NewGuidParsed))
	{
		OutError = TEXT("Invalid GUID format for remapping");
		return false;
	}

	// Find and update the variable GUID
	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarGuid == OldGuidParsed)
		{
			Variable.VarGuid = NewGuidParsed;
			
			// Update all references to this variable
			if (!UpdateVariableReferences(Blueprint, OldGuid, NewGuid, OutError))
			{
				return false;
			}

			UE_LOG(LogTemp, VeryVerbose, TEXT("Remapped variable GUID: %s -> %s"), *OldGuid, *NewGuid);
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Variable with GUID %s not found for remapping"), *OldGuid);
	return false;
}

bool FApplyEngine::AddNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	TSharedPtr<FJsonObject> NodeData,
	FString& OutError)
{
	if (!Blueprint || !NodeData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddNode");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* NewNode = CreateNodeFromData(TargetGraph, NodeData, OutError);
	if (!NewNode)
	{
		return false;
	}

	TargetGraph->AddNode(NewNode, true);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Added node: %s to graph: %s"), *NewNode->GetName(), *GraphName);
	return true;
}

bool FApplyEngine::RemoveNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* NodeToRemove = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!NodeToRemove)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found in graph %s"), *NodeGuid, *GraphName);
		return false;
	}

	// Break all connections first
	for (UEdGraphPin* Pin : NodeToRemove->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks();
		}
	}

	// Remove the node
	TargetGraph->RemoveNode(NodeToRemove);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed node: %s from graph: %s"), *NodeGuid, *GraphName);
	return true;
}

bool FApplyEngine::UpdateNodeProperty(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found"), *NodeGuid);
		return false;
	}

	// Update specific properties based on node type and property name
	if (PropertyName == TEXT("NodeComment"))
	{
		TargetNode->NodeComment = NewValue;
	}
	else if (PropertyName == TEXT("NodeTitle"))
	{
		// Node title is usually read-only, but we can update the underlying data for some node types
		if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(TargetNode))
		{
			CustomEvent->CustomFunctionName = FName(*NewValue);
			CustomEvent->ReconstructNode();
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Unknown node property: %s"), *PropertyName);
		return false;
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated node %s property %s = %s"), *NodeGuid, *PropertyName, *NewValue);
	return true;
}

bool FApplyEngine::MoveNode(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& NodeGuid,
	float NewX,
	float NewY,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, NodeGuid);
	if (!TargetNode)
	{
		OutError = FString::Printf(TEXT("Node with GUID %s not found"), *NodeGuid);
		return false;
	}

	TargetNode->NodePosX = NewX;
	TargetNode->NodePosY = NewY;

	UE_LOG(LogTemp, VeryVerbose, TEXT("Moved node %s to (%.0f, %.0f)"), *NodeGuid, NewX, NewY);
	return true;
}

bool FApplyEngine::LinkPins(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& SourceNodeGuid,
	const FString& SourcePinName,
	const FString& TargetNodeGuid,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeByGuid(TargetGraph, SourceNodeGuid);
	UEdGraphNode* TargetNode = FindNodeByGuid(TargetGraph, TargetNodeGuid);

	// Debug logging
	UE_LOG(LogTemp, Log, TEXT("LinkPins: Looking for nodes - Source: %s, Target: %s"), *SourceNodeGuid, *TargetNodeGuid);
	UE_LOG(LogTemp, Log, TEXT("LinkPins: Found nodes - Source: %s, Target: %s"), 
		SourceNode ? *SourceNode->GetName() : TEXT("NULL"), 
		TargetNode ? *TargetNode->GetName() : TEXT("NULL"));

	if (!SourceNode || !TargetNode)
	{
		// List all available nodes for debugging
		UE_LOG(LogTemp, Warning, TEXT("LinkPins: Available nodes in graph '%s':"), *GraphName);
		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (UK2Node* K2Node = Cast<UK2Node>(Node))
			{
				UE_LOG(LogTemp, Warning, TEXT("  Node: %s, GUID: %s"), *Node->GetName(), *K2Node->NodeGuid.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("  Node: %s (not a K2Node)"), *Node->GetName());
			}
		}
		
		OutError = TEXT("Source or target node not found for pin linking");
		return false;
	}

	UEdGraphPin* SourcePin = FindPin(SourceNode, TEXT(""), SourcePinName);
	UEdGraphPin* TargetPin = FindPin(TargetNode, TEXT(""), TargetPinName);

	if (!SourcePin || !TargetPin)
	{
		// Fallback: Try to find pins by normalized names (remove spaces/underscores, lowercase)
		FString NormalizedSourcePinName = SourcePinName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();
		FString NormalizedTargetPinName = TargetPinName.Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();

		UE_LOG(LogTemp, Log, TEXT("LinkPins: Trying normalized fallback - Source: '%s' -> '%s', Target: '%s' -> '%s'"), 
			*SourcePinName, *NormalizedSourcePinName, *TargetPinName, *NormalizedTargetPinName);

		for (UEdGraphPin* Pin : SourceNode->Pins)
		{
			FString NormalizedExistingPinName = Pin->PinName.ToString().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();
			if (NormalizedExistingPinName == NormalizedSourcePinName)
			{
				SourcePin = Pin;
				UE_LOG(LogTemp, Log, TEXT("LinkPins: Found source pin with normalized name: '%s' -> '%s'"), *Pin->PinName.ToString(), *NormalizedExistingPinName);
				break;
			}
		}

		for (UEdGraphPin* Pin : TargetNode->Pins)
		{
			FString NormalizedExistingPinName = Pin->PinName.ToString().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower();
			if (NormalizedExistingPinName == NormalizedTargetPinName)
			{
				TargetPin = Pin;
				UE_LOG(LogTemp, Log, TEXT("LinkPins: Found target pin with normalized name: '%s' -> '%s'"), *Pin->PinName.ToString(), *NormalizedExistingPinName);
				break;
			}
		}

		if (!SourcePin || !TargetPin)
		{
			// List all available pins for debugging
			UE_LOG(LogTemp, Warning, TEXT("LinkPins: Available pins on source node '%s':"), *SourceNode->GetName());
			for (UEdGraphPin* Pin : SourceNode->Pins)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Pin: '%s' (normalized: '%s')"), *Pin->PinName.ToString(), 
					*Pin->PinName.ToString().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower());
			}
			UE_LOG(LogTemp, Warning, TEXT("LinkPins: Available pins on target node '%s':"), *TargetNode->GetName());
			for (UEdGraphPin* Pin : TargetNode->Pins)
			{
				UE_LOG(LogTemp, Warning, TEXT("  Pin: '%s' (normalized: '%s')"), *Pin->PinName.ToString(), 
					*Pin->PinName.ToString().Replace(TEXT(" "), TEXT("")).Replace(TEXT("_"), TEXT("")).ToLower());
			}
			
			OutError = FString::Printf(TEXT("Source pin '%s' or target pin '%s' not found (after normalized fallback)"), *SourcePinName, *TargetPinName);
			return false;
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("LinkPins: Successfully found pins with normalized names: %s -> %s"), *SourcePinName, *TargetPinName);
		}
	}

	// Check compatibility
	if (TargetGraph->GetSchema()->CanCreateConnection(SourcePin, TargetPin).Response != CONNECT_RESPONSE_MAKE)
	{
		OutError = FString::Printf(TEXT("Cannot connect pins '%s' to '%s' - incompatible types"), *SourcePinName, *TargetPinName);
		return false;
	}

	// Make the connection
	SourcePin->MakeLinkTo(TargetPin);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Linked pins: %s.%s -> %s.%s"), 
		*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
	return true;
}

bool FApplyEngine::UnlinkPins(
	UBlueprint* Blueprint,
	const FString& GraphName,
	const FString& SourceNodeGuid,
	const FString& SourcePinName,
	const FString& TargetNodeGuid,
	const FString& TargetPinName,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph)
	{
		OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
		return false;
	}

	UEdGraphNode* SourceNode = FindNodeByGuid(TargetGraph, SourceNodeGuid);
	if (!SourceNode)
	{
		OutError = FString::Printf(TEXT("Source node %s not found"), *SourceNodeGuid);
		return false;
	}

	UEdGraphPin* SourcePin = FindPin(SourceNode, TEXT(""), SourcePinName);
	if (!SourcePin)
	{
		OutError = FString::Printf(TEXT("Source pin '%s' not found"), *SourcePinName);
		return false;
	}

	// Find the specific connection to break
	for (UEdGraphPin* LinkedPin : SourcePin->LinkedTo)
	{
		if (LinkedPin && FindPin(LinkedPin->GetOwningNode(), TEXT(""), TargetPinName) == LinkedPin)
		{
			SourcePin->BreakLinkTo(LinkedPin);
			UE_LOG(LogTemp, VeryVerbose, TEXT("Unlinked pins: %s.%s -> %s.%s"), 
				*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Connection between %s.%s and %s.%s not found"), 
		*SourceNodeGuid, *SourcePinName, *TargetNodeGuid, *TargetPinName);
	return false;
}

bool FApplyEngine::AddComponent(
	UBlueprint* Blueprint,
	TSharedPtr<FJsonObject> ComponentData,
	FString& OutError)
{
	if (!Blueprint || !ComponentData.IsValid())
	{
		OutError = TEXT("Invalid parameters for AddComponent");
		return false;
	}

	if (!Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Blueprint has no SimpleConstructionScript");
		return false;
	}

	FString ComponentName = ComponentData->GetStringField(TEXT("ComponentName"));
	FString ComponentClassName = ComponentData->GetStringField(TEXT("ComponentClass"));

	if (ComponentName.IsEmpty() || ComponentClassName.IsEmpty())
	{
		OutError = TEXT("Component name or class is empty");
		return false;
	}

	// Find the component class
	UClass* ComponentClass = FindObject<UClass>(nullptr, *ComponentClassName);
	if (!ComponentClass)
	{
		// Try common component classes
		if (ComponentClassName.Contains(TEXT("StaticMesh")))
		{
			ComponentClass = UStaticMeshComponent::StaticClass();
		}
		else if (ComponentClassName.Contains(TEXT("Scene")))
		{
			ComponentClass = USceneComponent::StaticClass();
		}
		else
		{
			ComponentClass = UActorComponent::StaticClass(); // Fallback
		}
	}

	// Create the component node
	USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, FName(*ComponentName));
	if (!NewNode)
	{
		OutError = FString::Printf(TEXT("Failed to create component node for %s"), *ComponentName);
		return false;
	}

	// Set attachment if specified
	FString AttachToComponent = ComponentData->GetStringField(TEXT("AttachToComponent"));
	if (!AttachToComponent.IsEmpty())
	{
		USCS_Node* ParentNode = nullptr;
		for (USCS_Node* ExistingNode : Blueprint->SimpleConstructionScript->GetAllNodes())
		{
			if (ExistingNode && ExistingNode->GetVariableName().ToString() == AttachToComponent)
			{
				ParentNode = ExistingNode;
				break;
			}
		}

		if (ParentNode)
		{
			ParentNode->AddChildNode(NewNode);
		}
		else
		{
			// Attach to root if parent not found
			Blueprint->SimpleConstructionScript->GetDefaultSceneRootNode()->AddChildNode(NewNode);
		}
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Added component: %s (%s)"), *ComponentName, *ComponentClassName);
	return true;
}

bool FApplyEngine::RemoveComponent(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	FString& OutError)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Invalid Blueprint or SimpleConstructionScript");
		return false;
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		OutError = FString::Printf(TEXT("Component '%s' not found"), *ComponentName);
		return false;
	}

	// Remove the component
	Blueprint->SimpleConstructionScript->RemoveNode(ComponentNode);

	UE_LOG(LogTemp, VeryVerbose, TEXT("Removed component: %s"), *ComponentName);
	return true;
}

bool FApplyEngine::UpdateComponent(
	UBlueprint* Blueprint,
	const FString& ComponentName,
	const FString& PropertyName,
	const FString& NewValue,
	FString& OutError)
{
	if (!Blueprint || !Blueprint->SimpleConstructionScript)
	{
		OutError = TEXT("Invalid Blueprint or SimpleConstructionScript");
		return false;
	}

	// Find the component node
	USCS_Node* ComponentNode = nullptr;
	for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
	{
		if (Node && Node->GetVariableName().ToString() == ComponentName)
		{
			ComponentNode = Node;
			break;
		}
	}

	if (!ComponentNode)
	{
		OutError = FString::Printf(TEXT("Component '%s' not found"), *ComponentName);
		return false;
	}

	// Update properties based on property name
	if (PropertyName == TEXT("RelativeLocation"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FVector Location;
			if (Location.InitFromString(NewValue))
			{
				SceneComp->SetRelativeLocation(Location);
			}
		}
	}
	else if (PropertyName == TEXT("RelativeRotation"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FRotator Rotation;
			if (Rotation.InitFromString(NewValue))
			{
				SceneComp->SetRelativeRotation(Rotation);
			}
		}
	}
	else if (PropertyName == TEXT("RelativeScale"))
	{
		if (USceneComponent* SceneComp = Cast<USceneComponent>(ComponentNode->ComponentTemplate))
		{
			FVector Scale;
			if (Scale.InitFromString(NewValue))
			{
				SceneComp->SetRelativeScale3D(Scale);
			}
		}
	}

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated component %s property %s = %s"), *ComponentName, *PropertyName, *NewValue);
	return true;
}

UEdGraph* FApplyEngine::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> AllGraphs;
	FSnapshotManager::GetAllBlueprintGraphs(Blueprint, AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	return nullptr;
}

UEdGraphNode* FApplyEngine::FindNodeByGuid(UEdGraph* Graph, const FString& NodeGuid)
{
	if (!Graph)
	{
		return nullptr;
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(NodeGuid, TargetGuid))
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node* K2Node = Cast<UK2Node>(Node))
		{
			if (K2Node->NodeGuid == TargetGuid)
			{
				return Node;
			}
		}
	}

	return nullptr;
}

UEdGraphPin* FApplyEngine::FindPin(UEdGraphNode* Node, const FString& PinId, const FString& PinName)
{
	if (!Node)
	{
		return nullptr;
	}

	// Try to find by PinId first
	if (!PinId.IsEmpty())
	{
		FGuid TargetPinId;
		if (FGuid::Parse(PinId, TargetPinId))
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin && Pin->PinId == TargetPinId)
				{
					return Pin;
				}
			}
		}
	}

	// Fallback to name
	if (!PinName.IsEmpty())
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString() == PinName)
			{
				return Pin;
			}
		}
	}

	return nullptr;
}

FBPVariableDescription* FApplyEngine::FindVariableByGuid(UBlueprint* Blueprint, const FString& VariableGuid)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	FGuid TargetGuid;
	if (!FGuid::Parse(VariableGuid, TargetGuid))
	{
		return nullptr;
	}

	for (FBPVariableDescription& Variable : Blueprint->NewVariables)
	{
		if (Variable.VarGuid == TargetGuid)
		{
			return &Variable;
		}
	}

	return nullptr;
}

UEdGraphNode* FApplyEngine::CreateNodeFromData(
	UEdGraph* Graph,
	TSharedPtr<FJsonObject> NodeData,
	FString& OutError)
{
	if (!Graph || !NodeData.IsValid())
	{
		OutError = TEXT("Invalid parameters for CreateNodeFromData");
		return nullptr;
	}

	FString NodeClass = NodeData->GetStringField(TEXT("NodeClass"));
	if (NodeClass.IsEmpty())
	{
		OutError = TEXT("Node class is empty");
		return nullptr;
	}

	// Create node based on class
	UEdGraphNode* NewNode = nullptr;

	if (NodeClass.Contains(TEXT("K2Node_CallFunction")))
	{
		UK2Node_CallFunction* CallFunctionNode = NewObject<UK2Node_CallFunction>(Graph);
		
		// Set function reference if provided
		FString FunctionName = NodeData->GetStringField(TEXT("FunctionName"));
		FString FunctionClass = NodeData->GetStringField(TEXT("FunctionClass"));
		FString FunctionClassPath = NodeData->GetStringField(TEXT("FunctionClassPath"));
		FString FunctionModuleName = NodeData->GetStringField(TEXT("FunctionModuleName"));
		
		UE_LOG(LogTemp, Log, TEXT("CreateNodeFromData: Creating CallFunction node - FunctionName: %s, FunctionClass: %s, Path: %s"), 
			*FunctionName, *FunctionClass, *FunctionClassPath);
		
		if (!FunctionName.IsEmpty())
		{
			// Try to find the function
			UFunction* Function = nullptr;
			
			// First try using the full class path if available
			if (!FunctionClassPath.IsEmpty())
			{
				UClass* OwnerClass = FindFirstObject<UClass>(*FunctionClassPath);
				if (OwnerClass)
				{
					Function = OwnerClass->FindFunctionByName(FName(*FunctionName));
					UE_LOG(LogTemp, Log, TEXT("CreateNodeFromData: Found function using full path %s: %s"), 
						*FunctionClassPath, Function ? TEXT("YES") : TEXT("NO"));
				}
			}
			
			// If not found with full path, try with class name
			if (!Function && !FunctionClass.IsEmpty())
			{
				UClass* OwnerClass = FindObject<UClass>(nullptr, *FunctionClass);
				if (OwnerClass)
				{
					Function = OwnerClass->FindFunctionByName(FName(*FunctionName));
					UE_LOG(LogTemp, Log, TEXT("CreateNodeFromData: Found function in class %s: %s"), 
						*FunctionClass, Function ? TEXT("YES") : TEXT("NO"));
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("CreateNodeFromData: Could not find class %s"), *FunctionClass);
				}
			}
			
			if (Function)
			{
				CallFunctionNode->SetFromFunction(Function);
				UE_LOG(LogTemp, Log, TEXT("CreateNodeFromData: Successfully set function reference"));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("CreateNodeFromData: Could not find function %s in class %s"), *FunctionName, *FunctionClass);
			}
		}

		NewNode = CallFunctionNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_Event")))
	{
		UK2Node_Event* EventNode = NewObject<UK2Node_Event>(Graph);
		
		FString EventName = NodeData->GetStringField(TEXT("EventName"));
		if (!EventName.IsEmpty())
		{
			EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
		}

		NewNode = EventNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_CustomEvent")))
	{
		UK2Node_CustomEvent* CustomEventNode = NewObject<UK2Node_CustomEvent>(Graph);
		
		FString CustomEventName = NodeData->GetStringField(TEXT("CustomEventName"));
		if (!CustomEventName.IsEmpty())
		{
			CustomEventNode->CustomFunctionName = FName(*CustomEventName);
		}

		NewNode = CustomEventNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_FunctionEntry")))
	{
		UK2Node_FunctionEntry* FunctionEntryNode = NewObject<UK2Node_FunctionEntry>(Graph);
		
		// Function entry nodes are automatically created when the function graph is created
		// We just need to position it correctly
		NewNode = FunctionEntryNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_VariableGet")))
	{
		UK2Node_VariableGet* VariableGetNode = NewObject<UK2Node_VariableGet>(Graph);
		
		// Set variable reference if provided
		FString VariableName = NodeData->GetStringField(TEXT("VariableName"));
		FString VariableGuid = NodeData->GetStringField(TEXT("VariableGuid"));
		
		if (!VariableName.IsEmpty())
		{
			// Try to find the variable in the blueprint
			if (UBlueprint* Blueprint = Cast<UBlueprint>(Graph->GetOuter()))
			{
				for (FBPVariableDescription& Var : Blueprint->NewVariables)
				{
					if (Var.VarName.ToString() == VariableName)
					{
						// In UE 5.5, set the variable reference using the variable name
						VariableGetNode->VariableReference.SetSelfMember(Var.VarName);
						break;
					}
				}
			}
		}
		
		NewNode = VariableGetNode;
	}
	else if (NodeClass.Contains(TEXT("K2Node_FunctionResult")))
	{
		UK2Node_FunctionResult* FunctionResultNode = NewObject<UK2Node_FunctionResult>(Graph);
		
		// Function result nodes are automatically created when the function graph is created
		// We just need to position it correctly
		NewNode = FunctionResultNode;

		// Add output pins from payload if provided (e.g., return parameters)
		const TArray<TSharedPtr<FJsonValue>>* PinsArray = nullptr;
		if (NodeData->TryGetArrayField(TEXT("Pins"), PinsArray) && PinsArray)
		{
			// Ensure default pins (Exec) exist
			for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
			{
				if (!PinValue.IsValid() || PinValue->Type != EJson::Object)
				{
					continue;
				}
				TSharedPtr<FJsonObject> PinObj = PinValue->AsObject();
				FString PinName = PinObj->GetStringField(TEXT("PinName"));
				FString PinDirection = PinObj->GetStringField(TEXT("Direction")); // "Input" or "Output"
				FString PinCategory = PinObj->GetStringField(TEXT("PinType")); // matches CapturePins

				// Only add data pins (skip exec pins)
				if (PinCategory.Equals(TEXT("exec"), ESearchCase::IgnoreCase))
				{
					continue;
				}

				// For FunctionResult, user-defined data pins are INPUT pins on the Return node
				EEdGraphPinDirection Dir = EGPD_Input;

				// Map common pin categories
				FName SchemaType = UEdGraphSchema_K2::PC_Wildcard;
				if (PinCategory.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
				{
					SchemaType = UEdGraphSchema_K2::PC_Boolean;
				}
				else if (PinCategory.Equals(TEXT("int"), ESearchCase::IgnoreCase))
				{
					SchemaType = UEdGraphSchema_K2::PC_Int;
				}
				else if (PinCategory.Equals(TEXT("float"), ESearchCase::IgnoreCase) || PinCategory.Equals(TEXT("real"), ESearchCase::IgnoreCase))
				{
					SchemaType = UEdGraphSchema_K2::PC_Float;
				}
				else if (PinCategory.Equals(TEXT("string"), ESearchCase::IgnoreCase))
				{
					SchemaType = UEdGraphSchema_K2::PC_String;
				}

				// UE5.5+: Create pins via FEdGraphPinType
				FEdGraphPinType PinType;
				PinType.PinCategory = SchemaType;
				UEdGraphPin* CreatedPin = FunctionResultNode->CreatePin(Dir, PinType, *PinName);
				if (!CreatedPin)
				{
					UE_LOG(LogTemp, Warning, TEXT("CreateNodeFromData: Failed to create result pin %s"), *PinName);
				}
			}
			// Refresh node after adding pins
			FunctionResultNode->ReconstructNode();
		}
	}
	else
	{
		// Try to create a generic node
		UClass* NodeClassObj = FindObject<UClass>(nullptr, *NodeClass);
		if (NodeClassObj && NodeClassObj->IsChildOf<UEdGraphNode>())
		{
			NewNode = NewObject<UEdGraphNode>(Graph, NodeClassObj);
		}
		else
		{
			OutError = FString::Printf(TEXT("Unknown or invalid node class: %s"), *NodeClass);
			return nullptr;
		}
	}

	if (NewNode)
	{
		// Set position
		NewNode->NodePosX = NodeData->GetNumberField(TEXT("NodePosX"));
		NewNode->NodePosY = NodeData->GetNumberField(TEXT("NodePosY"));

		// Set GUID if provided
		FString NodeGuidStr = NodeData->GetStringField(TEXT("NodeGuid"));
		if (!NodeGuidStr.IsEmpty())
		{
			if (UK2Node* K2Node = Cast<UK2Node>(NewNode))
			{
				FGuid NodeGuid;
				if (FGuid::Parse(NodeGuidStr, NodeGuid))
				{
					K2Node->NodeGuid = NodeGuid;
				}
			}
		}

		// Allocate default pins
		NewNode->AllocateDefaultPins();

		// Process captured pins data to set default values and properties
		const TArray<TSharedPtr<FJsonValue>>* PinsArray = nullptr;
		if (NodeData->TryGetArrayField(TEXT("Pins"), PinsArray) && PinsArray)
		{
			for (const TSharedPtr<FJsonValue>& PinValue : *PinsArray)
			{
				if (!PinValue.IsValid() || PinValue->Type != EJson::Object)
				{
					continue;
				}

				TSharedPtr<FJsonObject> PinObj = PinValue->AsObject();
				FString PinName = PinObj->GetStringField(TEXT("PinName"));
				
				// Only read DefaultValue if the field exists
				FString DefaultValue;
				if (PinObj->HasField(TEXT("DefaultValue")))
				{
					DefaultValue = PinObj->GetStringField(TEXT("DefaultValue"));
				}
				else
				{
					UE_LOG(LogTemp, Log, TEXT("CreateNodeFromData: No DefaultValue found for pin '%s'"), *PinName);
				}

				// Only read DefaultObject if the field exists
				FString DefaultObjectPath;
				if (PinObj->HasField(TEXT("DefaultObject")))
				{
					DefaultObjectPath = PinObj->GetStringField(TEXT("DefaultObject"));
				}

				// Find the pin by name
				UEdGraphPin* Pin = NewNode->FindPin(*PinName);
				if (Pin)
				{
					// Set default value if available
					if (!DefaultValue.IsEmpty())
					{
						Pin->DefaultValue = DefaultValue;
						UE_LOG(LogTemp, VeryVerbose, TEXT("Set default value for pin '%s': '%s'"), *PinName, *DefaultValue);
					}
					
					// Set default object if available
					if (!DefaultObjectPath.IsEmpty())
					{
						UObject* DefaultObject = FindFirstObject<UObject>(*DefaultObjectPath);
						if (DefaultObject)
						{
							Pin->DefaultObject = DefaultObject;
							UE_LOG(LogTemp, VeryVerbose, TEXT("Set default object for pin '%s': '%s'"), *PinName, *DefaultObjectPath);
						}
						else
						{
							UE_LOG(LogTemp, Warning, TEXT("Could not find default object '%s' for pin '%s'"), *DefaultObjectPath, *PinName);
						}
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Could not find pin '%s' to set default value/object"), *PinName);
				}
			}
		}

		UE_LOG(LogTemp, VeryVerbose, TEXT("Created node: %s (%s)"), *NewNode->GetName(), *NodeClass);
	}

	return NewNode;
}

bool FApplyEngine::UpdateVariableReferences(
	UBlueprint* Blueprint,
	const FString& OldGuid,
	const FString& NewGuid,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	FGuid OldGuidParsed, NewGuidParsed;
	if (!FGuid::Parse(OldGuid, OldGuidParsed) || !FGuid::Parse(NewGuid, NewGuidParsed))
	{
		OutError = TEXT("Invalid GUID format");
		return false;
	}

	int32 UpdateCount = 0;

	// Update variable references across known graph containers (UE5)
	auto UpdateGraphNodes = [&](UEdGraph* Graph)
	{
		if (!Graph) { return; }
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(Node))
			{
				if (VarGetNode->VariableReference.GetMemberGuid() == OldGuidParsed)
				{
					VarGetNode->VariableReference.SetExternalMember(
						VarGetNode->VariableReference.GetMemberName(),
						VarGetNode->VariableReference.GetMemberParentClass(),
						NewGuidParsed
					);
					VarGetNode->ReconstructNode();
					UpdateCount++;
				}
			}
			else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(Node))
			{
				if (VarSetNode->VariableReference.GetMemberGuid() == OldGuidParsed)
				{
					VarSetNode->VariableReference.SetExternalMember(
						VarSetNode->VariableReference.GetMemberName(),
						VarSetNode->VariableReference.GetMemberParentClass(),
						NewGuidParsed
					);
					VarSetNode->ReconstructNode();
					UpdateCount++;
				}
			}
		}
	};

	TArray<UEdGraph*> AllGraphs;
	FSnapshotManager::GetAllBlueprintGraphs(Blueprint, AllGraphs);
	for (UEdGraph* Graph : AllGraphs) { UpdateGraphNodes(Graph); }

	UE_LOG(LogTemp, VeryVerbose, TEXT("Updated %d variable references for GUID remap: %s -> %s"), 
		UpdateCount, *OldGuid, *NewGuid);
	return true;
}


bool FApplyEngine::EnsureGameThread(TFunction<bool()> Operation)
{
	if (IsInGameThread())
	{
		return Operation();
	}
	else
	{
		// Queue operation on game thread
		bool bResult = false;
		AsyncTask(ENamedThreads::GameThread, [&Operation, &bResult]()
		{
			bResult = Operation();
		});
		return bResult;
	}
}

TSharedPtr<FScopedTransaction> FApplyEngine::CreateUndoTransaction(const FString& Description)
{
	return MakeShareable(new FScopedTransaction(FText::FromString(Description)));
}

void FApplyEngine::LogOperationResult(
	EMergeOperationType OperationType,
	const FString& TargetName,
	bool bSuccess,
	const FString& ErrorMessage)
{
	FString OpTypeName = StaticEnum<EMergeOperationType>()->GetValueAsString(OperationType);
	
	if (bSuccess)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("✅ %s on %s succeeded"), *OpTypeName, *TargetName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("❌ %s on %s failed: %s"), *OpTypeName, *TargetName, *ErrorMessage);
	}
}

bool FApplyEngine::IsBlueprintBeingEdited(UBlueprint* Blueprint)
{
	if (!Blueprint || !GEditor)
	{
		return false;
	}

	// Conservative fallback: avoid calling removed APIs; consider the BP as editable/open state unknown
	return false;
}

void FApplyEngine::RefreshBlueprintEditor(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return;
	}

	// Use standard refresh/compile path compatible with UE5
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

bool FApplyEngine::ApplyStructuredData(
	UBlueprint* TargetBlueprint,
	const FBlueprintMergeData& BlueprintData,
	FApplyResult& OutResult)
{
	if (!TargetBlueprint)
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is null");
		return false;
	}

	if (!IsValid(TargetBlueprint))
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Target Blueprint is not valid");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Applying structured data to Blueprint: %s"), *TargetBlueprint->GetName());
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Variables to apply: %d, Graphs to apply: %d"), 
		BlueprintData.Variables.Num(), BlueprintData.Graphs.Num());

	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Must be called from game thread"));
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = TEXT("Operations must be applied from the game thread");
		return false;
	}

	// Validate custom class availability first
	TArray<FString> MissingClasses;
	if (!ValidateCustomClassAvailability(BlueprintData, MissingClasses))
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = FString::Printf(TEXT("Missing custom classes: %s"), 
			*FString::Join(MissingClasses, TEXT(", ")));
		UE_LOG(LogTemp, Error, TEXT("ApplyEngine: Cannot apply merge - missing custom classes"));
		return false;
	}

	// Start a transaction for undo support
	FScopedTransaction Transaction(FText::FromString(TEXT("Apply Blueprint Merge")));

	try
	{
		// Apply variables
		for (const FBlueprintMergeVariableData& VariableData : BlueprintData.Variables)
		{
			FString ErrorMessage;
			if (AddVariableFromStructuredData(TargetBlueprint, VariableData, ErrorMessage))
			{
				OutResult.AppliedOperations.Add(FString::Printf(TEXT("Added variable: %s"), *VariableData.VariableName.ToString()));
			}
			else
			{
				OutResult.FailedOperations.Add(FString::Printf(TEXT("Failed to add variable %s: %s"), 
					*VariableData.VariableName.ToString(), *ErrorMessage));
			}
		}

		// Apply graphs and nodes
		for (const FBlueprintMergeGraphData& GraphData : BlueprintData.Graphs)
		{
			FString ErrorMessage;
			if (AddGraphFromStructuredData(TargetBlueprint, GraphData, ErrorMessage))
			{
				OutResult.AppliedOperations.Add(FString::Printf(TEXT("Added graph: %s"), *GraphData.GraphName));
			}
			else
			{
				OutResult.FailedOperations.Add(FString::Printf(TEXT("Failed to add graph %s: %s"), 
					*GraphData.GraphName, *ErrorMessage));
			}
		}

		// Mark Blueprint as modified
		TargetBlueprint->Modify();
		FBlueprintEditorUtils::MarkBlueprintAsModified(TargetBlueprint);

		// Compile the Blueprint
		FKismetEditorUtilities::CompileBlueprint(TargetBlueprint, EBlueprintCompileOptions::None);
		OutResult.bBlueprintCompiled = true;

		// Save the Blueprint
		if (UEditorAssetSubsystem* EditorAssetSubsystem = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>())
		{
			OutResult.bBlueprintSaved = EditorAssetSubsystem->SaveAsset(TargetBlueprint->GetPathName());
		}
		else
		{
			OutResult.bBlueprintSaved = false;
		}

		OutResult.bSuccess = OutResult.FailedOperations.Num() == 0;
		
		UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Structured data application completed. Applied: %d, Failed: %d"), 
			OutResult.AppliedOperations.Num(), OutResult.FailedOperations.Num());

		return OutResult.bSuccess;
	}
	catch (const std::exception& e)
	{
		OutResult.bSuccess = false;
		OutResult.ErrorMessage = FString::Printf(TEXT("Exception during structured data application: %s"), ANSI_TO_TCHAR(e.what()));
		UE_LOG(LogTemp, Error, TEXT("ApplyEngine: %s"), *OutResult.ErrorMessage);
		return false;
	}
}

bool FApplyEngine::AddVariableFromStructuredData(
	UBlueprint* Blueprint,
	const FBlueprintMergeVariableData& VariableData,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Adding variable from structured data - Name: %s, Type: %s"), 
		*VariableData.VariableName.ToString(), *VariableData.VarType.PinCategory.ToString());

	// Check if variable already exists (name-based conflict detection)
	for (const FBPVariableDescription& ExistingVar : Blueprint->NewVariables)
	{
		if (ExistingVar.VarName == VariableData.VariableName)
		{
			OutError = FString::Printf(TEXT("Variable '%s' already exists"), *VariableData.VariableName.ToString());
			return false;
		}
	}

	// Convert structured data to FBPVariableDescription
	FBPVariableDescription NewVariable = VariableData.ToVariableDescription();

	// Add the variable directly to the Blueprint's NewVariables array
	Blueprint->NewVariables.Add(NewVariable);

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Successfully added variable from structured data: %s"), *VariableData.VariableName.ToString());
	return true;
}

bool FApplyEngine::AddGraphFromStructuredData(
	UBlueprint* Blueprint,
	const FBlueprintMergeGraphData& GraphData,
	FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("Blueprint is null");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Adding graph from structured data - Name: %s, Type: %s"), 
		*GraphData.GraphName, *GraphData.GraphType);

	// For now, we'll focus on variables. Graph/node creation is more complex and would require
	// more detailed implementation. This is a placeholder for future enhancement.
	
	UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Graph creation from structured data not yet implemented: %s"), *GraphData.GraphName);
	return true;
}

bool FApplyEngine::ValidateCustomClassAvailability(
	const FBlueprintMergeData& BlueprintData,
	TArray<FString>& OutMissingClasses)
{
	OutMissingClasses.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Validating custom class availability..."));
	
	// Check all graphs for custom class references
	for (const FBlueprintMergeGraphData& GraphData : BlueprintData.Graphs)
	{
		for (const FBlueprintMergeNodeData& NodeData : GraphData.Nodes)
		{
			// Check function class references
			if (!NodeData.FunctionClassPath.IsEmpty())
			{
				UClass* FoundClass = FindObject<UClass>(nullptr, *NodeData.FunctionClassPath);
				if (!FoundClass)
				{
					OutMissingClasses.AddUnique(NodeData.FunctionClassPath);
					UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Missing custom class: %s"), *NodeData.FunctionClassPath);
				}
			}
			
			// Check custom references
			for (const auto& RefPair : NodeData.CustomReferences)
			{
				UClass* FoundClass = FindObject<UClass>(nullptr, *RefPair.Value);
				if (!FoundClass)
				{
					OutMissingClasses.AddUnique(RefPair.Value);
					UE_LOG(LogTemp, Warning, TEXT("ApplyEngine: Missing custom reference: %s"), *RefPair.Value);
				}
			}
		}
	}
	
	bool bAllClassesAvailable = OutMissingClasses.Num() == 0;
	UE_LOG(LogTemp, Log, TEXT("ApplyEngine: Custom class validation complete. Missing: %d classes"), OutMissingClasses.Num());
	
	return bAllClassesAvailable;
}

