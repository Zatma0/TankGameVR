#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CockpitJoystickComponent.generated.h"

class UMotionControllerComponent;

/**
 * A physical cockpit stick. Place it at the stick's pivot (base), with local +Z pointing up the stick,
 * +X forward and +Y to the right. Attach the stick's visible meshes and a VR template BP_GrabComponent
 * (Grab Type = Custom) below it: the component reads the grabbing hand and tilts itself (and therefore the
 * attached meshes) to match. Output: X = right deflection, Y = forward deflection, both in [-1, 1].
 * When nobody holds it, an external value (thumbstick / keyboard) can drive it, otherwise it springs back.
 * Visual meshes are free to change: the logic never looks at them.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitJoystickComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCockpitJoystickComponent();

	/** Tilt (deg) that corresponds to full deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float MaxTiltAngle = 25.0f;

	/** How fast the stick springs back to the centre when released (units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "0.1"))
	float ReturnSpeed = 6.0f;

	/** Round gate: the combined deflection never exceeds 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	bool bCircularGate = true;

	/** Optional: name of the grab component under this stick. Empty = first child whose class name contains "GrabComponent". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	FName GrabComponentName;

	/** Current output (X = right, Y = forward). */
	UFUNCTION(BlueprintPure, Category = "Joystick")
	FVector2D GetValue() const { return Value; }

	UFUNCTION(BlueprintPure, Category = "Joystick")
	bool IsHeld() const { return bHeld; }

	/** Thumbstick/keyboard value used while the stick is not held (also moves the visual stick). */
	UFUNCTION(BlueprintCallable, Category = "Joystick")
	void SetExternalValue(FVector2D InValue);

	/** Directly set the hand-driven state (used by tests and custom interaction code). */
	UFUNCTION(BlueprintCallable, Category = "Joystick")
	void SetHeldByHand(USceneComponent* Hand);

	UFUNCTION(BlueprintCallable, Category = "Joystick")
	void ReleaseHand();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	USceneComponent* FindGrabComponent() const;
	void PollGrabComponent();
	void ApplyKnobCollision();
	FTransform GetBaseTransform() const;
	FVector2D HandAngles(const FVector& HandWorld) const;
	void ApplyVisualTilt();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrabComponent;

	TWeakObjectPtr<USceneComponent> ExplicitHand;
	TWeakObjectPtr<USceneComponent> Hand;
	FTransform RestRelativeTransform;
	FVector2D Value = FVector2D::ZeroVector;
	FVector2D ExternalValue = FVector2D::ZeroVector;
	FVector2D ValueAtGrab = FVector2D::ZeroVector;
	FVector2D AnglesAtGrab = FVector2D::ZeroVector;
	bool bHeld = false;
	bool bCollisionApplied = false;
};
