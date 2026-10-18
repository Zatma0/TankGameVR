#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TargetRegistrySubsystem.generated.h"

class UTargetableComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTargetRegistryChanged, UTargetableComponent*);

// Targets register themselves here, so nothing has to search all actors every frame.
UCLASS()
class TANKGAMEVR_API UTargetRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UTargetRegistrySubsystem* Get(const UObject* WorldContext);

	void RegisterTarget(UTargetableComponent* Target);
	void UnregisterTarget(UTargetableComponent* Target);
	void NotifyTargetStateChanged(UTargetableComponent* Target);

	void GetValidTargets(TArray<UTargetableComponent*>& OutTargets);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	int32 GetValidTargetCount();

	FOnTargetRegistryChanged OnTargetRemoved;
	FOnTargetRegistryChanged OnTargetAdded;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TArray<TWeakObjectPtr<UTargetableComponent>> Targets;
};
