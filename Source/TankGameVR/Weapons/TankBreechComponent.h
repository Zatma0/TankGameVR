#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Combat/TankDamageTypes.h"
#include "TankBreechComponent.generated.h"

class UHapticFeedbackEffect_Base;
class UMaterialInstanceDynamic;
class USoundBase;
class UTankShellComponent;

UENUM(BlueprintType)
enum class ETankBreechState : uint8
{
	/** Waiting for a round. */
	Open,
	/** A round is in the mouth and the hand is pushing it in. */
	Ramming,
	/** Desktop fallback: the round is loaded by itself. */
	AutoLoading,
	/** Pushed far enough: the round slides home and the block closes. */
	Closing,
	/** "UP" - ready to fire. */
	Loaded,
	Recoil
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreechStateChanged, ETankBreechState, NewState);

/**
 * Gun breech in the cockpit. Place it at the breech mouth, +X pointing into the gun.
 * Like a real loader: bring a round nose-first to the mouth, push it in, the wedge closes by itself ("UP"),
 * and after the shot it opens again and throws out the case stub.
 * Optional children found by name: BreechBlock (wedge), ReadyLamp, CasingStub.
 */
UCLASS(ClassGroup = (Weapons), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankBreechComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTankBreechComponent();

	/** Nose must be this close to the mouth to go in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float CaptureRadius = 12.0f;

	/** Max angle between the round and the bore. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "1.0", ClampMax = "90.0", Units = "Degrees"))
	float CaptureAngle = 35.0f;

	/** Nose depth when fully home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "5.0", Units = "Centimeters"))
	float RamDepth = 50.0f;

	/** Share of RamDepth after which the breech takes over and closes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float RamThreshold = 0.8f;

	/** Pull the nose this far back out of the mouth to take the round out again. Keep it above CaptureRadius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "1.0", Units = "Centimeters"))
	float ExitMargin = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "0.05", Units = "Seconds"))
	float CloseTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Loading", meta = (ClampMin = "0.1", Units = "Seconds"))
	float AutoLoadTime = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Recoil", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float RecoilDistance = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Recoil", meta = (ClampMin = "0.01", Units = "Seconds"))
	float RecoilKickTime = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Recoil", meta = (ClampMin = "0.05", Units = "Seconds"))
	float RecoilReturnTime = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	FName BlockComponentName = TEXT("BreechBlock");

	/** Where the wedge sits when open, relative to closed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	FVector BlockOpenOffset = FVector(0.0, 0.0, -24.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	FName LampComponentName = TEXT("ReadyLamp");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	FName LampParameter = TEXT("EmissiveStrength");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	float LampOnStrength = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Visuals")
	float LampOffStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Casing")
	FName CasingComponentName = TEXT("CasingStub");

	/** Start velocity of the ejected stub, in breech space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Casing")
	FVector CasingVelocity = FVector(-150.0, 25.0, 110.0);

	/** cm/s^2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Casing")
	float CasingGravity = 700.0f;

	/** Cockpit floor height in the breech's parent space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Casing")
	float CasingFloorZ = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Casing", meta = (ClampMin = "0.0", Units = "Seconds"))
	float CasingLifetime = 3.0f;

	/** The round scraping into the chamber (as it is caught, or when an auto load starts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> CaptureSound;

	/** The round sliding home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> SeatSound;

	/** The wedge shut: played on the frame it has closed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> CloseSound;

	/** The ready lamp coming on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> ReadySound;

	/** The breech dropping open after the recoil. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> OpenSound;

	/** The case stub hitting the floor: played on the frame it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<USoundBase> CasingSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	float CloseSoundPitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	float CloseSoundVolume = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breech|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> ClickHaptic;

	UPROPERTY(BlueprintAssignable, Category = "Breech")
	FOnBreechStateChanged OnStateChanged;

	UFUNCTION(BlueprintPure, Category = "Breech")
	ETankBreechState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Breech")
	bool IsLoaded() const { return State == ETankBreechState::Loaded; }

	/** 0..1 while ramming. */
	UFUNCTION(BlueprintPure, Category = "Breech")
	float GetRamProgress() const { return RamProgress; }

	UFUNCTION(BlueprintPure, Category = "Breech")
	UTankShellComponent* GetShell() const { return Shell.Get(); }

	/** One line for the cockpit display. */
	UFUNCTION(BlueprintPure, Category = "Breech")
	FString GetStatusText() const;

	/** Type of the round in the breech (or the last one loaded). */
	UFUNCTION(BlueprintPure, Category = "Breech")
	ETankRoundType GetLoadedRoundType() const { return LoadedType; }

	/** Spends the loaded round. Returns false if not loaded. */
	UFUNCTION(BlueprintCallable, Category = "Breech")
	bool Fire();

	/** Loads the next rack round without hands (keyboard/gamepad). */
	UFUNCTION(BlueprintCallable, Category = "Breech")
	bool AutoLoad();

	/** Same, but only takes a round of this type. */
	UFUNCTION(BlueprintCallable, Category = "Breech")
	bool AutoLoadType(ETankRoundType Type);

	/** Rack rounds of this type ready to use. */
	UFUNCTION(BlueprintPure, Category = "Breech")
	int32 CountAvailable(ETankRoundType Type) const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	void SetState(ETankBreechState NewState);
	bool StartAutoLoad(UTankShellComponent* Round);
	void TryCapture();
	void UpdateRamming();
	void UpdateAutoLoading();
	void StartClosing(USceneComponent* FromHand);
	void UpdateRecoil();
	void UpdateVisuals(float DeltaTime);
	void EjectCasing();
	void UpdateCasing(float DeltaTime);
	float RailMinFor(const UTankShellComponent* Round) const;
	float RailMaxFor(const UTankShellComponent* Round) const;
	void PlaySound(const TSoftObjectPtr<USoundBase>& Sound, float Volume, float Pitch) const;
	void Click(USceneComponent* Hand, float Scale) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTankShellComponent>> Shells;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Block;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Casing;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LampMaterial;

	TWeakObjectPtr<UTankShellComponent> Shell;
	ETankBreechState State = ETankBreechState::Open;
	ETankRoundType LoadedType = ETankRoundType::AP;
	FTransform RestRelativeTransform;
	FTransform AutoStartInBreech;
	FVector BlockClosedLocation = FVector::ZeroVector;
	FVector CasingLocation = FVector::ZeroVector;
	FVector CasingSpeed = FVector::ZeroVector;
	float StateTime = 0.0f;
	float RamProgress = 0.0f;
	float CloseStartPosition = 0.0f;
	float BlockAlpha = 0.0f;
	float CasingRemaining = 0.0f;
	bool bCasingFlying = false;
	bool bCasingLanded = false;
};
