#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/TankTestUtils.h"
#include "Cockpit/CockpitJoystickComponent.h"
#include "Cockpit/TankSeatComponent.h"
#include "Settings/TankSettingsSubsystem.h"
#include "Tank/TankMovementComponent.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Testing/TankTestWaveSpawner.h"
#include "UI/AimAssistToggleWidget.h"
#include "UI/TankStatusHudWidget.h"
#include "UI/CockpitDisplayWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "UObject/StructOnScope.h"

using namespace TankTests;

namespace TankContentTests
{
	const TCHAR* TestSlot = TEXT("TankAutomationContentSettings");

	UClass* LoadBlueprintClass(const FString& AssetPath)
	{
		return StaticLoadClass(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + FPaths::GetBaseFilename(AssetPath) + TEXT("_C")));
	}

	ATankVehicle* SpawnPlayerTank(FWorld& Test, const FTransform& Transform = FTransform(FVector(0, 0, 90)))
	{
		return Test.World->SpawnActor<ATankVehicle>(LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Tank/BP_PlayerTank")), Transform);
	}

	USceneComponent* FindComponentByName(AActor* Actor, const TCHAR* Name)
	{
		TArray<USceneComponent*> Components;
		Actor->GetComponents(Components);
		for (USceneComponent* Component : Components)
		{
			if (Component->GetName() == Name)
			{
				return Component;
			}
		}
		return nullptr;
	}

	/** Calls a Blueprint function with at most one object input; returns false if any bool output is false. */
	bool CallBlueprintFunction(UObject* Target, FName FunctionName, UObject* ObjectArgument)
	{
		UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			return false;
		}
		FStructOnScope Params(Function);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (FObjectPropertyBase* ObjectParam = CastField<FObjectPropertyBase>(*It); ObjectParam && !It->HasAnyPropertyFlags(CPF_OutParm))
			{
				ObjectParam->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), ObjectArgument);
			}
		}
		Target->ProcessEvent(Function, Params.GetStructMemory());
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (FBoolProperty* BoolParam = CastField<FBoolProperty>(*It); BoolParam && It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)
				&& !BoolParam->GetPropertyValue_InContainer(Params.GetStructMemory()))
			{
				return false;
			}
		}
		return true;
	}
}

