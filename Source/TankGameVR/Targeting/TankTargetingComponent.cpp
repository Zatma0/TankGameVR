#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankDebug.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTankTargetingComponent::UTankTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the hull has moved this frame, so aim and crosshair use the final pose.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

double UTankTargetingComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UTankTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveComponents();
	AimRotation = GetSightTransform().GetRotation();

	if (UTankSettingsSubsystem* SettingsSubsystem = UTankSettingsSubsystem::Get(this))
	{
		Settings = SettingsSubsystem;
		bAimAssistEnabled = SettingsSubsystem->IsAimAssistEnabled();
		SettingsSubsystem->OnAimAssistChanged.AddDynamic(this, &UTankTargetingComponent::HandleAimAssistSettingChanged);
	}
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this))
	{
		TargetRemovedHandle = Registry->OnTargetRemoved.AddUObject(this, &UTankTargetingComponent::HandleTargetRemoved);
	}
}

void UTankTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTankSettingsSubsystem* SettingsSubsystem = Settings.Get())
	{
		SettingsSubsystem->OnAimAssistChanged.RemoveDynamic(this, &UTankTargetingComponent::HandleAimAssistSettingChanged);
	}
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this))
	{
		Registry->OnTargetRemoved.Remove(TargetRemovedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UTankTargetingComponent::ResolveComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	auto FindByName = [Owner](FName Name) -> USceneComponent*
	{
		if (Name.IsNone())
		{
			return nullptr;
		}
		TArray<USceneComponent*> Components;
		Owner->GetComponents(Components);
		for (USceneComponent* Component : Components)
		{
			if (Component->GetFName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
	};
	AimOriginComponent = FindByName(AimOriginComponentName);
	SightComponent = FindByName(SightComponentName);
	TurretComponent = FindByName(TurretComponentName);
	BarrelComponent = FindByName(BarrelComponentName);
}

FVector UTankTargetingComponent::GetAimOrigin() const
{
	if (AimOriginComponent)
	{
		return AimOriginComponent->GetComponentLocation();
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FTransform UTankTargetingComponent::GetSightTransform() const
{
	if (SightComponent)
	{
		return SightComponent->GetComponentTransform();
	}
	const AActor* Owner = GetOwner();
	return Owner ? FTransform(Owner->GetActorQuat(), GetAimOrigin()) : FTransform::Identity;
}

void UTankTargetingComponent::SetAimInput(FVector2D Input)
{
	if (!FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y))
	{
		Input = FVector2D::ZeroVector;
	}
	AimInput = FVector2D(FMath::Clamp(Input.X, -1.0, 1.0), FMath::Clamp(Input.Y, -1.0, 1.0));
}

void UTankTargetingComponent::HandleAimAssistSettingChanged(bool bEnabled)
{
	SetAimAssistEnabled(bEnabled);
}

void UTankTargetingComponent::SetAimAssistEnabled(bool bEnabled)
{
	if (bAimAssistEnabled == bEnabled)
	{
		return;
	}
	bAimAssistEnabled = bEnabled;
	if (!bAimAssistEnabled)
	{
		// Manual aiming continues from where the crosshair is now: no jump, no hidden correction.
		ClearTarget();
		LastDecision = TEXT("Aim assist disabled: manual aim");
	}
	else
	{
		NextAutoAcquireTime = Now();
		LastDecision = TEXT("Aim assist enabled");
	}
}

void UTankTargetingComponent::HandleTargetRemoved(UTargetableComponent* Target)
{
	if (Target && Target == CurrentTarget.Get())
	{
		SetTarget(nullptr, TEXT("Target removed"));
		if (bAimAssistEnabled)
		{
			TryAcquire(GetAimDirection(), TEXT("Re-acquired after target removed"));
		}
	}
}

void UTankTargetingComponent::ClearTarget()
{
	SyncManualAnglesToAim();
	SetTarget(nullptr, TEXT("Cleared"));
}

void UTankTargetingComponent::SetTarget(UTargetableComponent* NewTarget, const FString& Reason)
{
	UTargetableComponent* OldTarget = CurrentTarget.Get();
	if (OldTarget == NewTarget)
	{
		return;
	}
	CurrentTarget = NewTarget;
	IneligibleSince = -1.0;
	LastDecision = Reason;
	if (!NewTarget)
	{
		SyncManualAnglesToAim();
	}
	OnTargetChanged.Broadcast(OldTarget, NewTarget);
}

float UTankTargetingComponent::MeasureVisibility(const UTargetableComponent* Target, const FVector& Origin, const FVector& Location) const
{
	const UWorld* World = GetWorld();
	const AActor* TargetActor = Target ? Target->GetOwner() : nullptr;
	if (!World || !TargetActor)
	{
		return 0.0f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankTargetVisibility), false, GetOwner());
	TArray<AActor*> Attached;
	GetOwner()->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);

	const FVector Right = FVector::CrossProduct(FVector::UpVector, (Location - Origin).GetSafeNormal()).GetSafeNormal();
	const FVector Samples[] = { Location, Location + Right * VisibilitySampleSpread, Location - Right * VisibilitySampleSpread };
	int32 Clear = 0;
	for (const FVector& Sample : Samples)
	{
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Origin, Sample, ECC_Visibility, Params);
		if (!bBlocked || Hit.GetActor() == TargetActor)
		{
			++Clear;
		}
	}
	return static_cast<float>(Clear) / UE_ARRAY_COUNT(Samples);
}

void UTankTargetingComponent::BuildCandidates(const FVector& ReferenceDirection, TArray<FTargetCandidate>& Out) const
{
	Out.Reset();
	UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}
	TArray<UTargetableComponent*> Targets;
	Registry->GetValidTargets(Targets);

	const FVector Origin = GetAimOrigin();
	const FVector SightForward = GetSightTransform().GetRotation().GetForwardVector();
	for (UTargetableComponent* Target : Targets)
	{
		if (Target->GetOwner() == GetOwner())
		{
			continue;
		}
		FTargetCandidate& Candidate = Out.AddDefaulted_GetRef();
		Candidate.Target = Target;
		Candidate.Location = Target->GetTargetLocation();
		Candidate.StableId = Target->GetUniqueID();
		Candidate.Visibility = Selection.MinimumTargetVisibility > 0.0f ? MeasureVisibility(Target, Origin, Candidate.Location) : 1.0f;
		UTargetSelectionMath::EvaluateCandidate(Candidate, Origin, ReferenceDirection, SightForward, Selection);
	}
}

