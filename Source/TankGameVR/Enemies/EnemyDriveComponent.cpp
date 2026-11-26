#include "Enemies/EnemyDriveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

namespace
{
	// Every enemy vehicle, for keeping apart.
	TArray<TWeakObjectPtr<UEnemyDriveComponent>> GDrivers;
}

UEnemyDriveComponent::UEnemyDriveComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UEnemyDriveComponent::BeginPlay()
{
	Super::BeginPlay();
	GDrivers.Add(this);
	FollowGround(0.0f);
}

void UEnemyDriveComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GDrivers.RemoveAll([this](const TWeakObjectPtr<UEnemyDriveComponent>& Driver) { return !Driver.IsValid() || Driver.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

void UEnemyDriveComponent::MoveTo(FVector InGoal, float InAcceptRadius, float InSpeedScale)
{
	const bool bNewGoal = !bHasGoal || FVector::Dist2D(InGoal, Goal) > 250.0;
	Goal = InGoal;
	AcceptRadius = FMath::Max(50.0f, InAcceptRadius);
	SpeedScale = FMath::Clamp(InSpeedScale, 0.0f, 1.0f);
	bHasGoal = true;
	bFacing = false;
	if (bNewGoal)
	{
		bArrived = false;
		RepathTimer = 0.0f;
	}
}

void UEnemyDriveComponent::Stop()
{
	bHasGoal = false;
	Path.Reset();
}

void UEnemyDriveComponent::Freeze()
{
	Stop();
	bFacing = false;
	CurrentSpeed = 0.0f;
	Velocity = FVector::ZeroVector;
	RecoverTime = 0.0f;
}

void UEnemyDriveComponent::Teleport(FVector Location, FRotator Rotation)
{
	Freeze();
	bHasGround = false;
	bStepBlocked = false;
	bArrived = false;
	StuckTime = 0.0f;
	DetourSide = 0;
	GetOwner()->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	FollowGround(0.0f);
}

void UEnemyDriveComponent::FaceTowards(FVector Point)
{
	FacePoint = Point;
	bFacing = true;
}

void UEnemyDriveComponent::UpdatePath()
{
	Path.Reset();
	PathIndex = 0;
	bNavPath = false;
	const FVector Start = GetOwner()->GetActorLocation();
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		// Ask for the vehicle-sized nav mesh if the project has one, the default one otherwise.
		FNavAgentProperties Agent;
		Agent.AgentRadius = HalfWidth + 40.0f;
		Agent.AgentHeight = 250.0f;
		if (const ANavigationData* NavData = Nav->GetNavDataForProps(Agent))
		{
			FPathFindingQuery Query(GetOwner(), *NavData, Start, Goal);
			const FPathFindingResult Result = Nav->FindPathSync(Agent, Query);
			if (Result.IsSuccessful() && Result.Path.IsValid() && Result.Path->GetPathPoints().Num() > 1)
			{
				for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
				{
					Path.Add(Point.Location);
				}
				PathIndex = 1;
				bNavPath = true;
				return;
			}
		}
	}
	Path = { Start, Goal };
	PathIndex = 1;
}

FVector UEnemyDriveComponent::GetSteerTarget()
{
	const FVector Location = GetOwner()->GetActorLocation();
	// Skip corners we are already at.
	while (PathIndex < Path.Num() - 1 && FVector::Dist2D(Location, Path[PathIndex]) < FMath::Max(250.0f, HalfWidth * 1.5f))
	{
		++PathIndex;
	}
	return Path.IsValidIndex(PathIndex) ? Path[PathIndex] : Goal;
}

FVector UEnemyDriveComponent::Avoid(const FVector& Desired)
{
	const AActor* Owner = GetOwner();
	const FVector Location = Owner->GetActorLocation();
	FVector Push = FVector::ZeroVector;
	const double WallZ = FMath::Cos(FMath::DegreesToRadians(MaxClimbAngle));

	// No nav mesh and a building dead ahead: go round it, keeping to the side we picked until the way is clear.
	if (!bNavPath && FeelerLength > 0.0f && !Desired.IsNearlyZero())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now > DetourUntil)
		{
			DetourSide = 0;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyDetour), false, Owner);
		const FVector Start = Location + FVector(0, 0, 150);
		FHitResult Hit;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, Start + Desired * (FeelerLength * 1.5f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(40.0f), Params)
			&& !Hit.bStartPenetrating && Hit.ImpactNormal.Z < WallZ && Hit.GetComponent() && Hit.GetComponent()->Mobility != EComponentMobility::Movable)
		{
			const FVector Away = Hit.ImpactNormal.GetSafeNormal2D();
			FVector Along = FVector::CrossProduct(FVector::UpVector, Away);
			if (DetourSide == 0)
			{
				DetourSide = FVector::DotProduct(Along, Owner->GetActorForwardVector()) >= 0.0 ? 1 : -1;
			}
			DetourUntil = Now + 2.0;
			return (Along * DetourSide + Away * (0.5 * (1.0 - Hit.Time))).GetSafeNormal2D();
		}
	}

	// Keep apart from the others.
	for (const TWeakObjectPtr<UEnemyDriveComponent>& Other : GDrivers)
	{
		if (!Other.IsValid() || Other.Get() == this || Other->GetWorld() != GetWorld())
		{
			continue;
		}
		const FVector Away = (Location - Other->GetOwner()->GetActorLocation()) * FVector(1, 1, 0);
		const double Distance = Away.Size();
		if (Distance > KINDA_SMALL_NUMBER && Distance < SeparationRadius)
		{
			Push += Away / Distance * (1.0 - Distance / SeparationRadius) * 1.5;
		}
	}

	// Feelers left and right of the nose; steer away from whatever they touch.
	if (FeelerLength > 0.0f && CurrentSpeed > 20.0f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyFeeler), false, Owner);
		const FVector Forward = Owner->GetActorForwardVector();
		const FVector Right = Owner->GetActorRightVector();
		const float Length = FeelerLength * FMath::Clamp(CurrentSpeed / FMath::Max(MaxSpeed, 1.0f), 0.4f, 1.0f);
		for (const float Side : { -1.0f, 1.0f })
		{
			const FVector Start = Location + Right * (HalfWidth * Side) + FVector(0, 0, 40);
			const FVector Dir = (Forward + Right * (0.35f * Side)).GetSafeNormal();
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * Length, ECC_Visibility, Params) && Hit.ImpactNormal.Z < WallZ)
			{
				Push -= Right * Side * (1.0f - Hit.Time) * 2.0f;
			}
		}
	}
	return (Desired + Push).GetSafeNormal2D();
}

