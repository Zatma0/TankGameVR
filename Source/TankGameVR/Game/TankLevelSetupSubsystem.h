#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Enemies/TankEnemyStats.h"
#include "TankLevelSetupSubsystem.generated.h"

class ATankArenaDirector;
class ATankEnemy;
class UTankHealthComponent;

/**
 * Fills in, while the game runs, what a level with a player tank is missing. The level itself is never changed.
 * - No arena director: one is added.
 * - No NavMeshBoundsVolume: navigation is built around the player, so enemies path around buildings.
 * - Big meshes imported without simple collision: a blocking box around each, so nothing drives or shoots through.
 * - A Blueprint spawner with its own placeholder enemies (Ugurkan's BP_EnemySpawner / BP_EnemyBase): every placeholder
 *   is swapped for the real enemy of that type. From its first enemy on that spawner decides when and what, and the
 *   director only tracks them. Until then (or if it never manages to spawn anything) the director runs its own waves.
 * Whatever the level already has is left alone. Tank.LevelSetup 0 switches all of it off.
 */
UCLASS()
class TANKGAMEVR_API UTankLevelSetupSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Does all of the above now. Runs by itself at BeginPlay in levels that have a player tank. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Level")
	void SetUpLevel();

	/** The level's director; adds one if there is none. bExternalWaves: a spawner of its own decides the waves. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Level")
	ATankArenaDirector* GetOrCreateDirector(bool bExternalWaves);

	/** Builds navigation in a box around Center if the level has no NavMeshBoundsVolume. True if it added one. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Level")
	bool CreateNavigationIfMissing(FVector Center, FVector Size);

	/** Adds blocking boxes for big static meshes that have no simple collision. Returns how many. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Level")
	int32 BlockMeshesWithoutCollision(float MinSize = 300.0f);

	/**
	 * Actors of this class are placeholders to swap for real enemies. TypeProperty is a byte/enum on them
	 * (0 tank, 1 armored car, 2 AT gun); DiedEvent is their event dispatcher that fires when ours is destroyed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tank|Level")
	void SetPlaceholderEnemyClass(UClass* EnemyClass, FName TypeProperty = TEXT("SpawnedEnemyType"), FName DiedEvent = TEXT("OnEnemyDied"));

	/** The real enemy standing in for a placeholder, if any. */
	UFUNCTION(BlueprintPure, Category = "Tank|Level")
	ATankEnemy* GetReplacement(const AActor* Placeholder) const;

	UFUNCTION(BlueprintPure, Category = "Tank|Level")
	int32 GetReplacedCount() const { return Replaced; }

	UFUNCTION(BlueprintPure, Category = "Tank|Level")
	int32 GetBlockerCount() const { return Blockers; }

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UFUNCTION()
	void HandleOursDied(UTankHealthComponent* HealthComponent, AActor* Killer);

	UFUNCTION()
	void HandleActorDestroyed(AActor* Actor);

private:
	struct FSwap
	{
		TWeakObjectPtr<AActor> Placeholder;
		TWeakObjectPtr<ATankEnemy> Ours;
	};

	void HandleActorSpawned(AActor* Actor);
	void WatchForPlaceholders();
	void Replace(AActor* Placeholder);

	UPROPERTY(Transient)
	TObjectPtr<UClass> PlaceholderClass;

	UPROPERTY(Transient)
	TObjectPtr<AActor> BlockerHolder;

	TArray<FSwap> Swaps;
	TSet<TWeakObjectPtr<const UPrimitiveComponent>> BlockedMeshes;
	TArray<TWeakObjectPtr<AActor>> Pending;
	FDelegateHandle SpawnHandle;
	FName PlaceholderTypeProperty = TEXT("SpawnedEnemyType");
	FName PlaceholderDiedEvent = TEXT("OnEnemyDied");
	int32 Replaced = 0;
	int32 Blockers = 0;
	bool bSetUp = false;
};
