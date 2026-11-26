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

	/** Drive on whatever is below: follows hills and stops at slopes that are too steep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground")
	bool bFollowGround = true;

	/** Lean with the ground. Off keeps the hull level (it still climbs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground")
	bool bTiltWithGround = true;

	/** Steeper ground than this is a wall when going up. Going down is always allowed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (ClampMin = "5.0", ClampMax = "60.0", Units = "Degrees"))
	float MaxClimbAngle = 32.0f;

	/** How quickly the hull leans into a new slope. 0 = instantly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (ClampMin = "0.0"))
	float TiltSpeed = 5.0f;

	/** Share of the top speed left on the steepest slope we can climb. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SteepClimbSpeedScale = 0.55f;

	/** Where the tracks touch the ground, from the hull centre (forward, sideways). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (Units = "Centimeters"))
	FVector2D TrackContact = FVector2D(250.0, 130.0);

	/** Root height above the ground. <= 0 uses the placed height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Ground", meta = (Units = "Centimeters"))
	float RideHeight = 0.0f;

	/** A step or drop bigger than this in one go stops the tank. */
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

	/** Slope of the ground under the tank in degrees. */
	UFUNCTION(BlueprintPure, Category = "Tank|Ground")
	float GetGroundSlope() const { return GroundSlope; }

	/** Puts the tank on the ground below it, e.g. after it was teleported. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Ground")
	void SnapToGround();

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
	struct FGroundPose
	{
		bool bValid = false;
		float Z = 0.0f;
		FVector Normal = FVector::UpVector;
	};

	FGroundPose SampleGround(const FVector& Center, float HeadingYaw) const;
	bool TraceGround(const FVector& Point, float Up, float Down, float& OutZ) const;
	FQuat MakeHullRotation(float HeadingYaw, const FVector& Normal) const;
	float GetHeadingYaw() const;
	bool SweepTo(const FVector& TargetLocation, const FQuat& TargetRotation);
	void UpdateGroundIgnores();
	void DrawDebug() const;
	void ReleaseGhostedActors(UPrimitiveComponent* Hull);

	TArray<AActor*> GroundIgnores;
	FVector GroundNormal = FVector::UpVector;
	FVector SmoothedNormal = FVector::UpVector;
	float GroundSlope = 0.0f;

	/** Things we were stuck inside; ignored until the hull is clear of them. */
	TArray<TWeakObjectPtr<AActor>> GhostedActors;

	FVector2D SmoothedInput = FVector2D::ZeroVector;
	FVector2D LimitedInput = FVector2D::ZeroVector;
};
