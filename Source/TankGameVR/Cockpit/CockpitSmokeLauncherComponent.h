#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CockpitSmokeLauncherComponent.generated.h"

class ATankSmokeCloud;
class UHapticFeedbackEffect_Base;
class UMaterialInstanceDynamic;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSmokeLaunched, ATankSmokeCloud*, Cloud);

/**
 * Guarded smoke grenade switch on the dashboard. Flip the red cover up with your hand, then push the button under
 * it: the turret's grenade launchers fire a smoke screen in the direction the gun points.
 * Named children: SmokeCoverHinge (the cover pivots around its Y axis, mesh + BP_GrabComponent under it),
 * SmokeButton (pressed by touching its top) and SmokeLamp (ready light).
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitSmokeLauncherComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCockpitSmokeLauncherComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	TSubclassOf<ATankSmokeCloud> CloudClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "1"))
	int32 MaxCharges = 2;

	/** Time to reload one salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ReloadTime = 25.0f;

	/** How far out the screen stands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (Units = "Centimeters"))
	float ScreenDistance = 2200.0f;

	/** Launcher tubes in the bank on each side of the turret. One grenade each per salvo. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers", meta = (ClampMin = "1", ClampMax = "8"))
	int32 TubesPerSide = 4;

	/** The salvo fans out this far to each side of the gun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers", meta = (ClampMin = "5.0", ClampMax = "90.0", Units = "Degrees"))
	float FanAngle = 44.0f;

	/** Component the banks are mounted on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers")
	FName TurretName = TEXT("TurretPivot");

	/** Where the right bank sits on the turret; the left one is mirrored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers")
	FVector BankOffset = FVector(62.0, 100.0, 14.0);

	/** Show simple tubes on the turret (switch off once the turret model has its own). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers")
	bool bShowTubes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Launchers")
	TSoftObjectPtr<UMaterialInterface> TubeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Cover", meta = (Units = "Degrees"))
	float CoverOpenAngle = 105.0f;

	/** Let go above this and the cover stays up; below, it falls shut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Cover", meta = (Units = "Degrees"))
	float CoverLatchAngle = 55.0f;

	/** Fingertip must come this close to the top of the button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Button", meta = (Units = "Centimeters"))
	float PressRadius = 6.0f;

	/** Fingertip in front of the controller. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Button", meta = (Units = "Centimeters"))
	float FingerOffset = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Setup")
	FName CoverHingeName = TEXT("SmokeCoverHinge");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Setup")
	FName ButtonName = TEXT("SmokeButton");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Setup")
	FName LampName = TEXT("SmokeLamp");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Feedback")
	TSoftObjectPtr<USoundBase> LaunchSound;

	/** The guard cover latching and the button going down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Feedback")
	TSoftObjectPtr<USoundBase> ClickSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke|Feedback")
	TSoftObjectPtr<UHapticFeedbackEffect_Base> ClickHaptic;

	UPROPERTY(BlueprintAssignable, Category = "Smoke")
	FOnSmokeLaunched OnLaunched;

	/** Fires a salvo if one is loaded (keyboard V, or the button). */
	UFUNCTION(BlueprintCallable, Category = "Smoke")
	bool Launch();

	UFUNCTION(BlueprintPure, Category = "Smoke")
	int32 GetCharges() const { return Charges; }

	/** 0..1 of the current reload. */
	UFUNCTION(BlueprintPure, Category = "Smoke")
	float GetReloadProgress() const;

	UFUNCTION(BlueprintPure, Category = "Smoke")
	float GetCoverAngle() const { return CoverAngle; }

	UFUNCTION(BlueprintPure, Category = "Smoke")
	bool IsCoverOpen() const { return CoverAngle >= CoverLatchAngle; }

	/** Opens/closes the cover without hands (tests). */
	UFUNCTION(BlueprintCallable, Category = "Smoke")
	void SetCoverOpen(bool bOpen);

	/** Fingertips to test instead of the rider's controllers (tests). */
	UFUNCTION(BlueprintCallable, Category = "Smoke")
	void SetFingerOverride(USceneComponent* Finger) { FingerOverride = Finger; }

	UFUNCTION(BlueprintPure, Category = "Smoke")
	FString GetStatusText() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	/** Tube mouth and direction in turret space. Right bank first, inner tube first. */
	void GetTube(int32 Index, FVector& OutMouth, FVector& OutDirection, float& OutYaw) const;
	void UpdateCover(float DeltaTime);
	void UpdateButton(float DeltaTime);
	void UpdateLamp();
	void GetFingertips(TArray<FVector>& Out) const;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Turret;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Hinge;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> CoverGrab;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Button;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LampMaterial;

	TWeakObjectPtr<USceneComponent> FingerOverride;
	TWeakObjectPtr<USceneComponent> CoverHand;
	FRotator HingeRest = FRotator::ZeroRotator;
	FVector ButtonRest = FVector::ZeroVector;
	float CoverAngle = 0.0f;
	float GrabAngleOffset = 0.0f;
	float ButtonTravel = 0.0f;
	float ReloadRemaining = 0.0f;
	int32 Charges = 2;
	bool bFingerWasOnButton = false;
	bool bCoverWasOpen = false;
	bool bSurfaceApplied = false;
};
