#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TargetRegistrySubsystem.generated.h"

class UTargetableComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetRegistryChanged, UTargetableComponent*);

/**
 * Per-world list of targetable components. Targets add/remove themselves (BeginPlay/EndPlay), so nobody has to
 * search all actors every frame, and wave-spawned enemies become available the moment they begin play.
 * Only weak references are stored; stale entries are pruned on access.
 */
UCLASS()
class TANKGAMEVR_API UTargetRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTargetRegistrySubsystem* Get(const UObject* WorldContext);

	void RegisterTarget(UTargetableComponent* Target);
	void UnregisterTarget(UTargetableComponent* Target);
	void NotifyTargetStateChanged(UTargetableComponent* Target);

	/** All currently valid targets (alive, targetable, in play). */
	void GetValidTargets(TArray<UTargetableComponent*>& OutTargets);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	int32 GetValidTargetCount();

	/** Fired when a target leaves play or becomes non-targetable. */
	FOnTargetRegistryChanged OnTargetRemoved;
	FOnTargetRegistryChanged OnTargetAdded;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UTargetableComponent>> Targets;
};
