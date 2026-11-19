#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/TankEnemyStats.h"
#include "TankArenaDirector.generated.h"

class ATankEnemy;
class ATankEnemySpawnPoint;
class ATankVehicle;
class UTankHealthComponent;

USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTankWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0"))
	int32 Tanks = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0"))
	int32 ArmoredCars = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0"))
	int32 AntiTankGuns = 0;
};

UENUM(BlueprintType)
enum class ETankArenaPhase : uint8
{
	Idle,
	Break,
	Fighting,
	GameOver,
	Victory
};

/**
 * Runs a match in the arena: waves of enemies from the spawn points, score per kill, game over when the player tank
 * is destroyed and a restart a few seconds later (the rider stays seated). Put one in the level.
 * Ugurkan's BP spawner can also call SpawnEnemy directly.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankArenaDirector : public AActor
{
	GENERATED_BODY()

public:
	ATankArenaDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Enemies")
	TSubclassOf<ATankEnemy> TankClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Enemies")
	TSubclassOf<ATankEnemy> ArmoredCarClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Enemies")
	TSubclassOf<ATankEnemy> AntiTankClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	TArray<FTankWave> Waves;

	/** After the last wave keep going with bigger waves instead of winning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	bool bEndless = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	bool bAutoStart = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float FirstWaveDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float TimeBetweenWaves = 8.0f;

	/** Enemies of a wave arrive one after another. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float SpawnInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float RestartDelay = 6.0f;

	/** The crew patches the tank up after each cleared wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (ClampMin = "0.0"))
	float RepairBetweenWaves = 35.0f;

	/** Never spawn closer than this to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (Units = "Centimeters"))
	float MinDistanceFromPlayer = 2500.0f;

	/** Without spawn points: ring around the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (Units = "Centimeters"))
	FVector2D FallbackSpawnDistance = FVector2D(3500.0, 5000.0);

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void StartGame();

	UFUNCTION(BlueprintCallable, Category = "Arena")
	void RestartGame();

	/** Clears the current wave and starts the next (testing). */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SkipWave();

	/** Spawns one enemy of a type at a transform and tracks it (score, wave). */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	ATankEnemy* SpawnEnemy(ETankEnemyType Type, FTransform Transform);

	UFUNCTION(BlueprintPure, Category = "Arena")
	ETankArenaPhase GetPhase() const { return Phase; }

	/** 1-based, 0 before the first. */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetWave() const { return Wave; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetScore() const { return Score; }

	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetKills() const { return Kills; }

	/** Enemies still fighting, including those not spawned yet in this wave. */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 GetEnemiesLeft() const;

	/** Short line for the cockpit display (empty when nothing to say). */
	UFUNCTION(BlueprintPure, Category = "Arena")
	FString GetMessage() const;

	/** One line: wave, enemies, score. */
	UFUNCTION(BlueprintPure, Category = "Arena")
	FString GetStatusLine() const;

	UFUNCTION(BlueprintPure, Category = "Arena")
	TArray<ATankEnemy*> GetEnemies() const;

	UFUNCTION(BlueprintPure, Category = "Arena", meta = (WorldContext = "WorldContext"))
	static ATankArenaDirector* GetDirector(const UObject* WorldContext);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleEnemyDied(UTankHealthComponent* HealthComponent, AActor* Killer);

	UFUNCTION()
	void HandlePlayerDied(UTankHealthComponent* HealthComponent, AActor* Killer);

private:
	FTankWave GetWaveSetup(int32 WaveNumber) const;
	void BeginWave(int32 WaveNumber);
	void SpawnNext();
	bool FindSpawnTransform(ETankEnemyType Type, FTransform& Out);
	bool IsSpotFree(const FVector& Location, float Radius) const;
	void BindPlayer();
	void ClearEnemies();
	void SetPhase(ETankArenaPhase NewPhase, float Duration = 0.0f);

	TWeakObjectPtr<ATankVehicle> Player;
	FTransform PlayerStart;
	TArray<TWeakObjectPtr<ATankEnemy>> Enemies;
	TArray<ETankEnemyType> Queue;
	TMap<TWeakObjectPtr<ATankEnemySpawnPoint>, double> SpawnPointUsed;
	ETankArenaPhase Phase = ETankArenaPhase::Idle;
	float PhaseTime = 0.0f;
	float PhaseDuration = 0.0f;
	float SpawnTimer = 0.0f;
	float WaveTime = 0.0f;
	int32 Wave = 0;
	int32 Score = 0;
	int32 Kills = 0;
	float LastRepair = 0.0f;
};
