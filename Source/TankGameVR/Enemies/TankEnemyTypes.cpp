#include "Enemies/TankEnemyTypes.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/EnemyDriveComponent.h"
#include "Enemies/EnemyWeaponComponent.h"
#include "Targeting/TargetableComponent.h"
#include "Components/BoxComponent.h"

// ---------------------------------------------------------------------------------------------------------------
// Tank

ATankEnemy_Tank::ATankEnemy_Tank()
{
	EnemyType = ETankEnemyType::Tank;
	KillScore = 100;
	Health->DamageMultipliers.Add(UTankDamageType_HE::StaticClass(), 0.5f);
}

void ATankEnemy_Tank::ApplyStats()
{
	Super::ApplyStats();
	Drive->TurnRate = 32.0f;
	Drive->bCanPivot = true;
	Drive->Acceleration = 150.0f;
	Drive->Braking = 400.0f;
	Drive->HalfWidth = 160.0f;

	Weapon->DamageType = UTankDamageType_AP::StaticClass();
	Weapon->ProjectileSpeed = 7000.0f;
	Weapon->TracerScale = 1.2f;
	Weapon->ReloadTime = 6.0f;
	Weapon->AimTime = 1.0f;
	Weapon->SpreadStart = 3.0f;
	// Firing on the move is much less accurate than from the halt.
	Weapon->MovingSpreadMultiplier = 2.5f;
	Weapon->SpreadMin = 0.5f;
	Weapon->SettleTime = 3.0f;
	Weapon->TraverseSpeed = 24.0f;
	Weapon->ElevationSpeed = 8.0f;
	FlankSide = FMath::RandBool() ? 1.0f : -1.0f;
}

void ATankEnemy_Tank::PickNewPosition(float MinAngle, float MaxAngle)
{
	FlankSide = FMath::RandBool() ? 1.0f : -1.0f;
	Position = PointAroundPlayer(GetEngageRange() * FMath::FRandRange(0.7f, 0.95f), FlankSide * FMath::FRandRange(MinAngle, MaxAngle));
	SetState(ETankEnemyState::Repositioning);
}

void ATankEnemy_Tank::HandleHit(float Damage)
{
	// Taking hits at a halt: back off to a new spot (half the time).
	if (GetEnemyState() == ETankEnemyState::Engaging && GetStateTime() > 1.0f && FMath::RandBool())
	{
		PickNewPosition(30.0f, 60.0f);
	}
}

