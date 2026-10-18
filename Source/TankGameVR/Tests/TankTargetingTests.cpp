#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Cockpit/CockpitJoystickComponent.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankVehicle.h"
#include "Tank/TankMovementComponent.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetSelectionMath.h"
#include "Targeting/TargetableComponent.h"
#include "UI/CockpitDisplayWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

using namespace TankTests;

namespace TankTargetingTests
{
	const TCHAR* TestSlot = TEXT("TankAutomationTestSettings");

	/** Tank at the origin facing +X with four targets at the given bearings (deg, right positive), 30 m away. */
	struct FTargetScene
	{
		FWorld Test;
		ATankVehicle* Tank = nullptr;
		UTankTargetingComponent* Targeting = nullptr;
		TMap<int32, AActor*> TargetsByBearing;

		explicit FTargetScene(const TArray<int32>& Bearings, float Distance = 3000.0f)
		{
			if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World))
			{
				Settings->SetSaveSlotForTesting(TestSlot);
				Settings->SetAimAssistEnabled(true);
			}
			Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
			Targeting = Tank->GetTargeting();
			for (const int32 Bearing : Bearings)
			{
				const FVector Location = FVector(0, 0, 200) + FRotator(0.0f, Bearing, 0.0f).Vector() * Distance;
				TargetsByBearing.Add(Bearing, Test.SpawnTarget(Location, *FString::Printf(TEXT("T%+d"), Bearing)));
			}
		}

		~FTargetScene()
		{
			UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
		}

		AActor* Current() const
		{
			const UTargetableComponent* Target = Targeting->GetCurrentTarget();
			return Target ? Target->GetOwner() : nullptr;
		}

		int32 CurrentBearing() const
		{
			for (const TPair<int32, AActor*>& Pair : TargetsByBearing)
			{
				if (Pair.Value == Current())
				{
					return Pair.Key;
				}
			}
			return 9999;
		}

		/** Push the aim stick for Seconds, then release for a short while. */
		void Push(float X, float Seconds = 0.15f)
		{
			Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(X, 0.0));
			Test.Tick(Seconds);
			Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
			Test.Tick(0.3f);
		}
	};
}

