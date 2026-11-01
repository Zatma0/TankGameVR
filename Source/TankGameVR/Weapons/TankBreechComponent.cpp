#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankShellComponent.h"
#include "Cockpit/CockpitGrabUtils.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"

UTankBreechComponent::UTankBreechComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetMobility(EComponentMobility::Movable);

	CloseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02")));
	ClickHaptic = TSoftObjectPtr<UHapticFeedbackEffect_Base>(FSoftObjectPath(TEXT("/Game/XRFramework/Haptics/GrabHapticEffect.GrabHapticEffect")));
}

void UTankBreechComponent::BeginPlay()
{
	Super::BeginPlay();
	RestRelativeTransform = GetRelativeTransform();

	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents<UTankShellComponent>(Shells);
	}
	// Rounds move first, then the breech looks at them.
	for (UTankShellComponent* Round : Shells)
	{
		AddTickPrerequisiteComponent(Round);
	}

	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child->GetFName() == BlockComponentName)
		{
			Block = Child;
		}
		else if (Child->GetFName() == CasingComponentName)
		{
			Casing = Child;
		}
		else if (Child->GetFName() == LampComponentName)
		{
			if (UPrimitiveComponent* Lamp = Cast<UPrimitiveComponent>(Child); Lamp && Lamp->GetNumMaterials() > 0)
			{
				LampMaterial = Lamp->CreateDynamicMaterialInstance(0);
			}
		}
	}
	if (Block)
	{
		BlockClosedLocation = Block->GetRelativeLocation();
	}
	if (Casing)
	{
		// The stub flies in cockpit space, not with the recoiling breech.
		Casing->AttachToComponent(GetAttachParent(), FAttachmentTransformRules::KeepWorldTransform);
		Casing->SetVisibility(false, true);
	}
	UpdateVisuals(0.0f);
}

float UTankBreechComponent::RailMinFor(const UTankShellComponent* Round) const
{
	return -Round->NoseOffset - ExitMargin;
}

float UTankBreechComponent::RailMaxFor(const UTankShellComponent* Round) const
{
	return RamDepth - Round->NoseOffset;
}

void UTankBreechComponent::SetState(ETankBreechState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.0f;
	if (NewState != ETankBreechState::Ramming)
	{
		RamProgress = (NewState == ETankBreechState::Closing || NewState == ETankBreechState::Loaded) ? 1.0f : 0.0f;
	}
	OnStateChanged.Broadcast(NewState);
}

FString UTankBreechComponent::GetStatusText() const
{
	switch (State)
	{
	case ETankBreechState::Ramming:
		return FString::Printf(TEXT("RAMMING %d%%"), FMath::RoundToInt(RamProgress / FMath::Max(RamThreshold, 0.01f) * 100.0f));
	case ETankBreechState::AutoLoading:
	case ETankBreechState::Closing:
		return TEXT("LOADING...");
	case ETankBreechState::Loaded:
		return TEXT("UP - READY TO FIRE");
	case ETankBreechState::Recoil:
		return TEXT("SHOT OUT");
	default:
		break;
	}
	for (const UTankShellComponent* Round : Shells)
	{
		if (Round && Round->IsAvailable())
		{
			return TEXT("EMPTY - LOAD A SHELL");
		}
	}
	return TEXT("EMPTY - RACK REFILLING");
}

bool UTankBreechComponent::Fire()
{
	if (State != ETankBreechState::Loaded)
	{
		return false;
	}
	if (UTankShellComponent* Round = Shell.Get())
	{
		Round->Consume();
	}
	Shell = nullptr;
	SetState(ETankBreechState::Recoil);
	return true;
}

bool UTankBreechComponent::AutoLoad()
{
	if (State != ETankBreechState::Open)
	{
		return false;
	}
	for (UTankShellComponent* Round : Shells)
	{
		if (Round && Round->IsAtHome())
		{
			Round->SetLocked(true);
			AutoStartInBreech = Round->GetComponentTransform().GetRelativeTransform(GetComponentTransform());
			Round->SetRail(this, RailMinFor(Round), RailMaxFor(Round));
			Shell = Round;
			SetState(ETankBreechState::AutoLoading);
			return true;
		}
	}
	return false;
}

