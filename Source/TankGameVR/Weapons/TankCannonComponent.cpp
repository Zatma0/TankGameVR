#include "Weapons/TankCannonComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankShellProjectile.h"
#include "Audio/TankAudio.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankGroundFire.h"
#include "Combat/TankHealthComponent.h"
#include "Targeting/TargetableComponent.h"
#include "Cockpit/CockpitGrabUtils.h"
#include "Targeting/TankTargetingComponent.h"
#include "TankGameVR.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UTankCannonComponent::UTankCannonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	ProjectileClass = ATankShellProjectile::StaticClass();
	HEGroundFireClass = ATankGroundFire::StaticClass();
	FireSound = TankAudio::Sound(TEXT("SW_Cannon_Fire"));
	InteriorFireSound = TankAudio::Sound(TEXT("SW_Cannon_Interior"));
	DryFireSound = TankAudio::Sound(TEXT("SW_Dry_Fire"));
	APImpactSound = TankAudio::Sound(TEXT("SW_Impact_AP"));
	HEImpactSound = TankAudio::Sound(TEXT("SW_Explosion_HE"));
	GroundImpactSound = TankAudio::Sound(TEXT("SW_Impact_Dirt"));
	FireHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/PistolFireHapticEffect.PistolFireHapticEffect")));
	DryFireHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/GrabHapticEffect.GrabHapticEffect")));
}

void UTankCannonComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	Breech = Owner->FindComponentByClass<UTankBreechComponent>();

	TArray<USceneComponent*> Components;
	Owner->GetComponents(Components);
	for (USceneComponent* Component : Components)
	{
		const FName Name = Component->GetFName();
		if (Name == BarrelComponentName)
		{
			Barrel = Component;
		}
		else if (Name == MuzzleFlashComponentName || Name == MuzzleLightComponentName)
		{
			FlashComponents.Add(Component);
		}
	}
	ShowMuzzleFlash(false);
}

FTransform UTankCannonComponent::GetMuzzleTransform() const
{
	const UTankTargetingComponent* Targeting = GetOwner()->FindComponentByClass<UTankTargetingComponent>();
	const USceneComponent* Pivot = Barrel ? Barrel.Get() : GetOwner()->GetRootComponent();
	const FVector Direction = Targeting ? Targeting->GetAimDirection() : Pivot->GetForwardVector();
	return FTransform(Direction.Rotation(), Pivot->GetComponentLocation() + Direction * MuzzleDistance);
}

