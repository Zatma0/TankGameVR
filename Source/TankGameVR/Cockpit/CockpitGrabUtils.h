#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UHapticFeedbackEffect_Base;
class USceneComponent;
class UWorld;

/** Helpers for cockpit parts grabbed with the VR template's BP_GrabComponent (Grab Type = Custom). */
namespace CockpitGrab
{
	/** Named child, or the first child grab component when Name is None. */
	USceneComponent* FindGrabComponent(const USceneComponent* Root, FName Name);

	/** The motion controller holding this grab component, or null. Read via reflection, no Blueprint changes needed. */
	USceneComponent* GetHoldingHand(const USceneComponent* GrabComponent);

	/** BP_GrabComponent sets its parent to PhysicsActor in BeginPlay. Keep it PhysicsBody for the grab trace, but no collision. */
	void SetGrabSurface(USceneComponent* GrabComponent, bool bGrabbable);

	EControllerHand GetHandSide(const USceneComponent* Hand);

	void PlayHaptic(const UWorld* World, UHapticFeedbackEffect_Base* Effect, EControllerHand Hand, float Scale = 1.0f);
}