FVector UTankTargetingComponent::GetReferenceDirection() const
{
	if (const UTargetableComponent* Target = CurrentTarget.Get())
	{
		return (Target->GetTargetLocation() - GetAimOrigin()).GetSafeNormal(UE_SMALL_NUMBER, GetAimDirection());
	}
	return GetAimDirection();
}

void UTankTargetingComponent::RefreshCandidates()
{
	TArray<FTargetCandidate> Candidates;
	BuildCandidates(GetReferenceDirection(), Candidates);
	LastCandidates = MoveTemp(Candidates);
}

bool UTankTargetingComponent::TryAcquire(const FVector& ReferenceDirection, const FString& Reason)
{
	TArray<FTargetCandidate> Candidates;
	BuildCandidates(ReferenceDirection, Candidates);
	const int32 Index = UTargetSelectionMath::SelectNearestToReference(Candidates, Selection);
	LastCandidates = Candidates;
	if (Index == INDEX_NONE)
	{
		return false;
	}
	SetTarget(Candidates[Index].Target, Reason);
	return true;
}

bool UTankTargetingComponent::RequestTargetSwitch(int32 Direction)
{
	if (!bAimAssistEnabled || Direction == 0)
	{
		return false;
	}
	const double Time = Now();
	const TCHAR* SideName = Direction > 0 ? TEXT("right") : TEXT("left");
	if (Time - LastSwitchTime < TargetSwitchCooldown)
	{
		LastDecision = FString::Printf(TEXT("Switch %s ignored (cooldown)"), SideName);
		return false;
	}

	TArray<FTargetCandidate> Candidates;
	BuildCandidates(GetReferenceDirection(), Candidates);
	const int32 Index = UTargetSelectionMath::SelectSwitchTarget(Candidates, Direction, CurrentTarget.Get(), Selection);
	LastCandidates = Candidates;
	if (Index == INDEX_NONE)
	{
		LastDecision = FString::Printf(TEXT("No eligible target to the %s"), SideName);
		return false;
	}
	LastSwitchTime = Time;
	SetTarget(Candidates[Index].Target, FString::Printf(TEXT("Switched %s (score %.1f, yaw %+.1f)"), SideName,
		FMath::Abs(Candidates[Index].YawOffset), Candidates[Index].YawOffset));
	return true;
}

