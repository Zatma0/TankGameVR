#include "Combat/TankHealthComponent.h"
#include "Targeting/TargetableComponent.h"
#include "TankGameVR.h"
#include "GameFramework/Actor.h"

UTankHealthComponent::UTankHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTankHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UTankHealthComponent::HandleAnyDamage);
}

void UTankHealthComponent::ResetHealth()
{
	Health = MaxHealth;
	bDead = false;
}

void UTankHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (bDead || Damage <= 0.0f)
	{
		return;
	}
	Health = FMath::Max(0.0f, Health - Damage);
	OnHealthChanged.Broadcast(this, Damage);
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
