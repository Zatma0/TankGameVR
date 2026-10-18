#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankTestWaveSpawner.generated.h"

/**
 * Minimal wave source for testing the targeting system (not a game wave director).
 * Spawns EnemiesPerWave enemies in an arc in front of the spawner; when all are gone the next wave follows.
 * Enemies can expire on their own (simulated kills). Console: Tank.DestroyTarget, Tank.NextWave.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankTestWaveSpawner : public AActor
{
	GENERATED_BODY()

public:
	ATankTestWaveSpawner();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "1", ClampMax = "32"))
	int32 EnemiesPerWave = 4;

	/** 0 = endless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0"))
	int32 NumberOfWaves = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (Units = "Seconds"))
	float FirstWaveDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (Units = "Seconds"))
	float TimeBetweenWaves = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (Units = "Centimeters"))
	FVector2D SpawnDistance = FVector2D(2500.0, 4500.0);

	/** Width of the arc (deg) enemies spawn in, centred on the spawner's forward direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float SpawnArc = 90.0f;

	/** Each enemy removes itself after a random time in this range (0,0 = never). Simulates kills. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves", meta = (Units = "Seconds"))
	FVector2D EnemyLifetime = FVector2D(15.0, 30.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Waves")
	bool bAutoStart = true;

	UFUNCTION(BlueprintCallable, Category = "Waves")
	void SpawnWave();

	/** Destroys the remaining enemies and starts the next wave right away. */
	UFUNCTION(BlueprintCallable, Category = "Waves")
	void SkipToNextWave();

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetAliveCount() const;

	UFUNCTION(BlueprintPure, Category = "Waves")
	int32 GetCurrentWave() const { return CurrentWave; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	TArray<TWeakObjectPtr<AActor>> Alive;
	int32 CurrentWave = 0;
	float NextWaveTimer = 0.0f;
	bool bWaitingForWave = false;
};