void UTankTargetingComponent::MaintainCurrentTarget(double Time)
{
	UTargetableComponent* Target = CurrentTarget.Get();
	if (CurrentTarget.IsStale() || (Target && !Target->IsValidTarget()))
	{
		SetTarget(nullptr, TEXT("Target lost (destroyed or disabled)"));
		Target = nullptr;
		if (bAimAssistEnabled)
		{
			TryAcquire(GetAimDirection(), TEXT("Re-acquired after target lost"));
		}
		return;
	}
	if (!Target || Time < NextEligibilityCheckTime)
	{
		return;
	}
	// Range/angle/visibility check at 10 Hz; the lock only drops after staying ineligible for a while.
	NextEligibilityCheckTime = Time + 0.1;
	FTargetCandidate Candidate;
	Candidate.Target = Target;
	Candidate.Location = Target->GetTargetLocation();
	Candidate.Visibility = Selection.MinimumTargetVisibility > 0.0f ? MeasureVisibility(Target, GetAimOrigin(), Candidate.Location) : 1.0f;
	UTargetSelectionMath::EvaluateCandidate(Candidate, GetAimOrigin(), GetAimDirection(), GetSightTransform().GetRotation().GetForwardVector(), Selection);
	if (Candidate.IsEligible())
	{
		IneligibleSince = -1.0;
		return;
	}
	if (IneligibleSince < 0.0)
	{
		IneligibleSince = Time;
	}
	else if (Time - IneligibleSince >= TargetSelectionSmoothing)
	{
		SetTarget(nullptr, FString::Printf(TEXT("Target no longer eligible (%s)"), *StaticEnum<ETargetRejectReason>()->GetNameStringByValue(static_cast<int64>(Candidate.RejectReason))));
		TryAcquire(GetAimDirection(), TEXT("Re-acquired after previous target became ineligible"));
	}
}

void UTankTargetingComponent::HandleSwitchInput(double Time)
{
	const float Magnitude = FMath::Abs(AimInput.X);
	if (Magnitude >= TargetSwitchDeadZone)
	{
		const int32 Direction = AimInput.X > 0.0 ? 1 : -1;
		if (!bSwitchHeld || Direction != HeldDirection)
		{
			// Edge: a fresh push (or a reversal) switches once.
			bSwitchHeld = true;
			HeldDirection = Direction;
			NextRepeatTime = Time + TargetSwitchRepeatDelay;
			RequestTargetSwitch(Direction);
		}
		else if (TargetSwitchRepeatInterval > 0.0f && Time >= NextRepeatTime)
		{
			// Held: step again at a controlled rate.
			NextRepeatTime = Time + TargetSwitchRepeatInterval;
			RequestTargetSwitch(Direction);
		}
	}
	else if (Magnitude < TargetSwitchReleaseThreshold)
	{
		bSwitchHeld = false;
		HeldDirection = 0;
	}
}

void UTankTargetingComponent::UpdateManualAim(float DeltaTime)
{
	auto Shape = [this](double V) { return FMath::Abs(V) <= ManualAimDeadZone ? 0.0 : FMath::Sign(V) * (FMath::Abs(V) - ManualAimDeadZone) / (1.0 - ManualAimDeadZone); };
	ManualYaw = FMath::Clamp(ManualYaw + Shape(AimInput.X) * ManualAimSpeed * DeltaTime, -ManualYawLimit, ManualYawLimit);
	ManualPitch = FMath::Clamp(ManualPitch + Shape(AimInput.Y) * ManualAimSpeed * DeltaTime, ManualPitchRange.X, ManualPitchRange.Y);
}

void UTankTargetingComponent::SyncManualAnglesToAim()
{
	const FVector SightForward = GetSightTransform().GetRotation().GetForwardVector();
	float Yaw = 0.0f;
	float Pitch = 0.0f;
	UTargetSelectionMath::RelativeYawPitch(SightForward, GetAimDirection(), Yaw, Pitch);
	ManualYaw = FMath::Clamp(Yaw, -ManualYawLimit, ManualYawLimit);
	ManualPitch = FMath::Clamp(Pitch, ManualPitchRange.X, ManualPitchRange.Y);
}

void UTankTargetingComponent::UpdateAim(float DeltaTime)
{
	const FTransform Sight = GetSightTransform();
	const FQuat ManualRotation = Sight.GetRotation() * FRotator(ManualPitch, ManualYaw, 0.0f).Quaternion();
	FQuat Desired = ManualRotation;

	if (const UTargetableComponent* Target = CurrentTarget.Get())
	{
		const FVector ToTarget = Target->GetTargetLocation() - GetAimOrigin();
		if (!ToTarget.IsNearlyZero())
		{
			const FQuat TargetRotation = FRotationMatrix::MakeFromXZ(ToTarget, FVector::UpVector).ToQuat();
			Desired = FQuat::Slerp(ManualRotation, TargetRotation, TargetLockStrength);
		}
	}

	const float Alpha = 1.0f - FMath::Exp(-CrosshairTransitionSpeed * FMath::Max(DeltaTime, 0.0f));
	FQuat Next = FQuat::Slerp(AimRotation, Desired, Alpha);
	Next.Normalize();
	if (Next.ContainsNaN())
	{
		Next = Desired;
	}
	AimRotation = Next;
}

