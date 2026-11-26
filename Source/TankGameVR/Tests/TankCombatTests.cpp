#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Cockpit/CockpitExtinguisherComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Cockpit/CockpitSmokeLauncherComponent.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankGroundFire.h"
#include "Combat/TankHealthComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/EnemyWeaponComponent.h"
#include "Enemies/TankEnemy.h"
#include "Enemies/TankEnemyTypes.h"
#include "Game/TankArenaDirector.h"
#include "Game/TankEnemySpawnPoint.h"
#include "Game/TankLevelSetupSubsystem.h"
#include "Testing/TankPlaceholderEnemyStub.h"
#include "NavigationSystem.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankVehicle.h"
#include "Tank/TankVisionConeComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankCannonComponent.h"
#include "Weapons/TankShellProjectile.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

using namespace TankTests;

namespace TankCombatTests
{
	const TCHAR* CombatTestSlot = TEXT("TankAutomationCombatSettings");

	ATankVehicle* SpawnPlayer(FWorld& Test, const FVector& Location = FVector(0, 0, 90))
	{
		if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World))
		{
			Settings->SetSaveSlotForTesting(CombatTestSlot);
			Settings->SetAimAssistEnabled(true);
		}
		return Test.World->SpawnActor<ATankVehicle>(LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")), FTransform(Location));
	}

	ATankEnemy* SpawnEnemy(FWorld& Test, const TCHAR* Name, const FVector& Location, float Yaw = 180.0f)
	{
		UClass* Class = LoadBlueprintClass(FString(TEXT("/Game/TankGame/Blueprints/Enemies/")) + Name);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Test.World->SpawnActor<ATankEnemy>(Class, FTransform(FRotator(0.0f, Yaw, 0.0f), Location), Params);
	}

	void Damage(AActor* Target, float Amount, TSubclassOf<UDamageType> Type)
	{
		UGameplayStatics::ApplyDamage(Target, Amount, nullptr, nullptr, Type);
	}

	/** Bare scene component used as a VR hand (name decides the side). */
	USceneComponent* MakeHand(FWorld& Test, const TCHAR* Name)
	{
		AActor* Holder = Test.World->SpawnActor<AActor>();
		USceneComponent* Hand = NewObject<USceneComponent>(Holder, Name);
		Holder->SetRootComponent(Hand);
		Hand->RegisterComponent();
		return Hand;
	}
}