using namespace TankContentTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentInputTest, "Tank.Content.InputMappings", TankTestFlags)
bool FTankContentInputTest::RunTest(const FString& Parameters)
{
	const UInputMappingContext* TankContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/TankGame/Input/IMC_Tank.IMC_Tank"));
	const UInputMappingContext* TemplateContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/XRFramework/Input/IMC_Default.IMC_Default"));
	if (!TestNotNull(TEXT("IMC_Tank"), TankContext) || !TestNotNull(TEXT("IMC_Default"), TemplateContext))
	{
		return false;
	}
	TSet<FKey> TankKeys;
	TSet<FKey> ToggleViewKeys;
	for (const FEnhancedActionKeyMapping& Mapping : TankContext->GetMappings())
	{
		if (Mapping.Action && Mapping.Action->GetName() == TEXT("IA_Tank_ToggleView"))
		{
			ToggleViewKeys.Add(Mapping.Key);
			TestEqual(TEXT("view toggle is a button"), static_cast<int32>(Mapping.Action->ValueType), static_cast<int32>(EInputActionValueType::Boolean));
			continue;
		}
		TankKeys.Add(Mapping.Key);
		TestEqual(TEXT("tank actions are 2D axes"), static_cast<int32>(Mapping.Action->ValueType), static_cast<int32>(EInputActionValueType::Axis2D));
	}
	TestTrue(TEXT("view toggle on C and gamepad Y"), ToggleViewKeys.Contains(EKeys::C) && ToggleViewKeys.Contains(EKeys::Gamepad_FaceButton_Top));
	// Every key the template uses for teleport/snap turn must be claimed while seated.
	for (const FEnhancedActionKeyMapping& Mapping : TemplateContext->GetMappings())
	{
		const FString ActionName = Mapping.Action ? Mapping.Action->GetName() : FString();
		if (ActionName == TEXT("IA_Move") || ActionName == TEXT("IA_Turn"))
		{
			TestTrue(FString::Printf(TEXT("%s key %s is overridden by the tank"), *ActionName, *Mapping.Key.ToString()), TankKeys.Contains(Mapping.Key));
		}
		// The toggle must not use a button the template already uses.
		TestFalse(FString::Printf(TEXT("view toggle key %s is free in the template (%s)"), *Mapping.Key.ToString(), *ActionName), ToggleViewKeys.Contains(Mapping.Key));
	}
	TestTrue(TEXT("keyboard drive fallback"), TankKeys.Contains(EKeys::W) && TankKeys.Contains(EKeys::D));
	TestTrue(TEXT("keyboard aim fallback"), TankKeys.Contains(EKeys::Left) && TankKeys.Contains(EKeys::Right));

	// Run a full press through each mapping's saved modifiers.
	const TMap<FString, FVector2D> Expected = {
		{ TEXT("W"), FVector2D(0, 1) }, { TEXT("S"), FVector2D(0, -1) }, { TEXT("A"), FVector2D(-1, 0) }, { TEXT("D"), FVector2D(1, 0) },
		{ TEXT("Up"), FVector2D(0, 1) }, { TEXT("Down"), FVector2D(0, -1) }, { TEXT("Left"), FVector2D(-1, 0) }, { TEXT("Right"), FVector2D(1, 0) },
	};
	for (const FEnhancedActionKeyMapping& Mapping : TankContext->GetMappings())
	{
		if (ToggleViewKeys.Contains(Mapping.Key))
		{
			continue;
		}
		FInputActionValue Value(EInputActionValueType::Axis2D, FVector(1.0, 0.0, 0.0));
		for (UInputModifier* Modifier : Mapping.Modifiers)
		{
			Value = Modifier ? Modifier->ModifyRaw(nullptr, Value, 0.0f) : Value;
		}
		const FVector2D Result = Value.Get<FVector2D>();
		const FString KeyName = Mapping.Key.GetFName().ToString();
		FVector2D Want;
		if (const FVector2D* Keyboard = Expected.Find(KeyName))
		{
			Want = *Keyboard;
		}
		else
		{
			// Analog axes: X -> X, Y -> Y
			Want = (KeyName.EndsWith(TEXT("Y"))) ? FVector2D(0, 1) : FVector2D(1, 0);
		}
		TestTrue(FString::Printf(TEXT("%s -> %s (got %s)"), *KeyName, *Want.ToString(), *Result.ToString()), Result.Equals(Want, 1e-4));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentPlayerTankTest, "Tank.Content.PlayerTankSetup", TankTestFlags)
bool FTankContentPlayerTankTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Tank = SpawnPlayerTank(Test);
	if (!TestNotNull(TEXT("BP_PlayerTank spawns"), Tank))
	{
		return false;
	}
	Test.Tick(0.2f);
	TestNotNull(TEXT("mapping context assigned"), Tank->TankMappingContext.Get());
	TestNotNull(TEXT("drive action assigned"), Tank->DriveAction.Get());
	TestNotNull(TEXT("aim action assigned"), Tank->AimAction.Get());
	TestNotNull(TEXT("view toggle action assigned"), Tank->ToggleViewAction.Get());
	TestTrue(TEXT("display uses the cockpit display widget"), Cast<UCockpitDisplayWidget>(Tank->GetDisplay()->GetWidget()) != nullptr);

	for (const TCHAR* KnobName : { TEXT("DriveKnob"), TEXT("AimKnob") })
	{
		UPrimitiveComponent* Knob = Cast<UPrimitiveComponent>(FindComponentByName(Tank, KnobName));
		if (!TestNotNull(FString::Printf(TEXT("%s exists"), KnobName), Knob))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s is a PhysicsBody for the VR grab trace"), KnobName), static_cast<int32>(Knob->GetCollisionObjectType()), static_cast<int32>(ECC_PhysicsBody));
		TestEqual(FString::Printf(TEXT("%s does not physically collide"), KnobName), static_cast<int32>(Knob->GetCollisionEnabled()), static_cast<int32>(ECollisionEnabled::QueryOnly));
		TestFalse(FString::Printf(TEXT("%s does not simulate"), KnobName), Knob->IsSimulatingPhysics());
	}

	// Placeholder visuals never collide; only the hull box does.
	TArray<UStaticMeshComponent*> Meshes;
	Tank->GetComponents(Meshes);
	int32 Colliding = 0;
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const bool bKnob = Mesh->GetName().EndsWith(TEXT("Knob"));
		Colliding += (!bKnob && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision) ? 1 : 0;
	}
	TestEqual(TEXT("placeholder meshes have no collision"), Colliding, 0);
	TestTrue(TEXT("tank has many placeholder meshes (cockpit, hull, turret)"), Meshes.Num() > 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentGrabStickTest, "Tank.Content.VRGrabCockpitStick", TankTestFlags)
