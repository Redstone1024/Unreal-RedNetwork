#include "RedNetworkType.h"
#include "..\Public\RedNetworkType.h"

FRedNetworkPass::FRedNetworkPass()
	: ID(0)
	, Key(0)
{
}

FRedNetworkPass::FRedNetworkPass(uint8 * Data)
	: FRedNetworkPass()
{
	FromBytes(Data);
}

void FRedNetworkPass::FromBytes(const uint8 * Data)
{
	ID = 0;
	Key = 0;

	ID |= (int32)Data[0] << 0;
	ID |= (int32)Data[1] << 8;
	ID |= (int32)Data[2] << 16;
	ID |= (int32)Data[3] << 24;

	Key |= (int32)Data[4] << 0;
	Key |= (int32)Data[5] << 8;
	Key |= (int32)Data[6] << 16;
	Key |= (int32)Data[7] << 24;
}

void FRedNetworkPass::ToBytes(uint8 * Data) const
{
	Data[0] = ID >> 0;
	Data[1] = ID >> 8;
	Data[2] = ID >> 16;
	Data[3] = ID >> 24;

	Data[4] = Key >> 0;
	Data[5] = Key >> 8;
	Data[6] = Key >> 16;
	Data[7] = Key >> 24;
}

void FRedNetworkPass::RandKey()
{
	Key ^= FMath::Rand() << 0;
	Key ^= FMath::Rand() << 8;
	Key ^= FMath::Rand() << 16;
	Key ^= FMath::Rand() << 24;
}

void FRedNetworkPass::Reset()
{
	ID = 0;
	Key = 0;
}

bool FRedNetworkPass::IsValid() const
{
	return ID;
}
