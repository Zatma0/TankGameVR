#include "Combat/TankHealthComponent.h"
#include "Targeting/TargetableComponent.h"
#include "TankGameVR.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"

UTankHealthComponent::UTankHealthComponent()
{
	// Only ticks for owners that repair themselves.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UTankHealthComponent::IsRepairing() const
{
	return RepairPerSecond > 0.0f && !bDead && Health < MaxHealth && GetWorld() && GetWorld()->GetTimeSeconds() - LastDamageTime >= RepairDelay;
}

void UTankHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsRepairing())
	{
		Health = FMath::Min(MaxHealth, Health + RepairPerSecond * DeltaTime);
	}
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
	SetComponentTickEnabled(RepairPerSecond > 0.0f);
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
	LastDamageTime = -1000.0;
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
	LastDamageTime = GetWorld()->GetTimeSeconds();
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
