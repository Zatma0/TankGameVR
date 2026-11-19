#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankDamageReactionComponent.generated.h"

class UDamageType;
class UHapticFeedbackEffect_Base;
class UPointLightComponent;
class UTankHealthComponent;

/**
 * What a hit feels like inside the player tank: a kick in both hands, the cockpit light flickers, the display
 * flashes and says where it came from, and heavy hits can start a fire. No camera shake (VR comfort).
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankDamageReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankDamageReactionComponent();

	/** Chance that a heavy hit (tank or AT gun) starts a fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HeavyHitFireChance = 0.45f;

	/** Chance per autocannon round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LightHitFireChance = 0.06f;

	/** Hits below this are light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float HeavyHitDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FName CockpitLightName = TEXT("CockpitLight");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> HitHaptic;

	/** 1 right after a hit, fades to 0. */
	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetHitFlash() const { return HitFlash; }

	/** Side of the last hit for the display: FRONT, LEFT, RIGHT, REAR. */
	UFUNCTION(BlueprintPure, Category = "Damage")
	FString GetLastHitSide() const { return LastHitSide; }

	UFUNCTION(BlueprintPure, Category = "Damage")
	float GetTimeSinceHit() const { return TimeSinceHit; }

	/** Puts out every fire and clears the flash (restart). */
	UFUNCTION(BlueprintCallable, Category = "Damage")
	void ResetDamage();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType);

	UFUNCTION()
	void HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser);

private:
	void StartFire();

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> CockpitLight;

	FString LastHitSide;
	float HitFlash = 0.0f;
	float FlickerTime = 0.0f;
	float LightIntensity = 0.0f;
	float TimeSinceHit = 1000.0f;
};
