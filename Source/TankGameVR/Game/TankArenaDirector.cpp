#include "Game/TankArenaDirector.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Combat/TankSmokeCloud.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/EnemyWeaponComponent.h"
#include "Enemies/TankEnemy.h"
#include "Enemies/TankEnemyTypes.h"
#include "Game/TankEnemySpawnPoint.h"
#include "Tank/TankVisionConeComponent.h"
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
	if (bAutoStart || bExternalWaves)
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
	if (bExternalWaves)
	{
		// The other spawner sends the enemies; we just keep score.
		SetPhase(ETankArenaPhase::Fighting);
	}
	else
	{
		SetPhase(ETankArenaPhase::Break, FirstWaveDelay);
	}
}

void ATankArenaDirector::SetExternalWaves(bool bExternal)
{
	if (bExternalWaves == bExternal)
	{
		return;
	}
	bExternalWaves = bExternal;
	if (!HasActorBegunPlay())
	{
		return;
	}
	if (!bExternal)
	{
		StartGame();
	}
	else if (Phase != ETankArenaPhase::GameOver)
	{
		// Our own waves stop; whatever is already out stays.
		Queue.Reset();
		SetPhase(ETankArenaPhase::Fighting);
	}
}

void ATankArenaDirector::SetExternalWaveNumber(int32 Number)
{
	if (bExternalWaves)
	{
		Wave = FMath::Max(0, Number);
	}
}

void ATankArenaDirector::SetExternalWave(int32 Number, float SecondsToNextWave)
{
	SetExternalWaves(true);
	Wave = FMath::Max(0, Number);
	ExternalNextWaveTime = SecondsToNextWave > 0.0f && GetWorld() ? GetWorld()->GetTimeSeconds() + SecondsToNextWave : -1.0;
}

float ATankArenaDirector::GetTimeToNextWave() const
{
	if (bExternalWaves)
	{
		return ExternalNextWaveTime >= 0.0 && GetWorld() ? FMath::Max(0.0f, static_cast<float>(ExternalNextWaveTime - GetWorld()->GetTimeSeconds())) : -1.0f;
	}
	return Phase == ETankArenaPhase::Break ? FMath::Max(0.0f, PhaseDuration - PhaseTime) : -1.0f;
}

int32 ATankArenaDirector::CountEnemies(ETankEnemyType Type) const
{
	int32 Count = 0;
	for (const ETankEnemyType Queued : Queue)
	{
		Count += Queued == Type ? 1 : 0;
	}
	for (const TWeakObjectPtr<ATankEnemy>& Enemy : Enemies)
	{
		Count += (Enemy.IsValid() && !Enemy->IsWrecked() && Enemy->EnemyType == Type) ? 1 : 0;
	}
	return Count;
}

FTransform ATankArenaDirector::FindEnemySpotNear(FVector Wanted, ETankEnemyType Type)
{
	const FVector PlayerLocation = Player.IsValid() ? Player->GetActorLocation() : GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpotNear), false);
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, Wanted + FVector(0, 0, 200), Wanted - FVector(0, 0, 50000), FCollisionObjectQueryParams(ECC_WorldStatic), Params)
		&& !Hit.bStartPenetrating;
	const bool bInView = bSpawnOutOfSight && bGround && PlayerCanSee(Hit.ImpactPoint + FVector(0, 0, 150));
	if (bGround && !bInView && Hit.ImpactNormal.Z >= 0.94 && FVector::Dist2D(Hit.ImpactPoint, PlayerLocation) >= MinDistanceFromPlayer * 0.5f && IsSpotFree(Hit.ImpactPoint, 300.0f))
	{
		const FRotator Facing(0.0f, (PlayerLocation - Hit.ImpactPoint).GetSafeNormal2D().Rotation().Yaw, 0.0f);
		return FTransform(Facing, Hit.ImpactPoint + FVector(0, 0, 150));
	}
	// In the air above a roof, inside a building, on a cliff, on top of us or in plain view: come in the usual way.
	FTransform Out;
	if (FindSpawnTransform(Type, Out))
	{
		return Out;
	}
	return FTransform(bGround ? Hit.ImpactPoint + FVector(0, 0, 150) : Wanted);
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
	GroupLeft = 0;
	WaveTime = 0.0f;
	SetPhase(ETankArenaPhase::Fighting);
	UE_LOG(LogTankGame, Log, TEXT("Wave %d: %d tanks, %d armored cars, %d AT guns"), Wave, Setup.Tanks, Setup.ArmoredCars, Setup.AntiTankGuns);
}

