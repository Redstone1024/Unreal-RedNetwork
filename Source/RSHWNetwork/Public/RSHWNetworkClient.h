#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "UObject/Object.h"
#include "RSHWNetworkType.h"
#include "UObject/Interface.h"
#include "RSHWNetworkClient.generated.h"

class FInternetAddr;

UINTERFACE()
class RSHWNETWORK_API URSHWNetworkClientHandler : public UInterface
{
	GENERATED_BODY()
};

class RSHWNETWORK_API IRSHWNetworkClientHandler
{
	GENERATED_BODY()

public:

	virtual void OnLogin() { }

	virtual void OnLoginFailure() { }

	virtual void OnRecv(const TArray<uint8>& Data) { }

	virtual void OnUnlogin() { }

};

UCLASS()
class RSHWNETWORK_API URSHWNetworkClient : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	URSHWNetworkClient(const FObjectInitializer& ObjectInitializer);

	bool IsRunning() const { return bIsRunning; }

	bool SetHandler(TScriptInterface<IRSHWNetworkClientHandler> InHandlerObject);

	TScriptInterface<IRSHWNetworkClientHandler> GetHandler() const { return HandlerObject; }

	bool SetServerAddr(TSharedRef<FInternetAddr> InServerAddr);

	TSharedRef<FInternetAddr> GetServerAddr() const { return ServerAddr.ToSharedRef(); }

	bool SetServerAddrByString(const FString& InServerAddr = TEXT("127.0.0.1:25565"));

	FString GetServerAddrByString() const { return ServerAddr->ToString(true); }

	bool Send(const TArray<uint8>& Data);

	void Login();

	void Unlogin();

public:

	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

private:

	bool bIsRunning = false;

	TSharedPtr<FInternetAddr> ServerAddr;

	UPROPERTY()
	TScriptInterface<IRSHWNetworkClientHandler> HandlerObject;

private:

	FSocket* SocketPtr;

	TArray<uint8> SendBuffer;
	TArray<uint8> RecvBuffer;
	TArray<uint8> DataBuffer;

	FDateTime StartupTime;

	FRSHWNetworkPass ClientPass;

	FDateTime LastRecvTime;
	FDateTime LastHeartbeat;

	void ResetRunningData();

private:

	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

	//~ Begin FTickableGameObject Interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate() && bIsRunning; }
	virtual TStatId GetStatId() const override { return GetStatID(); }
	//~ End FTickableGameObject Interface

};
