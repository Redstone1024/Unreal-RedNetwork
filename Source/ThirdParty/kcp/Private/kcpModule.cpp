#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class KCP_API FkcpModule : public IModuleInterface
{
public:

	// ~Begin IModuleInterface Interface 
	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
	// ~End IModuleInterface Interface 

};

IMPLEMENT_MODULE(FkcpModule, kcp)