using namespace TankCombatTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankEnemyStatsTest, "Tank.Combat.EnemyStatsFromTable", TankTestFlags)
bool FTankEnemyStatsTest::RunTest(const FString& Parameters)
{
	const UDataTable* Table = ATankEnemy::FindDefaultStatsTable();
	if (!TestNotNull(TEXT("Ugurkan's DT_EnemyFeatures loads"), Table))
	{
		return false;
	}
	FTankEnemyStats Tank;
	TestTrue(TEXT("Tank_Row read"), UTankEnemyStatsLibrary::ReadEnemyStats(Table, TEXT("Tank_Row"), Tank));
	TestEqual(TEXT("tank HP"), Tank.Health, 50.0f);
	TestEqual(TEXT("tank damage"), Tank.Damage, 20.0f);
	TestTrue(FString::Printf(TEXT("10 km/h -> cm/s (%.1f)"), Tank.Speed), FMath::IsNearlyEqual(Tank.Speed, 277.8f, 0.5f));
	TestEqual(TEXT("20 m -> cm"), Tank.Range, 2000.0f);
	TestNotNull(TEXT("model read"), Tank.Model.Get());

	FTankEnemyStats Missing;
	Missing.Health = 77.0f;
	TestFalse(TEXT("unknown row fails"), UTankEnemyStatsLibrary::ReadEnemyStats(Table, TEXT("Nope_Row"), Missing));
	TestEqual(TEXT("unknown row keeps defaults"), Missing.Health, 77.0f);

	// The Blueprints pick up the table values.
	FWorld Test;
	ATankEnemy* Car = SpawnEnemy(Test, TEXT("BP_Enemy_ArmoredCar"), FVector(0, 0, 100));
	ATankEnemy* Gun = SpawnEnemy(Test, TEXT("BP_Enemy_AntiTank"), FVector(2000, 0, 100));
	if (!TestNotNull(TEXT("armored car spawns"), Car) || !TestNotNull(TEXT("AT gun spawns"), Gun))
	{
		return false;
	}
	TestEqual(TEXT("armored car HP from the table"), Car->GetHealth()->GetHealth(), 20.0f);
	TestTrue(TEXT("armored car is fast (40 km/h)"), FMath::IsNearlyEqual(Car->GetDrive()->MaxSpeed, 1111.1f, 1.0f));
	TestEqual(TEXT("burst splits the table damage"), Car->GetWeapon()->Damage * Car->GetWeapon()->BurstCount, 10.0f);
	TestEqual(TEXT("AT gun damage from the table"), Gun->GetWeapon()->Damage, 40.0f);
	TestEqual(TEXT("AT gun range 25 m"), Gun->GetEngageRange(), 2500.0f);
	TestEqual(TEXT("target name"), Gun->GetTargetable()->GetTargetDisplayName().ToString(), FString(TEXT("AT GUN")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankArmorTest, "Tank.Combat.ArmorAndRoundTypes", TankTestFlags)
bool FTankArmorTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankEnemy* Tank = SpawnEnemy(Test, TEXT("BP_Enemy_Tank"), FVector(0, 0, 100));
	ATankEnemy* Car = SpawnEnemy(Test, TEXT("BP_Enemy_ArmoredCar"), FVector(3000, 0, 100));
	ATankEnemy* Gun = SpawnEnemy(Test, TEXT("BP_Enemy_AntiTank"), FVector(-3000, 0, 100));
	if (!Tank || !Car || !Gun)
	{
		AddError(TEXT("enemy Blueprints missing"));
		return false;
	}
	Damage(Tank, 25.0f, UTankDamageType_AP::StaticClass());
	TestEqual(TEXT("AP does full damage to a tank"), Tank->GetHealth()->GetHealth(), 25.0f);
	Damage(Tank, 25.0f, UTankDamageType_HE::StaticClass());
	TestEqual(TEXT("HE does half to a tank"), Tank->GetHealth()->GetHealth(), 12.5f);

	Damage(Car, 25.0f, UTankDamageType_AP::StaticClass());
	TestEqual(TEXT("AP mostly goes through a car"), Car->GetHealth()->GetHealth(), 5.0f);
	Damage(Gun, 25.0f, UTankDamageType_HE::StaticClass());
	TestTrue(TEXT("one HE round wrecks the AT gun"), Gun->IsWrecked());
	TestFalse(TEXT("wreck can't be locked"), Gun->GetTargetable()->IsTargetable());
	Test.Tick(0.5f);
	TestTrue(TEXT("wreck stays for a while"), IsValid(Gun));

	// The breech knows which round it holds and the cannon fires the matching projectile.
	ATankVehicle* Player = SpawnPlayer(Test, FVector(0, 6000, 90));
	Test.Tick(0.1f);
	TestTrue(TEXT("load HE"), Player->GetBreech()->AutoLoadType(ETankRoundType::HE));
	Test.Tick(1.6f);
	TestEqual(TEXT("HE loaded"), Player->GetBreech()->GetLoadedRoundType(), ETankRoundType::HE);
	TestTrue(TEXT("status says HE"), Player->GetBreech()->GetStatusText().Contains(TEXT("HE")));
	TestEqual(TEXT("one HE left in the rack"), Player->GetBreech()->CountAvailable(ETankRoundType::HE), 1);
	TestTrue(TEXT("fire HE"), Player->Fire());
	ATankShellProjectile* Fired = nullptr;
	for (TActorIterator<ATankShellProjectile> It(Test.World); It; ++It)
	{
		Fired = It->GetOwner() == Player ? *It : Fired;
	}
	if (TestNotNull(TEXT("projectile"), Fired))
	{
		TestTrue(TEXT("HE has a blast"), Fired->SplashRadius > 0.0f && Fired->DamageType == UTankDamageType_HE::StaticClass());
	}
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankSplashTest, "Tank.Combat.HESplashAndNoFriendlyFire", TankTestFlags)
bool FTankSplashTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankEnemy* CarA = SpawnEnemy(Test, TEXT("BP_Enemy_ArmoredCar"), FVector(2000, 0, 100));
	ATankEnemy* CarB = SpawnEnemy(Test, TEXT("BP_Enemy_ArmoredCar"), FVector(2000, 330, 100));
	Test.Tick(0.1f);

	// An HE round into car A: the blast also hurts car B next to it.
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	const FTransform Transform(FVector(800, 0, 80));
	ATankShellProjectile* Round = Test.World->SpawnActor<ATankShellProjectile>(ATankShellProjectile::StaticClass(), Transform, Params);
	Round->Damage = 25.0f;
	Round->DamageType = UTankDamageType_HE::StaticClass();
	Round->SplashDamage = 18.0f;
	Round->SplashRadius = 450.0f;
	Round->Launch(FVector::ForwardVector, 15000.0f, nullptr);
	Round->FinishSpawning(Transform);
	Test.Tick(0.3f);
	TestTrue(TEXT("direct HE hit wrecks car A"), CarA->IsWrecked());
	TestTrue(FString::Printf(TEXT("blast damages car B (HP %.1f)"), CarB->GetHealth()->GetHealth()), CarB->GetHealth()->GetHealth() < 20.0f);

	// An enemy round flies through another enemy and hits the player.
	ATankVehicle* Player = SpawnPlayer(Test, FVector(-2500, 0, 90));
	ATankEnemy* Shooter = SpawnEnemy(Test, TEXT("BP_Enemy_Tank"), FVector(2500, -3000, 100));
	ATankEnemy* InTheWay = SpawnEnemy(Test, TEXT("BP_Enemy_Tank"), FVector(0, -3000, 100));
	Player->SetActorLocation(FVector(-2500, -3000, 90));
	Test.Tick(0.1f);
	const FTransform EnemyShot(FVector(1800, -3000, 90));
	ATankShellProjectile* EnemyRound = Test.World->SpawnActorDeferred<ATankShellProjectile>(ATankShellProjectile::StaticClass(), EnemyShot, Shooter);
	EnemyRound->Damage = 20.0f;
	EnemyRound->SetFriendlyClass(ATankEnemy::StaticClass());
	EnemyRound->Launch(-FVector::ForwardVector, 15000.0f, nullptr);
	EnemyRound->FinishSpawning(EnemyShot);
	Test.Tick(0.6f);
	TestEqual(TEXT("enemy in the way not hurt"), InTheWay->GetHealth()->GetHealth(), InTheWay->GetHealth()->MaxHealth);
	TestEqual(TEXT("player hit"), Player->GetHealth()->GetHealth(), 80.0f);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankEnemyTankTest, "Tank.Combat.TankClosesInAndFires", TankTestFlags)
bool FTankEnemyTankTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	ATankEnemy* Enemy = SpawnEnemy(Test, TEXT("BP_Enemy_Tank"), FVector(4500, 800, 100));
	if (!TestNotNull(TEXT("tank spawns"), Enemy))
	{
		return false;
	}
	const float Start = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()));
	bool bHalted = false;
	float MaxYawStep = 0.0f;
	float LastYaw = Enemy->GetActorRotation().Yaw;
	float FirstShotDistance = -1.0f;
	for (int32 Step = 0; Step < 90 * 30 && !bHalted; ++Step)
	{
		Test.Tick(1.0f / 90.0f);
		MaxYawStep = FMath::Max(MaxYawStep, FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Enemy->GetActorRotation().Yaw)));
		LastYaw = Enemy->GetActorRotation().Yaw;
		bHalted = Enemy->GetEnemyState() == ETankEnemyState::Engaging;
		if (FirstShotDistance < 0.0f && Enemy->GetWeapon()->GetShotsFired() > 0)
		{
			FirstShotDistance = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()));
		}
	}
	// Then a shot from the halt.
	const int32 ShotsBeforeHalt = Enemy->GetWeapon()->GetShotsFired();
	for (int32 Step = 0; Step < 90 * 12 && Enemy->GetWeapon()->GetShotsFired() == ShotsBeforeHalt; ++Step)
	{
		Test.Tick(1.0f / 90.0f);
	}
	TestTrue(FString::Printf(TEXT("opens fire on the move, from outside its halt range (%.0f cm)"), FirstShotDistance), FirstShotDistance > Enemy->GetEngageRange());
	TestTrue(TEXT("fires from the halt too"), Enemy->GetWeapon()->GetShotsFired() > ShotsBeforeHalt);
	const float Now = static_cast<float>(FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()));
	TestTrue(FString::Printf(TEXT("closed in (%.0f -> %.0f cm)"), Start, Now), Now < Start - 1000.0f && Now < Enemy->GetEngageRange() * 1.3f);
	TestTrue(TEXT("stopped for the shot"), bHalted);
	TestTrue(TEXT("fired"), Enemy->GetWeapon()->GetShotsFired() > 0);
	TestTrue(FString::Printf(TEXT("turns like a vehicle (max %.2f deg/frame)"), MaxYawStep), MaxYawStep < 1.0f);
	TestTrue(TEXT("doesn't drive into the player"), Now > 800.0f);
	// Let the round arrive.
	Test.Tick(1.5f);
	TestTrue(FString::Printf(TEXT("player hull hit (%.0f HP)"), Player->GetHealth()->GetHealth()), Player->GetHealth()->GetHealth() < 100.0f);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankEnemyCarTest, "Tank.Combat.ArmoredCarCirclesAndBursts", TankTestFlags)
