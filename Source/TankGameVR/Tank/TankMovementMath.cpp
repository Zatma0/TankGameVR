#include "Tank/TankMovementMath.h"

float UTankMovementMath::ShapeAxis(float Value, float DeadZone, float Curve, float Sensitivity)
{
	if (!FMath::IsFinite(Value))
	{
		return 0.0f;
	}
	const float Magnitude = FMath::Abs(Value);
	const float Zone = FMath::Clamp(DeadZone, 0.0f, 0.95f);
	if (Magnitude <= Zone)
	{
		return 0.0f;
	}
	const float Rescaled = FMath::Clamp((Magnitude - Zone) / (1.0f - Zone) * Sensitivity, 0.0f, 1.0f);
	return FMath::Sign(Value) * FMath::Pow(Rescaled, FMath::Max(Curve, 0.1f));
}

float UTankMovementMath::ComputeTargetSpeed(const FTankDriveTuning& Tuning, float Steering, float Throttle)
{
	float Target = Throttle >= 0.0f ? Throttle * Tuning.MaximumForwardSpeed : Throttle * Tuning.ReverseSpeed;
	const float SteerAmount = FMath::Clamp(FMath::Abs(Steering), 0.0f, 1.0f);
	if (SteerAmount <= 0.0f)
	{
		return Target;
	}

	if (Target >= 0.0f)
	{
		// Steering alone still moves forward (no pivot).
		if (!Tuning.bAllowPivotTurn)
		{
			const float CreepSpeed = Tuning.MinimumMovementSpeedWhileTurning * FMath::Lerp(Tuning.SmallSteerSpeedRatio, 1.0f, SteerAmount);
			Target = FMath::Max(Target, CreepSpeed);
		}
		const float Cap = FMath::Lerp(Tuning.MaximumForwardSpeed, FMath::Min(Tuning.MaximumMovementSpeedWhileTurning, Tuning.MaximumForwardSpeed), SteerAmount);
		Target = FMath::Min(Target, Cap);
	}
	else
	{
		const float ReverseCap = FMath::Lerp(Tuning.ReverseSpeed, FMath::Min(Tuning.MaximumMovementSpeedWhileTurning, Tuning.ReverseSpeed), SteerAmount);
		Target = FMath::Max(Target, -ReverseCap);
	}
	return Target;
}

float UTankMovementMath::ComputeTargetTurnRate(const FTankDriveTuning& Tuning, float Steering, float CurrentSpeed)
{
	float SpeedFactor = FMath::Clamp(FMath::Abs(CurrentSpeed) / FMath::Max(Tuning.TurnRateReferenceSpeed, 1.0f), 0.0f, 1.0f);
	if (Tuning.bAllowPivotTurn)
	{
		SpeedFactor = 1.0f;
	}
	const float Direction = (CurrentSpeed < -1.0f && Tuning.bInvertSteeringInReverse) ? -1.0f : 1.0f;
	return FMath::Clamp(Steering, -1.0f, 1.0f) * Tuning.MaximumTurnRate * Tuning.TurnSpeedMultiplier * SpeedFactor * Direction;
}

float UTankMovementMath::StepSpeed(const FTankDriveTuning& Tuning, float CurrentSpeed, float TargetSpeed, float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return CurrentSpeed;
	}
	float Rate = Tuning.Acceleration;
	const bool bOppositeDirection = (CurrentSpeed > 1.0f && TargetSpeed < 0.0f) || (CurrentSpeed < -1.0f && TargetSpeed > 0.0f);
	if (bOppositeDirection)
	{
		Rate = Tuning.BrakingStrength;
	}
	else if (FMath::Abs(TargetSpeed) < FMath::Abs(CurrentSpeed))
	{
		Rate = Tuning.Deceleration;
	}
	return FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, FMath::Max(Rate, 1.0f));
}

FVector2D UTankMovementMath::SmoothStick(const FVector2D& Previous, const FVector2D& Raw, float DeltaTime, float TimeConstant)
{
	if (TimeConstant <= UE_SMALL_NUMBER || DeltaTime <= 0.0f)
	{
		return Raw;
	}
	const float Alpha = 1.0f - FMath::Exp(-DeltaTime / TimeConstant);
	return FMath::Lerp(Previous, Raw, Alpha);
}

FVector2D UTankMovementMath::RateLimit(const FVector2D& Current, const FVector2D& Target, float Rate, float DeltaTime)
{
	const float Step = FMath::Max(Rate, 0.0f) * FMath::Max(DeltaTime, 0.0f);
	return FVector2D(
		FMath::FInterpConstantTo(Current.X, Target.X, 1.0f, Step),
		FMath::FInterpConstantTo(Current.Y, Target.Y, 1.0f, Step));
}
