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
	AActor* Owner = GetOwner();
	if (!bFollowGround || !Owner || !GetWorld())
	{
		return;
	}
	if (RideHeight <= 0.0f)
	{
		// Use the height the tank was placed at.
		UpdateGroundIgnores();
		const FVector Start = Owner->GetActorLocation();
		float GroundZ = 0.0f;
		if (TraceGround(Start, 0.0f, 5000.0f, GroundZ))
		{
			RideHeight = Start.Z - GroundZ;
		}
	}
	SnapToGround();
}

void UTankMovementComponent::SnapToGround()
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root || !GetWorld() || !bFollowGround || RideHeight <= 0.0f)
	{
		return;
	}
	UpdateGroundIgnores();
	FVector Location = Owner->GetActorLocation();
	float GroundZ = 0.0f;
	if (!TraceGround(Location, 200.0f, 5000.0f, GroundZ))
	{
		return;
	}
	const float Yaw = GetHeadingYaw();
	Location.Z = GroundZ + RideHeight;
	const FGroundPose Pose = SampleGround(Location, Yaw);
	if (Pose.bValid)
	{
		Location.Z = Pose.Z + RideHeight;
		GroundNormal = SmoothedNormal = Pose.Normal;
		GroundSlope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Pose.Normal.Z, -1.0, 1.0)));
	}
	Root->SetWorldLocationAndRotation(Location, MakeHullRotation(Yaw, bTiltWithGround ? SmoothedNormal : FVector::UpVector), false, nullptr, ETeleportType::TeleportPhysics);
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
	// Avoid big jumps after a hitch.
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	// Stick shaping
	SmoothedInput = UTankMovementMath::SmoothStick(SmoothedInput, RawInput, Dt, Tuning.JoystickSmoothing);
	LimitedInput = UTankMovementMath::RateLimit(LimitedInput, SmoothedInput, Tuning.InputResponseSpeed, Dt);
	ProcessedInput.X = UTankMovementMath::ShapeAxis(LimitedInput.X, Tuning.SteeringDeadZone, Tuning.SteeringCurve, Tuning.SteeringSensitivity);
	ProcessedInput.Y = UTankMovementMath::ShapeAxis(LimitedInput.Y, Tuning.ThrottleDeadZone, Tuning.ThrottleCurve, 1.0f);

	// Target speed / turn rate, then accelerate toward them
	TargetSpeed = UTankMovementMath::ComputeTargetSpeed(Tuning, ProcessedInput.X, ProcessedInput.Y);
	if (bFollowGround && SteepClimbSpeedScale < 1.0f && !FMath::IsNearlyZero(TargetSpeed))
	{
		// Slower uphill.
		const FVector Travel = GetForwardVector() * FMath::Sign(TargetSpeed);
		const double Grade = -FVector::DotProduct(Travel, GroundNormal) / FMath::Max(GroundNormal.Z, 0.1);
		if (Grade > 0.0)
		{
			const float Climb = FMath::RadiansToDegrees(FMath::Atan(Grade));
			TargetSpeed *= FMath::Lerp(1.0f, SteepClimbSpeedScale, FMath::Clamp(Climb / MaxClimbAngle, 0.0f, 1.0f));
		}
	}
	CurrentSpeed = UTankMovementMath::StepSpeed(Tuning, CurrentSpeed, TargetSpeed, Dt);
	const float TargetTurnRate = UTankMovementMath::ComputeTargetTurnRate(Tuning, ProcessedInput.X, CurrentSpeed);
	CurrentTurnRate = FMath::FInterpConstantTo(CurrentTurnRate, TargetTurnRate, Dt, Tuning.MaximumTurnAcceleration);

	// Move along the mid-step heading (keeps curves the same at any frame rate).
	const float YawDelta = CurrentTurnRate * Dt;
	const float Heading = GetHeadingYaw();
	const FVector Location = Root->GetComponentLocation();
	const FVector Delta = FRotator(0.0f, Heading + YawDelta * 0.5f, 0.0f).Vector() * CurrentSpeed * Dt;

	UPrimitiveComponent* Hull = Cast<UPrimitiveComponent>(Root);
	ReleaseGhostedActors(Hull);

	FVector TargetLocation = Location + Delta;
	FQuat TargetRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDelta)) * Root->GetComponentQuat();
	bool bGroundBlocked = false;
	const bool bOnGround = bFollowGround && RideHeight > 0.0f;
	if (bOnGround)
	{
		UpdateGroundIgnores();
		const float GroundZ = Location.Z - RideHeight;
		FGroundPose Pose = SampleGround(TargetLocation, Heading + YawDelta);
		const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Pose.Normal.Z, -1.0, 1.0)));
		if (!Delta.IsNearlyZero())
		{
			// No ground, a ledge, or a climb that is too steep: stay where we are (turning still works).
			const bool bTooSteep = Slope > MaxClimbAngle && Pose.Z - GroundZ > 0.1 * Delta.Size();
			bGroundBlocked = !Pose.bValid || bTooSteep || FMath::Abs(Pose.Z - GroundZ) > MaxGroundStep;
		}
		if (bGroundBlocked || !Pose.bValid)
		{
			TargetLocation = Location;
			Pose = SampleGround(Location, Heading + YawDelta);
		}
		if (Pose.bValid)
		{
			TargetLocation.Z = Pose.Z + RideHeight;
			GroundNormal = Pose.Normal;
			GroundSlope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Pose.Normal.Z, -1.0, 1.0)));
			const float Alpha = TiltSpeed > 0.0f ? 1.0f - FMath::Exp(-TiltSpeed * Dt) : 1.0f;
			SmoothedNormal = FMath::Lerp(SmoothedNormal, GroundNormal, Alpha).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		}
		TargetRotation = MakeHullRotation(Heading + YawDelta, bTiltWithGround ? SmoothedNormal : FVector::UpVector);
	}

	const bool bHitWall = SweepTo(TargetLocation, TargetRotation);
	if (bHitWall && bOnGround)
	{
		// Stopped short of where the ground was measured.
		FVector Stopped = Root->GetComponentLocation();
		const FGroundPose Pose = SampleGround(Stopped, Heading + YawDelta);
		if (Pose.bValid && !FMath::IsNearlyEqual(Stopped.Z, Pose.Z + RideHeight, 0.05))
		{
			Stopped.Z = Pose.Z + RideHeight;
			Root->SetWorldLocation(Stopped, false, nullptr, ETeleportType::None);
		}
	}
	bBlockedLastFrame = (bHitWall || bGroundBlocked) && !Delta.IsNearlyZero();
	if (bBlockedLastFrame)
	{
		// Hit something: stop, don't slide along it.
		CurrentSpeed = 0.0f;
	}

	if (TankDebug::IsMovementDebugEnabled(bDebugDraw))
	{
		DrawDebug();
	}
}

