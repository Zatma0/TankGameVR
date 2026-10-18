#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TankMovementMath.generated.h"

// Drive tuning. Stick X = steering (right +), Y = throttle (forward +). Speeds in cm/s, turn rates in deg/s.
USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTankDriveTuning
{
	GENERATED_BODY()

	/** Stick deflection (0..1) ignored around the centre for steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float SteeringDeadZone = 0.12f;

	/** Stick deflection (0..1) ignored around the centre for throttle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ThrottleDeadZone = 0.12f;

	/** Multiplier on steering after the dead zone (1 = full lock at full deflection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SteeringSensitivity = 1.0f;

	/** Response exponent: 1 = linear, > 1 = finer control near the centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float SteeringCurve = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float ThrottleCurve = 1.2f;

	/** Smoothing time on the raw stick, 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "0.5", Units = "Seconds"))
	float JoystickSmoothing = 0.06f;

	/** Max change per second of the processed stick values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "50.0"))
	float InputResponseSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MaximumForwardSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float ReverseSpeed = 200.0f;

	/** Speed gain per second when speeding up in the current direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float Acceleration = 150.0f;

	/** Speed loss per second when slowing down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float Deceleration = 220.0f;

	/** Speed loss per second when the stick points the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed", meta = (ClampMin = "1.0"))
	float BrakingStrength = 400.0f;

	/** Turn rate (deg/s) at full steering once the tank moves at TurnRateReferenceSpeed or faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float MaximumTurnRate = 35.0f;

	/** Global multiplier on the turn rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float TurnSpeedMultiplier = 1.0f;

	/** Below this speed the turn rate scales down linearly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0", Units = "CentimetersPerSecond"))
	float TurnRateReferenceSpeed = 150.0f;

	/** Forward speed at full steering with no throttle, so steering alone drives along a curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MinimumMovementSpeedWhileTurning = 120.0f;

	/** Fraction of MinimumMovementSpeedWhileTurning used for small steering inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SmallSteerSpeedRatio = 0.35f;

	/** Speed cap at full steering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
	float MaximumMovementSpeedWhileTurning = 280.0f;

	/** How fast the turn rate may change (deg/s per second), for comfort. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning", meta = (ClampMin = "1.0"))
	float MaximumTurnAcceleration = 90.0f;

	/** Allow turning on the spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning")
	bool bAllowPivotTurn = false;

	/** Car-like steering when reversing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turning")
	bool bInvertSteeringInReverse = false;
};

// Drive math, kept separate so it can be tested.
UCLASS()
class TANKGAMEVR_API UTankMovementMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Dead zone, sensitivity and response curve. Output in [-1, 1]. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ShapeAxis(float Value, float DeadZone, float Curve, float Sensitivity);

	/** Desired signed speed (cm/s) for processed steering and throttle in [-1, 1]. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ComputeTargetSpeed(const FTankDriveTuning& Tuning, float Steering, float Throttle);

	/** Desired turn rate (deg/s, positive = right) for processed steering at the current speed. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float ComputeTargetTurnRate(const FTankDriveTuning& Tuning, float Steering, float CurrentSpeed);

	/** Moves CurrentSpeed toward TargetSpeed using acceleration, deceleration or braking. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static float StepSpeed(const FTankDriveTuning& Tuning, float CurrentSpeed, float TargetSpeed, float DeltaTime);

	/** Frame-rate independent exponential smoothing. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static FVector2D SmoothStick(const FVector2D& Previous, const FVector2D& Raw, float DeltaTime, float TimeConstant);

	/** Moves Current toward Target by at most Rate * DeltaTime per component. */
	UFUNCTION(BlueprintPure, Category = "Tank|Math")
	static FVector2D RateLimit(const FVector2D& Current, const FVector2D& Target, float Rate, float DeltaTime);
};
