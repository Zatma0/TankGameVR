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
 * Lock-on aiming. Aim Assist on: one target, the aim stick steps left/right to the next one on that side.
 * Aim Assist off: the stick aims freely. The aim always moves smoothly toward its goal.
 */
UCLASS(ClassGroup = (Targeting), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UTankTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTankTargetingComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Selection", meta = (ShowOnlyInnerProperties))
	FTargetSelectionTuning Selection;

	/** Aim stick X beyond which a left/right target switch is requested. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.1", ClampMax = "0.95"))
	float TargetSwitchDeadZone = 0.55f;

	/** Stick must return below this before the next push counts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float TargetSwitchReleaseThreshold = 0.35f;

	/** Minimum time between two switches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchCooldown = 0.2f;

	/** Delay before a held stick starts repeating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchRepeatDelay = 0.6f;

	/** Repeat interval while held. 0 = no repeat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSwitchRepeatInterval = 0.45f;

	/** How long the target may be out of range/view before the lock drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Switching", meta = (ClampMin = "0.0", Units = "Seconds"))
	float TargetSelectionSmoothing = 0.5f;

	/** How strongly the aim is pulled onto the target (1 = exactly on it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetLockStrength = 1.0f;

	/** Turret turning speed. Real tanks: about 25 (T-72) to 40 (Abrams, Leopard 2) deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Turret", meta = (ClampMin = "1.0", Units = "DegreesPerSecond"))
	float TurretTraverseSpeed = 30.0f;

	/** How fast the turret motor spins up and brakes (deg/s^2). Lower = heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Turret", meta = (ClampMin = "1.0"))
	float TurretTraverseAcceleration = 60.0f;

	/** Gun elevation speed. Real tanks: about 4-8 deg/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Turret", meta = (ClampMin = "0.5", Units = "DegreesPerSecond"))
	float GunElevationSpeed = 8.0f;

	/** deg/s^2 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Turret", meta = (ClampMin = "1.0"))
	float GunElevationAcceleration = 30.0f;

	/** Below this error the gun counts as laid on the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Turret", meta = (ClampMin = "0.1", Units = "Degrees"))
	float OnTargetTolerance = 1.0f;

	/** deg/s at full stick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "1.0"))
	float ManualAimSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ManualYawLimit = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim")
	FVector2D ManualPitchRange = FVector2D(-10.0, 25.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ManualAimDeadZone = 0.15f;

	/** Lock the best target automatically when there is none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire")
	bool bAutoAcquire = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.05", Units = "Seconds"))
	float AutoAcquireInterval = 0.25f;

	/** While you aim by hand nothing is locked; it waits until the stick has been still this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.0", Units = "Seconds"))
	float AutoAcquireAfterManualDelay = 0.75f;

	/** How often all targets are re-checked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.02", Units = "Seconds"))
	float CandidateRefreshInterval = 0.1f;

	/** No target and idle stick: aim returns to the front after this delay. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "0.0", Units = "Seconds"))
	float AimRecenterDelay = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Aim", meta = (ClampMin = "1.0"))
	float AimRecenterSpeed = 30.0f;

	/** Side offset of the extra line-of-sight rays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Acquire", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float VisibilitySampleSpread = 80.0f;

	/** Component names on the owner. Missing ones fall back to the root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName AimOriginComponentName = TEXT("AimOrigin");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName SightComponentName = TEXT("SightFrame");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName TurretComponentName = TEXT("TurretPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Setup")
	FName BarrelComponentName = TEXT("BarrelPivot");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Debug")
	bool bDebugDraw = false;

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnTankTargetChanged OnTargetChanged;

	/** X = left/right, Y = up/down. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetAimInput(FVector2D Input);

	/** Direction > 0 = right. Returns true if the target changed. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool RequestTargetSwitch(int32 Direction);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearTarget();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetAimAssistEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsAimAssistEnabled() const { return bAimAssistEnabled; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	UTargetableComponent* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetAimDirection() const { return AimRotation.GetForwardVector(); }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetAimOrigin() const;

	/** Angle between where the gun points and where it should point. */
	UFUNCTION(BlueprintPure, Category = "Targeting|Turret")
	float GetAimError() const { return AimError; }

	/** Locked and the gun has caught up with the target. */
	UFUNCTION(BlueprintPure, Category = "Targeting|Turret")
	bool IsOnTarget() const { return CurrentTarget.IsValid() && AimError <= OnTargetTolerance; }

	/** deg/s, + = right */
	UFUNCTION(BlueprintPure, Category = "Targeting|Turret")
	float GetTurretSpeed() const { return TurretYawSpeed; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FTransform GetSightTransform() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	const TArray<FTargetCandidate>& GetLastCandidates() const { return LastCandidates; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FString GetLastDecision() const { return LastDecision; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsTargetEligible(const UTargetableComponent* Target) const;

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
	// Where the turret/gun really points, relative to the hull.
	float TurretYaw = 0.0f;
	float GunPitch = 0.0f;
	float TurretYawSpeed = 0.0f;
	float GunPitchSpeed = 0.0f;
	float AimError = 0.0f;
	bool bAimAssistEnabled = true;
	bool bSwitchHeld = false;
	int32 HeldDirection = 0;
	double LastSwitchTime = -1000.0;
	double NextRepeatTime = 0.0;
	double NextAutoAcquireTime = 0.0;
	double NextEligibilityCheckTime = 0.0;
	double IneligibleSince = -1.0;
	double NextCandidateRefreshTime = 0.0;
	double IdleSince = -1.0;
	double LastManualAimTime = -1000.0;
	FString LastDecision;
};