bool FTankEnemyCarTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	ATankEnemy* Car = SpawnEnemy(Test, TEXT("BP_Enemy_ArmoredCar"), FVector(3500, 0, 100));
	if (!TestNotNull(TEXT("car spawns"), Car))
	{
		return false;
	}
	float Travelled = 0.0f;
	float Stopped = 0.0f;
	float MinAngle = 0.0f;
	float MaxAngle = 0.0f;
	FVector Last = Car->GetActorLocation();
	for (int32 Step = 0; Step < 90 * 15; ++Step)
	{
		Test.Tick(1.0f / 90.0f);
		const FVector Location = Car->GetActorLocation();
		const float Moved = static_cast<float>(FVector::Dist2D(Location, Last));
		Travelled += Moved;
		Stopped += Moved < 1.0f ? 1.0f / 90.0f : 0.0f;
		Last = Location;
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Location.Y, Location.X));
		MinAngle = FMath::Min(MinAngle, Angle);
		MaxAngle = FMath::Max(MaxAngle, Angle);
	}
	TestTrue(FString::Printf(TEXT("keeps driving (%.0f m)"), Travelled / 100.0f), Travelled > 4000.0f);
	TestTrue(FString::Printf(TEXT("only short stops to fire (%.1f s)"), Stopped), Stopped < 6.0f);
	TestTrue(FString::Printf(TEXT("moves around the player (%.0f..%.0f deg)"), MinAngle, MaxAngle), MaxAngle - MinAngle > 60.0f);
	TestTrue(FString::Printf(TEXT("fires bursts (%d rounds)"), Car->GetWeapon()->GetShotsFired()), Car->GetWeapon()->GetShotsFired() >= 4);
	TestTrue(TEXT("player took autocannon hits"), Player->GetHealth()->GetHealth() < 100.0f);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankEnemyATTest, "Tank.Combat.ATGunLaysThenFiresAndSmokeBreaksIt", TankTestFlags)
bool FTankEnemyATTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	ATankEnemy* Gun = SpawnEnemy(Test, TEXT("BP_Enemy_AntiTank"), FVector(2600, 300, 100));
	if (!TestNotNull(TEXT("AT gun spawns"), Gun))
	{
		return false;
	}
	float FirstThreat = -1.0f;
	float Time = 0.0f;
	for (int32 Step = 0; Step < 90 * 30 && Gun->GetWeapon()->GetShotsFired() == 0; ++Step)
	{
		Test.Tick(1.0f / 90.0f);
		Time += 1.0f / 90.0f;
		if (FirstThreat < 0.0f && Gun->GetThreatTo(Player) > 0.0f)
		{
			FirstThreat = Time;
		}
	}
	TestTrue(TEXT("AT gun fired"), Gun->GetWeapon()->GetShotsFired() == 1);
	TestTrue(TEXT("warning showed before the shot"), FirstThreat > 0.0f);
	TestTrue(FString::Printf(TEXT("laid the gun for at least AimTime (%.1f s)"), Time - FirstThreat), Time - FirstThreat >= Gun->GetWeapon()->AimTime - 0.05f);
	TestTrue(TEXT("deployed before firing"), Gun->GetEnemyState() == ETankEnemyState::Engaging);
	Test.Tick(0.5f);
	TestTrue(FString::Printf(TEXT("heavy hit (%.0f HP)"), Player->GetHealth()->GetHealth()), Player->GetHealth()->GetHealth() <= 60.0f);

	// Next lay: the warning comes up, then smoke between us breaks it.
	for (int32 Step = 0; Step < 90 * 12 && Gun->GetWeapon()->GetAimProgress() < 0.15f; ++Step)
	{
		Test.Tick(1.0f / 90.0f);
	}
	TestTrue(TEXT("laying again"), Gun->GetWeapon()->GetAimProgress() >= 0.15f);
	const FVector Middle = (Gun->GetActorLocation() + Player->GetActorLocation()) * 0.5;
	FActorSpawnParameters Params;
	Params.bDeferConstruction = true;
	const FTransform Screen((Gun->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D().Rotation(), FVector(Middle.X, Middle.Y, 0.0));
	ATankSmokeCloud* Cloud = Test.World->SpawnActor<ATankSmokeCloud>(ATankSmokeCloud::StaticClass(), Screen, Params);
	Cloud->LaunchFrom = Player->GetActorLocation();
	Cloud->FinishSpawning(Screen);
	Test.Tick(3.0f);
	TestTrue(TEXT("smoke blocks the gunner's view"), ATankSmokeCloud::IsLineBlocked(Test.World, Gun->GetWeapon()->GetMuzzleLocation(), Player->GetActorLocation()));
	TestFalse(TEXT("no line of sight through smoke"), Gun->GetWeapon()->HasLineOfSight());
	TestEqual(TEXT("lay lost"), Gun->GetWeapon()->GetAimProgress(), 0.0f);
	TestEqual(TEXT("no second shot through the smoke"), Gun->GetWeapon()->GetShotsFired(), 1);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankFireTest, "Tank.Combat.CockpitFireAndExtinguisher", TankTestFlags)
