#include "Cockpit/TankSeatComponent.h"
#include "TankGameVR.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

UTankSeatComponent::UTankSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
}

void UTankSeatComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoBoardLocalPlayer)
	{
		// The player pawn may be spawned after this actor; board on the next tick.
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UTankSeatComponent::BoardLocalPlayer);
	}
}

void UTankSeatComponent::BoardLocalPlayer()
{
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Board(Pawn);
	}
}

bool UTankSeatComponent::Board(APawn* Pawn)
{
	if (!IsValid(Pawn) || Pawn == GetOwner())
	{
		return false;
	}
	Rider = Pawn;
	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
	Pawn->AttachToComponent(this, Rules);
	UE_LOG(LogTankGame, Log, TEXT("%s boarded %s"), *Pawn->GetName(), *GetNameSafe(GetOwner()));
	OnBoarded.Broadcast(Pawn);

	if (bCalibrateEyeHeight)
	{
		GetWorld()->GetTimerManager().SetTimer(CalibrationTimer, this, &UTankSeatComponent::CalibrateEyeHeight, FMath::Max(CalibrationDelay, 0.01f), false);
	}
	return true;
}

void UTankSeatComponent::CalibrateEyeHeight()
{
	APawn* Pawn = Rider.Get();
	if (!Pawn || !UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled())
	{
		return;
	}
	FRotator Orientation;
	FVector Position;
	UHeadMountedDisplayFunctionLibrary::GetOrientationAndPosition(Orientation, Position);
	// With a floor-level tracking origin Position.Z is the real head height; move the rider so it matches the cockpit.
	const float Offset = FMath::Clamp(DesignEyeHeight - static_cast<float>(Position.Z), -MaxCalibrationOffset, MaxCalibrationOffset);
	if (USceneComponent* PawnRoot = Pawn->GetRootComponent())
	{
		PawnRoot->SetRelativeLocation(FVector(0.0, 0.0, Offset));
	}
	UE_LOG(LogTankGame, Log, TEXT("Seat calibration: head %.1f cm, design eye %.1f cm, offset %.1f cm"), Position.Z, DesignEyeHeight, Offset);
}
