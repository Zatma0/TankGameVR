#include "Game/TankLevelSetupSubsystem.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/TankEnemy.h"
#include "Game/TankArenaDirector.h"
#include "Tank/TankVehicle.h"
#include "TankGameVR.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
	TAutoConsoleVariable<int32> CVarLevelSetup(TEXT("Tank.LevelSetup"), 1,
		TEXT("0: do not add a director, navigation, blockers or real enemies for placeholder ones at run time."));

	const TCHAR* const DirectorClassPath = TEXT("/Game/TankGame/Blueprints/Game/BP_TankArenaDirector.BP_TankArenaDirector_C");

	// Ugurkan's Blueprints. Only looked up by name, so a checkout without them just skips that part.
	const TCHAR* const PlaceholderEnemyPackage = TEXT("/Game/TankGame/Ugurkan/BPs/Gameplay/BP_EnemyBase");

	UClass* LoadBlueprintClassIfThere(const FString& Package)
	{
		if (!FPackageName::DoesPackageExist(Package))
		{
			return nullptr;
		}
		return LoadObject<UClass>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package) + TEXT("_C")));
	}

	/** Value of a byte or enum variable, whichever way the Blueprint declared it. */
	int32 ReadIndex(const UObject* Object, FName PropertyName)
	{
		const FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
		if (const FEnumProperty* Enum = CastField<FEnumProperty>(Property))
		{
			return static_cast<int32>(Enum->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Object)));
		}
		if (const FNumericProperty* Number = CastField<FNumericProperty>(Property))
		{
			return static_cast<int32>(Number->GetSignedIntPropertyValue(Number->ContainerPtrToValuePtr<void>(Object)));
		}
		return 0;
	}

	/** Fires a Blueprint event dispatcher by name; object inputs get the actor itself. */
	void CallDispatcher(AActor* Actor, FName EventName)
	{
		const FMulticastDelegateProperty* Event = FindFProperty<FMulticastDelegateProperty>(Actor->GetClass(), EventName);
		const FMulticastScriptDelegate* Delegate = Event ? Event->GetMulticastDelegate(Event->ContainerPtrToValuePtr<void>(Actor)) : nullptr;
		if (!Delegate || !Event->SignatureFunction)
		{
			return;
		}
		FStructOnScope Params(Event->SignatureFunction);
		for (TFieldIterator<FObjectPropertyBase> It(Event->SignatureFunction); It; ++It)
		{
			if (Actor->IsA(It->PropertyClass))
			{
				It->SetObjectPropertyValue_InContainer(Params.GetStructMemory(), Actor);
			}
		}
		Delegate->ProcessDelegate<UObject>(Params.GetStructMemory());
	}
}

bool UTankLevelSetupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTankLevelSetupSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTankLevelSetupSubsystem, STATGROUP_Tickables);
}

void UTankLevelSetupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (CVarLevelSetup.GetValueOnGameThread() == 0)
	{
		return;
	}
	if (!PlaceholderClass)
	{
		PlaceholderClass = LoadBlueprintClassIfThere(PlaceholderEnemyPackage);
	}
	WatchForPlaceholders();
	// Only levels that are meant for the tank.
	for (TActorIterator<ATankVehicle> It(&InWorld); It; ++It)
	{
		SetUpLevel();
		break;
	}
}

void UTankLevelSetupSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld(); World && SpawnHandle.IsValid())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	SpawnHandle.Reset();
	Super::Deinitialize();
}

void UTankLevelSetupSubsystem::SetUpLevel()
{
	UWorld* World = GetWorld();
	if (bSetUp || !World)
	{
		return;
	}
	bSetUp = true;
	// Our own waves to begin with. A spawner of the level's own takes over once it really sends an enemy.
	ATankArenaDirector* Director = GetOrCreateDirector(false);
	if (!Director)
	{
		return;
	}
	if (Director->bBlockMeshesWithoutCollision)
	{
		BlockMeshesWithoutCollision();
	}
	if (Director->bCreateNavigationIfMissing)
	{
		FVector Center = Director->GetActorLocation();
		for (TActorIterator<ATankVehicle> It(World); It; ++It)
		{
			Center = It->GetActorLocation();
			break;
		}
		CreateNavigationIfMissing(Center, Director->AutoNavigationSize);
	}
}

