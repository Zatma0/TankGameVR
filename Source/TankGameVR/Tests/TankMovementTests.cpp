#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Tank/TankMovementComponent.h"
#include "Tank/TankMovementMath.h"
#include "Tank/TankVehicle.h"

using namespace TankTests;

namespace TankMovementTests
{
	struct FDriveResult
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float StartYaw = 0.0f;
		float YawChange = 0.0f;
		float MaxSlipDegrees = 0.0f;
		float FinalSpeed = 0.0f;
	};

	/** Drives a fresh tank with a constant stick for Seconds and measures path and heading. */
	static FDriveResult Drive(const FVector2D& Stick, float Seconds, float Dt = 1.0f / 90.0f, TFunction<void(FWorld&)> Setup = nullptr)
	{
		FWorld Test;
		if (Setup)
		{
			Setup(Test);
		}
		ATankVehicle* Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
		Test.Tick(0.1f, Dt);

		FDriveResult Result;
		Result.Start = Tank->GetActorLocation();
		Result.StartYaw = Tank->GetActorRotation().Yaw;
		float AccumulatedYaw = 0.0f;
		float PreviousYaw = Result.StartYaw;
		FVector Previous = Result.Start;
		Tank->SetFallbackInputs(Stick, FVector2D::ZeroVector);
		for (float Time = 0.0f; Time < Seconds - 1e-4f; Time += Dt)
		{
			Test.Wrapper.TickTestWorld(Dt);
			const FVector Now = Tank->GetActorLocation();
			const float Yaw = Tank->GetActorRotation().Yaw;
			AccumulatedYaw += FRotator::NormalizeAxis(Yaw - PreviousYaw);
			const FVector Step = (Now - Previous) * FVector(1, 1, 0);
			if (Step.Size() > 0.05)
			{
				// Angle between the actual step and the hull heading (mid-step): must stay tiny (no sideways slide).
				const FVector Heading = FRotator(0.0f, (Yaw + PreviousYaw) * 0.5f, 0.0f).Vector();
				const float Slip = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FMath::Abs(FVector::DotProduct(Step.GetSafeNormal(), Heading)), 0.0, 1.0)));
				Result.MaxSlipDegrees = FMath::Max(Result.MaxSlipDegrees, Slip);
			}
			Previous = Now;
			PreviousYaw = Yaw;
		}
		Result.End = Tank->GetActorLocation();
		Result.YawChange = AccumulatedYaw;
		Result.FinalSpeed = Tank->GetTankMovement()->CurrentSpeed;
		return Result;
	}
}

using namespace TankMovementTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankShapeAxisTest, "Tank.Movement.InputShaping", Flags)
bool FTankShapeAxisTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("inside dead zone -> 0"), UTankMovementMath::ShapeAxis(0.1f, 0.12f, 1.6f, 1.0f), 0.0f);
	TestTrue(TEXT("full deflection -> 1"), FMath::IsNearlyEqual(UTankMovementMath::ShapeAxis(1.0f, 0.12f, 1.6f, 1.0f), 1.0f, 1e-4f));
	TestTrue(TEXT("symmetric"), FMath::IsNearlyEqual(UTankMovementMath::ShapeAxis(-0.6f, 0.12f, 1.6f, 1.0f), -UTankMovementMath::ShapeAxis(0.6f, 0.12f, 1.6f, 1.0f), 1e-5f));
	TestEqual(TEXT("NaN input -> 0"), UTankMovementMath::ShapeAxis(NAN, 0.12f, 1.6f, 1.0f), 0.0f);
	float Previous = 0.0f;
	for (float V = 0.0f; V <= 1.0f; V += 0.05f)
	{
		const float Shaped = UTankMovementMath::ShapeAxis(V, 0.12f, 1.6f, 1.0f);
		TestTrue(TEXT("monotonic"), Shaped >= Previous - 1e-6f);
		Previous = Shaped;
	}
	TestTrue(TEXT("curve > 1 makes small deflections gentler than linear"), UTankMovementMath::ShapeAxis(0.4f, 0.12f, 1.6f, 1.0f) < UTankMovementMath::ShapeAxis(0.4f, 0.12f, 1.0f, 1.0f));

	const FTankDriveTuning Tuning;
	TestTrue(TEXT("steering alone gives forward motion (no pivot)"), UTankMovementMath::ComputeTargetSpeed(Tuning, 1.0f, 0.0f) > 0.0f);
	TestTrue(TEXT("more steering -> more speed when throttle is neutral"), UTankMovementMath::ComputeTargetSpeed(Tuning, 1.0f, 0.0f) > UTankMovementMath::ComputeTargetSpeed(Tuning, 0.3f, 0.0f));
	TestTrue(TEXT("hard steering caps full throttle"), UTankMovementMath::ComputeTargetSpeed(Tuning, 1.0f, 1.0f) <= Tuning.MaximumMovementSpeedWhileTurning + 0.01f);
	TestEqual(TEXT("neutral stick -> stop"), UTankMovementMath::ComputeTargetSpeed(Tuning, 0.0f, 0.0f), 0.0f);
	TestTrue(TEXT("reverse"), UTankMovementMath::ComputeTargetSpeed(Tuning, 0.0f, -1.0f) < 0.0f);
	TestEqual(TEXT("no turn rate without movement"), UTankMovementMath::ComputeTargetTurnRate(Tuning, 1.0f, 0.0f), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankSteeringCurveTest, "Tank.Movement.SteeringCurves", Flags)
