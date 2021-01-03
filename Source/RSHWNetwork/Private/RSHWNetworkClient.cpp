#include "RSHWNetworkClient.h"

#include "KCPWrap.h"
#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

URSHWNetworkClient::URSHWNetworkClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool URSHWNetworkClient::Send(const TArray<uint8>& Data)
{
	if (!IsActive() || !(ClientPass.ID | ClientPass.Key)) return false;

	return KCPUnit->Send(Data.GetData(), Data.Num());
}

int32 URSHWNetworkClient::UDPSend(const uint8 * Data, int32 Count)
{
	if (!IsActive() || !(ClientPass.ID | ClientPass.Key)) return false;

	SendBuffer.SetNumUninitialized(8, false);

	SendBuffer[0] = ClientPass.ID >> 0;
	SendBuffer[1] = ClientPass.ID >> 8;
	SendBuffer[2] = ClientPass.ID >> 16;
	SendBuffer[3] = ClientPass.ID >> 24;

	SendBuffer[4] = ClientPass.Key >> 0;
	SendBuffer[5] = ClientPass.Key >> 8;
	SendBuffer[6] = ClientPass.Key >> 16;
	SendBuffer[7] = ClientPass.Key >> 24;

	SendBuffer.Append(Data, Count);

	int32 BytesSend;
	SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddrPtr);
	return 0;
}

void URSHWNetworkClient::BeginPlay()
{
	Super::BeginPlay();
}

void URSHWNetworkClient::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();

	Super::EndPlay(EndPlayReason);
}

void URSHWNetworkClient::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive()) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);

	const FDateTime NowTime = FDateTime::Now();

	// update kcp
	if (KCPUnit)
	{
		int32 Current = FPlatformTime::Cycles64() / 1000;

		KCPUnit->Update(Current);
	}

	// send heartbeat 
	{
		if (NowTime - LastHeartbeat > Heartbeat)
		{
			SendBuffer.SetNumUninitialized(8, false);

			SendBuffer[0] = ClientPass.ID >> 0;
			SendBuffer[1] = ClientPass.ID >> 8;
			SendBuffer[2] = ClientPass.ID >> 16;
			SendBuffer[3] = ClientPass.ID >> 24;

			SendBuffer[4] = ClientPass.Key >> 0;
			SendBuffer[5] = ClientPass.Key >> 8;
			SendBuffer[6] = ClientPass.Key >> 16;
			SendBuffer[7] = ClientPass.Key >> 24;

			int32 BytesSend;
			if (SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddrPtr) && BytesSend == SendBuffer.Num())
			{
				LastHeartbeat = NowTime;
			}
		}
	}

	// handle socket recv
	{
		int32 BytesRead;
		TSharedRef<FInternetAddr> SourceAddr = SocketSubsystem->CreateInternetAddr();

		while (SocketPtr) {

			RecvBuffer.SetNumUninitialized(65535, false);

			if (!SocketPtr->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), BytesRead, *SourceAddr)) break;

			if (BytesRead < 8) continue;
			RecvBuffer.SetNumUninitialized(BytesRead, false);

			FRSHWNetworkPass SourcePass;
			SourcePass.ID = 0;
			SourcePass.Key = 0;

			SourcePass.ID |= (int32)RecvBuffer[0] << 0;
			SourcePass.ID |= (int32)RecvBuffer[1] << 8;
			SourcePass.ID |= (int32)RecvBuffer[2] << 16;
			SourcePass.ID |= (int32)RecvBuffer[3] << 24;

			SourcePass.Key |= (int32)RecvBuffer[4] << 0;
			SourcePass.Key |= (int32)RecvBuffer[5] << 8;
			SourcePass.Key |= (int32)RecvBuffer[6] << 16;
			SourcePass.Key |= (int32)RecvBuffer[7] << 24;

			// is registration request
			if (!(ClientPass.ID | ClientPass.Key))
			{
				ClientPass = SourcePass;

				KCPUnit = MakeShared<FKCPWrap>(ClientPass.ID, GetName());
				KCPUnit->SetTurboMode();
				KCPUnit->GetKCPCB().logmask = KCPLogMask;

				KCPUnit->OutputFunc = [this](const uint8* Data, int32 Count)->int32
				{
					return UDPSend(Data, Count);
				};

				OnLogin.Broadcast();
			}

			if (SourcePass.ID == ClientPass.ID && SourcePass.Key == ClientPass.Key)
			{
				LastRecvTime = NowTime;
			}

			// is heartbeat request
			if ((SourcePass.ID | SourcePass.Key) && RecvBuffer.Num() == 8) continue;

			// is server request
			if (SourcePass.ID == ClientPass.ID && SourcePass.Key == ClientPass.Key)
			{
				KCPUnit->Input(RecvBuffer.GetData() + 8, RecvBuffer.Num() - 8);
			}
		}
	}

	// handle kcp recv
	{
		while (KCPUnit)
		{
			int32 Size = KCPUnit->PeekSize();

			if (Size <= 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			Size = KCPUnit->Recv(RecvBuffer.GetData(), RecvBuffer.Num());

			if (Size <= 0) break;

			RecvBuffer.SetNumUninitialized(Size, false);

			OnRecv.Broadcast(RecvBuffer);
		}
	}

	// handle timeout
	{
		if ((ClientPass.ID | ClientPass.Key) && NowTime - LastRecvTime > TimeoutLimit)
		{
			ClientPass.ID = 0;
			ClientPass.Key = 0;

			KCPUnit = nullptr;

			UE_LOG(LogRSHWNetwork, Warning, TEXT("RSHW network client '%s' timeout."), *GetName());

			OnUnlogin.Broadcast();
		}
	}
}

