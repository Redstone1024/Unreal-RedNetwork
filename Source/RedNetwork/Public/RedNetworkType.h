#pragma once

#include "CoreMinimal.h"

struct REDNETWORK_API FRedNetworkPass
{
	int32 ID;
	int32 Key;

	FRedNetworkPass();
	FRedNetworkPass(uint8* Data);

	void FromBytes(const uint8* Data);
	void ToBytes(uint8* Data) const;

	void RandKey();
	void Reset();

	bool IsValid() const;
};
