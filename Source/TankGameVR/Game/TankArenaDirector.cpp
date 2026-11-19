#include "Game/TankArenaDirector.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Enemies/TankEnemy.h"
#include "Enemies/TankEnemyTypes.h"
#include "Game/TankEnemySpawnPoint.h"
#include "Tank/TankVehicle.h"
#include "TankGameVR.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"

ATankArenaDirector::ATankArenaDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<UBillboardComponent>(TEXT("Root")));
	TankClass = ATankEnemy_Tank::StaticClass();
	ArmoredCarClass = ATankEnemy_ArmoredCar::StaticClass();
	AntiTankClass = ATankEnemy_AntiTank::StaticClass();

	// Each wave adds a new problem: tanks, then fast cars, then AT guns, then all of it.
	Waves = {
		{ 2, 0, 0 },
		{ 1, 2, 0 },
		{ 1, 1, 1 },
		{ 2, 2, 1 },
		{ 2, 2, 2 },
	};
}

ATankArenaDirector* ATankArenaDirector::GetDirector(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATankArenaDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATankArenaDirector::BeginPlay()
{
	Super::BeginPlay();
	BindPlayer();
	if (bAutoStart)
	{
		StartGame();
	}
}

void ATankArenaDirector::BindPlayer()
{
	if (Player.IsValid())
	{
		return;
	}
	for (TActorIterator<ATankVehicle> It(GetWorld()); It; ++It)
	{
		Player = *It;
		PlayerStart = It->GetActorTransform();
		if (UTankHealthComponent* Health = It->FindComponentByClass<UTankHealthComponent>())
		{
			Health->OnDied.AddUniqueDynamic(this, &ATankArenaDirector::HandlePlayerDied);
		}
		break;
	}
}

void ATankArenaDirector::SetPhase(ETankArenaPhase NewPhase, float Duration)
{
	Phase = NewPhase;
	PhaseTime = 0.0f;
	PhaseDuration = Duration;
}

void ATankArenaDirector::StartGame()
{
	BindPlayer();
	Wave = 0;
	Score = 0;
	Kills = 0;
	Queue.Reset();
	SetPhase(ETankArenaPhase::Break, FirstWaveDelay);
}

void ATankArenaDirector::ClearEnemies()
{
	for (const TWeakObjectPtr<ATankEnemy>& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	Queue.Reset();
	for (TActorIterator<ATankSmokeCloud> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}

void ATankArenaDirector::RestartGame()
{
	ClearEnemies();
	if (ATankVehicle* Tank = Player.Get())
	{
		Tank->SetActorTransform(PlayerStart, false, nullptr, ETeleportType::TeleportPhysics);
		if (UTankHealthComponent* Health = Tank->FindComponentByClass<UTankHealthComponent>())
		{
			Health->ResetHealth();
		}
		if (UTankDamageReactionComponent* Reaction = Tank->FindComponentByClass<UTankDamageReactionComponent>())
		{
			Reaction->ResetDamage();
		}
	}
	UE_LOG(LogTankGame, Log, TEXT("Arena restarted"));
	StartGame();
}

void ATankArenaDirector::SkipWave()
{
	ClearEnemies();
	BeginWave(Wave + 1);
}

FTankWave ATankArenaDirector::GetWaveSetup(int32 WaveNumber) const
{
	if (Waves.Num() == 0)
	{
		return FTankWave();
	}
	if (Waves.IsValidIndex(WaveNumber - 1))
	{
		return Waves[WaveNumber - 1];
	}
	// Endless: the last wave, one more of each type every two waves.
	FTankWave Setup = Waves.Last();
	const int32 Extra = (WaveNumber - Waves.Num() + 1) / 2;
	Setup.Tanks += Extra;
	Setup.ArmoredCars += Extra;
	Setup.AntiTankGuns += Extra / 2;
	return Setup;
}

void ATankArenaDirector::BeginWave(int32 WaveNumber)
{
	Wave = WaveNumber;
	const FTankWave Setup = GetWaveSetup(WaveNumber);
	Queue.Reset();
	// Mixed order, but the slow ones first so the fast cars don't arrive alone.
	for (int32 Index = 0; Index < Setup.Tanks; ++Index)
	{
		Queue.Add(ETankEnemyType::Tank);
	}
	for (int32 Index = 0; Index < Setup.AntiTankGuns; ++Index)
	{
		Queue.Add(ETankEnemyType::AntiTank);
	}
	for (int32 Index = 0; Index < Setup.ArmoredCars; ++Index)
	{
		Queue.Add(ETankEnemyType::ArmoredCar);
	}
	SpawnTimer = 0.0f;
	WaveTime = 0.0f;
	SetPhase(ETankArenaPhase::Fighting);
	UE_LOG(LogTankGame, Log, TEXT("Wave %d: %d tanks, %d armored cars, %d AT guns"), Wave, Setup.Tanks, Setup.ArmoredCars, Setup.AntiTankGuns);
}

bool ATankArenaDirector::IsSpotFree(const FVector& Location, float Radius) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpawnFree), false);
	return !GetWorld()->OverlapAnyTestByChannel(Location + FVector(0, 0, Radius + 40.0f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params);
}

bool ATankArenaDirector::FindSpawnTransform(ETankEnemyType Type, FTransform& Out)
{
	const FVector PlayerLocation = Player.IsValid() ? Player->GetActorLocation() : GetActorLocation();
	auto Facing = [&](const FVector& Location)
	{
		return FRotator(0.0f, (PlayerLocation - Location).GetSafeNormal2D().Rotation().Yaw, 0.0f);
	};
	auto Ground = [&](FVector Location)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpawnGround), false);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 2000), Location - FVector(0, 0, 4000), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Location.Z = Hit.ImpactPoint.Z;
		}
		return Location;
	};

	// Spawn points that allow this type, far enough away, not used a moment ago.
	TArray<ATankEnemySpawnPoint*> Points;
	const double Now = GetWorld()->GetTimeSeconds();
	for (TActorIterator<ATankEnemySpawnPoint> It(GetWorld()); It; ++It)
	{
		const bool bAllowed = (Type == ETankEnemyType::Tank && It->bAllowTanks) || (Type == ETankEnemyType::ArmoredCar && It->bAllowArmoredCars)
			|| (Type == ETankEnemyType::AntiTank && It->bAllowAntiTankGuns);
		const double* Used = SpawnPointUsed.Find(*It);
		if (bAllowed && FVector::Dist2D(It->GetActorLocation(), PlayerLocation) >= MinDistanceFromPlayer && (!Used || Now - *Used > 4.0)
			&& IsSpotFree(Ground(It->GetActorLocation()), 250.0f))
		{
			Points.Add(*It);
		}
	}
	if (Points.Num() > 0)
	{
		ATankEnemySpawnPoint* Point = Points[FMath::RandRange(0, Points.Num() - 1)];
		SpawnPointUsed.Add(Point, Now);
		const FVector Location = Ground(Point->GetActorLocation()) + FVector(0, 0, 150);
		Out = FTransform(Facing(Location), Location);
		return true;
	}

	// No spawn points (or all busy): a free spot on a ring around the player.
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const float Yaw = FMath::FRandRange(0.0f, 360.0f);
		const float Distance = FMath::Max(MinDistanceFromPlayer, FMath::FRandRange(FallbackSpawnDistance.X, FallbackSpawnDistance.Y));
		FVector Location = PlayerLocation + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance;
		FNavLocation Projected;
		if (Nav && Nav->ProjectPointToNavigation(Location, Projected, FVector(500.0, 500.0, 1000.0)))
		{
			Location = Projected.Location;
		}
		Location = Ground(Location);
		if (FVector::Dist2D(Location, PlayerLocation) >= MinDistanceFromPlayer && IsSpotFree(Location, 300.0f))
		{
			Out = FTransform(Facing(Location), Location + FVector(0, 0, 150));
			return true;
		}
	}
	return false;
}

