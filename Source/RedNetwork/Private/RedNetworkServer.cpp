#include "RedNetworkServer.h"

#include "KCPWrap.h"
#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "HAL/UnrealMemory.h"
#include "..\Public\RedNetworkServer.h"

bool URedNetworkServer::Send(int32 ClientID, const TArray<uint8>& Data)
{
	if (!IsActive() || !Connections.Contains(ClientID)) return false;

	const FConnectionInfo& Info = Connections[ClientID];

	return Info.KCPUnit->Send(Data.GetData(), Data.Num()) == 0;
}

void URedNetworkServer::UDPSend(int32 ClientID, const uint8* Data, int32 Count)
{
	if (!IsActive() || !Connections.Contains(ClientID)) return;

	const FConnectionInfo& Info = Connections[ClientID];

	SendBuffer.SetNumUninitialized(8, false);

	Info.Pass.ToBytes(SendBuffer.GetData());

	if (Count != 0) SendBuffer.Append(Data, Count);

	int32 BytesSend;
	SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *Info.Addr);
}

void URedNetworkServer::UpdateKCP()
{
	int32 Current = FPlatformTime::Cycles64() / 1000;

	for (auto Info : Connections)
	{
		Info.Value.KCPUnit->Update(Current);
	}
}

void URedNetworkServer::SendHeartbeat()
{
	for (auto Info : Connections)
	{
		UDPSend(Info.Key, nullptr, 0);
	}
}

void URedNetworkServer::HandleSocketRecv()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	check(SocketPtr);
	int32 BytesRead;

	while (SocketPtr) {

		TSharedRef<FInternetAddr> SourceAddr = SocketSubsystem->CreateInternetAddr();

		RecvBuffer.SetNumUninitialized(65535, false);

		if (!SocketPtr->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SourceAddr)) break;

		if (RecvBuffer.Num() < 8) continue;
		RecvBuffer.SetNumUninitialized(BytesRead, false);

		FRedNetworkPass SourcePass(RecvBuffer.GetData());

		if (!SourcePass.IsValid())
		{
			SendReadyPass(SourceAddr);
			continue;
		}

		RedirectConnection(SourcePass, SourceAddr);
		RegisterConnection(SourcePass, SourceAddr);
	
		if (!Connections.Contains(SourcePass.ID)) continue;

		Connections[SourcePass.ID].RecvTime = NowTime;

		if (RecvBuffer.Num() == 8) continue;

		if (Connections.Contains(SourcePass.ID))
		{
			Connections[SourcePass.ID].KCPUnit->Input(RecvBuffer.GetData() + 8, RecvBuffer.Num() - 8);
		}
	}
}

void URedNetworkServer::SendReadyPass(const TSharedRef<FInternetAddr>& SourceAddr)
{
	FString SourceAddrStr = SourceAddr->ToString(true);

	if (!ReadyPass.Contains(SourceAddrStr))
	{
		FReadyInfo NewReadyPass;
		NewReadyPass.Time = NowTime;
		NewReadyPass.Pass.ID = NextReadyID++;
		NewReadyPass.Pass.RandKey();

		ReadyPass.Add(SourceAddrStr, NewReadyPass);

		UE_LOG(LogRedNetwork, Log, TEXT("Ready pass %i from %s."), NewReadyPass.Pass.ID, *SourceAddrStr);
	}

	const FRedNetworkPass& Pass = ReadyPass[SourceAddrStr].Pass;

	SendBuffer.SetNum(8, false);

	Pass.ToBytes(SendBuffer.GetData());

	int32 BytesSend;
	SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *SourceAddr);

	UE_LOG(LogRedNetwork, Log, TEXT("Send ready pass %i to %s."), Pass.ID, *SourceAddrStr);
}

void URedNetworkServer::RedirectConnection(const FRedNetworkPass& SourcePass, const TSharedRef<FInternetAddr>& SourceAddr)
{
	if (!Connections.Contains(SourcePass.ID) || Connections[SourcePass.ID].Pass.Key != SourcePass.Key) return;

	if (!(*Connections[SourcePass.ID].Addr == *SourceAddr))
	{
		UE_LOG(LogRedNetwork, Log, TEXT("Redirect connection %i from %s to %s."), SourcePass.ID, *Connections[SourcePass.ID].Addr->ToString(true), *SourceAddr->ToString(true));

		Connections[SourcePass.ID].Addr = SourceAddr;
	}
}