bool FTankContentGrabStickTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	Test.World->SpawnActor<APlayerController>();
	ATankVehicle* Tank = SpawnPlayerTank(Test);
	Test.Tick(0.2f);

	// A motion controller "hand" riding in the tank.
	AActor* HandActor = Test.World->SpawnActor<AActor>();
	UMotionControllerComponent* Hand = NewObject<UMotionControllerComponent>(HandActor, TEXT("MotionControllerLeftGrip"));
	Hand->SetTrackingMotionSource(FName(TEXT("Left")));
	HandActor->SetRootComponent(Hand);
	Hand->RegisterComponent();
	HandActor->AttachToActor(Tank, FAttachmentTransformRules::KeepWorldTransform);

	USceneComponent* Grab = FindComponentByName(Tank, TEXT("DriveGrab"));
	USceneComponent* Knob = FindComponentByName(Tank, TEXT("DriveKnob"));
	UCockpitJoystickComponent* Stick = Tank->GetDriveStick();
	if (!TestNotNull(TEXT("DriveGrab"), Grab) || !TestNotNull(TEXT("DriveKnob"), Knob))
	{
		return false;
	}
	HandActor->SetActorLocation(Knob->GetComponentLocation());

	TestTrue(TEXT("template TryGrab succeeds on the stick knob"), CallBlueprintFunction(Grab, TEXT("TryGrab"), Hand));
	Test.Tick(0.1f);
	TestTrue(TEXT("knob stays on the stick (custom grab does not attach it to the hand)"), Knob->GetAttachParent() == Stick);
	TestTrue(TEXT("stick sees the hand"), Stick->IsHeld());
	TestTrue(TEXT("grabbing does not deflect the stick"), Stick->GetValue().IsNearlyZero(0.02));

	// Push the hand to the right: steering right; the tank turns right.
	const FTransform Base = Stick->GetAttachParent()->GetComponentTransform();
	HandActor->AddActorWorldOffset(Base.TransformVectorNoScale(FVector(0, 12, 0)));
	Test.Tick(0.1f);
	TestTrue(FString::Printf(TEXT("hand right -> stick right (%.2f)"), Stick->GetValue().X), Stick->GetValue().X > 0.5);
	const float YawBefore = Tank->GetActorRotation().Yaw;
	Test.Tick(2.0f);
	TestTrue(TEXT("held cockpit stick steers the tank to the right"), FRotator::NormalizeAxis(Tank->GetActorRotation().Yaw - YawBefore) > 3.0f);

	TestTrue(TEXT("template TryRelease succeeds"), CallBlueprintFunction(Grab, TEXT("TryRelease"), nullptr));
	Test.Tick(0.5f);
	TestFalse(TEXT("released"), Stick->IsHeld());
	TestTrue(TEXT("stick springs back"), Stick->GetValue().IsNearlyZero(0.01));
	TestTrue(TEXT("knob still on the stick after release"), Knob->GetAttachParent() == Stick);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentBoardingTest, "Tank.Content.RiderMovesWithTank", TankTestFlags)
