#include "Cockpit/CockpitGrabbableComponent.h"
#include "Cockpit/CockpitGrabUtils.h"

UCockpitGrabbableComponent::UCockpitGrabbableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetMobility(EComponentMobility::Movable);
}

void UCockpitGrabbableComponent::BeginPlay()
{
	Super::BeginPlay();
	HomeRelativeTransform = GetRelativeTransform();
	GrabComponent = CockpitGrab::FindGrabComponent(this, GrabComponentName);
}

void UCockpitGrabbableComponent::SetHeldByHand(USceneComponent* InHand)
{
	ExplicitHand = InHand;
}

void UCockpitGrabbableComponent::ReleaseHand()
{
	ExplicitHand = nullptr;
}

void UCockpitGrabbableComponent::SetRail(USceneComponent* Frame, float Min, float Max)
{
	RailFrame = Frame;
	RailMin = FMath::Min(Min, Max);
	RailMax = FMath::Max(Min, Max);
	if (Frame)
	{
		RailPosition = FMath::Clamp(static_cast<float>(Frame->GetComponentTransform().InverseTransformPositionNoScale(GetComponentLocation()).X), RailMin, RailMax);
	}
}

void UCockpitGrabbableComponent::ClearRail()
{
	RailFrame = nullptr;
}

bool UCockpitGrabbableComponent::IsOnRail(const USceneComponent* Frame) const
{
	return RailFrame.IsValid() && (!Frame || RailFrame.Get() == Frame);
}

void UCockpitGrabbableComponent::MoveOnRail(float Position)
{
	if (const USceneComponent* Frame = RailFrame.Get())
	{
		RailPosition = FMath::Clamp(Position, RailMin, RailMax);
		const FTransform Rail = Frame->GetComponentTransform();
		SetWorldLocationAndRotation(Rail.TransformPositionNoScale(FVector(RailPosition, 0.0, 0.0)), Rail.GetRotation());
	}
}

void UCockpitGrabbableComponent::SetLocked(bool bLocked)
{
	if (bLocked)
	{
		Hand = nullptr;
		State = ECockpitGrabState::Locked;
	}
	else if (State == ECockpitGrabState::Locked)
	{
		State = ECockpitGrabState::Home;
		if (bReturnHomeOnRelease)
		{
			ReturnHome();
		}
	}
}

void UCockpitGrabbableComponent::SetAway(bool bAway)
{
	if (bAway)
	{
		ClearRail();
		Hand = nullptr;
		State = ECockpitGrabState::Away;
		SetVisibility(false, true);
		CockpitGrab::SetGrabSurface(GrabComponent, false);
	}
	else if (State == ECockpitGrabState::Away)
	{
		SetRelativeTransform(HomeRelativeTransform);
		SetVisibility(true, true);
		CockpitGrab::SetGrabSurface(GrabComponent, true);
		State = ECockpitGrabState::Home;
	}
}

void UCockpitGrabbableComponent::ReturnHome(bool bInstant)
{
	ClearRail();
	if (State == ECockpitGrabState::Away)
	{
		return;
	}
	Hand = nullptr;
	if (bInstant)
	{
		SetRelativeTransform(HomeRelativeTransform);
		State = ECockpitGrabState::Home;
	}
	else
	{
		State = ECockpitGrabState::Returning;
	}
}

void UCockpitGrabbableComponent::FollowHand()
{
	const FTransform Desired = GrabOffset * Hand->GetComponentTransform();
	if (const USceneComponent* Frame = RailFrame.Get())
	{
		const FTransform Rail = Frame->GetComponentTransform();
		RailPosition = FMath::Clamp(static_cast<float>(Rail.InverseTransformPositionNoScale(Desired.GetLocation()).X), RailMin, RailMax);
		SetWorldLocationAndRotation(Rail.TransformPositionNoScale(FVector(RailPosition, 0.0, 0.0)), Rail.GetRotation());
	}
	else
	{
		SetWorldLocationAndRotation(Desired.GetLocation(), Desired.GetRotation());
	}
}

void UCockpitGrabbableComponent::UpdateReturn(float DeltaTime)
{
	// In parent space, so it rides along with the tank.
	const FVector Location = FMath::VInterpTo(GetRelativeLocation(), HomeRelativeTransform.GetLocation(), DeltaTime, ReturnSpeed);
	const FQuat Rotation = FMath::QInterpTo(GetRelativeRotation().Quaternion(), HomeRelativeTransform.GetRotation(), DeltaTime, ReturnSpeed);
	if (FVector::Dist(Location, HomeRelativeTransform.GetLocation()) < 0.3 && Rotation.AngularDistance(HomeRelativeTransform.GetRotation()) < 0.02)
	{
		SetRelativeTransform(HomeRelativeTransform);
		State = ECockpitGrabState::Home;
		return;
	}
	SetRelativeLocationAndRotation(Location, Rotation);
}

void UCockpitGrabbableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSurfaceApplied)
	{
		bSurfaceApplied = true;
		CockpitGrab::SetGrabSurface(GrabComponent, State != ECockpitGrabState::Away);
	}

	USceneComponent* Holding = ExplicitHand.IsValid() ? ExplicitHand.Get() : CockpitGrab::GetHoldingHand(GrabComponent);
	const bool bHolding = Holding != nullptr;

	// A grab needs a fresh press, so a hand that was holding while the item was locked doesn't snatch it back.
	if (bHolding && !bHandWasHolding && (State == ECockpitGrabState::Home || State == ECockpitGrabState::Returning))
	{
		Hand = Holding;
		GrabOffset = GetComponentTransform().GetRelativeTransform(Holding->GetComponentTransform());
		State = ECockpitGrabState::Held;
		HandleGrabbed();
		OnGrabbed.Broadcast(this);
	}
	else if (State == ECockpitGrabState::Held && (!bHolding || !Hand.IsValid()))
	{
		Hand = nullptr;
		State = ECockpitGrabState::Home;
		if (bReturnHomeOnRelease)
		{
			ReturnHome();
		}
		else
		{
			ClearRail();
		}
		HandleReleased();
		OnReleased.Broadcast(this);
	}
	bHandWasHolding = bHolding;

	if (State == ECockpitGrabState::Held)
	{
		FollowHand();
	}
	else if (State == ECockpitGrabState::Returning)
	{
		UpdateReturn(DeltaTime);
	}
}
