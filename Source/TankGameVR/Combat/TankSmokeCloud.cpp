#include "Combat/TankSmokeCloud.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TArray<TWeakObjectPtr<ATankSmokeCloud>> GClouds;
}

ATankSmokeCloud::ATankSmokeCloud()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	PuffMesh = Sphere.Object;
	SmokeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Smoke_Screen.MI_Smoke_Screen")));
}

void ATankSmokeCloud::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Lifetime);
	GClouds.Add(this);
	if (LaunchFrom.IsNearlyZero())
	{
		LaunchFrom = GetActorLocation();
	}

	UMaterialInterface* Material = SmokeMaterial.LoadSynchronous();
	const FTransform Frame = GetActorTransform();
	Puffs.SetNum(PuffCount);
	for (int32 Index = 0; Index < PuffCount; ++Index)
	{
		// A wall across the line of fire, a little uneven, with a second row behind the middle.
		const float Alpha = PuffCount > 1 ? static_cast<float>(Index) / (PuffCount - 1) : 0.5f;
		const bool bBackRow = Index % 3 == 1;
		const FVector Local(bBackRow ? 250.0f : FMath::FRandRange(-80.0f, 80.0f), (Alpha - 0.5f) * Width + FMath::FRandRange(-120.0f, 120.0f), PuffRadius * 0.55f);
		FPuff& Puff = Puffs[Index];
		Puff.Target = Frame.TransformPositionNoScale(Local);
		Puff.Drift = FVector(FMath::FRandRange(-15.0f, 15.0f), FMath::FRandRange(-15.0f, 15.0f), FMath::FRandRange(8.0f, 18.0f));
		Puff.Delay = FMath::FRandRange(0.0f, 0.25f);
		Puff.Scale = FMath::FRandRange(0.8f, 1.15f);

		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this);
		Mesh->SetStaticMesh(PuffMesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->RegisterComponent();
		if (Material)
		{
			UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(0, Material);
			PuffMaterials.Add(Dynamic);
		}
		Mesh->SetVisibility(false);
		PuffMeshes.Add(Mesh);
	}
}

void ATankSmokeCloud::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GClouds.RemoveAll([this](const TWeakObjectPtr<ATankSmokeCloud>& Cloud) { return !Cloud.IsValid() || Cloud.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

float ATankSmokeCloud::GetDensity() const
{
	const float Bloom = FMath::Clamp((Age - FlightTime) / BloomTime, 0.0f, 1.0f);
	const float Fade = FMath::Clamp((Lifetime - Age) / FadeTime, 0.0f, 1.0f);
	return Bloom * Fade;
}

FVector ATankSmokeCloud::GetPuffLocation(int32 Index) const
{
	const FPuff& Puff = Puffs[Index];
	const float T = Age - Puff.Delay;
	if (T < FlightTime)
	{
		// Grenade arc out of the launcher.
		const float A = FMath::Clamp(T / FlightTime, 0.0f, 1.0f);
		FVector Location = FMath::Lerp(LaunchFrom, Puff.Target, A);
		Location.Z += FMath::Sin(A * PI) * 500.0f;
		return Location;
	}
	return Puff.Target + Puff.Drift * (T - FlightTime);
}

float ATankSmokeCloud::GetPuffRadius(int32 Index) const
{
	const FPuff& Puff = Puffs[Index];
	const float T = Age - Puff.Delay - FlightTime;
	if (T < 0.0f)
	{
		return 12.0f;
	}
	// Fast bloom, then keeps spreading slowly.
	const float Bloom = 1.0f - FMath::Pow(1.0f - FMath::Clamp(T / BloomTime, 0.0f, 1.0f), 3.0f);
	return PuffRadius * Puff.Scale * (0.15f + 0.85f * Bloom) * (1.0f + T * 0.02f);
}

bool ATankSmokeCloud::BlocksLine(const FVector& A, const FVector& B) const
{
	if (GetDensity() < 0.35f)
	{
		return false;
	}
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		// Only the thick core blocks a gunner's view.
		const float Radius = GetPuffRadius(Index) * 0.8f;
		if (FMath::PointDistToSegment(GetPuffLocation(Index), A, B) < Radius)
		{
			return true;
		}
	}
	return false;
}

bool ATankSmokeCloud::IsLineBlocked(const UWorld* World, const FVector& A, const FVector& B)
{
	for (const TWeakObjectPtr<ATankSmokeCloud>& Cloud : GClouds)
	{
		if (Cloud.IsValid() && Cloud->GetWorld() == World && Cloud->BlocksLine(A, B))
		{
			return true;
		}
	}
	return false;
}

void ATankSmokeCloud::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Fade = FMath::Clamp((Lifetime - Age) / FadeTime, 0.0f, 1.0f);
	for (int32 Index = 0; Index < PuffMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Mesh = PuffMeshes[Index];
		const bool bVisible = Age >= Puffs[Index].Delay;
		Mesh->SetVisibility(bVisible);
		if (!bVisible)
		{
			continue;
		}
		Mesh->SetWorldLocation(GetPuffLocation(Index));
		Mesh->SetWorldScale3D(FVector(GetPuffRadius(Index) / 50.0f));
		if (PuffMaterials.IsValidIndex(Index) && PuffMaterials[Index])
		{
			PuffMaterials[Index]->SetScalarParameterValue(TEXT("Opacity"), 0.85f * Fade);
		}
	}
}
