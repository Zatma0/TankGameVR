#include "Combat/TankDamageReactionComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Cockpit/CockpitGrabUtils.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/Actor.h"
#include "Haptics/HapticFeedbackEffect_Base.h"

UTankDamageReactionComponent::UTankDamageReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	HitHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/PistolFireHapticEffect.PistolFireHapticEffect")));
}

void UTankDamageReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (UTankHealthComponent* Health = Owner->FindComponentByClass<UTankHealthComponent>())
	{
		Health->OnHealthChanged.AddDynamic(this, &UTankDamageReactionComponent::HandleHealthChanged);
	}
	Owner->OnTakePointDamage.AddDynamic(this, &UTankDamageReactionComponent::HandlePointDamage);

	TArray<UPointLightComponent*> Lights;
	Owner->GetComponents(Lights);
	for (UPointLightComponent* Light : Lights)
	{
		if (Light->GetFName() == CockpitLightName)
		{
			CockpitLight = Light;
			LightIntensity = Light->Intensity;
		}
	}
}

void UTankDamageReactionComponent::HandlePointDamage(AActor* DamagedActor, float Damage, AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* HitComponent, FName BoneName, FVector ShotFromDirection, const UDamageType* DamageType, AActor* DamageCauser)
{
	// ShotFromDirection is the way the round was flying.
	const FVector Local = GetOwner()->GetActorTransform().InverseTransformVectorNoScale(-ShotFromDirection);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	if (FMath::Abs(Yaw) <= 45.0f)
	{
		LastHitSide = TEXT("FRONT");
	}
	else if (FMath::Abs(Yaw) >= 135.0f)
	{
		LastHitSide = TEXT("REAR");
	}
	else
	{
		LastHitSide = Yaw > 0.0f ? TEXT("RIGHT") : TEXT("LEFT");
	}
}

void UTankDamageReactionComponent::HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType)
{
	if (DamageType && DamageType->IsA<UTankDamageType_Fire>())
	{
		return;
	}
	const bool bHeavy = Damage >= HeavyHitDamage;
	HitFlash = 1.0f;
	TimeSinceHit = 0.0f;
	FlickerTime = bHeavy ? 0.7f : 0.25f;
	const float Kick = bHeavy ? 1.0f : 0.45f;
	CockpitGrab::PlayHaptic(GetWorld(), HitHaptic.LoadSynchronous(), EControllerHand::Left, Kick);
	CockpitGrab::PlayHaptic(GetWorld(), HitHaptic.LoadSynchronous(), EControllerHand::Right, Kick);

	if (!HealthComponent->IsDead() && FMath::FRand() < (bHeavy ? HeavyHitFireChance : LightHitFireChance))
	{
		StartFire();
	}
}

void UTankDamageReactionComponent::StartFire()
{
	TArray<UCockpitFireComponent*> Spots;
	GetOwner()->GetComponents(Spots);
	if (Spots.Num() > 0)
	{
		Spots[FMath::RandRange(0, Spots.Num() - 1)]->Ignite(0.35f);
	}
}

void UTankDamageReactionComponent::ResetDamage()
{
	TArray<UCockpitFireComponent*> Spots;
	GetOwner()->GetComponents(Spots);
	for (UCockpitFireComponent* Spot : Spots)
	{
		Spot->PutOut();
	}
	HitFlash = 0.0f;
	FlickerTime = 0.0f;
	TimeSinceHit = 1000.0f;
	LastHitSide.Reset();
	if (CockpitLight)
	{
		CockpitLight->SetIntensity(LightIntensity);
	}
}

void UTankDamageReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TimeSinceHit += DeltaTime;
	HitFlash = FMath::Max(0.0f, HitFlash - DeltaTime * 1.5f);
	if (CockpitLight)
	{
		if (FlickerTime > 0.0f)
		{
			FlickerTime -= DeltaTime;
			CockpitLight->SetIntensity(FMath::FRand() < 0.5f ? LightIntensity * 0.1f : LightIntensity * FMath::FRandRange(0.6f, 1.2f));
		}
		else
		{
			CockpitLight->SetIntensity(LightIntensity);
		}
	}
}
