#include "Audio/TankAudioComponent.h"
#include "Audio/TankAudio.h"
#include "Cockpit/CockpitExtinguisherComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/TankEnemy.h"
#include "Tank/TankMovementComponent.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetableComponent.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"

UTankAudioComponent::UTankAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	EngineLoop = TankAudio::Sound(TEXT("SW_Engine_Loop"));
	TracksLoop = TankAudio::Sound(TEXT("SW_Tracks_Loop"));
	TurretLoop = TankAudio::Sound(TEXT("SW_Turret_Loop"));
	FireLoop = TankAudio::Sound(TEXT("SW_Fire_Loop"));
	ExtinguisherLoop = TankAudio::Sound(TEXT("SW_Extinguisher_Loop"));
	WarningLoop = TankAudio::Sound(TEXT("SW_Warning_Loop"));
	HullHit = TankAudio::Sound(TEXT("SW_Hull_Hit"));
	HullHitLight = TankAudio::Sound(TEXT("SW_Hull_Hit_Light"));
	LockBeep = TankAudio::Sound(TEXT("SW_Lock_Beep"));
	OnTargetBeep = TankAudio::Sound(TEXT("SW_On_Target"));
	WaveStart = TankAudio::Sound(TEXT("SW_Wave_Start"));
	WaveClear = TankAudio::Sound(TEXT("SW_Wave_Clear"));
	GameOver = TankAudio::Sound(TEXT("SW_Game_Over"));
}

void UTankAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	USceneComponent* Root = Tank ? Tank->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}
	// All of these are heard from the seat, so none of them is placed in the world.
	Engine = UTankAudioSubsystem::MakeLoop(Root, EngineLoop.LoadSynchronous(), true);
	Tracks = UTankAudioSubsystem::MakeLoop(Root, TracksLoop.LoadSynchronous(), true);
	Turret = UTankAudioSubsystem::MakeLoop(Root, TurretLoop.LoadSynchronous(), true);
	Fire = UTankAudioSubsystem::MakeLoop(Root, FireLoop.LoadSynchronous(), true);
	Extinguisher = UTankAudioSubsystem::MakeLoop(Root, ExtinguisherLoop.LoadSynchronous(), true);
	Warning = UTankAudioSubsystem::MakeLoop(Root, WarningLoop.LoadSynchronous(), true);
	if (UTankHealthComponent* Health = Tank->GetHealth())
	{
		Health->OnHealthChanged.AddUniqueDynamic(this, &UTankAudioComponent::HandleHealthChanged);
	}
}

void UTankAudioComponent::HandleHealthChanged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType)
{
	// Burning is heard as the fire itself, not as a hit.
	if (DamageType && DamageType->IsA<UTankDamageType_Fire>())
	{
		return;
	}
	const ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	const UTankDamageReactionComponent* Reaction = Tank ? Tank->GetDamageReaction() : nullptr;
	const bool bHeavy = !Reaction || Damage >= Reaction->HeavyHitDamage;
	if (bHeavy)
	{
		UTankAudioSubsystem::PlayInside(this, HullHit.LoadSynchronous(), 0.85f, FMath::FRandRange(0.94f, 1.06f));
	}
	else
	{
		// Small rounds still have to be heard over the engine: they mean someone has found you.
		UTankAudioSubsystem::PlayInside(this, HullHitLight.LoadSynchronous(), 1.4f, FMath::FRandRange(0.85f, 1.2f));
	}
}

