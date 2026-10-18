#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CockpitJoystickComponent.generated.h"

class UMotionControllerComponent;

/**
 * Grabbable cockpit stick. Place at the pivot (+Z up the stick, +X forward) with the meshes and a BP_GrabComponent
 * (Grab Type = Custom) under it. Output: X = right, Y = forward, in [-1, 1].
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

	/** Spring back speed when released. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick", meta = (ClampMin = "0.1"))
	float ReturnSpeed = 6.0f;

	/** Round gate instead of square. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	bool bCircularGate = true;

	/** Grab component under this stick. Empty = first child grab component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Joystick")
	FName GrabComponentName;

		UFUNCTION(BlueprintPure, Category = "Joystick")
	FVector2D GetValue() const { return Value; }

	UFUNCTION(BlueprintPure, Category = "Joystick")
	bool IsHeld() const { return bHeld; }

	/** Thumbstick/keyboard value used while not held. */
	UFUNCTION(BlueprintCallable, Category = "Joystick")
	void SetExternalValue(FVector2D InValue);

	/** Hold the stick with a given hand (tests / custom interaction). */
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