void UEnemyDriveComponent::Drive(float DeltaTime, float TargetYaw, float TargetSpeed)
{
	AActor* Owner = GetOwner();
	FRotator Rotation = Owner->GetActorRotation();
	const float Error = FMath::FindDeltaAngleDegrees(Rotation.Yaw, TargetYaw);

	// Wheels turn with speed; tracks turn anyway.
	float Rate = TurnRate;
	if (!bCanPivot)
	{
		Rate *= FMath::Clamp(FMath::Abs(CurrentSpeed) / 250.0f, 0.0f, 1.0f);
	}
	Rotation.Yaw += FMath::Clamp(Error, -Rate * DeltaTime, Rate * DeltaTime);

	// Slow down for sharp turns.
	const float Turn = FMath::Abs(Error);
	if (bCanPivot)
	{
		TargetSpeed *= FMath::Clamp(1.0f - Turn / PivotAngle, 0.0f, 1.0f);
	}
	else
	{
		TargetSpeed *= FMath::Lerp(1.0f, 0.45f, FMath::Clamp(Turn / 90.0f, 0.0f, 1.0f));
	}

	const float Rate2 = (FMath::Abs(TargetSpeed) > FMath::Abs(CurrentSpeed) && FMath::Sign(TargetSpeed) == FMath::Sign(CurrentSpeed)) ? Acceleration : Braking;
	CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Rate2);

	const FVector Start = Owner->GetActorLocation();
	PreMoveLocation = Start;
	Owner->SetActorRotation(FRotator(Rotation.Pitch, Rotation.Yaw, Rotation.Roll));
	const FVector Delta = Owner->GetActorForwardVector().GetSafeNormal2D() * CurrentSpeed * DeltaTime;
	FHitResult Hit;
	Owner->AddActorWorldOffset(Delta, true, &Hit);
	if (Hit.bBlockingHit)
	{
		const FVector Rest = Delta * (1.0f - Hit.Time);
		if (Hit.ImpactNormal.Z >= FMath::Cos(FMath::DegreesToRadians(MaxClimbAngle)))
		{
			// Only the hill we are climbing; FollowGround lifts us onto it.
			Owner->AddActorWorldOffset(Rest, false);
		}
		else
		{
			// Slide along what we hit.
			Owner->AddActorWorldOffset(FVector::VectorPlaneProject(Rest, Hit.Normal) * FVector(1, 1, 0), true);
		}
	}
	const FVector Moved = Owner->GetActorLocation() - Start;
	Velocity = DeltaTime > 0.0f ? Moved / DeltaTime : FVector::ZeroVector;

	// Wanted to move but barely did: count as stuck.
	const bool bWantsToMove = FMath::Abs(TargetSpeed) > 50.0f || FMath::Abs(CurrentSpeed) > 50.0f;
	const bool bWasStepBlocked = bStepBlocked;
	bStepBlocked = false;
	if (bWantsToMove && (bWasStepBlocked || Moved.Size2D() < FMath::Abs(CurrentSpeed) * DeltaTime * 0.25f + 0.05f) && RecoverTime <= 0.0f)
	{
		StuckTime += DeltaTime;
	}
	else
	{
		StuckTime = FMath::Max(0.0f, StuckTime - DeltaTime);
	}
}

