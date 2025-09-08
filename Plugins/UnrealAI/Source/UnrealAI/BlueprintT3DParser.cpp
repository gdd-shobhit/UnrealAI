#include "BlueprintT3DParser.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

static FString TrimComment(const FString& In)
{
	int32 HashIdx;
	if (In.FindChar('#', HashIdx))
	{
		return In.Left(HashIdx).TrimStartAndEnd();
	}
	return In.TrimStartAndEnd();
}

bool FBlueprintT3DParser::Parse(const FString& Text, FT3DAssetInfo& OutInfo)
{
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);

	TArray<FObjectContext> Stack;
	FObjectContext Current;
	bool bInsideObject = false;

	for (const FString& RawLine : Lines)
	{
		const FString Line = TrimComment(RawLine);
		if (Line.IsEmpty())
		{
			continue;
		}

		if (Line.StartsWith(TEXT("Begin Object")))
		{
			FString Cls, Name;
			if (ParseBeginObjectHeader(Line, Cls, Name))
			{
				if (bInsideObject)
				{
					Stack.Add(Current);
				}
				Current = FObjectContext{Cls, Name, {}};
				bInsideObject = true;
				continue;
			}
		}

		if (Line.StartsWith(TEXT("End Object")))
		{
			if (bInsideObject)
			{
				ParseObjectBlock(Current, OutInfo);
				if (Stack.Num() > 0)
				{
					Current = Stack.Pop();
				}
				else
				{
					bInsideObject = false;
				}
			}
			continue;
		}

		if (bInsideObject)
		{
			Current.PropertyLines.Add(Line);
		}
		else
		{
			ParseTopLevelLine(Line, OutInfo);
		}
	}

	return true;
}

void FBlueprintT3DParser::LogSummary(const FT3DAssetInfo& Info)
{
	UE_LOG(LogTemp, Log, TEXT("T3D Summary: ParentClass=%s, Vars=%d, FuncGraphs=%d, NodesList=%d, Graphs=%d, TopLevelNodes=%d"),
		*Info.ParentClass, Info.NewVariables.Num(), Info.FunctionGraphs.Num(), Info.NodesList.Num(), Info.Graphs.Num(), Info.TopLevelNodes.Num());
	for (const FT3DGraphInfo& G : Info.Graphs)
	{
		UE_LOG(LogTemp, Log, TEXT(" Graph: %s Name=%s Nodes=%d"), *G.ClassName, *G.Name, G.Nodes.Num());
	}
	for (const FT3DNodeInfo& N : Info.TopLevelNodes)
	{
		UE_LOG(LogTemp, Log, TEXT(" Node: %s Name=%s NodeGuid=%s Pins=%d"), *N.ClassName, *N.Name, *N.NodeGuid, N.Pins.Num());
	}
}