bool FTankFireTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	Test.Tick(0.1f);
	UCockpitFireComponent* Fire = Player->GetFireSpots()[0];
	Fire->Ignite(0.35f);
	Test.Tick(5.0f);
	TestTrue(TEXT("fire grows"), Fire->GetIntensity() > 0.5f);
	TestTrue(FString::Printf(TEXT("fire burns the hull (%.1f HP)"), Player->GetHealth()->GetHealth()), Player->GetHealth()->GetHealth() < 100.0f);

	// Right hand holds the bottle, nozzle 70 cm from the fire and pointing at it.
	UCockpitExtinguisherComponent* Bottle = Player->GetExtinguisher();
	USceneComponent* Hand = MakeHand(Test, TEXT("RightHand"));
	Hand->SetWorldLocation(Bottle->GetComponentLocation());
	Bottle->SetHeldByHand(Hand);
	Test.Tick(0.05f);
	TestTrue(TEXT("bottle held"), Bottle->IsHeld());
	const FVector Target = Fire->GetComponentLocation() + FVector(0, 0, 10);
	const FVector Nozzle = Target - Player->GetActorForwardVector() * 70.0;
	const FRotator Aim = (Target - Nozzle).Rotation();
	Hand->SetWorldLocationAndRotation(Nozzle - Aim.RotateVector(Bottle->NozzleOffset), Aim);
	Test.Tick(0.05f);
	TestTrue(TEXT("bottle follows the hand"), Bottle->GetSprayDirection().Equals(Aim.Vector(), 0.01));

	// A loaded gun must not go off while that hand's trigger sprays.
	TestTrue(TEXT("load"), Player->GetBreech()->AutoLoad());
	Test.Tick(1.6f);
	const int32 Shots = Player->GetCannon()->GetShotsFired();
	Player->HandleTrigger(false, true);
	TestTrue(TEXT("spraying"), Bottle->IsSpraying());
	Test.Tick(4.0f);
	Player->HandleTrigger(false, false);
	TestFalse(TEXT("fire is out"), Fire->IsBurning());
	TestEqual(TEXT("spraying hand did not fire the gun"), Player->GetCannon()->GetShotsFired(), Shots);
	TestTrue(TEXT("the bottle does not run out"), Bottle->GetCharge() >= 1.0f);
	// The other hand still fires.
	Player->HandleTrigger(true, true);
	TestEqual(TEXT("left trigger fires"), Player->GetCannon()->GetShotsFired(), Shots + 1);
	Bottle->ReleaseHand();

	// Hands-free: G puts out the next fire.
	Fire->Ignite(0.5f);
	TestTrue(TEXT("auto spray"), Bottle->AutoSpray());
	Test.Tick(3.0f);
	TestFalse(TEXT("auto spray puts it out"), Fire->IsBurning());

	// Hits start fires, but only one at a time and not right after the last one.
	UTankDamageReactionComponent* Reaction = Player->GetDamageReaction();
	auto Burning = [Player]()
	{
		int32 Count = 0;
		for (const UCockpitFireComponent* Spot : Player->GetFireSpots())
		{
			Count += Spot->IsBurning() ? 1 : 0;
		}
		return Count;
	};
	Reaction->HeavyHitFireChance = 1.0f;
	Reaction->MinTimeBetweenFires = 0.0f;
	Damage(Player, 15.0f, UTankDamageType_AP::StaticClass());
	TestEqual(TEXT("a heavy hit starts a fire"), Burning(), 1);
	Damage(Player, 15.0f, UTankDamageType_AP::StaticClass());
	Damage(Player, 15.0f, UTankDamageType_AP::StaticClass());
	TestEqual(TEXT("three heavy hits: two fires in different places, never three"), Burning(), 2);
	// Left alone they do not spread to a third place either.
	Player->GetHealth()->ResetHealth();
	Test.Tick(20.0f);
	TestEqual(TEXT("still two after burning for a while"), Burning(), 2);
	for (UCockpitFireComponent* Spot : Player->GetFireSpots())
	{
		Spot->PutOut();
	}
	Player->GetHealth()->ResetHealth();
	Reaction->MinTimeBetweenFires = 60.0f;
	Damage(Player, 15.0f, UTankDamageType_AP::StaticClass());
	TestEqual(TEXT("no new fire right after the last one"), Burning(), 0);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankGroundFireTest, "Tank.Combat.HELeavesBurningGround", TankTestFlags)
