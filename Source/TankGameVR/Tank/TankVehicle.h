#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TankVehicle.generated.h"

class APawn;
class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UUserWidget;
class UCockpitCigaretteComponent;
class UCockpitJoystickComponent;
class UInputAction;
class UTankBreechComponent;
class UTankCannonComponent;
class UTankShellComponent;
class UInputMappingContext;
class UTankMovementComponent;
class UTankSeatComponent;
class UTankTargetingComponent;
class UWidgetComponent;
struct FInputActionValue;

/**
 * Player tank. Holds the gameplay components; the meshes live in BP_PlayerTank.
 * A held cockpit stick wins, otherwise the thumbstick/keyboard drives it. IMC_Tank is added on boarding.
 */
UCLASS(Blueprintable)
class TANKGAMEVR_API ATankVehicle : public AActor
{
	GENERATED_BODY()

public:
	ATankVehicle();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputMappingContext> TankMappingContext;

	/** Axis2D: X = steer (right +), Y = throttle (forward +). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> DriveAction;

	/** Axis2D: X = aim/target switch (right +), Y = aim up/down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> AimAction;

	/** Button: switch between the cockpit view and the overview camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> ToggleViewAction;

	/** Button: fire the main gun (controller trigger). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> FireAction;

	/** Button: Aim Assist on/off (same saved setting as the VR menu button). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> ToggleAimAssistAction;

	/** Button: load a round without hands (keyboard/gamepad play). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	TObjectPtr<UInputAction> AutoLoadAction;

	/** Above the template's IMC_Default (0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|Input")
	int32 MappingPriority = 10;

	/** Blend time on a monitor; with a headset the switch is a cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|View", meta = (ClampMin = "0.0", Units = "Seconds"))
	float ViewBlendTime = 0.35f;

	/** Screen panel shown in the overview view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tank|View")
	TSubclassOf<UUserWidget> OverviewHudClass;

	UFUNCTION(BlueprintCallable, Category = "Tank|View")
	void SetOverviewView(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Tank|View")
	void ToggleOverviewView() { SetOverviewView(!bOverviewView); }

	UFUNCTION(BlueprintPure, Category = "Tank|View")
	bool IsOverviewViewActive() const { return bOverviewView; }

	UFUNCTION(BlueprintPure, Category = "Tank|View")
	UCameraComponent* GetOverviewCamera() const { return OverviewCamera; }

	UFUNCTION(BlueprintPure, Category = "Tank|View")
	UUserWidget* GetOverviewHud() const { return OverviewHud; }

	UFUNCTION(BlueprintPure, Category = "Tank")
	UTankMovementComponent* GetTankMovement() const { return TankMovement; }

	UFUNCTION(BlueprintPure, Category = "Tank")
	UTankTargetingComponent* GetTargeting() const { return Targeting; }

	UFUNCTION(BlueprintPure, Category = "Tank")
	UCockpitJoystickComponent* GetDriveStick() const { return DriveStick; }

	UFUNCTION(BlueprintPure, Category = "Tank")
	UCockpitJoystickComponent* GetAimStick() const { return AimStick; }

	UFUNCTION(BlueprintPure, Category = "Tank")
	UWidgetComponent* GetDisplay() const { return Display; }

	UFUNCTION(BlueprintPure, Category = "Tank|Weapons")
	UTankBreechComponent* GetBreech() const { return Breech; }

	UFUNCTION(BlueprintPure, Category = "Tank|Weapons")
	UTankCannonComponent* GetCannon() const { return Cannon; }

	UFUNCTION(BlueprintPure, Category = "Tank|Weapons")
	TArray<UTankShellComponent*> GetShells() const;

	UFUNCTION(BlueprintPure, Category = "Tank|Cockpit")
	UCockpitCigaretteComponent* GetCigarette() const { return Cigarette; }

	UFUNCTION(BlueprintCallable, Category = "Tank|Weapons")
	bool Fire();

	/** Thumbstick/keyboard values (tests set them directly). */
	UFUNCTION(BlueprintCallable, Category = "Tank|Input")
	void SetFallbackInputs(FVector2D Drive, FVector2D Aim);

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBoarded(APawn* Pawn);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank")
	TObjectPtr<UBoxComponent> HullCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank")
	TObjectPtr<UTankMovementComponent> TankMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank")
	TObjectPtr<UTankTargetingComponent> Targeting;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Turret")
	TObjectPtr<USceneComponent> TurretPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Turret")
	TObjectPtr<USceneComponent> BarrelPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Turret")
	TObjectPtr<USceneComponent> AimOrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Turret")
	TObjectPtr<USceneComponent> SightFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<USceneComponent> CockpitRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<UTankSeatComponent> Seat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<UCockpitJoystickComponent> DriveStick;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<UCockpitJoystickComponent> AimStick;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<UWidgetComponent> Display;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Weapons")
	TObjectPtr<UTankCannonComponent> Cannon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Weapons")
	TObjectPtr<UTankBreechComponent> Breech;

	/** Ready rack, left of the seat. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Weapons")
	TObjectPtr<UTankShellComponent> ShellA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Weapons")
	TObjectPtr<UTankShellComponent> ShellB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Weapons")
	TObjectPtr<UTankShellComponent> ShellC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|Cockpit")
	TObjectPtr<UCockpitCigaretteComponent> Cigarette;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|View")
	TObjectPtr<USpringArmComponent> OverviewArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tank|View")
	TObjectPtr<UCameraComponent> OverviewCamera;

private:
	void OnDriveAction(const FInputActionValue& Value);
	void OnDriveReleased(const FInputActionValue& Value);
	void OnAimAction(const FInputActionValue& Value);
	void OnAimReleased(const FInputActionValue& Value);
	void OnToggleView(const FInputActionValue& Value);
	void OnFire(const FInputActionValue& Value);
	void OnAutoLoad(const FInputActionValue& Value);
	void OnToggleAimAssist(const FInputActionValue& Value);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> OverviewHud;

	bool bOverviewView = false;

	TWeakObjectPtr<APlayerController> InputController;
	FVector2D FallbackDrive = FVector2D::ZeroVector;
	FVector2D FallbackAim = FVector2D::ZeroVector;
};
