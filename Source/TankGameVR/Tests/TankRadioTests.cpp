#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Audio/TankAudio.h"
#include "CableComponent.h"
#include "Cockpit/CockpitCableJoltComponent.h"
#include "Cockpit/CockpitRadioComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/TankEnemy.h"
#include "Tank/TankVehicle.h"
#include "Tank/TankVisionConeComponent.h"
#include "Weapons/TankBreechComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

using namespace TankTests;

namespace TankRadioTests
{
	struct FRadioTest
	{
		FWorld Test;
		ATankVehicle* Tank = nullptr;
		UCockpitRadioComponent* Radio = nullptr;
		USceneComponent* Head = nullptr;
		USceneComponent* Hand = nullptr;

		FRadioTest()
		{
			Tank = Test.World->SpawnActor<ATankVehicle>(ATankVehicle::StaticClass(), FTransform(FVector(0, 0, 90)));
			Test.Tick(0.2f);
			Radio = Tank->GetRadio();
			// Looking straight ahead over the hull.
			Tank->GetVisionCone()->SetViewOverride(true, Tank->GetActorLocation() + FVector(0, 0, 200), FVector::ForwardVector);
			Head = MakeRider(TEXT("Head"), FVector(130, -70, 130));
			Radio->SetHeadOverride(Head);
		}

		USceneComponent* MakeRider(const TCHAR* Name, const FVector& TankSpace)
		{
			AActor* Holder = Test.World->SpawnActor<AActor>();
			USceneComponent* Scene = NewObject<USceneComponent>(Holder, Name);
			Holder->SetRootComponent(Scene);
			Scene->RegisterComponent();
			Scene->SetWorldLocation(Tank->GetActorTransform().TransformPosition(TankSpace));
			Holder->AttachToActor(Tank, FAttachmentTransformRules::KeepWorldTransform);
			return Scene;
		}

		/** An enemy that just stands there. */
		ATankEnemy* Enemy(const TCHAR* Blueprint, const FVector& Location)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UClass* Class = LoadBlueprintClass(FString(TEXT("/Game/TankGame/Blueprints/Enemies/")) + Blueprint);
			ATankEnemy* Spawned = Test.World->SpawnActor<ATankEnemy>(Class, FTransform(Location), Params);
			Spawned->SetActorTickEnabled(false);
			for (UActorComponent* Component : Spawned->GetComponents())
			{
				Component->SetComponentTickEnabled(false);
			}
			return Spawned;
		}

		void Grab()
		{
			Hand = MakeRider(TEXT("HandRight"), Tank->GetActorTransform().InverseTransformPosition(Radio->GetComponentLocation()));
			Radio->SetHeldByHand(Hand);
			Test.Tick(0.05f);
		}

		void ToEar()
		{
			const FVector Ear = Head->GetComponentTransform().TransformPosition(Radio->EarOffset);
			Hand->AddWorldOffset(Ear - Radio->GetComponentLocation());
			Test.Tick(1.0f / 90.0f);
		}
	};
}

