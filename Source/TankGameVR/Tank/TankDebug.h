#pragma once

#include "CoreMinimal.h"

// Tank.Debug.Movement / Tank.Debug.Targeting: 1 forces debug draw on, 0 off, -1 uses the component's flag.
namespace TankDebug
{
	TANKGAMEVR_API bool IsMovementDebugEnabled(bool bLocalFlag);
	TANKGAMEVR_API bool IsTargetingDebugEnabled(bool bLocalFlag);
}