ATankEnemy* ATankArenaDirector::SpawnEnemy(ETankEnemyType Type, FTransform Transform)
{
	TSubclassOf<ATankEnemy> Class = TankClass;
	if (Type == ETankEnemyType::ArmoredCar)
	{
		Class = ArmoredCarClass;
	}
	else if (Type == ETankEnemyType::AntiTank)
	{
		Class = AntiTankClass;
	}
	if (!Class)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ATankEnemy* Enemy = GetWorld()->SpawnActor<ATankEnemy>(Class, Transform, Params);
	if (Enemy)
	{
		Enemy->GetHealth()->OnDied.AddUniqueDynamic(this, &ATankArenaDirector::HandleEnemyDied);
		Enemies.Add(Enemy);
	}
	return Enemy;
}

void ATankArenaDirector::SpawnNext()
{
	if (Queue.Num() == 0)
	{
		return;
	}
	FTransform Transform;
	if (FindSpawnTransform(Queue[0], Transform))
	{
		SpawnEnemy(Queue[0], Transform);
		Queue.RemoveAt(0);
	}
}

void ATankArenaDirector::HandleEnemyDied(UTankHealthComponent* HealthComponent, AActor* Killer)
{
	if (const ATankEnemy* Enemy = Cast<ATankEnemy>(HealthComponent->GetOwner()))
	{
		if (Phase == ETankArenaPhase::Fighting || Phase == ETankArenaPhase::Break)
		{
			Score += Enemy->KillScore;
			++Kills;
		}
	}
}

void ATankArenaDirector::HandlePlayerDied(UTankHealthComponent* HealthComponent, AActor* Killer)
{
	if (Phase != ETankArenaPhase::GameOver)
	{
		UE_LOG(LogTankGame, Log, TEXT("Player tank destroyed in wave %d, score %d"), Wave, Score);
		Queue.Reset();
		SetPhase(ETankArenaPhase::GameOver, RestartDelay);
	}
}

int32 ATankArenaDirector::GetEnemiesLeft() const
{
	int32 Count = Queue.Num();
	for (const TWeakObjectPtr<ATankEnemy>& Enemy : Enemies)
	{
		Count += (Enemy.IsValid() && !Enemy->IsWrecked()) ? 1 : 0;
	}
	return Count;
}

