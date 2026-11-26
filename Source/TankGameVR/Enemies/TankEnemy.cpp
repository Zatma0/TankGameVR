#include "Enemies/TankEnemy.h"
#include "Audio/TankEnemyAudioComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/EnemyWeaponComponent.h"
#include "Tank/TankVehicle.h"
#include "Targeting/TargetableComponent.h"
#include "TankGameVR.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "NavigationSystem.h"

namespace
{
	// Ugurkan's enemy table: where it is now and where it was before he moved it.
	const TCHAR* const EnemyTablePackages[] =
	{
		TEXT("/Game/TankGame/Ugurkan/BPs/Gameplay/EnemyInfos/DT_EnemyFeatures"),
		TEXT("/Game/TankGame/Ugurkan/BPs/EnemyInfos/DT_EnemyFeatures"),
	};

	UDataTable* LoadTableIfThere(const FString& Package)
	{
		if (Package.IsEmpty() || !FPackageName::DoesPackageExist(Package))
		{
			return nullptr;
		}
		return LoadObject<UDataTable>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package)));
	}
}

ATankEnemy::ATankEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	HullCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HullCollision"));
	HullCollision->SetBoxExtent(FVector(270.0, 160.0, 60.0));
	HullCollision->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	HullCollision->SetMobility(EComponentMobility::Movable);
	HullCollision->SetCanEverAffectNavigation(false);
	SetRootComponent(HullCollision);

	TurretPivot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretPivot"));
	TurretPivot->SetupAttachment(HullCollision);
	TurretPivot->SetRelativeLocation(FVector(-30.0, 0.0, 80.0));

	BarrelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BarrelPivot"));
	BarrelPivot->SetupAttachment(TurretPivot);
	BarrelPivot->SetRelativeLocation(FVector(90.0, 0.0, 10.0));

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(BarrelPivot);
	Muzzle->SetRelativeLocation(FVector(320.0, 0.0, 0.0));

	TableModel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TableModel"));
	TableModel->SetupAttachment(HullCollision);
	TableModel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TableModel->SetHiddenInGame(true);
	TableModel->SetCanEverAffectNavigation(false);

	Targetable = CreateDefaultSubobject<UTargetableComponent>(TEXT("Targetable"));
	Targetable->TargetPointOffset = FVector(0.0, 0.0, 20.0);

	Health = CreateDefaultSubobject<UTankHealthComponent>(TEXT("Health"));
	Health->bDestroyOwnerOnDeath = false;

	Drive = CreateDefaultSubobject<UEnemyDriveComponent>(TEXT("Drive"));
	Sounds = CreateDefaultSubobject<UTankEnemyAudioComponent>(TEXT("Sounds"));
	Drive->RideHeight = 70.0f;

	Weapon = CreateDefaultSubobject<UEnemyWeaponComponent>(TEXT("Weapon"));
	Weapon->FriendlyClass = ATankEnemy::StaticClass();

	StatsTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(FString(EnemyTablePackages[0]) + TEXT(".DT_EnemyFeatures")));
	WreckMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Enemy_Wreck.MI_Enemy_Wreck")));
}

void ATankEnemy::BeginPlay()
{
	// Stats first, so the components start with the right HP and speed.
	LoadStats();
	Super::BeginPlay();

	Health->OnHealthChanged.AddDynamic(this, &ATankEnemy::HandleDamaged);
	Health->OnDied.AddDynamic(this, &ATankEnemy::HandleDied);

	TArray<USceneComponent*> Components;
	GetComponents(Components);
	for (USceneComponent* Component : Components)
	{
		// Moving vehicles must not cut holes in the nav mesh.
		Component->SetCanEverAffectNavigation(false);
		if (Component->GetName().StartsWith(WreckFireComponentName.ToString()))
		{
			Component->SetVisibility(false, true);
		}
	}
	if (Targetable->DisplayName.IsEmpty())
	{
		Targetable->DisplayName = GetTypeName();
	}
	FindPlayer();
}

UDataTable* ATankEnemy::FindDefaultStatsTable()
{
	for (const TCHAR* Package : EnemyTablePackages)
	{
		if (UDataTable* Table = LoadTableIfThere(Package))
		{
			return Table;
		}
	}
	return nullptr;
}

UDataTable* ATankEnemy::FindStatsTable() const
{
	UDataTable* Table = LoadTableIfThere(StatsTable.GetLongPackageName());
	return Table ? Table : FindDefaultStatsTable();
}

void ATankEnemy::LoadStats()
{
	if (const UDataTable* Table = FindStatsTable())
	{
		const FName Row = StatsRow.IsNone() ? UTankEnemyStatsLibrary::GetDefaultRowName(EnemyType) : StatsRow;
		bStatsLoaded = UTankEnemyStatsLibrary::ReadEnemyStats(Table, Row, Stats);
	}
	else
	{
		UE_LOG(LogTankGame, Warning, TEXT("%s: no enemy table, using its own stats"), *GetName());
	}
	ApplyStats();
	ShowTableModel();
}

void ATankEnemy::ApplyStats()
{
	Health->SetMaxHealth(Stats.Health);
	Drive->MaxSpeed = Stats.Speed;
	Weapon->Damage = Stats.Damage;
}

