#include "UnrealBridgeClient.h"
#include "UnrealToolRegistry.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"

UUnrealBridgeClient::UUnrealBridgeClient()
{
	BridgeBaseUrl = GetDefaultBridgeUrl();
}

void UUnrealBridgeClient::SetBridgeUrl(const FString& BaseUrl)
{
	BridgeBaseUrl = BaseUrl;
}

FString UUnrealBridgeClient::GetDefaultBridgeUrl()
{
	FString Url;
	GConfig->GetString(TEXT("UnrealAI"), TEXT("BridgeUrl"), Url, GEngineIni);
	if (Url.IsEmpty()) Url = TEXT("http://127.0.0.1:8765");
	return Url;
}

void UUnrealBridgeClient::SendRunPrompt(const FString& Prompt, const FString& Provider, TFunction<void(bool bSuccess, const FString& SessionId)> Callback)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BridgeBaseUrl + TEXT("/run"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FString JsonBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("prompt"), Prompt);
	Writer->WriteValue(TEXT("provider"), Provider);
	Writer->WriteObjectEnd();
	Writer->Close();
	Request->SetContentAsString(JsonBody);
	Request->OnProcessRequestComplete().BindLambda([this, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
	{
		OnRunResponse(Req, Res, bOk, Callback);
	});
	Request->ProcessRequest();
}

void UUnrealBridgeClient::OnRunResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(bool, const FString&)> Callback)
{
	FString SessionId;
	if (bSuccess && Response.IsValid() && Response->GetResponseCode() == 200)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
			SessionId = Obj->GetStringField(TEXT("session_id"));
	}
	Callback(bSuccess && !SessionId.IsEmpty(), SessionId);
}

void UUnrealBridgeClient::Poll(const FString& SessionId, TFunction<void(const FBridgePollResult&)> Callback)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BridgeBaseUrl + FString::Printf(TEXT("/poll?session=%s"), *FPlatformHttp::UrlEncode(SessionId)));
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([this, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
	{
		OnPollResponse(Req, Res, bOk, Callback);
	});
	Request->ProcessRequest();
}

void UUnrealBridgeClient::OnPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(const FBridgePollResult&)> Callback)
{
	FBridgePollResult Result;
	if (!bSuccess || !Response.IsValid())
	{
		Result.Error = TEXT("Poll request failed");
		Callback(Result);
		return;
	}
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		Result.Error = TEXT("Invalid poll response JSON");
		Callback(Result);
		return;
	}
	Result.bDone = Obj->GetBoolField(TEXT("done"));
	const TArray<TSharedPtr<FJsonValue>>* LogArr = nullptr;
	if (Obj->TryGetArrayField(TEXT("log_lines"), LogArr) && LogArr)
	{
		for (const TSharedPtr<FJsonValue>& V : *LogArr)
			Result.LogLines.Add(V->AsString());
	}
	const TSharedPtr<FJsonObject>* ToolCallPtr = nullptr;
	if (Obj->TryGetObjectField(TEXT("tool_call"), ToolCallPtr) && ToolCallPtr && ToolCallPtr->IsValid())
	{
		const TSharedPtr<FJsonObject>& ToolCall = *ToolCallPtr;
		Result.ToolCallId = ToolCall->GetStringField(TEXT("id"));
		Result.ToolName = ToolCall->GetStringField(TEXT("name"));
		const TSharedPtr<FJsonObject>* ArgsObjPtr = nullptr;
		if (ToolCall->TryGetObjectField(TEXT("args"), ArgsObjPtr) && ArgsObjPtr && (*ArgsObjPtr).IsValid())
		{
			TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Result.ToolArgsJson);
			FJsonSerializer::Serialize((*ArgsObjPtr).ToSharedRef(), W);
		}
		else
			Result.ToolArgsJson = ToolCall->GetStringField(TEXT("args"));
	}
	Callback(Result);
}

void UUnrealBridgeClient::SendToolResult(const FString& SessionId, const FString& ToolCallId, const FString& Result, TFunction<void(bool bSuccess)> Callback)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BridgeBaseUrl + TEXT("/tool_result"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	FString JsonBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonBody);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("session_id"), SessionId);
	Writer->WriteValue(TEXT("tool_call_id"), ToolCallId);
	Writer->WriteValue(TEXT("result"), Result);
	Writer->WriteObjectEnd();
	Writer->Close();
	Request->SetContentAsString(JsonBody);
	Request->OnProcessRequestComplete().BindLambda([this, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bOk)
	{
		OnToolResultResponse(Req, Res, bOk, Callback);
	});
	Request->ProcessRequest();
}

void UUnrealBridgeClient::OnToolResultResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, TFunction<void(bool)> Callback)
{
	bool Ok = bSuccess && Response.IsValid() && Response->GetResponseCode() == 200;
	Callback(Ok);
}
