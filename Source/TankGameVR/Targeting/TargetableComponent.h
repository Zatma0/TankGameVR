#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetableComponent.generated.h"

/**
 * Add to any actor that the tank may lock on to (enemy tanks, turrets, ...). The actor's look does not matter.
 * Registers itself with UTargetRegistrySubsystem while playing; destroyed or disabled targets drop out automatically.
 */
UCLASS(ClassGroup = (Targeting), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTargetableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetableComponent();

	/** Offset (actor space) of the point the crosshair locks onto, e.g. the turret centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector TargetPointOffset = FVector(0.0, 0.0, 120.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FText DisplayName;

	/** Set false when the target is dead/disabled but the actor still exists (e.g. a burning wreck). */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetTargetable(bool bInTargetable);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsTargetable() const { return bTargetable; }

	/** Alive, enabled and in play. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsValidTarget() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetTargetLocation() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FText GetTargetDisplayName() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, Category = "Targeting")
	bool bTargetable = true;

private:
	bool bRegistered = false;
};
