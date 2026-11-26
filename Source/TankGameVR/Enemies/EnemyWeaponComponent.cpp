#include "Enemies/EnemyWeaponComponent.h"
#include "Audio/TankAudio.h"
#include "Combat/TankSmokeCloud.h"
#include "Weapons/TankShellProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

UEnemyWeaponComponent::UEnemyWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	ProjectileClass = ATankShellProjectile::StaticClass();
	TracerMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Enemy_Tracer.MI_Enemy_Tracer")));
}

void UEnemyWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	TArray<USceneComponent*> Components;
	GetOwner()->GetComponents(Components);
	for (USceneComponent* Component : Components)
	{
		const FName Name = Component->GetFName();
		if (Name == TurretComponentName)
		{
			Turret = Component;
		}
		else if (Name == BarrelComponentName)
		{
			Barrel = Component;
		}
		else if (Name == MuzzleComponentName)
		{
			Muzzle = Component;
		}
		else if (Name == FlashComponentName)
		{
			Flash = Component;
		}
		else if (Name == LaserComponentName)
		{
			Laser = Component;
		}
	}
	if (Flash)
	{
		Flash->SetVisibility(false, true);
	}
	if (Laser)
	{
		LaserScale = Laser->GetRelativeScale3D();
		Laser->SetVisibility(false, true);
	}
	OwnerLastLocation = GetOwner()->GetActorLocation();
	// Don't open fire the instant it spawns.
	ReloadRemaining = FMath::FRandRange(0.5f, 1.5f);
}

void UEnemyWeaponComponent::SetTarget(AActor* InTarget)
{
	if (Target.Get() != InTarget)
	{
		Target = InTarget;
		AimHold = 0.0f;
		TrackTime = 0.0f;
		if (InTarget)
		{
			TargetLastLocation = InTarget->GetActorLocation();
			TargetVelocity = FVector::ZeroVector;
		}
	}
}

FVector UEnemyWeaponComponent::GetGunDirection() const
{
	if (Barrel)
	{
		return Barrel->GetForwardVector();
	}
	return Turret ? Turret->GetForwardVector() : GetOwner()->GetActorForwardVector();
}

FVector UEnemyWeaponComponent::GetMuzzleLocation() const
{
	if (Muzzle)
	{
		return Muzzle->GetComponentLocation();
	}
	const USceneComponent* Pivot = Barrel ? Barrel.Get() : GetOwner()->GetRootComponent();
	return Pivot->GetComponentLocation() + GetGunDirection() * 300.0;
}

float UEnemyWeaponComponent::GetCurrentSpread() const
{
	const float Settle = FMath::Clamp(TrackTime / SettleTime, 0.0f, 1.0f);
	float Spread = FMath::Lerp(SpreadStart, SpreadMin, Settle);
	if (OwnerSpeed > 80.0f)
	{
		Spread *= MovingSpreadMultiplier;
	}
	return Spread;
}

void UEnemyWeaponComponent::UpdateTargetMotion(float DeltaTime)
{
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	OwnerSpeed = static_cast<float>(FVector::Dist2D(OwnerLocation, OwnerLastLocation) / DeltaTime);
	OwnerLastLocation = OwnerLocation;

	const AActor* Goal = Target.Get();
	if (!Goal)
	{
		return;
	}
	const FVector Location = Goal->GetActorLocation();
	const FVector Measured = (Location - TargetLastLocation) / DeltaTime;
	TargetLastLocation = Location;
	// Smoothed, like a gunner estimating the target's speed.
	TargetVelocity = FMath::Lerp(TargetVelocity, Measured, FMath::Clamp(DeltaTime * 2.0f, 0.0f, 1.0f));

	// Aim at the middle of the hull, a bit high.
	FVector Origin, Extent;
	Goal->GetActorBounds(true, Origin, Extent);
	AimPoint = FVector(Location.X, Location.Y, Location.Z + FMath::Min(Extent.Z * 0.3, 60.0));
	if (bLeadTarget)
	{
		const float Flight = static_cast<float>(FVector::Dist(GetMuzzleLocation(), AimPoint) / FMath::Max(ProjectileSpeed, 1.0f));
		AimPoint += TargetVelocity * FVector(1, 1, 0) * Flight;
	}
}