ATankArenaDirector* UTankLevelSetupSubsystem::GetOrCreateDirector(bool bExternalWaves)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	ATankArenaDirector* Director = ATankArenaDirector::GetDirector(World);
	if (!Director)
	{
		UClass* Class = LoadObject<UClass>(nullptr, DirectorClassPath);
		if (!Class || !Class->IsChildOf(ATankArenaDirector::StaticClass()))
		{
			Class = ATankArenaDirector::StaticClass();
		}
		FTransform Where = FTransform::Identity;
		for (TActorIterator<ATankVehicle> It(World); It; ++It)
		{
			Where.SetLocation(It->GetActorLocation());
			break;
		}
		Director = World->SpawnActorDeferred<ATankArenaDirector>(Class, Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Director)
		{
			return nullptr;
		}
		Director->SetExternalWaves(bExternalWaves);
		Director->FinishSpawning(Where);
		UE_LOG(LogTankGame, Log, TEXT("%s has no arena director, added one%s"), *World->GetMapName(), bExternalWaves ? TEXT(" (the level's own spawner decides the waves)") : TEXT(""));
	}
	else if (bExternalWaves && !Director->HasExternalWaves())
	{
		Director->SetExternalWaves(true);
	}
	return Director;
}

bool UTankLevelSetupSubsystem::CreateNavigationIfMissing(FVector Center, FVector Size)
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav || Size.GetMin() <= 0.0)
	{
		return false;
	}
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		// The level brings its own.
		return false;
	}
	ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(Center, FRotator::ZeroRotator);
	UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
	if (!Brush)
	{
		return false;
	}
	// A volume made at run time has no brush shape; its bounds come from this box instead.
	UBodySetup* Shape = NewObject<UBodySetup>(Brush);
	Shape->AggGeom.BoxElems.Add(FKBoxElem(Size.X, Size.Y, Size.Z));
	Brush->BrushBodySetup = Shape;
	Brush->UpdateBounds();
	Nav->OnNavigationBoundsUpdated(Volume);
	UE_LOG(LogTankGame, Log, TEXT("%s has no NavMeshBoundsVolume, building navigation for %.0f x %.0f m around the player"),
		*World->GetMapName(), Size.X / 100.0, Size.Y / 100.0);
	return true;
}

