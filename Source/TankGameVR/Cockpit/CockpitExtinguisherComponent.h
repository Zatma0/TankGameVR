#pragma once

#include "CoreMinimal.h"
#include "Cockpit/CockpitGrabbableComponent.h"
#include "CockpitExtinguisherComponent.generated.h"

class UCockpitFireComponent;
class UHapticFeedbackEffect_Base;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Hand fire extinguisher on a wall bracket. Grab it, point the nozzle at the fire and hold the trigger of that
 * hand: the spray puts out fires inside its cone. Limited charge that slowly comes back. Pivot = middle of the
 * bottle, +X = where the nozzle points.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitExtinguisherComponent : public UCockpitGrabbableComponent
{
	GENERATED_BODY()

public:
	UCockpitExtinguisherComponent();

	/** Nozzle tip in item space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher")
	FVector NozzleOffset = FVector(12.0, 0.0, 14.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (Units = "Centimeters"))
	float SprayRange = 170.0f;

	/** Half angle of the spray cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (ClampMin = "1.0", ClampMax = "80.0", Units = "Degrees"))
	float SprayAngle = 22.0f;

	/** Fire size removed per second at the centre of the cone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (ClampMin = "0.0"))
	float ExtinguishRate = 0.7f;

	/** Charge used per second of spraying (1 = full bottle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (ClampMin = "0.0"))
	float UsePerSecond = 0.1f;

	/** Charge that comes back per second (a crew mate swapping bottles...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (ClampMin = "0.0"))
	float RechargePerSecond = 0.02f;

	/** Hands-free spray time (keyboard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher", meta = (Units = "Seconds"))
	float AutoSprayTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Visuals", meta = (ClampMin = "1", ClampMax = "40"))
	int32 SprayPuffs = 18;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Visuals")
	TObjectPtr<UStaticMesh> SprayMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Visuals")
	TSoftObjectPtr<UMaterialInterface> SprayMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Feedback")
	TSoftObjectPtr<USoundBase> SpraySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Extinguisher|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> SprayHaptic;

	/** Trigger of the holding hand. Only sprays while held (or during an auto spray). */
	UFUNCTION(BlueprintCallable, Category = "Extinguisher")
	void SetSpraying(bool bInSpraying);

	/** Without hands: sprays at the biggest fire for AutoSprayTime. */
	UFUNCTION(BlueprintCallable, Category = "Extinguisher")
	bool AutoSpray();

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	bool IsSpraying() const;

	/** 0..1 */
	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	float GetCharge() const { return Charge; }

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	FVector GetNozzleLocation() const;

	UFUNCTION(BlueprintPure, Category = "Extinguisher")
	FVector GetSprayDirection() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleReleased() override;

private:
	struct FSprayPuff
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = -1.0f;
	};

	UCockpitFireComponent* FindBiggestFire() const;
	void SprayFires(float DeltaTime, const FVector& Origin, const FVector& Direction);
	void EmitPuffs(float DeltaTime, const FVector& Origin, const FVector& Direction);
	void UpdatePuffs(float DeltaTime);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PuffMeshes;

	TArray<FSprayPuff> Puffs;
	TWeakObjectPtr<UCockpitFireComponent> AutoTarget;
	float Charge = 1.0f;
	float AutoRemaining = 0.0f;
	float EmitTimer = 0.0f;
	float HapticTimer = 0.0f;
	bool bTrigger = false;
};
