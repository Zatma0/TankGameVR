#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Engine/World.h"

UTargetRegistrySubsystem* UTargetRegistrySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTargetRegistrySubsystem>() : nullptr;
}

bool UTargetRegistrySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UTargetRegistrySubsystem::RegisterTarget(UTargetableComponent* Target)
{
	if (Target && !Targets.Contains(Target))
	{
		Targets.Add(Target);
		OnTargetAdded.Broadcast(Target);
	}
}

void UTargetRegistrySubsystem::UnregisterTarget(UTargetableComponent* Target)
{
	if (Targets.Remove(Target) > 0)
	{
		OnTargetRemoved.Broadcast(Target);
	}
}

void UTargetRegistrySubsystem::NotifyTargetStateChanged(UTargetableComponent* Target)
{
	if (Target && !Target->IsTargetable())
	{
		OnTargetRemoved.Broadcast(Target);
	}
	else if (Target)
	{
		OnTargetAdded.Broadcast(Target);
	}
}

void UTargetRegistrySubsystem::GetValidTargets(TArray<UTargetableComponent*>& OutTargets)
{
	OutTargets.Reset();
	Targets.RemoveAll([](const TWeakObjectPtr<UTargetableComponent>& Entry) { return !Entry.IsValid(); });
	for (const TWeakObjectPtr<UTargetableComponent>& Entry : Targets)
	{
		if (Entry->IsValidTarget())
		{
			OutTargets.Add(Entry.Get());
		}
	}
}

int32 UTargetRegistrySubsystem::GetValidTargetCount()
{
	TArray<UTargetableComponent*> Valid;
	GetValidTargets(Valid);
	return Valid.Num();
}