void UTankAudioComponent::UpdateLoops(ATankVehicle* Tank, float DeltaTime)
{
	const bool bDead = Tank->IsDestroyed();
	const UTankMovementComponent* Movement = Tank->GetTankMovement();
	const float TopSpeed = FMath::Max(Movement->Tuning.MaximumForwardSpeed, 1.0f);
	const float Speed = FMath::Clamp(FMath::Abs(Movement->GetCurrentSpeed()) / TopSpeed, 0.0f, 1.0f);
	const float Throttle = FMath::Clamp(static_cast<float>(FMath::Max(FMath::Abs(Movement->ProcessedInput.Y), FMath::Abs(Movement->ProcessedInput.X) * 0.6)), 0.0f, 1.0f);

	// The engine answers the stick at once and the tank follows, like the real thing.
	Rev = FMath::FInterpTo(Rev, bDead ? 0.0f : FMath::Max(Speed, Throttle), DeltaTime, 3.5f);
	UTankAudioSubsystem::SetLoop(Engine, bDead ? 0.0f : FMath::Lerp(EngineVolume.X, EngineVolume.Y, Rev), FMath::Lerp(EnginePitch.X, EnginePitch.Y, Rev));
	UTankAudioSubsystem::SetLoop(Tracks, bDead ? 0.0f : TracksVolume * FMath::Clamp(Speed * 1.3f, 0.0f, 1.0f), FMath::Lerp(0.7f, 1.25f, Speed));

	const UTankTargetingComponent* Targeting = Tank->GetTargeting();
	const float Turning = FMath::Clamp(FMath::Abs(Targeting->GetTurretSpeed()) / FMath::Max(Targeting->TurretTraverseSpeed, 1.0f), 0.0f, 1.0f);
	// Up quickly, down a touch slower, so the motor is heard to run down.
	TurretLevel = FMath::FInterpTo(TurretLevel, bDead ? 0.0f : Turning, DeltaTime, Turning > TurretLevel ? 14.0f : 8.0f);
	UTankAudioSubsystem::SetLoop(Turret, TurretVolume * TurretLevel, FMath::Lerp(0.8f, 1.15f, TurretLevel));

	float Burning = 0.0f;
	for (const UCockpitFireComponent* Spot : Tank->GetFireSpots())
	{
		Burning = FMath::Max(Burning, Spot && Spot->IsBurning() ? Spot->GetIntensity() : 0.0f);
	}
	UTankAudioSubsystem::SetLoop(Fire, Burning > 0.0f ? 0.2f + 0.55f * Burning : 0.0f, FMath::Lerp(0.9f, 1.1f, Burning));
	const bool bSpraying = Tank->GetExtinguisher() && Tank->GetExtinguisher()->IsSpraying();
	UTankAudioSubsystem::SetLoop(Extinguisher, bSpraying ? 0.65f : 0.0f);

	// Horn: all the time an AT gun is laying on us, and for a moment when a fire breaks out.
	float Threat = 0.0f;
	for (TActorIterator<ATankEnemy> It(GetWorld()); It; ++It)
	{
		if (It->EnemyType == ETankEnemyType::AntiTank)
		{
			Threat = FMath::Max(Threat, It->GetThreatTo(Tank));
		}
	}
	const bool bBurning = Burning > 0.0f;
	if (bBurning && !bWasBurning)
	{
		FireAlarm = FireAlarmTime;
	}
	bWasBurning = bBurning;
	FireAlarm = FMath::Max(0.0f, FireAlarm - DeltaTime);
	const bool bHorn = !bDead && (Threat > 0.05f || (FireAlarm > 0.0f && bBurning));
	UTankAudioSubsystem::SetLoop(Warning, bHorn ? WarningVolume : 0.0f, Threat > 0.05f ? 1.0f : 0.8f);
}

void UTankAudioComponent::UpdateFireControl(ATankVehicle* Tank)
{
	const UTankTargetingComponent* Targeting = Tank->GetTargeting();
	UTargetableComponent* Target = Targeting->GetCurrentTarget();
	if (Target && Target != LastTarget.Get())
	{
		UTankAudioSubsystem::PlayInside(this, LockBeep.LoadSynchronous(), BeepVolume);
		bWasOnTarget = false;
	}
	LastTarget = Target;
	const bool bOnTarget = Target && Targeting->IsOnTarget();
	if (bOnTarget && !bWasOnTarget)
	{
		UTankAudioSubsystem::PlayInside(this, OnTargetBeep.LoadSynchronous(), BeepVolume);
	}
	bWasOnTarget = bOnTarget;
}

void UTankAudioComponent::UpdateRadio(ATankVehicle* Tank)
{
	const ATankArenaDirector* Director = ATankArenaDirector::GetDirector(this);
	if (!Director)
	{
		return;
	}
	const ETankArenaPhase Phase = Director->GetPhase();
	const int32 Wave = Director->GetWave();
	if (Phase == ETankArenaPhase::Fighting && (Wave != LastWave || LastPhase == ETankArenaPhase::Break) && Wave > 0 && !Director->HasExternalWaves())
	{
		UTankAudioSubsystem::PlayInside(this, WaveStart.LoadSynchronous(), BeepVolume);
	}
	else if (Phase != LastPhase && (Phase == ETankArenaPhase::Break || Phase == ETankArenaPhase::Victory) && Wave > 0)
	{
		UTankAudioSubsystem::PlayInside(this, WaveClear.LoadSynchronous(), BeepVolume);
	}
	else if (Phase != LastPhase && Phase == ETankArenaPhase::GameOver)
	{
		UTankAudioSubsystem::PlayInside(this, GameOver.LoadSynchronous(), 0.7f);
	}
	LastPhase = Phase;
	LastWave = Wave;
}

void UTankAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	if (!Tank || DeltaTime <= 0.0f)
	{
		return;
	}
	UpdateLoops(Tank, DeltaTime);
	UpdateFireControl(Tank);
	UpdateRadio(Tank);
}