void URSHWNetworkClient::Activate(bool bReset)
{
	if (!GetOwner()->GetGameInstance()) return;
	if (bReset) Deactivate();
	if (!ShouldActivate()) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket subsystem is nullptr in '%s'."), *GetName());
		return;
	}

	ServerAddrPtr = SocketSubsystem->CreateInternetAddr();

	bool bIsValid = false;
	ServerAddrPtr->SetPort(25565);
	ServerAddrPtr->SetIp(*ServerAddr, bIsValid);

	if (!bIsValid)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Server addr invalid in '%s'."), *GetName());
		ServerAddrPtr = nullptr;
		return;
	}

	SocketPtr = SocketSubsystem->CreateSocket(NAME_DGram, FString::Printf(TEXT("RSHW Client Socket in '%s'."), *GetName()));

	if (SocketPtr == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket creation failed in '%s'."), *GetName());
		return;
	}

	if (!SocketPtr->SetNonBlocking())
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket set non-blocking failed in '%s'."), *GetName());
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	ClientPass.ID = 0;
	ClientPass.Key = 0;
	LastRecvTime = FDateTime::Now();
	LastHeartbeat = FDateTime::MinValue();
	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network client '%s' activate."), *GetName());

	SetComponentTickEnabled(true);
	SetActiveFlag(true);

	OnComponentActivated.Broadcast(this, bReset);
}

void URSHWNetworkClient::Deactivate()
{
	if (ShouldActivate()) return;

	if (ClientPass.ID | ClientPass.Key)
	{
		OnUnlogin.Broadcast();
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	SocketSubsystem->DestroySocket(SocketPtr);

	SendBuffer.SetNum(0);
	RecvBuffer.SetNum(0);
	DataBuffer.SetNum(0);

	ClientPass.ID = 0;
	ClientPass.Key = 0;

	KCPUnit = nullptr;

	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network client '%s' deactivate."), *GetName());

	SetComponentTickEnabled(false);
	SetActiveFlag(false);

	OnComponentDeactivated.Broadcast(this);
}

void URSHWNetworkClient::BeginDestroy()
{
	Deactivate();

	Super::BeginDestroy();
}
