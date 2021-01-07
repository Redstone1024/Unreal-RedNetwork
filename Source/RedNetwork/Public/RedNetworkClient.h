#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/Object.h"
#include "RedNetworkType.h"
#include "RedNetworkClient.generated.h"

class FKCPWrap;
class FInternetAddr;

UCLASS(BlueprintType)
class REDNETWORK_API URedNetworkClient : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FLoginSignature, URedNetworkClient, OnLogin);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FRecvSignature, URedNetworkClient, OnRecv, uint8, Channel, const TArray<uint8>&, Data);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE(FUnloginSignature, URedNetworkClient, OnUnlogin);

public:

	UPROPERTY(BlueprintAssignable, Category = "Red|Network")
	FLoginSignature OnLogin;

	UPROPERTY(BlueprintAssignable, Category = "Red|Network")
	FRecvSignature OnRecv;

	UPROPERTY(BlueprintAssignable, Category = "Red|Network")
	FUnloginSignature OnUnlogin;

public:

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	bool IsActive() const { return bIsActive; }

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	void Activate(bool bReset = false);

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	void Deactivate();

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	bool IsLogged() const { return ClientPass.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	bool Send(uint8 Channel, const TArray<uint8>& Data);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	FString ServerAddr = TEXT("127.0.0.1:25565");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	FTimespan Heartbeat = FTimespan::FromSeconds(1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	int32 KCPLogMask = 0;

private:

	bool bIsActive = false;

	TSharedPtr<FInternetAddr> ServerAddrPtr;

	FSocket* SocketPtr;

	TArray<uint8> SendBuffer;
	TArray<uint8> RecvBuffer;

	FRedNetworkPass ClientPass;

	FDateTime LastRecvTime;
	FDateTime LastHeartbeat;

	TArray<TSharedPtr<FKCPWrap>> KCPUnits;

	FDateTime NowTime;

	void UpdateKCP();
	void SendHeartbeat();
	void HandleSocketRecv();
	void HandleLoginRecv(const FRedNetworkPass& SourcePass);
	void HandleKCPRecv();
	void HandleTimeout();

	void EnsureChannelCreated(uint8 Channel);

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