TArray<ATankEnemy*> ATankArenaDirector::GetEnemies() const
{
	TArray<ATankEnemy*> Out;
	for (const TWeakObjectPtr<ATankEnemy>& Enemy : Enemies)
	{
		if (Enemy.IsValid())
		{
			Out.Add(Enemy.Get());
		}
	}
	return Out;
}

FString ATankArenaDirector::GetMessage() const
{
	const int32 Left = FMath::Max(0, FMath::CeilToInt(PhaseDuration - PhaseTime));
	switch (Phase)
	{
	case ETankArenaPhase::Break:
		if (Wave > 0 && PhaseTime < 3.0f)
		{
			return LastRepair > 0.5f ? FString::Printf(TEXT("WAVE %d CLEAR - CREW REPAIRS +%d"), Wave, FMath::RoundToInt(LastRepair)) : FString::Printf(TEXT("WAVE %d CLEAR"), Wave);
		}
		return FString::Printf(TEXT("WAVE %d IN %d"), Wave + 1, Left);
	case ETankArenaPhase::Fighting:
		if (WaveTime < 4.0f)
		{
			const FTankWave Setup = GetWaveSetup(Wave);
			TArray<FString> Parts;
			if (Setup.Tanks > 0)
			{
				Parts.Add(FString::Printf(TEXT("%d TANK%s"), Setup.Tanks, Setup.Tanks > 1 ? TEXT("S") : TEXT("")));
			}
			if (Setup.ArmoredCars > 0)
			{
				Parts.Add(FString::Printf(TEXT("%d CAR%s"), Setup.ArmoredCars, Setup.ArmoredCars > 1 ? TEXT("S") : TEXT("")));
			}
			if (Setup.AntiTankGuns > 0)
			{
				Parts.Add(FString::Printf(TEXT("%d AT GUN%s"), Setup.AntiTankGuns, Setup.AntiTankGuns > 1 ? TEXT("S") : TEXT("")));
			}
			return FString::Printf(TEXT("WAVE %d: %s"), Wave, *FString::Join(Parts, TEXT(", ")));
		}
		return FString();
	case ETankArenaPhase::GameOver:
		return FString::Printf(TEXT("TANK DESTROYED - RESTART IN %d"), Left);
	case ETankArenaPhase::Victory:
		return FString::Printf(TEXT("AREA CLEAR - SCORE %d"), Score);
	default:
		return FString();
	}
}

FString ATankArenaDirector::GetStatusLine() const
{
	if (Wave == 0)
	{
		return FString::Printf(TEXT("SCORE %d"), Score);
	}
	return FString::Printf(TEXT("WAVE %d  ENEMY %d  SCORE %d"), Wave, GetEnemiesLeft(), Score);
}

void ATankArenaDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PhaseTime += DeltaSeconds;
	Enemies.RemoveAll([](const TWeakObjectPtr<ATankEnemy>& Enemy) { return !Enemy.IsValid(); });
	if (!Player.IsValid())
	{
		BindPlayer();
	}

	switch (Phase)
	{
	case ETankArenaPhase::Break:
		if (PhaseTime >= PhaseDuration)
		{
			BeginWave(Wave + 1);
		}
		break;

	case ETankArenaPhase::Fighting:
		WaveTime += DeltaSeconds;
		SpawnTimer -= DeltaSeconds;
		if (Queue.Num() > 0 && SpawnTimer <= 0.0f)
		{
			SpawnTimer = SpawnInterval;
			SpawnNext();
		}
		if (GetEnemiesLeft() == 0)
		{
			if (!bEndless && Wave >= Waves.Num())
			{
				UE_LOG(LogTankGame, Log, TEXT("All waves cleared, score %d"), Score);
				SetPhase(ETankArenaPhase::Victory, RestartDelay * 2.0f);
			}
			else
			{
				LastRepair = 0.0f;
				if (UTankHealthComponent* Health = Player.IsValid() ? Player->FindComponentByClass<UTankHealthComponent>() : nullptr)
				{
					LastRepair = Health->Heal(RepairBetweenWaves);
				}
				SetPhase(ETankArenaPhase::Break, TimeBetweenWaves);
			}
		}
		break;

	case ETankArenaPhase::GameOver:
	case ETankArenaPhase::Victory:
		if (PhaseTime >= PhaseDuration)
		{
			RestartGame();
		}
		break;

	default:
		break;
	}
}

static FAutoConsoleCommandWithWorld GTankSkipWaveCommand(
	TEXT("Tank.SkipWave"),
	TEXT("Arena: clears the current wave and starts the next one."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ATankArenaDirector* Director = ATankArenaDirector::GetDirector(World))
		{
			Director->SkipWave();
		}
	}));

static FAutoConsoleCommandWithWorld GTankRestartCommand(
	TEXT("Tank.Restart"),
	TEXT("Arena: restarts the match."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (ATankArenaDirector* Director = ATankArenaDirector::GetDirector(World))
		{
			Director->RestartGame();
		}
	}));
