#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Targeting/TargetSelectionMath.h"
#include "TankTargetingComponent.generated.h"

class UTargetableComponent;
class UTargetRegistrySubsystem;
class UTankSettingsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTankTargetChanged, UTargetableComponent*, OldTarget, UTargetableComponent*, NewTarget);

/**
 * Lock-on aiming for the tank (Dark Souls-style target cycling).
 *
 * Aim Assist ON : one current target. Pushing the aim stick left/right steps to the nearest eligible target on that
 *                 side of the current one (edge-triggered, with repeat interval, cooldown and hysteresis, so holding
 *                 the stick never cycles uncontrollably and noise near the threshold never flips targets).
 *                 Lost/destroyed targets are re-acquired automatically; with no target the stick aims manually
 *                 until a target is acquired.
 * Aim Assist OFF: no locking at all; the stick turns the aim (yaw/pitch) manually.
 *
 * The aim direction (what the crosshair shows) always moves smoothly toward the desired direction.
 * Works with any actor that has a UTargetableComponent; the look of enemies is irrelevant.
 */
UCLASS(ClassGroup = (Targeting), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankTargetingComponent();

	// --- Tuning --------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Selection", meta = (ShowOnlyInnerProperties))
	FTargetSelectionTuning Selection;

	/** Aim stick X beyond which a left/right target switch is requested. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float TargetSwitchDeadZone = 0.55f;

	/** The stick must come back below this before the next push counts as a new request (hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float TargetSwitchReleaseThreshold = 0.35f;

	/** Minimum time between two switches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchCooldown = 0.2f;

	/** Holding the stick to one side: delay before it starts stepping again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchRepeatDelay = 0.6f;

	/** Holding the stick: step interval after the first repeat. 0 = never repeat (one step per push). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchRepeatInterval = 0.45f;

	/** Seconds the current target may be ineligible (angle/range/visibility) before the lock drops. Stops flicker at limits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSelectionSmoothing = 0.5f;

	/** 0..1: how strongly the aim is pulled onto the locked target (1 = exactly on target). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetLockStrength = 1.0f;

	/** Exponential rate (1/s) at which the aim/crosshair moves toward its goal. Higher = snappier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.5", ClampMax = "60.0"))
	float CrosshairTransitionSpeed = 10.0f;

	/** Manual aiming speed at full stick (deg/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "1.0"))
	float ManualAimSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ManualYawLimit = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim")
	FVector2D ManualPitchRange = FVector2D(-10.0, 25.0);

	/** Stick dead zone for manual aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ManualAimDeadZone = 0.15f;

	/** With Aim Assist on and no target, lock the best target in view automatically (e.g. when a new wave arrives). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire")
	bool bAutoAcquire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.05", Units = "Seconds"))
	float AutoAcquireInterval = 0.25f;

	/** Sideways spread of the extra visibility rays (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float VisibilitySampleSpread = 80.0f;

	/** Owner components (by name) used for aiming. Missing ones fall back to the actor root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName AimOriginComponentName = TEXT("AimOrigin");

	/** Frame the cockpit display looks through (rotates with the hull, not with the turret). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName SightComponentName = TEXT("SightFrame");

	/** Optional visual turret (yaw follows the aim) and barrel (pitch follows the aim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName TurretComponentName = TEXT("TurretPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName BarrelComponentName = TEXT("BarrelPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Debug")
	bool bDebugDraw = false;

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTankTargetChanged OnTargetChanged;

	// --- API -----------------------------------------------------------------------------------------

	/** Aim stick: X = left/right, Y = up/down, each in [-1, 1]. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetAimInput(FVector2D Input);

	/** Steps to the next target on a side (Direction > 0 = right). Returns true if the target changed. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool RequestTargetSwitch(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearTarget();

	/** Normally driven by the Aim Assist setting; exposed for tests and special cases. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetAimAssistEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsAimAssistEnabled() const { return bAimAssistEnabled; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	UTargetableComponent* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** Direction the crosshair currently points at (smoothed). */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetAimDirection() const { return AimRotation.GetForwardVector(); }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetAimOrigin() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FTransform GetSightTransform() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	const TArray<FTargetCandidate>& GetLastCandidates() const { return LastCandidates; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FString GetLastDecision() const { return LastDecision; }

	/** Evaluates all registered targets against the current reference direction (for UI and debugging). */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void RefreshCandidates();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleAimAssistSettingChanged(bool bEnabled);

	void HandleTargetRemoved(UTargetableComponent* Target);
	void ResolveComponents();
	void BuildCandidates(const FVector& ReferenceDirection, TArray<FTargetCandidate>& Out) const;
	float MeasureVisibility(const UTargetableComponent* Target, const FVector& Origin, const FVector& Location) const;
	FVector GetReferenceDirection() const;
	void SetTarget(UTargetableComponent* NewTarget, const FString& Reason);
	bool TryAcquire(const FVector& ReferenceDirection, const FString& Reason);
	void MaintainCurrentTarget(double Now);
	void HandleSwitchInput(double Now);
	void UpdateManualAim(float DeltaTime);
	void UpdateAim(float DeltaTime);
	void SyncManualAnglesToAim();
	void UpdateTurretVisuals() const;
	void DrawDebug() const;
	double Now() const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> AimOriginComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> SightComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> TurretComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> BarrelComponent;

	UPROPERTY(Transient)
	TArray<FTargetCandidate> LastCandidates;

	TWeakObjectPtr<UTargetableComponent> CurrentTarget;
	TWeakObjectPtr<UTankSettingsSubsystem> Settings;
	FDelegateHandle TargetRemovedHandle;

	FVector2D AimInput = FVector2D::ZeroVector;
	FQuat AimRotation = FQuat::Identity;
	float ManualYaw = 0.0f;
	float ManualPitch = 0.0f;
	bool bAimAssistEnabled = true;
	bool bSwitchHeld = false;
	int32 HeldDirection = 0;
	double LastSwitchTime = -1000.0;
	double NextRepeatTime = 0.0;
	double NextAutoAcquireTime = 0.0;
	double NextEligibilityCheckTime = 0.0;
	double IneligibleSince = -1.0;
	FString LastDecision;
};
