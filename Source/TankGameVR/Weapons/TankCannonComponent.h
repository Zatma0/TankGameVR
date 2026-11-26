#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/TankDamageTypes.h"
#include "TankCannonComponent.generated.h"

class ATankGroundFire;
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

	/** AP round damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	/** HE round: direct hit damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|HE", meta = (ClampMin = "0.0"))
	float HEDamage = 25.0f;

	/** HE blast damage at the centre (falls off to 0 at HESplashRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|HE", meta = (ClampMin = "0.0"))
	float HESplashDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|HE", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float HESplashRadius = 450.0f;

	/** HE is slower than AP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|HE", meta = (ClampMin = "100.0", Units = "CentimetersPerSecond"))
	float HEMuzzleSpeed = 11000.0f;

	/** HE sets the ground on fire where it lands. None = no fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|HE")
	TSubclassOf<ATankGroundFire> HEGroundFireClass;

	/** AP: thin pale streak. HE: fat red one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	FLinearColor APTracerColor = FLinearColor(1.0f, 0.92f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	FLinearColor HETracerColor = FLinearColor(1.0f, 0.2f, 0.04f);

	/** Barrel tip distance from the barrel pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon", meta = (Units = "Centimeters"))
	float MuzzleDistance = 510.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	float FireSoundPitch = 1.0f;

	/** The same shot as the crew hears it inside the hull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> InteriorFireSound;

	/** AP on a vehicle: steel on steel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> APImpactSound;

	/** HE going off, wherever it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> HEImpactSound;

	/** AP into the ground or a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cannon|Feedback")
	TSoftObjectPtr<USoundBase> GroundImpactSound;

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

	/** What the last round did, for the display ("AP HIT TANK -25"). */
	UFUNCTION(BlueprintPure, Category = "Cannon")
	FString GetShotReport() const { return ShotReport; }

	UFUNCTION(BlueprintPure, Category = "Cannon")
	float GetTimeSinceShotReport() const;

	UFUNCTION(BlueprintPure, Category = "Cannon")
	FTransform GetMuzzleTransform() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleShellImpact(AActor* HitActor, FVector Location);

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
	FString ShotReport;
	double ShotReportTime = -1000.0;
	ETankRoundType LastRound = ETankRoundType::AP;
	float LastDamage = 0.0f;
	float FlashRemaining = 0.0f;
};
