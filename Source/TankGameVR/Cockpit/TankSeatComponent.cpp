#include "Cockpit/TankSeatComponent.h"
#include "TankGameVR.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Camera/CameraComponent.h"
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
		// The pawn may spawn after us.
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
	USceneComponent* PawnRoot = Pawn ? Pawn->GetRootComponent() : nullptr;
	const UCameraComponent* Camera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (!PawnRoot || !Camera)
	{
		return;
	}
	// Without a headset the camera sits at the tracking origin, so this also works on desktop.
	const float EyeHeight = static_cast<float>(FVector::DotProduct(Camera->GetComponentLocation() - GetComponentLocation(), GetUpVector()));
	const float CurrentOffset = static_cast<float>(PawnRoot->GetRelativeLocation().Z);
	const float Offset = FMath::Clamp(CurrentOffset + DesignEyeHeight - EyeHeight, -MaxCalibrationOffset, MaxCalibrationOffset);
	PawnRoot->SetRelativeLocation(FVector(0.0, 0.0, Offset));
	UE_LOG(LogTankGame, Log, TEXT("Seat calibration: eye %.1f cm above seat (headset %s), design %.1f cm, rider offset %.1f cm"),
		EyeHeight, UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() ? TEXT("on") : TEXT("off"), DesignEyeHeight, Offset);
}