void UEnemyWeaponComponent::UpdateTurret(float DeltaTime)
{
	const AActor* Owner = GetOwner();
	const FRotator Hull = Owner->GetActorRotation();
	float DesiredYaw = TurretYaw;
	float DesiredPitch = 0.0f;
	if (Target.IsValid())
	{
		const USceneComponent* Pivot = Barrel ? Barrel.Get() : Owner->GetRootComponent();
		const FVector Local = Owner->GetActorTransform().InverseTransformVectorNoScale(AimPoint - Pivot->GetComponentLocation());
		const FRotator LocalAim = Local.Rotation();
		DesiredYaw = FMath::Clamp(LocalAim.Yaw, -TraverseLimit, TraverseLimit);
		DesiredPitch = FMath::Clamp(LocalAim.Pitch, PitchLimits.X, PitchLimits.Y);
	}
	TurretYaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(TurretYaw, DesiredYaw), -TraverseSpeed * DeltaTime, TraverseSpeed * DeltaTime);
	if (TraverseLimit < 180.0f)
	{
		TurretYaw = FMath::Clamp(TurretYaw, -TraverseLimit, TraverseLimit);
	}
	GunPitch = FMath::FInterpConstantTo(GunPitch, DesiredPitch, DeltaTime, ElevationSpeed);

	if (Turret)
	{
		Turret->SetRelativeRotation(FRotator(0.0f, TurretYaw, 0.0f));
	}
	if (Barrel)
	{
		if (Barrel->GetAttachParent() == Turret.Get() && Turret)
		{
			Barrel->SetRelativeRotation(FRotator(GunPitch, 0.0f, 0.0f));
		}
		else
		{
			Barrel->SetRelativeRotation(FRotator(GunPitch, TurretYaw, 0.0f));
		}
	}

	if (Target.IsValid())
	{
		const FVector Wanted = (AimPoint - GetMuzzleLocation()).GetSafeNormal();
		const float Error = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Wanted, GetGunDirection()), -1.0, 1.0)));
		bOnTarget = Error <= OnTargetTolerance;
	}
	else
	{
		bOnTarget = false;
	}
}

bool UEnemyWeaponComponent::CheckLineOfSight() const
{
	const AActor* Goal = Target.Get();
	if (!Goal)
	{
		return false;
	}
	// The gunner looks through the sight at the breech, not from the muzzle (which can poke into a rock).
	const FVector Start = Barrel ? Barrel->GetComponentLocation() + FVector(0, 0, 15) : GetMuzzleLocation();
	const FVector End = AimPoint;
	if (ATankSmokeCloud::IsLineBlocked(GetWorld(), Start, End))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight), false, GetOwner());
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return true;
	}
	const AActor* HitActor = Hit.GetActor();
	return HitActor == Goal || (HitActor && HitActor->GetAttachParentActor() == Goal);
}

bool UEnemyWeaponComponent::FireNow()
{
	if (ReloadRemaining > 0.0f || BurstLeft > 0)
	{
		return false;
	}
	BurstLeft = BurstCount;
	BurstRemainingTime = 0.0f;
	FireRound();
	return true;
}

