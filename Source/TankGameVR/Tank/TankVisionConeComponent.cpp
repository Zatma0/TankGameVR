#include "Tank/TankVisionConeComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Tank/TankVehicle.h"
#include "Camera/PlayerCameraManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDebugVisionCone(TEXT("Tank.Debug.VisionCone"), -1,
		TEXT("1: draw the player's vision cone, 0: never, -1: use the component's flag."));
}

UTankVisionConeComponent::UTankVisionConeComponent()
{
	// Ticks only to draw the debug cone.
	PrimaryComponentTick.bCanEverTick = true;
}

UTankVisionConeComponent* UTankVisionConeComponent::GetPlayerVisionCone(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (World)
	{
		for (TActorIterator<ATankVehicle> It(World); It; ++It)
		{
			return It->GetVisionCone();
		}
	}
	return nullptr;
}

void UTankVisionConeComponent::SetViewOverride(bool bEnable, FVector Location, FVector Direction)
{
	bViewOverride = bEnable;
	OverrideLocation = Location;
	OverrideDirection = Direction;
}

void UTankVisionConeComponent::GetView(FVector& OutLocation, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	// Without a camera (tests, a tank nobody sits in): the tank itself, looking ahead.
	OutLocation = Owner->GetActorLocation() + FVector(0.0, 0.0, 150.0);
	OutDirection = Owner->GetActorForwardVector();
	if (bViewOverride)
	{
		OutLocation = OverrideLocation;
		OutDirection = OverrideDirection;
	}
	else if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		// The headset in VR, the cockpit or overview camera on a monitor.
		OutLocation = Camera->GetCameraLocation();
		OutDirection = Camera->GetCameraRotation().Vector();
	}
	OutDirection = OutDirection.GetSafeNormal2D(UE_SMALL_NUMBER, Owner->GetActorForwardVector().GetSafeNormal2D(UE_SMALL_NUMBER, FVector::ForwardVector));
}

bool UTankVisionConeComponent::IsInCone(FVector Point) const
{
	FVector Eye;
	FVector Direction;
	GetView(Eye, Direction);
	const FVector To = (Point - Eye) * FVector(1.0, 1.0, 0.0);
	const double Distance = To.Size();
	if (Distance > Range)
	{
		return false;
	}
	// Right next to us counts as seen whichever way we look.
	if (Distance < 300.0)
	{
		return true;
	}
	const double Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To / Distance, Direction), -1.0, 1.0)));
	return Angle <= HalfAngle;
}

bool UTankVisionConeComponent::IsLineClear(const FVector& Eye, const FVector& Point, const AActor* Ignore) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TankVision), false, GetOwner());
	TArray<AActor*> Attached;
	GetOwner()->GetAttachedActors(Attached, true, true);
	Params.AddIgnoredActors(Attached);
	if (Ignore)
	{
		Params.AddIgnoredActor(Ignore);
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params) && Hit.Distance < FVector::Dist(Eye, Point) - 100.0)
	{
		return false;
	}
	return !ATankSmokeCloud::IsLineBlocked(World, Eye, Point);
}

bool UTankVisionConeComponent::CanSee(FVector Point) const
{
	if (!IsInCone(Point))
	{
		return false;
	}
	FVector Eye;
	FVector Direction;
	GetView(Eye, Direction);
	return !bCheckLineOfSight || IsLineClear(Eye, Point, nullptr);
}

bool UTankVisionConeComponent::CanSeeActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	FVector Middle;
	FVector Extent;
	Actor->GetActorBounds(true, Middle, Extent);
	if (!IsInCone(Middle))
	{
		return false;
	}
	FVector Eye;
	FVector Direction;
	GetView(Eye, Direction);
	return !bCheckLineOfSight || IsLineClear(Eye, Middle, Actor);
}

void UTankVisionConeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const int32 Forced = CVarDebugVisionCone.GetValueOnGameThread();
	if (Forced == 0 || (Forced < 0 && !bDebugDraw))
	{
		return;
	}
	FVector Eye;
	FVector Direction;
	GetView(Eye, Direction);
	// On the ground under the tank, 60 m out.
	const FVector Origin(Eye.X, Eye.Y, GetOwner()->GetActorLocation().Z - 60.0);
	const double Length = FMath::Min<double>(Range, 6000.0);
	const double Yaw = Direction.Rotation().Yaw;
	FVector Previous = Origin;
	for (int32 Step = 0; Step <= 16; ++Step)
	{
		const FVector Point = Origin + FRotator(0.0, Yaw - HalfAngle + 2.0 * HalfAngle * Step / 16.0, 0.0).Vector() * Length;
		DrawDebugLine(GetWorld(), Previous, Point, FColor::Yellow, false, -1.0f, 0, 6.0f);
		Previous = Point;
	}
	DrawDebugLine(GetWorld(), Previous, Origin, FColor::Yellow, false, -1.0f, 0, 6.0f);
}
