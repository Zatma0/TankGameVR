#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Tank/TankMovementMath.h"
#include "TankMovementComponent.generated.h"

// Tracked vehicle movement from one stick (X = steer, Y = throttle). Sweeps the root along the hull's forward.
UCLASS(ClassGroup = (Tank), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive", meta = (ShowOnlyInnerProperties))
	FTankDriveTuning Tuning;

	/** Local axis of the hull that counts as "forward". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Drive")
	FVector LocalForwardAxis = FVector::ForwardVector;

	/** Keep a constant height above the ground (no pitch/roll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground")
	bool bFollowGround = true;

	/** Root height above the ground. <= 0 uses the placed height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (Units = "Centimeters"))
	float RideHeight = 0.0f;

	/** Ground steps bigger than this are ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float MaxGroundStep = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Debug")
	bool bDebugDraw = false;

	/** X = steering (right +), Y = throttle (forward +). */
	UFUNCTION(BlueprintCallable, Category = "Tank|Drive")
	void SetDriveInput(FVector2D RawStick);

		UFUNCTION(BlueprintCallable, Category = "Tank|Drive")
	void StopImmediately();

	UFUNCTION(BlueprintPure, Category = "Tank|Drive")
	FVector GetForwardVector() const;

	UFUNCTION(BlueprintPure, Category = "Tank|Drive")
	FVector GetTankVelocity() const { return GetForwardVector() * CurrentSpeed; }

	/** cm/s along the hull, negative when reversing. */
	UFUNCTION(BlueprintPure, Category = "Tank|Drive")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	/** deg/s, positive = turning right. */
	UFUNCTION(BlueprintPure, Category = "Tank|Drive")
	float GetCurrentTurnRate() const { return CurrentTurnRate; }

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	FVector2D RawInput = FVector2D::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	FVector2D ProcessedInput = FVector2D::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	float CurrentSpeed = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	float TargetSpeed = 0.0f;

	/** deg/s, positive = turning right. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	float CurrentTurnRate = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Tank|State")
	bool bBlockedLastFrame = false;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void FollowGround();
	void DrawDebug() const;

	FVector2D SmoothedInput = FVector2D::ZeroVector;
	FVector2D LimitedInput = FVector2D::ZeroVector;
};