void UEnemyDriveComponent::FollowGround(float DeltaTime)
{
	AActor* Owner = GetOwner();
	const FVector Location = Owner->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyGround), false, Owner);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 150), Location - FVector(0, 0, 1000), Objects, Params))
	{
		const double Rise = Hit.ImpactPoint.Z - GroundZ;
		const bool bTooSteep = Hit.ImpactNormal.Z < FMath::Cos(FMath::DegreesToRadians(MaxClimbAngle)) && Rise > 0.1 * FVector::Dist2D(Location, PreMoveLocation);
		if (bHasGround && DeltaTime > 0.0f && (Rise > MaxStepHeight || bTooSteep))
		{
			// Too high or too steep to climb (rubble, rocks, a cliff): treat it as a wall.
			Owner->SetActorLocation(PreMoveLocation);
			Velocity = FVector::ZeroVector;
			bStepBlocked = true;
			return;
		}
		GroundZ = static_cast<float>(Hit.ImpactPoint.Z);
		bHasGround = true;
		FVector NewLocation = Location;
		NewLocation.Z = Hit.ImpactPoint.Z + RideHeight;
		// Lean with the slope a little, smoothly.
		const FRotator Rotation = Owner->GetActorRotation();
		const FVector Forward = FVector::VectorPlaneProject(FRotator(0, Rotation.Yaw, 0).Vector(), Hit.ImpactNormal).GetSafeNormal();
		const FRotator Slope = FRotationMatrix::MakeFromXZ(Forward, Hit.ImpactNormal).Rotator();
		const float Alpha = DeltaTime > 0.0f ? FMath::Clamp(DeltaTime * 4.0f, 0.0f, 1.0f) : 1.0f;
		const float Pitch = FMath::Clamp(static_cast<float>(Slope.Pitch), -MaxTilt, MaxTilt);
		const float Roll = FMath::Clamp(static_cast<float>(Slope.Roll), -MaxTilt, MaxTilt);
		const FRotator Smoothed(FMath::Lerp(static_cast<float>(Rotation.Pitch), Pitch, Alpha), Rotation.Yaw, FMath::Lerp(static_cast<float>(Rotation.Roll), Roll, Alpha));
		Owner->SetActorLocationAndRotation(NewLocation, Smoothed);
	}
}

void UEnemyDriveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (DeltaTime <= 0.0f)
	{
		return;
	}
	AActor* Owner = GetOwner();
	const FVector Location = Owner->GetActorLocation();
	const float Yaw = Owner->GetActorRotation().Yaw;

	// Backing out of a dead end: reverse while turning away.
	if (RecoverTime > 0.0f)
	{
		RecoverTime -= DeltaTime;
		Drive(DeltaTime, Yaw + RecoverTurn * 60.0f, -MaxSpeed * 0.5f);
		FollowGround(DeltaTime);
		if (RecoverTime <= 0.0f)
		{
			RepathTimer = 0.0f;
		}
		return;
	}
	if (StuckTime > 1.2f)
	{
		StuckTime = 0.0f;
		RecoverTime = 1.6f;
		RecoverTurn = FMath::RandBool() ? 1.0f : -1.0f;
		// Going round that way did not work; try the other.
		DetourSide = -DetourSide;
		CurrentSpeed = 0.0f;
		return;
	}

	float TargetYaw = Yaw;
	float TargetSpeed = 0.0f;
	if (bHasGoal)
	{
		RepathTimer -= DeltaTime;
		if (RepathTimer <= 0.0f || Path.Num() == 0)
		{
			RepathTimer = RepathInterval;
			UpdatePath();
		}
		const double ToGoal = FVector::Dist2D(Location, Goal);
		bArrived = ToGoal <= AcceptRadius;
		if (!bArrived)
		{
			const FVector Steer = Avoid((GetSteerTarget() - Location).GetSafeNormal2D());
			TargetYaw = Steer.Rotation().Yaw;
			// Brake in time to stop near the goal.
			const float StopDistance = CurrentSpeed * CurrentSpeed / (2.0f * Braking) + AcceptRadius * 0.5f;
			const float Approach = FMath::Clamp(static_cast<float>(ToGoal) / FMath::Max(StopDistance, 1.0f), 0.25f, 1.0f);
			TargetSpeed = MaxSpeed * SpeedScale * Approach;
		}
	}
	else if (bFacing)
	{
		const FVector To = (FacePoint - Location).GetSafeNormal2D();
		if (!To.IsNearlyZero())
		{
			TargetYaw = To.Rotation().Yaw;
		}
	}

	// Without a goal only the facing turn happens (tracked vehicles pivot, wheels creep).
	if (!bHasGoal && bFacing && !bCanPivot && FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, TargetYaw)) > 10.0f)
	{
		TargetSpeed = 120.0f;
	}
	Drive(DeltaTime, TargetYaw, TargetSpeed);
	FollowGround(DeltaTime);
}
