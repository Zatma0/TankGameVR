#include "Cockpit/CockpitExtinguisherComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Cockpit/CockpitGrabUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UCockpitExtinguisherComponent::UCockpitExtinguisherComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	SprayMesh = Sphere.Object;
	SprayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Extinguisher_Spray.MI_Extinguisher_Spray")));
	SprayHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/GrabHapticEffect.GrabHapticEffect")));
}

void UCockpitExtinguisherComponent::BeginPlay()
{
	Super::BeginPlay();
	// Spray lives in the cockpit space so it rides along with the tank.
	USceneComponent* Parent = GetAttachParent();
	UMaterialInterface* Material = SprayMaterial.LoadSynchronous();
	for (int32 Index = 0; Index < SprayPuffs && Parent; ++Index)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(GetOwner());
		Puff->SetStaticMesh(SprayMesh);
		if (Material)
		{
			Puff->SetMaterial(0, Material);
		}
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetMobility(EComponentMobility::Movable);
		Puff->SetupAttachment(Parent);
		Puff->RegisterComponent();
		Puff->SetVisibility(false);
		PuffMeshes.Add(Puff);
	}
	Puffs.SetNum(PuffMeshes.Num());
}

void UCockpitExtinguisherComponent::SetSpraying(bool bInSpraying)
{
	bTrigger = bInSpraying;
}

bool UCockpitExtinguisherComponent::IsSpraying() const
{
	return Charge > 0.0f && ((bTrigger && IsHeld()) || AutoRemaining > 0.0f);
}

void UCockpitExtinguisherComponent::HandleReleased()
{
	bTrigger = false;
}

FVector UCockpitExtinguisherComponent::GetNozzleLocation() const
{
	return GetComponentTransform().TransformPositionNoScale(NozzleOffset);
}

FVector UCockpitExtinguisherComponent::GetSprayDirection() const
{
	if (AutoRemaining > 0.0f && AutoTarget.IsValid())
	{
		return (AutoTarget->GetComponentLocation() + FVector(0, 0, 10) - GetNozzleLocation()).GetSafeNormal();
	}
	return GetForwardVector();
}

UCockpitFireComponent* UCockpitExtinguisherComponent::FindBiggestFire() const
{
	TArray<UCockpitFireComponent*> Fires;
	GetOwner()->GetComponents(Fires);
	UCockpitFireComponent* Biggest = nullptr;
	for (UCockpitFireComponent* Fire : Fires)
	{
		if (Fire->IsBurning() && (!Biggest || Fire->GetIntensity() > Biggest->GetIntensity()))
		{
			Biggest = Fire;
		}
	}
	return Biggest;
}

bool UCockpitExtinguisherComponent::AutoSpray()
{
	UCockpitFireComponent* Fire = FindBiggestFire();
	if (!Fire || Charge <= 0.0f)
	{
		return false;
	}
	AutoTarget = Fire;
	AutoRemaining = AutoSprayTime;
	return true;
}

void UCockpitExtinguisherComponent::SprayFires(float DeltaTime, const FVector& Origin, const FVector& Direction)
{
	TArray<UCockpitFireComponent*> Fires;
	GetOwner()->GetComponents(Fires);
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SprayAngle));
	for (UCockpitFireComponent* Fire : Fires)
	{
		if (!Fire->IsBurning())
		{
			continue;
		}
		const FVector To = Fire->GetComponentLocation() + FVector(0, 0, 10) - Origin;
		const float Distance = static_cast<float>(To.Size());
		const float Dot = static_cast<float>(FVector::DotProduct(To.GetSafeNormal(), Direction));
		// A bit of slack so a fire at arm's length still counts when the cone just misses its middle.
		if (Distance > SprayRange + Fire->Radius || Dot < CosHalf - Fire->Radius / FMath::Max(Distance, 1.0f))
		{
			continue;
		}
		// Best in the middle of the cone and not too far.
		const float Aim = FMath::Clamp((Dot - CosHalf) / (1.0f - CosHalf), 0.3f, 1.0f);
		const float Reach = FMath::Clamp(1.2f - Distance / SprayRange, 0.3f, 1.0f);
		Fire->Extinguish(ExtinguishRate * Aim * Reach * DeltaTime);
	}
}

void UCockpitExtinguisherComponent::EmitPuffs(float DeltaTime, const FVector& Origin, const FVector& Direction)
{
	const USceneComponent* Parent = GetAttachParent();
	if (!Parent)
	{
		return;
	}
	EmitTimer -= DeltaTime;
	if (EmitTimer > 0.0f)
	{
		return;
	}
	EmitTimer = 0.035f;
	const FTransform Space = Parent->GetComponentTransform();
	for (FSprayPuff& Puff : Puffs)
	{
		if (Puff.Age < 0.0f)
		{
			const FVector World = FMath::VRandCone(Direction, FMath::DegreesToRadians(SprayAngle * 0.6f)) * FMath::FRandRange(260.0f, 340.0f);
			Puff.Age = 0.0f;
			Puff.Location = Space.InverseTransformPositionNoScale(Origin);
			Puff.Velocity = Space.InverseTransformVectorNoScale(World);
			return;
		}
	}
}

void UCockpitExtinguisherComponent::UpdatePuffs(float DeltaTime)
{
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		FSprayPuff& Puff = Puffs[Index];
		UStaticMeshComponent* Mesh = PuffMeshes[Index];
		if (Puff.Age < 0.0f)
		{
			continue;
		}
		Puff.Age += DeltaTime;
		const float T = Puff.Age / 0.7f;
		if (T >= 1.0f)
		{
			Puff.Age = -1.0f;
			Mesh->SetVisibility(false);
			continue;
		}
		// Powder: fast out of the nozzle, slows, spreads and sinks a little.
		Puff.Velocity *= FMath::Max(0.0f, 1.0f - 3.5f * DeltaTime);
		Puff.Velocity.Z -= 40.0f * DeltaTime;
		Puff.Location += Puff.Velocity * DeltaTime;
		const float Size = (3.0f + 22.0f * T) * (1.0f - T * T * T);
		Mesh->SetRelativeLocation(Puff.Location);
		Mesh->SetRelativeScale3D(FVector(FMath::Max(Size, 0.1f) / 100.0f));
		Mesh->SetVisibility(true);
	}
}

void UCockpitExtinguisherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (AutoRemaining > 0.0f)
	{
		AutoRemaining -= DeltaTime;
		if (!AutoTarget.IsValid() || !AutoTarget->IsBurning())
		{
			AutoTarget = FindBiggestFire();
			if (!AutoTarget.IsValid())
			{
				AutoRemaining = 0.0f;
			}
		}
	}

	if (IsSpraying())
	{
		const FVector Origin = GetNozzleLocation();
		const FVector Direction = GetSprayDirection();
		Charge = FMath::Max(0.0f, Charge - UsePerSecond * DeltaTime);
		SprayFires(DeltaTime, Origin, Direction);
		EmitPuffs(DeltaTime, Origin, Direction);
		HapticTimer -= DeltaTime;
		if (HapticTimer <= 0.0f && GetHand())
		{
			HapticTimer = 0.12f;
			CockpitGrab::PlayHaptic(GetWorld(), SprayHaptic.LoadSynchronous(), CockpitGrab::GetHandSide(GetHand()), 0.25f);
		}
	}
	else
	{
		Charge = FMath::Min(1.0f, Charge + RechargePerSecond * DeltaTime);
	}
	UpdatePuffs(DeltaTime);
}
