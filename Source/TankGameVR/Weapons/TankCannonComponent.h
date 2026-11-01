#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankCannonComponent.generated.h"

class ATankShellProjectile;
class UHapticFeedbackEffect_Base;
class USoundBase;
class UTankBreechComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCannonFired, ATankShellProjectile*, Projectile);

/**
 * Main gun. Fire() only works when the breech is loaded; then it sends a round along the aim direction from the
 * barrel tip, with flash, sound and a kick in both hands.
 */
UCLASS(ClassGroup = (Weapons), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankCannonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankCannonComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon")
	TSubclassOf<ATankShellProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon", meta = (ClampMin = "100.0", Units = "CentimetersPerSecond"))
	float MuzzleSpeed = 15000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	/** Barrel tip distance from the barrel pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon", meta = (Units = "Centimeters"))
	float MuzzleDistance = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Setup")
	FName BarrelComponentName = TEXT("BarrelPivot");

	/** Optional children of the barrel shown for a moment on each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Setup")
	FName MuzzleFlashComponentName = TEXT("MuzzleFlash");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Setup")
	FName MuzzleLightComponentName = TEXT("MuzzleFlashLight");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback", meta = (ClampMin = "0.01", Units = "Seconds"))
	float MuzzleFlashTime = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> FireSound;

	/** Low pitch makes the pistol sample sound like a big gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	float FireSoundPitch = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> DryFireSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> FireHaptic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> DryFireHaptic;

	UPROPERTY(BlueprintAssignable, Category = "Cannon")
	FOnCannonFired OnFired;

	/** Returns false (click) when nothing is loaded. */
	UFUNCTION(BlueprintCallable, Category = "Cannon")
	bool Fire();

	UFUNCTION(BlueprintPure, Category = "Cannon")
	int32 GetShotsFired() const { return ShotsFired; }

	UFUNCTION(BlueprintPure, Category = "Cannon")
	FTransform GetMuzzleTransform() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void ShowMuzzleFlash(bool bShow);
	void PlayBothHands(UHapticFeedbackEffect_Base* Effect, float Scale) const;

	UPROPERTY(Transient)
	TObjectPtr<UTankBreechComponent> Breech;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Barrel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> FlashComponents;

	int32 ShotsFired = 0;
	float FlashRemaining = 0.0f;
};
