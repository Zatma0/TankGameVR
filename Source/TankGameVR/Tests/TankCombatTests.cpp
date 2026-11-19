#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Cockpit/CockpitExtinguisherComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Cockpit/CockpitSmokeLauncherComponent.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/EnemyWeaponComponent.h"
#include "Enemies/TankEnemy.h"
#include "Enemies/TankEnemyTypes.h"
#include "Game/TankArenaDirector.h"
#include "Game/TankEnemySpawnPoint.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankVehicle.h"
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
	const TCHAR* TestSlot = TEXT("TankAutomationCombatSettings");

	UClass* LoadBlueprintClass(const FString& AssetPath)
	{
		return StaticLoadClass(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + FPaths::GetBaseFilename(AssetPath) + TEXT("_C")));
	}

	ATankVehicle* SpawnPlayer(FWorld& Test, const FVector& Location = FVector(0, 0, 90))
	{
		if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World))
		{
			Settings->SetSaveSlotForTesting(TestSlot);
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
	const UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/TankGame/Ugurkan/BPs/EnemyInfos/DT_EnemyFeatures.DT_EnemyFeatures"));
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	TestTrue(TEXT("used some charge"), Bottle->GetCharge() < 1.0f);
	// The other hand still fires.
	Player->HandleTrigger(true, true);
	TestEqual(TEXT("left trigger fires"), Player->GetCannon()->GetShotsFired(), Shots + 1);
	Bottle->ReleaseHand();

	// Hands-free: G puts out the next fire.
	Fire->Ignite(0.5f);
	TestTrue(TEXT("auto spray"), Bottle->AutoSpray());
	Test.Tick(3.0f);
	TestFalse(TEXT("auto spray puts it out"), Fire->IsBurning());
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
		break;
	}
	TestEqual(TEXT("cloud spawned"), Clouds, 1);
	TestTrue(TEXT("keyboard salvo"), Smoke->Launch());
	TestFalse(TEXT("empty after two"), Smoke->Launch());
	TestTrue(TEXT("display shows reloading"), Smoke->GetStatusText().Contains(TEXT("RELOADING")));
	Test.Tick(Smoke->ReloadTime + 0.5f);
	TestTrue(TEXT("reloads"), Smoke->GetCharges() >= 1);
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
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
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	return true;
}

#endif