void UTankTargetingComponent::UpdateTurretVisuals() const
{
	const FVector Aim = GetAimDirection();
	if (TurretComponent)
	{
		FRotator TurretRotation = TurretComponent->GetComponentRotation();
		TurretRotation.Yaw = Aim.Rotation().Yaw;
		TurretComponent->SetWorldRotation(TurretRotation);
	}
	if (BarrelComponent)
	{
		float Yaw = 0.0f;
		float Pitch = 0.0f;
		const USceneComponent* Parent = BarrelComponent->GetAttachParent();
		UTargetSelectionMath::RelativeYawPitch(Parent ? Parent->GetForwardVector() : GetOwner()->GetActorForwardVector(), Aim, Yaw, Pitch);
		BarrelComponent->SetRelativeRotation(FRotator(Pitch, 0.0f, 0.0f));
	}
}

void UTankTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Time = Now();

	if (bAimAssistEnabled)
	{
		MaintainCurrentTarget(Time);
		if (CurrentTarget.IsValid())
		{
			HandleSwitchInput(Time);
		}
		else
		{
			// No lock: the stick aims manually until a target can be acquired.
			bSwitchHeld = false;
			UpdateManualAim(DeltaTime);
			if (bAutoAcquire && Time >= NextAutoAcquireTime)
			{
				NextAutoAcquireTime = Time + AutoAcquireInterval;
				TryAcquire(GetAimDirection(), TEXT("Acquired"));
			}
		}
	}
	else
	{
		if (CurrentTarget.IsValid())
		{
			ClearTarget();
		}
		UpdateManualAim(DeltaTime);
	}

	UpdateAim(DeltaTime);
	if (CurrentTarget.IsValid())
	{
		// Keep manual angles following the lock, so dropping it continues from the current aim.
		SyncManualAnglesToAim();
	}
	UpdateTurretVisuals();

	if (TankDebug::IsTargetingDebugEnabled(bDebugDraw))
	{
		// Debug only: re-score every target each frame so the overlay shows live values.
		RefreshCandidates();
		DrawDebug();
	}
}

void UTankTargetingComponent::DrawDebug() const
{
	UWorld* World = GetWorld();
	const FVector Origin = GetAimOrigin();
	DrawDebugLine(World, Origin, Origin + GetAimDirection() * 3000.0, bAimAssistEnabled ? FColor::Yellow : FColor::White, false, -1.0f, 0, 2.0f);

	for (const FTargetCandidate& Candidate : LastCandidates)
	{
		if (!Candidate.Target)
		{
			continue;
		}
		const bool bCurrent = Candidate.Target == CurrentTarget.Get();
		const FColor Color = bCurrent ? FColor::Red : (Candidate.IsEligible() ? FColor::Yellow : FColor(90, 90, 90));
		DrawDebugSphere(World, Candidate.Location, bCurrent ? 90.0f : 60.0f, 12, Color, false, -1.0f, 0, bCurrent ? 4.0f : 1.5f);
		const FString Label = FString::Printf(TEXT("%s\nyaw %+.1f pitch %+.1f\n%.0f m vis %.2f\n%s"),
			*Candidate.Target->GetTargetDisplayName().ToString(), Candidate.YawOffset, Candidate.PitchOffset, Candidate.Distance / 100.0f,
			Candidate.Visibility, *StaticEnum<ETargetRejectReason>()->GetNameStringByValue(static_cast<int64>(Candidate.RejectReason)));
		DrawDebugString(World, Candidate.Location + FVector(0.0, 0.0, 150.0), Label, nullptr, Color, 0.0f, false, 1.0f);
	}
	const FString Status = FString::Printf(TEXT("AimAssist %s  Target %s\nInput (%.2f, %.2f)  %s"),
		bAimAssistEnabled ? TEXT("ON") : TEXT("OFF"),
		CurrentTarget.IsValid() ? *CurrentTarget->GetTargetDisplayName().ToString() : TEXT("none"),
		AimInput.X, AimInput.Y, *LastDecision);
	DrawDebugString(World, Origin + FVector(0.0, 0.0, 120.0), Status, nullptr, FColor::White, 0.0f, false, 1.1f);
}
