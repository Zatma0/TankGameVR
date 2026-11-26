#include "Combat/TankGroundFire.h"
#include "Audio/TankAudio.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TArray<TWeakObjectPtr<ATankGroundFire>> GGroundFires;
}

ATankGroundFire::ATankGroundFire()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	FlameMesh = Cone.Object;
	SmokeMesh = Sphere.Object;
	FlameMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Fire_Flame.MI_Fire_Flame")));
	SmokeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Fire_Smoke.MI_Fire_Smoke")));
}

ATankGroundFire* ATankGroundFire::StartBelow(UWorld* World, TSubclassOf<ATankGroundFire> FireClass, const FVector& Point, AActor* FireOwner, AController* Instigator)
{
	if (!World || !FireClass)
	{
		return nullptr;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundFire), false);
	if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0, 0, 60), Point - FVector(0, 0, 450), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return nullptr;
	}
	// A second round into the same spot keeps that fire going instead of stacking another one on it.
	for (const TWeakObjectPtr<ATankGroundFire>& Entry : GGroundFires)
	{
		ATankGroundFire* Fire = Entry.Get();
		if (Fire && Fire->GetWorld() == World && FVector::Dist(Fire->GetActorLocation(), Hit.ImpactPoint) < Fire->Radius * 0.5f)
		{
			Fire->Age = FMath::Min(Fire->Age, 0.5f);
			Fire->SetLifeSpan(Fire->Duration - Fire->Age + 0.2f);
			return Fire;
		}
	}
	FActorSpawnParameters Spawn;
	Spawn.Owner = FireOwner;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATankGroundFire* Fire = World->SpawnActor<ATankGroundFire>(FireClass, FTransform(Hit.ImpactPoint), Spawn);
	if (Fire)
	{
		Fire->FireInstigator = Instigator;
	}
	return Fire;
}

UStaticMeshComponent* ATankGroundFire::MakeMesh(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetupAttachment(GetRootComponent());
	Component->RegisterComponent();
	Component->SetVisibility(false);
	return Component;
}

void ATankGroundFire::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(Duration + 0.2f);
	GGroundFires.Add(this);

	UMaterialInterface* Flame = FlameMaterial.LoadSynchronous();
	UMaterialInterface* SmokeMat = SmokeMaterial.LoadSynchronous();
	const FVector Center = GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundFireFlame), false);
	for (int32 Index = 0; Index < FlameCount; ++Index)
	{
		FFlame Shape;
		// Evenly over the disc, the first one in the middle.
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Distance = Index == 0 ? 0.0f : Radius * 0.9f * FMath::Sqrt(FMath::FRand());
		Shape.Offset = FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0);
		// Stand each flame on the ground, the patch may be on a slope.
		FHitResult Hit;
		const FVector Spot = Center + Shape.Offset;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Spot + FVector(0, 0, 200), Spot - FVector(0, 0, 300), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Shape.Offset.Z = Hit.ImpactPoint.Z - Center.Z;
		}
		Shape.Height = FMath::FRandRange(70.0f, 160.0f) * (Index == 0 ? 1.3f : 1.0f);
		Shape.Width = FMath::FRandRange(35.0f, 60.0f);
		Shape.Phase = FMath::FRand();
		FlameShapes.Add(Shape);
		Flames.Add(MakeMesh(FlameMesh, Flame));
	}
	for (int32 Index = 0; Index < SmokePuffs; ++Index)
	{
		Smoke.Add(MakeMesh(SmokeMesh, SmokeMat));
	}

	Glow = NewObject<UPointLightComponent>(this);
	Glow->SetupAttachment(GetRootComponent());
	Glow->SetRelativeLocation(FVector(0, 0, 90));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(0.0f);
	Glow->SetAttenuationRadius(Radius * 4.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.45f, 0.12f));
	Glow->SetCastShadows(false);
	Glow->RegisterComponent();
	Crackle = UTankAudioSubsystem::MakeLoop(GetRootComponent(), TankAudio::Sound(TEXT("SW_Fire_Loop")).LoadSynchronous(), false, 4500.0f);
	UpdateVisuals();
}

