#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TankMovementMath.generated.h"

/**
 * Tuning for the single-stick tank drive.
 * Stick X = steering (right positive), stick Y = throttle (forward positive).
 * Speeds are in cm/s, turn rates in deg/s (Unreal units: 1 uu = 1 cm).
 */
USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTankDriveTuning
{
	GENERATED_BODY()

	// --- Input shaping -------------------------------------------------------------------------------

	/** Stick deflection (0..1) ignored around the centre for steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float SteeringDeadZone = 0.12f;

	/** Stick deflection (0..1) ignored around the centre for throttle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ThrottleDeadZone = 0.12f;

	/** Multiplier on steering after the dead zone (1 = full lock at full deflection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SteeringSensitivity = 1.0f;

	/** Response exponent: 1 = linear, > 1 = finer control near the centre (gentle small turns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float SteeringCurve = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float ThrottleCurve = 1.2f;

	/** Time constant (s) of the exponential filter on the raw stick. 0 = no filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.5", Units = "Seconds"))
	float JoystickSmoothing = 0.06f;

	/** Maximum change per second of the processed stick values (limits jerky inputs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "50.0"))
	float InputResponseSpeed = 5.0f;

	// --- Speed ---------------------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MaximumForwardSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float ReverseSpeed = 200.0f;

	/** Speed gain per second when speeding up in the current direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float Acceleration = 150.0f;

	/** Speed loss per second when the stick asks for less speed (coasting down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float Deceleration = 220.0f;

	/** Speed loss per second when the stick asks for the opposite direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float BrakingStrength = 400.0f;

	// --- Turning -------------------------------------------------------------------------------------

	/** Turn rate (deg/s) at full steering once the tank moves at TurnRateReferenceSpeed or faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MaximumTurnRate = 35.0f;

	/** Global multiplier on the turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float TurnSpeedMultiplier = 1.0f;

	/** Below this speed the available turn rate scales down linearly (a tracked tank turns along a path, not on a coin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float TurnRateReferenceSpeed = 150.0f;

	/** Forward speed reached at full steering with a neutral throttle: steering alone drives the tank along a curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MinimumMovementSpeedWhileTurning = 120.0f;

	/** Fraction of MinimumMovementSpeedWhileTurning used for the smallest steering input (grows to 1 at full steering). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmallSteerSpeedRatio = 0.35f;

	/** Speed cap at full steering; the cap blends from MaximumForwardSpeed as steering increases (tanks slow in sharp turns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MaximumMovementSpeedWhileTurning = 280.0f;

	/** VR comfort: how fast the turn rate itself may change (deg/s per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0"))
	float MaximumTurnAcceleration = 90.0f;

	/** Allow turning on the spot (neutral throttle, no forward creep). Off by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning")
	bool bAllowPivotTurn = false;

	/** When reversing, steering right swings the nose left (car-like) instead of right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning")
	bool bInvertSteeringInReverse = false;
};

/** Pure, side-effect free drive math. Used by UTankMovementComponent and the automation tests. */
UCLASS()
class TANKGAMEVR_API UTankMovementMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Dead zone, rescale, sensitivity and response curve. Keeps the sign, output in [-1, 1]. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ShapeAxis(float Value, float DeadZone, float Curve, float Sensitivity);

	/** Desired signed speed (cm/s) for processed steering and throttle in [-1, 1]. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ComputeTargetSpeed(const FTankDriveTuning& Tuning, float Steering, float Throttle);

	/** Desired turn rate (deg/s, positive = right) for processed steering at the current speed. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ComputeTargetTurnRate(const FTankDriveTuning& Tuning, float Steering, float CurrentSpeed);

	/** Moves CurrentSpeed toward TargetSpeed using acceleration, deceleration or braking as appropriate. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float StepSpeed(const FTankDriveTuning& Tuning, float CurrentSpeed, float TargetSpeed, float DeltaTime);

	/** Frame-rate independent exponential smoothing. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static FVector2D SmoothStick(const FVector2D& Previous, const FVector2D& Raw, float DeltaTime, float TimeConstant);

	/** Moves Current toward Target by at most Rate * DeltaTime per component. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static FVector2D RateLimit(const FVector2D& Current, const FVector2D& Target, float Rate, float DeltaTime);
};