using namespace TankRadioTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankRadioReportTest, "Tank.Cockpit.RadioReportsUnseenEnemies", TankTestFlags)
bool FTankRadioReportTest::RunTest(const FString& Parameters)
{
	FRadioTest R;
	UTankAudioSubsystem* Audio = UTankAudioSubsystem::Get(R.Test.World);
	TestFalse(TEXT("quiet with nobody around"), R.Radio->IsCalling());

	// A tank off to the left, out of view: the set rings.
	R.Enemy(TEXT("BP_Enemy_Tank"), FVector(0, -3000, 100));
	R.Test.Tick(1.0f);
	TestTrue(TEXT("rings for an unseen enemy"), R.Radio->IsCalling());
	TestTrue(TEXT("ring heard"), Audio->GetLastPlayed(TEXT("SW_Radio_Call_Loop")) >= 0.0f);

	// Lift the handset to the ear: static, then the voice.
	R.Grab();
	TestTrue(TEXT("handset in the hand"), R.Radio->IsHeld());
	R.ToEar();
	const float Lifted = R.Test.World->GetTimeSeconds();
	TestTrue(TEXT("at the ear"), R.Radio->IsAtEar());
	TestEqual(TEXT("cord long enough to reach the ear"), R.Radio->GetSlipCount(), 0);
	TestFalse(TEXT("ringing stops when answered"), R.Radio->IsCalling());
	TestEqual(TEXT("subtitle"), R.Radio->GetTranscript(), FString(TEXT("RADIO: CONTACT LEFT - ENEMY TANKS")));
	TestTrue(TEXT("static on pickup"), FMath::IsNearlyEqual(Audio->GetLastPlayed(TEXT("SW_Radio_Pickup")), Lifted, 0.02f));
	R.Test.Tick(0.4f);
	const float Spoke = Audio->GetLastPlayed(TEXT("SW_Radio_Left_Tanks"));
	TestTrue(FString::Printf(TEXT("voice after the static (%.0f ms)"), (Spoke - Lifted) * 1000.0f), FMath::Abs(Spoke - Lifted - R.Radio->VoiceDelay) < 0.03f);

	// Hang up; the same tank does not make it ring again.
	R.Radio->ReleaseHand();
	R.Test.Tick(R.Radio->TimeBetweenCalls + 1.0f);
	TestFalse(TEXT("no second call for a known enemy"), R.Radio->IsCalling());
	TestTrue(TEXT("handset back on the hook"), R.Radio->IsAtHome());

	// Two cars come up behind; one in front is in plain view and does not count.
	R.Enemy(TEXT("BP_Enemy_ArmoredCar"), FVector(-3000, 300, 100));
	R.Enemy(TEXT("BP_Enemy_ArmoredCar"), FVector(-3200, -300, 100));
	ATankEnemy* Seen = R.Enemy(TEXT("BP_Enemy_ArmoredCar"), FVector(3000, 0, 100));
	R.Test.Tick(1.0f);
	TestTrue(TEXT("rings for the newcomers"), R.Radio->IsCalling());
	R.Radio->Answer();
	TestEqual(TEXT("busiest side wins"), R.Radio->GetTranscript(), FString(TEXT("RADIO: CONTACT REAR - ARMORED CARS")));
	R.Test.Tick(0.4f);
	TestTrue(TEXT("rear message played"), Audio->GetLastPlayed(TEXT("SW_Radio_Rear_Cars")) >= 0.0f);

	// Everything hidden dead: all clear (the car in view does not count).
	R.Test.Tick(5.0f);
	TArray<ATankEnemy*> Hidden;
	for (TActorIterator<ATankEnemy> It(R.Test.World); It; ++It)
	{
		if (*It != Seen)
		{
			Hidden.Add(*It);
		}
	}
	for (ATankEnemy* Enemy : Hidden)
	{
		UGameplayStatics::ApplyDamage(Enemy, 100000.0f, nullptr, nullptr, UDamageType::StaticClass());
	}
	R.Radio->Answer();
	TestEqual(TEXT("all clear"), R.Radio->GetTranscript(), FString(TEXT("RADIO: NO CONTACTS AROUND YOU")));
	R.Test.Tick(10.0f);
	TestTrue(TEXT("subtitle goes away"), R.Radio->GetTranscript().IsEmpty());

	// An unanswered call gives up.
	R.Enemy(TEXT("BP_Enemy_AntiTank"), FVector(0, 3500, 100));
	R.Test.Tick(R.Radio->TimeBetweenCalls + 1.0f);
	TestTrue(TEXT("rings for the AT gun"), R.Radio->IsCalling());
	float Rang = 0.0f;
	while (R.Radio->IsCalling() && Rang < 60.0f)
	{
		R.Test.Tick(0.1f);
		Rang += 0.1f;
	}
	TestTrue(FString::Printf(TEXT("stops ringing after a while (%.1f s)"), Rang), Rang <= R.Radio->CallTimeout + 0.2f);
	R.Test.Tick(R.Radio->TimeBetweenCalls + 1.0f);
	TestTrue(TEXT("and tries again later, the AT gun is still out there"), R.Radio->IsCalling());
	FString What;
	int32 Count = 0;
	TestEqual(TEXT("AT gun to the right"), R.Radio->GetHiddenSector(What, Count), ETankRadioSector::Right);
	TestEqual(TEXT("as an AT gun"), What, FString(TEXT("ATGun")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankRadioCordTest, "Tank.Cockpit.RadioCordFollowsAndSlips", TankTestFlags)
bool FTankRadioCordTest::RunTest(const FString& Parameters)
{
	FRadioTest R;
	UCableComponent* Cable = R.Tank->GetRadioCable();
	if (!TestNotNull(TEXT("cord"), Cable) || !TestTrue(TEXT("radio uses the cord"), R.Radio->GetCable() == Cable))
	{
		return false;
	}
	auto CordEnd = [Cable]()
	{
		TArray<FVector> Points;
		Cable->GetCableParticleLocations(Points);
		return Points.Num() ? Points.Last() : FVector::ZeroVector;
	};
	auto HandsetBottom = [&R]() { return R.Radio->GetComponentTransform().TransformPosition(FVector(0, 0, -9)); };

	R.Grab();
	R.Hand->AddWorldOffset(FVector(-30, -20, 25));
	R.Test.Tick(0.3f);
	TestTrue(FString::Printf(TEXT("cord end on the handset (%.1f cm off)"), FVector::Dist(CordEnd(), HandsetBottom())), FVector::Dist(CordEnd(), HandsetBottom()) < 2.0f);
	TestTrue(TEXT("still held within reach"), R.Radio->IsHeld());

	// Too far: it slips out and goes back on the hook.
	R.Hand->AddWorldOffset(FVector(-120, -60, 0));
	R.Test.Tick(0.05f);
	TestEqual(TEXT("slipped once"), R.Radio->GetSlipCount(), 1);
	TestFalse(TEXT("not in the hand any more"), R.Radio->IsHeld());
	R.Test.Tick(2.0f);
	TestTrue(TEXT("back on the hook"), R.Radio->IsAtHome());
	TestFalse(TEXT("the same grip does not snatch it back"), R.Radio->IsHeld());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankCableSwingTest, "Tank.Cockpit.CablesSwingAndJolt", TankTestFlags)
bool FTankCableSwingTest::RunTest(const FString& Parameters)
{
	FRadioTest R;
	UCableComponent* Cable = R.Tank->GetRadioCable();
	UCockpitCableJoltComponent* Jolt = R.Tank->FindComponentByClass<UCockpitCableJoltComponent>();
	if (!TestNotNull(TEXT("jolt component"), Jolt))
	{
		return false;
	}
	auto Middle = [&]()
	{
		TArray<FVector> Points;
		Cable->GetCableParticleLocations(Points);
		return R.Tank->GetActorTransform().InverseTransformPosition(Points[Points.Num() / 2]);
	};
	R.Test.Tick(2.0f);
	const FVector Rest = Middle();

	// The cord swings back when the tank pulls away.
	R.Tank->SetFallbackInputs(FVector2D(0.0, 1.0), FVector2D::ZeroVector);
	R.Test.Tick(0.6f);
	const float Swing = FVector::Dist(Middle(), Rest);
	TestTrue(FString::Printf(TEXT("swings when the tank moves off (%.1f cm)"), Swing), Swing > 1.0f);
	R.Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	R.Test.Tick(4.0f);

	// A shot kicks it, then the kick is gone.
	TestTrue(TEXT("loaded"), R.Tank->GetBreech()->AutoLoadType(ETankRoundType::AP));
	R.Test.Tick(2.0f);
	const FVector Before = Middle();
	TestTrue(TEXT("fires"), R.Tank->Fire());
	TestEqual(TEXT("one jolt"), Jolt->GetJoltCount(), 1);
	TestTrue(TEXT("cord pushed"), !Cable->CableForce.IsNearlyZero());
	R.Test.Tick(0.1f);
	const float Kick = FVector::Dist(Middle(), Before);
	TestTrue(FString::Printf(TEXT("cord moves from the shot (%.1f cm)"), Kick), Kick > 1.0f);
	R.Test.Tick(0.3f);
	TestTrue(TEXT("push gone again"), Cable->CableForce.IsNearlyZero());

	// A hit kicks it too; fire damage does not.
	UGameplayStatics::ApplyDamage(R.Tank, 20.0f, nullptr, nullptr, UDamageType::StaticClass());
	TestEqual(TEXT("hit jolt"), Jolt->GetJoltCount(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankCockpitCablesInPlaceTest, "Tank.Content.CockpitCablesHangInPlace", TankTestFlags)
bool FTankCockpitCablesInPlaceTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Tank = Test.World->SpawnActor<ATankVehicle>(LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")), FTransform(FVector(0, 0, 90)));
	Test.Tick(2.0f);
	TArray<UCableComponent*> Cables;
	Tank->GetComponents(Cables);
	TestTrue(FString::Printf(TEXT("radio cord and the hanging cables (%d)"), Cables.Num()), Cables.Num() >= 4);

	const USceneComponent* Cockpit = Tank->GetRadio()->GetAttachParent();
	auto InCockpit = [Cockpit](const FVector& World) { return Cockpit->GetComponentTransform().InverseTransformPosition(World); };
	TMap<UCableComponent*, TArray<FVector>> Rest;
	for (UCableComponent* Cable : Cables)
	{
		TArray<FVector> Points;
		Cable->GetCableParticleLocations(Points);
		TArray<FVector>& Local = Rest.Add(Cable);
		for (const FVector& Point : Points)
		{
			Local.Add(InCockpit(Point));
		}
		const FVector Start = InCockpit(Points[0]);
		const FVector End = InCockpit(Points.Last());
		const FVector Wanted = Cable == Tank->GetRadioCable()
			? InCockpit(Tank->GetRadio()->GetComponentTransform().TransformPosition(Cable->EndLocation))
			: Cable->EndLocation;
		TestTrue(FString::Printf(TEXT("%s ends where it is set (%.1f cm off)"), *Cable->GetName(), FVector::Dist(End, Wanted)), FVector::Dist(End, Wanted) < 2.0f);
		float Lowest = FMath::Min(Start.Z, End.Z);
		float Sag = 0.0f;
		for (const FVector& Point : Points)
		{
			Sag = FMath::Max(Sag, Lowest - static_cast<float>(InCockpit(Point).Z));
		}
		// The radio cord is a coiled cord and hangs lower on purpose.
		const float MaxSag = Cable == Tank->GetRadioCable() ? 35.0f : 20.0f;
		TestTrue(FString::Printf(TEXT("%s sags %.0f cm"), *Cable->GetName(), Sag), Sag < MaxSag);
	}

	// Full speed into a tight turn: they sway but stay by their wall.
	Tank->SetFallbackInputs(FVector2D(1.0, 1.0), FVector2D::ZeroVector);
	float Worst = 0.0f;
	FString WorstName;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		Test.Tick(0.1f);
		for (UCableComponent* Cable : Cables)
		{
			TArray<FVector> Points;
			Cable->GetCableParticleLocations(Points);
			const TArray<FVector>& Before = Rest[Cable];
			for (int32 Index = 0; Index < Points.Num() && Index < Before.Num(); ++Index)
			{
				const float Move = FVector::Dist(InCockpit(Points[Index]), Before[Index]);
				if (Move > Worst)
				{
					Worst = Move;
					WorstName = Cable->GetName();
				}
			}
		}
	}
	TestTrue(FString::Printf(TEXT("largest sway %.0f cm (%s)"), Worst, *WorstName), Worst > 0.5f && Worst < 25.0f);
	return true;
}

#endif
