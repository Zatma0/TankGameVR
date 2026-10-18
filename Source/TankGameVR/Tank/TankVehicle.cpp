#include "Tank/TankVehicle.h"
#include "Tank/TankMovementComponent.h"
#include "Cockpit/CockpitJoystickComponent.h"
#include "Cockpit/TankSeatComponent.h"
#include "Targeting/TankTargetingComponent.h"
#include "UI/CockpitDisplayWidget.h"
#include "UI/TankStatusHudWidget.h"
#include "TankGameVR.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "InputMappingContext.h"

ATankVehicle::ATankVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Root sits 90 cm above the ground.
	HullCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HullCollision"));
	HullCollision->SetBoxExtent(FVector(300.0, 160.0, 50.0));
	HullCollision->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	HullCollision->SetMobility(EComponentMobility::Movable);
	SetRootComponent(HullCollision);

	TankMovement = CreateDefaultSubobject<UTankMovementComponent>(TEXT("TankMovement"));
	Targeting = CreateDefaultSubobject<UTankTargetingComponent>(TEXT("Targeting"));

	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(HullCollision);
	TurretPivot->SetRelativeLocation(FVector(-60.0, 0.0, 80.0));

	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(TurretPivot);
	BarrelPivot->SetRelativeLocation(FVector(110.0, 0.0, 25.0));

	AimOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("AimOrigin"));
	AimOrigin->SetupAttachment(TurretPivot);
	AimOrigin->SetRelativeLocation(FVector(110.0, 0.0, 25.0));

	// What the cockpit display looks through (fixed to the hull, not the turret).
	SightFrame = CreateDefaultSubobject<USceneComponent>(TEXT("SightFrame"));
	SightFrame->SetupAttachment(HullCollision);
	SightFrame->SetRelativeLocation(FVector(50.0, 0.0, 105.0));

	// Driver position front-left. CockpitRoot = seat floor.
	CockpitRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CockpitRoot"));
	CockpitRoot->SetupAttachment(HullCollision);
	CockpitRoot->SetRelativeLocation(FVector(150.0, -70.0, 10.0));

	Seat = CreateDefaultSubobject<UTankSeatComponent>(TEXT("Seat"));
	Seat->SetupAttachment(CockpitRoot);

	DriveStick = CreateDefaultSubobject<UCockpitJoystickComponent>(TEXT("DriveStick"));
	DriveStick->SetupAttachment(CockpitRoot);
	DriveStick->SetRelativeLocation(FVector(38.0, -26.0, 68.0));

	AimStick = CreateDefaultSubobject<UCockpitJoystickComponent>(TEXT("AimStick"));
	AimStick->SetupAttachment(CockpitRoot);
	AimStick->SetRelativeLocation(FVector(38.0, 26.0, 68.0));

	// ~36 x 27 cm screen facing the seat.
	Display = CreateDefaultSubobject<UWidgetComponent>(TEXT("Display"));
	Display->SetupAttachment(CockpitRoot);
	Display->SetRelativeLocation(FVector(66.0, 0.0, 100.0));
	Display->SetRelativeRotation(FRotator(20.0, 180.0, 0.0));
	Display->SetRelativeScale3D(FVector(0.035));
	Display->SetWidgetSpace(EWidgetSpace::World);
	Display->SetDrawSize(FVector2D(1024.0, 768.0));
	Display->SetWidgetClass(UCockpitDisplayWidget::StaticClass());
	Display->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Display->SetTwoSided(false);
	Display->SetTickWhenOffscreen(true);

	// Overview camera behind and above the hull.
	OverviewArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("OverviewArm"));
	OverviewArm->SetupAttachment(HullCollision);
	OverviewArm->SetRelativeLocation(FVector(0.0, 0.0, 150.0));
	OverviewArm->SetRelativeRotation(FRotator(-40.0, 0.0, 0.0));
	OverviewArm->TargetArmLength = 1500.0f;
	OverviewArm->bDoCollisionTest = false;
	OverviewArm->bInheritPitch = false;
	OverviewArm->bInheritRoll = false;
	OverviewArm->bEnableCameraLag = true;
	OverviewArm->CameraLagSpeed = 6.0f;
	OverviewArm->bEnableCameraRotationLag = true;
	OverviewArm->CameraRotationLagSpeed = 4.0f;

	OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
	OverviewCamera->SetupAttachment(OverviewArm, USpringArmComponent::SocketName);
	OverviewCamera->SetFieldOfView(80.0f);

	OverviewHudClass = UTankStatusHudWidget::StaticClass();
}