void URedNetworkServer::RegisterConnection(const FRedNetworkPass& SourcePass, const TSharedRef<FInternetAddr>& SourceAddr)
{
	FString SourceAddrStr = SourceAddr->ToString(true);

	if (!ReadyPass.Contains(SourceAddrStr)) return;
	if (ReadyPass[SourceAddrStr].Pass.ID != SourcePass.ID || ReadyPass[SourceAddrStr].Pass.Key != SourcePass.Key) return;

	FConnectionInfo NewConnections;
	NewConnections.Pass = SourcePass;
	NewConnections.RecvTime = NowTime;
	NewConnections.Heartbeat = FDateTime::MinValue();
	NewConnections.Addr = SourceAddr;

	NewConnections.KCPUnit = MakeShared<FKCPWrap>(NewConnections.Pass.ID, FString::Printf(TEXT("Server-%i"), NewConnections.Pass.ID));
	NewConnections.KCPUnit->SetTurboMode();
	NewConnections.KCPUnit->GetKCPCB().logmask = KCPLogMask;

	NewConnections.KCPUnit->OutputFunc = [this, ID = NewConnections.Pass.ID](const uint8* Data, int32 Count)->int32
	{
		UDPSend(ID, Data, Count);
		return 0;
	};

	Connections.Add(SourcePass.ID, NewConnections);

	ReadyPass.Remove(SourceAddrStr);

	UE_LOG(LogRedNetwork, Log, TEXT("Register connection %i."), SourcePass.ID);

	OnLogin.Broadcast(SourcePass.ID);
}

void URedNetworkServer::HandleKCPRecv()
{
	for (auto Info : Connections)
	{
		while (Info.Value.KCPUnit)
		{
			int32 Size = Info.Value.KCPUnit->PeekSize();

			if (Size < 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			Size = Info.Value.KCPUnit->Recv(RecvBuffer.GetData(), RecvBuffer.Num());

			if (Size < 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			OnRecv.Broadcast(Info.Key, RecvBuffer);
		}
	}
}

void URedNetworkServer::HandleExpiredReadyPass()
{
	TArray<FString> ReadyPassAddr;
	ReadyPass.GetKeys(ReadyPassAddr);

	for (const FString& Addr : ReadyPassAddr)
	{
		if (NowTime - ReadyPass[Addr].Time > TimeoutLimit)
		{
			UE_LOG(LogRedNetwork, Log, TEXT("Ready pass %i timeout."), ReadyPass[Addr].Pass.ID);

			ReadyPass.Remove(Addr);
		}
	}
}

void URedNetworkServer::HandleExpiredConnection()
{
	TArray<int32> ConnectionsAddr;
	Connections.GetKeys(ConnectionsAddr);

	for (int32 ID : ConnectionsAddr)
	{
		if (NowTime - Connections[ID].RecvTime > TimeoutLimit)
		{
			UE_LOG(LogRedNetwork, Log, TEXT("Connections connection %i timeout."), Connections[ID].Pass.ID);

			Connections.Remove(ID);

			OnUnlogin.Broadcast(ID);
		}
	}
}

void URedNetworkServer::Tick(float DeltaTime)
{
	if (!IsActive()) return;
	NowTime = FDateTime::Now();

	UpdateKCP();
	SendHeartbeat();
	HandleSocketRecv();
	HandleKCPRecv();
	HandleExpiredReadyPass();
	HandleExpiredConnection();
}

void URedNetworkServer::Activate(bool bReset)
{
	if (bReset) Deactivate();
	if (bIsActive) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket subsystem is nullptr."));
		return;
	}

	SocketPtr = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("Red Server Socket"));

	if (SocketPtr == nullptr)
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket creation failed."));
		return;
	}

	TSharedRef<FInternetAddr> ServerAddr = SocketSubsystem->CreateInternetAddr();

	ServerAddr->SetAnyAddress();
	ServerAddr->SetPort(Port);

	if (!SocketPtr->Bind(*ServerAddr))
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket bind failed."));
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	if (!SocketPtr->SetNonBlocking())
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket set non-blocking failed."));
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	NextReadyID = 1;

	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Server activate."));

	bIsActive = true;
}

void URedNetworkServer::Deactivate()
{
	if (!bIsActive) return;

	TArray<int32> ConnectionsAddr;
	Connections.GetKeys(ConnectionsAddr);

	for (int32 ID : ConnectionsAddr)
	{
		OnUnlogin.Broadcast(ID);
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	SocketSubsystem->DestroySocket(SocketPtr);

	SendBuffer.SetNum(0);
	RecvBuffer.SetNum(0);

	ReadyPass.Reset();
	Connections.Reset();

	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Server deactivate."));

	bIsActive = false;
}

void URedNetworkServer::BeginDestroy()
{
	Deactivate();

	Super::BeginDestroy();
}