TSharedPtr<FJsonObject> FBlueprintT3DParser::ToJson(const FT3DAssetInfo& Info)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("ParentClass"), Info.ParentClass);

	TArray<TSharedPtr<FJsonValue>> Vars;
	for (const FString& V : Info.NewVariables) Vars.Add(MakeShared<FJsonValueString>(V));
	Root->SetArrayField(TEXT("NewVariables"), Vars);

	TArray<TSharedPtr<FJsonValue>> Funcs;
	for (const FString& F : Info.FunctionGraphs) Funcs.Add(MakeShared<FJsonValueString>(F));
	Root->SetArrayField(TEXT("FunctionGraphs"), Funcs);

	TArray<TSharedPtr<FJsonValue>> NodeList;
	for (const FString& N : Info.NodesList) NodeList.Add(MakeShared<FJsonValueString>(N));
	Root->SetArrayField(TEXT("Nodes"), NodeList);

	TArray<TSharedPtr<FJsonValue>> Graphs;
	for (const FT3DGraphInfo& G : Info.Graphs)
	{
		TSharedPtr<FJsonObject> Gj = MakeShared<FJsonObject>();
		Gj->SetStringField(TEXT("ClassName"), G.ClassName);
		Gj->SetStringField(TEXT("Name"), G.Name);
		TArray<TSharedPtr<FJsonValue>> GProps;
		for (const auto& P : G.Properties)
		{
			TSharedPtr<FJsonObject> Pj = MakeShared<FJsonObject>();
			Pj->SetStringField(P.Key, P.Value);
			GProps.Add(MakeShared<FJsonValueObject>(Pj));
		}
		Gj->SetArrayField(TEXT("Properties"), GProps);

		TArray<TSharedPtr<FJsonValue>> GNodes;
		for (const FT3DNodeInfo& Nd : G.Nodes)
		{
			TSharedPtr<FJsonObject> Nj = MakeShared<FJsonObject>();
			Nj->SetStringField(TEXT("ClassName"), Nd.ClassName);
			Nj->SetStringField(TEXT("Name"), Nd.Name);
			Nj->SetStringField(TEXT("NodeGuid"), Nd.NodeGuid);
			Nj->SetStringField(TEXT("FunctionReference"), Nd.FunctionReference);
			Nj->SetStringField(TEXT("VarGuid"), Nd.VarGuid);
			TArray<TSharedPtr<FJsonValue>> Pins;
			for (const FT3DPinInfo& P : Nd.Pins)
			{
				TSharedPtr<FJsonObject> Pj = MakeShared<FJsonObject>();
				Pj->SetStringField(TEXT("PinId"), P.PinId);
				Pj->SetStringField(TEXT("PinName"), P.PinName);
				for (const auto& A : P.Attributes)
				{
					Pj->SetStringField(A.Key, A.Value);
				}
				Pins.Add(MakeShared<FJsonValueObject>(Pj));
			}
			Nj->SetArrayField(TEXT("Pins"), Pins);
			GNodes.Add(MakeShared<FJsonValueObject>(Nj));
		}
		Gj->SetArrayField(TEXT("Nodes"), GNodes);
		Graphs.Add(MakeShared<FJsonValueObject>(Gj));
	}
	Root->SetArrayField(TEXT("Graphs"), Graphs);

	TArray<TSharedPtr<FJsonValue>> TLNodes;
	for (const FT3DNodeInfo& Nd : Info.TopLevelNodes)
	{
		TSharedPtr<FJsonObject> Nj = MakeShared<FJsonObject>();
		Nj->SetStringField(TEXT("ClassName"), Nd.ClassName);
		Nj->SetStringField(TEXT("Name"), Nd.Name);
		Nj->SetStringField(TEXT("NodeGuid"), Nd.NodeGuid);
		TLNodes.Add(MakeShared<FJsonValueObject>(Nj));
	}
	Root->SetArrayField(TEXT("TopLevelNodes"), TLNodes);

	return Root;
}

bool FBlueprintT3DParser::ParseBeginObjectHeader(const FString& Line, FString& OutClass, FString& OutName)
{
	// Example: Begin Object Class=/Script/BlueprintGraph.K2Node_CallFunction Name="K2Node_CallFunction_0"
	int32 ClassIdx = Line.Find(TEXT("Class="));
	int32 NameIdx = Line.Find(TEXT("Name="));
	if (ClassIdx == INDEX_NONE || NameIdx == INDEX_NONE) return false;
	const FString ClassPart = Line.Mid(ClassIdx + 6, NameIdx - (ClassIdx + 6)).TrimStartAndEnd();
	OutClass = ClassPart;
	OutName = Line.Mid(NameIdx + 5).TrimStartAndEnd();
	return true;
}

void FBlueprintT3DParser::ParseTopLevelLine(const FString& Line, FT3DAssetInfo& OutInfo)
{
	// Capture simple key=val and known array-like aggregates by pattern
	if (Line.StartsWith(TEXT("ParentClass=")))
	{
		OutInfo.ParentClass = Line.Mid(12).TrimStartAndEnd();
		return;
	}
	if (Line.StartsWith(TEXT("NewVariables=(")))
	{
		OutInfo.NewVariables.Add(Line);
		return;
	}
	if (Line.StartsWith(TEXT("FunctionGraphs=(")))
	{
		OutInfo.FunctionGraphs.Add(Line);
		return;
	}
	if (Line.StartsWith(TEXT("Nodes=(")))
	{
		OutInfo.NodesList.Add(Line);
		return;
	}
	// generic top-level property capture
	int32 EqIdx;
	if (Line.FindChar('=', EqIdx))
	{
		OutInfo.TopLevelProperties.Add(Line.Left(EqIdx).TrimStartAndEnd(), Line.Mid(EqIdx + 1).TrimStartAndEnd());
	}
}

