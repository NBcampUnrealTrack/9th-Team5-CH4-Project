#include "RoomServiceBackend.h"
#include "RoomServiceProtocol.h"

void URoomServiceBackend::Authenticate(const FString& Credential, FRoomAuthComplete Complete)
{
	Complete(false, FString());
}

void URoomServiceBackend::PublishRoom(const FRoomServiceInfo& Room, FRoomPublishComplete Complete)
{
	Complete(false);
}

void URoomServiceBackend::RemoveRoom(const FString& RoomId)
{
}

void URoomServiceBackend::DiscoverRooms(const FString& Identity, FRoomDiscoveryComplete Complete)
{
	Complete({});
}

void URoomServiceBackend::IssueConnection(const FRoomServiceInfo& Room, const FString& Identity,
	const FString& Endpoint, FRoomConnectionComplete Complete)
{
	Complete(false, {});
}

void URoomServiceBackend::ValidateConnection(const FString& Presented, const FString& Issued,
	FRoomValidationComplete Complete)
{
	Complete(false);
}

void ULocalRoomServiceBackend::Authenticate(const FString& Credential, FRoomAuthComplete Complete)
{
	Complete(true, TEXT("local-anonymous"));
}

void ULocalRoomServiceBackend::PublishRoom(const FRoomServiceInfo& Room,
	FRoomPublishComplete Complete)
{
	Directory.Add(Room.RoomId, Room);
	Complete(true);
}

void ULocalRoomServiceBackend::RemoveRoom(const FString& RoomId)
{
	Directory.Remove(RoomId);
}

void ULocalRoomServiceBackend::DiscoverRooms(const FString& Identity,
	FRoomDiscoveryComplete Complete)
{
	TArray<FRoomServiceInfo> Rooms;
	Directory.GenerateValueArray(Rooms);
	Complete(MoveTemp(Rooms));
}

void ULocalRoomServiceBackend::IssueConnection(const FRoomServiceInfo& Room,
	const FString& Identity, const FString& Endpoint, FRoomConnectionComplete Complete)
{
	FRoomServiceConnection Connection;
	Connection.RoomId = Room.RoomId;
	Connection.Endpoint = Endpoint;
	Connection.Token = RoomServiceProtocol::NewSecret();
	Complete(true, MoveTemp(Connection));
}

void ULocalRoomServiceBackend::ValidateConnection(const FString& Presented, const FString& Issued,
	FRoomValidationComplete Complete)
{
	Complete(!Presented.IsEmpty() && Presented == Issued);
}
