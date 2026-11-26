#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CockpitCableJoltComponent.generated.h"

class ATankShellProjectile;
class UCableComponent;
class UDamageType;
class UTankHealthComponent;

/**
 * Gives the cockpit cables a kick when the gun fires or the hull is hit. They already swing on their own when the
 * tank drives, turns and brakes (the cable sim lives in world space); this adds the jolt of a shot.
 * A cable whose end is not tied to anything gets its EndLocation read in its parent's space.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitCableJoltComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCockpitCableJoltComponent();

	/** Push given to the cables by our own shot (cm/s^2, along the hull, backwards). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables")
	float ShotJolt = 2500.0f;

	/** Push from a hit, scaled by the damage (full at 30 HP). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables")
	float HitJolt = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cables", meta = (Units = "Seconds"))
	float JoltTime = 0.12f;

	/** Kick all the owner's cables (direction in world space, cm/s^2). */
	UFUNCTION(BlueprintCallable, Category = "Cables")
	void Jolt(FVector Push);

	UFUNCTION(BlueprintPure, Category = "Cables")
	int32 GetJoltCount() const { return JoltCount; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleFired(ATankShellProjectile* Projectile);

	UFUNCTION()
	void HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCableComponent>> Cables;

	float Remaining = 0.0f;
	int32 JoltCount = 0;
};
