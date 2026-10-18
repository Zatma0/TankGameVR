#include "Targeting/TargetableComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTargetableComponent::UTargetableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTargetableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this))
	{
		Registry->RegisterTarget(this);
		bRegistered = true;
	}
}

void UTargetableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bRegistered)
	{
		if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this))
		{
			Registry->UnregisterTarget(this);
		}
		bRegistered = false;
	}
	Super::EndPlay(EndPlayReason);
}

void UTargetableComponent::SetTargetable(bool bInTargetable)
{
	if (bTargetable == bInTargetable)
	{
		return;
	}
	bTargetable = bInTargetable;
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(this))
	{
		Registry->NotifyTargetStateChanged(this);
	}
}

bool UTargetableComponent::IsValidTarget() const
{
	const AActor* Owner = GetOwner();
	return bTargetable && bRegistered && IsValid(Owner) && !Owner->IsActorBeingDestroyed() && Owner->HasActorBegunPlay();
}

FVector UTargetableComponent::GetTargetLocation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform().TransformPosition(TargetPointOffset) : FVector::ZeroVector;
}

FText UTargetableComponent::GetTargetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	const AActor* Owner = GetOwner();
#if WITH_EDITOR
	return FText::FromString(Owner ? Owner->GetActorLabel() : FString());
#else
	return FText::FromString(GetNameSafe(Owner));
#endif
}
