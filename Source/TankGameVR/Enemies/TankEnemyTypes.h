#pragma once

#include "CoreMinimal.h"
#include "Enemies/TankEnemy.h"
#include "TankEnemyTypes.generated.h"

/**
 * Main battle tank. Closes in to its range from a flank, stops for the shot (short halt), fires a couple of slow
 * heavy rounds and then moves to a new spot. Thick armour: AP works, HE mostly doesn't.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankEnemy_Tank : public ATankEnemy
{
	GENERATED_BODY()

public:
	ATankEnemy_Tank();

	/** Shots fired from one halt before moving on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (ClampMin = "1"))
	int32 ShotsPerHalt = 2;

	/** Moves on after this long in one spot anyway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (Units = "Seconds"))
	float MaxHaltTime = 12.0f;

protected:
	virtual void ApplyStats() override;
	virtual void UpdateBehaviour(float DeltaSeconds) override;
	virtual void HandleHit(float Damage) override;

private:
	void PickNewPosition(float MinAngle, float MaxAngle);

	FVector Position = FVector::ZeroVector;
	float FlankSide = 1.0f;
	float LostSightTime = 0.0f;
	int32 ShotsAtHalt = 0;
	int32 HaltStartShots = 0;
};

/**
 * Fast wheeled scout. Never stops: runs from one flank to the other around the player, firing autocannon bursts on
 * the move. Thin armour: one HE round wrecks it.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankEnemy_ArmoredCar : public ATankEnemy
{
	GENERATED_BODY()

public:
	ATankEnemy_ArmoredCar();

	/** Circles this many times its range from the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour")
	FVector2D OrbitDistance = FVector2D(1.5, 2.0);

	/** Share of top speed while circling (full speed would outrun any turret). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float OrbitSpeed = 0.55f;

	/** Every other run ends with a short stop to fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (Units = "Seconds"))
	float StopTime = 2.5f;

protected:
	virtual void ApplyStats() override;
	virtual void UpdateBehaviour(float DeltaSeconds) override;
	virtual void HandleHit(float Damage) override;

private:
	void PickRun();

	float Direction = 1.0f;
	int32 Runs = 0;
};

/**
 * Anti-tank gun with a small crew. Moves to a firing spot, digs in (deploys), lays the gun on the player with a
 * visible laser for a couple of seconds and fires one very hard shot. Moves after two shots or when hit.
 * Smoke or cover breaks the lay, and it has to start again.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankEnemy_AntiTank : public ATankEnemy
{
	GENERATED_BODY()

public:
	ATankEnemy_AntiTank();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (Units = "Seconds"))
	float DeployTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Behaviour", meta = (ClampMin = "1"))
	int32 ShotsPerPosition = 2;

protected:
	virtual void ApplyStats() override;
	virtual void UpdateBehaviour(float DeltaSeconds) override;
	virtual void HandleHit(float Damage) override;

private:
	void Relocate();

	int32 PositionStartShots = 0;
	bool bHitWhileDeployed = false;
};
