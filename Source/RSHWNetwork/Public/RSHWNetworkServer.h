#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/Object.h"
#include "RSHWNetworkType.h"
#include "RSHWNetworkServer.generated.h"

class FSocket;
class FKCPWrap;

UCLASS(BlueprintType)
class RSHWNETWORK_API URSHWNetworkServer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FLoginSignature, URSHWNetworkServer, OnLogin, int32, ClientID);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FRecvSignature, URSHWNetworkServer, OnRecv, int32, ClientID, const TArray<uint8>&, Data);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FUnloginSignature, URSHWNetworkServer, OnUnlogin, int32, ClientID);

public:

	UPROPERTY(BlueprintAssignable, Category = "RSHW|Network")
	FLoginSignature OnLogin;

	UPROPERTY(BlueprintAssignable, Category = "RSHW|Network")
	FRecvSignature OnRecv;

	UPROPERTY(BlueprintAssignable, Category = "RSHW|Network")
	FUnloginSignature OnUnlogin;

public:

	UFUNCTION(BlueprintCallable, Category = "RSHW|Network")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintCallable, Category = "RSHW|Network")
	void Activate(bool bReset = false);

	UFUNCTION(BlueprintCallable, Category = "RSHW|Network")
	void Deactivate();

	UFUNCTION(BlueprintCallable, Category = "RSHW|Network")
	bool Send(int32 ClientID, const TArray<uint8>& Data);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	int32 Port = 25565;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan Heartbeat = FTimespan::FromSeconds(1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	int32 KCPLogMask = 0;

private:

	bool bIsActive = false;

	FSocket* SocketPtr;

	TArray<uint8> SendBuffer;
	TArray<uint8> RecvBuffer;
	TArray<uint8> DataBuffer;

	int32 NextRegistrationID;
		
	struct FPreRegistrationInfo
	{
		FDateTime Time;
		FRSHWNetworkPass Pass;
	};

	TMap<FString, FPreRegistrationInfo> PreRegistration;

	struct FRegistrationInfo
	{
		FRSHWNetworkPass Pass;
		FDateTime RecvTime;
		FDateTime Heartbeat;
		TSharedPtr<FInternetAddr> Addr;
		TSharedPtr<FKCPWrap> KCPUnit;
	};

	TMap<int32, FRegistrationInfo> Registration;

	int32 UDPSend(int32 ClientID, const uint8* Data, int32 Count);

public:

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate() && IsActive(); }
	virtual TStatId GetStatId() const override { return GetStatID(); }
	//~ End FTickableGameObject Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

};
