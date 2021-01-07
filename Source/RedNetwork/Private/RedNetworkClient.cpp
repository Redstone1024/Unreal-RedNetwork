#include "RedNetworkClient.h"

#include "KCPWrap.h"
#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "..\Public\RedNetworkClient.h"

bool URedNetworkClient::Send(uint8 Channel, const TArray<uint8>& Data)
{
	if (!IsActive() || !IsLogged()) return false;

	EnsureChannelCreated(Channel);

	return KCPUnits[Channel]->Send(Data.GetData(), Data.Num()) == 0;
}

void URedNetworkClient::UpdateKCP()
{
	int32 Current = FPlatformTime::Cycles64() / 1000;

	for (auto KCPUnit : KCPUnits)
	{
		if (!KCPUnit) continue;

		KCPUnit->Update(Current);
	}
}

void URedNetworkClient::SendHeartbeat()
{
	SendBuffer.SetNumUninitialized(8, false);

	ClientPass.ToBytes(SendBuffer.GetData());

	int32 BytesSend;
	SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddrPtr);
}

void URedNetworkClient::HandleSocketRecv()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	check(SocketPtr);
	int32 BytesRead;

	while (SocketPtr) {

		TSharedRef<FInternetAddr> SourceAddr = SocketSubsystem->CreateInternetAddr();

		RecvBuffer.SetNumUninitialized(65535, false);

		if (!SocketPtr->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SourceAddr)) break;

		if (BytesRead < 8) continue;
		RecvBuffer.SetNumUninitialized(BytesRead, false);

		FRedNetworkPass SourcePass(RecvBuffer.GetData());

		HandleLoginRecv(SourcePass);

		if (!IsLogged()) continue;

		if (SourcePass.ID == ClientPass.ID && SourcePass.Key == ClientPass.Key)
		{
			LastRecvTime = NowTime;
		}

		if (RecvBuffer.Num() < 9) continue;

		if (SourcePass.ID == ClientPass.ID && SourcePass.Key == ClientPass.Key)
		{
			uint8 Channel = RecvBuffer[8];

			EnsureChannelCreated(Channel);

			KCPUnits[Channel]->Input(RecvBuffer.GetData() + 9, RecvBuffer.Num() - 9);
		}
	}
}

void URedNetworkClient::HandleLoginRecv(const FRedNetworkPass & SourcePass)
{
	if (IsLogged()) return;

	ClientPass = SourcePass;

	KCPUnits.SetNum(256);

	OnLogin.Broadcast();
}

void URedNetworkClient::HandleKCPRecv()
{
	for (int32 Channel = 0; Channel < KCPUnits.Num(); ++Channel)
	{
		const TSharedPtr<FKCPWrap>& KCPUnit = KCPUnits[Channel];

		while (KCPUnit)
		{
			int32 Size = KCPUnit->PeekSize();

			if (Size < 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			Size = KCPUnit->Recv(RecvBuffer.GetData(), RecvBuffer.Num());

			if (Size < 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			OnRecv.Broadcast(Channel, RecvBuffer);
		}
	}
}

void URedNetworkClient::HandleTimeout()
{
	if (IsLogged() && NowTime - LastRecvTime > TimeoutLimit)
	{
		ClientPass.Reset();

		KCPUnits.SetNum(0);

		UE_LOG(LogRedNetwork, Warning, TEXT("Red Network Client timeout."));

		OnUnlogin.Broadcast();
	}
}

void URedNetworkClient::EnsureChannelCreated(uint8 Channel)
{
	if (KCPUnits[Channel]) return;

	TSharedPtr<FKCPWrap> KCPUnit = MakeShared<FKCPWrap>(0, FString::Printf(TEXT("Client-%i:%i"), ClientPass.ID, Channel));
	KCPUnit->SetTurboMode();
	KCPUnit->GetKCPCB().logmask = KCPLogMask;

	KCPUnit->OutputFunc = [this, Channel](const uint8* Data, int32 Count)->int32
	{
		SendBuffer.SetNumUninitialized(9, false);

		ClientPass.ToBytes(SendBuffer.GetData());

		SendBuffer[8] = Channel;

		if (Count != 0) SendBuffer.Append(Data, Count);

		int32 BytesSend;
		SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddrPtr);

		return 0;
	};

	KCPUnits[Channel] = KCPUnit;
}

void URedNetworkClient::Tick(float DeltaTime)
{
	if (!IsActive()) return;

	NowTime = FDateTime::Now();

	UpdateKCP();
	SendHeartbeat();
	HandleSocketRecv();
	HandleKCPRecv();
	HandleTimeout();
}

void URedNetworkClient::Activate(bool bReset)
{
	if (bReset) Deactivate();
	if (bIsActive) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket subsystem is nullptr."));
		return;
	}

	ServerAddrPtr = SocketSubsystem->CreateInternetAddr();

	bool bIsValid = false;
	ServerAddrPtr->SetPort(25565);
	ServerAddrPtr->SetIp(*ServerAddr, bIsValid);

	if (!bIsValid)
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Server addr invalid."));
		ServerAddrPtr = nullptr;
		return;
	}

	SocketPtr = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("Red Client Socket"));

	if (SocketPtr == nullptr)
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket creation failed."));
		return;
	}

	if (!SocketPtr->SetNonBlocking())
	{
		UE_LOG(LogRedNetwork, Error, TEXT("Socket set non-blocking failed."));
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	ClientPass.Reset();
	LastRecvTime = FDateTime::Now();
	LastHeartbeat = FDateTime::MinValue();
	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Client activate."));

	bIsActive = true;
}

void URedNetworkClient::Deactivate()
{
	if (!bIsActive) return;

	if (IsLogged())
	{
		OnUnlogin.Broadcast();
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	SocketSubsystem->DestroySocket(SocketPtr);

	SendBuffer.SetNum(0);
	RecvBuffer.SetNum(0);

	ClientPass.Reset();

	KCPUnits.SetNum(0);

	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Client deactivate."));

	bIsActive = false;
}

void URedNetworkClient::BeginDestroy()
{
	Deactivate();

	Super::BeginDestroy();
}
