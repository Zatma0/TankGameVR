#include "Cockpit/CockpitGrabUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Haptics/HapticFeedbackEffect_Base.h"
#include "MotionControllerComponent.h"
#include "UObject/UnrealType.h"

namespace CockpitGrab
{
	USceneComponent* FindGrabComponent(const USceneComponent* Root, FName Name)
	{
		if (!Root)
		{
			return nullptr;
		}
		TArray<USceneComponent*> Children;
		Root->GetChildrenComponents(true, Children);
		for (USceneComponent* Child : Children)
		{
			const bool bNameMatches = !Name.IsNone() && Child->GetFName() == Name;
			const bool bClassMatches = Name.IsNone() && Child->GetClass()->GetName().Contains(TEXT("GrabComponent"));
			if (bNameMatches || bClassMatches)
			{
				return Child;
			}
		}
		return nullptr;
	}

	USceneComponent* GetHoldingHand(const USceneComponent* GrabComponent)
	{
		if (!GrabComponent)
		{
			return nullptr;
		}
		bool bIsHeld = false;
		if (const FBoolProperty* HeldProperty = FindFProperty<FBoolProperty>(GrabComponent->GetClass(), TEXT("bIsHeld")))
		{
			bIsHeld = HeldProperty->GetPropertyValue_InContainer(GrabComponent);
		}
		USceneComponent* Controller = nullptr;
		if (const FObjectPropertyBase* ControllerProperty = FindFProperty<FObjectPropertyBase>(GrabComponent->GetClass(), TEXT("MotionControllerRef")))
		{
			Controller = Cast<USceneComponent>(ControllerProperty->GetObjectPropertyValue_InContainer(GrabComponent));
		}
		return (bIsHeld && Controller) ? Controller : nullptr;
	}

	void SetGrabSurface(USceneComponent* GrabComponent, bool bGrabbable)
	{
		if (UPrimitiveComponent* Surface = GrabComponent ? Cast<UPrimitiveComponent>(GrabComponent->GetAttachParent()) : nullptr)
		{
			Surface->SetSimulatePhysics(false);
			Surface->SetCollisionEnabled(bGrabbable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			Surface->SetCollisionObjectType(ECC_PhysicsBody);
			Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
	}

	EControllerHand GetHandSide(const USceneComponent* Hand)
	{
		if (const UMotionControllerComponent* Controller = Cast<UMotionControllerComponent>(Hand))
		{
			return Controller->MotionSource.ToString().Contains(TEXT("Left")) ? EControllerHand::Left : EControllerHand::Right;
		}
		return (Hand && Hand->GetName().Contains(TEXT("Left"))) ? EControllerHand::Left : EControllerHand::Right;
	}

	void PlayHaptic(const UWorld* World, UHapticFeedbackEffect_Base* Effect, EControllerHand Hand, float Scale)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC && Effect && PC->IsLocalController())
		{
			PC->PlayHapticEffect(Effect, Hand, Scale);
		}
	}
}
