#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Cockpit/CockpitCigaretteComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TankTargetingComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankCannonComponent.h"
#include "Weapons/TankShellComponent.h"
#include "Weapons/TankShellProjectile.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

using namespace TankTests;

namespace TankWeaponTests
{
	struct FGunTest
	{
		FWorld Test;
		ATankVehicle* Tank = nullptr;
		UTankBreechComponent* Breech = nullptr;
		UTankShellComponent* Round = nullptr;
		USceneComponent* Hand = nullptr;

		FGunTest()
		{
			Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
			Test.Tick(0.2f);
			Breech = Tank->GetBreech();
			Round = Tank->GetShells().Last();
		}

		/** A "hand" riding in the tank, holding the round at its middle. */
		void Grab(UCockpitGrabbableComponent* Item)
		{
			AActor* HandActor = Test.World->SpawnActor<AActor>();
			Hand = NewObject<USceneComponent>(HandActor, TEXT("HandLeft"));
			HandActor->SetRootComponent(Hand);
			Hand->RegisterComponent();
			Hand->SetWorldLocation(Item->GetComponentLocation());
			HandActor->AttachToActor(Tank, FAttachmentTransformRules::KeepWorldTransform);
			Item->SetHeldByHand(Hand);
			Test.Tick(0.05f);
		}

		/** Nose just outside the mouth, round lined up with the bore. */
		void BringToMouth()
		{
			Hand->AddWorldOffset(Breech->GetComponentLocation() - Breech->GetForwardVector() * 2.0 - Round->GetNoseLocation());
			Test.Tick(0.05f);
		}

		void Push(float Centimeters, int32 Steps = 10)
		{
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Hand->AddWorldOffset(Breech->GetForwardVector() * (Centimeters / Steps));
				Test.Tick(0.03f);
			}
		}

		int32 CountProjectiles() const
		{
			int32 Count = 0;
			for (TActorIterator<ATankShellProjectile> It(Test.World); It; ++It)
			{
				Count += IsValid(*It) ? 1 : 0;
			}
			return Count;
		}
	};
}