void ATankEnemy_Tank::UpdateBehaviour(float DeltaSeconds)
{
	const float Range = GetEngageRange();
	const float Distance = GetDistanceToPlayer();
	const bool bSight = Weapon->HasLineOfSight();

	switch (GetEnemyState())
	{
	case ETankEnemyState::Engaging:
		// Short halt: hull towards the threat, fire, then move on.
		Drive->Stop();
		Drive->FaceTowards(Player->GetActorLocation());
		Weapon->SetWeaponsFree(true);
		LostSightTime = bSight ? 0.0f : LostSightTime + DeltaSeconds;
		if (Weapon->GetShotsFired() - HaltStartShots >= ShotsPerHalt || GetStateTime() > MaxHaltTime || LostSightTime > 2.5f)
		{
			PickNewPosition(25.0f, 55.0f);
		}
		else if (Distance > Range * 1.4f)
		{
			SetState(ETankEnemyState::Advancing);
		}
		break;

	case ETankEnemyState::Repositioning:
		Weapon->SetWeaponsFree(false);
		Drive->MoveTo(Position, 300.0f, 1.0f);
		if (Drive->HasArrived() || GetStateTime() > 12.0f)
		{
			SetState(Distance <= Range * 1.2f ? ETankEnemyState::Engaging : ETankEnemyState::Advancing);
			HaltStartShots = Weapon->GetShotsFired();
		}
		break;

	default:
		// Advancing (also from Holding): close in from one flank, firing on the move once in range (less accurate).
		Weapon->SetWeaponsFree(Distance <= GetFireRange());
		if (Position.IsZero() || FMath::Fmod(GetStateTime(), 3.0f) < DeltaSeconds)
		{
			Position = PointAroundPlayer(Range * 0.8f, FlankSide * FMath::FRandRange(15.0f, 35.0f));
		}
		Drive->MoveTo(Position, 300.0f, 1.0f);
		if ((Distance <= Range && bSight) || Drive->HasArrived())
		{
			SetState(ETankEnemyState::Engaging);
			HaltStartShots = Weapon->GetShotsFired();
			LostSightTime = 0.0f;
		}
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Armored car

ATankEnemy_ArmoredCar::ATankEnemy_ArmoredCar()
{
	EnemyType = ETankEnemyType::ArmoredCar;
	KillScore = 60;
	Health->DamageMultipliers.Add(UTankDamageType_AP::StaticClass(), 0.6f);
	Health->DamageMultipliers.Add(UTankDamageType_HE::StaticClass(), 1.5f);
	FireRangeScale = 2.2f;
	HullCollision->SetBoxExtent(FVector(230.0, 125.0, 55.0));
	Drive->RideHeight = 80.0f;
	TurretPivot->SetRelativeLocation(FVector(-20.0, 0.0, 60.0));
	BarrelPivot->SetRelativeLocation(FVector(45.0, 0.0, 20.0));
	Muzzle->SetRelativeLocation(FVector(150.0, 0.0, 0.0));
}

void ATankEnemy_ArmoredCar::ApplyStats()
{
	Super::ApplyStats();
	Drive->TurnRate = 75.0f;
	Drive->bCanPivot = false;
	Drive->Acceleration = 450.0f;
	Drive->Braking = 700.0f;
	Drive->HalfWidth = 125.0f;

	// The table damage is for a whole burst.
	Weapon->DamageType = UTankDamageType_Autocannon::StaticClass();
	Weapon->BurstCount = 4;
	Weapon->BurstInterval = 0.16f;
	Weapon->Damage = Stats.Damage / Weapon->BurstCount;
	Weapon->ProjectileSpeed = 11000.0f;
	Weapon->TracerScale = 0.45f;
	Weapon->ReloadTime = 3.0f;
	Weapon->AimTime = 0.4f;
	Weapon->OnTargetTolerance = 3.0f;
	Weapon->SpreadStart = 3.5f;
	Weapon->SpreadMin = 1.5f;
	Weapon->MovingSpreadMultiplier = 2.2f;
	Weapon->TraverseSpeed = 80.0f;
	Weapon->ElevationSpeed = 30.0f;
	Direction = FMath::RandBool() ? 1.0f : -1.0f;
}

void ATankEnemy_ArmoredCar::PickRun()
{
	++Runs;
	if (Runs % 4 == 0)
	{
		Direction = -Direction;
	}
	float Radius = GetEngageRange() * FMath::FRandRange(OrbitDistance.X, OrbitDistance.Y);
	if (Runs % 3 == 0)
	{
		// Now and then a closer, cheekier pass.
		Radius *= 0.8f;
	}
	// Short hops around the player: the chord never cuts in much closer than the radius.
	const FVector Goal = PointAroundPlayer(Radius, Direction * FMath::FRandRange(50.0f, 80.0f));
	Drive->MoveTo(Goal, 450.0f, OrbitSpeed);
	// Flip between the two moving states to restart the run timer.
	SetState(GetEnemyState() == ETankEnemyState::Advancing ? ETankEnemyState::Repositioning : ETankEnemyState::Advancing);
}

void ATankEnemy_ArmoredCar::HandleHit(float Damage)
{
	// Jink the other way.
	Direction = -Direction;
	PickRun();
}

void ATankEnemy_ArmoredCar::UpdateBehaviour(float DeltaSeconds)
{
	if (GetEnemyState() == ETankEnemyState::Engaging)
	{
		// Short stop for an aimed burst - the moment to hit it.
		Drive->Stop();
		Weapon->SetWeaponsFree(true);
		if (GetStateTime() > StopTime)
		{
			PickRun();
		}
		return;
	}
	if (!Drive->HasGoal() || Drive->HasArrived() || GetStateTime() > 8.0f || GetEnemyState() == ETankEnemyState::Holding)
	{
		if (Drive->HasArrived() && Runs % 2 == 0 && GetDistanceToPlayer() <= GetFireRange())
		{
			SetState(ETankEnemyState::Engaging);
		}
		else
		{
			PickRun();
		}
	}
	Weapon->SetWeaponsFree(GetDistanceToPlayer() <= GetFireRange());
}

// ---------------------------------------------------------------------------------------------------------------
// Anti-tank gun

ATankEnemy_AntiTank::ATankEnemy_AntiTank()
{
	EnemyType = ETankEnemyType::AntiTank;
	// Long-range sniper: digs in well outside the table range if it can see us.
	FireRangeScale = 1.8f;
	KillScore = 150;
	Health->DamageMultipliers.Add(UTankDamageType_AP::StaticClass(), 0.5f);
	Health->DamageMultipliers.Add(UTankDamageType_HE::StaticClass(), 1.5f);
	HullCollision->SetBoxExtent(FVector(160.0, 110.0, 50.0));
	Drive->RideHeight = 55.0f;
	TurretPivot->SetRelativeLocation(FVector(0.0, 0.0, 30.0));
	BarrelPivot->SetRelativeLocation(FVector(0.0, 0.0, 25.0));
	Muzzle->SetRelativeLocation(FVector(300.0, 0.0, 0.0));
	Targetable->TargetPointOffset = FVector(0.0, 0.0, 10.0);
}

void ATankEnemy_AntiTank::ApplyStats()
{
	Super::ApplyStats();
	Drive->TurnRate = 28.0f;
	Drive->bCanPivot = true;
	Drive->Acceleration = 200.0f;
	Drive->Braking = 500.0f;
	Drive->HalfWidth = 110.0f;
	Drive->SeparationRadius = 700.0f;

	Weapon->DamageType = UTankDamageType_AP::StaticClass();
	Weapon->BurstCount = 1;
	Weapon->ProjectileSpeed = 14000.0f;
	Weapon->TracerScale = 1.4f;
	Weapon->ReloadTime = 6.5f;
	Weapon->AimTime = 2.6f;
	Weapon->bResetAimWhenSightLost = true;
	Weapon->OnTargetTolerance = 1.2f;
	Weapon->SpreadStart = 0.5f;
	Weapon->SpreadMin = 0.25f;
	Weapon->MovingSpreadMultiplier = 3.0f;
	Weapon->TraverseLimit = 28.0f;
	Weapon->TraverseSpeed = 18.0f;
	Weapon->ElevationSpeed = 8.0f;
}

void ATankEnemy_AntiTank::Relocate()
{
	const float Side = FMath::RandBool() ? 1.0f : -1.0f;
	Drive->MoveTo(PointAroundPlayer(GetEngageRange() * FMath::FRandRange(1.1f, 1.5f), Side * FMath::FRandRange(35.0f, 70.0f)), 250.0f, 1.0f);
	bHitWhileDeployed = false;
	SetState(ETankEnemyState::Repositioning);
}

void ATankEnemy_AntiTank::HandleHit(float Damage)
{
	if (GetEnemyState() == ETankEnemyState::Engaging || GetEnemyState() == ETankEnemyState::Deploying)
	{
		bHitWhileDeployed = true;
	}
}

void ATankEnemy_AntiTank::UpdateBehaviour(float DeltaSeconds)
{
	const float Range = GetEngageRange();
	const float Distance = GetDistanceToPlayer();

	switch (GetEnemyState())
	{
	case ETankEnemyState::Deploying:
		// Trails down, gun swung towards the target; no shot yet.
		Drive->Stop();
		Drive->FaceTowards(Player->GetActorLocation());
		Weapon->SetWeaponsFree(false);
		if (GetStateTime() >= DeployTime)
		{
			SetState(ETankEnemyState::Engaging);
			PositionStartShots = Weapon->GetShotsFired();
		}
		break;

	case ETankEnemyState::Engaging:
		Drive->Stop();
		Drive->FaceTowards(Player->GetActorLocation());
		Weapon->SetWeaponsFree(true);
		if ((Weapon->GetShotsFired() - PositionStartShots >= ShotsPerPosition && Weapon->IsReloading())
			|| (bHitWhileDeployed && GetStateTime() > 0.5f)
			|| Distance > GetFireRange() * 1.1f
			|| (!Weapon->HasLineOfSight() && GetStateTime() > 5.0f && Weapon->GetAimProgress() <= 0.0f))
		{
			Relocate();
		}
		break;

	case ETankEnemyState::Repositioning:
		Weapon->SetWeaponsFree(false);
		// Only dig in where the gunner can see us; a blind spot means moving on.
		if (Distance <= GetFireRange() && Weapon->HasLineOfSight() && GetStateTime() > 2.0f)
		{
			Drive->Stop();
			SetState(ETankEnemyState::Deploying);
		}
		else if (Drive->HasArrived() || GetStateTime() > 12.0f)
		{
			Relocate();
		}
		break;

	default:
		// First move: to a spot at the edge of its range.
		Weapon->SetWeaponsFree(false);
		if (!Drive->HasGoal())
		{
			Drive->MoveTo(PointAroundPlayer(Range * FMath::FRandRange(1.1f, 1.5f), FMath::FRandRange(-40.0f, 40.0f)), 250.0f, 1.0f);
		}
		if (Distance <= GetFireRange() && Weapon->HasLineOfSight() && GetStateTime() > 1.5f)
		{
			Drive->Stop();
			SetState(ETankEnemyState::Deploying);
		}
		else if (Drive->HasArrived() || GetStateTime() > 15.0f)
		{
			Relocate();
		}
		break;
	}
}
