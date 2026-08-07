#pragma once

#include "CoreMinimal.h"

enum class EDRLogVerbosity : uint8
{
	Log,
	Warning,
	Error
};

class DRLoggingLibrary
{
public:
	FORCEINLINE static void PrintStringInternal(
		const UObject* InContextObject,
		const FString& InString,
		const float InTimeToDisplay,
		const FColor InColor,
		const FString& InFuncName,
		const int32 InLineNumber,
		const EDRLogVerbosity InVerbosity)
	{
		if (!InContextObject || !InContextObject->GetWorld())
			return;

		AActor* InWorldContextActor = InContextObject->GetWorld()->GetFirstPlayerController();
		if (!IsValid(InWorldContextActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("InWorldContextActor is invalid"));
			return;
		}

		if (IsValid(GEngine))
		{
			const ENetMode NetMode = InWorldContextActor->GetNetMode();
			const FString NetModeString = ConvertNetModeToString(NetMode);

			// [실행모드] 로그 내용 (함수 이름 : (%s:%d))
			FString FullFormattedString = FString::Printf(
				TEXT("[%s] %s (%s:%d)"), *NetModeString, *InString, *InFuncName, InLineNumber);

			if (NetMode == NM_Standalone || NetMode == NM_Client || NetMode == NM_ListenServer)
			{
				GEngine->AddOnScreenDebugMessage(-1, InTimeToDisplay, InColor, FullFormattedString);
			}
			else
			{
				switch (InVerbosity)
				{
				case EDRLogVerbosity::Warning:
					UE_LOG(LogTemp, Warning, TEXT("%s"), *FullFormattedString);
					break;
				case EDRLogVerbosity::Error:
					UE_LOG(LogTemp, Error, TEXT("%s"), *FullFormattedString);
					break;
				case EDRLogVerbosity::Log:
				default:
					UE_LOG(LogTemp, Log, TEXT("%s"), *FullFormattedString);
					break;
				}
			}
		}
	}

	FORCEINLINE static void LogInternal(
		const UObject* InContextObject,
		const FString& InString,
		const FString& InFuncName,
		const int32 InLineNumber,
		const EDRLogVerbosity InVerbosity)
	{
		if (!InContextObject || !InContextObject->GetWorld())
			return;

		AActor* InWorldContextActor = InContextObject->GetWorld()->GetFirstPlayerController();
		if (!IsValid(InWorldContextActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("InWorldContextActor is invalid"));
			return;
		}

		const ENetMode NetMode = InWorldContextActor->GetNetMode();
		const FString NetModeString = ConvertNetModeToString(NetMode);

		// [실행모드] 로그 내용 (함수 이름 : (%s:%d))
		FString FullFormattedString = FString::Printf(
			TEXT("[%s] %s (%s:%d)"), *NetModeString, *InString, *InFuncName, InLineNumber);

		switch (InVerbosity)
		{
		case EDRLogVerbosity::Warning:
			UE_LOG(LogTemp, Warning, TEXT("%s"), *FullFormattedString);
			break;
		case EDRLogVerbosity::Error:
			UE_LOG(LogTemp, Error, TEXT("%s"), *FullFormattedString);
			break;
		case EDRLogVerbosity::Log:
		default:
			UE_LOG(LogTemp, Log, TEXT("%s"), *FullFormattedString);
			break;
		}
	}

private:
	static FString ConvertNetModeToString(const ENetMode NetMode)
	{
		FString NetModeString = TEXT("None");

		switch (NetMode)
		{
		case NM_Standalone:
			NetModeString = TEXT("StandAlone");
			break;
		case NM_DedicatedServer:
			NetModeString = TEXT("DedicatedServer");
			break;
		case NM_ListenServer:
			NetModeString = TEXT("ListenServer");
			break;
		case NM_Client:
			NetModeString = FString::Printf(TEXT("Client%02d"), UE::GetPlayInEditorID());
			break;
		default:
			NetModeString = TEXT("Unknown");
			break;
		}

		return NetModeString;
	}
};

#define DR_PRINT_LOG(Format, ...) \
	DRLoggingLibrary::PrintStringInternal(this, FString::Printf(Format, ##__VA_ARGS__), 2.f, FColor::Cyan, TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Log)

#define DR_PRINT_WARNING(Format, ...) \
	DRLoggingLibrary::PrintStringInternal(this, FString::Printf(Format, ##__VA_ARGS__), 3.f, FColor::Yellow, TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Warning)

#define DR_PRINT_ERROR(Format, ...) \
	DRLoggingLibrary::PrintStringInternal(this, FString::Printf(Format, ##__VA_ARGS__), 4.f, FColor::Red, TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Error)

#define DR_LOG(Format, ...) \
	DRLoggingLibrary::LogInternal(this, FString::Printf(Format, ##__VA_ARGS__), TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Log)

#define DR_WARNING(Format, ...) \
	DRLoggingLibrary::LogInternal(this, FString::Printf(Format, ##__VA_ARGS__), TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Warning)

#define DR_ERROR(Format, ...) \
	DRLoggingLibrary::LogInternal(this, FString::Printf(Format, ##__VA_ARGS__), TEXT(__FUNCTION__), __LINE__, EDRLogVerbosity::Error)