bool UTankMovementComponent::SweepTo(const FVector& TargetLocation, const FQuat& TargetRotation)
{
	AActor* Owner = GetOwner();
	USceneComponent* Root = Owner->GetRootComponent();
	UPrimitiveComponent* Hull = Cast<UPrimitiveComponent>(Root);
	const bool bOnGround = bFollowGround && RideHeight > 0.0f;
	const float WalkableZ = FMath::Cos(FMath::DegreesToRadians(FMath::Min(MaxClimbAngle + 8.0f, 80.0f)));

	TArray<UPrimitiveComponent*, TInlineAllocator<4>> Ground;
	bool bBlocked = false;
	for (int32 Attempt = 0; Attempt < 6; ++Attempt)
	{
		FHitResult Hit;
		Root->MoveComponent(TargetLocation - Root->GetComponentLocation(), TargetRotation, true, &Hit, MOVECOMP_NoFlags, ETeleportType::None);
		if (!Hit.bBlockingHit)
		{
			break;
		}
		AActor* Other = Hit.GetActor();
		UPrimitiveComponent* OtherPart = Hit.GetComponent();
		const bool bMovable = OtherPart && OtherPart->Mobility == EComponentMobility::Movable;
		if (Hit.bStartPenetrating && bMovable && Hull && Other && Other != Owner)
		{
			// Something moved into us (e.g. an enemy spawned here). Shallow: push out. Deep: ignore it until we're out.
			FVector PushOut = Hit.Normal * (Hit.PenetrationDepth + 1.0);
			PushOut.Z = 0.0;
			if (Hit.PenetrationDepth > 0.0f && Hit.PenetrationDepth < 50.0f && !PushOut.IsNearlyZero())
			{
				Root->MoveComponent(PushOut, Root->GetComponentQuat(), false, nullptr, MOVECOMP_NoFlags, ETeleportType::None);
			}
			else
			{
				Hull->IgnoreActorWhenMoving(Other, true);
				GhostedActors.AddUnique(Other);
			}
			continue;
		}
		const double Up = Hit.bStartPenetrating ? Hit.Normal.Z : Hit.ImpactNormal.Z;
		if (bOnGround && Hull && OtherPart && Up >= WalkableZ && !Ground.Contains(OtherPart))
		{
			// The hull touched the ground itself; the ground checks decide about that, not the sweep.
			Hull->IgnoreComponentWhenMoving(OtherPart, true);
			Ground.Add(OtherPart);
			continue;
		}
		if (Hit.bStartPenetrating)
		{
			// Inside a wall (turning swung a corner into it): get pushed back out, never through.
			FVector PushOut = Hit.Normal * (Hit.PenetrationDepth + 1.0);
			PushOut.Z = 0.0;
			if (Hit.PenetrationDepth > 0.0f && !PushOut.IsNearlyZero())
			{
				Root->MoveComponent(PushOut, Root->GetComponentQuat(), false, nullptr, MOVECOMP_NoFlags, ETeleportType::None);
			}
		}
		bBlocked = true;
		break;
	}
	for (UPrimitiveComponent* Part : Ground)
	{
		Hull->IgnoreComponentWhenMoving(Part, false);
	}
	return bBlocked;
}

