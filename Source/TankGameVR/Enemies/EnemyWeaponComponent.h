#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyWeaponComponent.generated.h"

class ATankShellProjectile;
class UMaterialInterface;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyWeaponFired, ATankShellProjectile*, Projectile);

/**
 * Enemy gun. Turns the turret (TurretPivot) and gun (BarrelPivot) towards the target at a limited speed, needs a
 * clear line of sight (smoke counts) and to stay on target for AimTime, then fires a burst and reloads.
 * Accuracy gets better the longer it tracks, worse while its own vehicle moves.
 * Optional named children: Muzzle (shot origin), MuzzleFlash, AimLaser (stretched along the gun while aiming).
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UEnemyWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyWeaponComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<ATankShellProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<UDamageType> DamageType;

	/** Rounds fly through actors of this class (no friendly fire). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<AActor> FriendlyClass;

	/** Per round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "100.0", Units = "CentimetersPerSecond"))
	float ProjectileSpeed = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1"))
	float TracerScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "1"))
	int32 BurstCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.02", Units = "Seconds"))
	float BurstInterval = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1", Units = "Seconds"))
	float ReloadTime = 5.0f;

	/** Time on target before the first shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.0", Units = "Seconds"))
	float AimTime = 1.0f;

	/** Losing sight restarts the aim (the AT gun has to lay again). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bResetAimWhenSightLost = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon", meta = (ClampMin = "0.1", Units = "Degrees"))
	float OnTargetTolerance = 2.0f;

	/** Spread when it starts tracking, and after tracking for SettleTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (Units = "Degrees"))
	float SpreadStart = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (Units = "Degrees"))
	float SpreadMin = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "0.1", Units = "Seconds"))
	float SettleTime = 3.0f;

	/** Spread multiplier while the own vehicle drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy", meta = (ClampMin = "1.0"))
	float MovingSpreadMultiplier = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Accuracy")
	bool bLeadTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Turret", meta = (ClampMin = "1.0", Units = "Degrees"))
	float TraverseSpeed = 25.0f;

	/** How far the turret turns either side of the hull (180 = all round). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Turret", meta = (ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float TraverseLimit = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Turret", meta = (ClampMin = "1.0", Units = "Degrees"))
	float ElevationSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Turret")
	FVector2D PitchLimits = FVector2D(-8.0, 18.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Setup")
	FName TurretComponentName = TEXT("TurretPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Setup")
	FName BarrelComponentName = TEXT("BarrelPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Setup")
	FName MuzzleComponentName = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Setup")
	FName FlashComponentName = TEXT("MuzzleFlash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Setup")
	FName LaserComponentName = TEXT("AimLaser");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Feedback")
	TSoftObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Feedback")
	float FireSoundPitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Feedback")
	TSoftObjectPtr<UMaterialInterface> TracerMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnEnemyWeaponFired OnFired;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetTarget(AActor* InTarget);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AActor* GetTarget() const { return Target.Get(); }

	/** The brain decides when it may shoot. Aiming happens anyway. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetWeaponsFree(bool bFree) { bWeaponsFree = bFree; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWeaponsFree() const { return bWeaponsFree; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasLineOfSight() const { return bLineOfSight; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsOnTarget() const { return bOnTarget; }

	/** 0..1 of AimTime while laying on the target. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetAimProgress() const { return AimTime > 0.0f ? FMath::Clamp(AimHold / AimTime, 0.0f, 1.0f) : (bOnTarget ? 1.0f : 0.0f); }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return ReloadRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetShotsFired() const { return ShotsFired; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentSpread() const;

	/** Where the gun points. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetGunDirection() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	/** Point on the target the gun aims at (with lead). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetAimPoint() const { return AimPoint; }

	/** Fires right away if loaded, ignoring the aim (tests). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool FireNow();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateTargetMotion(float DeltaTime);
	void UpdateTurret(float DeltaTime);
	bool CheckLineOfSight() const;
	void FireRound();
	void UpdateLaser();

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Turret;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Barrel;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Flash;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Laser;

	TWeakObjectPtr<AActor> Target;
	FVector TargetLastLocation = FVector::ZeroVector;
	FVector TargetVelocity = FVector::ZeroVector;
	FVector OwnerLastLocation = FVector::ZeroVector;
	float OwnerSpeed = 0.0f;
	FVector AimPoint = FVector::ZeroVector;
	FVector LaserScale = FVector::OneVector;
	float TurretYaw = 0.0f;
	float GunPitch = 0.0f;
	float AimHold = 0.0f;
	float TrackTime = 0.0f;
	float ReloadRemaining = 0.0f;
	float BurstRemainingTime = 0.0f;
	float FlashRemaining = 0.0f;
	float LosTimer = 0.0f;
	int32 BurstLeft = 0;
	int32 ShotsFired = 0;
	bool bWeaponsFree = false;
	bool bLineOfSight = false;
	bool bOnTarget = false;
};
