#include "Tank/TankDebug.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarTankDebugMovement(
	TEXT("Tank.Debug.Movement"), -1,
	TEXT("Tank movement debug. -1 = use component flag, 0 = force off, 1 = force on."));

static TAutoConsoleVariable<int32> CVarTankDebugTargeting(
	TEXT("Tank.Debug.Targeting"), -1,
	TEXT("Tank targeting debug. -1 = use component flag, 0 = force off, 1 = force on."));

namespace
{
	bool Resolve(int32 CVarValue, bool bLocalFlag)
	{
		return CVarValue < 0 ? bLocalFlag : CVarValue > 0;
	}
}

bool TankDebug::IsMovementDebugEnabled(bool bLocalFlag)
{
	return Resolve(CVarTankDebugMovement.GetValueOnGameThread(), bLocalFlag);
}

bool TankDebug::IsTargetingDebugEnabled(bool bLocalFlag)
{
	return Resolve(CVarTankDebugTargeting.GetValueOnGameThread(), bLocalFlag);
}
