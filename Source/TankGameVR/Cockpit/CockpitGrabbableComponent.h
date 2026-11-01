#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CockpitGrabbableComponent.generated.h"

class UCockpitGrabbableComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCockpitGrabChanged, UCockpitGrabbableComponent*, Grabbable);

UENUM(BlueprintType)
enum class ECockpitGrabState : uint8
{
	Home,
	Held,
	Returning,
	/** Placed by someone else (e.g. the breech); hands are ignored. */
	Locked,
	/** Used up and hidden until it comes back. */
	Away
};

/**
 * Loose cockpit item (shell, cigarette...). Place it at the item's pivot with the meshes and a BP_GrabComponent
 * (Grab Type = Custom) under it. Follows the hand while held and slides back home when released.
 * No physics: the tank moves kinematically, so everything stays in cockpit space.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitGrabbableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCockpitGrabbableComponent();

	/** Grab component under this item. Empty = first child grab component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	FName GrabComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable")
	bool bReturnHomeOnRelease = true;

	/** How fast it slides home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grabbable", meta = (ClampMin = "0.5"))
	float ReturnSpeed = 8.0f;

	UPROPERTY(BlueprintAssignable, Category = "Grabbable")
	FOnCockpitGrabChanged OnGrabbed;

	UPROPERTY(BlueprintAssignable, Category = "Grabbable")
	FOnCockpitGrabChanged OnReleased;

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsHeld() const { return State == ECockpitGrabState::Held; }

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	ECockpitGrabState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	USceneComponent* GetHand() const { return Hand.Get(); }

	/** Hold with a given hand (tests / custom interaction). */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void SetHeldByHand(USceneComponent* InHand);

	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void ReleaseHand();

	/** While held, the item can only slide along Frame's X axis (pivot between Min and Max, in cm) with Frame's rotation. */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void SetRail(USceneComponent* Frame, float Min, float Max);

	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void ClearRail();

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsOnRail(const USceneComponent* Frame = nullptr) const;

	/** Pivot X in the rail frame. */
	UFUNCTION(BlueprintPure, Category = "Grabbable")
	float GetRailPosition() const { return RailPosition; }

	/** Puts the item on the rail at Position (works while locked). */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void MoveOnRail(float Position);

	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void SetLocked(bool bLocked);

	/** Hide it (used up) or bring it back home. */
	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void SetAway(bool bAway);

	UFUNCTION(BlueprintCallable, Category = "Grabbable")
	void ReturnHome(bool bInstant = false);

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	bool IsAtHome() const { return State == ECockpitGrabState::Home; }

	UFUNCTION(BlueprintPure, Category = "Grabbable")
	FTransform GetHomeRelativeTransform() const { return HomeRelativeTransform; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

	virtual void HandleGrabbed() {}
	virtual void HandleReleased() {}

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrabComponent;

	TWeakObjectPtr<USceneComponent> Hand;

private:
	void FollowHand();
	void UpdateReturn(float DeltaTime);

	TWeakObjectPtr<USceneComponent> ExplicitHand;
	TWeakObjectPtr<USceneComponent> RailFrame;
	FTransform HomeRelativeTransform;
	FTransform GrabOffset;
	ECockpitGrabState State = ECockpitGrabState::Home;
	float RailMin = 0.0f;
	float RailMax = 0.0f;
	float RailPosition = 0.0f;
	bool bHandWasHolding = false;
	bool bSurfaceApplied = false;
};