void ATankGroundFire::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GGroundFires.RemoveAll([this](const TWeakObjectPtr<ATankGroundFire>& Fire) { return !Fire.IsValid() || Fire.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

float ATankGroundFire::GetStrength() const
{
	// Flares up in half a second, dies down at the end.
	return FMath::Clamp(Age / 0.5f, 0.0f, 1.0f) * FMath::Clamp((Duration - Age) / FadeTime, 0.0f, 1.0f);
}

bool ATankGroundFire::IsInside(const FVector& Location) const
{
	const FVector Center = GetActorLocation();
	return GetStrength() > 0.1f && FVector::Dist2D(Location, Center) <= Radius && FMath::Abs(Location.Z - Center.Z) < 300.0;
}

bool ATankGroundFire::IsBurningAt(const UWorld* World, const FVector& Location)
{
	for (const TWeakObjectPtr<ATankGroundFire>& Fire : GGroundFires)
	{
		if (Fire.IsValid() && Fire->GetWorld() == World && Fire->IsInside(Location))
		{
			return true;
		}
	}
	return false;
}

void ATankGroundFire::Burn(float DeltaSeconds)
{
	DamageTimer += DeltaSeconds;
	if (DamageTimer < 0.5f)
	{
		return;
	}
	const float Amount = DamagePerSecond * DamageTimer * GetStrength();
	DamageTimer = 0.0f;
	if (Amount <= 0.0f)
	{
		return;
	}
	// Every tank whose hull reaches into the patch.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GroundFireBurn), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation() + FVector(0, 0, 80), FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);
	TArray<AActor*, TInlineAllocator<8>> Burned;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Burned.Contains(Actor))
		{
			continue;
		}
		Burned.Add(Actor);
		const UTankHealthComponent* Health = Actor->FindComponentByClass<UTankHealthComponent>();
		if (Health && !Health->IsDead())
		{
			UGameplayStatics::ApplyDamage(Actor, Amount, FireInstigator.Get(), this, UTankDamageType_Fire::StaticClass());
		}
	}
}

void ATankGroundFire::UpdateVisuals()
{
	const float Strength = GetStrength();
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		UStaticMeshComponent* Flame = Flames[Index];
		const FFlame& Shape = FlameShapes[Index];
		// The edge flames go out first as it dies down.
		const bool bShow = Strength > 0.02f && Index < FMath::CeilToInt(Strength * Flames.Num());
		Flame->SetVisibility(bShow);
		if (!bShow)
		{
			continue;
		}
		const float Flicker = 0.8f + 0.2f * FMath::Sin(Time * (9.0f + Shape.Phase * 6.0f) + Shape.Phase * 6.28f) + FMath::FRandRange(-0.08f, 0.08f);
		const float Height = Shape.Height * (0.4f + 0.6f * Strength) * Flicker;
		const float Width = Shape.Width * (0.7f + 0.3f * Strength);
		// Cone pivot is in its middle, 100 cm tall.
		Flame->SetRelativeLocation(Shape.Offset + FVector(0, 0, Height * 0.5f));
		Flame->SetRelativeScale3D(FVector(Width / 100.0f, Width / 100.0f, Height / 100.0f));
	}
	for (int32 Index = 0; Index < Smoke.Num(); ++Index)
	{
		// Dark puffs rising one after another.
		const float T = FMath::Frac(Time * 0.3f + static_cast<float>(Index) / Smoke.Num());
		const float Angle = Index * 2.4f;
		const float Size = (90.0f + 260.0f * T) * (1.0f - T * T) * Strength;
		Smoke[Index]->SetVisibility(Size > 5.0f);
		Smoke[Index]->SetRelativeLocation(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Radius * 0.35f + FVector(T * 120.0f, 0.0, 120.0f + 650.0f * T));
		Smoke[Index]->SetRelativeScale3D(FVector(FMath::Max(Size, 1.0f) / 100.0f));
	}
	if (Glow)
	{
		Glow->SetIntensity(60.0f * Strength * FMath::FRandRange(0.8f, 1.1f));
	}
	UTankAudioSubsystem::SetLoop(Crackle, 0.9f * Strength, 0.95f);
}

void ATankGroundFire::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Burn(DeltaSeconds);
	UpdateVisuals();
}
