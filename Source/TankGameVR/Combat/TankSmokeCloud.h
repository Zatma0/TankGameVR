#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankSmokeCloud.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Smoke screen from the tank's grenade launchers. Spawn it where the screen should stand, facing away from the tank:
 * the grenades fly out from LaunchFrom, burst and bloom into a wall of smoke along the actor's Y axis.
 * Enemy gunners can't see through it (IsLineBlocked).
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankSmokeCloud : public AActor
{
	GENERATED_BODY()

public:
	ATankSmokeCloud();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "1", ClampMax = "24"))
	int32 PuffCount = 9;

	/** Width of the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (Units = "Centimeters"))
	float Width = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (Units = "Centimeters"))
	float PuffRadius = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1", Units = "Seconds"))
	float FlightTime = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1", Units = "Seconds"))
	float BloomTime = 1.6f;

	/** Total time, fading included. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "1.0", Units = "Seconds"))
	float Lifetime = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1", Units = "Seconds"))
	float FadeTime = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	TObjectPtr<UStaticMesh> PuffMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	TSoftObjectPtr<UMaterialInterface> SmokeMaterial;

	/** Where the grenades leave the tank (set before BeginPlay). */
	UPROPERTY(BlueprintReadWrite, Category = "Smoke", meta = (ExposeOnSpawn = "true"))
	FVector LaunchFrom = FVector::ZeroVector;

	/** 0..1, how thick the screen is right now. */
	UFUNCTION(BlueprintPure, Category = "Smoke")
	float GetDensity() const;

	/** Does this screen block the line A-B. */
	UFUNCTION(BlueprintPure, Category = "Smoke")
	bool BlocksLine(const FVector& A, const FVector& B) const;

	/** Is the line A-B blocked by any smoke in the world. */
	static bool IsLineBlocked(const UWorld* World, const FVector& A, const FVector& B);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FPuff
	{
		FVector Target = FVector::ZeroVector;
		FVector Drift = FVector::ZeroVector;
		float Delay = 0.0f;
		float Scale = 1.0f;
	};

	FVector GetPuffLocation(int32 Index) const;
	float GetPuffRadius(int32 Index) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> PuffMeshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PuffMaterials;

	TArray<FPuff> Puffs;
	float Age = 0.0f;
};