void UTankMovementComponent::ReleaseGhostedActors(UPrimitiveComponent* Hull)
{
	for (int32 Index = GhostedActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* Other = GhostedActors[Index].Get();
		bool bStillInside = false;
		if (Other && Hull)
		{
			// Bounding boxes: a bit conservative when turned, which is fine here.
			const FBox HullBox = Hull->Bounds.GetBox();
			TArray<UPrimitiveComponent*> Parts;
			Other->GetComponents(Parts);
			for (UPrimitiveComponent* Part : Parts)
			{
				if (Part->IsCollisionEnabled() && HullBox.Intersect(Part->Bounds.GetBox()))
				{
					bStillInside = true;
					break;
				}
			}
		}
		if (!bStillInside)
		{
			if (Other && Hull)
			{
				Hull->IgnoreActorWhenMoving(Other, false);
			}
			GhostedActors.RemoveAt(Index);
		}
	}
}

void UTankMovementComponent::UpdateGroundIgnores()
{
	GroundIgnores.Reset();
	if (AActor* Owner = GetOwner())
	{
		Owner->GetAttachedActors(GroundIgnores, true, true);
		GroundIgnores.Add(Owner);
	}
	for (const TWeakObjectPtr<AActor>& Ghost : GhostedActors)
	{
		if (Ghost.IsValid())
		{
			GroundIgnores.Add(Ghost.Get());
		}
	}
}

bool UTankMovementComponent::TraceGround(const FVector& Point, float Up, float Down, float& OutZ) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankGround), false);
	Params.AddIgnoredActors(GroundIgnores);
	const FVector Start = Point + FVector(0.0, 0.0, Up);
	const FVector End = Point - FVector(0.0, 0.0, Down);
	for (int32 Attempt = 0; Attempt < 5; ++Attempt)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel, Params))
		{
			return false;
		}
		const UPrimitiveComponent* Part = Hit.GetComponent();
		if (!Part)
		{
			return false;
		}
		// Ground is part of the level. Vehicles, wrecks and whatever we started inside are not.
		if (!Hit.bStartPenetrating && Part->Mobility != EComponentMobility::Movable)
		{
			OutZ = static_cast<float>(Hit.ImpactPoint.Z);
			return true;
		}
		Params.AddIgnoredComponent(Part);
	}
	return false;
}

UTankMovementComponent::FGroundPose UTankMovementComponent::SampleGround(const FVector& Center, float HeadingYaw) const
{
	FGroundPose Pose;
	const FVector Ground(Center.X, Center.Y, Center.Z - RideHeight);
	float CenterZ = 0.0f;
	if (!TraceGround(Ground, MaxGroundStep, MaxGroundStep, CenterZ))
	{
		return Pose;
	}

	// The four track ends. They may be higher or lower than the middle by as much as a steep slope allows.
	const FVector Forward = FRotator(0.0f, HeadingYaw, 0.0f).Vector();
	const FVector Right(-Forward.Y, Forward.X, 0.0);
	const float Reach = MaxGroundStep + TrackContact.Size() * FMath::Tan(FMath::DegreesToRadians(FMath::Min(MaxClimbAngle + 15.0f, 70.0f)));
	FVector Corners[4];
	double Sum = 0.0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const double Front = (Index < 2) ? 1.0 : -1.0;
		const double Side = (Index % 2 == 0) ? -1.0 : 1.0;
		Corners[Index] = FVector(Ground.X, Ground.Y, CenterZ) + Forward * (TrackContact.X * Front) + Right * (TrackContact.Y * Side);
		float Z = CenterZ;
		if (TraceGround(Corners[Index], Reach, Reach, Z))
		{
			Corners[Index].Z = Z;
		}
		Sum += Corners[Index].Z;
	}
	// Front-left, front-right, back-left, back-right.
	FVector Normal = FVector::CrossProduct(Corners[1] - Corners[2], Corners[0] - Corners[3]).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (Normal.Z < 0.0)
	{
		Normal = -Normal;
	}
	Pose.bValid = true;
	Pose.Normal = Normal;
	// On a crest the belly rests on it; in a dip the track ends carry the tank.
	Pose.Z = FMath::Max(CenterZ, static_cast<float>(Sum / 4.0));
	return Pose;
}

FQuat UTankMovementComponent::MakeHullRotation(float HeadingYaw, const FVector& Normal) const
{
	// Lay the hull on the ground plane without changing where it points on the map.
	const FVector Axis = LocalForwardAxis.GetSafeNormal2D();
	const float AxisYaw = Axis.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
	FVector X = FRotator(0.0f, HeadingYaw - AxisYaw, 0.0f).Vector();
	X.Z = -FVector::DotProduct(X, Normal) / FMath::Max(Normal.Z, 0.1);
	return FRotationMatrix::MakeFromXZ(X, Normal).ToQuat();
}

float UTankMovementComponent::GetHeadingYaw() const
{
	const FVector Forward = GetForwardVector();
	return FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
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