bool ATankArenaDirector::IsSpotFree(const FVector& Location, float Radius) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpawnFree), false);
	return !GetWorld()->OverlapAnyTestByChannel(Location + FVector(0, 0, Radius + 40.0f), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Radius), Params);
}

bool ATankArenaDirector::FindSpawnTransform(ETankEnemyType Type, FTransform& Out, bool bPreferInSight)
{
	const FVector PlayerLocation = Player.IsValid() ? Player->GetActorLocation() : GetActorLocation();
	auto Facing = [&](const FVector& Location)
	{
		return FRotator(0.0f, (PlayerLocation - Location).GetSafeNormal2D().Rotation().Yaw, 0.0f);
	};
	auto Ground = [&](FVector Location, FVector* OutNormal = nullptr)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaSpawnGround), false);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0, 0, 2000), Location - FVector(0, 0, 4000), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Location.Z = Hit.ImpactPoint.Z;
			if (OutNormal)
			{
				*OutNormal = Hit.ImpactNormal;
			}
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
	if (bSpawnOutOfSight && Points.Num() > 0)
	{
		// The ones the player is not looking at, if there are any.
		const TArray<ATankEnemySpawnPoint*> Hidden = Points.FilterByPredicate([&](const ATankEnemySpawnPoint* Point)
		{
			return !PlayerCanSee(Ground(Point->GetActorLocation()) + FVector(0, 0, 150));
		});
		if (Hidden.Num() > 0)
		{
			Points = Hidden;
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
	const double PlayerGroundZ = Ground(PlayerLocation).Z;
	for (int32 Attempt = 0; Attempt < 32; ++Attempt)
	{
		// For the first half of the tries insist on a spot that can see the player.
		const bool bNeedSight = bPreferInSight && Attempt < 16;
		// And for most of them on one the player is not looking at.
		const bool bNeedHidden = bSpawnOutOfSight && Attempt < 24;
		const float Yaw = FMath::FRandRange(0.0f, 360.0f);
		const float Distance = FMath::Max(MinDistanceFromPlayer, FMath::FRandRange(FallbackSpawnDistance.X, FallbackSpawnDistance.Y));
		FVector Location = PlayerLocation + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance;
		FNavLocation Projected;
		if (Nav && Nav->ProjectPointToNavigation(Location, Projected, FVector(500.0, 500.0, 1000.0)))
		{
			Location = Projected.Location;
		}
		FVector Normal = FVector::UpVector;
		Location = Ground(Location, &Normal);
		// Not on a hillside, and not far above or below us (a roof, a cliff top, a pit).
		const bool bLevel = Normal.Z >= 0.94 && FMath::Abs(Location.Z - PlayerGroundZ) <= 0.25 * Distance;
		if (bNeedSight)
		{
			FHitResult Blocker;
			FCollisionQueryParams Sight(SCENE_QUERY_STAT(ArenaSpawnSight), false, Player.Get());
			if (GetWorld()->LineTraceSingleByObjectType(Blocker, Location + FVector(0, 0, 200), PlayerLocation + FVector(0, 0, 100), FCollisionObjectQueryParams(ECC_WorldStatic), Sight))
			{
				continue;
			}
		}
		if (bNeedHidden && PlayerCanSee(Location + FVector(0, 0, 150)))
		{
			continue;
		}
		if (bLevel && FVector::Dist2D(Location, PlayerLocation) >= MinDistanceFromPlayer && IsSpotFree(Location, 300.0f))
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

bool ATankArenaDirector::SpawnNext()
{
	FTransform Transform;
	if (Queue.Num() == 0 || !FindSpawnTransform(Queue[0], Transform))
	{
		return false;
	}
	SpawnEnemy(Queue[0], Transform);
	Queue.RemoveAt(0);
	return true;
}

bool ATankArenaDirector::PlayerCanSee(FVector Point) const
{
	const UTankVisionConeComponent* Vision = Player.IsValid() ? Player->GetVisionCone() : nullptr;
	return Vision && Vision->CanSee(Point);
}

FTransform ATankArenaDirector::GetHiddenSpawnTransform(ETankEnemyType Type, bool& bOutOfSight)
{
	BindPlayer();
	FTransform Out;
	if (!FindSpawnTransform(Type, Out))
	{
		// Nowhere free: at least put it behind the player.
		FVector Eye = GetActorLocation();
		FVector Looking = FVector::ForwardVector;
		if (const UTankVisionConeComponent* Vision = Player.IsValid() ? Player->GetVisionCone() : nullptr)
		{
			Vision->GetView(Eye, Looking);
		}
		Out = FTransform((Looking).Rotation(), Eye - Looking * FallbackSpawnDistance.X);
	}
	bOutOfSight = !PlayerCanSee(Out.GetLocation());
	return Out;
}

FTransform ATankArenaDirector::GetHiddenSpawnTransformByIndex(int32 TypeIndex, bool& bOutOfSight)
{
	return GetHiddenSpawnTransform(static_cast<ETankEnemyType>(FMath::Clamp(TypeIndex, 0, 2)), bOutOfSight);
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
	if (bExternalWaves && Phase == ETankArenaPhase::Fighting)
	{
		return FString();
	}
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
	const float Next = bExternalWaves ? GetTimeToNextWave() : -1.0f;
	if (Next >= 0.0f)
	{
		const int32 Seconds = FMath::CeilToInt(Next);
		return FString::Printf(TEXT("WAVE %d  NEXT %d:%02d  ENEMY %d  SCORE %d"), Wave, Seconds / 60, Seconds % 60, GetEnemiesLeft(), Score);
	}
	return FString::Printf(TEXT("WAVE %d  ENEMY %d  SCORE %d"), Wave, GetEnemiesLeft(), Score);
}

void ATankArenaDirector::BringBackLostEnemies(float DeltaSeconds)
{
	if (LostEnemyTimeout <= 0.0f)
	{
		return;
	}
	for (auto It = LostChecks.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	for (const TWeakObjectPtr<ATankEnemy>& Entry : Enemies)
	{
		ATankEnemy* Enemy = Entry.Get();
		if (!Enemy || Enemy->IsWrecked())
		{
			continue;
		}
		FLostCheck& Check = LostChecks.FindOrAdd(Enemy);
		const FVector Location = Enemy->GetActorLocation();
		if (Enemy->GetWeapon()->HasLineOfSight())
		{
			Check = FLostCheck();
			continue;
		}
		FTransform Transform;
		if (Enemy->EnemyType == ETankEnemyType::AntiTank)
		{
			// AT guns sit still, so one dug in where it never sees us would hold the wave up: it moves to a new spot.
			Check.Hidden += DeltaSeconds;
			if (Check.Hidden > LostEnemyTimeout * 1.5f && FindSpawnTransform(Enemy->EnemyType, Transform, true))
			{
				UE_LOG(LogTankGame, Log, TEXT("%s never saw the player, moved to a new spot"), *Enemy->GetName());
				Enemy->GetDrive()->Teleport(Transform.GetLocation(), Transform.Rotator());
				Check = FLostCheck();
			}
			continue;
		}
		if (Check.Time <= 0.0f || FVector::Dist2D(Location, Check.Anchor) > 300.0)
		{
			Check.Anchor = Location;
			Check.Time = KINDA_SMALL_NUMBER;
		}
		Check.Time += DeltaSeconds;
		Check.Hidden += DeltaSeconds;
		// Standing still behind something, or driving around without ever finding us.
		if ((Check.Time > LostEnemyTimeout || Check.Hidden > LostEnemyTimeout * 2.0f) && FindSpawnTransform(Enemy->EnemyType, Transform, true))
		{
			UE_LOG(LogTankGame, Log, TEXT("%s could not reach the player, brought back in"), *Enemy->GetName());
			Enemy->GetDrive()->Teleport(Transform.GetLocation(), Transform.Rotator());
			Check = FLostCheck();
		}
	}
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
			if (GroupLeft <= 0)
			{
				GroupLeft = FMath::RandRange(FMath::Max(1, SpawnGroupSize.X), FMath::Max(1, FMath::Max(SpawnGroupSize.X, SpawnGroupSize.Y)));
			}
			if (SpawnNext())
			{
				--GroupLeft;
			}
			// One or two come in together, then a pause before the next ones.
			SpawnTimer = GroupLeft > 0 ? SpawnInterval : TimeBetweenGroups;
		}
		BringBackLostEnemies(DeltaSeconds);
		if (bExternalWaves)
		{
			break;
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