int32 UTankLevelSetupSubsystem::BlockMeshesWithoutCollision(float MinSize)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	TArray<FVector> Tanks;
	for (TActorIterator<ATankVehicle> It(World); It; ++It)
	{
		Tanks.Add(It->GetActorLocation());
	}
	int32 Added = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (*It == BlockerHolder)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(*It);
		for (const UStaticMeshComponent* Mesh : Meshes)
		{
			const UStaticMesh* Asset = Mesh->GetStaticMesh();
			const UBodySetup* Body = Asset ? Asset->GetBodySetup() : nullptr;
			// Solid level geometry that vehicles and shells would pass straight through.
			if (!Body || BlockedMeshes.Contains(Mesh) || Mesh->Mobility == EComponentMobility::Movable || Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision
				|| Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic) != ECR_Block
				|| Body->AggGeom.GetElementCount() > 0 || Body->CollisionTraceFlag == CTF_UseComplexAsSimple)
			{
				continue;
			}
			const FBox Local = Asset->GetBoundingBox();
			const FTransform MeshTransform = Mesh->GetComponentTransform();
			const FVector Size = Local.GetSize() * MeshTransform.GetScale3D().GetAbs();
			// Buildings, not props.
			if (Size.GetMin() < MinSize)
			{
				continue;
			}
			// Never box in a tank (a mesh that is the ground or a shell around the play area).
			const FBox Roomy = Local.ExpandBy(FVector(200.0) / MeshTransform.GetScale3D().GetAbs().ComponentMax(FVector(0.01)));
			const bool bHoldsTank = Tanks.ContainsByPredicate([&](const FVector& Tank) { return Roomy.IsInside(MeshTransform.InverseTransformPosition(Tank)); });
			if (bHoldsTank)
			{
				UE_LOG(LogTankGame, Log, TEXT("%s has no simple collision but the tank stands inside its bounds, left open"), *Asset->GetName());
				continue;
			}
			if (!BlockerHolder)
			{
				BlockerHolder = World->SpawnActor<AActor>();
				USceneComponent* Root = NewObject<USceneComponent>(BlockerHolder, TEXT("Root"));
				Root->SetMobility(EComponentMobility::Static);
				BlockerHolder->SetRootComponent(Root);
				Root->RegisterComponent();
			}
			UBoxComponent* Box = NewObject<UBoxComponent>(BlockerHolder);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetBoxExtent(Local.GetExtent(), false);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Box->SetCanEverAffectNavigation(true);
			Box->SetupAttachment(BlockerHolder->GetRootComponent());
			Box->SetRelativeTransform(FTransform(MeshTransform.GetRotation(), MeshTransform.TransformPosition(Local.GetCenter()), MeshTransform.GetScale3D()));
			Box->RegisterComponent();
			BlockedMeshes.Add(Mesh);
			++Added;
			UE_LOG(LogTankGame, Log, TEXT("%s (%s) has no simple collision, blocking its bounding box (%.0f x %.0f x %.0f m)"),
				*Asset->GetName(), *It->GetName(), Size.X / 100.0, Size.Y / 100.0, Size.Z / 100.0);
		}
	}
	Blockers += Added;
	return Added;
}

void UTankLevelSetupSubsystem::SetPlaceholderEnemyClass(UClass* EnemyClass, FName TypeProperty, FName DiedEvent)
{
	PlaceholderClass = EnemyClass;
	PlaceholderTypeProperty = TypeProperty;
	PlaceholderDiedEvent = DiedEvent;
	WatchForPlaceholders();
}

void UTankLevelSetupSubsystem::WatchForPlaceholders()
{
	UWorld* World = GetWorld();
	if (!World || !PlaceholderClass)
	{
		return;
	}
	if (!SpawnHandle.IsValid())
	{
		SpawnHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UTankLevelSetupSubsystem::HandleActorSpawned));
	}
	for (TActorIterator<AActor> It(World, PlaceholderClass); It; ++It)
	{
		HandleActorSpawned(*It);
	}
}

void UTankLevelSetupSubsystem::HandleActorSpawned(AActor* Actor)
{
	// Its variables are only set once it has finished spawning, so it is handled on the next tick.
	if (Actor && PlaceholderClass && Actor->IsA(PlaceholderClass) && !Actor->IsA<ATankEnemy>() && !GetReplacement(Actor))
	{
		Pending.AddUnique(Actor);
	}
}

ATankEnemy* UTankLevelSetupSubsystem::GetReplacement(const AActor* Placeholder) const
{
	for (const FSwap& Swap : Swaps)
	{
		if (Swap.Placeholder.Get() == Placeholder)
		{
			return Swap.Ours.Get();
		}
	}
	return nullptr;
}