bool FTankSteeringCurveTest::RunTest(const FString& Parameters)
{
	const FDriveResult SmallRight = Drive(FVector2D(0.35, 0.0), 6.0f);
	const FDriveResult LargeRight = Drive(FVector2D(1.0, 0.0), 6.0f);
	const FDriveResult SmallLeft = Drive(FVector2D(-0.35, 0.0), 6.0f);
	const FDriveResult LargeLeft = Drive(FVector2D(-1.0, 0.0), 6.0f);

	AddInfo(FString::Printf(TEXT("small right: yaw %+.1f, moved %s"), SmallRight.YawChange, *(SmallRight.End - SmallRight.Start).ToCompactString()));
	AddInfo(FString::Printf(TEXT("large right: yaw %+.1f, moved %s"), LargeRight.YawChange, *(LargeRight.End - LargeRight.Start).ToCompactString()));

	TestTrue(TEXT("small right deflection turns right (noticeably, not barely)"), SmallRight.YawChange > 5.0f);
	TestTrue(TEXT("small deflection still drives along its curve"), (SmallRight.End - SmallRight.Start).Size2D() > 150.0);
	TestTrue(TEXT("large right deflection turns right more strongly"), LargeRight.YawChange > SmallRight.YawChange * 2.0f);
	TestTrue(TEXT("small left deflection turns left"), SmallLeft.YawChange < -1.0f);
	TestTrue(TEXT("large left deflection turns left more strongly"), LargeLeft.YawChange < SmallLeft.YawChange * 2.0f);
	TestTrue(TEXT("left and right are mirror images"), FMath::IsNearlyEqual(LargeRight.YawChange, -LargeLeft.YawChange, 0.5f));

	// Facing +X: a right curve moves forward (+X) and drifts right (+Y), a left curve drifts to -Y.
	TestTrue(TEXT("right curve moves forward"), LargeRight.End.X - LargeRight.Start.X > 100.0);
	TestTrue(TEXT("right curve bends to the right"), LargeRight.End.Y - LargeRight.Start.Y > 50.0);
	TestTrue(TEXT("left curve bends to the left"), LargeLeft.End.Y - LargeLeft.Start.Y < -50.0);
	TestTrue(TEXT("steering alone never pivots in place"), (SmallRight.End - SmallRight.Start).Size2D() > 50.0);

	for (const FDriveResult* Result : { &SmallRight, &LargeRight, &SmallLeft, &LargeLeft })
	{
		TestTrue(FString::Printf(TEXT("no sideways sliding (max slip %.2f deg)"), Result->MaxSlipDegrees), Result->MaxSlipDegrees < 1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankStraightAndNeutralTest, "Tank.Movement.ThrottleAndNeutral", Flags)
bool FTankStraightAndNeutralTest::RunTest(const FString& Parameters)
{
	const FDriveResult Forward = Drive(FVector2D(0.0, 1.0), 5.0f);
	TestTrue(TEXT("full throttle drives straight ahead"), Forward.End.X - Forward.Start.X > 800.0);
	TestTrue(TEXT("no heading change without steering"), FMath::Abs(Forward.YawChange) < 0.01f);
	TestTrue(TEXT("speed builds up gradually, not instantly (acceleration)"), Forward.FinalSpeed <= 450.0f + 0.1f);

	const FDriveResult Reverse = Drive(FVector2D(0.0, -1.0), 3.0f);
	TestTrue(TEXT("pulling back reverses"), Reverse.End.X < Reverse.Start.X - 100.0);

	const FDriveResult Neutral = Drive(FVector2D::ZeroVector, 2.0f);
	TestTrue(TEXT("neutral stick: tank stays put"), (Neutral.End - Neutral.Start).Size2D() < 0.5);

	// Release after driving: decelerates to a stop without drifting sideways.
	FWorld Test;
	ATankVehicle* Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
	Test.Tick(0.1f);
	Tank->SetFallbackInputs(FVector2D(0.0, 1.0), FVector2D::ZeroVector);
	Test.Tick(3.0f);
	const float SpeedWhileDriving = Tank->GetTankMovement()->CurrentSpeed;
	Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	Test.Tick(0.5f);
	const float SpeedAfterRelease = Tank->GetTankMovement()->CurrentSpeed;
	Test.Tick(4.0f);
	TestTrue(TEXT("releasing the stick decelerates"), SpeedAfterRelease < SpeedWhileDriving && SpeedAfterRelease > 0.0f);
	TestTrue(TEXT("and eventually stops"), FMath::IsNearlyZero(Tank->GetTankMovement()->CurrentSpeed, 0.01f));
	TestTrue(TEXT("follows the ground at its ride height"), FMath::IsNearlyEqual(Tank->GetActorLocation().Z, 90.0, 1.0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankFrameRateTest, "Tank.Movement.FrameRateIndependence", Flags)
bool FTankFrameRateTest::RunTest(const FString& Parameters)
{
	const FDriveResult At30 = Drive(FVector2D(0.7, 0.6), 5.0f, 1.0f / 30.0f);
	const FDriveResult At90 = Drive(FVector2D(0.7, 0.6), 5.0f, 1.0f / 90.0f);
	const FDriveResult At144 = Drive(FVector2D(0.7, 0.6), 5.0f, 1.0f / 144.0f);
	const float Distance90 = (At90.End - At90.Start).Size2D();
	for (const FDriveResult* Result : { &At30, &At144 })
	{
		const float Gap = (Result->End - At90.End).Size2D();
		TestTrue(FString::Printf(TEXT("path matches 90 fps within 5%% (gap %.1f cm of %.1f cm)"), Gap, Distance90), Gap < Distance90 * 0.05f);
		TestTrue(TEXT("heading matches within 3 deg"), FMath::IsNearlyEqual(Result->YawChange, At90.YawChange, 3.0f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankCollisionTest, "Tank.Movement.BlockedByWall", Flags)
bool FTankCollisionTest::RunTest(const FString& Parameters)
{
	FDriveResult Result = Drive(FVector2D(0.0, 1.0), 6.0f, 1.0f / 90.0f, [](FWorld& Test)
	{
		Test.SpawnBlock(FVector(800, 0, 100), FVector(0.5, 10, 3));
	});
	// Wall face at x = 775, hull front at x + 300.
	TestTrue(FString::Printf(TEXT("tank stops at the wall (front at %.1f)"), Result.End.X + 300.0), Result.End.X + 300.0 <= 776.0);
	TestTrue(TEXT("tank does not slide sideways along the wall"), FMath::Abs(Result.End.Y - Result.Start.Y) < 1.0);
	TestTrue(TEXT("speed reset when blocked"), Result.FinalSpeed < 50.0f);
	return true;
}

#endif
