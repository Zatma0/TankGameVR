#include "Cockpit/CockpitFireComponent.h"
#include "Combat/TankDamageTypes.h"
#include "TankGameVR.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UCockpitFireComponent::UCockpitFireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	FlameMesh = Cone.Object;
	SmokeMesh = Sphere.Object;
	FlameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Fire_Flame.MI_Fire_Flame")));
	SmokeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Fire_Smoke.MI_Fire_Smoke")));
}

void UCockpitFireComponent::BeginPlay()
{
	Super::BeginPlay();
	UMaterialInterface* Flame = FlameMaterial.LoadSynchronous();
	UMaterialInterface* SmokeMat = SmokeMaterial.LoadSynchronous();
	auto MakeMesh = [this](UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(GetOwner());
		Component->SetStaticMesh(Mesh);
		if (Material)
		{
			Component->SetMaterial(0, Material);
		}
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetupAttachment(this);
		Component->RegisterComponent();
		Component->SetVisibility(false);
		return Component;
	};
	for (int32 Index = 0; Index < FlameCount; ++Index)
	{
		Flames.Add(MakeMesh(FlameMesh, Flame));
		const float Angle = Index * 2.0f * PI / FlameCount + FMath::FRandRange(-0.3f, 0.3f);
		const float Distance = Index == 0 ? 0.0f : Radius * FMath::FRandRange(0.35f, 0.9f);
		FlameOffsets.Add(FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0));
	}
	for (int32 Index = 0; Index < SmokePuffs; ++Index)
	{
		Smoke.Add(MakeMesh(SmokeMesh, SmokeMat));
	}
	Puffs.SetNum(SmokePuffs);

	Glow = NewObject<UPointLightComponent>(GetOwner());
	Glow->SetupAttachment(this);
	Glow->SetRelativeLocation(FVector(0, 0, 15));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(180.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
	Glow->SetCastShadows(false);
	Glow->RegisterComponent();
	Glow->SetVisibility(false);
}

void UCockpitFireComponent::Ignite(float Amount)
{
	const bool bWasBurning = IsBurning();
	Intensity = FMath::Clamp(Intensity + Amount, 0.0f, 1.0f);
	if (!bWasBurning && IsBurning())
	{
		BigTime = 0.0f;
		UE_LOG(LogTankGame, Log, TEXT("Fire at %s"), *GetName());
	}
}

void UCockpitFireComponent::Extinguish(float Amount)
{
	if (!IsBurning())
	{
		return;
	}
	Intensity = FMath::Max(0.0f, Intensity - Amount);
	if (Intensity < 0.03f)
	{
		Intensity = 0.0f;
		BigTime = 0.0f;
		UE_LOG(LogTankGame, Log, TEXT("Fire at %s is out"), *GetName());
	}
}

void UCockpitFireComponent::SpreadToNeighbour()
{
	TArray<UCockpitFireComponent*> Spots;
	GetOwner()->GetComponents(Spots);
	UCockpitFireComponent* Nearest = nullptr;
	double Best = TNumericLimits<double>::Max();
	for (UCockpitFireComponent* Spot : Spots)
	{
		if (Spot != this && !Spot->IsBurning())
		{
			const double Distance = FVector::DistSquared(Spot->GetComponentLocation(), GetComponentLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = Spot;
			}
		}
	}
	if (Nearest)
	{
		Nearest->Ignite(0.25f);
	}
}

void UCockpitFireComponent::UpdateVisuals(float DeltaTime)
{
	const bool bBurning = IsBurning();
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		UStaticMeshComponent* Flame = Flames[Index];
		// Small fires only show the middle flames.
		const bool bShow = bBurning && Index < FMath::CeilToInt(Intensity * Flames.Num());
		Flame->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		const float Flicker = 0.75f + 0.25f * FMath::Sin(Time * (11.0f + Index * 3.1f)) + FMath::FRandRange(-0.12f, 0.12f);
		const float Height = (10.0f + 22.0f * Intensity) * Flicker * (Index == 0 ? 1.3f : 1.0f);
		const float Width = (5.0f + 6.0f * Intensity) * (0.9f + 0.2f * Flicker);
		// Cone pivot is in its middle, 100 cm tall.
		Flame->SetRelativeLocation(FlameOffsets[Index] * (0.6f + 0.4f * Intensity) + FVector(0, 0, Height * 0.5f));
		Flame->SetRelativeScale3D(FVector(Width / 100.0f, Width / 100.0f, Height / 100.0f));
	}
	if (Glow)
	{
		Glow->SetVisibility(bBurning);
		Glow->SetIntensity(bBurning ? LightIntensity * Intensity * FMath::FRandRange(0.7f, 1.1f) : 0.0f);
	}

	// Dark smoke rising to the roof.
	PuffTimer -= DeltaTime;
	if (bBurning && PuffTimer <= 0.0f)
	{
		PuffTimer = FMath::Lerp(0.6f, 0.2f, Intensity);
		for (FSmokePuff& Puff : Puffs)
		{
			if (Puff.Age < 0.0f)
			{
				Puff.Age = 0.0f;
				Puff.Location = FVector(FMath::FRandRange(-Radius, Radius) * 0.5f, FMath::FRandRange(-Radius, Radius) * 0.5f, 20.0f);
				break;
			}
		}
	}
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		FSmokePuff& Puff = Puffs[Index];
		UStaticMeshComponent* Mesh = Smoke[Index];
		if (Puff.Age < 0.0f)
		{
			Mesh->SetVisibility(false);
			continue;
		}
		Puff.Age += DeltaTime;
		const float T = Puff.Age / 2.2f;
		if (T >= 1.0f)
		{
			Puff.Age = -1.0f;
			Mesh->SetVisibility(false);
			continue;
		}
		Puff.Location.Z += 30.0f * DeltaTime;
		const float Size = (8.0f + 20.0f * T) * (1.0f - T * T) * (0.5f + 0.5f * Intensity);
		Mesh->SetRelativeLocation(Puff.Location);
		Mesh->SetRelativeScale3D(FVector(FMath::Max(Size, 0.1f) / 100.0f));
		Mesh->SetVisibility(true);
	}
}

void UCockpitFireComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (IsBurning())
	{
		Intensity = FMath::Min(1.0f, Intensity + GrowthRate * DeltaTime);
		PendingDamage += BurnDamagePerSecond * Intensity * DeltaTime;
		if (PendingDamage >= 1.0f)
		{
			UGameplayStatics::ApplyDamage(GetOwner(), PendingDamage, nullptr, GetOwner(), UTankDamageType_Fire::StaticClass());
			PendingDamage = 0.0f;
		}
		BigTime = Intensity > 0.8f ? BigTime + DeltaTime : 0.0f;
		if (SpreadTime > 0.0f && BigTime >= SpreadTime)
		{
			BigTime = 0.0f;
			SpreadToNeighbour();
		}
	}
	UpdateVisuals(DeltaTime);
}
