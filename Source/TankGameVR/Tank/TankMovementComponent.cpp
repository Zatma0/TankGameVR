#include "Tank/TankMovementComponent.h"
#include "Tank/TankDebug.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTankMovementComponent::UTankMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTankMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bFollowGround && RideHeight <= 0.0f && GetOwner() && GetWorld())
	{
		const FVector Start = GetOwner()->GetActorLocation();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TankRideHeight), false, GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.0, 0.0, 5000.0), GroundTraceChannel, Params))
		{
			RideHeight = Start.Z - Hit.ImpactPoint.Z;
		}
	}
}

void UTankMovementComponent::SetDriveInput(FVector2D RawStick)
{
	if (!FMath::IsFinite(RawStick.X) || !FMath::IsFinite(RawStick.Y))
	{
		RawStick = FVector2D::ZeroVector;
	}
	RawInput = FVector2D(FMath::Clamp(RawStick.X, -1.0, 1.0), FMath::Clamp(RawStick.Y, -1.0, 1.0));
}

void UTankMovementComponent::StopImmediately()
{
	RawInput = SmoothedInput = LimitedInput = ProcessedInput = FVector2D::ZeroVector;
	CurrentSpeed = TargetSpeed = CurrentTurnRate = 0.0f;
}

FVector UTankMovementComponent::GetForwardVector() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ForwardVector;
	}
	FVector Forward = Owner->GetActorTransform().TransformVectorNoScale(LocalForwardAxis.GetSafeNormal());
	Forward.Z = 0.0;
	return Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

void UTankMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root || DeltaTime <= 0.0f)
	{
		return;
	}
	// Long hitches would integrate a huge step; clamp so the tank never jumps.
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	// 1) Stick shaping: filter, rate limit, dead zone + response curve.
	SmoothedInput = UTankMovementMath::SmoothStick(SmoothedInput, RawInput, Dt, Tuning.JoystickSmoothing);
	LimitedInput = UTankMovementMath::RateLimit(LimitedInput, SmoothedInput, Tuning.InputResponseSpeed, Dt);
	ProcessedInput.X = UTankMovementMath::ShapeAxis(LimitedInput.X, Tuning.SteeringDeadZone, Tuning.SteeringCurve, Tuning.SteeringSensitivity);
	ProcessedInput.Y = UTankMovementMath::ShapeAxis(LimitedInput.Y, Tuning.ThrottleDeadZone, Tuning.ThrottleCurve, 1.0f);

	// 2) Desired motion, then smooth integration (speed by accel/decel/brake, turn rate by comfort limit).
	TargetSpeed = UTankMovementMath::ComputeTargetSpeed(Tuning, ProcessedInput.X, ProcessedInput.Y);
	CurrentSpeed = UTankMovementMath::StepSpeed(Tuning, CurrentSpeed, TargetSpeed, Dt);
	const float TargetTurnRate = UTankMovementMath::ComputeTargetTurnRate(Tuning, ProcessedInput.X, CurrentSpeed);
	CurrentTurnRate = FMath::FInterpConstantTo(CurrentTurnRate, TargetTurnRate, Dt, Tuning.MaximumTurnAcceleration);

	// 3) Move along the hull's own forward axis (mid-step heading keeps arcs accurate at any frame rate).
	const float YawDelta = CurrentTurnRate * Dt;
	const FQuat CurrentRotation = Root->GetComponentQuat();
	const FQuat HalfStep = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDelta * 0.5f)) * CurrentRotation;
	FVector Forward = HalfStep.RotateVector(LocalForwardAxis.GetSafeNormal());
	Forward.Z = 0.0;
	Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	const FQuat NewRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDelta)) * CurrentRotation;
	const FVector Delta = Forward * CurrentSpeed * Dt;

	FHitResult Hit;
	Root->MoveComponent(Delta, NewRotation, true, &Hit, MOVECOMP_NoFlags, ETeleportType::None);
	bBlockedLastFrame = Hit.bBlockingHit && !Delta.IsNearlyZero();
	if (bBlockedLastFrame)
	{
		// Driving into something solid: stop instead of sliding sideways along it.
		CurrentSpeed = 0.0f;
	}

	if (bFollowGround)
	{
		FollowGround();
	}

	if (TankDebug::IsMovementDebugEnabled(bDebugDraw))
	{
		DrawDebug();
	}
}

void UTankMovementComponent::FollowGround()
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World || RideHeight <= 0.0f)
	{
		return;
	}
	const FVector Location = Owner->GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankGround), false, Owner);
	TArray<AActor*> Attached;
	Owner->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, MaxGroundStep), Location - FVector(0.0, 0.0, RideHeight + MaxGroundStep), GroundTraceChannel, Params))
	{
		const float DesiredZ = Hit.ImpactPoint.Z + RideHeight;
		if (FMath::Abs(DesiredZ - Location.Z) <= MaxGroundStep && !FMath::IsNearlyEqual(DesiredZ, Location.Z, 0.05))
		{
			Owner->GetRootComponent()->MoveComponent(FVector(0.0, 0.0, DesiredZ - Location.Z), Owner->GetActorQuat(), false);
		}
	}
}

void UTankMovementComponent::DrawDebug() const
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const FVector Origin = Owner->GetActorLocation() + FVector(0.0, 0.0, 150.0);
	DrawDebugDirectionalArrow(World, Origin, Origin + GetForwardVector() * 250.0, 25.0f, FColor::Green, false, -1.0f, 0, 3.0f);
	if (FMath::Abs(CurrentSpeed) > 1.0f)
	{
		DrawDebugDirectionalArrow(World, Origin, Origin + GetTankVelocity().GetSafeNormal() * 200.0, 25.0f, FColor::Cyan, false, -1.0f, 0, 2.0f);
	}
	const FString Text = FString::Printf(
		TEXT("Raw (%.2f, %.2f)  Processed steer %.2f throttle %.2f\nSpeed %.0f / %.0f cm/s  Turn %.1f deg/s\nHeading %.1f  Move dir %s%s"),
		RawInput.X, RawInput.Y, ProcessedInput.X, ProcessedInput.Y, CurrentSpeed, TargetSpeed, CurrentTurnRate,
		Owner->GetActorRotation().Yaw, *GetTankVelocity().GetSafeNormal().ToCompactString(), bBlockedLastFrame ? TEXT("  BLOCKED") : TEXT(""));
	DrawDebugString(World, Origin + FVector(0.0, 0.0, 60.0), Text, nullptr, FColor::White, 0.0f, false, 1.1f);
}
