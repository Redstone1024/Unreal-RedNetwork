#include "RedNetworkServer.h"

#include "KCPWrap.h"
#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "HAL/UnrealMemory.h"

bool URedNetworkServer::Send(int32 ClientID, const TArray<uint8>& Data)
{
	if (!IsActive() || !Registration.Contains(ClientID)) return false;

	const FRegistrationInfo& Info = Registration[ClientID];

	return !Info.KCPUnit->Send(Data.GetData(), Data.Num());
}

int32 URedNetworkServer::UDPSend(int32 ClientID, const uint8* Data, int32 Count)
{
	if (!IsActive() || !Registration.Contains(ClientID)) return false;

	const FRegistrationInfo& Info = Registration[ClientID];

	SendBuffer.SetNumUninitialized(8, false);

	SendBuffer[0] = Info.Pass.ID >> 0;
	SendBuffer[1] = Info.Pass.ID >> 8;
	SendBuffer[2] = Info.Pass.ID >> 16;
	SendBuffer[3] = Info.Pass.ID >> 24;

	SendBuffer[4] = Info.Pass.Key >> 0;
	SendBuffer[5] = Info.Pass.Key >> 8;
	SendBuffer[6] = Info.Pass.Key >> 16;
	SendBuffer[7] = Info.Pass.Key >> 24;

	SendBuffer.Append(Data, Count);

	int32 BytesSend;
	SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *Info.Addr);
	return 0;
}

