#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RSHWNetworkType.h"
#include "UObject/Interface.h"
#include "RSHWNetworkServer.generated.h"

class FSocket;

UINTERFACE()
class RSHWNETWORK_API URSHWNetworkServerHandler : public UInterface
{
	GENERATED_BODY()
};

class RSHWNETWORK_API IRSHWNetworkServerHandler
{
	GENERATED_BODY()

public:

	virtual void OnLogin(int32 ClientID) { }

	virtual void OnRecv(int32 ClientID, const TArray<uint8>& Data) { }

	virtual void OnUnlogin(int32 ClientID) { }

};

UCLASS()
class RSHWNETWORK_API URSHWNetworkServer : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:

	bool IsRunning() const { return bIsRunning; }

	bool SetHandler(TScriptInterface<IRSHWNetworkServerHandler> InHandlerObject);

	TScriptInterface<IRSHWNetworkServerHandler> GetHandler() const { return HandlerObject; }

	bool SetBindPort(int32 InPort = 25565);

	int32 GetBindPort() const { return Port; }

	bool Send(int32 ClientID, const TArray<uint8>& Data);

	bool RunServer();

	void StopServer();

public:

	FTimespan TimeoutLimit = FTimespan::FromSeconds(8.0);

private:

	bool bIsRunning = false;

	int32 Port = 25565;

	UPROPERTY()
	TScriptInterface<IRSHWNetworkServerHandler> HandlerObject;

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
