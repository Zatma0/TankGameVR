#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Tests/AutomationCommon.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Targeting/TargetableComponent.h"
#include "UObject/StrongObjectPtr.h"

namespace TankTests
{
	constexpr EAutomationTestFlags TankTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	inline UClass* LoadBlueprintClass(const FString& AssetPath)
	{
		return StaticLoadClass(UObject::StaticClass(), nullptr, *(AssetPath + TEXT(".") + FPaths::GetBaseFilename(AssetPath) + TEXT("_C")));
	}

	/** Game world with game instance, game mode and a large floor whose top is at z = 0. */
	struct FWorld
	{
		FTestWorldWrapper Wrapper;
		UWorld* World = nullptr;

		FWorld()
		{
			Wrapper.CreateTestWorld(EWorldType::Game);
			Wrapper.BeginPlayInTestWorld();
			World = Wrapper.GetTestWorld();
			SpawnBlock(FVector(0, 0, -50), FVector(400, 400, 1))->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
		}

		~FWorld()
		{
			Wrapper.DestroyTestWorld(false);
		}

		AStaticMeshActor* SpawnBlock(const FVector& Center, const FVector& Scale) const
		{
			AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
			UStaticMeshComponent* Mesh = Block->GetStaticMeshComponent();
			Mesh->SetMobility(EComponentMobility::Movable);
			Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Block->SetActorScale3D(Scale);
			return Block;
		}

		/** Part of the level (buildings, terrain), not something that drove up. */
		AStaticMeshActor* SpawnStaticBlock(const FVector& Center, const FVector& Scale) const
		{
			AStaticMeshActor* Block = SpawnBlock(Center, Scale);
			Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			return Block;
		}

		/** A slope rising along +X from Foot. */
		AStaticMeshActor* SpawnRamp(const FVector& Foot, float Angle, float Length = 4000.0f, float Width = 1200.0f) const
		{
			const FRotator Rotation(Angle, 0.0f, 0.0f);
			// 20 cm slab with its top edge at Foot.
			AStaticMeshActor* Ramp = SpawnBlock(Foot + Rotation.RotateVector(FVector(Length * 0.5, 0.0, -10.0)), FVector(Length / 100.0, Width / 100.0, 0.2));
			Ramp->SetActorRotation(Rotation);
			Ramp->GetStaticMeshComponent()->SetMobility(EComponentMobility::Static);
			return Ramp;
		}

		void Tick(float Seconds, float Dt = 1.0f / 90.0f)
		{
			for (float Time = 0.0f; Time < Seconds - 1e-4f; Time += Dt)
			{
				Wrapper.TickTestWorld(Dt);
			}
		}

		/** A bare targetable actor (no mesh, no collision) at Location. */
		AActor* SpawnTarget(const FVector& Location, const TCHAR* Name) const
		{
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Location));
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Root->SetWorldLocation(Location);
			UTargetableComponent* Targetable = NewObject<UTargetableComponent>(Actor, TEXT("Targetable"));
			Targetable->TargetPointOffset = FVector::ZeroVector;
			Targetable->DisplayName = FText::FromString(Name);
			// Registering on an actor that has begun play runs the component's BeginPlay (target registration).
			Targetable->RegisterComponent();
			return Actor;
		}
	};
}

#endif
