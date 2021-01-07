#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/Object.h"
#include "RedNetworkType.h"
#include "RedNetworkServer.generated.h"

class FSocket;
class FKCPWrap;
class FInternetAddr;

UCLASS(BlueprintType)
class REDNETWORK_API URedNetworkServer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FLoginSignature, URedNetworkServer, OnLogin, int32, ClientID);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FRecvSignature, URedNetworkServer, OnRecv, int32, ClientID, uint8, Channel, const TArray<uint8>&, Data);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FUnloginSignature, URedNetworkServer, OnUnlogin, int32, ClientID);

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
	bool Send(int32 ClientID, uint8 Channel, const TArray<uint8>& Data);

	TSharedPtr<FInternetAddr> GetSocketAddr() const;

	UFUNCTION(BlueprintCallable, Category = "Red|Network")
	FString GetSocketAddrString() const;

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	int32 Port = 25565;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	FTimespan Heartbeat = FTimespan::FromSeconds(1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Red|Network")
	int32 KCPLogMask = 0;

private:

	bool bIsActive = false;

	FSocket* SocketPtr;

	TArray<uint8> SendBuffer;
	TArray<uint8> RecvBuffer;

	int32 NextReadyID;
		
	struct FReadyInfo
	{
		FDateTime Time;
		FRedNetworkPass Pass;
	};

	TMap<FString, FReadyInfo> ReadyPass;

	struct FConnectionInfo
	{
		FRedNetworkPass Pass;
		FDateTime RecvTime;
		FDateTime Heartbeat;
		TSharedPtr<FInternetAddr> Addr;
		TArray<TSharedPtr<FKCPWrap>> KCPUnits;
	};

	TMap<int32, FConnectionInfo> Connections;

	FDateTime NowTime;

	void UpdateKCP();
	void SendHeartbeat();
	void HandleSocketRecv();
	void SendReadyPass(const TSharedRef<FInternetAddr>& SourceAddr);
	void RedirectConnection(const FRedNetworkPass& SourcePass, const TSharedRef<FInternetAddr>& SourceAddr);
	void RegisterConnection(const FRedNetworkPass& SourcePass, const TSharedRef<FInternetAddr>& SourceAddr);
	void HandleKCPRecv();
	void HandleExpiredReadyPass();
	void HandleExpiredConnection();

	void EnsureChannelCreated(int32 ClientID, uint8 Channel);

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
