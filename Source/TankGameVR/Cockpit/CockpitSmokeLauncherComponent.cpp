#include "Cockpit/CockpitSmokeLauncherComponent.h"
#include "Cockpit/CockpitGrabUtils.h"
#include "Cockpit/TankSeatComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Targeting/TankTargetingComponent.h"
#include "TankGameVR.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MotionControllerComponent.h"
#include "Sound/SoundBase.h"

UCockpitSmokeLauncherComponent::UCockpitSmokeLauncherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetMobility(EComponentMobility::Movable);
	CloudClass = ATankSmokeCloud::StaticClass();
	ClickHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/GrabHapticEffect.GrabHapticEffect")));
}

void UCockpitSmokeLauncherComponent::BeginPlay()
{
	Super::BeginPlay();
	Charges = MaxCharges;
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child->GetFName() == CoverHingeName)
		{
			Hinge = Child;
		}
		else if (Child->GetFName() == ButtonName)
		{
			Button = Child;
		}
		else if (Child->GetFName() == LampName)
		{
			if (UPrimitiveComponent* Lamp = Cast<UPrimitiveComponent>(Child); Lamp && Lamp->GetNumMaterials() > 0)
			{
				LampMaterial = Lamp->CreateDynamicMaterialInstance(0);
			}
		}
	}
	if (Hinge)
	{
		HingeRest = Hinge->GetRelativeRotation();
		CoverGrab = CockpitGrab::FindGrabComponent(Hinge, NAME_None);
	}
	if (Button)
	{
		ButtonRest = Button->GetRelativeLocation();
	}
	UpdateLamp();
}

float UCockpitSmokeLauncherComponent::GetReloadProgress() const
{
	return (Charges >= MaxCharges || ReloadTime <= 0.0f) ? 1.0f : 1.0f - ReloadRemaining / ReloadTime;
}

FString UCockpitSmokeLauncherComponent::GetStatusText() const
{
	if (Charges > 0)
	{
		return FString::Printf(TEXT("SMOKE x%d"), Charges);
	}
	return FString::Printf(TEXT("SMOKE RELOADING %d%%"), FMath::RoundToInt(GetReloadProgress() * 100.0f));
}

void UCockpitSmokeLauncherComponent::SetCoverOpen(bool bOpen)
{
	CoverAngle = bOpen ? CoverOpenAngle : 0.0f;
	CoverHand = nullptr;
}

bool UCockpitSmokeLauncherComponent::Launch()
{
	if (Charges <= 0 || !CloudClass)
	{
		return false;
	}
	AActor* Tank = GetOwner();
	const UTankTargetingComponent* Targeting = Tank->FindComponentByClass<UTankTargetingComponent>();
	// The launchers sit on the turret, so the screen goes where the gun points.
	const FVector Aim = (Targeting ? Targeting->GetAimDirection() : Tank->GetActorForwardVector()).GetSafeNormal2D();
	const FVector From = (Targeting ? Targeting->GetAimOrigin() : Tank->GetActorLocation()) + FVector(0, 0, 60);
	FVector Center = Tank->GetActorLocation() + Aim * ScreenDistance;

	// Stand it on the ground.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SmokeGround), false, Tank);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Center + FVector(0, 0, 1000), Center - FVector(0, 0, 3000), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Center.Z = Hit.ImpactPoint.Z;
	}

	FActorSpawnParameters Spawn;
	Spawn.Owner = Tank;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Spawn.bDeferConstruction = true;
	const FTransform Transform(Aim.Rotation(), Center);
	ATankSmokeCloud* Cloud = GetWorld()->SpawnActor<ATankSmokeCloud>(CloudClass, Transform, Spawn);
	if (!Cloud)
	{
		return false;
	}
	Cloud->LaunchFrom = From;
	Cloud->FinishSpawning(Transform);

	--Charges;
	if (ReloadRemaining <= 0.0f)
	{
		ReloadRemaining = ReloadTime;
	}
	if (USoundBase* Sound = LaunchSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, From);
	}
	UE_LOG(LogTankGame, Log, TEXT("Smoke launched, %d left"), Charges);
	OnLaunched.Broadcast(Cloud);
	return true;
}

void UCockpitSmokeLauncherComponent::GetFingertips(TArray<FVector>& Out) const
{
	if (const USceneComponent* Finger = FingerOverride.Get())
	{
		Out.Add(Finger->GetComponentLocation());
		return;
	}
	const UTankSeatComponent* Seat = GetOwner()->FindComponentByClass<UTankSeatComponent>();
	const APawn* Rider = Seat ? Seat->GetRider() : nullptr;
	if (!Rider)
	{
		return;
	}
	TArray<UMotionControllerComponent*> Controllers;
	Rider->GetComponents(Controllers);
	for (const UMotionControllerComponent* Controller : Controllers)
	{
		if (Controller->IsTracked() || Controller->GetName().Contains(TEXT("Grip")))
		{
			Out.Add(Controller->GetComponentLocation() + Controller->GetForwardVector() * FingerOffset);
		}
	}
}

