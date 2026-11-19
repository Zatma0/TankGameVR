#include "Weapons/TankShellProjectile.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

ATankShellProjectile::ATankShellProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	InitialLifeSpan = 8.0f;
	DamageType = UDamageType::StaticClass();

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	SetRootComponent(Collision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// 60 x 7 cm streak.
	Tracer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tracer"));
	Tracer->SetupAttachment(Collision);
	Tracer->SetStaticMesh(Sphere.Object);
	Tracer->SetRelativeLocation(FVector(-25.0, 0.0, 0.0));
	Tracer->SetRelativeScale3D(FVector(0.6, 0.07, 0.07));
	Tracer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tracer->SetCastShadow(false);

	ImpactFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ImpactFlash"));
	ImpactFlash->SetupAttachment(Collision);
	ImpactFlash->SetStaticMesh(Sphere.Object);
	ImpactFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ImpactFlash->SetCastShadow(false);
	ImpactFlash->SetHiddenInGame(true);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(40.0f);
	Light->SetAttenuationRadius(600.0f);
	Light->SetLightColor(FLinearColor(1.0f, 0.6f, 0.25f));
	Light->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 15000.0f;
	Movement->MaxSpeed = 15000.0f;
	Movement->ProjectileGravityScale = 0.15f;
	Movement->bRotationFollowsVelocity = true;
	// Launch() gives a world direction; the default would rotate it by the spawn rotation a second time.
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bShouldBounce = false;

	TracerMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Tank_Tracer.MI_Tank_Tracer")));
	ImpactSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/XRFramework/Audio/Fire_Cue.Fire_Cue")));
}

void ATankShellProjectile::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentHit.AddDynamic(this, &ATankShellProjectile::HandleHit);
	if (AActor* Shooter = GetOwner())
	{
		// Never hit our own tank or whoever rides in it.
		Collision->IgnoreActorWhenMoving(Shooter, true);
		TArray<AActor*> Riders;
		Shooter->GetAttachedActors(Riders, true, true);
		for (AActor* Rider : Riders)
		{
			Collision->IgnoreActorWhenMoving(Rider, true);
		}
	}
	if (FriendlyClass)
	{
		for (TActorIterator<AActor> It(GetWorld(), FriendlyClass); It; ++It)
		{
			Collision->IgnoreActorWhenMoving(*It, true);
		}
	}
	if (UMaterialInterface* Material = TracerMaterial.LoadSynchronous())
	{
		Tracer->SetMaterial(0, Material);
		ImpactFlash->SetMaterial(0, Material);
	}
}

void ATankShellProjectile::Launch(const FVector& Direction, float Speed, AController* InInstigatorController)
{
	InstigatorController = InInstigatorController;
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->Velocity = Direction.GetSafeNormal() * Speed;
}

void ATankShellProjectile::SetTracerScale(float Scale)
{
	Tracer->SetRelativeLocation(FVector(-25.0 * Scale, 0.0, 0.0));
	Tracer->SetRelativeScale3D(FVector(0.6 * Scale, 0.07 * Scale, 0.07 * Scale));
	Light->SetIntensity(40.0f * Scale);
}

void ATankShellProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHit)
	{
		return;
	}
	// A friendly vehicle driving into the round in flight (its own sweep reports the hit) doesn't count.
	if (OtherActor && (OtherActor == GetOwner() || (FriendlyClass && OtherActor->IsA(FriendlyClass)) || (GetOwner() && OtherActor->IsAttachedTo(GetOwner()))))
	{
		return;
	}
	bHit = true;
	const FVector Direction = Movement->Velocity.GetSafeNormal();
	if (OtherActor && OtherActor != GetOwner())
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, Direction, Hit, InstigatorController.Get(), this, DamageType);
	}
	if (SplashRadius > 0.0f && SplashDamage > 0.0f)
	{
		// The direct hit already counted, so the blast skips that actor.
		TArray<AActor*> Ignore = { this, OtherActor };
		if (AActor* Shooter = GetOwner())
		{
			Ignore.Add(Shooter);
			Shooter->GetAttachedActors(Ignore, false, true);
		}
		UGameplayStatics::ApplyRadialDamageWithFalloff(this, SplashDamage, 0.0f, Hit.ImpactPoint + Hit.ImpactNormal * 20.0,
			SplashRadius * 0.3f, SplashRadius, 1.0f, DamageType, Ignore, this, InstigatorController.Get(), ECC_Visibility);
	}
	if (USoundBase* Sound = ImpactSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Hit.ImpactPoint, 1.0f, ImpactSoundPitch);
	}
	OnImpact.Broadcast(OtherActor, Hit.ImpactPoint);

	// Stop and show the flash for a moment.
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorLocation(Hit.ImpactPoint);
	Tracer->SetHiddenInGame(true);
	ImpactFlash->SetHiddenInGame(false);
	FlashPeak = SplashRadius > 0.0f ? 900.0f : 400.0f;
	Light->SetIntensity(FlashPeak);
	Light->SetAttenuationRadius(SplashRadius > 0.0f ? 2500.0f : 1500.0f);
	FlashTime = 0.0f;
}

void ATankShellProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bHit)
	{
		return;
	}
	FlashTime += DeltaSeconds;
	const float T = FMath::Clamp(FlashTime / ImpactFlashTime, 0.0f, 1.0f);
	// Grows fast, then shrinks away.
	const float Size = ImpactFlashSize * FMath::Sin(T * PI) * (1.0f - 0.4f * T);
	ImpactFlash->SetWorldScale3D(FVector(FMath::Max(Size, 1.0f) / 100.0f));
	Light->SetIntensity(FlashPeak * (1.0f - T));
	if (T >= 1.0f)
	{
		Destroy();
	}
}
