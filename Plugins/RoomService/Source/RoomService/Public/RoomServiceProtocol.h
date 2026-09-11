#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "RoomServiceTypes.h"

namespace RoomServiceProtocol
{
	using FJson = TSharedPtr<FJsonObject>;

	inline FJson Object()
	{
		return MakeShared<FJsonObject>();
	}

	inline FString Encode(const FJson& Json)
	{
		FString Text;
		FJsonSerializer::Serialize(Json.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
		return Text;
	}

	inline FJson Decode(const FString& Text)
	{
		FJson Json;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json);
		return Json;
	}

	inline FString String(const FJson& Json, const TCHAR* Field)
	{
		FString Value;
		if (Json)
		{
			Json->TryGetStringField(Field, Value);
		}
		return Value;
	}

	inline FString NewSecret()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::Digits)
			+ FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}

	inline bool IsIdentifier(const FString& Value)
	{
		if (Value.IsEmpty() || Value.Len() > 128)
		{
			return false;
		}
		for (const TCHAR Char : Value)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('_') && Char != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	inline FJson ToJson(const FRoomServiceInfo& Room)
	{
		FJson Json = Object();
		Json->SetStringField(TEXT("roomId"), Room.RoomId);
		Json->SetStringField(TEXT("title"), Room.Title);
		Json->SetStringField(TEXT("mapId"), Room.MapId);
		Json->SetStringField(TEXT("state"), Room.State);
		Json->SetNumberField(TEXT("currentPlayers"), Room.CurrentPlayers);
		Json->SetNumberField(TEXT("maxPlayers"), Room.MaxPlayers);
		Json->SetBoolField(TEXT("private"), Room.bPrivate);
		FJson Attributes = Object();
		for (const auto& Pair : Room.Attributes)
		{
			Attributes->SetStringField(Pair.Key, Pair.Value);
		}
		Json->SetObjectField(TEXT("attributes"), Attributes);
		return Json;
	}

	inline bool ReadRoom(const FJson& Json, FRoomServiceInfo& Room)
	{
		if (!Json || !Json->TryGetStringField(TEXT("mapId"), Room.MapId)
			|| !Json->TryGetStringField(TEXT("title"), Room.Title)
			|| !Json->TryGetBoolField(TEXT("private"), Room.bPrivate))
		{
			return false;
		}
		Room.RoomId = String(Json, TEXT("roomId"));
		Room.State = String(Json, TEXT("state"));
		Json->TryGetNumberField(TEXT("currentPlayers"), Room.CurrentPlayers);
		Json->TryGetNumberField(TEXT("maxPlayers"), Room.MaxPlayers);
		const FJson* Attributes = nullptr;
		if (Json->TryGetObjectField(TEXT("attributes"), Attributes))
		{
			if ((*Attributes)->Values.Num() > 16)
			{
				return false;
			}
			for (const auto& Pair : (*Attributes)->Values)
			{
				FString Value;
				if (Pair.Key.Len() > 64 || !Pair.Value->TryGetString(Value) || Value.Len() > 256)
				{
					return false;
				}
				Room.Attributes.Add(Pair.Key, Value);
			}
		}
		return IsIdentifier(Room.MapId) && !Room.Title.IsEmpty() && Room.Title.Len() <= 80;
	}
}