void ATankVehicle::BeginPlay()
{
	Super::BeginPlay();

	// Tick order: sticks -> tank -> movement -> targeting
	AddTickPrerequisiteComponent(DriveStick);
	AddTickPrerequisiteComponent(AimStick);
	TankMovement->AddTickPrerequisiteActor(this);
	Targeting->AddTickPrerequisiteComponent(TankMovement);

	Seat->OnBoarded.AddDynamic(this, &ATankVehicle::HandleBoarded);

	Display->InitWidget();
	if (UCockpitDisplayWidget* DisplayWidget = Cast<UCockpitDisplayWidget>(Display->GetWidget()))
	{
		DisplayWidget->SetTargeting(Targeting);
	}
}

void ATankVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OverviewHud)
	{
		OverviewHud->RemoveFromParent();
		OverviewHud = nullptr;
	}
	if (APlayerController* PC = InputController.Get())
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (TankMappingContext)
				{
					Subsystem->RemoveMappingContext(TankMappingContext);
				}
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ATankVehicle::HandleBoarded(APawn* Pawn)
{
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}
	InputController = PC;
	EnableInput(PC);

	if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (TankMappingContext)
			{
				Subsystem->AddMappingContext(TankMappingContext, MappingPriority);
			}
		}
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTankGame, Warning, TEXT("%s: input component is not an Enhanced Input component; tank thumbstick input disabled"), *GetName());
		return;
	}
	if (DriveAction)
	{
		EnhancedInput->BindAction(DriveAction, ETriggerEvent::Triggered, this, &ATankVehicle::OnDriveAction);
		EnhancedInput->BindAction(DriveAction, ETriggerEvent::Completed, this, &ATankVehicle::OnDriveReleased);
	}
	if (AimAction)
	{
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ATankVehicle::OnAimAction);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &ATankVehicle::OnAimReleased);
	}
	if (ToggleViewAction)
	{
		EnhancedInput->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &ATankVehicle::OnToggleView);
	}
}

void ATankVehicle::OnToggleView(const FInputActionValue& Value)
{
	ToggleOverviewView();
}

void ATankVehicle::SetOverviewView(bool bEnable)
{
	bOverviewView = bEnable;

	APlayerController* PC = InputController.Get();
	if (!PC && GetWorld())
	{
		PC = GetWorld()->GetFirstPlayerController();
	}
	if (PC)
	{
		AActor* ViewTarget = bEnable ? static_cast<AActor*>(this) : static_cast<AActor*>(Seat->GetRider());
		if (!ViewTarget)
		{
			ViewTarget = PC->GetPawn();
		}
		if (ViewTarget)
		{
			const bool bHeadset = UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled();
			PC->SetViewTargetWithBlend(ViewTarget, bHeadset ? 0.0f : ViewBlendTime, VTBlend_Cubic);
		}
	}

	if (bEnable && !OverviewHud && OverviewHudClass && GetWorld())
	{
		OverviewHud = (PC && PC->GetLocalPlayer()) ? CreateWidget<UUserWidget>(PC, OverviewHudClass) : CreateWidget<UUserWidget>(GetWorld(), OverviewHudClass);
		if (UTankStatusHudWidget* StatusHud = Cast<UTankStatusHudWidget>(OverviewHud))
		{
			StatusHud->SetTank(this);
		}
		if (OverviewHud && PC && PC->GetLocalPlayer() && GetWorld()->GetGameViewport())
		{
			OverviewHud->AddToViewport(10);
		}
	}
	if (OverviewHud)
	{
		OverviewHud->SetVisibility(bEnable ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UE_LOG(LogTankGame, Log, TEXT("%s view: %s"), *GetName(), bEnable ? TEXT("overview camera") : TEXT("cockpit"));
}

void ATankVehicle::OnDriveAction(const FInputActionValue& Value)
{
	FallbackDrive = Value.Get<FVector2D>();
}

void ATankVehicle::OnDriveReleased(const FInputActionValue& Value)
{
	FallbackDrive = FVector2D::ZeroVector;
}

void ATankVehicle::OnAimAction(const FInputActionValue& Value)
{
	FallbackAim = Value.Get<FVector2D>();
}

void ATankVehicle::OnAimReleased(const FInputActionValue& Value)
{
	FallbackAim = FVector2D::ZeroVector;
}

void ATankVehicle::SetFallbackInputs(FVector2D Drive, FVector2D Aim)
{
	FallbackDrive = Drive;
	FallbackAim = Aim;
}

void ATankVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Held stick wins over the thumbstick.
	DriveStick->SetExternalValue(FallbackDrive);
	AimStick->SetExternalValue(FallbackAim);
	const FVector2D Drive = DriveStick->IsHeld() ? DriveStick->GetValue() : FallbackDrive;
	const FVector2D Aim = AimStick->IsHeld() ? AimStick->GetValue() : FallbackAim;

	TankMovement->SetDriveInput(Drive);
	Targeting->SetAimInput(Aim);
}
