#include "Weapons/TankCannonComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankShellProjectile.h"
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
	FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/XRFramework/Audio/Fire_Cue.Fire_Cue")));
	DryFireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02")));
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
	if (!Breech || !Breech->Fire())
	{
		if (USoundBase* Sound = DryFireSound.LoadSynchronous())
		{
			const FVector Location = Breech ? Breech->GetComponentLocation() : GetOwner()->GetActorLocation();
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, 0.25f, 2.5f);
		}
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
			Projectile->Damage = Damage;
			Projectile->Launch(Muzzle.GetRotation().GetForwardVector(), MuzzleSpeed, GetWorld()->GetFirstPlayerController());
			Projectile->FinishSpawning(Muzzle);
		}
	}
	++ShotsFired;

	if (USoundBase* Sound = FireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Muzzle.GetLocation(), 1.0f, FireSoundPitch);
		// Second, quieter copy inside the cockpit so the shot is felt in the seat.
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Breech->GetComponentLocation(), 0.6f, FireSoundPitch * 0.8f);
	}
	PlayBothHands(FireHaptic.LoadSynchronous(), 1.0f);
	ShowMuzzleFlash(true);
	FlashRemaining = MuzzleFlashTime;

	UE_LOG(LogTankGame, Log, TEXT("%s fired shot %d"), *GetOwner()->GetName(), ShotsFired);
	OnFired.Broadcast(Projectile);
	return true;
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
