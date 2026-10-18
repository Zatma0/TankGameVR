#include "Cockpit/CockpitJoystickComponent.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/UnrealType.h"

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
	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		const bool bNameMatches = !GrabComponentName.IsNone() && Child->GetFName() == GrabComponentName;
		const bool bClassMatches = GrabComponentName.IsNone() && Child->GetClass()->GetName().Contains(TEXT("GrabComponent"));
		if (bNameMatches || bClassMatches)
		{
			return Child;
		}
	}
	return nullptr;
}

void UCockpitJoystickComponent::ApplyKnobCollision()
{
	// The template grab component forces its parent to the PhysicsActor profile in BeginPlay. A cockpit knob must
	// stay findable by the hand's grab trace (PhysicsBody object type) without colliding with anything.
	bCollisionApplied = true;
	if (UPrimitiveComponent* Knob = GrabComponent ? Cast<UPrimitiveComponent>(GrabComponent->GetAttachParent()) : nullptr)
	{
		Knob->SetSimulatePhysics(false);
		Knob->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Knob->SetCollisionObjectType(ECC_PhysicsBody);
		Knob->SetCollisionResponseToAllChannels(ECR_Ignore);
	}
}

void UCockpitJoystickComponent::PollGrabComponent()
{
	if (ExplicitHand.IsValid())
	{
		Hand = ExplicitHand;
		return;
	}
	if (!GrabComponent)
	{
		Hand = nullptr;
		return;
	}
	// Read the template grab component's public state through reflection, so the Blueprint stays untouched.
	bool bIsHeld = false;
	if (const FBoolProperty* HeldProperty = FindFProperty<FBoolProperty>(GrabComponent->GetClass(), TEXT("bIsHeld")))
	{
		bIsHeld = HeldProperty->GetPropertyValue_InContainer(GrabComponent);
	}
	USceneComponent* Controller = nullptr;
	if (const FObjectPropertyBase* ControllerProperty = FindFProperty<FObjectPropertyBase>(GrabComponent->GetClass(), TEXT("MotionControllerRef")))
	{
		Controller = Cast<USceneComponent>(ControllerProperty->GetObjectPropertyValue_InContainer(GrabComponent));
	}
	Hand = (bIsHeld && Controller) ? Controller : nullptr;
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
			// Relative to where the hand grabbed: grabbing never makes the stick jump.
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
	// Positive pitch would tip +Z backwards and positive roll would tip it left, hence the negations.
	const FQuat Tilt = FRotator(-Value.Y * MaxTiltAngle, 0.0f, -Value.X * MaxTiltAngle).Quaternion();
	SetRelativeRotation(RestRelativeTransform.GetRotation() * Tilt);
}
