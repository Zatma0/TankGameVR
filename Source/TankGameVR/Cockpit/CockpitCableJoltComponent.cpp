#include "Cockpit/CockpitCableJoltComponent.h"
#include "CableComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "GameFramework/Actor.h"
#include "Weapons/TankCannonComponent.h"

UCockpitCableJoltComponent::UCockpitCableJoltComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCockpitCableJoltComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	TArray<UCableComponent*> Found;
	Owner->GetComponents(Found);
	Cables.Append(Found);
	for (UCableComponent* Cable : Found)
	{
		// A cable with nothing set ties its end to the actor root; here the end is meant in the space of the part the
		// cable hangs from (e.g. CockpitRoot), so moving that part moves both ends.
		if (!Cable->AttachEndTo.OverrideComponent.IsValid() && Cable->AttachEndTo.ComponentProperty.IsNone() && Cable->GetAttachParent())
		{
			Cable->SetAttachEndToComponent(Cable->GetAttachParent());
		}
	}
	if (UTankCannonComponent* Cannon = Owner->FindComponentByClass<UTankCannonComponent>())
	{
		Cannon->OnFired.AddUniqueDynamic(this, &UCockpitCableJoltComponent::HandleFired);
	}
	if (UTankHealthComponent* Health = Owner->FindComponentByClass<UTankHealthComponent>())
	{
		Health->OnHealthChanged.AddUniqueDynamic(this, &UCockpitCableJoltComponent::HandleHealthChanged);
	}
}

void UCockpitCableJoltComponent::Jolt(FVector Push)
{
	for (UCableComponent* Cable : Cables)
	{
		if (Cable)
		{
			// A little different for each cable so they do not swing as one.
			Cable->CableForce = Push * FMath::FRandRange(0.7f, 1.2f) + FMath::VRand() * Push.Size() * 0.25f;
		}
	}
	Remaining = JoltTime;
	++JoltCount;
	SetComponentTickEnabled(true);
}

void UCockpitCableJoltComponent::HandleFired(ATankShellProjectile* Projectile)
{
	Jolt(-GetOwner()->GetActorForwardVector() * ShotJolt + FVector(0, 0, ShotJolt * 0.4f));
}

void UCockpitCableJoltComponent::HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType)
{
	if (Damage <= 0.0f || (DamageType && DamageType->IsA<UTankDamageType_Fire>()))
	{
		return;
	}
	Jolt(FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.6f).GetSafeNormal() * HitJolt * FMath::Clamp(Damage / 30.0f, 0.2f, 1.0f));
}

void UCockpitCableJoltComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Remaining -= DeltaTime;
	if (Remaining <= 0.0f)
	{
		for (UCableComponent* Cable : Cables)
		{
			if (Cable)
			{
				Cable->CableForce = FVector::ZeroVector;
			}
		}
		SetComponentTickEnabled(false);
	}
}