void UTankBreechComponent::TryCapture()
{
	const FVector Mouth = GetComponentLocation();
	const FVector Bore = GetForwardVector();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(CaptureAngle));
	for (UTankShellComponent* Round : Shells)
	{
		if (!Round || !Round->IsHeld() || Round->IsOnRail())
		{
			continue;
		}
		if (FVector::Dist(Round->GetNoseLocation(), Mouth) > CaptureRadius || FVector::DotProduct(Round->GetForwardVector(), Bore) < MinDot)
		{
			continue;
		}
		Round->SetRail(this, RailMinFor(Round), RailMaxFor(Round));
		Shell = Round;
		SetState(ETankBreechState::Ramming);
		PlaySound(CaptureSound, 0.5f, 1.0f);
		Click(Round->GetHand(), 0.4f);
		return;
	}
}

void UTankBreechComponent::UpdateRamming()
{
	UTankShellComponent* Round = Shell.Get();
	if (!Round || !Round->IsOnRail(this) || !Round->IsHeld())
	{
		// Let go before it was in far enough: it goes back to the rack by itself.
		Shell = nullptr;
		SetState(ETankBreechState::Open);
		return;
	}
	const float Depth = Round->GetRailPosition() + Round->NoseOffset;
	RamProgress = FMath::Clamp(Depth / RamDepth, 0.0f, 1.0f);
	if (Depth <= -ExitMargin + 0.01f)
	{
		Round->ClearRail();
		Shell = nullptr;
		SetState(ETankBreechState::Open);
	}
	else if (RamProgress >= RamThreshold)
	{
		StartClosing(Round->GetHand());
	}
}

void UTankBreechComponent::UpdateAutoLoading()
{
	UTankShellComponent* Round = Shell.Get();
	if (!Round)
	{
		SetState(ETankBreechState::Open);
		return;
	}
	const float Alpha = FMath::Clamp(StateTime / AutoLoadTime, 0.0f, 1.0f);
	const float Entry = -Round->NoseOffset;
	if (Alpha < 0.5f)
	{
		// Carry it from the rack to the mouth.
		const float T = FMath::SmoothStep(0.0f, 1.0f, Alpha * 2.0f);
		const FTransform EntryPose(FQuat::Identity, FVector(Entry, 0.0, 0.0));
		FTransform Pose;
		Pose.Blend(AutoStartInBreech, EntryPose, T);
		const FTransform World = Pose * GetComponentTransform();
		Round->SetWorldLocationAndRotation(World.GetLocation(), World.GetRotation());
	}
	else
	{
		const float T = (Alpha - 0.5f) * 2.0f;
		Round->MoveOnRail(FMath::Lerp(Entry, Entry + RamDepth * RamThreshold, T));
		RamProgress = RamThreshold * T;
		if (Alpha >= 1.0f)
		{
			StartClosing(nullptr);
		}
	}
}

void UTankBreechComponent::StartClosing(USceneComponent* FromHand)
{
	UTankShellComponent* Round = Shell.Get();
	if (!Round)
	{
		return;
	}
	Round->SetLocked(true);
	CloseStartPosition = Round->GetRailPosition();
	SetState(ETankBreechState::Closing);
	PlaySound(CloseSound, CloseSoundVolume, CloseSoundPitch);
	Click(FromHand, 1.0f);
}

void UTankBreechComponent::UpdateRecoil()
{
	float Offset = 0.0f;
	if (StateTime < RecoilKickTime)
	{
		Offset = RecoilDistance * (StateTime / RecoilKickTime);
	}
	else
	{
		const float T = FMath::Clamp((StateTime - RecoilKickTime) / RecoilReturnTime, 0.0f, 1.0f);
		Offset = RecoilDistance * (1.0f - FMath::SmoothStep(0.0f, 1.0f, T));
	}
	SetRelativeLocation(RestRelativeTransform.GetLocation() - RestRelativeTransform.GetRotation().GetForwardVector() * Offset);

	if (StateTime >= RecoilKickTime + RecoilReturnTime)
	{
		SetRelativeTransform(RestRelativeTransform);
		SetState(ETankBreechState::Open);
		EjectCasing();
	}
}

