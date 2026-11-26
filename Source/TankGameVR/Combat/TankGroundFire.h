#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankGroundFire.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** A patch of burning ground left by an HE round. Any tank standing or driving in it loses HP, ours too. */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankGroundFire : public AActor
{
	GENERATED_BODY()

public:
	ATankGroundFire();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "50.0", Units = "Centimeters"))
	float Radius = 300.0f;

	/** Total burn time, dying down included. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "1.0", Units = "Seconds"))
	float Duration = 10.0f;

	/** HP per second for a tank inside (before its armour). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "0.0"))
	float DamagePerSecond = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire", meta = (ClampMin = "0.1", Units = "Seconds"))
	float FadeTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals", meta = (ClampMin = "1", ClampMax = "40"))
	int32 FlameCount = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals", meta = (ClampMin = "0", ClampMax = "20"))
	int32 SmokePuffs = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TObjectPtr<UStaticMesh> FlameMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TObjectPtr<UStaticMesh> SmokeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TSoftObjectPtr<UMaterialInterface> FlameMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fire|Visuals")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	/** 0..1, how strongly it burns right now. */
	UFUNCTION(BlueprintPure, Category = "Fire")
	float GetStrength() const;

	UFUNCTION(BlueprintPure, Category = "Fire")
	bool IsInside(const FVector& Location) const;

	/** Is this spot inside any ground fire. */
	static bool IsBurningAt(const UWorld* World, const FVector& Location);

	/** Starts a fire on the ground under Point. Nothing if there is no ground close below. */
	static ATankGroundFire* StartBelow(UWorld* World, TSubclassOf<ATankGroundFire> FireClass, const FVector& Point, AActor* FireOwner, AController* Instigator);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FFlame
	{
		FVector Offset = FVector::ZeroVector;
		float Height = 100.0f;
		float Width = 45.0f;
		float Phase = 0.0f;
	};

	void Burn(float DeltaSeconds);
	void UpdateVisuals();
	UStaticMeshComponent* MakeMesh(UStaticMesh* Mesh, UMaterialInterface* Material);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Smoke;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(Transient)
	TObjectPtr<class UAudioComponent> Crackle;

	TArray<FFlame> FlameShapes;
	TWeakObjectPtr<AController> FireInstigator;
	float Age = 0.0f;
	float DamageTimer = 0.0f;
};
