#include "Audio/TankEnemyAudioComponent.h"
#include "Audio/TankAudio.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/TankEnemy.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

UTankEnemyAudioComponent::UTankEnemyAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	EngineLoop = TankAudio::Sound(TEXT("SW_Enemy_Engine_Loop"));
	DestroyedSound = TankAudio::Sound(TEXT("SW_Enemy_Destroyed"));
	WreckFireLoop = TankAudio::Sound(TEXT("SW_Fire_Loop"));
}

void UTankEnemyAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	ATankEnemy* Enemy = Cast<ATankEnemy>(GetOwner());
	if (!Enemy || !Enemy->GetRootComponent())
	{
		return;
	}
	Engine = UTankAudioSubsystem::MakeLoop(Enemy->GetRootComponent(), EngineLoop.LoadSynchronous(), false, EngineReach);
	WreckFire = UTankAudioSubsystem::MakeLoop(Enemy->GetRootComponent(), WreckFireLoop.LoadSynchronous(), false, 5000.0f);
	if (UTankHealthComponent* Health = Enemy->GetHealth())
	{
		Health->OnDied.AddUniqueDynamic(this, &UTankEnemyAudioComponent::HandleDied);
	}
}

void UTankEnemyAudioComponent::HandleDied(UTankHealthComponent* HealthComponent, AActor* Killer)
{
	if (const AActor* Owner = GetOwner())
	{
		UTankAudioSubsystem::PlayAt(this, DestroyedSound.LoadSynchronous(), Owner->GetActorLocation(), 1.0f, FMath::FRandRange(0.92f, 1.08f), 35000.0f);
	}
}

void UTankEnemyAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ATankEnemy* Enemy = Cast<ATankEnemy>(GetOwner());
	if (!Enemy)
	{
		return;
	}
	const bool bWreck = Enemy->IsWrecked();
	const UEnemyDriveComponent* Drive = Enemy->GetDrive();
	const float Speed = Drive ? FMath::Clamp(FMath::Abs(Drive->GetSpeed()) / FMath::Max(Drive->MaxSpeed, 1.0f), 0.0f, 1.0f) : 0.0f;
	UTankAudioSubsystem::SetLoop(Engine, bWreck ? 0.0f : EngineVolume * FMath::Lerp(0.6f, 1.0f, Speed), EnginePitch * FMath::Lerp(0.9f, 1.3f, Speed));
	UTankAudioSubsystem::SetLoop(WreckFire, bWreck ? 0.8f : 0.0f);
}