void UEnemyWeaponComponent::FireRound()
{
	--BurstLeft;
	BurstRemainingTime = BurstInterval;
	if (BurstLeft <= 0)
	{
		ReloadRemaining = ReloadTime;
		AimHold = 0.0f;
	}
	++ShotsFired;

	const FVector Start = GetMuzzleLocation();
	const float Spread = FMath::DegreesToRadians(GetCurrentSpread());
	const FVector Direction = FMath::VRandCone(GetGunDirection(), Spread * 0.5f, Spread * 0.35f);
	if (ProjectileClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		const FTransform Transform(Direction.Rotation(), Start);
		if (ATankShellProjectile* Projectile = GetWorld()->SpawnActor<ATankShellProjectile>(ProjectileClass, Transform, Params))
		{
			Projectile->Damage = Damage;
			if (DamageType)
			{
				Projectile->DamageType = DamageType;
			}
			if (!TracerMaterial.IsNull())
			{
				Projectile->TracerMaterial = TracerMaterial;
			}
			Projectile->ImpactFlashSize = 220.0f * FMath::Sqrt(TracerScale);
			Projectile->SetTracerScale(TracerScale);
			Projectile->SetFriendlyClass(FriendlyClass ? FriendlyClass.Get() : GetOwner()->GetClass());
			Projectile->Launch(Direction, ProjectileSpeed, nullptr);
			Projectile->FinishSpawning(Transform);
			OnFired.Broadcast(Projectile);
		}
	}
	UTankAudioSubsystem::PlayAt(this, FireSound.LoadSynchronous(), Start, 1.0f, FireSoundPitch * FMath::FRandRange(0.96f, 1.04f), 35000.0f);
	if (Flash)
	{
		Flash->SetVisibility(true, true);
		FlashRemaining = FMath::Min(0.07f, BurstInterval * 0.6f);
	}
}

void UEnemyWeaponComponent::UpdateLaser()
{
	if (!Laser)
	{
		return;
	}
	const float Progress = GetAimProgress();
	const bool bAiming = bWeaponsFree && bLineOfSight && Target.IsValid() && Progress > 0.0f && ReloadRemaining <= 0.0f;
	bool bShow = bAiming;
	if (bAiming && Progress < 0.8f)
	{
		// Blinks faster and faster, solid just before the shot.
		const float Rate = FMath::Lerp(2.0f, 9.0f, Progress);
		bShow = FMath::Frac(GetWorld()->GetTimeSeconds() * Rate) < 0.6f;
	}
	Laser->SetVisibility(bShow, true);
	if (bShow)
	{
		const double Length = FVector::Dist(Laser->GetComponentLocation(), AimPoint);
		Laser->SetRelativeScale3D(FVector(Length / 100.0, LaserScale.Y, LaserScale.Z));
	}
}

void UEnemyWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	UpdateTargetMotion(DeltaTime);
	UpdateTurret(DeltaTime);

	// Sight checks a few times a second are plenty.
	LosTimer -= DeltaTime;
	if (LosTimer <= 0.0f)
	{
		LosTimer = 0.15f;
		const bool bWasVisible = bLineOfSight;
		bLineOfSight = CheckLineOfSight();
		if (!bLineOfSight && bWasVisible && bResetAimWhenSightLost)
		{
			AimHold = 0.0f;
		}
	}

	if (bOnTarget && bLineOfSight)
	{
		TrackTime += DeltaTime;
		// The lay (and the warning laser) only counts once it is allowed to shoot.
		AimHold = bWeaponsFree ? AimHold + DeltaTime : 0.0f;
	}
	else
	{
		TrackTime = FMath::Max(0.0f, TrackTime - DeltaTime * 2.0f);
		AimHold = bResetAimWhenSightLost && !bLineOfSight ? 0.0f : FMath::Max(0.0f, AimHold - DeltaTime * 2.0f);
	}

	if (FlashRemaining > 0.0f)
	{
		FlashRemaining -= DeltaTime;
		if (FlashRemaining <= 0.0f && Flash)
		{
			Flash->SetVisibility(false, true);
		}
	}

	if (BurstLeft > 0)
	{
		BurstRemainingTime -= DeltaTime;
		if (BurstRemainingTime <= 0.0f && bLineOfSight)
		{
			FireRound();
		}
		else if (!bLineOfSight || !bWeaponsFree)
		{
			// Lost the target mid burst: stop and reload.
			BurstLeft = 0;
			ReloadRemaining = ReloadTime * 0.5f;
		}
	}
	else if (ReloadRemaining > 0.0f)
	{
		ReloadRemaining -= DeltaTime;
	}
	else if (bWeaponsFree && bLineOfSight && AimHold >= AimTime && bOnTarget)
	{
		BurstLeft = BurstCount;
		FireRound();
	}
	UpdateLaser();
}