void ATankEnemy::ShowTableModel()
{
	// Engine shapes in the table are placeholders; our own placeholder meshes look more like vehicles.
	UStaticMesh* Model = Stats.Model;
	if (!Model || Model->GetPathName().StartsWith(TEXT("/Engine/")))
	{
		return;
	}
	TableModel->SetStaticMesh(Model);
	TableModel->SetHiddenInGame(false);
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		const FString Name = Mesh->GetName();
		if (Mesh != TableModel && !Name.StartsWith(TEXT("MuzzleFlash")) && !Name.StartsWith(TEXT("AimLaser")) && !Name.StartsWith(WreckFireComponentName.ToString()))
		{
			Mesh->SetHiddenInGame(true);
		}
	}
}

FText ATankEnemy::GetTypeName() const
{
	switch (EnemyType)
	{
	case ETankEnemyType::ArmoredCar:
		return FText::FromString(TEXT("ARMORED CAR"));
	case ETankEnemyType::AntiTank:
		return FText::FromString(TEXT("AT GUN"));
	default:
		return FText::FromString(TEXT("TANK"));
	}
}

void ATankEnemy::SetState(ETankEnemyState NewState)
{
	if (State != NewState && State != ETankEnemyState::Wrecked)
	{
		State = NewState;
		StateTime = 0.0f;
	}
}

void ATankEnemy::FindPlayer()
{
	AActor* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<ATankVehicle> It(GetWorld()); It; ++It)
	{
		const UTankHealthComponent* PlayerHealth = It->FindComponentByClass<UTankHealthComponent>();
		if (PlayerHealth && PlayerHealth->IsDead())
		{
			continue;
		}
		const double Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	Player = Best;
	Weapon->SetTarget(Best);
}

float ATankEnemy::GetDistanceToPlayer() const
{
	return Player.IsValid() ? static_cast<float>(FVector::Dist2D(Player->GetActorLocation(), GetActorLocation())) : TNumericLimits<float>::Max();
}

FVector ATankEnemy::PointAroundPlayer(float Distance, float AngleOffset) const
{
	if (!Player.IsValid())
	{
		return GetActorLocation();
	}
	const FVector Center = Player->GetActorLocation();
	FVector Away = (GetActorLocation() - Center).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	const FVector Point = Center + Away.RotateAngleAxis(AngleOffset, FVector::UpVector) * Distance;
	if (const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (Nav->ProjectPointToNavigation(Point, Projected, FVector(600.0, 600.0, 500.0)))
		{
			return Projected.Location;
		}
	}
	return Point;
}

float ATankEnemy::GetThreatTo(const AActor* Actor) const
{
	if (IsWrecked() || !Actor || Weapon->GetTarget() != Actor || !Weapon->IsWeaponsFree() || !Weapon->HasLineOfSight() || Weapon->IsReloading())
	{
		return 0.0f;
	}
	return Weapon->GetAimProgress();
}

void ATankEnemy::HandleDamaged(UTankHealthComponent* HealthComponent, float Damage, const UDamageType* DamageType)
{
	if (!HealthComponent->IsDead())
	{
		HandleHit(Damage);
	}
}

void ATankEnemy::HandleDied(UTankHealthComponent* HealthComponent, AActor* Killer)
{
	BecomeWreck();
}

void ATankEnemy::BecomeWreck()
{
	State = ETankEnemyState::Wrecked;
	StateTime = 0.0f;
	Drive->Freeze();
	Drive->SetComponentTickEnabled(false);
	Weapon->SetWeaponsFree(false);
	Weapon->SetTarget(nullptr);
	Weapon->SetComponentTickEnabled(false);
	// Gun droops.
	BarrelPivot->SetRelativeRotation(FRotator(-7.0f, BarrelPivot->GetRelativeRotation().Yaw, 0.0f));

	UMaterialInterface* Burnt = WreckMaterial.LoadSynchronous();
	TArray<USceneComponent*> Components;
	GetComponents(Components);
	for (USceneComponent* Component : Components)
	{
		const FString Name = Component->GetName();
		if (Name.StartsWith(WreckFireComponentName.ToString()))
		{
			Component->SetVisibility(true, true);
		}
		else if (Name.StartsWith(TEXT("AimLaser")) || Name.StartsWith(TEXT("MuzzleFlash")))
		{
			Component->SetVisibility(false, true);
		}
		else if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component); Mesh && Burnt)
		{
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Burnt);
			}
		}
	}
	SetLifeSpan(WreckTime);
	UE_LOG(LogTankGame, Log, TEXT("%s destroyed"), *GetName());
}

void ATankEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTime += DeltaSeconds;
	if (State == ETankEnemyState::Wrecked)
	{
		// Flickering fire on the wreck.
		TArray<UPointLightComponent*> Lights;
		GetComponents(Lights);
		for (UPointLightComponent* Light : Lights)
		{
			if (Light->GetName().StartsWith(WreckFireComponentName.ToString()))
			{
				Light->SetIntensity(FMath::FRandRange(25.0f, 60.0f) * FMath::Clamp((WreckTime - StateTime) / 3.0f, 0.0f, 1.0f));
			}
		}
		return;
	}

	PlayerSearchTimer -= DeltaSeconds;
	const UTankHealthComponent* PlayerHealth = Player.IsValid() ? Player->FindComponentByClass<UTankHealthComponent>() : nullptr;
	if (PlayerSearchTimer <= 0.0f || !Player.IsValid() || (PlayerHealth && PlayerHealth->IsDead()))
	{
		PlayerSearchTimer = 1.0f;
		FindPlayer();
	}
	if (!Player.IsValid())
	{
		// Nobody to fight: stop and wait.
		Drive->Stop();
		Weapon->SetWeaponsFree(false);
		SetState(ETankEnemyState::Holding);
		return;
	}
	UpdateBehaviour(DeltaSeconds);
}