bool FTankContentBoardingTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Tank = SpawnPlayerTank(Test);
	APawn* Pawn = Test.World->SpawnActor<APawn>(LoadBlueprintClass(TEXT("/Game/XRFramework/Blueprints/BP_XRPawn")), FTransform(FVector(-500, 0, 0)));
	if (!TestNotNull(TEXT("BP_XRPawn spawns"), Pawn))
	{
		return false;
	}
	Test.Tick(0.1f);
	UTankSeatComponent* Seat = Tank->FindComponentByClass<UTankSeatComponent>();
	TestTrue(TEXT("pawn boards the seat"), Seat->Board(Pawn));
	Test.Tick(0.1f);
	TestTrue(TEXT("pawn is placed at the seat"), FVector::Dist2D(Pawn->GetActorLocation(), Seat->GetComponentLocation()) < 1.0);

	// No headset: calibration lifts the camera to eye height.
	Test.Tick(Seat->CalibrationDelay + 0.2f);
	const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
	if (TestNotNull(TEXT("pawn camera"), Camera))
	{
		const double EyeHeight = Camera->GetComponentLocation().Z - Seat->GetComponentLocation().Z;
		TestTrue(FString::Printf(TEXT("eyes at the design height above the seat (%.1f cm)"), EyeHeight), FMath::IsNearlyEqual(EyeHeight, Seat->DesignEyeHeight, 1.0));
	}

	Tank->SetFallbackInputs(FVector2D(1.0, 0.5), FVector2D::ZeroVector);
	Test.Tick(3.0f);
	TestTrue(TEXT("rider stays in the seat while the tank drives and turns"), FVector::Dist2D(Pawn->GetActorLocation(), Seat->GetComponentLocation()) < 1.0);
	TestTrue(TEXT("rider's frame turns with the hull (camera itself is never rotated by code)"),
		FMath::IsNearlyEqual(FRotator::NormalizeAxis(Pawn->GetActorRotation().Yaw - Tank->GetActorRotation().Yaw), 0.0f, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentOverviewCameraTest, "Tank.Content.OverviewCamera", TankTestFlags)
bool FTankContentOverviewCameraTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	ATankVehicle* Tank = SpawnPlayerTank(Test);
	APlayerController* PC = Test.World->SpawnActor<APlayerController>();
	APawn* Pawn = Test.World->SpawnActor<APawn>(LoadBlueprintClass(TEXT("/Game/XRFramework/Blueprints/BP_XRPawn")), FTransform(FVector(-500, 0, 0)));
	if (!TestNotNull(TEXT("BP_XRPawn spawns"), Pawn) || !TestNotNull(TEXT("BP_PlayerTank spawns"), Tank))
	{
		return false;
	}
	// Possessing BP_XRPawn in this test world crashes, so just board it.
	Test.Tick(0.1f);
	Tank->FindComponentByClass<UTankSeatComponent>()->Board(Pawn);
	Test.Tick(0.2f);
	TestFalse(TEXT("starts in the cockpit view"), Tank->IsOverviewViewActive());

	// The overview camera sits above and behind the hull, looking down at it.
	const UCameraComponent* Camera = Tank->GetOverviewCamera();
	if (TestNotNull(TEXT("overview camera"), Camera))
	{
		const FVector Local = Tank->GetActorTransform().InverseTransformPosition(Camera->GetComponentLocation());
		TestTrue(FString::Printf(TEXT("camera above the tank (%.0f cm)"), Local.Z), Local.Z > 500.0);
		TestTrue(FString::Printf(TEXT("camera behind the tank (%.0f cm)"), Local.X), Local.X < -500.0);
		const FVector ToTank = (Tank->GetActorLocation() - Camera->GetComponentLocation()).GetSafeNormal();
		TestTrue(TEXT("camera looks at the tank"), FVector::DotProduct(Camera->GetForwardVector(), ToTank) > 0.9);
	}

	Tank->SetOverviewView(true);
	Tank->SetFallbackInputs(FVector2D(0.0, 1.0), FVector2D::ZeroVector);
	Test.Tick(1.0f);
	TestTrue(TEXT("overview active"), Tank->IsOverviewViewActive());
	TestTrue(TEXT("player views through the tank's camera"), PC->PlayerCameraManager && PC->PlayerCameraManager->GetViewTarget() == Tank);
	UTankStatusHudWidget* Hud = Cast<UTankStatusHudWidget>(Tank->GetOverviewHud());
	if (TestNotNull(TEXT("status panel created"), Hud))
	{
		Hud->UpdateStatus();
		const FString Status = Hud->GetStatusString();
		TestTrue(FString::Printf(TEXT("status shows the speed (%s)"), *Status), Status.Contains(TEXT("SPEED:")) && !Status.Contains(TEXT("SPEED: 0 km/h")));
		TestTrue(TEXT("status shows Aim Assist"), Status.Contains(TEXT("AIM ASSIST:")));
	}

	// The camera keeps following the moving tank.
	if (Camera)
	{
		const double Distance = FVector::Dist(Camera->GetComponentLocation(), Tank->GetActorLocation());
		TestTrue(FString::Printf(TEXT("camera follows the tank (%.0f cm away)"), Distance), Distance < 2500.0);
	}

	Tank->ToggleOverviewView();
	Test.Tick(1.0f);
	TestFalse(TEXT("back to the cockpit"), Tank->IsOverviewViewActive());
	TestTrue(TEXT("player views through the rider's camera again"), PC->PlayerCameraManager && PC->PlayerCameraManager->GetViewTarget() == Pawn);
	TestTrue(TEXT("status panel hidden in the cockpit"), !Hud || Hud->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentWavesTest, "Tank.Content.WavesAndLockOn", TankTestFlags)
bool FTankContentWavesTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	if (UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World))
	{
		Settings->SetSaveSlotForTesting(TestSlot);
		Settings->SetAimAssistEnabled(true);
	}
	ATankVehicle* Tank = SpawnPlayerTank(Test);
	ATankTestWaveSpawner* Spawner = Test.World->SpawnActorDeferred<ATankTestWaveSpawner>(
		LoadBlueprintClass(TEXT("/Game/TankGame/Blueprints/Testing/BP_TankTestWaveSpawner")), FTransform(FVector(0, 0, 0)));
	Spawner->FirstWaveDelay = 0.2f;
	Spawner->TimeBetweenWaves = 0.5f;
	Spawner->EnemyLifetime = FVector2D(2.0, 2.0);
	Spawner->EnemiesPerWave = 3;
	Spawner->FinishSpawning(FTransform(FVector(0, 0, 0)));

	Test.Tick(1.0f);
	TestEqual(TEXT("wave 1 spawned"), Spawner->GetCurrentWave(), 1);
	TestEqual(TEXT("wave enemies are registered targets"), UTargetRegistrySubsystem::Get(Test.World)->GetValidTargetCount(), 3);
	UTargetableComponent* FirstLock = Tank->GetTargeting()->GetCurrentTarget();
	TestNotNull(TEXT("tank locks onto wave 1"), FirstLock);

	Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D(1.0, 0.0));
	Test.Tick(0.1f);
	Tank->SetFallbackInputs(FVector2D::ZeroVector, FVector2D::ZeroVector);
	Test.Tick(0.3f);

	// Wave 1 expires (simulated kills); wave 2 arrives and is locked again.
	Test.Tick(2.5f);
	TestEqual(TEXT("wave 2 spawned"), Spawner->GetCurrentWave(), 2);
	UTargetableComponent* SecondLock = Tank->GetTargeting()->GetCurrentTarget();
	TestNotNull(TEXT("tank locks onto wave 2"), SecondLock);
	TestTrue(TEXT("lock belongs to the new wave"), SecondLock && SecondLock->GetTargetDisplayName().ToString().StartsWith(TEXT("W2")));

	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTankContentMenuTest, "Tank.Content.MenuAimAssistToggle", TankTestFlags)
