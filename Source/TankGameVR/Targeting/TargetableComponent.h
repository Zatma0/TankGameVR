#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetableComponent.generated.h"

// Add to any actor the tank can lock on to.
UCLASS(ClassGroup = (Targeting), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTargetableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetableComponent();

	/** Point the crosshair locks onto, in actor space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector TargetPointOffset = FVector(0.0, 0.0, 120.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FText DisplayName;

	/** e.g. false for a wreck that should stay in the level. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetTargetable(bool bInTargetable);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsTargetable() const { return bTargetable; }

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
