#include "RSHWNetworkServer.h"

#include "Logging.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "HAL/UnrealMemory.h"

URSHWNetworkServer::URSHWNetworkServer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool URSHWNetworkServer::Send(int32 ClientID, const TArray<uint8>& Data)
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

	SendBuffer.Append(Data);

	int32 BytesSend;
	return SocketPtr->SendTo(SendBuffer.GetData(), SendBuffer.Num(), BytesSend, *Info.Addr) && BytesSend == SendBuffer.Num();
}

void URSHWNetworkServer::BeginPlay()
{
	Super::BeginPlay();
}

void URSHWNetworkServer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Deactivate();

	Super::EndPlay(EndPlayReason);
}

void URSHWNetworkServer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsActive()) return;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get();
	check(SocketSubsystem);

	const FDateTime NowTime = FDateTime::Now();

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

					UE_LOG(LogRSHWNetwork, Log, TEXT("Pre register pass [ %s - %i:%i ] in '%s'."), *SourceAddrStr, NewRegistration.Pass.ID, NewRegistration.Pass.Key, *GetName());
				}

				const FRSHWNetworkPass& Pass = PreRegistration[SourceAddrStr].Pass;

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
					UE_LOG(LogRSHWNetwork, Log, TEXT("Send pre registration pass [ %s - %i:%i ] in '%s'."), *SourceAddrStr, Pass.ID, Pass.Key, *GetName());
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

						UE_LOG(LogRSHWNetwork, Log, TEXT("Redirect connection [ %i:%i ] in '%s'."), SourcePass.ID, SourcePass.Key, *GetName());
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

						Registration.Add(SourcePass.ID, NewRegistration);

						PreRegistration.Remove(SourceAddrStr);

						UE_LOG(LogRSHWNetwork, Log, TEXT("Register connection [ %i:%i ] in '%s'."), SourcePass.ID, SourcePass.Key, *GetName());

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
				DataBuffer.SetNumUninitialized(RecvBuffer.Num() - 8, false);
				FMemory::Memcpy(DataBuffer.GetData(), RecvBuffer.GetData() + 8, RecvBuffer.Num() - 8);
				OnRecv.Broadcast(SourcePass.ID, DataBuffer);
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
				UE_LOG(LogRSHWNetwork, Log, TEXT("Pre-registration pass [ %i:%i ] timeout in '%s'."), PreRegistration[Addr].Pass.ID, PreRegistration[Addr].Pass.Key, *GetName());

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
				UE_LOG(LogRSHWNetwork, Log, TEXT("Registration pass [ %i:%i ] timeout in '%s'."), Registration[ID].Pass.ID, Registration[ID].Pass.Key, *GetName());

				Registration.Remove(ID);

				OnUnlogin.Broadcast(ID);
			}
		}
	}
}

void URSHWNetworkServer::Activate(bool bReset)
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

	SocketPtr = SocketSubsystem->CreateSocket(NAME_DGram, FString::Printf(TEXT("RSHW Server Socket in '%s'."), *GetName()));

	if (SocketPtr == nullptr)
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket creation failed in '%s'."), *GetName());
		return;
	}

	TSharedRef<FInternetAddr> ServerAddr = SocketSubsystem->CreateInternetAddr();

	ServerAddr->SetAnyAddress();
	ServerAddr->SetPort(Port);

	if (!SocketPtr->Bind(*ServerAddr))
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket bind failed in '%s'."), *GetName());
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	if (!SocketPtr->SetNonBlocking())
	{
		UE_LOG(LogRSHWNetwork, Error, TEXT("Socket set non-blocking failed in '%s'."), *GetName());
		SocketSubsystem->DestroySocket(SocketPtr);
		return;
	}

	NextRegistrationID = 1;

	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network server '%s' activate."), *GetName());

	SetComponentTickEnabled(true);
	SetActiveFlag(true);

	OnComponentActivated.Broadcast(this, bReset);
}

void URSHWNetworkServer::Deactivate()
{
	if (ShouldActivate()) return;

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

	UE_LOG(LogRSHWNetwork, Log, TEXT("RSHW network server '%s' deactivate."), *GetName());

	SetComponentTickEnabled(false);
	SetActiveFlag(false);

	OnComponentDeactivated.Broadcast(this);
}

void URSHWNetworkServer::BeginDestroy()
{
	Deactivate();

	Super::BeginDestroy();
}