using namespace TankTargetingTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankProjectionTest, "Tank.Targeting.SightProjection", Flags)
bool FTankProjectionTest::RunTest(const FString& Parameters)
{
	const FTransform Sight(FRotator::ZeroRotator, FVector(0, 0, 100));
	FVector2D UV;
	bool bOnScreen = false;

	TestTrue(TEXT("point ahead is in front"), UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(1000, 0, 100), UV, bOnScreen));
	TestTrue(TEXT("straight ahead -> centre"), UV.Equals(FVector2D(0.5, 0.5), 1e-4) && bOnScreen);

	UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(1000, 300, 100), UV, bOnScreen);
	TestTrue(TEXT("right of the sight -> right half"), UV.X > 0.5 && FMath::IsNearlyEqual(UV.Y, 0.5, 1e-4));
	UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(1000, 0, 300), UV, bOnScreen);
	TestTrue(TEXT("above the sight -> upper half"), UV.Y < 0.5);

	// Exact edge: a point at half the FOV lands on the border.
	const float EdgeY = 1000.0f * FMath::Tan(FMath::DegreesToRadians(35.0f));
	UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(1000, EdgeY, 100), UV, bOnScreen);
	TestTrue(TEXT("half-FOV point is on the right edge"), FMath::IsNearlyEqual(UV.X, 1.0, 1e-3));

	const bool bInFront = UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(-1000, 200, 100), UV, bOnScreen);
	TestFalse(TEXT("behind the sight is reported"), bInFront);
	TestFalse(TEXT("behind the sight is not on screen"), bOnScreen);
	TestTrue(TEXT("behind: clamped to the edge, finite"), FMath::IsFinite(UV.X) && FMath::IsFinite(UV.Y) && UV.X >= 0.0 && UV.X <= 1.0 && UV.Y >= 0.0 && UV.Y <= 1.0);
	TestTrue(TEXT("behind-right is shown on the right edge"), FMath::IsNearlyEqual(UV.X, 1.0, 1e-3));

	UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(1000, 100000, 100), UV, bOnScreen);
	TestTrue(TEXT("far off-screen clamps inside the display"), !bOnScreen && UV.X <= 1.0 && UV.X >= 0.0);

	UTankSightMath::ProjectToSight(Sight, 70.0f, 4.0f / 3.0f, FVector(NAN, 0, 0), UV, bOnScreen);
	TestTrue(TEXT("NaN input never produces NaN output"), FMath::IsFinite(UV.X) && FMath::IsFinite(UV.Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankRegistryTest, "Tank.Targeting.RegistryLifecycle", Flags)
bool FTankRegistryTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(Test.World);
	if (!TestNotNull(TEXT("registry exists in game worlds"), Registry))
	{
		return false;
	}
	AActor* A = Test.SpawnTarget(FVector(1000, 0, 0), TEXT("A"));
	AActor* B = Test.SpawnTarget(FVector(1000, 500, 0), TEXT("B"));
	TestEqual(TEXT("spawned targets register themselves"), Registry->GetValidTargetCount(), 2);

	B->FindComponentByClass<UTargetableComponent>()->SetTargetable(false);
	TestEqual(TEXT("disabled target is not valid"), Registry->GetValidTargetCount(), 1);
	B->FindComponentByClass<UTargetableComponent>()->SetTargetable(true);

	A->Destroy();
	TestEqual(TEXT("destroyed target leaves immediately"), Registry->GetValidTargetCount(), 1);
	Test.Tick(0.1f);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestEqual(TEXT("no stale entries after GC"), Registry->GetValidTargetCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankTargetCyclingTest, "Tank.Targeting.LeftRightCycling", Flags)
bool FTankTargetCyclingTest::RunTest(const FString& Parameters)
{
	FTargetScene Scene({ -30, -10, 15, 40 });
	Scene.Test.Tick(0.5f);
	TestEqual(TEXT("auto-acquires the target closest to the aim (not just the nearest)"), Scene.CurrentBearing(), -10);

	Scene.Push(1.0f);
	TestEqual(TEXT("right -> next target to the right"), Scene.CurrentBearing(), 15);
	Scene.Push(1.0f);
	TestEqual(TEXT("right again -> next one"), Scene.CurrentBearing(), 40);
	Scene.Push(1.0f);
	TestEqual(TEXT("right at the rightmost target: stays (no wrap)"), Scene.CurrentBearing(), 40);
	Scene.Push(-1.0f);
	TestEqual(TEXT("left -> back one"), Scene.CurrentBearing(), 15);
	Scene.Push(-1.0f);
	Scene.Push(-1.0f);
	TestEqual(TEXT("left twice -> leftmost"), Scene.CurrentBearing(), -30);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankTargetStabilityTest, "Tank.Targeting.NoJitterAndControlledRepeat", Flags)
bool FTankTargetStabilityTest::RunTest(const FString& Parameters)
{
	FTargetScene Scene({ -30, -10, 15, 40 });
	Scene.Test.Tick(0.5f);
	TestEqual(TEXT("start"), Scene.CurrentBearing(), -10);

	// Noisy stick hovering around the threshold must not keep flipping targets.
	int32 Changes = 0;
	Scene.Targeting->OnTargetChanged.Clear();
	UTargetableComponent* Last = Scene.Targeting->GetCurrentTarget();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Scene.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(Frame % 2 ? 0.5 : 0.6, 0.0));
		Scene.Test.Wrapper.TickTestWorld(1.0f / 90.0f);
		if (Scene.Targeting->GetCurrentTarget() != Last)
		{
			++Changes;
			Last = Scene.Targeting->GetCurrentTarget();
		}
	}
	TestTrue(FString::Printf(TEXT("jittery input around the threshold: %d change(s) in 1.3 s (one push + at most the timed repeats)"), Changes), Changes <= 2);

	// Holding the stick steps at a controlled rate, not every frame.
	FTargetScene Held({ -30, -10, 15, 40 });
	Held.Test.Tick(0.5f);
	Held.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(1.0, 0.0));
	Held.Test.Tick(0.3f);
	TestEqual(TEXT("holding right: one step at first"), Held.CurrentBearing(), 15);
	Held.Test.Tick(0.5f);
	TestEqual(TEXT("holding right: repeats after the repeat delay"), Held.CurrentBearing(), 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankTargetLifecycleTest, "Tank.Targeting.DestroyedTargetsAndWaves", Flags)