bool FTankGroundFireTest::RunTest(const FString& Parameters)
{
	auto CountFires = [](UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<ATankGroundFire> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	};

	// AP into a wall leaves nothing, HE leaves the ground below burning.
	{
		FWorld Test;
		ATankVehicle* Player = SpawnPlayer(Test);
		Test.SpawnStaticBlock(FVector(2500, 0, 300), FVector(1, 20, 6));
		Test.Tick(0.2f);
		TestTrue(TEXT("load AP"), Player->GetBreech()->AutoLoadType(ETankRoundType::AP));
		Test.Tick(1.8f);
		TestTrue(TEXT("AP fired"), Player->Fire());
		Test.Tick(1.0f);
		TestEqual(TEXT("AP leaves no fire"), CountFires(Test.World), 0);
		TestTrue(FString::Printf(TEXT("AP report (%s)"), *Player->GetCannon()->GetShotReport()), Player->GetCannon()->GetShotReport().StartsWith(TEXT("AP")));

		Test.Tick(2.0f);
		TestTrue(TEXT("load HE"), Player->GetBreech()->AutoLoadType(ETankRoundType::HE));
		Test.Tick(1.8f);
		TestTrue(TEXT("HE fired"), Player->Fire());
		Test.Tick(1.0f);
		TestEqual(TEXT("HE leaves burning ground"), CountFires(Test.World), 1);
		TestTrue(FString::Printf(TEXT("HE report (%s)"), *Player->GetCannon()->GetShotReport()), Player->GetCannon()->GetShotReport().StartsWith(TEXT("HE")));
		UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	}

	// Whoever is in it burns: an enemy tank, and our own tank too.
	{
		FWorld Test;
		ATankVehicle* Player = SpawnPlayer(Test);
		ATankEnemy* Enemy = SpawnEnemy(Test, TEXT("BP_Enemy_Tank"), FVector(4000, 0, 100));
		Test.Tick(0.2f);
		Enemy->GetDrive()->Freeze();
		// It must not shoot at us while we measure the fire.
		Enemy->SetActorTickEnabled(false);
		Enemy->GetWeapon()->SetComponentTickEnabled(false);
		const TWeakObjectPtr<ATankGroundFire> Fire = ATankGroundFire::StartBelow(Test.World, ATankGroundFire::StaticClass(), Enemy->GetActorLocation() + FVector(0, 150, 0), Player, nullptr);
		if (!TestTrue(TEXT("fire on the ground"), Fire.IsValid()))
		{
			return false;
		}
		const float EnemyBefore = Enemy->GetHealth()->GetHealth();
		const float PlayerBefore = Player->GetHealth()->GetHealth();
		Test.Tick(3.0f);
		TestTrue(FString::Printf(TEXT("enemy tank in the fire loses HP (%.1f of %.1f)"), Enemy->GetHealth()->GetHealth(), EnemyBefore), Enemy->GetHealth()->GetHealth() < EnemyBefore);
		TestEqual(TEXT("our tank, 40 m away, is fine"), Player->GetHealth()->GetHealth(), PlayerBefore);
		TestFalse(TEXT("not burning where we stand"), ATankGroundFire::IsBurningAt(Test.World, Player->GetActorLocation()));

		const TWeakObjectPtr<ATankGroundFire> Mine = ATankGroundFire::StartBelow(Test.World, ATankGroundFire::StaticClass(), Player->GetActorLocation(), Player, nullptr);
		Test.Tick(3.0f);
		TestTrue(TEXT("burning under us"), ATankGroundFire::IsBurningAt(Test.World, Player->GetActorLocation()));
		TestTrue(FString::Printf(TEXT("our own tank burns in it too (%.1f HP)"), Player->GetHealth()->GetHealth()), Player->GetHealth()->GetHealth() < PlayerBefore);

		// It burns out by itself.
		Test.Tick(ATankGroundFire::StaticClass()->GetDefaultObject<ATankGroundFire>()->Duration + 1.0f);
		TestFalse(TEXT("burnt out"), Fire.IsValid() || Mine.IsValid());
		UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankHullRepairTest, "Tank.Combat.HullRepairsWhenLeftAlone", TankTestFlags)
bool FTankHullRepairTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	Test.Tick(0.1f);
	UTankHealthComponent* Hull = Player->GetHealth();
	// No cockpit fires here, they would burn HP on their own.
	Player->GetDamageReaction()->HeavyHitFireChance = 0.0f;
	Player->GetDamageReaction()->LightHitFireChance = 0.0f;
	TestTrue(TEXT("the player tank repairs itself"), Hull->RepairPerSecond > 0.0f);

	Damage(Player, 40.0f, UTankDamageType_AP::StaticClass());
	const float AfterHit = Hull->GetHealth();
	Test.Tick(Hull->RepairDelay - 1.0f);
	TestEqual(TEXT("nothing comes back right after a hit"), Hull->GetHealth(), AfterHit);
	TestFalse(TEXT("not repairing yet"), Hull->IsRepairing());
	Test.Tick(6.0f);
	const float Gained = Hull->GetHealth() - AfterHit;
	TestTrue(TEXT("repairing once left alone"), Hull->IsRepairing());
	TestTrue(FString::Printf(TEXT("slowly: +%.1f HP in 5 s at %.1f per second"), Gained, Hull->RepairPerSecond), FMath::IsNearlyEqual(Gained, 5.0f * Hull->RepairPerSecond, 0.6f));

	// The next hit stops it again.
	Damage(Player, 15.0f, UTankDamageType_AP::StaticClass());
	const float AfterSecond = Hull->GetHealth();
	Test.Tick(3.0f);
	TestEqual(TEXT("a new hit stops the repair"), Hull->GetHealth(), AfterSecond);

	// Up to full, not beyond.
	Hull->RepairPerSecond = 50.0f;
	Test.Tick(Hull->RepairDelay + 3.0f);
	TestEqual(TEXT("stops at full"), Hull->GetHealth(), Hull->MaxHealth);
	TestFalse(TEXT("nothing left to repair"), Hull->IsRepairing());

	// A destroyed tank stays destroyed.
	Damage(Player, 1000.0f, UTankDamageType_AP::StaticClass());
	Test.Tick(Hull->RepairDelay + 3.0f);
	TestTrue(TEXT("no repair after it is destroyed"), Hull->IsDead() && Hull->GetHealth() <= 0.0f);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankPlaceholderEnemyTest, "Tank.Combat.PlaceholderEnemiesBecomeReal", TankTestFlags)
bool FTankPlaceholderEnemyTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	SpawnPlayer(Test);
	Test.Tick(0.1f);
	UTankLevelSetupSubsystem* Setup = Test.World->GetSubsystem<UTankLevelSetupSubsystem>();
	if (!TestNotNull(TEXT("level setup subsystem"), Setup))
	{
		return false;
	}
	Setup->SetPlaceholderEnemyClass(ATankPlaceholderEnemyStub::StaticClass());
	ATankPlaceholderEnemyStub::TotalDied = 0;

	// The way a Blueprint spawner does it: deferred spawn, set the type, finish.
	auto SpawnStub = [&Test](uint8 Type, const FVector& Location)
	{
		const FTransform Where(Location);
		ATankPlaceholderEnemyStub* Stub = Test.World->SpawnActorDeferred<ATankPlaceholderEnemyStub>(ATankPlaceholderEnemyStub::StaticClass(), Where);
		Stub->SpawnedEnemyType = Type;
		Stub->FinishSpawning(Where);
		return Stub;
	};
	auto CountEnemies = [&Test]()
	{
		int32 Count = 0;
		for (TActorIterator<ATankEnemy> It(Test.World); It; ++It)
		{
			++Count;
		}
		return Count;
	};

	// An "armored car" dropped in mid-air 30 m behind the player, where nobody is looking.
	const TWeakObjectPtr<ATankPlaceholderEnemyStub> Stub = SpawnStub(1, FVector(-3000, 500, 600));
	Test.Tick(0.2f);
	ATankEnemy* Car = Setup->GetReplacement(Stub.Get());
	if (!TestNotNull(TEXT("the placeholder got a real enemy"), Car))
	{
		return false;
	}
	TestTrue(TEXT("of the type the spawner asked for"), Car->EnemyType == ETankEnemyType::ArmoredCar);
	TestTrue(FString::Printf(TEXT("on the ground where it was dropped (%s)"), *Car->GetActorLocation().ToCompactString()),
		FVector::Dist2D(Car->GetActorLocation(), FVector(-3000, 500, 0)) < 400.0 && Car->GetActorLocation().Z < 200.0);
	TestTrue(TEXT("the placeholder is hidden, not gone"), Stub.IsValid() && Stub->IsHidden());
	ATankArenaDirector* Director = ATankArenaDirector::GetDirector(Test.World);
	TestTrue(TEXT("a director was added and leaves the waves to that spawner"), Director && Director->HasExternalWaves());
	Test.Tick(8.0f);
	TestEqual(TEXT("no waves of our own on top"), CountEnemies(), 1);

	// Killing ours scores and tells the spawner through its own event.
	Damage(Car, 1000.0f, UTankDamageType_HE::StaticClass());
	Test.Tick(0.2f);
	TestEqual(TEXT("the spawner's died event fired once"), ATankPlaceholderEnemyStub::TotalDied, 1);
	TestFalse(TEXT("its placeholder is gone"), Stub.IsValid());
	TestTrue(TEXT("the kill scored"), Director && Director->GetScore() > 0);

	// The spawner removing its enemy takes ours with it, without a kill. This one is dropped right in front of
	// the player, so it comes in somewhere out of view instead.
	const TWeakObjectPtr<ATankPlaceholderEnemyStub> Second = SpawnStub(0, FVector(3000, -500, 300));
	Test.Tick(0.1f);
	const TWeakObjectPtr<ATankEnemy> Tank = Setup->GetReplacement(Second.Get());
	TestTrue(TEXT("second placeholder became a tank"), Tank.IsValid() && Tank->EnemyType == ETankEnemyType::Tank);
	TestTrue(TEXT("not in plain view of the player"), Tank.IsValid() && Director && !Director->PlayerCanSee(Tank->GetActorLocation())
		&& FVector::Dist2D(Tank->GetActorLocation(), FVector(3000, -500, 0)) > 1000.0);
	Second->Destroy();
	Test.Tick(0.2f);
	TestFalse(TEXT("ours is removed with it"), Tank.IsValid());
	TestEqual(TEXT("no died event for that"), ATankPlaceholderEnemyStub::TotalDied, 1);
	TestEqual(TEXT("two swaps in total"), Setup->GetReplacedCount(), 2);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankLevelSetupTest, "Tank.Combat.LevelSetupFillsTheGaps", TankTestFlags)
