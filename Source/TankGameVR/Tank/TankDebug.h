#pragma once

#include "CoreMinimal.h"

/**
 * Development-only debug switches. Console variables:
 *   Tank.Debug.Movement 1   steering input, speed, turn rate, heading, movement direction
 *   Tank.Debug.Targeting 1  candidates, scores, switch decisions, aim and crosshair data
 * Components also have a local bDebugDraw flag; the console variable (if > 0) forces it on, (if == 0) forces it off.
 */
namespace TankDebug
{
	TANKGAMEVR_API bool IsMovementDebugEnabled(bool bLocalFlag);
	TANKGAMEVR_API bool IsTargetingDebugEnabled(bool bLocalFlag);
}
