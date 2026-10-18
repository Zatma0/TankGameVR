#include "Testing/TankTestWaveSpawner.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetableComponent.h"
#include "TankGameVR.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

ATankTestWaveSpawner::ATankTestWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<UBillboardComponent>(TEXT("Root")));
}

void ATankTestWaveSpawner::BeginPlay()
{
	Super::BeginPlay();
	bWaitingForWave = bAutoStart;
	NextWaveTimer = FirstWaveDelay;
}

int32 ATankTestWaveSpawner::GetAliveCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Enemy : Alive)
	{
		Count += (Enemy.IsValid() && !Enemy->IsActorBeingDestroyed()) ? 1 : 0;
	}
	return Count;
}

void ATankTestWaveSpawner::SpawnWave()
{
	if (!EnemyClass || (NumberOfWaves > 0 && CurrentWave >= NumberOfWaves))
	{
		return;
	}
	++CurrentWave;
	const FVector Origin = GetActorLocation();
	const float BaseYaw = GetActorRotation().Yaw;
	for (int32 Index = 0; Index < EnemiesPerWave; ++Index)
	{
		const float Alpha = EnemiesPerWave > 1 ? static_cast<float>(Index) / (EnemiesPerWave - 1) : 0.5f;
		const float Yaw = BaseYaw + (Alpha - 0.5f) * SpawnArc + FMath::FRandRange(-4.0f, 4.0f);
		const float Distance = FMath::FRandRange(SpawnDistance.X, SpawnDistance.Y);
		const FVector Direction = FRotator(0.0f, Yaw, 0.0f).Vector();
		const FTransform Transform(FRotator(0.0f, Yaw + 180.0f, 0.0f), Origin + Direction * Distance);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AActor* Enemy = GetWorld()->SpawnActor<AActor>(EnemyClass, Transform, Params))
		{
			if (UTargetableComponent* Targetable = Enemy->FindComponentByClass<UTargetableComponent>())
			{
				Targetable->DisplayName = FText::FromString(FString::Printf(TEXT("W%d-%d"), CurrentWave, Index + 1));
			}
			if (EnemyLifetime.Y > 0.0f)
			{
				Enemy->SetLifeSpan(FMath::FRandRange(EnemyLifetime.X, EnemyLifetime.Y));
			}
			Alive.Add(Enemy);
		}
	}
	UE_LOG(LogTankGame, Log, TEXT("Test wave %d spawned (%d enemies)"), CurrentWave, GetAliveCount());
}

void ATankTestWaveSpawner::SkipToNextWave()
{
	for (const TWeakObjectPtr<AActor>& Enemy : Alive)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Alive.Reset();
	SpawnWave();
}

void ATankTestWaveSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Alive.RemoveAll([](const TWeakObjectPtr<AActor>& Enemy) { return !Enemy.IsValid() || Enemy->IsActorBeingDestroyed(); });

	if (!bWaitingForWave && Alive.Num() == 0 && CurrentWave > 0)
	{
		bWaitingForWave = true;
		NextWaveTimer = TimeBetweenWaves;
	}
	if (bWaitingForWave)
	{
		NextWaveTimer -= DeltaSeconds;
		if (NextWaveTimer <= 0.0f)
		{
			bWaitingForWave = false;
			SpawnWave();
		}
	}
}

static FAutoConsoleCommandWithWorld GTankDestroyTargetCommand(
	TEXT("Tank.DestroyTarget"),
	TEXT("Destroys the actor currently locked by every tank (testing)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TObjectIterator<UTankTargetingComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCurrentTarget())
			{
				It->GetCurrentTarget()->GetOwner()->Destroy();
			}
		}
	}));

static FAutoConsoleCommandWithWorld GTankNextWaveCommand(
	TEXT("Tank.NextWave"),
	TEXT("Clears the current test wave and spawns the next one."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ATankTestWaveSpawner> It(World); It; ++It)
		{
			It->SkipToNextWave();
		}
	}));
