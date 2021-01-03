#include "KCPWrap.h"

#include "Logging.h"

namespace FKCPFuncWrap
{
	int Output(const char* buf, int len, ikcpcb* kcp, void* user)
	{
		FKCPWrap* KCPWrap = (FKCPWrap*)user;
		if (KCPWrap->OutputFunc) return KCPWrap->OutputFunc((const uint8*)buf, len);
		return 0;
	}

	void Writelog(const char* log, ikcpcb* kcp, void* user)
	{
		FKCPWrap* KCPWrap = (FKCPWrap*)user;
		UE_LOG(LogKCP, Log, TEXT("%s: %s"), *KCPWrap->GetDebugName(), ANSI_TO_TCHAR(log));
	}
}

FKCPWrap::FKCPWrap(uint32 Conv)
	: KCPPtr(ikcp_create(Conv, this))
	, DebugName(FString::Printf(TEXT("[%i]"), Conv))
{
	KCPPtr->output = &FKCPFuncWrap::Output;
	KCPPtr->writelog = &FKCPFuncWrap::Writelog;
}

FKCPWrap::FKCPWrap(uint32 Conv, const FString & InDebugName)
	: FKCPWrap(Conv)
{
	DebugName = InDebugName;
}

FKCPWrap::~FKCPWrap()
{
	ikcp_release(KCPPtr);
}

ikcpcb & FKCPWrap::GetKCPCB()
{
	return *KCPPtr;
}

int FKCPWrap::Recv(uint8 * Data, int32 Count)
{
	return ikcp_recv(KCPPtr, (char*)Data, Count);
}

int FKCPWrap::Send(const uint8 * Data, int32 Count)
{
	return ikcp_send(KCPPtr, (const char*)Data, Count);
}

void FKCPWrap::Update(uint32 Current)
{
	ikcp_update(KCPPtr, Current);
}

void FKCPWrap::Check(uint32 Current) const
{
	ikcp_check(KCPPtr, Current);
}

int FKCPWrap::Input(const uint8 * Data, int32 Count)
{
	return ikcp_input(KCPPtr, (const char*)Data, Count);
}

void FKCPWrap::Flush()
{
	ikcp_flush(KCPPtr);
}

int FKCPWrap::PeekSize() const
{
	return ikcp_peeksize(KCPPtr);
}

int FKCPWrap::SetMTU(int32 MTU)
{
	return ikcp_setmtu(KCPPtr, MTU);
}

int FKCPWrap::SetWindowSize(int32 SentWindow, int32 RecvWindow)
{
	return ikcp_wndsize(KCPPtr, SentWindow, RecvWindow);
}

int FKCPWrap::GetWaitSent() const
{
	return ikcp_waitsnd(KCPPtr);
}

int FKCPWrap::SetNoDelay(int32 NoDelay, int32 Internal, int32 FastResend, int32 NC)
{
	return ikcp_nodelay(KCPPtr, NoDelay, Internal, FastResend, NC);
}

int FKCPWrap::SetNormalMode()
{
	return SetNoDelay(0, 40, 0, 0);
}

int FKCPWrap::SetTurboMode()
{
	return SetNoDelay(1, 10, 2, 1);
}

void FKCPWrap::SetDebugName(const FString & InDebugName)
{
	DebugName = InDebugName;
}

const FString & FKCPWrap::GetDebugName() const
{
	return DebugName;
}
