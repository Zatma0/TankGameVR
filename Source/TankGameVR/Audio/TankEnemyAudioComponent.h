#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TankEnemyAudioComponent.generated.h"

class UAudioComponent;
class USoundBase;
class UTankHealthComponent;

/** An enemy vehicle as heard from the player tank: its engine, its end, and the wreck burning. */
UCLASS(ClassGroup = (Tank))
class TANKGAMEVR_API UTankEnemyAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankEnemyAudioComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> EngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> DestroyedSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TSoftObjectPtr<USoundBase> WreckFireLoop;

	/** Engine pitch of this vehicle: a heavy tank low, a light car high. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	float EnginePitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0"))
	float EngineVolume = 0.7f;

	/** How far the engine carries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (Units = "Centimeters"))
	float EngineReach = 9000.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDied(UTankHealthComponent* HealthComponent, AActor* Killer);

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Engine;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WreckFire;
};