void UCockpitSmokeLauncherComponent::UpdateCover(float DeltaTime)
{
	if (!Hinge)
	{
		return;
	}
	USceneComponent* Holding = CockpitGrab::GetHoldingHand(CoverGrab);
	const USceneComponent* Parent = Hinge->GetAttachParent();
	auto HandAngle = [&](const USceneComponent* Hand)
	{
		// Angle of the hand around the hinge, in the hinge's resting frame (0 = shut, + = up).
		const FTransform Rest = FTransform(HingeRest, Hinge->GetRelativeLocation()) * (Parent ? Parent->GetComponentTransform() : FTransform::Identity);
		const FVector Local = Rest.InverseTransformPositionNoScale(Hand->GetComponentLocation());
		return FMath::RadiansToDegrees(FMath::Atan2(Local.Z, Local.X));
	};

	if (Holding && !CoverHand.IsValid())
	{
		CoverHand = Holding;
		GrabAngleOffset = CoverAngle - HandAngle(Holding);
	}
	else if (!Holding)
	{
		CoverHand = nullptr;
	}

	if (const USceneComponent* Hand = CoverHand.Get())
	{
		CoverAngle = FMath::Clamp(HandAngle(Hand) + GrabAngleOffset, 0.0f, CoverOpenAngle);
	}
	else
	{
		// Spring: latches open, or drops shut.
		const float Target = CoverAngle >= CoverLatchAngle ? CoverOpenAngle : 0.0f;
		CoverAngle = FMath::FInterpConstantTo(CoverAngle, Target, DeltaTime, 400.0f);
	}
	Hinge->SetRelativeRotation(HingeRest + FRotator(CoverAngle, 0.0f, 0.0f));
}

void UCockpitSmokeLauncherComponent::UpdateButton(float DeltaTime)
{
	if (!Button)
	{
		return;
	}
	bool bFingerOnButton = false;
	if (IsCoverOpen())
	{
		TArray<FVector> Tips;
		GetFingertips(Tips);
		const FVector Top = Button->GetComponentLocation() + Button->GetUpVector() * 1.5;
		for (const FVector& Tip : Tips)
		{
			bFingerOnButton |= FVector::Dist(Tip, Top) <= PressRadius;
		}
	}
	// One press per touch.
	if (bFingerOnButton && !bFingerWasOnButton)
	{
		const bool bFired = Launch();
		if (const UTankSeatComponent* Seat = GetOwner()->FindComponentByClass<UTankSeatComponent>(); Seat && Seat->GetRider())
		{
			CockpitGrab::PlayHaptic(GetWorld(), ClickHaptic.LoadSynchronous(), EControllerHand::Right, bFired ? 1.0f : 0.3f);
			CockpitGrab::PlayHaptic(GetWorld(), ClickHaptic.LoadSynchronous(), EControllerHand::Left, bFired ? 1.0f : 0.3f);
		}
	}
	bFingerWasOnButton = bFingerOnButton;
	ButtonTravel = FMath::FInterpConstantTo(ButtonTravel, bFingerOnButton ? 1.0f : 0.0f, DeltaTime, 12.0f);
	Button->SetRelativeLocation(ButtonRest - FVector(0, 0, 0.8f * ButtonTravel));
}

void UCockpitSmokeLauncherComponent::UpdateLamp()
{
	if (!LampMaterial)
	{
		return;
	}
	float Strength = 0.0f;
	if (Charges > 0)
	{
		Strength = 40.0f;
	}
	else if (GetWorld())
	{
		// Slow blink while reloading.
		Strength = FMath::Frac(GetWorld()->GetTimeSeconds() * 1.5f) < 0.5f ? 15.0f : 0.0f;
	}
	LampMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength);
}

void UCockpitSmokeLauncherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bSurfaceApplied)
	{
		// BP_GrabComponent changes its parent's collision in its BeginPlay; fix it once after that.
		bSurfaceApplied = true;
		CockpitGrab::SetGrabSurface(CoverGrab, true);
	}
	if (Charges < MaxCharges && ReloadTime > 0.0f)
	{
		ReloadRemaining -= DeltaTime;
		if (ReloadRemaining <= 0.0f)
		{
			++Charges;
			ReloadRemaining = Charges < MaxCharges ? ReloadTime : 0.0f;
		}
	}
	UpdateCover(DeltaTime);
	UpdateButton(DeltaTime);
	UpdateLamp();
}