bool FTankLevelSetupTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);

	// A "building" 20 m ahead whose mesh has no simple collision (the engine's sky dome is such a mesh).
	UStaticMesh* Bare = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EngineSky/SM_SkySphere.SM_SkySphere"));
	if (!TestNotNull(TEXT("mesh without simple collision"), Bare))
	{
		return false;
	}
	AStaticMeshActor* Building = Test.World->SpawnActor<AStaticMeshActor>(FVector(2000, 0, 0), FRotator::ZeroRotator);
	UStaticMeshComponent* Mesh = Building->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Bare);
	Building->SetActorScale3D(FVector(800.0 / Bare->GetBoundingBox().GetSize().GetMax()));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetMobility(EComponentMobility::Static);
	Test.Tick(0.1f);

	auto BlockedAhead = [&Test, Player]()
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(LevelSetupTest), false, Player);
		return Test.World->LineTraceSingleByChannel(Hit, FVector(600, 0, 150), FVector(4000, 0, 150), ECC_Visibility, Params);
	};
	TestFalse(TEXT("before: a shell would fly straight through the building"), BlockedAhead());
	TestNull(TEXT("before: no director"), ATankArenaDirector::GetDirector(Test.World));

	UTankLevelSetupSubsystem* Setup = Test.World->GetSubsystem<UTankLevelSetupSubsystem>();
	if (!TestNotNull(TEXT("level setup subsystem"), Setup))
	{
		return false;
	}
	Setup->SetUpLevel();
	Test.Tick(0.5f);

	const ATankArenaDirector* Director = ATankArenaDirector::GetDirector(Test.World);
	TestTrue(TEXT("a director was added, running its own waves"), Director && !Director->HasExternalWaves());
	TestEqual(TEXT("one blocker for the one bare mesh"), Setup->GetBlockerCount(), 1);
	TestTrue(TEXT("after: the building stops a shell"), BlockedAhead());
	TestEqual(TEXT("asking again adds nothing"), Setup->BlockMeshesWithoutCollision(), 0);

	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Test.World);
	bool bBounds = false;
	if (Nav && Director)
	{
		for (const FNavigationBounds& Bounds : Nav->GetNavigationBounds())
		{
			bBounds |= Bounds.AreaBox.IsValid && FMath::IsNearlyEqual(Bounds.AreaBox.GetSize().X, Director->AutoNavigationSize.X, 10.0);
		}
	}
	// Another spawner in the level takes the waves over with its first enemy, not before.
	Setup->SetPlaceholderEnemyClass(ATankPlaceholderEnemyStub::StaticClass());
	const FTransform Where(FVector(-3000, 0, 300));
	ATankPlaceholderEnemyStub* Stub = Test.World->SpawnActorDeferred<ATankPlaceholderEnemyStub>(ATankPlaceholderEnemyStub::StaticClass(), Where);
	Stub->FinishSpawning(Where);
	Test.Tick(0.2f);
	TestTrue(TEXT("with the other spawner's first enemy the director hands the waves over"), Director && Director->HasExternalWaves() && Setup->GetReplacement(Stub) != nullptr);

	if (Nav)
	{
		TestTrue(TEXT("navigation area around the player"), bBounds);
		TestFalse(TEXT("a level that has a nav volume gets no second one"), Setup->CreateNavigationIfMissing(FVector::ZeroVector, FVector(1000.0)));
	}
	else
	{
		AddInfo(TEXT("test worlds have no navigation system; the navigation part is checked in the running game"));
	}
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankVisionConeTest, "Tank.Combat.VisionConeAndHiddenSpawns", TankTestFlags)
bool FTankVisionConeTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	Test.Tick(0.1f);
	UTankVisionConeComponent* Vision = Player->GetVisionCone();
	if (!TestNotNull(TEXT("the player tank has a vision cone"), Vision))
	{
		return false;
	}
	auto At = [](float Bearing, float Distance) { return FRotator(0.0f, Bearing, 0.0f).Vector() * Distance + FVector(0, 0, 150); };

	// The tank faces +X; no headset in a test, so the view is straight ahead.
	TestTrue(TEXT("sees what is ahead"), Vision->CanSee(At(0, 3000)));
	TestTrue(TEXT("sees up to the edge of the cone"), Vision->CanSee(At(Vision->HalfAngle - 5.0f, 3000)));
	TestFalse(TEXT("not past the edge"), Vision->CanSee(At(Vision->HalfAngle + 5.0f, 3000)));
	TestFalse(TEXT("not behind"), Vision->CanSee(At(180, 3000)));
	TestFalse(TEXT("not beyond the range"), Vision->CanSee(At(0, Vision->Range + 1000.0f)));

	// Behind a building: inside the cone, still not seen.
	AStaticMeshActor* Wall = Test.SpawnStaticBlock(FVector(2000, 0, 300), FVector(1, 12, 6));
	TestTrue(TEXT("a spot behind a wall is inside the cone"), Vision->IsInCone(At(0, 3500)));
	TestFalse(TEXT("but hidden by the wall"), Vision->CanSee(At(0, 3500)));
	Wall->Destroy();

	// Looking the other way swaps what is hidden.
	Vision->SetViewOverride(true, Player->GetActorLocation() + FVector(0, 0, 150), FVector(-1, 0, 0));
	TestTrue(TEXT("looking back: sees behind the tank"), Vision->CanSee(At(180, 3000)));
	TestFalse(TEXT("looking back: no longer sees ahead"), Vision->CanSee(At(0, 3000)));
	Vision->SetViewOverride(false);

	// The director only hands out spots the player is not looking at.
	UClass* DirectorClass = LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector"));
	ATankArenaDirector* Director = Test.World->SpawnActorDeferred<ATankArenaDirector>(DirectorClass, FTransform::Identity);
	Director->bAutoStart = false;
	Director->FinishSpawning(FTransform::Identity);
	int32 InView = 0;
	for (int32 Index = 0; Index < 25; ++Index)
	{
		bool bOutOfSight = false;
		const FTransform Spot = Director->GetHiddenSpawnTransform(ETankEnemyType::Tank, bOutOfSight);
		InView += (!bOutOfSight || Vision->CanSee(Spot.GetLocation())) ? 1 : 0;
		TestTrue(TEXT("away from the player"), FVector::Dist2D(Spot.GetLocation(), Player->GetActorLocation()) >= Director->MinDistanceFromPlayer - 1.0);
	}
	TestEqual(TEXT("25 spots around the player, none of them in view"), InView, 0);

	// With spawn points: the ones in view are left alone.
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Test.World->SpawnActor<ATankEnemySpawnPoint>(At(Index * 60.0f, 4000) - FVector(0, 0, 100), FRotator::ZeroRotator);
	}
	int32 Ahead = 0;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		bool bOutOfSight = false;
		const FTransform Spot = Director->GetHiddenSpawnTransformByIndex(Index % 3, bOutOfSight);
		const float Bearing = FMath::Abs(FRotator::NormalizeAxis(Spot.GetLocation().Rotation().Yaw));
		Ahead += (Bearing < 90.0f) ? 1 : 0;
		// A point that was just used rests for a few seconds.
		Test.Tick(4.5f);
	}
	TestEqual(TEXT("only the spawn points beside and behind the player are used"), Ahead, 0);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankSpawnGroupsTest, "Tank.Combat.EnemiesArriveInOnesAndTwos", TankTestFlags)
