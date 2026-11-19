#include "Combat/TankHealthComponent.h"
#include "Targeting/TargetableComponent.h"
#include "TankGameVR.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"

UTankHealthComponent::UTankHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTankHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bStarted)
	{
		Health = MaxHealth;
	}
	bStarted = true;
	GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this, &UTankHealthComponent::HandleAnyDamage);
}

float UTankHealthComponent::GetDamageMultiplier(TSubclassOf<UDamageType> DamageType) const
{
	for (UClass* Class = DamageType.Get(); Class; Class = Class->GetSuperClass())
	{
		if (const float* Multiplier = DamageMultipliers.Find(Class))
		{
			return *Multiplier;
		}
	}
	return 1.0f;
}

void UTankHealthComponent::SetMaxHealth(float NewMaxHealth)
{
	MaxHealth = FMath::Max(1.0f, NewMaxHealth);
	bStarted = true;
	ResetHealth();
}

float UTankHealthComponent::Heal(float Amount)
{
	if (bDead || Amount <= 0.0f)
	{
		return 0.0f;
	}
	const float Before = Health;
	Health = FMath::Min(MaxHealth, Health + Amount);
	return Health - Before;
}

void UTankHealthComponent::ResetHealth()
{
	Health = MaxHealth;
	bDead = false;
	if (UTargetableComponent* Targetable = GetOwner() ? GetOwner()->FindComponentByClass<UTargetableComponent>() : nullptr)
	{
		Targetable->SetTargetable(true);
	}
}

void UTankHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.0f)
	{
		return;
	}
	Damage *= GetDamageMultiplier(DamageType ? DamageType->GetClass() : nullptr);
	if (Damage <= 0.0f)
	{
		return;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	OnHealthChanged.Broadcast(this, Damage, DamageType);
	UE_LOG(LogTankGame, Log, TEXT("%s took %.0f damage, %.0f left"), *GetOwner()->GetName(), Damage, Health);
	if (Health > 0.0f)
	{
		return;
	}

	bDead = true;
	AActor* Owner = GetOwner();
	// Drop the lock-on right away, even if the wreck stays for a moment.
	if (UTargetableComponent* Targetable = Owner->FindComponentByClass<UTargetableComponent>())
	{
		Targetable->SetTargetable(false);
	}
	OnDied.Broadcast(this, DamageCauser ? DamageCauser->GetOwner() : nullptr);
	if (bDestroyOwnerOnDeath)
	{
		if (DestroyDelay > 0.0f)
		{
			Owner->SetLifeSpan(DestroyDelay);
		}
		else
		{
			Owner->Destroy();
		}
	}
}