void UTankBreechComponent::EjectCasing()
{
	if (!Casing)
	{
		return;
	}
	CasingLocation = RestRelativeTransform.GetLocation();
	CasingSpeed = RestRelativeTransform.GetRotation().RotateVector(CasingVelocity);
	CasingRemaining = CasingLifetime;
	bCasingFlying = true;
	Casing->SetRelativeLocationAndRotation(CasingLocation, RestRelativeTransform.GetRotation());
	Casing->SetVisibility(true, true);
}

void UTankBreechComponent::UpdateCasing(float DeltaTime)
{
	if (!Casing || CasingRemaining <= 0.0f)
	{
		return;
	}
	if (bCasingFlying)
	{
		CasingSpeed.Z -= CasingGravity * DeltaTime;
		CasingLocation += CasingSpeed * DeltaTime;
		if (CasingLocation.Z <= CasingFloorZ)
		{
			CasingLocation.Z = CasingFloorZ;
			// Small bounce, then it lies there.
			CasingSpeed = FVector(CasingSpeed.X * 0.4, CasingSpeed.Y * 0.4, -CasingSpeed.Z * 0.3);
			bCasingFlying = CasingSpeed.Z > 25.0;
		}
		const FQuat Spin(Casing->GetRelativeRotation().Quaternion().GetRightVector(), FMath::DegreesToRadians(540.0f) * DeltaTime);
		Casing->SetRelativeLocationAndRotation(CasingLocation, Spin * Casing->GetRelativeRotation().Quaternion());
	}
	CasingRemaining -= DeltaTime;
	if (CasingRemaining <= 0.0f)
	{
		Casing->SetVisibility(false, true);
	}
}

void UTankBreechComponent::UpdateVisuals(float DeltaTime)
{
	const bool bClosed = State == ETankBreechState::Closing || State == ETankBreechState::Loaded || State == ETankBreechState::Recoil;
	const float Target = bClosed ? 1.0f : 0.0f;
	BlockAlpha = DeltaTime > 0.0f ? FMath::FInterpConstantTo(BlockAlpha, Target, DeltaTime, 1.0f / CloseTime) : Target;
	if (Block)
	{
		Block->SetRelativeLocation(BlockClosedLocation + BlockOpenOffset * (1.0f - BlockAlpha));
	}
	if (LampMaterial)
	{
		LampMaterial->SetScalarParameterValue(LampParameter, State == ETankBreechState::Loaded ? LampOnStrength : LampOffStrength);
	}
}

void UTankBreechComponent::PlaySound(const TSoftObjectPtr<USoundBase>& Sound, float Volume, float Pitch) const
{
	if (USoundBase* Loaded = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Loaded, GetComponentLocation(), Volume, Pitch);
	}
}

void UTankBreechComponent::Click(USceneComponent* Hand, float Scale) const
{
	if (Hand)
	{
		CockpitGrab::PlayHaptic(GetWorld(), ClickHaptic.LoadSynchronous(), CockpitGrab::GetHandSide(Hand), Scale);
	}
}

void UTankBreechComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	StateTime += DeltaTime;

	switch (State)
	{
	case ETankBreechState::Open:
		TryCapture();
		break;
	case ETankBreechState::Ramming:
		UpdateRamming();
		break;
	case ETankBreechState::AutoLoading:
		UpdateAutoLoading();
		break;
	case ETankBreechState::Closing:
		if (UTankShellComponent* Round = Shell.Get())
		{
			const float T = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(StateTime / CloseTime, 0.0f, 1.0f));
			Round->MoveOnRail(FMath::Lerp(CloseStartPosition, RailMaxFor(Round), T));
		}
		if (StateTime >= CloseTime)
		{
			SetState(ETankBreechState::Loaded);
		}
		break;
	case ETankBreechState::Recoil:
		UpdateRecoil();
		break;
	default:
		break;
	}

	UpdateVisuals(DeltaTime);
	UpdateCasing(DeltaTime);
}
