#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/TankEnemyStats.h"
#include "TankEnemy.generated.h"

class UBoxComponent;
class UDataTable;
class UEnemyDriveComponent;
class UEnemyWeaponComponent;
class UTankEnemyAudioComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UTankHealthComponent;
class UTargetableComponent;

UENUM(BlueprintType)
enum class ETankEnemyState : uint8
{
	Advancing,
	Engaging,
	Repositioning,
	Deploying,
	Holding,
	Wrecked
};

/**
 * Shared enemy vehicle. HP, damage, speed and range come from Ugurkan's DT_EnemyFeatures (row by EnemyType).
 * The behaviour is in the subclasses (tank, armored car, anti-tank gun). Meshes live in the BP_Enemy_* Blueprints.
 * If the table row points to a real model (not an engine basic shape), it is shown on TableModel instead of the
 * placeholder meshes.
 */
UCLASS(Abstract, Blueprintable)
class TANKGAMEVR_API ATankEnemy : public AActor
{
	GENERATED_BODY()

public:
	ATankEnemy();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	ETankEnemyType EnemyType = ETankEnemyType::Tank;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	TSoftObjectPtr<UDataTable> StatsTable;

	/** StatsTable if that asset exists, otherwise Ugurkan's DT_EnemyFeatures wherever it is. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Stats")
	UDataTable* FindStatsTable() const;

	/** Ugurkan's DT_EnemyFeatures. He has moved it once, so both places are checked. */
	static UDataTable* FindDefaultStatsTable();

	/** Empty = the default row for EnemyType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	FName StatsRow;

	/** Filled from the table at BeginPlay; these are used when the table is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	FTankEnemyStats Stats;

	/** Tuning on top of the table range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "0.1"))
	float RangeScale = 1.0f;

	/** The table range is where it likes to fight; it opens fire from this many times further out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "1.0"))
	float FireRangeScale = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	int32 KillScore = 100;

	/** How long the burning wreck stays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Wreck", meta = (Units = "Seconds"))
	float WreckTime = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Wreck")
	TSoftObjectPtr<UMaterialInterface> WreckMaterial;

	/** Shown only on the wreck (flames, smoke). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Wreck")
	FName WreckFireComponentName = TEXT("WreckFire");

	UFUNCTION(BlueprintPure, Category = "Enemy")
	ETankEnemyState GetEnemyState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	bool IsWrecked() const { return State == ETankEnemyState::Wrecked; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetEngageRange() const { return Stats.Range * RangeScale; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetFireRange() const { return GetEngageRange() * FireRangeScale; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	AActor* GetPlayerTank() const { return Player.Get(); }

	/** Is this enemy laying its gun on Actor right now (for the cockpit warning). 0..1, 0 = no. */
	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetThreatTo(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FText GetTypeName() const;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UTankHealthComponent* GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UEnemyDriveComponent* GetDrive() const { return Drive; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UEnemyWeaponComponent* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UTargetableComponent* GetTargetable() const { return Targetable; }

	/** Re-reads the table and applies the stats (BeginPlay does this). */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void LoadStats();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Type specific: put the stats on the drive and gun. */
	virtual void ApplyStats();

	/** Type specific behaviour, called every frame while alive. */
	virtual void UpdateBehaviour(float DeltaSeconds) {}

	/** Called when hit (not killed). */
	virtual void HandleHit(float Damage) {}

	void SetState(ETankEnemyState NewState);
	float GetStateTime() const { return StateTime; }
	float GetDistanceToPlayer() const;

	/** A point Distance away from the player at Angle degrees from the player-to-us direction, on the nav mesh if any. */
	FVector PointAroundPlayer(float Distance, float AngleOffset) const;

	UFUNCTION()
	void HandleDamaged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType);

	UFUNCTION()
	void HandleDied(UTankHealthComponent* HealthComponent, AActor* Killer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UBoxComponent> HullCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<USceneComponent> TurretPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<USceneComponent> BarrelPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<USceneComponent> Muzzle;

	/** Shows the table's model when it has a real one. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UStaticMeshComponent> TableModel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UTargetableComponent> Targetable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UTankHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UEnemyDriveComponent> Drive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UTankEnemyAudioComponent> Sounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy")
	TObjectPtr<UEnemyWeaponComponent> Weapon;

	TWeakObjectPtr<AActor> Player;

private:
	void FindPlayer();
	void ShowTableModel();
	void BecomeWreck();

	ETankEnemyState State = ETankEnemyState::Advancing;
	float StateTime = 0.0f;
	float PlayerSearchTimer = 0.0f;
	bool bStatsLoaded = false;
};
