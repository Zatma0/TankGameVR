#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "TankSeatComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTankBoarded, APawn*, Pawn);

// Seat on the cockpit floor (rider's tracking origin, facing forward). Attaches the player pawn so it rides along.
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankSeatComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTankSeatComponent();

	/** Board the local player's pawn automatically at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	bool bAutoBoardLocalPlayer = true;

	/** Eye height the cockpit is built around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "60.0", ClampMax = "200.0", Units = "Centimeters"))
	float DesignEyeHeight = 120.0f;

	/** Move the rider up/down so the eyes end up at DesignEyeHeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat")
	bool bCalibrateEyeHeight = true;

	/** Wait for the headset pose before calibrating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "0.0", Units = "Seconds"))
	float CalibrationDelay = 1.0f;

	/** Max calibration offset (no headset needs the full eye height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seat", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float MaxCalibrationOffset = 150.0f;

	UPROPERTY(BlueprintAssignable, Category = "Seat")
	FOnTankBoarded OnBoarded;

	UFUNCTION(BlueprintCallable, Category = "Seat")
	bool Board(APawn* Pawn);

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
