#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankShellProjectile.generated.h"

class UMaterialInterface;
class UPointLightComponent;
class UProjectileMovementComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShellImpact, AActor*, HitActor, FVector, Location);

/** Fired tank round: a fast tracer that deals point damage to whatever it hits and flashes. */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankShellProjectile : public AActor
{
	GENERATED_BODY()

public:
	ATankShellProjectile();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
	float Damage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell")
	TSubclassOf<UDamageType> DamageType;

	/** Size the hit flash grows to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|Impact", meta = (Units = "Centimeters"))
	float ImpactFlashSize = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|Impact", meta = (ClampMin = "0.05", Units = "Seconds"))
	float ImpactFlashTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|Impact")
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|Impact")
	float ImpactSoundPitch = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shell|Visuals")
	TSoftObjectPtr<UMaterialInterface> TracerMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Shell")
	FOnShellImpact OnImpact;

	/** Speed is set by the cannon before the round is finished spawning. */
	void Launch(const FVector& Direction, float Speed, AController* InInstigatorController);

	UFUNCTION(BlueprintPure, Category = "Shell")
	bool HasHit() const { return bHit; }

	UFUNCTION(BlueprintPure, Category = "Shell")
	UProjectileMovementComponent* GetProjectileMovement() const { return Movement; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shell")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shell")
	TObjectPtr<UStaticMeshComponent> Tracer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shell")
	TObjectPtr<UStaticMeshComponent> ImpactFlash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shell")
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shell")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	TWeakObjectPtr<AController> InstigatorController;
	bool bHit = false;
	float FlashTime = 0.0f;
};
