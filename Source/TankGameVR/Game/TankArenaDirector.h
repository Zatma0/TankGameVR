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

	/** No NavMeshBoundsVolume in the level: build navigation in a box around the player (UTankLevelSetupSubsystem). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Level")
	bool bCreateNavigationIfMissing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Level", meta = (Units = "Centimeters"))
	FVector AutoNavigationSize = FVector(40000.0, 40000.0, 12000.0);

	/** Big meshes without simple collision get a blocking box, so nothing drives or shoots through them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Level")
	bool bBlockMeshesWithoutCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float FirstWaveDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float TimeBetweenWaves = 8.0f;

	/** Time between the enemies of one group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float SpawnInterval = 1.5f;

	/** A wave arrives in small groups: this many together (picked between the two numbers), then a pause. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves")
	FIntPoint SpawnGroupSize = FIntPoint(1, 2);

	/** Pause between two groups. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float TimeBetweenGroups = 6.0f;

	/** Enemies come in where the player is not looking (outside the tank's vision cone or behind cover). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning")
	bool bSpawnOutOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (Units = "Seconds"))
	float RestartDelay = 6.0f;

	/** The crew patches the tank up after each cleared wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Waves", meta = (ClampMin = "0.0"))
	float RepairBetweenWaves = 35.0f;

	/** Never spawn closer than this to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (Units = "Centimeters"))
	float MinDistanceFromPlayer = 2500.0f;

	/** A tank or car stuck out of sight for this long (or just out of sight three times as long) comes back in somewhere else. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Spawning", meta = (ClampMin = "0.0", Units = "Seconds"))
	float LostEnemyTimeout = 20.0f;

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

	/**
	 * Another spawner decides the waves (e.g. Ugurkan's BP_EnemySpawner). This director then spawns nothing by
	 * itself and only tracks the enemies, the score, game over and restart.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SetExternalWaves(bool bExternal);

	UFUNCTION(BlueprintPure, Category = "Arena")
	bool HasExternalWaves() const { return bExternalWaves; }

	/** Wave number to show while another spawner runs the waves. */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SetExternalWaveNumber(int32 Number);

	/**
	 * For a spawner that runs its own waves: the wave that has just started and how long until the next one
	 * (0 = unknown). Hands the waves over to it like SetExternalWaves(true).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	void SetExternalWave(int32 Number, float SecondsToNextWave);

	/** Seconds until the next wave starts, -1 if not known (our own waves: only during the break). */
	UFUNCTION(BlueprintPure, Category = "Arena")
	float GetTimeToNextWave() const;

	/** Enemies of one type still fighting. */
	UFUNCTION(BlueprintPure, Category = "Arena")
	int32 CountEnemies(ETankEnemyType Type) const;

	/**
	 * A place for an enemy: Wanted put on the ground if that is free, level ground away from the player,
	 * otherwise one of the usual spawn spots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FTransform FindEnemySpotNear(FVector Wanted, ETankEnemyType Type);

	/**
	 * A spawn spot the player is not looking at right now: one of the spawn points if the level has any, otherwise
	 * level ground around the player. bOutOfSight is false if every possible spot was in view.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FTransform GetHiddenSpawnTransform(ETankEnemyType Type, bool& bOutOfSight);

	/** Same for a Blueprint with its own enemy enum: 0 tank, 1 armored car, 2 AT gun. */
	UFUNCTION(BlueprintCallable, Category = "Arena")
	FTransform GetHiddenSpawnTransformByIndex(int32 TypeIndex, bool& bOutOfSight);

	/** Can the player see this point right now (the player tank's vision cone). */
	UFUNCTION(BlueprintPure, Category = "Arena")
	bool PlayerCanSee(FVector Point) const;

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
	bool SpawnNext();
	bool FindSpawnTransform(ETankEnemyType Type, FTransform& Out, bool bPreferInSight = false);
	bool IsSpotFree(const FVector& Location, float Radius) const;
	void BindPlayer();
	void ClearEnemies();
	void SetPhase(ETankArenaPhase NewPhase, float Duration = 0.0f);
	void BringBackLostEnemies(float DeltaSeconds);

	struct FLostCheck
	{
		FVector Anchor = FVector::ZeroVector;
		float Time = 0.0f;
		float Hidden = 0.0f;
	};
	TMap<TWeakObjectPtr<ATankEnemy>, FLostCheck> LostChecks;

	TWeakObjectPtr<ATankVehicle> Player;
	FTransform PlayerStart;
	TArray<TWeakObjectPtr<ATankEnemy>> Enemies;
	TArray<ETankEnemyType> Queue;
	TMap<TWeakObjectPtr<ATankEnemySpawnPoint>, double> SpawnPointUsed;
	ETankArenaPhase Phase = ETankArenaPhase::Idle;
	float PhaseTime = 0.0f;
	float PhaseDuration = 0.0f;
	float SpawnTimer = 0.0f;
	int32 GroupLeft = 0;
	float WaveTime = 0.0f;
	int32 Wave = 0;
	int32 Score = 0;
	int32 Kills = 0;
	float LastRepair = 0.0f;
	bool bExternalWaves = false;
	// World time the other spawner's next wave starts, < 0 if it did not say.
	double ExternalNextWaveTime = -1.0;
};