using namespace TankWeaponTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankLoadAndFireTest, "Tank.Weapons.LoadByHandAndFire", TankTestFlags)
bool FTankLoadAndFireTest::RunTest(const FString& Parameters)
{
	FGunTest Gun;
	// A 50 HP block 30 m in front of the barrel.
	AStaticMeshActor* Target = Gun.Test.SpawnBlock(FVector(3000, 0, 200), FVector(2, 4, 4));
	UTankHealthComponent* Health = NewObject<UTankHealthComponent>(Target);
	Health->MaxHealth = 50.0f;
	Health->DestroyDelay = 0.0f;
	Health->RegisterComponent();
	Gun.Test.Tick(0.1f);

	TestEqual(TEXT("breech starts open"), Gun.Breech->GetState(), ETankBreechState::Open);
	TestTrue(TEXT("display asks for a round"), Gun.Breech->GetStatusText().Contains(TEXT("EMPTY")));
	TestFalse(TEXT("firing while empty does nothing"), Gun.Tank->Fire());
	TestEqual(TEXT("no round left the barrel"), Gun.CountProjectiles(), 0);

	Gun.Grab(Gun.Round);
	TestTrue(TEXT("round is in the hand"), Gun.Round->IsHeld());
	const FVector Before = Gun.Round->GetComponentLocation();
	Gun.Hand->AddWorldOffset(FVector(0, 0, 10));
	Gun.Test.Tick(0.05f);
	TestTrue(TEXT("round follows the hand"), FVector::Dist(Gun.Round->GetComponentLocation(), Before + FVector(0, 0, 10)) < 0.5);

	Gun.BringToMouth();
	TestEqual(TEXT("nose at the mouth -> ramming"), Gun.Breech->GetState(), ETankBreechState::Ramming);
	TestTrue(TEXT("round is now on the bore axis"), Gun.Round->IsOnRail(Gun.Breech));

	Gun.Push(60.0f);
	TestTrue(TEXT("pushed in -> breech closes"), Gun.Breech->GetState() == ETankBreechState::Closing || Gun.Breech->IsLoaded());
	Gun.Test.Tick(0.5f);
	TestTrue(TEXT("loaded"), Gun.Breech->IsLoaded());
	TestTrue(TEXT("display says UP"), Gun.Breech->GetStatusText().StartsWith(TEXT("UP")));
	const FVector RoundInBreech = Gun.Breech->GetComponentTransform().InverseTransformPosition(Gun.Round->GetNoseLocation());
	TestTrue(FString::Printf(TEXT("round is fully home (nose depth %.1f)"), RoundInBreech.X), FMath::IsNearlyEqual(RoundInBreech.X, Gun.Breech->RamDepth, 0.5));

	Gun.Round->ReleaseHand();
	Gun.Test.Tick(0.1f);
	TestEqual(TEXT("letting go keeps the round in the breech"), Gun.Round->GetState(), ECockpitGrabState::Locked);

	TestTrue(TEXT("fires when loaded"), Gun.Tank->Fire());
	TestEqual(TEXT("one round in flight"), Gun.CountProjectiles(), 1);
	for (TActorIterator<ATankShellProjectile> It(Gun.Test.World); It; ++It)
	{
		const FVector Velocity = It->GetProjectileMovement()->Velocity.GetSafeNormal();
		TestTrue(TEXT("round flies along the aim"), FVector::DotProduct(Velocity, Gun.Tank->GetTargeting()->GetAimDirection()) > 0.999);
	}
	TestFalse(TEXT("fired round is used up"), Gun.Round->IsAvailable());
	Gun.Test.Tick(0.4f);
	TestEqual(TEXT("hit took 25 HP"), Health->GetHealth(), 25.0f);
	TestFalse(TEXT("second shot needs a new round"), Gun.Tank->Fire());

	Gun.Test.Tick(0.8f);
	TestEqual(TEXT("breech opens again after recoil"), Gun.Breech->GetState(), ETankBreechState::Open);
	Gun.Test.Tick(Gun.Round->RefillDelay);
	TestTrue(TEXT("rack slot refilled"), Gun.Round->IsAvailable() && Gun.Round->IsAtHome());
	TestTrue(TEXT("refilled round is back in its slot"), Gun.Round->GetRelativeTransform().Equals(Gun.Round->GetHomeRelativeTransform(), 0.1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankWrongAngleTest, "Tank.Weapons.OnlyAlignedRoundsGoIn", TankTestFlags)
bool FTankWrongAngleTest::RunTest(const FString& Parameters)
{
	FGunTest Gun;
	Gun.Grab(Gun.Round);

	// Sideways: nose at the mouth, but 90 degrees off the bore.
	Gun.Hand->SetWorldRotation(FRotator(0.0, 90.0, 0.0));
	Gun.Test.Tick(0.05f);
	Gun.BringToMouth();
	Gun.Test.Tick(0.1f);
	TestEqual(TEXT("sideways round is not accepted"), Gun.Breech->GetState(), ETankBreechState::Open);

	// Straight again: goes in. Pull it back out: free in the hand again.
	Gun.Hand->SetWorldRotation(FRotator::ZeroRotator);
	Gun.Test.Tick(0.05f);
	Gun.BringToMouth();
	TestEqual(TEXT("lined-up round is accepted"), Gun.Breech->GetState(), ETankBreechState::Ramming);
	Gun.Push(-30.0f, 6);
	TestEqual(TEXT("pulled back out"), Gun.Breech->GetState(), ETankBreechState::Open);
	TestTrue(TEXT("still in the hand, off the rail"), Gun.Round->IsHeld() && !Gun.Round->IsOnRail());

	// Half way in and let go: it slides back to the rack.
	Gun.BringToMouth();
	Gun.Push(15.0f, 3);
	TestEqual(TEXT("half way"), Gun.Breech->GetState(), ETankBreechState::Ramming);
	Gun.Round->ReleaseHand();
	Gun.Test.Tick(1.5f);
	TestEqual(TEXT("breech open again"), Gun.Breech->GetState(), ETankBreechState::Open);
	TestTrue(TEXT("round back in the rack"), Gun.Round->IsAtHome() && Gun.Round->GetRelativeTransform().Equals(Gun.Round->GetHomeRelativeTransform(), 0.1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankAutoLoadTest, "Tank.Weapons.AutoLoadFallback", TankTestFlags)
bool FTankAutoLoadTest::RunTest(const FString& Parameters)
{
	FGunTest Gun;
	for (UTankShellComponent* Shell : Gun.Tank->GetShells())
	{
		Shell->RefillDelay = 60.0f;
	}
	TestTrue(TEXT("auto load starts"), Gun.Breech->AutoLoad());
	TestFalse(TEXT("no second auto load while busy"), Gun.Breech->AutoLoad());
	Gun.Test.Tick(Gun.Breech->AutoLoadTime + Gun.Breech->CloseTime + 0.2f);
	TestTrue(TEXT("loaded without hands"), Gun.Breech->IsLoaded());

	int32 Fired = 0;
	for (int32 Shot = 0; Shot < 3; ++Shot)
	{
		Fired += Gun.Tank->Fire() ? 1 : 0;
		Gun.Test.Tick(0.8f);
		Gun.Breech->AutoLoad();
		Gun.Test.Tick(Gun.Breech->AutoLoadTime + Gun.Breech->CloseTime + 0.2f);
	}
	TestEqual(TEXT("three rounds from the rack"), Fired, 3);
	TestEqual(TEXT("shots counted"), Gun.Tank->GetCannon()->GetShotsFired(), 3);
	TestFalse(TEXT("rack is empty until it refills"), Gun.Breech->IsLoaded());
	TestTrue(TEXT("display says refilling"), Gun.Breech->GetStatusText().Contains(TEXT("REFILLING")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankHealthTest, "Tank.Weapons.HealthAndDeath", TankTestFlags)
bool FTankHealthTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	AActor* Enemy = Test.SpawnTarget(FVector(2000, 0, 100), TEXT("Enemy"));
	UTankHealthComponent* Health = NewObject<UTankHealthComponent>(Enemy);
	Health->MaxHealth = 50.0f;
	Health->DestroyDelay = 0.0f;
	Health->RegisterComponent();
	UTargetableComponent* Targetable = Enemy->FindComponentByClass<UTargetableComponent>();

	UGameplayStatics::ApplyDamage(Enemy, 20.0f, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("standard damage lowers HP"), Health->GetHealth(), 30.0f);
	TestTrue(TEXT("still a target"), Targetable->IsValidTarget());
	UGameplayStatics::ApplyDamage(Enemy, 40.0f, nullptr, nullptr, UDamageType::StaticClass());
	TestTrue(TEXT("dead at 0 HP"), Health->IsDead() && Health->GetHealth() == 0.0f);
	TestTrue(TEXT("destroyed"), !IsValid(Enemy) || Enemy->IsActorBeingDestroyed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankCigaretteTest, "Tank.Cockpit.CigaretteDragAndSmoke", TankTestFlags)
bool FTankCigaretteTest::RunTest(const FString& Parameters)
{
	FGunTest Gun;
	UCockpitCigaretteComponent* Cigarette = Gun.Tank->GetCigarette();

	// Head of a seated rider.
	AActor* HeadActor = Gun.Test.World->SpawnActor<AActor>();
	USceneComponent* Head = NewObject<USceneComponent>(HeadActor, TEXT("Head"));
	HeadActor->SetRootComponent(Head);
	Head->RegisterComponent();
	Head->SetWorldLocation(Gun.Tank->GetActorTransform().TransformPosition(FVector(130, -70, 130)));
	HeadActor->AttachToActor(Gun.Tank, FAttachmentTransformRules::KeepWorldTransform);
	Cigarette->SetHeadOverride(Head);

	const float IdleHeat = Cigarette->GetHeat();
	Gun.Grab(Cigarette);
	TestTrue(TEXT("cigarette in the hand"), Cigarette->IsHeld());
	TestFalse(TEXT("not smoking at the dashboard"), Cigarette->IsDragging());

	const FVector Mouth = Head->GetComponentTransform().TransformPosition(Cigarette->MouthOffset);
	const FVector Filter = Cigarette->GetComponentTransform().TransformPosition(FVector(Cigarette->FilterOffset, 0, 0));
	Gun.Hand->AddWorldOffset(Mouth - Filter);
	Gun.Test.Tick(1.0f);
	TestTrue(TEXT("filter at the mouth -> drag"), Cigarette->IsDragging());
	TestTrue(FString::Printf(TEXT("ember glows (%.2f)"), Cigarette->GetHeat()), Cigarette->GetHeat() > 0.9f);
	TestEqual(TEXT("no smoke while inhaling"), Cigarette->GetVisiblePuffs(), 0);

	Gun.Hand->AddWorldOffset(FVector(10, 0, -30));
	Gun.Test.Tick(0.7f);
	TestFalse(TEXT("away from the mouth"), Cigarette->IsDragging());
	TestEqual(TEXT("one drag"), Cigarette->GetDragCount(), 1);
	TestTrue(TEXT("blowing out smoke"), Cigarette->GetVisiblePuffs() > 0);

	Cigarette->ReleaseHand();
	Gun.Test.Tick(3.0f);
	TestTrue(TEXT("back on the ashtray"), Cigarette->IsAtHome());
	TestTrue(TEXT("ember cools down"), Cigarette->GetHeat() < 0.5f && Cigarette->GetHeat() >= IdleHeat - 0.01f);
	TestEqual(TEXT("smoke gone"), Cigarette->GetVisiblePuffs(), 0);
	return true;
}

#endif
