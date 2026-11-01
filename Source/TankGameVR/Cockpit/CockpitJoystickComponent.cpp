#include "Cockpit/CockpitJoystickComponent.h"
#include "Cockpit/CockpitGrabUtils.h"

UCockpitJoystickComponent::UCockpitJoystickComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetMobility(EComponentMobility::Movable);
}

void UCockpitJoystickComponent::BeginPlay()
{
	Super::BeginPlay();
	RestRelativeTransform = GetRelativeTransform();
	GrabComponent = FindGrabComponent();
}

USceneComponent* UCockpitJoystickComponent::FindGrabComponent() const
{
	return CockpitGrab::FindGrabComponent(this, GrabComponentName);
}

void UCockpitJoystickComponent::ApplyKnobCollision()
{
	bCollisionApplied = true;
	CockpitGrab::SetGrabSurface(GrabComponent, true);
}

void UCockpitJoystickComponent::PollGrabComponent()
{
	Hand = ExplicitHand.IsValid() ? ExplicitHand.Get() : CockpitGrab::GetHoldingHand(GrabComponent);
}

FTransform UCockpitJoystickComponent::GetBaseTransform() const
{
	const USceneComponent* Parent = GetAttachParent();
	return Parent ? RestRelativeTransform * Parent->GetComponentTransform() : RestRelativeTransform;
}

FVector2D UCockpitJoystickComponent::HandAngles(const FVector& HandWorld) const
{
	const FVector Local = GetBaseTransform().InverseTransformPositionNoScale(HandWorld);
	const double Up = FMath::Max(Local.Z, 1.0);
	return FVector2D(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Up)), FMath::RadiansToDegrees(FMath::Atan2(Local.X, Up)));
}

void UCockpitJoystickComponent::SetExternalValue(FVector2D InValue)
{
	ExternalValue = FVector2D(FMath::Clamp(InValue.X, -1.0, 1.0), FMath::Clamp(InValue.Y, -1.0, 1.0));
}

void UCockpitJoystickComponent::SetHeldByHand(USceneComponent* InHand)
{
	ExplicitHand = InHand;
}

void UCockpitJoystickComponent::ReleaseHand()
{
	ExplicitHand = nullptr;
}

void UCockpitJoystickComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bCollisionApplied)
	{
		ApplyKnobCollision();
	}
	PollGrabComponent();

	const bool bWasHeld = bHeld;
	bHeld = Hand.IsValid();
	if (bHeld)
	{
		const FVector2D Angles = HandAngles(Hand->GetComponentLocation());
		if (!bWasHeld)
		{
			// Relative to the grab point so the stick doesn't jump.
			AnglesAtGrab = Angles;
			ValueAtGrab = Value;
		}
		Value = ValueAtGrab + (Angles - AnglesAtGrab) / MaxTiltAngle;
	}
	else if (!ExternalValue.IsNearlyZero(0.01))
	{
		Value = ExternalValue;
	}
	else
	{
		Value = FMath::Vector2DInterpConstantTo(Value, FVector2D::ZeroVector, DeltaTime, ReturnSpeed);
	}

	Value.X = FMath::Clamp(Value.X, -1.0, 1.0);
	Value.Y = FMath::Clamp(Value.Y, -1.0, 1.0);
	if (bCircularGate && Value.SizeSquared() > 1.0)
	{
		Value.Normalize();
	}
	ApplyVisualTilt();
}

void UCockpitJoystickComponent::ApplyVisualTilt()
{
	// +pitch tips back, +roll tips right.
	const FQuat Tilt = FRotator(-Value.Y * MaxTiltAngle, 0.0f, Value.X * MaxTiltAngle).Quaternion();
	SetRelativeRotation(RestRelativeTransform.GetRotation() * Tilt);
}
