#pragma once

#include "CoreMinimal.h"
#include "Cockpit/CockpitGrabbableComponent.h"
#include "CockpitCigaretteComponent.generated.h"

class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Just for fun. Bring the filter end to your mouth: the ember glows. Take it away: you blow out smoke.
 * Pivot = middle, +X = ember end. Optional children found by name: CigaretteEmber (mesh), CigaretteGlow (point light).
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitCigaretteComponent : public UCockpitGrabbableComponent
{
	GENERATED_BODY()

public:
	UCockpitCigaretteComponent();

	/** Pivot to filter end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (Units = "Centimeters"))
	float FilterOffset = -5.0f;

	/** Mouth position in head (camera) space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette")
	FVector MouthOffset = FVector(7.0, 0.0, -9.0);

	/** Filter must be this close to the mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float DragDistance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "0.05", Units = "Seconds"))
	float HeatUpTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "0.05", Units = "Seconds"))
	float CoolDownTime = 2.5f;

	/** Ember level when nobody is smoking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleHeat = 0.15f;

	/** A drag at least this long ends with smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "0.0", Units = "Seconds"))
	float MinDragTime = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ExhaleDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Smoke", meta = (ClampMin = "1", ClampMax = "16"))
	int32 PuffCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Smoke", meta = (ClampMin = "0.1", Units = "Seconds"))
	float PuffLifetime = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Smoke", meta = (Units = "Centimeters"))
	float PuffSize = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Smoke")
	TObjectPtr<UStaticMesh> SmokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Smoke")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Glow")
	FName EmberComponentName = TEXT("CigaretteEmber");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Glow")
	FName GlowComponentName = TEXT("CigaretteGlow");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Glow")
	FName EmberParameter = TEXT("EmissiveStrength");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Glow")
	FVector2D EmberStrengthRange = FVector2D(0.5, 40.0);

	/** Candelas. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Glow")
	FVector2D GlowIntensityRange = FVector2D(0.0, 3.0);

	/** Loops while dragging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cigarette|Feedback")
	TSoftObjectPtr<USoundBase> CrackleSound;

	/** Head to use instead of the player camera (tests). */
	UFUNCTION(BlueprintCallable, Category = "Cigarette")
	void SetHeadOverride(USceneComponent* Head) { HeadOverride = Head; }

	UFUNCTION(BlueprintPure, Category = "Cigarette")
	bool IsDragging() const { return bDragging; }

	/** 0..1 */
	UFUNCTION(BlueprintPure, Category = "Cigarette")
	float GetHeat() const { return Heat; }

	UFUNCTION(BlueprintPure, Category = "Cigarette")
	int32 GetVisiblePuffs() const;

	UFUNCTION(BlueprintPure, Category = "Cigarette")
	int32 GetDragCount() const { return DragCount; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleReleased() override;

private:
	struct FPuff
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = -1.0f;
		float Delay = 0.0f;
	};

	bool GetHead(FTransform& OutHead) const;
	void StartExhale(const FTransform& Head);
	void UpdatePuffs(float DeltaTime);
	void UpdateGlow();
	void SetCrackle(bool bOn);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PuffMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EmberMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Crackle;

	TWeakObjectPtr<USceneComponent> HeadOverride;
	TArray<FPuff> Puffs;
	float Heat = 0.0f;
	float DragTime = 0.0f;
	float ExhaleIn = -1.0f;
	int32 DragCount = 0;
	bool bDragging = false;
};
