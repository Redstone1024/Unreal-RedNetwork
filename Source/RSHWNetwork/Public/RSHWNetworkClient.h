#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "RSHWNetworkType.h"
#include "Components/ActorComponent.h"
#include "RSHWNetworkClient.generated.h"

class FKCPWrap;
class FInternetAddr;

UCLASS(BlueprintType, hidecategories = ("Cooking", "ComponentReplication"), meta = (BlueprintSpawnableComponent))
class RSHWNETWORK_API URSHWNetworkClient : public UActorComponent
{
	GENERATED_BODY()

public:

	URSHWNetworkClient(const FObjectInitializer& ObjectInitializer);

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

private:

	//~ Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void Activate(bool bReset) override;
	virtual void Deactivate() override;
	//~ End UActorComponent Interface

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

};