bool FTankSpawnGroupsTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	// Nothing hurts the tank here, the test only counts arrivals.
	Player->GetHealth()->DamageMultipliers.Add(UDamageType::StaticClass(), 0.0f);
	UClass* DirectorClass = LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector"));
	ATankArenaDirector* Director = Test.World->SpawnActorDeferred<ATankArenaDirector>(DirectorClass, FTransform::Identity);
	FTankWave Six;
	Six.Tanks = 6;
	Six.ArmoredCars = 0;
	Six.AntiTankGuns = 0;
	Director->Waves = { Six };
	Director->FirstWaveDelay = 0.2f;
	Director->SpawnInterval = 0.3f;
	Director->TimeBetweenGroups = 3.0f;
	Director->LostEnemyTimeout = 0.0f;
	Director->FinishSpawning(FTransform::Identity);

	TArray<float> Arrivals;
	const float Dt = 0.1f;
	for (float Time = 0.0f; Time < 25.0f && Arrivals.Num() < 6; Time += Dt)
	{
		Test.Tick(Dt);
		while (Arrivals.Num() < Director->GetEnemies().Num())
		{
			Arrivals.Add(Time);
		}
	}
	TestEqual(TEXT("all six arrive in the end"), Arrivals.Num(), 6);

	// Split the arrivals where there was a pause.
	int32 Groups = 0;
	int32 Biggest = 0;
	int32 Current = 0;
	for (int32 Index = 0; Index < Arrivals.Num(); ++Index)
	{
		if (Index == 0 || Arrivals[Index] - Arrivals[Index - 1] > 1.5f)
		{
			++Groups;
			Current = 0;
		}
		Biggest = FMath::Max(Biggest, ++Current);
	}
	TestTrue(FString::Printf(TEXT("never more than two at a time (biggest group %d)"), Biggest), Biggest <= 2);
	TestTrue(FString::Printf(TEXT("spread over several groups (%d)"), Groups), Groups >= 3);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankSmokeLauncherTest, "Tank.Combat.SmokeSwitch", TankTestFlags)
bool FTankSmokeLauncherTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	Test.Tick(0.1f);
	UCockpitSmokeLauncherComponent* Smoke = Player->GetSmokeLauncher();
	USceneComponent* Button = nullptr;
	TArray<USceneComponent*> Children;
	Smoke->GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		Button = Child->GetName() == TEXT("SmokeButton") ? Child : Button;
	}
	if (!TestNotNull(TEXT("button"), Button))
	{
		return false;
	}

	// Finger on the button with the cover shut: nothing.
	USceneComponent* Finger = MakeHand(Test, TEXT("RightFinger"));
	Finger->SetWorldLocation(Button->GetComponentLocation() + Button->GetUpVector() * 1.5);
	Smoke->SetFingerOverride(Finger);
	Test.Tick(0.2f);
	TestEqual(TEXT("guarded: shut cover blocks the button"), Smoke->GetCharges(), 2);

	// Cover up, touch again: one salvo per touch.
	Finger->SetWorldLocation(Button->GetComponentLocation() + FVector(0, 0, 30));
	Smoke->SetCoverOpen(true);
	Test.Tick(0.2f);
	TestTrue(TEXT("cover stays open"), Smoke->IsCoverOpen());
	Finger->SetWorldLocation(Button->GetComponentLocation() + Button->GetUpVector() * 1.5);
	Test.Tick(0.5f);
	TestEqual(TEXT("one press, one salvo"), Smoke->GetCharges(), 1);
	int32 Clouds = 0;
	for (TActorIterator<ATankSmokeCloud> It(Test.World); It; ++It)
	{
		++Clouds;
		Test.Tick(2.5f);
		const FVector Far = It->GetActorLocation() + It->GetActorForwardVector() * 2000.0;
		TestTrue(TEXT("screen blocks the view through it"), ATankSmokeCloud::IsLineBlocked(Test.World, Player->GetActorLocation(), Far + FVector(0, 0, 150)));

		// The grenades come out of the launcher banks on both sides of the turret, half from each.
		int32 Left = 0;
		int32 Right = 0;
		const FTransform Hull = Player->GetActorTransform();
		for (const FVector& Start : It->GrenadeStarts)
		{
			const FVector Local = Hull.InverseTransformPositionNoScale(Start);
			Left += (Local.Y < -60.0 && Local.Z > 100.0) ? 1 : 0;
			Right += (Local.Y > 60.0 && Local.Z > 100.0) ? 1 : 0;
		}
		TestEqual(TEXT("one grenade per tube"), It->GrenadeStarts.Num(), Smoke->TubesPerSide * 2);
		TestTrue(FString::Printf(TEXT("left bank %d, right bank %d"), Left, Right), Left == Smoke->TubesPerSide && Right == Smoke->TubesPerSide);
		break;
	}
	TestEqual(TEXT("cloud spawned"), Clouds, 1);
	TestTrue(TEXT("keyboard salvo"), Smoke->Launch());
	TestFalse(TEXT("empty after two"), Smoke->Launch());
	TestTrue(TEXT("display shows reloading"), Smoke->GetStatusText().Contains(TEXT("RELOADING")));
	Test.Tick(Smoke->ReloadTime + 0.5f);
	TestTrue(TEXT("reloads"), Smoke->GetCharges() >= 1);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankArenaTest, "Tank.Combat.ArenaWavesGameOverRestart", TankTestFlags)
