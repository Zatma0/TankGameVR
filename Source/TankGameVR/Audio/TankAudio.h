#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TankAudio.generated.h"

class UAudioComponent;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

/** One sound that was started: what, when (world seconds) and where. */
USTRUCT(BlueprintType)
struct TANKGAMEVR_API FTankSoundEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FName Sound;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	float Time = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	FVector Location = FVector::ZeroVector;

	/** Cockpit / headset sound, not placed in the world. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	bool bInside = false;

	/** The volume it was really started with, after the shared headroom. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio")
	float Volume = 1.0f;
};

/**
 * Every game sound goes through here. World sounds fade with distance, cockpit sounds do not, and each start is
 * written to a journal with its time, so it can be checked that a sound lands on the frame of its event.
 */
UCLASS()
class TANKGAMEVR_API UTankAudioSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UTankAudioSubsystem* Get(const UObject* WorldContext);

	/** Scales everything played through here. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float Volume = 1.0f;

	/**
	 * Big bangs (gun blasts, explosions, hull hits) share this much level. Two of them together fit; a third one on top
	 * of those is played only as loud as the room left, so a pile-up never clips. Units: sum of volume scales.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float LoudHeadroom = 1.8f;

	/** How fast the room comes back after a bang (seconds for the load to drop to a third). */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float LoudRecoveryTime = 0.3f;

	/** A bang never goes below this share of its volume, so it is still heard under the others. */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float LoudFloor = 0.2f;

	/** A sound out in the world: quieter with distance, gone at Reach. */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static void PlayAt(const UObject* WorldContext, USoundBase* Sound, FVector Location, float VolumeScale = 1.0f, float Pitch = 1.0f, float Reach = 15000.0f);

	/** A sound in the cockpit or the headset: the same level wherever the tank is. */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static void PlayInside(const UObject* WorldContext, USoundBase* Sound, float VolumeScale = 1.0f, float Pitch = 1.0f);

	/** A looping sound on a component, silent until SetLoop gives it a level. */
	static UAudioComponent* MakeLoop(USceneComponent* AttachTo, USoundBase* Sound, bool bInside, float Reach = 6000.0f);

	/** Level and pitch of a loop; it stops below a whisper and starts again when it comes back. */
	static void SetLoop(UAudioComponent* Loop, float VolumeScale, float Pitch = 1.0f);

	UFUNCTION(BlueprintPure, Category = "Audio")
	const TArray<FTankSoundEvent>& GetJournal() const { return Journal; }

	/** World time the named sound was last started, or -1 if it never was. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetLastPlayed(FName Sound) const;

	UFUNCTION(BlueprintPure, Category = "Audio")
	int32 CountPlayed(FName Sound) const;

	/** Volume the named sound was last started with (after the shared headroom), or -1. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	float GetLastVolume(FName Sound) const;

	/** True for the sounds that share LoudHeadroom. */
	static bool IsLoud(const USoundBase* Sound);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void ClearJournal() { Journal.Reset(); }

	/** The journal as text, one "seconds  sound" line per start. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	FString GetJournalText() const;

	/** This subsystem, for Blueprints and scripts. */
	UFUNCTION(BlueprintPure, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static UTankAudioSubsystem* GetTankAudio(const UObject* WorldContext) { return Get(WorldContext); }

	/** Records everything the game plays, to check the mix or to put under a gameplay video. */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static void StartRecording(const UObject* WorldContext, float Seconds = 30.0f);

	/** Ends the recording and writes Folder/Name.wav. */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static void StopRecording(const UObject* WorldContext, const FString& Name, const FString& Folder);

	void Note(const UObject* WorldContext, const USoundBase* Sound, const FVector& Location, bool bInside, float StartVolume = 1.0f);

private:
	USoundAttenuation* GetAttenuation(float Reach);

	/** Share of its volume a loud sound gets right now; Weight is how loud it reaches the listener. */
	float TakeHeadroom(const UWorld* World, float Weight);

	float LoudLoad = 0.0f;
	double LoudLoadTime = 0.0;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USoundAttenuation>> Attenuations;

	TArray<FTankSoundEvent> Journal;
};

namespace TankAudio
{
	/** The sound /Game/TankGame/Audio/<Name>. */
	TANKGAMEVR_API TSoftObjectPtr<USoundBase> Sound(const TCHAR* Name);
}
