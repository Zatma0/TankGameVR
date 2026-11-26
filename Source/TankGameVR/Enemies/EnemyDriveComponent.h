#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyDriveComponent.generated.h"

/**
 * Drives an enemy vehicle like a vehicle: it turns at a limited rate, never strafes, slows down for sharp turns and
 * follows the ground. Uses the nav mesh when the level has one (NavMeshBoundsVolume), otherwise drives straight and
 * steers around what its feelers hit. Backs up when it gets stuck.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UEnemyDriveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyDriveComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MaxSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "1.0"))
	float Acceleration = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "1.0"))
	float Braking = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "1.0", Units = "Degrees"))
	float TurnRate = 40.0f;

	/** Tracks can turn on the spot; wheels need speed to turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive")
	bool bCanPivot = true;

	/** Heading error at which a tracked vehicle stops and pivots. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "5.0", ClampMax = "180.0", Units = "Degrees"))
	float PivotAngle = 70.0f;

	/** Actor origin height above the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Centimeters"))
	float RideHeight = 0.0f;

	/** Highest step it drives up; anything higher blocks it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Centimeters"))
	float MaxStepHeight = 50.0f;

	/** Steeper ground than this is a wall when going up (same as the nav mesh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "5.0", ClampMax = "60.0", Units = "Degrees"))
	float MaxClimbAngle = 35.0f;

	/** Most it leans with the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Degrees"))
	float MaxTilt = 15.0f;

	/** Keep this far from other enemy vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Centimeters"))
	float SeparationRadius = 900.0f;

	/** Feeler length for walls and rocks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Centimeters"))
	float FeelerLength = 700.0f;

	/** Half width used for the feelers (should match the hull). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (Units = "Centimeters"))
	float HalfWidth = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drive", meta = (ClampMin = "0.1", Units = "Seconds"))
	float RepathInterval = 1.0f;

	/** Drives to Goal. SpeedScale 0..1 of MaxSpeed. */
	UFUNCTION(BlueprintCallable, Category = "Drive")
	void MoveTo(FVector Goal, float AcceptRadius = 300.0f, float SpeedScale = 1.0f);

	/** Brake to a stop and stay. */
	UFUNCTION(BlueprintCallable, Category = "Drive")
	void Stop();

	/** While stopped, turn the hull towards this point (0 = don't). */
	UFUNCTION(BlueprintCallable, Category = "Drive")
	void FaceTowards(FVector Point);

	UFUNCTION(BlueprintCallable, Category = "Drive")
	void ClearFacing() { bFacing = false; }

	/** Dead stop, no braking (wreck). */
	UFUNCTION(BlueprintCallable, Category = "Drive")
	void Freeze();

	/** Puts the vehicle somewhere else, on the ground there. */
	UFUNCTION(BlueprintCallable, Category = "Drive")
	void Teleport(FVector Location, FRotator Rotation);

	UFUNCTION(BlueprintPure, Category = "Drive")
	bool HasGoal() const { return bHasGoal; }

	UFUNCTION(BlueprintPure, Category = "Drive")
	bool HasArrived() const { return bArrived; }

	UFUNCTION(BlueprintPure, Category = "Drive")
	FVector GetGoal() const { return Goal; }

	UFUNCTION(BlueprintPure, Category = "Drive")
	float GetSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Drive")
	FVector GetDriveVelocity() const { return Velocity; }

	/** True while it is backing out of a dead end. */
	UFUNCTION(BlueprintPure, Category = "Drive")
	bool IsRecovering() const { return RecoverTime > 0.0f; }

	/** Did the last path come from the nav mesh. */
	UFUNCTION(BlueprintPure, Category = "Drive")
	bool IsUsingNavMesh() const { return bNavPath; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdatePath();
	FVector GetSteerTarget();
	FVector Avoid(const FVector& Desired);
	void Drive(float DeltaTime, float TargetYaw, float TargetSpeed);
	void FollowGround(float DeltaTime);

	TArray<FVector> Path;
	int32 PathIndex = 0;
	FVector Goal = FVector::ZeroVector;
	FVector FacePoint = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector PreMoveLocation = FVector::ZeroVector;
	float GroundZ = 0.0f;
	bool bHasGround = false;
	bool bStepBlocked = false;
	float AcceptRadius = 300.0f;
	float SpeedScale = 1.0f;
	float CurrentSpeed = 0.0f;
	float RepathTimer = 0.0f;
	float StuckTime = 0.0f;
	float RecoverTime = 0.0f;
	float RecoverTurn = 1.0f;
	// Which way round a building we are going (0 = not going round anything).
	int32 DetourSide = 0;
	double DetourUntil = 0.0;
	bool bHasGoal = false;
	bool bArrived = false;
	bool bFacing = false;
	bool bNavPath = false;
};
