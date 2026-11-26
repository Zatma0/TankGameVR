#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankHealthComponent.generated.h"

class UDamageType;
class UTankHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnTankHealthChanged, UTankHealthComponent*, HealthComponent, float, Damage, const UDamageType*, DamageType);
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

	/** Armour: damage of these types is scaled (e.g. HE x0.5 on a tank). Types not listed do full damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	TMap<TSubclassOf<UDamageType>, float> DamageMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	bool bDestroyOwnerOnDeath = true;

	/** HP per second that come back while nothing has hit it for RepairDelay. 0 = no self repair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Repair", meta = (ClampMin = "0.0"))
	float RepairPerSecond = 0.0f;

	/** Seconds without any damage (fire counts) before the repair starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Repair", meta = (ClampMin = "0.0", Units = "Seconds"))
	float RepairDelay = 8.0f;

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
	float GetHealthFraction() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	/** True while HP are coming back by themselves. */
	UFUNCTION(BlueprintPure, Category = "Health|Repair")
	bool IsRepairing() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetDamageMultiplier(TSubclassOf<UDamageType> DamageType) const;

	/** Adds HP (not above MaxHealth, not on a dead owner). Returns what was added. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float Amount);

	/** Sets MaxHealth and fills up. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void SetMaxHealth(float NewMaxHealth);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetHealth();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

private:
	float Health = 0.0f;
	double LastDamageTime = -1000.0;
	bool bDead = false;
	bool bStarted = false;
};
