#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankVisionConeComponent.generated.h"

/**
 * What the player can see right now: a cone from the player's eyes (the headset, or the camera on a monitor) along
 * the view direction. Used to bring enemies in where the player is not looking. Blueprint spawners can ask it too.
 * Only the horizontal direction counts, so looking down at the controls does not blind the player.
 */
UCLASS(ClassGroup = (Tank), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankVisionConeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankVisionConeComponent();

	/** Half of the field of view. A little wider than a headset, so nothing pops in at the edge of the picture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "5.0", ClampMax = "180.0", Units = "Degrees"))
	float HalfAngle = 65.0f;

	/** Further away than this nothing counts as seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision", meta = (ClampMin = "100.0", Units = "Centimeters"))
	float Range = 20000.0f;

	/** Behind a building, a hill or smoke is not seen either, even inside the cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision")
	bool bCheckLineOfSight = true;

	/** Draws the cone on the ground (or Tank.Debug.VisionCone 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vision|Debug")
	bool bDebugDraw = false;

	/** Where the player looks from and in which (level) direction. */
	UFUNCTION(BlueprintPure, Category = "Vision")
	void GetView(FVector& OutLocation, FVector& OutDirection) const;

	/** Inside the cone: within the angle and the range. Walls are not checked. */
	UFUNCTION(BlueprintPure, Category = "Vision")
	bool IsInCone(FVector Point) const;

	/** Inside the cone and nothing in the way. */
	UFUNCTION(BlueprintPure, Category = "Vision")
	bool CanSee(FVector Point) const;

	/** Same for an actor (aims at its middle and looks past the actor itself). */
	UFUNCTION(BlueprintPure, Category = "Vision")
	bool CanSeeActor(const AActor* Actor) const;

	/** Fixes the view instead of following the camera (tests, cut scenes). */
	UFUNCTION(BlueprintCallable, Category = "Vision")
	void SetViewOverride(bool bEnable, FVector Location = FVector::ZeroVector, FVector Direction = FVector::ForwardVector);

	/** The player tank's vision cone, from anywhere. */
	UFUNCTION(BlueprintPure, Category = "Vision", meta = (WorldContext = "WorldContext"))
	static UTankVisionConeComponent* GetPlayerVisionCone(const UObject* WorldContext);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool IsLineClear(const FVector& Eye, const FVector& Point, const AActor* Ignore) const;

	FVector OverrideLocation = FVector::ZeroVector;
	FVector OverrideDirection = FVector::ForwardVector;
	bool bViewOverride = false;
};