void UTankLevelSetupSubsystem::Replace(AActor* Placeholder)
{
	const ATankArenaDirector* Before = ATankArenaDirector::GetDirector(GetWorld());
	const bool bHandOver = !Before || !Before->HasExternalWaves();
	ATankArenaDirector* Director = GetOrCreateDirector(true);
	if (!Director)
	{
		return;
	}
	if (bHandOver)
	{
		UE_LOG(LogTankGame, Log, TEXT("The level's own spawner has started sending enemies: the director leaves the waves to it"));
	}
	const ETankEnemyType Type = static_cast<ETankEnemyType>(FMath::Clamp(ReadIndex(Placeholder, PlaceholderTypeProperty), 0, 2));
	const FTransform Where = Director->FindEnemySpotNear(Placeholder->GetActorLocation(), Type);
	ATankEnemy* Ours = Director->SpawnEnemy(Type, Where);
	if (!Ours)
	{
		return;
	}
	// The placeholder stays as a hidden stand-in, so whatever its spawner tracks still exists.
	Placeholder->SetActorHiddenInGame(true);
	Placeholder->SetActorEnableCollision(false);
	Placeholder->SetActorTickEnabled(false);
	TInlineComponentArray<UActorComponent*> Parts(Placeholder);
	for (UActorComponent* Part : Parts)
	{
		Part->SetComponentTickEnabled(false);
	}
	Placeholder->AttachToActor(Ours, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	FSwap& Swap = Swaps.AddDefaulted_GetRef();
	Swap.Placeholder = Placeholder;
	Swap.Ours = Ours;
	Ours->GetHealth()->OnDied.AddUniqueDynamic(this, &UTankLevelSetupSubsystem::HandleOursDied);
	Ours->OnDestroyed.AddUniqueDynamic(this, &UTankLevelSetupSubsystem::HandleActorDestroyed);
	Placeholder->OnDestroyed.AddUniqueDynamic(this, &UTankLevelSetupSubsystem::HandleActorDestroyed);
	++Replaced;
	UE_LOG(LogTankGame, Log, TEXT("%s from the level's spawner is now %s at %s"), *Placeholder->GetName(), *Ours->GetName(), *Where.GetLocation().ToCompactString());
}

void UTankLevelSetupSubsystem::HandleOursDied(UTankHealthComponent* HealthComponent, AActor* Killer)
{
	const AActor* Dead = HealthComponent ? HealthComponent->GetOwner() : nullptr;
	for (int32 Index = Swaps.Num() - 1; Index >= 0; --Index)
	{
		if (Swaps[Index].Ours.Get() != Dead)
		{
			continue;
		}
		AActor* Placeholder = Swaps[Index].Placeholder.Get();
		Swaps.RemoveAt(Index);
		if (Placeholder)
		{
			// Tell its spawner the way its own enemy would have.
			CallDispatcher(Placeholder, PlaceholderDiedEvent);
			Placeholder->Destroy();
		}
	}
}

void UTankLevelSetupSubsystem::HandleActorDestroyed(AActor* Actor)
{
	for (int32 Index = Swaps.Num() - 1; Index >= 0; --Index)
	{
		const FSwap Swap = Swaps[Index];
		if (Swap.Ours.Get() == Actor)
		{
			// Ours was removed without being killed (restart): take the stand-in along.
			Swaps.RemoveAt(Index);
			if (AActor* Placeholder = Swap.Placeholder.Get())
			{
				Placeholder->Destroy();
			}
		}
		else if (Swap.Placeholder.Get() == Actor)
		{
			// The spawner removed its enemy: ours goes too, unless it is already a wreck.
			Swaps.RemoveAt(Index);
			if (ATankEnemy* Ours = Swap.Ours.Get(); Ours && !Ours->IsWrecked())
			{
				Ours->Destroy();
			}
		}
	}
}

void UTankLevelSetupSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (CVarLevelSetup.GetValueOnGameThread() == 0)
	{
		return;
	}
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		AActor* Placeholder = Pending[Index].Get();
		if (!Placeholder)
		{
			Pending.RemoveAt(Index);
		}
		else if (Placeholder->HasActorBegunPlay())
		{
			Pending.RemoveAt(Index);
			Replace(Placeholder);
		}
	}
}