bool UTankCannonComponent::Fire()
{
	const ETankRoundType Round = Breech ? Breech->GetLoadedRoundType() : ETankRoundType::AP;
	if (!Breech || !Breech->Fire())
	{
		UTankAudioSubsystem::PlayInside(this, DryFireSound.LoadSynchronous(), 0.8f);
		PlayBothHands(DryFireHaptic.LoadSynchronous(), 0.3f);
		return false;
	}

	const FTransform Muzzle = GetMuzzleTransform();
	ATankShellProjectile* Projectile = nullptr;
	if (ProjectileClass)
	{
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bDeferConstruction = true;
		Projectile = GetWorld()->SpawnActor<ATankShellProjectile>(ProjectileClass, Muzzle, Params);
		if (Projectile)
		{
			const bool bHE = Round == ETankRoundType::HE;
			Projectile->Damage = bHE ? HEDamage : Damage;
			Projectile->DamageType = bHE ? UTankDamageType_HE::StaticClass() : UTankDamageType_AP::StaticClass();
			Projectile->SplashDamage = bHE ? HESplashDamage : 0.0f;
			Projectile->SplashRadius = bHE ? HESplashRadius : 0.0f;
			// AP: a small sharp flash. HE: a fireball and burning ground.
			Projectile->ImpactFlashSize = bHE ? 560.0f : 200.0f;
			Projectile->ImpactFlashTime = bHE ? 0.6f : 0.3f;
			Projectile->GroundFireClass = bHE ? HEGroundFireClass : nullptr;
			Projectile->Tint = bHE ? HETracerColor : APTracerColor;
			Projectile->OnImpact.AddDynamic(this, &UTankCannonComponent::HandleShellImpact);
			Projectile->ImpactSound = bHE ? HEImpactSound : APImpactSound;
			Projectile->GroundImpactSound = bHE ? HEImpactSound : GroundImpactSound;
			Projectile->ImpactReach = bHE ? 35000.0f : 25000.0f;
			Projectile->Launch(Muzzle.GetRotation().GetForwardVector(), bHE ? HEMuzzleSpeed : MuzzleSpeed, GetWorld()->GetFirstPlayerController());
			Projectile->FinishSpawning(Muzzle);
			Projectile->SetTracerScale(bHE ? 1.9f : 1.0f);
			LastRound = Round;
			LastDamage = Projectile->Damage;
		}
	}
	++ShotsFired;

	// The blast out at the muzzle, and the thump and clank of it inside the hull. HE sits a little lower.
	const float ShotPitch = FireSoundPitch * (Round == ETankRoundType::HE ? 0.93f : 1.0f);
	UTankAudioSubsystem::PlayAt(this, FireSound.LoadSynchronous(), Muzzle.GetLocation(), 0.85f, ShotPitch, 40000.0f);
	UTankAudioSubsystem::PlayInside(this, InteriorFireSound.LoadSynchronous(), 0.75f, ShotPitch);
	PlayBothHands(FireHaptic.LoadSynchronous(), 1.0f);
	ShowMuzzleFlash(true);
	FlashRemaining = MuzzleFlashTime;

	UE_LOG(LogTankGame, Log, TEXT("%s fired shot %d"), *GetOwner()->GetName(), ShotsFired);
	OnFired.Broadcast(Projectile);
	return true;
}

float UTankCannonComponent::GetTimeSinceShotReport() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - ShotReportTime) : 1000.0f;
}

void UTankCannonComponent::HandleShellImpact(AActor* HitActor, FVector Location)
{
	const bool bHE = LastRound == ETankRoundType::HE;
	const TCHAR* Round = bHE ? TEXT("HE") : TEXT("AP");
	const UTankHealthComponent* Health = HitActor ? HitActor->FindComponentByClass<UTankHealthComponent>() : nullptr;
	if (!Health)
	{
		ShotReport = bHE ? TEXT("HE BURST - GROUND ON FIRE") : TEXT("AP MISSED");
	}
	else
	{
		const TSubclassOf<UDamageType> Type = bHE ? TSubclassOf<UDamageType>(UTankDamageType_HE::StaticClass()) : TSubclassOf<UDamageType>(UTankDamageType_AP::StaticClass());
		const float Effect = Health->GetDamageMultiplier(Type);
		const UTargetableComponent* Targetable = HitActor->FindComponentByClass<UTargetableComponent>();
		const FString Name = Targetable ? Targetable->GetTargetDisplayName().ToString().ToUpper() : FString(TEXT("TARGET"));
		const TCHAR* Result = Health->IsDead() ? TEXT("  DESTROYED") : (Effect < 0.95f ? TEXT("  WEAK - WRONG ROUND") : TEXT(""));
		ShotReport = FString::Printf(TEXT("%s HIT %s  -%d%s"), Round, *Name, FMath::RoundToInt(LastDamage * Effect), Result);
	}
	ShotReportTime = GetWorld()->GetTimeSeconds();
}

void UTankCannonComponent::ShowMuzzleFlash(bool bShow)
{
	for (USceneComponent* Component : FlashComponents)
	{
		if (Component)
		{
			Component->SetVisibility(bShow, true);
		}
	}
}

void UTankCannonComponent::PlayBothHands(UHapticFeedbackEffect_Base* Effect, float Scale) const
{
	CockpitGrab::PlayHaptic(GetWorld(), Effect, EControllerHand::Left, Scale);
	CockpitGrab::PlayHaptic(GetWorld(), Effect, EControllerHand::Right, Scale);
}

void UTankCannonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (FlashRemaining > 0.0f)
	{
		FlashRemaining -= DeltaTime;
		if (FlashRemaining <= 0.0f)
		{
			ShowMuzzleFlash(false);
		}
	}
}