void URedNetworkServer::Tick(float DeltaTime)
{
	if (!IsActive()) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);

	const FDateTime NowTime = FDateTime::Now();

	// update kcp
	{
		TArray<int32> RegistrationAddr;
		Registration.GetKeys(RegistrationAddr);

		int32 Current = FPlatformTime::Cycles64() / 1000;

		for (int32 ID : RegistrationAddr)
		{
			Registration[ID].KCPUnit->Update(Current);
		}
	}

	// send heartbeat 
	{
		TArray<int32> RegistrationAddr;
		Registration.GetKeys(RegistrationAddr);

		for (int32 ID : RegistrationAddr)
		{
			if (NowTime - Registration[ID].Heartbeat > Heartbeat)
			{
				SendBuffer.SetNum(8, false);

				SendBuffer[0] = Registration[ID].Pass.ID >> 0;
				SendBuffer[1] = Registration[ID].Pass.ID >> 8;
				SendBuffer[2] = Registration[ID].Pass.ID >> 16;
				SendBuffer[3] = Registration[ID].Pass.ID >> 24;

				SendBuffer[4] = Registration[ID].Pass.Key >> 0;
				SendBuffer[5] = Registration[ID].Pass.Key >> 8;
				SendBuffer[6] = Registration[ID].Pass.Key >> 16;
				SendBuffer[7] = Registration[ID].Pass.Key >> 24;

				int32 BytesSend;
				if (SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *Registration[ID].Addr) && BytesSend == SendBuffer.Num())
				{
					Registration[ID].Heartbeat = NowTime;
				}
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

			FRedNetworkPass SourcePass;
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

			FString SourceAddrStr = SourceAddr->ToString(true);

			// is pre-register pass request
			if (!(SourcePass.ID | SourcePass.Key))
			{
				if (!PreRegistration.Contains(SourceAddrStr))
				{
					FPreRegistrationInfo NewRegistration;
					NewRegistration.Time = NowTime;
					NewRegistration.Pass.ID = NextRegistrationID++;
					NewRegistration.Pass.Key ^= FMath::Rand() << 0;
					NewRegistration.Pass.Key ^= FMath::Rand() << 8;
					NewRegistration.Pass.Key ^= FMath::Rand() << 16;
					NewRegistration.Pass.Key ^= FMath::Rand() << 24;

					PreRegistration.Add(SourceAddrStr, NewRegistration);

					UE_LOG(LogRedNetwork, Log, TEXT("Pre-register pass %i from %s."), NewRegistration.Pass.ID, *SourceAddrStr);
				}

				const FRedNetworkPass& Pass = PreRegistration[SourceAddrStr].Pass;

				SendBuffer.SetNum(8, false);

				SendBuffer[0] = Pass.ID >> 0;
				SendBuffer[1] = Pass.ID >> 8;
				SendBuffer[2] = Pass.ID >> 16;
				SendBuffer[3] = Pass.ID >> 24;

				SendBuffer[4] = Pass.Key >> 0;
				SendBuffer[5] = Pass.Key >> 8;
				SendBuffer[6] = Pass.Key >> 16;
				SendBuffer[7] = Pass.Key >> 24;

				int32 BytesSend;
				if (SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *SourceAddr) && BytesSend == SendBuffer.Num())
				{
					UE_LOG(LogRedNetwork, Log, TEXT("Send pre-registration pass %i to %s."), Pass.ID, *SourceAddrStr);
				}
			}
			else
			{
				// redirect connection
				if (Registration.Contains(SourcePass.ID))
				{
					if (!(*Registration[SourcePass.ID].Addr == *SourceAddr))
					{
						Registration[SourcePass.ID].Addr = SourceAddr;

						UE_LOG(LogRedNetwork, Log, TEXT("Redirect connection %i."), SourcePass.ID);
					}

				}

				// register connection
				{
					bool bIsValidRegistration = false;
					if (PreRegistration.Contains(SourceAddrStr))
					{
						if (PreRegistration[SourceAddrStr].Pass.ID == SourcePass.ID && PreRegistration[SourceAddrStr].Pass.Key == SourcePass.Key)
						{
							bIsValidRegistration = true;
						}
					}

					if (bIsValidRegistration)
					{
						FRegistrationInfo NewRegistration;
						NewRegistration.Pass = SourcePass;
						NewRegistration.RecvTime = NowTime;
						NewRegistration.Heartbeat = FDateTime::MinValue();
						NewRegistration.Addr = SourceAddr;

						NewRegistration.KCPUnit = MakeShared<FKCPWrap>(NewRegistration.Pass.ID, FString::Printf(TEXT("Server-%i"), NewRegistration.Pass.ID));
						NewRegistration.KCPUnit->SetTurboMode();
						NewRegistration.KCPUnit->GetKCPCB().logmask = KCPLogMask;

						NewRegistration.KCPUnit->OutputFunc = [this, ID = NewRegistration.Pass.ID](const uint8* Data, int32 Count) -> int32
						{
							return UDPSend(ID, Data, Count);
						};

						Registration.Add(SourcePass.ID, NewRegistration);

						PreRegistration.Remove(SourceAddrStr);

						UE_LOG(LogRedNetwork, Log, TEXT("Register connection %i."), SourcePass.ID);

						OnLogin.Broadcast(SourcePass.ID);
					}
				}
			}

			if (Registration.Contains(SourcePass.ID))
			{
				Registration[SourcePass.ID].RecvTime = NowTime;
			}

			// is heartbeat request
			if ((SourcePass.ID | SourcePass.Key) && RecvBuffer.Num() == 8) continue;

			// is client request
			if (Registration.Contains(SourcePass.ID))
			{
				Registration[SourcePass.ID].KCPUnit->Input(RecvBuffer.GetData() + 8, RecvBuffer.Num() - 8);
			}
		}
	}

	// handle pre-registration timeout
	{
		TArray<FString> PreRegistrationAddr;
		PreRegistration.GetKeys(PreRegistrationAddr);

		for (const FString& Addr : PreRegistrationAddr)
		{
			if (NowTime - PreRegistration[Addr].Time > TimeoutLimit)
			{
				UE_LOG(LogRedNetwork, Log, TEXT("Pre-registration pass %i timeout."), PreRegistration[Addr].Pass.ID);

				PreRegistration.Remove(Addr);
			}
		}
	}

	// handle running timeout
	{
		TArray<int32> RegistrationAddr;
		Registration.GetKeys(RegistrationAddr);

		for (int32 ID : RegistrationAddr)
		{
			if (NowTime - Registration[ID].RecvTime > TimeoutLimit)
			{
				UE_LOG(LogRedNetwork, Log, TEXT("Registration connection %i timeout."), Registration[ID].Pass.ID);

				Registration.Remove(ID);

				OnUnlogin.Broadcast(ID);
			}
		}
	}

	// handle kcp recv
	{
		TArray<int32> RegistrationAddr;
		Registration.GetKeys(RegistrationAddr);

		for (int32 ID : RegistrationAddr)
		{
			while (Registration[ID].KCPUnit)
			{
				int32 Size = Registration[ID].KCPUnit->PeekSize();

				if (Size <= 0) break;

				RecvBuffer.SetNumUninitialized(Size, false);

				Size = Registration[ID].KCPUnit->Recv(RecvBuffer.GetData(), RecvBuffer.Num());

				if (Size <= 0) break;

				RecvBuffer.SetNumUninitialized(Size, false);

				OnRecv.Broadcast(ID, RecvBuffer);
			}
		}
	}
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

	NextRegistrationID = 1;

	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Server activate."));

	bIsActive = true;
}

void URedNetworkServer::Deactivate()
{
	if (!bIsActive) return;

	TArray<int32> RegistrationAddr;
	Registration.GetKeys(RegistrationAddr);

	for (int32 ID : RegistrationAddr)
	{
		OnUnlogin.Broadcast(ID);
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);
	SocketSubsystem->DestroySocket(SocketPtr);

	SendBuffer.SetNum(0);
	RecvBuffer.SetNum(0);
	DataBuffer.SetNum(0);

	PreRegistration.Reset();
	Registration.Reset();

	UE_LOG(LogRedNetwork, Log, TEXT("Red Network Server deactivate."));

	bIsActive = false;
}

void URedNetworkServer::BeginDestroy()
{
	Deactivate();

	Super::BeginDestroy();
}
