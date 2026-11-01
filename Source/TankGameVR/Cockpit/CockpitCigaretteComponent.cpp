#include "Cockpit/CockpitCigaretteComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UCockpitCigaretteComponent::UCockpitCigaretteComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	SmokeMesh = Sphere.Object;
	SmokeMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TankGame/Materials/MI_Cigarette_Smoke.MI_Cigarette_Smoke")));
}

void UCockpitCigaretteComponent::BeginPlay()
{
	Super::BeginPlay();
	Heat = IdleHeat;

	TArray<USceneComponent*> Children;
	GetChildrenComponents(true, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child->GetFName() == EmberComponentName)
		{
			if (UPrimitiveComponent* Ember = Cast<UPrimitiveComponent>(Child); Ember && Ember->GetNumMaterials() > 0)
			{
				EmberMaterial = Ember->CreateDynamicMaterialInstance(0);
			}
		}
		else if (Child->GetFName() == GlowComponentName)
		{
			Glow = Cast<UPointLightComponent>(Child);
		}
	}

	// Puffs live in the parent (cockpit) space so the smoke rides along with the tank.
	USceneComponent* Parent = GetAttachParent();
	UMaterialInterface* Material = SmokeMaterial.LoadSynchronous();
	for (int32 Index = 0; Index < PuffCount && Parent; ++Index)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(GetOwner());
		Puff->SetStaticMesh(SmokeMesh);
		if (Material)
		{
			Puff->SetMaterial(0, Material);
		}
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetMobility(EComponentMobility::Movable);
		Puff->SetupAttachment(Parent);
		Puff->RegisterComponent();
		Puff->SetVisibility(false);
		PuffMeshes.Add(Puff);
	}
	Puffs.SetNum(PuffMeshes.Num());
	UpdateGlow();
}

bool UCockpitCigaretteComponent::GetHead(FTransform& OutHead) const
{
	if (const USceneComponent* Head = HeadOverride.Get())
	{
		OutHead = Head->GetComponentTransform();
		return true;
	}
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return false;
	}
	if (const APawn* Pawn = PC->GetPawn())
	{
		if (const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>())
		{
			OutHead = Camera->GetComponentTransform();
			return true;
		}
	}
	if (PC->PlayerCameraManager)
	{
		OutHead = FTransform(PC->PlayerCameraManager->GetCameraRotation(), PC->PlayerCameraManager->GetCameraLocation());
		return true;
	}
	return false;
}

int32 UCockpitCigaretteComponent::GetVisiblePuffs() const
{
	int32 Count = 0;
	for (const FPuff& Puff : Puffs)
	{
		Count += (Puff.Age >= 0.0f && Puff.Delay <= 0.0f) ? 1 : 0;
	}
	return Count;
}

void UCockpitCigaretteComponent::SetCrackle(bool bOn)
{
	if (bOn)
	{
		if (!Crackle)
		{
			if (USoundBase* Sound = CrackleSound.LoadSynchronous())
			{
				Crackle = UGameplayStatics::SpawnSoundAttached(Sound, this, NAME_None, FVector(-FilterOffset, 0.0, 0.0), EAttachLocation::KeepRelativeOffset, false, 0.4f);
			}
		}
		else
		{
			Crackle->Play();
		}
	}
	else if (Crackle)
	{
		Crackle->FadeOut(0.2f, 0.0f);
	}
}

void UCockpitCigaretteComponent::StartExhale(const FTransform& Head)
{
	const USceneComponent* Parent = GetAttachParent();
	const FTransform Space = Parent ? Parent->GetComponentTransform() : FTransform::Identity;
	const FVector Start = Head.TransformPositionNoScale(MouthOffset + FVector(6.0, 0.0, 0.0));
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		const FVector Spread(0.0, FMath::FRandRange(-6.0f, 6.0f), FMath::FRandRange(-2.0f, 6.0f));
		const FVector Velocity = Head.TransformVectorNoScale(FVector(35.0, 0.0, 0.0) + Spread);
		FPuff& Puff = Puffs[Index];
		Puff.Age = 0.0f;
		Puff.Delay = Index * 0.07f;
		Puff.Location = Space.InverseTransformPositionNoScale(Start);
		Puff.Velocity = Space.InverseTransformVectorNoScale(Velocity);
	}
}

void UCockpitCigaretteComponent::UpdatePuffs(float DeltaTime)
{
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		FPuff& Puff = Puffs[Index];
		UStaticMeshComponent* Mesh = PuffMeshes[Index];
		if (Puff.Age < 0.0f)
		{
			continue;
		}
		if (Puff.Delay > 0.0f)
		{
			Puff.Delay -= DeltaTime;
			continue;
		}
		Puff.Age += DeltaTime;
		const float T = Puff.Age / PuffLifetime;
		if (T >= 1.0f)
		{
			Puff.Age = -1.0f;
			Mesh->SetVisibility(false);
			continue;
		}
		// Slows down, drifts up, swells and then thins out.
		Puff.Velocity *= FMath::Max(0.0f, 1.0f - 1.6f * DeltaTime);
		Puff.Velocity.Z += 6.0f * DeltaTime;
		Puff.Location += Puff.Velocity * DeltaTime;
		const float Size = PuffSize * (0.25f + 0.75f * T) * (1.0f - T * T);
		Mesh->SetRelativeLocation(Puff.Location);
		Mesh->SetRelativeScale3D(FVector(FMath::Max(Size, 0.1f) / 100.0f));
		Mesh->SetVisibility(true);
	}
}

void UCockpitCigaretteComponent::UpdateGlow()
{
	if (EmberMaterial)
	{
		EmberMaterial->SetScalarParameterValue(EmberParameter, FMath::Lerp(EmberStrengthRange.X, EmberStrengthRange.Y, Heat));
	}
	if (Glow)
	{
		Glow->SetIntensity(FMath::Lerp(GlowIntensityRange.X, GlowIntensityRange.Y, Heat));
	}
}

void UCockpitCigaretteComponent::HandleReleased()
{
	if (bDragging)
	{
		bDragging = false;
		SetCrackle(false);
	}
}

void UCockpitCigaretteComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FTransform Head;
	const bool bHasHead = GetHead(Head);
	bool bAtMouth = false;
	if (IsHeld() && bHasHead)
	{
		const FVector Mouth = Head.TransformPositionNoScale(MouthOffset);
		const FVector Filter = GetComponentTransform().TransformPositionNoScale(FVector(FilterOffset, 0.0, 0.0));
		bAtMouth = FVector::Dist(Mouth, Filter) <= DragDistance;
	}

	if (bAtMouth && !bDragging)
	{
		bDragging = true;
		DragTime = 0.0f;
		ExhaleIn = -1.0f;
		SetCrackle(true);
	}
	else if (!bAtMouth && bDragging)
	{
		bDragging = false;
		SetCrackle(false);
		if (DragTime >= MinDragTime)
		{
			++DragCount;
			ExhaleIn = ExhaleDelay;
		}
	}

	if (bDragging)
	{
		DragTime += DeltaTime;
		Heat = FMath::Min(1.0f, Heat + DeltaTime / HeatUpTime);
	}
	else
	{
		Heat = FMath::FInterpConstantTo(Heat, IdleHeat, DeltaTime, 1.0f / CoolDownTime);
	}

	if (ExhaleIn > 0.0f)
	{
		ExhaleIn -= DeltaTime;
		if (ExhaleIn <= 0.0f && bHasHead)
		{
			StartExhale(Head);
		}
	}

	UpdateGlow();
	UpdatePuffs(DeltaTime);
}
