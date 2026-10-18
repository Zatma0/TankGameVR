#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TankSeatComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTankBoarded, APawn*, Pawn);

/**
 * The cockpit seat. Place it on the cockpit floor where the player's tracking origin should be, facing forward.
 * Boards the existing VR pawn by attaching it here, so the pawn's head tracking stays untouched while the
 * tank carries it. Optionally calibrates height so seated and standing players get the same eye point.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankSeatComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTankSeatComponent();

	/** Board the local player's pawn automatically at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	bool bAutoBoardLocalPlayer = true;

	/** Intended eye height above this component, in cm (the cockpit is laid out around this point). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "60.0", ClampMax = "200.0", Units = "Centimeters"))
	float DesignEyeHeight = 120.0f;

	/** Shift the rider vertically so their real head height lands on DesignEyeHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	bool bCalibrateEyeHeight = true;

	/** Delay before calibrating (lets the headset report a valid pose). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "0.0", Units = "Seconds"))
	float CalibrationDelay = 1.0f;

	/** Largest correction applied by calibration (desktop play without a headset needs the full eye height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float MaxCalibrationOffset = 150.0f;

	UPROPERTY(BlueprintAssignable, Category = "Seat")
	FOnTankBoarded OnBoarded;

	UFUNCTION(BlueprintCallable, Category = "Seat")
	bool Board(APawn* Pawn);

	/** Re-measures the headset and shifts the rider so the eyes are at DesignEyeHeight. */
	UFUNCTION(BlueprintCallable, Category = "Seat")
	void CalibrateEyeHeight();

	UFUNCTION(BlueprintPure, Category = "Seat")
	APawn* GetRider() const { return Rider.Get(); }

protected:
	virtual void BeginPlay() override;

private:
	void BoardLocalPlayer();

	TWeakObjectPtr<APawn> Rider;
	FTimerHandle CalibrationTimer;
};
