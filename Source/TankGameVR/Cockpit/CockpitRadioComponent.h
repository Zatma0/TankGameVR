#pragma once

#include "CoreMinimal.h"
#include "Cockpit/CockpitGrabbableComponent.h"
#include "CockpitRadioComponent.generated.h"

class UAudioComponent;
class UCableComponent;
class USoundBase;

/** Where the hidden enemies are, as the radio says it (sector relative to the hull). */
UENUM(BlueprintType)
enum class ETankRadioSector : uint8
{
	None,
	Front,
	Left,
	Right,
	Rear
};

/**
 * Radio handset on a hook, tied to the set by a cable. Lift it to your ear and command tells you where the enemies
 * you cannot see are (front / left / right / rear of the hull, and what they are). When new enemies show up out of
 * sight the set rings; pick up to hear them. Pull the cable too far and the handset slips out of the hand.
 * Pivot = middle of the handset, +Z = earpiece end.
 */
UCLASS(ClassGroup = (Cockpit), meta = (BlueprintSpawnableComponent))
class TANKGAMEVR_API UCockpitRadioComponent : public UCockpitGrabbableComponent
{
	GENERATED_BODY()

public:
	UCockpitRadioComponent();

	/** Ears in head (camera) space; the handset counts as "at the ear" near either one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	FVector EarOffset = FVector(-5.0, 8.0, -3.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (ClampMin = "3.0", Units = "Centimeters"))
	float EarDistance = 16.0f;

	/** The cable tying the handset to the set. Empty = the first cable component of the owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio")
	FName CableComponentName = TEXT("RadioCable");

	/** Beyond this from the cable's root the handset slips out of the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (ClampMin = "10.0", Units = "Centimeters"))
	float MaxCableReach = 100.0f;

	/** A call rings this long, then gives up (the enemies are reported again later). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (Units = "Seconds"))
	float CallTimeout = 15.0f;

	/** Quiet time after a report or an unanswered call before it rings again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (Units = "Seconds"))
	float TimeBetweenCalls = 10.0f;

	/** The static rush before the voice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (Units = "Seconds"))
	float VoiceDelay = 0.25f;

	/** Level of the voice when the handset is away from the ear (heard faintly from the earpiece). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AwayVolume = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio|Sound")
	TSoftObjectPtr<USoundBase> CallSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio|Sound")
	TSoftObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio|Sound")
	TSoftObjectPtr<USoundBase> SlipSound;

	/** Messages are /Game/TankGame/Audio/<Prefix><Sector>_<Type> and <Prefix>Clear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radio|Sound")
	FString MessagePrefix = TEXT("SW_Radio_");

	/** Listen now without the handset (keyboard / gamepad). */
	UFUNCTION(BlueprintCallable, Category = "Radio")
	void Answer();

	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsCalling() const { return bCalling; }

	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsAtEar() const { return bAtEar; }

	/** A report is being spoken (or about to be). */
	UFUNCTION(BlueprintPure, Category = "Radio")
	bool IsTalking() const;

	/** Subtitle of the last report for the display, empty when it is old. */
	UFUNCTION(BlueprintPure, Category = "Radio")
	FString GetTranscript() const;

	/** What the radio would say right now. */
	UFUNCTION(BlueprintCallable, Category = "Radio")
	ETankRadioSector GetHiddenSector(FString& OutWhat, int32& OutCount) const;

	UFUNCTION(BlueprintPure, Category = "Radio")
	int32 GetReportCount() const { return ReportCount; }

	UFUNCTION(BlueprintPure, Category = "Radio")
	int32 GetSlipCount() const { return SlipCount; }

	UFUNCTION(BlueprintPure, Category = "Radio")
	UCableComponent* GetCable() const { return Cable; }

	/** Tests: a head to use instead of the player camera. */
	void SetHeadOverride(USceneComponent* Head) { HeadOverride = Head; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleReleased() override;

private:
	bool GetHead(FTransform& OutHead) const;
	void CheckForNewContacts();
	void StartReport(bool bButton);
	void SetRinging(bool bRing);

	UPROPERTY(Transient)
	TObjectPtr<UCableComponent> Cable;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Ring;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Voice;

	TWeakObjectPtr<USceneComponent> HeadOverride;
	TSet<TWeakObjectPtr<AActor>> Reported;
	TObjectPtr<USoundBase> PendingMessage;
	FString Transcript;
	float CallAge = 0.0f;
	float SinceLastCall = 1000.0f;
	float ScanTimer = 0.0f;
	float VoiceTimer = -1.0f;
	float SinceReport = 1000.0f;
	float VoiceLeft = 0.0f;
	int32 ReportCount = 0;
	int32 SlipCount = 0;
	bool bCalling = false;
	bool bAtEar = false;
	bool bButtonAnswer = false;
};