bool FTankTargetLifecycleTest::RunTest(const FString& Parameters)
{
	FTargetScene Scene({ -10, 15 });
	Scene.Test.Tick(0.5f);
	TestEqual(TEXT("locked"), Scene.CurrentBearing(), -10);

	Scene.TargetsByBearing[-10]->Destroy();
	Scene.Test.Tick(0.1f);
	TestEqual(TEXT("destroyed target replaced by the remaining one"), Scene.CurrentBearing(), 15);

	Scene.TargetsByBearing[15]->Destroy();
	Scene.Test.Tick(0.5f);
	TestNull(TEXT("no targets -> lock cleared"), Scene.Targeting->GetCurrentTarget());

	// A new wave arrives.
	AActor* NewEnemy = Scene.Test.SpawnTarget(FVector(2500, 300, 200), TEXT("Wave2"));
	Scene.Test.Tick(0.5f);
	TestTrue(TEXT("new wave enemy is acquired automatically"), Scene.Current() == NewEnemy);

	// Too far / outside the angle never selected.
	FTargetScene Limits({ 0 });
	Limits.Test.SpawnTarget(FVector(-3000, 0, 200), TEXT("Behind"));
	Limits.Test.SpawnTarget(FVector(30000, 0, 200), TEXT("TooFar"));
	Limits.Test.Tick(0.5f);
	TestEqual(TEXT("only the eligible target is chosen"), Limits.CurrentBearing(), 0);
	Limits.TargetsByBearing[0]->Destroy();
	Limits.Test.Tick(0.5f);
	TestNull(TEXT("behind/too-far targets are never locked"), Limits.Targeting->GetCurrentTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankAimAssistToggleTest, "Tank.Targeting.AimAssistSetting", Flags)
bool FTankAimAssistToggleTest::RunTest(const FString& Parameters)
{
	FTargetScene Scene({ -10, 15 });
	Scene.Test.Tick(0.5f);
	UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Scene.Test.World);
	if (!TestNotNull(TEXT("settings subsystem"), Settings))
	{
		return false;
	}
	TestNotNull(TEXT("assist on: target locked"), Scene.Targeting->GetCurrentTarget());

	const FVector AimBefore = Scene.Targeting->GetAimDirection();
	Settings->SetAimAssistEnabled(false);
	Scene.Test.Wrapper.TickTestWorld(1.0f / 90.0f);
	TestFalse(TEXT("setting reaches the live system immediately"), Scene.Targeting->IsAimAssistEnabled());
	TestNull(TEXT("assist off: lock cleared"), Scene.Targeting->GetCurrentTarget());
	TestTrue(TEXT("no jump when the lock is released"), Scene.Targeting->GetAimDirection().Equals(AimBefore, 0.01));

	Scene.Test.Tick(2.0f);
	TestNull(TEXT("assist off: nothing gets locked again"), Scene.Targeting->GetCurrentTarget());

	// Manual aim: stick right for one second turns the aim right.
	const float YawBefore = Scene.Targeting->GetAimDirection().Rotation().Yaw;
	Scene.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(1.0, 0.0));
	Scene.Test.Tick(1.0f);
	Scene.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	Scene.Test.Tick(0.5f);
	const float YawAfter = Scene.Targeting->GetAimDirection().Rotation().Yaw;
	TestTrue(FString::Printf(TEXT("manual aim follows the stick (%.1f -> %.1f deg)"), YawBefore, YawAfter), YawAfter - YawBefore > 30.0f);
	TestNull(TEXT("manual aim does not lock even when crossing a target"), Scene.Targeting->GetCurrentTarget());

	// Persistence: a fresh load from the slot returns the saved value.
	Settings->LoadSettings();
	TestFalse(TEXT("setting persisted (off)"), Settings->IsAimAssistEnabled());
	Settings->SetAimAssistEnabled(true);
	Settings->LoadSettings();
	TestTrue(TEXT("setting persisted (on)"), Settings->IsAimAssistEnabled());
	Scene.Test.Tick(0.5f);
	TestNotNull(TEXT("turning assist back on locks again"), Scene.Targeting->GetCurrentTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankCrosshairTest, "Tank.Targeting.CrosshairFollowsTarget", Flags)
