#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankHealthComponent.generated.h"

class UTankHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTankHealthChanged, UTankHealthComponent*, HealthComponent, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTankDied, UTankHealthComponent*, HealthComponent, AActor*, Killer);

/**
 * Hit points for anything the cannon can shoot. Listens to the owner's standard Unreal damage (Event AnyDamage),
 * so Blueprint enemies can use their own HP instead and skip this component.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bDestroyOwnerOnDeath = true;

	/** Delay before the owner is removed (lets the hit flash play). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0.0", Units = "Seconds"))
	float DestroyDelay = 0.3f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnTankHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnTankDied OnDied;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

private:
	float Health = 0.0f;
	bool bDead = false;
};
