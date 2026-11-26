#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/TankArenaDirector.h"
#include "TankAudioComponent.generated.h"

class ATankVehicle;
class UAudioComponent;
class UDamageType;
class USoundBase;
class UTankHealthComponent;
class UTargetableComponent;

/**
 * What the crew hears in the player tank: engine, tracks and turret drive following what the tank does, hits on the
 * hull, fire and the extinguisher, the fire-control beeps, the warning horn and the radio calls for the waves.
 * (The gun, the breech and the smoke launchers play their own sounds at the exact moment things happen.)
 */
UCLASS(ClassGroup = (Tank), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankAudioComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> EngineLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> TracksLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> TurretLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> FireLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> ExtinguisherLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Loops")
	TSoftObjectPtr<USoundBase> WarningLoop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Hits")
	TSoftObjectPtr<USoundBase> HullHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Hits")
	TSoftObjectPtr<USoundBase> HullHitLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Fire control")
	TSoftObjectPtr<USoundBase> LockBeep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Fire control")
	TSoftObjectPtr<USoundBase> OnTargetBeep;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Radio")
	TSoftObjectPtr<USoundBase> WaveStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Radio")
	TSoftObjectPtr<USoundBase> WaveClear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Radio")
	TSoftObjectPtr<USoundBase> GameOver;

	/** Engine level standing still and at full speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix")
	FVector2D EngineVolume = FVector2D(0.55, 0.8);

	/** Engine pitch standing still and at full speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix")
	FVector2D EnginePitch = FVector2D(0.85, 1.4);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0"))
	float TracksVolume = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0"))
	float TurretVolume = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0"))
	float BeepVolume = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0"))
	float WarningVolume = 0.4f;

	/** The horn sounds this long when a fire breaks out (it keeps going while an AT gun lays on us). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Mix", meta = (ClampMin = "0.0", Units = "Seconds"))
	float FireAlarmTime = 2.0f;

	/** 0..1 how hard the engine is working right now (drives its level and pitch). */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetEngineRev() const { return Rev; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType);

private:
	void UpdateLoops(ATankVehicle* Tank, float DeltaTime);
	void UpdateFireControl(ATankVehicle* Tank);
	void UpdateRadio(ATankVehicle* Tank);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Engine;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Tracks;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Turret;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Fire;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Extinguisher;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Warning;

	TWeakObjectPtr<UTargetableComponent> LastTarget;
	ETankArenaPhase LastPhase = ETankArenaPhase::Idle;
	int32 LastWave = 0;
	float Rev = 0.0f;
	float TurretLevel = 0.0f;
	float FireAlarm = 0.0f;
	bool bWasOnTarget = false;
	bool bWasBurning = false;
};