bool FTankCrosshairTest::RunTest(const FString& Parameters)
{
	FTargetScene Scene({ -10, 15 });
	Scene.Test.Tick(1.0f);

	UCockpitDisplayWidget* Display = CreateWidget<UCockpitDisplayWidget>(Scene.Test.World, UCockpitDisplayWidget::StaticClass());
	if (!TestNotNull(TEXT("display widget"), Display))
	{
		return false;
	}
	Display->SetTargeting(Scene.Targeting);

	auto TargetUV = [&](int32 Bearing)
	{
		FVector2D UV;
		bool bOnScreen = false;
		const FTransform Sight = Scene.Targeting->GetSightTransform();
		UTankSightMath::ProjectToSight(Sight, Display->SightFieldOfView, Display->DisplaySize.X / Display->DisplaySize.Y,
			Scene.TargetsByBearing[Bearing]->FindComponentByClass<UTargetableComponent>()->GetTargetLocation(), UV, bOnScreen);
		return UV;
	};

	Display->UpdateDisplay();
	TestTrue(FString::Printf(TEXT("crosshair sits on the locked target (%s vs %s)"), *Display->GetCrosshairUV().ToString(), *TargetUV(-10).ToString()),
		Display->GetCrosshairUV().Equals(TargetUV(-10), 0.01));
	TestTrue(TEXT("display shows the target"), Display->GetTargetString().Contains(TEXT("T-10")));
	TestTrue(TEXT("display shows assist state"), Display->GetStatusString().Contains(TEXT("ON")));

	// Switch right: the crosshair travels across over several frames (visible transition), then settles.
	Scene.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(1.0, 0.0));
	Scene.Test.Wrapper.TickTestWorld(1.0f / 90.0f);
	Scene.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	Scene.Test.Tick(0.05f);
	Display->UpdateDisplay();
	const float MidX = Display->GetCrosshairUV().X;
	TestTrue(FString::Printf(TEXT("crosshair in transit (x %.3f between %.3f and %.3f)"), MidX, TargetUV(-10).X, TargetUV(15).X), MidX > TargetUV(-10).X + 0.01 && MidX < TargetUV(15).X - 0.01);
	Scene.Test.Tick(1.0f);
	Display->UpdateDisplay();
	TestTrue(TEXT("crosshair arrives on the new target"), Display->GetCrosshairUV().Equals(TargetUV(15), 0.01));

	Scene.TargetsByBearing[-10]->Destroy();
	Scene.TargetsByBearing[15]->Destroy();
	Scene.Test.Tick(0.5f);
	Display->UpdateDisplay();
	TestTrue(TEXT("no targets: idle text"), Display->GetTargetString().Contains(TEXT("NO TARGET")));
	TestTrue(TEXT("crosshair stays valid"), FMath::IsFinite(Display->GetCrosshairUV().X) && FMath::IsFinite(Display->GetCrosshairUV().Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankJoystickTest, "Tank.Cockpit.JoystickFromHand", Flags)
bool FTankJoystickTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
	Test.Tick(0.1f);
	UCockpitJoystickComponent* Stick = Tank->GetDriveStick();

	// The hand belongs to the rider, who is attached to (and moves with) the tank.
	AActor* HandActor = Test.World->SpawnActor<AActor>();
	USceneComponent* Hand = NewObject<USceneComponent>(HandActor, TEXT("Hand"));
	HandActor->SetRootComponent(Hand);
	Hand->RegisterComponent();
	HandActor->AttachToActor(Tank, FAttachmentTransformRules::KeepWorldTransform);

	// Grab at the knob (25 cm above the pivot): no jump.
	const FTransform Base = Stick->GetComponentTransform();
	Hand->SetWorldLocation(Base.TransformPosition(FVector(0, 0, 25)));
	Stick->SetHeldByHand(Hand);
	Test.Tick(0.05f);
	TestTrue(TEXT("grabbing does not move the stick"), Stick->GetValue().IsNearlyZero(0.01));
	TestTrue(TEXT("stick reports held"), Stick->IsHeld());

	// Push the hand right and forward by ~half the max tilt.
	const float HalfTilt = FMath::DegreesToRadians(Stick->MaxTiltAngle * 0.5f);
	Hand->SetWorldLocation(Base.TransformPosition(FVector(0, 25.0 * FMath::Tan(HalfTilt), 25)));
	Test.Tick(0.05f);
	TestTrue(FString::Printf(TEXT("right push -> +X about 0.5 (%.2f)"), Stick->GetValue().X), FMath::IsNearlyEqual(Stick->GetValue().X, 0.5, 0.05));
	Hand->SetWorldLocation(Base.TransformPosition(FVector(25.0 * FMath::Tan(HalfTilt * 2.0f), 0, 25)));
	Test.Tick(0.05f);
	TestTrue(FString::Printf(TEXT("forward push -> +Y about 1 (%.2f, %.2f)"), Stick->GetValue().X, Stick->GetValue().Y), Stick->GetValue().Y > 0.9);

	// The held stick drives the tank (and wins over the thumbstick fallback).
	Tank->SetFallbackInputs(FVector2D(0.0, -1.0), FVector2D::ZeroVector);
	Test.Tick(1.0f);
	TestTrue(TEXT("held stick drives forward even with thumbstick pulled back"), Tank->GetTankMovement()->CurrentSpeed > 0.0f);

	Stick->ReleaseHand();
	Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	Test.Tick(0.5f);
	TestTrue(TEXT("released stick springs back to centre"), Stick->GetValue().IsNearlyZero(0.01));

	// Thumbstick moves the visual stick when not held.
	Tank->SetFallbackInputs(FVector2D(1.0, 0.0), FVector2D::ZeroVector);
	Test.Tick(0.1f);
	TestTrue(TEXT("thumbstick deflects the cockpit stick"), FMath::IsNearlyEqual(Stick->GetValue().X, 1.0, 0.01));
	TestTrue(TEXT("visual tilt follows the value"), !Stick->GetRelativeRotation().Equals(FRotator::ZeroRotator, 1.0f));
	return true;
}

#endif
