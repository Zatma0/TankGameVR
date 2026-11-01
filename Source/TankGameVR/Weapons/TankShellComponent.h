#pragma once

#include "CoreMinimal.h"
#include "Cockpit/CockpitGrabbableComponent.h"
#include "TankShellComponent.generated.h"

/**
 * One round in the ready rack. Pivot = middle of the round, +X = nose. After it is fired it disappears and a new
 * round shows up in the same rack slot after RefillDelay.
 */
UCLASS(ClassGroup = (Weapons), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankShellComponent : public UCockpitGrabbableComponent
{
	GENERATED_BODY()

public:
	UTankShellComponent();

	/** Pivot to nose tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float NoseOffset = 25.0f;

	/** Time until the rack slot is full again. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell", meta = (ClampMin = "0.0", Units = "Seconds"))
	float RefillDelay = 4.0f;

	UFUNCTION(BlueprintPure, Category = "Shell")
	FVector GetNoseLocation() const;

	/** Ready in the rack (or being carried), not fired. */
	UFUNCTION(BlueprintPure, Category = "Shell")
	bool IsAvailable() const { return GetState() != ECockpitGrabState::Away; }

	/** Fired: hide and start the refill timer. */
	UFUNCTION(BlueprintCallable, Category = "Shell")
	void Consume();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float RefillRemaining = -1.0f;
};
