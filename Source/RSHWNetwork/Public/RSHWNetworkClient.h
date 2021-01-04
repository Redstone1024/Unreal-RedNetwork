#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/Object.h"
#include "RSHWNetworkType.h"
#include "RSHWNetworkClient.generated.h"

class FKCPWrap;
class FInternetAddr;

UCLASS(BlueprintType)
class RSHWNETWORK_API URSHWNetworkClient : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FLoginSignature, URSHWNetworkClient, OnLogin);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FRecvSignature, URSHWNetworkClient, OnRecv, const TArray<uint8>&, Data);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FUnloginSignature, URSHWNetworkClient, OnUnlogin);

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
	bool IsLogged() const { return ClientPass.ID | ClientPass.Key; }

	UFUNCTION(BlueprintCallable, Category = "RSHW|Network")
	bool Send(const TArray<uint8>& Data);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FString ServerAddr = TEXT("127.0.0.1:25565");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan Heartbeat = FTimespan::FromSeconds(1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	int32 KCPLogMask = 0;

private:

	bool bIsActive = false;

	TSharedPtr<FInternetAddr> ServerAddrPtr;

	FSocket* SocketPtr;

	TArray<uint8> SendBuffer;
	TArray<uint8> RecvBuffer;
	TArray<uint8> DataBuffer;

	FRSHWNetworkPass ClientPass;

	FDateTime LastRecvTime;
	FDateTime LastHeartbeat;

	TSharedPtr<FKCPWrap> KCPUnit;

	int32 UDPSend(const uint8* Data, int32 Count);

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