bool FTankContentMenuTest::RunTest(const FString& Parameters)
{
	FWorld Test;
	UTankSettingsSubsystem* Settings = UTankSettingsSubsystem::Get(Test.World);
	if (!TestNotNull(TEXT("settings"), Settings))
	{
		return false;
	}
	Settings->SetSaveSlotForTesting(TestSlot);
	Settings->SetAimAssistEnabled(true);

	UClass* MenuClass = LoadBlueprintClass(TEXT("/Game/XRFramework/Blueprints/WBP_Menu"));
	UUserWidget* Menu = MenuClass ? CreateWidget<UUserWidget>(Test.World, MenuClass) : nullptr;
	if (!TestNotNull(TEXT("existing VR menu widget"), Menu))
	{
		return false;
	}
	UAimAssistToggleWidget* Toggle = nullptr;
	Menu->WidgetTree->ForEachWidget([&Toggle](UWidget* Widget)
	{
		Toggle = Toggle ? Toggle : Cast<UAimAssistToggleWidget>(Widget);
	});
	if (!TestNotNull(TEXT("menu contains the Aim Assist option"), Toggle))
	{
		return false;
	}
	TestEqual(TEXT("label shows On"), Toggle->GetLabel().ToString(), FString(TEXT("Aim Assist: On")));
	Toggle->Toggle();
	TestFalse(TEXT("menu option switches the setting off"), Settings->IsAimAssistEnabled());
	TestEqual(TEXT("label shows Off"), Toggle->GetLabel().ToString(), FString(TEXT("Aim Assist: Off")));
	Toggle->Toggle();
	TestTrue(TEXT("and back on"), Settings->IsAimAssistEnabled());
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	return true;
}

#endif
