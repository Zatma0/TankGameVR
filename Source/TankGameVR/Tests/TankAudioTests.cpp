#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Audio/TankAudio.h"
#include "Audio/TankAudioComponent.h"
#include "Cockpit/CockpitSmokeLauncherComponent.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TankTargetingComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "Weapons/TankCannonComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"

using namespace TankTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankAudioSyncTest, "Tank.Audio.SoundsLandOnTheirEvents", TankTestFlags)
bool FTankAudioSyncTest::RunTest(const FString& Parameters)
{
	const TCHAR* Slot = TEXT("TankAutomationAudioSettings");
	FWorld Test;
	UTankAudioSubsystem* Audio = UTankAudioSubsystem::Get(Test.World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio))
	{
		return false;
	}
	if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World))
	{
		Settings->SetSaveSlotForTesting(Slot);
		Settings->SetAimAssistEnabled(true);
	}
	ON_SCOPE_EXIT { UGameplayStatics::DeleteGameInSlot(Slot, 0); };

	ATankVehicle* Player = Test.World->SpawnActor<ATankVehicle>(LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")), FTransform(FVector(0, 0, 90)));
	Test.SpawnStaticBlock(FVector(2500, 0, 300), FVector(1, 20, 6));
	Player->GetDamageReaction()->HeavyHitFireChance = 0.0f;
	Player->GetDamageReaction()->LightHitFireChance = 0.0f;
	Test.Tick(0.2f);

	const float Dt = 1.0f / 90.0f;
	auto Now = [&Test]() { return static_cast<float>(Test.World->GetTimeSeconds()); };
	// A sound belongs to an event if it started on that frame (or the one the state change is noticed on).
	auto At = [&](const TCHAR* Sound, float Expected, float Frames = 1.0f)
	{
		const float Played = Audio->GetLastPlayed(Sound);
		const bool bOk = Played >= 0.0f && FMath::Abs(Played - Expected) <= Frames * Dt + 0.002f;
		TestTrue(FString::Printf(TEXT("%s at %.3f s (expected %.3f s, %.0f ms off)"), Sound, Played, Expected, (Played - Expected) * 1000.0f), bOk);
	};

	TestTrue(TEXT("the engine runs from the start"), Audio->GetLastPlayed(TEXT("SW_Engine_Loop")) >= 0.0f);

	// Loading: scrape at once, the round seats when it slides home, clunk and ready tone when the wedge is shut.
	UTankBreechComponent* Breech = Player->GetBreech();
	const float LoadStart = Now();
	TestTrue(TEXT("auto load"), Breech->AutoLoadType(ETankRoundType::AP));
	At(TEXT("SW_Shell_Ram"), LoadStart, 0.0f);
	Test.Tick(Breech->AutoLoadTime + Breech->CloseTime + 0.2f);
	At(TEXT("SW_Shell_Seat"), LoadStart + Breech->AutoLoadTime, 2.0f);
	At(TEXT("SW_Breech_Close"), LoadStart + Breech->AutoLoadTime + Breech->CloseTime, 3.0f);
	At(TEXT("SW_Gun_Ready"), LoadStart + Breech->AutoLoadTime + Breech->CloseTime, 3.0f);

	// The shot: muzzle blast and the thump inside on the frame of the trigger, the impact on the frame the round lands.
	const float Shot = Now();
	TestTrue(TEXT("fires"), Player->Fire());
	At(TEXT("SW_Cannon_Fire"), Shot, 0.0f);
	At(TEXT("SW_Cannon_Interior"), Shot, 0.0f);
	TestTrue(TEXT("our own shot is not turned down"), Audio->GetLastVolume(TEXT("SW_Cannon_Interior")) > 0.74f);
	Test.Tick(1.6f);
	const float Landed = Now() - Player->GetCannon()->GetTimeSinceShotReport();
	TestTrue(FString::Printf(TEXT("the round took %.0f ms to the wall"), (Landed - Shot) * 1000.0f), Landed > Shot + 0.05f && Landed < Shot + 0.5f);
	At(TEXT("SW_Impact_Dirt"), Landed, 0.0f);
	TestEqual(TEXT("AP into a wall is not a steel-on-steel hit"), Audio->CountPlayed(TEXT("SW_Impact_AP")), 0);
	// The breech opens when the recoil is over, the stub lands a moment after that.
	At(TEXT("SW_Breech_Open"), Shot + Breech->RecoilKickTime + Breech->RecoilReturnTime, 2.0f);
	const float Stub = Audio->GetLastPlayed(TEXT("SW_Casing_Drop"));
	TestTrue(FString::Printf(TEXT("the case stub is heard when it lands (%.0f ms after the breech opened)"), (Stub - Audio->GetLastPlayed(TEXT("SW_Breech_Open"))) * 1000.0f),
		Stub > Audio->GetLastPlayed(TEXT("SW_Breech_Open")) + 0.1f);

	// Empty gun.
	const float Dry = Now();
	TestFalse(TEXT("nothing loaded"), Player->Fire());
	At(TEXT("SW_Dry_Fire"), Dry, 0.0f);

	// Hits on our hull: a heavy round and an autocannon round sound different.
	UGameplayStatics::ApplyDamage(Player, 30.0f, nullptr, nullptr, UTankDamageType_AP::StaticClass());
	At(TEXT("SW_Hull_Hit"), Now(), 0.0f);
	UGameplayStatics::ApplyDamage(Player, 4.0f, nullptr, nullptr, UTankDamageType_Autocannon::StaticClass());
	At(TEXT("SW_Hull_Hit_Light"), Now(), 0.0f);
	TestEqual(TEXT("one heavy bang, not two"), Audio->CountPlayed(TEXT("SW_Hull_Hit")), 1);
	TestTrue(FString::Printf(TEXT("a hit on its own gets its full level (%.2f)"), Audio->GetLastVolume(TEXT("SW_Hull_Hit"))),
		FMath::IsNearlyEqual(Audio->GetLastVolume(TEXT("SW_Hull_Hit")), 0.85f, 0.01f));

	// Smoke: the launchers at once, the burst when the grenades get there.
	const float Smoke = Now();
	TestTrue(TEXT("smoke launched"), Player->GetSmokeLauncher()->Launch());
	At(TEXT("SW_Smoke_Salvo"), Smoke, 0.0f);
	const float Flight = ATankSmokeCloud::StaticClass()->GetDefaultObject<ATankSmokeCloud>()->FlightTime;
	Test.Tick(Flight + 0.3f);
	At(TEXT("SW_Smoke_Burst"), Smoke + Flight, 2.0f);

	// Fire control: a beep when a target is locked, another when the gun is on it.
	TestEqual(TEXT("no lock beep without a target"), Audio->CountPlayed(TEXT("SW_Lock_Beep")), 0);
	Test.SpawnTarget(FVector(0, 0, 200) + FRotator(0.0f, 40.0f, 0.0f).Vector() * 3000.0, TEXT("Target"));
	Test.Tick(4.0f);
	const float Lock = Audio->GetLastPlayed(TEXT("SW_Lock_Beep"));
	const float Laid = Audio->GetLastPlayed(TEXT("SW_On_Target"));
	TestTrue(TEXT("lock beep"), Lock >= 0.0f);
	TestTrue(FString::Printf(TEXT("on-target tone after the turret has swung over (%.2f s after the lock)"), Laid - Lock), Laid > Lock + 0.5f);
	TestTrue(TEXT("the gun really is on target then"), Player->GetTargeting()->IsOnTarget());

	// Driving: tracks come in and the engine works harder.
	TestTrue(TEXT("no track noise standing still"), Audio->GetLastPlayed(TEXT("SW_Tracks_Loop")) < 0.0f);
	const float Idle = Player->GetSounds()->GetEngineRev();
	Player->SetFallbackInputs(FVector2D(0.0, 1.0), FVector2D::ZeroVector);
	Test.Tick(1.5f);
	TestTrue(TEXT("tracks heard when moving"), Audio->GetLastPlayed(TEXT("SW_Tracks_Loop")) >= 0.0f);
	TestTrue(FString::Printf(TEXT("engine revs up with the stick (%.2f -> %.2f)"), Idle, Player->GetSounds()->GetEngineRev()), Player->GetSounds()->GetEngineRev() > Idle + 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankAudioHeadroomTest, "Tank.Audio.BangsShareTheHeadroom", TankTestFlags)
bool FTankAudioHeadroomTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	UTankAudioSubsystem* Audio = UTankAudioSubsystem::Get(Test.World);
	if (!TestNotNull(TEXT("audio subsystem"), Audio))
	{
		return false;
	}
	auto Bang = [&](const TCHAR* Name)
	{
		UTankAudioSubsystem::PlayInside(Test.World, TankAudio::Sound(Name).LoadSynchronous(), 1.0f);
		return Audio->GetLastVolume(Name);
	};
	const float First = Bang(TEXT("SW_Cannon_Interior"));
	const float Second = Bang(TEXT("SW_Hull_Hit"));
	const float Third = Bang(TEXT("SW_Explosion_HE"));
	const float Fourth = Bang(TEXT("SW_Enemy_Cannon"));
	TestEqual(TEXT("first bang at full level"), First, 1.0f);
	TestTrue(FString::Printf(TEXT("second fills the room (%.2f)"), Second), Second > 0.75f && Second <= 1.0f);
	TestTrue(FString::Printf(TEXT("third and fourth are turned down (%.2f, %.2f)"), Third, Fourth), Third < 0.5f && Fourth < 0.5f);
	TestTrue(TEXT("but still heard"), Fourth >= Audio->LoudFloor - 0.001f);
	UTankAudioSubsystem::PlayInside(Test.World, TankAudio::Sound(TEXT("SW_Switch_Click")).LoadSynchronous(), 0.6f);
	TestTrue(TEXT("small sounds are left alone"), FMath::IsNearlyEqual(Audio->GetLastVolume(TEXT("SW_Switch_Click")), 0.6f));

	// After a second the room is back.
	Test.Tick(1.0f);
	const float Later = Bang(TEXT("SW_ATGun_Fire"));
	TestTrue(FString::Printf(TEXT("a bang a second later is at full level again (%.2f)"), Later), Later > 0.99f);
	return true;
}

#endif
