#pragma once

#include "CoreMinimal.h"
#include "RSHWNetworkType.h"
#include "Components/ActorComponent.h"
#include "RSHWNetworkServer.generated.h"

class FSocket;

UCLASS(BlueprintType, hidecategories = ("Cooking", "ComponentReplication"), meta = (BlueprintSpawnableComponent))
class RSHWNETWORK_API URSHWNetworkServer : public UActorComponent
{
	GENERATED_BODY()

public:

	URSHWNetworkServer(const FObjectInitializer& ObjectInitializer);
	
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
	bool Send(int32 ClientID, const TArray<uint8>& Data);

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	int32 Port = 25565;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan Heartbeat = FTimespan::FromSeconds(1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RSHW|Network")
	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

private:

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
	};

	TMap<int32, FRegistrationInfo> Registration;

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
