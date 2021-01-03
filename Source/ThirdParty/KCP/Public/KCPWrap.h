#pragma once

#include "CoreMinimal.h"
#include "ikcp.h"

class KCP_API FKCPWrap
{
public:

	FKCPWrap(uint32 Conv);

	~FKCPWrap();

	int Recv(uint8* Data, int32 Count);

	int Send(const uint8* Data, int32 Count);

	void Update(uint32 Current);

	void Check(uint32 Current) const;

	int Input(const uint8* Data, int32 Count);

	void Flush();

	int PeekSize() const;

	int SetMTU(int32 MTU = 1400);

	int SetWindowSize(int32 SentWindow = -1, int32 RecvWindow = -1);

	int GetWaitSent() const;

	int SetNoDelay(int32 NoDelay = -1, int32 Internal = -1, int32 FastResend = -1, int32 NC = -1);

	int SetNormalMode();

	int SetTurboMode();

	TFunction<int(const uint8* Data, int32 Count)> OutputFunc;

private:

	ikcpcb* KCPPtr;

};