void FBlueprintT3DParser::ParseObjectBlock(const FObjectContext& Ctx, FT3DAssetInfo& OutInfo)
{
	// Route K2Node_* objects as nodes, and UEdGraph as graphs; otherwise generic capture
	if (Ctx.ClassName.Contains(TEXT("K2Node_")))
	{
		FT3DNodeInfo Node; Node.ClassName = Ctx.ClassName; Node.Name = Ctx.Name;
		ParseNodeProperties(Ctx.PropertyLines, Node);
		OutInfo.TopLevelNodes.Add(Node);
		return;
	}
	if (Ctx.ClassName.Contains(TEXT("/Script/Engine.EdGraph")) || Ctx.ClassName.Contains(TEXT("UEdGraph")))
	{
		FT3DGraphInfo Graph; Graph.ClassName = Ctx.ClassName; Graph.Name = Ctx.Name;
		// a deeper pass could extract contained nodes from inner Begin/End Object, but top-level capture is sufficient here
		for (const FString& L : Ctx.PropertyLines)
		{
			int32 EqIdx;
			if (L.FindChar('=', EqIdx))
			{
				Graph.Properties.Add(L.Left(EqIdx).TrimStartAndEnd(), L.Mid(EqIdx + 1).TrimStartAndEnd());
			}
		}
		OutInfo.Graphs.Add(Graph);
		return;
	}
	// generic object properties ignored for now
}

void FBlueprintT3DParser::ParseNodeProperties(const TArray<FString>& Lines, FT3DNodeInfo& OutNode)
{
	for (const FString& L : Lines)
	{
		if (L.StartsWith(TEXT("NodeGuid=")))
		{
			OutNode.NodeGuid = L.Mid(9).TrimStartAndEnd();
			continue;
		}
		if (L.StartsWith(TEXT("FunctionReference=")))
		{
			OutNode.FunctionReference = L.Mid(18).TrimStartAndEnd();
			continue;
		}
		if (L.StartsWith(TEXT("VarGuid=")))
		{
			OutNode.VarGuid = L.Mid(8).TrimStartAndEnd();
			continue;
		}
		if (L.StartsWith(TEXT("CustomProperties ")))
		{
			// CustomProperties Pin (PinId=...,PinName=..., ...)
			ParsePinsFromCustomProperties(L, OutNode.Pins);
			continue;
		}
		int32 EqIdx;
		if (L.FindChar('=', EqIdx))
		{
			OutNode.Properties.Add(L.Left(EqIdx).TrimStartAndEnd(), L.Mid(EqIdx + 1).TrimStartAndEnd());
		}
	}
}

void FBlueprintT3DParser::ParsePinsFromCustomProperties(const FString& Line, TArray<FT3DPinInfo>& OutPins)
{
	// Expect: CustomProperties Pin (PinId=...,PinName=...,...,LinkedTo=(...))
	int32 PinIdx = Line.Find(TEXT("Pin ("));
	if (PinIdx == INDEX_NONE) return;
	const FString After = Line.Mid(PinIdx + 4).TrimStartAndEnd();
	// extract content inside parentheses
	int32 OpenIdx = After.Find(TEXT("("));
	int32 CloseIdx = After.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (OpenIdx == INDEX_NONE || CloseIdx == INDEX_NONE || CloseIdx <= OpenIdx) return;
	const FString Content = After.Mid(OpenIdx + 1, CloseIdx - OpenIdx - 1);

	TArray<FString> Pairs; SplitTopLevelCommaPairs(Content, Pairs);
	FT3DPinInfo Pin;
	for (const FString& Pair : Pairs)
	{
		FString K, V; ParseKeyValue(Pair, K, V);
		if (K.Equals(TEXT("PinId"), ESearchCase::IgnoreCase)) Pin.PinId = V;
		else if (K.Equals(TEXT("PinName"), ESearchCase::IgnoreCase)) Pin.PinName = V;
		else Pin.Attributes.Add(K, V);
	}
	OutPins.Add(Pin);
}

void FBlueprintT3DParser::SplitTopLevelCommaPairs(const FString& ParenContent, TArray<FString>& OutPairs)
{
	int32 Depth = 0; int32 Start = 0;
	for (int32 i = 0; i < ParenContent.Len(); ++i)
	{
		TCHAR C = ParenContent[i];
		if (C == '(') { Depth++; }
		else if (C == ')') { Depth--; }
		else if (C == ',' && Depth == 0)
		{
			OutPairs.Add(ParenContent.Mid(Start, i - Start).TrimStartAndEnd());
			Start = i + 1;
		}
	}
	if (Start < ParenContent.Len())
	{
		OutPairs.Add(ParenContent.Mid(Start).TrimStartAndEnd());
	}
}

void FBlueprintT3DParser::ParseKeyValue(const FString& Pair, FString& OutKey, FString& OutVal)
{
	int32 EqIdx; if (!Pair.FindChar('=', EqIdx)) { OutKey = Pair.TrimStartAndEnd(); OutVal.Reset(); return; }
	OutKey = Pair.Left(EqIdx).TrimStartAndEnd();
	OutVal = Pair.Mid(EqIdx + 1).TrimStartAndEnd();
}


