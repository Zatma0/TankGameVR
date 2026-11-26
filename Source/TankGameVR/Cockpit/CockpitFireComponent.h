#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CockpitFireComponent.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A spot in the crew compartment that can catch fire when the tank is hit (cables, hydraulic oil...).
 * A fire grows if left alone, slowly burns the tank's HP and can spread to the next spot. Spray it with the
 * extinguisher to put it out. Flames, glow and smoke are made at runtime around this component.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitFireComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCockpitFireComponent();

	/** HP per second at full size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "0.0"))
	float BurnDamagePerSecond = 1.5f;

	/** How fast an unattended fire grows (size per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "0.0"))
	float GrowthRate = 0.06f;

	/** A big fire left this long lights the next spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "0.0", Units = "Seconds"))
	float SpreadTime = 8.0f;

	/** Size of the area the flames cover. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (Units = "Centimeters"))
	float Radius = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals", meta = (ClampMin = "1", ClampMax = "12"))
	int32 FlameCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals", meta = (ClampMin = "0", ClampMax = "16"))
	int32 SmokePuffs = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TObjectPtr<UStaticMesh> FlameMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TObjectPtr<UStaticMesh> SmokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	/** Candelas at full size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	float LightIntensity = 8.0f;

	/** Starts a fire (or feeds one). Amount 0..1. */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	void Ignite(float Amount = 0.35f);

	/** Takes Amount off the fire's size. */
	UFUNCTION(BlueprintCallable, Category = "Fire")
	void Extinguish(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Fire")
	void PutOut() { Extinguish(10.0f); }

	UFUNCTION(BlueprintPure, Category = "Fire")
	bool IsBurning() const { return Intensity > 0.0f; }

	/** 0..1 */
	UFUNCTION(BlueprintPure, Category = "Fire")
	float GetIntensity() const { return Intensity; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FSmokePuff
	{
		FVector Location = FVector::ZeroVector;
		float Age = -1.0f;
	};

	void UpdateVisuals(float DeltaTime);
	void SpreadToNeighbour();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Smoke;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Glow;

	TArray<FVector> FlameOffsets;
	TArray<FSmokePuff> Puffs;
	float Intensity = 0.0f;
	float PendingDamage = 0.0f;
	float BigTime = 0.0f;
	float PuffTimer = 0.0f;
};
