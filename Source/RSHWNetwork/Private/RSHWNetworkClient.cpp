#include "RSHWNetworkClient.h"

#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

URSHWNetworkClient::URSHWNetworkClient(const FObjectInitializer & ObjectInitializer)
	: ServerAddr(ISocketSubsystem::Get()->CreateInternetAddr())
{
	ServerAddr->SetLoopbackAddress();
	ServerAddr->SetPort(25565);
}

bool URSHWNetworkClient::SetHandler(TScriptInterface<IRSHWNetworkClientHandler> InHandlerObject)
{
	if (bIsRunning) return false;
	HandlerObject = InHandlerObject;
	return true;
}

bool URSHWNetworkClient::SetServerAddr(TSharedRef<FInternetAddr> InServerAddr)
{
	if (bIsRunning) return false;
	if (!InServerAddr->IsValid()) return false;
	ServerAddr = InServerAddr;
	return true;
}

bool URSHWNetworkClient::SetServerAddrByString(const FString & InServerAddr)
{
	if (bIsRunning) return false;
	
	TSharedRef<FInternetAddr> NewServerAddr = ISocketSubsystem::Get()->CreateInternetAddr();

	bool bIsValid;
	NewServerAddr->SetPort(ServerAddr->GetPort());
	NewServerAddr->SetIp(*InServerAddr, bIsValid);

	if (bIsValid)
	{
		ServerAddr = NewServerAddr;
	}
	else
	{
		return false;
	}

	return true;
}

bool URSHWNetworkClient::Send(const TArray<uint8>& Data)
{
	if (!bIsRunning || !(ClientPass.ID | ClientPass.Key)) return false;

	SendBuffer.SetNumUninitialized(8, false);

	SendBuffer[0] = ClientPass.ID >> 0;
	SendBuffer[1] = ClientPass.ID >> 8;
	SendBuffer[2] = ClientPass.ID >> 16;
	SendBuffer[3] = ClientPass.ID >> 24;

	SendBuffer[4] = ClientPass.Key >> 0;
	SendBuffer[5] = ClientPass.Key >> 8;
	SendBuffer[6] = ClientPass.Key >> 16;
	SendBuffer[7] = ClientPass.Key >> 24;

	SendBuffer.Append(Data);

	int32 BytesSend;
	return SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddr) && BytesSend == SendBuffer.Num();
}

void URSHWNetworkClient::Login()
{
	if (bIsRunning) return;

	if (HandlerObject.GetInterface() == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("HandlerObject is nullptr in '%s'."), *GetName());
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();

	if (SocketSubsystem == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket subsystem is nullptr in '%s'."), *GetName());
		HandlerObject->OnLoginFailure();
		return;
	}

	SocketPtr = SocketSubsystem->CreateSocket(NAME_DGram, FString::Printf(TEXT("RSHW Client Socket in '%s'."), *GetName()));

	if (SocketPtr == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket creation failed in '%s'."), *GetName());
		HandlerObject->OnLoginFailure();
		return;
	}

	if (!SocketPtr->SetNonBlocking())
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket set non-blocking failed in '%s'."), *GetName());
		SocketSubsystem->DestroySocket(SocketPtr);
		HandlerObject->OnLoginFailure();
		return;
	}

	ClientPass.ID = 0;
	ClientPass.Key = 0;
	bIsRunning = true;
	StartupTime = FDateTime::Now();
	LastRecvTime = StartupTime;
	LastHeartbeat = FDateTime::MinValue();
	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network client '%s' try login."), *GetName());

}

void URSHWNetworkClient::Unlogin()
{
	if (!bIsRunning) return;

	HandlerObject->OnUnlogin();

	ResetRunningData();

	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network client '%s' unlogin."), *GetName());
}

void URSHWNetworkClient::ResetRunningData()
{
	bIsRunning = false;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	SocketSubsystem->DestroySocket(SocketPtr);

	SendBuffer.SetNum(0);
	RecvBuffer.SetNum(0);
	DataBuffer.SetNum(0);
}

void URSHWNetworkClient::BeginDestroy()
{
	Unlogin();

	Super::BeginDestroy();
}

void URSHWNetworkClient::Tick(float DeltaTime)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);

	const FDateTime NowTime = FDateTime::Now();

	// send heartbeat 
	{
		if (NowTime - LastHeartbeat > FTimespan::FromSeconds(1.0))
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
			if (SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *ServerAddr) && BytesSend == SendBuffer.Num())
			{
				LastHeartbeat = NowTime;
			}
		}
	}

	// handle socket recv
	{
		int32 BytesRead;
		TSharedRef<FInternetAddr> SourceAddr = SocketSubsystem->CreateInternetAddr();

		while (true) {

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
				HandlerObject->OnLogin();
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
				DataBuffer.SetNumUninitialized(RecvBuffer.Num() - 8, false);
				FMemory::Memcpy(DataBuffer.GetData(), RecvBuffer.GetData() + 8, RecvBuffer.Num() - 8);
				HandlerObject->OnRecv(DataBuffer);
			}
		}
	}

	// handle login timeout
	{
		if (!(ClientPass.ID | ClientPass.Key) && NowTime - StartupTime > TimeoutLimit)
		{
			ResetRunningData();

			UE_LOG(LogRSHWNetwork, Warning, TEXT("RSHW network client '%s' login timeout."), *GetName());
			HandlerObject->OnLoginFailure();
		}
	}

	// handle timeout
	{
		if ((ClientPass.ID | ClientPass.Key) && NowTime - LastRecvTime > TimeoutLimit)
		{
			ResetRunningData();

			UE_LOG(LogRSHWNetwork, Warning, TEXT("RSHW network client '%s' timeout."), *GetName());
			HandlerObject->OnUnlogin();
		}
	}
}