bool FTankArenaTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FVector Location = FRotator(0.0f, Index * 60.0f, 0.0f).Vector() * 4000.0 + FVector(0, 0, 50);
		Test.World->SpawnActor<ATankEnemySpawnPoint>(Location, FRotator::ZeroRotator);
	}
	UClass* DirectorClass = LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector"));
	ATankArenaDirector* Director = Test.World->SpawnActorDeferred<ATankArenaDirector>(DirectorClass, FTransform::Identity);
	Director->FirstWaveDelay = 0.5f;
	Director->TimeBetweenWaves = 0.5f;
	Director->SpawnInterval = 0.2f;
	Director->TimeBetweenGroups = 0.2f;
	Director->RestartDelay = 1.0f;
	Director->FinishSpawning(FTransform::Identity);

	Test.Tick(1.5f);
	TestEqual(TEXT("wave 1 running"), Director->GetWave(), 1);
	TArray<ATankEnemy*> Enemies = Director->GetEnemies();
	TestEqual(TEXT("wave 1 = two tanks"), Enemies.Num(), 2);
	for (ATankEnemy* Enemy : Enemies)
	{
		TestEqual(TEXT("wave 1 are tanks"), Enemy->EnemyType, ETankEnemyType::Tank);
		TestTrue(TEXT("spawned at a spawn point, away from the player"), FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) > 3500.0);
		Damage(Enemy, 100.0f, UTankDamageType_AP::StaticClass());
	}
	TestEqual(TEXT("score for two tanks"), Director->GetScore(), 200);
	TestTrue(TEXT("display line shows the score"), Director->GetStatusLine().Contains(TEXT("SCORE 200")));
	Test.Tick(1.5f);
	TestEqual(TEXT("wave 2 starts"), Director->GetWave(), 2);
	bool bCar = false;
	for (ATankEnemy* Enemy : Director->GetEnemies())
	{
		bCar |= Enemy->EnemyType == ETankEnemyType::ArmoredCar;
	}
	TestTrue(TEXT("wave 2 brings armored cars"), bCar);

	// Knocked out: no driving, no firing, game over, then a clean restart.
	Damage(Player, 150.0f, UTankDamageType_AP::StaticClass());
	TestTrue(TEXT("player destroyed"), Player->IsDestroyed());
	TestEqual(TEXT("game over"), Director->GetPhase(), ETankArenaPhase::GameOver);
	TestTrue(TEXT("game over message"), Director->GetMessage().StartsWith(TEXT("TANK DESTROYED")));
	Player->GetBreech()->AutoLoad();
	Test.Tick(0.5f);
	TestFalse(TEXT("dead tank can't fire"), Player->Fire());
	Test.Tick(1.0f);
	TestFalse(TEXT("restarted: tank repaired"), Player->IsDestroyed());
	TestEqual(TEXT("restarted: full hull"), Player->GetHealth()->GetHealth(), 100.0f);
	TestEqual(TEXT("restarted: score reset"), Director->GetScore(), 0);
	TestEqual(TEXT("restarted: no enemies left"), Director->GetEnemies().Num(), 0);
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankHiddenATGunMovesTest, "Tank.Combat.HiddenATGunMovesToANewSpot", TankTestFlags)
bool FTankHiddenATGunMovesTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Player = SpawnPlayer(Test);
	Player->GetHealth()->DamageMultipliers.Add(UDamageType::StaticClass(), 0.0f);
	UClass* DirectorClass = LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector"));
	ATankArenaDirector* Director = Test.World->SpawnActorDeferred<ATankArenaDirector>(DirectorClass, FTransform::Identity);
	FTankWave One;
	One.Tanks = 0;
	One.ArmoredCars = 0;
	One.AntiTankGuns = 1;
	Director->Waves = { One };
	Director->FirstWaveDelay = 0.2f;
	Director->FinishSpawning(FTransform::Identity);
	for (int32 Step = 0; Step < 50 && Director->GetEnemies().Num() == 0; ++Step)
	{
		Test.Tick(0.1f);
	}
	if (!TestEqual(TEXT("the AT gun is out"), Director->GetEnemies().Num(), 1))
	{
		return false;
	}
	ATankEnemy* Gun = Director->GetEnemies()[0];

	// Dug in behind a wall where it can never see the tank.
	const FVector Hideout(0, 6000, 100);
	Gun->GetDrive()->Teleport(Hideout, FRotator(0.0f, -90.0f, 0.0f));
	Test.SpawnStaticBlock(FVector(0, 5300, 300), FVector(40, 1, 8));
	Test.Tick(0.5f);
	TestFalse(TEXT("blind behind the wall"), Gun->GetWeapon()->HasLineOfSight());
	// Either the crew moves it or the director does; within twice the timeout it has the tank in sight again.
	float Waited = 0.0f;
	while (!Gun->GetWeapon()->HasLineOfSight() && Waited < Director->LostEnemyTimeout * 2.0f)
	{
		Test.Tick(0.5f);
		Waited += 0.5f;
	}
	TestTrue(FString::Printf(TEXT("found a spot that sees the tank after %.0f s (%.0f m from the hideout)"), Waited,
		FVector::Dist2D(Gun->GetActorLocation(), Hideout) / 100.0), Gun->GetWeapon()->HasLineOfSight());
	UGameplayStatics::DeleteGameInSlot(CombatTestSlot, 0);
	return true;
}

#endif
