#include "Testing/TestTargetMoverComponent.h"
#include "GameFramework/Actor.h"

UTestTargetMoverComponent::UTestTargetMoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTestTargetMoverComponent::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetOwner()->GetActorLocation();
	// Different phase per actor so a wave does not move in lockstep.
	Phase = FMath::Frac(GetOwner()->GetUniqueID() * 0.618f) * UE_TWO_PI;
}

void UTestTargetMoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Elapsed += DeltaTime;
	const float Offset = Amplitude * FMath::Sin(Phase + Elapsed * CyclesPerMinute / 60.0f * UE_TWO_PI);
	GetOwner()->SetActorLocation(StartLocation + GetOwner()->GetActorRightVector() * Offset);
}
