#include "Audio/TankAudio.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

TSoftObjectPtr<USoundBase> TankAudio::Sound(const TCHAR* Name)
{
	return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/TankGame/Audio/%s.%s"), Name, Name)));
}

UTankAudioSubsystem* UTankAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	return Game ? Game->GetSubsystem<UTankAudioSubsystem>() : nullptr;
}

USoundAttenuation* UTankAudioSubsystem::GetAttenuation(float Reach)
{
	const int32 Key = FMath::Max(1, FMath::RoundToInt(Reach / 100.0f));
	if (const TObjectPtr<USoundAttenuation>* Found = Attenuations.Find(Key))
	{
		return *Found;
	}
	USoundAttenuation* Falloff = NewObject<USoundAttenuation>(this);
	FSoundAttenuationSettings& Settings = Falloff->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	// Full level right next to it, then the natural drop over the reach.
	Settings.AttenuationShapeExtents = FVector(FMath::Min(500.0f, Reach * 0.06f));
	Settings.FalloffDistance = Reach;
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Settings.dBAttenuationAtMax = -40.0f;
	Attenuations.Add(Key, Falloff);
	return Falloff;
}

bool UTankAudioSubsystem::IsLoud(const USoundBase* Sound)
{
	static const TSet<FName> Loud = {
		TEXT("SW_Cannon_Fire"), TEXT("SW_Cannon_Interior"), TEXT("SW_Explosion_HE"), TEXT("SW_Impact_AP"), TEXT("SW_Hull_Hit"),
		TEXT("SW_Enemy_Cannon"), TEXT("SW_ATGun_Fire"), TEXT("SW_Enemy_Destroyed")};
	return Sound && Loud.Contains(Sound->GetFName());
}

float UTankAudioSubsystem::TakeHeadroom(const UWorld* World, float Weight)
{
	if (!World || Weight <= 0.0f)
	{
		return 1.0f;
	}
	const double Now = World->GetTimeSeconds();
	LoudLoad *= FMath::Exp(-static_cast<float>(Now - LoudLoadTime) / FMath::Max(LoudRecoveryTime, 0.01f));
	LoudLoadTime = Now;
	const float Share = LoudLoad + Weight <= LoudHeadroom ? 1.0f : FMath::Clamp((LoudHeadroom - LoudLoad) / Weight, LoudFloor, 1.0f);
	LoudLoad += Weight * Share;
	return Share;
}

void UTankAudioSubsystem::Note(const UObject* WorldContext, const USoundBase* Sound, const FVector& Location, bool bInside, float StartVolume)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !Sound)
	{
		return;
	}
	if (Journal.Num() >= 512)
	{
		Journal.RemoveAt(0, 128);
	}
	FTankSoundEvent& Event = Journal.AddDefaulted_GetRef();
	Event.Sound = Sound->GetFName();
	Event.Time = World->GetTimeSeconds();
	Event.Location = Location;
	Event.bInside = bInside;
	Event.Volume = StartVolume;
}

float UTankAudioSubsystem::GetLastPlayed(FName Sound) const
{
	for (int32 Index = Journal.Num() - 1; Index >= 0; --Index)
	{
		if (Journal[Index].Sound == Sound)
		{
			return Journal[Index].Time;
		}
	}
	return -1.0f;
}

FString UTankAudioSubsystem::GetJournalText() const
{
	FString Text;
	for (const FTankSoundEvent& Event : Journal)
	{
		Text += FString::Printf(TEXT("%.3f  %s%s\n"), Event.Time, *Event.Sound.ToString(), Event.bInside ? TEXT("") : TEXT("  (world)"));
	}
	return Text;
}

void UTankAudioSubsystem::StartRecording(const UObject* WorldContext, float Seconds)
{
	UAudioMixerBlueprintLibrary::StartRecordingOutput(WorldContext, Seconds, nullptr);
}

void UTankAudioSubsystem::StopRecording(const UObject* WorldContext, const FString& Name, const FString& Folder)
{
	UAudioMixerBlueprintLibrary::StopRecordingOutput(WorldContext, EAudioRecordingExportType::WavFile, Name, Folder, nullptr, nullptr);
}

float UTankAudioSubsystem::GetLastVolume(FName Sound) const
{
	for (int32 Index = Journal.Num() - 1; Index >= 0; --Index)
	{
		if (Journal[Index].Sound == Sound)
		{
			return Journal[Index].Volume;
		}
	}
	return -1.0f;
}

int32 UTankAudioSubsystem::CountPlayed(FName Sound) const
{
	int32 Count = 0;
	for (const FTankSoundEvent& Event : Journal)
	{
		Count += Event.Sound == Sound ? 1 : 0;
	}
	return Count;
}

void UTankAudioSubsystem::PlayAt(const UObject* WorldContext, USoundBase* Sound, FVector Location, float VolumeScale, float Pitch, float Reach)
{
	if (!Sound || !WorldContext || VolumeScale <= 0.0f)
	{
		return;
	}
	UTankAudioSubsystem* Audio = Get(WorldContext);
	USoundAttenuation* Falloff = Audio ? Audio->GetAttenuation(Reach) : nullptr;
	if (Audio && IsLoud(Sound))
	{
		// A far bang barely reaches us, so it takes little of the headroom.
		const UWorld* World = WorldContext->GetWorld();
		const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(WorldContext, 0);
		const float Distance = Camera ? FVector::Dist(Camera->GetCameraLocation(), Location) : 0.0f;
		const float Near = FMath::Square(1.0f - FMath::Clamp(Distance / FMath::Max(Reach, 1.0f), 0.0f, 1.0f));
		VolumeScale *= Audio->TakeHeadroom(World, VolumeScale * Near);
	}
	UGameplayStatics::PlaySoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, VolumeScale * (Audio ? Audio->Volume : 1.0f), Pitch, 0.0f, Falloff);
	if (Audio)
	{
		Audio->Note(WorldContext, Sound, Location, false, VolumeScale);
	}
}

void UTankAudioSubsystem::PlayInside(const UObject* WorldContext, USoundBase* Sound, float VolumeScale, float Pitch)
{
	if (!Sound || !WorldContext || VolumeScale <= 0.0f)
	{
		return;
	}
	UTankAudioSubsystem* Audio = Get(WorldContext);
	if (Audio && IsLoud(Sound))
	{
		VolumeScale *= Audio->TakeHeadroom(WorldContext->GetWorld(), VolumeScale);
	}
	UGameplayStatics::PlaySound2D(WorldContext, Sound, VolumeScale * (Audio ? Audio->Volume : 1.0f), Pitch);
	if (Audio)
	{
		Audio->Note(WorldContext, Sound, FVector::ZeroVector, true, VolumeScale);
	}
}

UAudioComponent* UTankAudioSubsystem::MakeLoop(USceneComponent* AttachTo, USoundBase* Sound, bool bInside, float Reach)
{
	if (!AttachTo || !Sound || !AttachTo->GetOwner())
	{
		return nullptr;
	}
	UAudioComponent* Loop = NewObject<UAudioComponent>(AttachTo->GetOwner());
	Loop->SetSound(Sound);
	Loop->bAutoActivate = false;
	Loop->bAutoDestroy = false;
	Loop->bAllowSpatialization = !bInside;
	if (!bInside)
	{
		if (UTankAudioSubsystem* Audio = Get(AttachTo))
		{
			Loop->AttenuationSettings = Audio->GetAttenuation(Reach);
		}
	}
	Loop->SetupAttachment(AttachTo);
	Loop->RegisterComponent();
	return Loop;
}

void UTankAudioSubsystem::SetLoop(UAudioComponent* Loop, float VolumeScale, float Pitch)
{
	if (!Loop)
	{
		return;
	}
	const bool bWanted = VolumeScale > 0.01f;
	const bool bRunning = Loop->ComponentHasTag(TEXT("Running"));
	if (bWanted && !bRunning)
	{
		Loop->ComponentTags.Add(TEXT("Running"));
		Loop->Play();
		if (UTankAudioSubsystem* Audio = Get(Loop))
		{
			Audio->Note(Loop, Loop->GetSound(), Loop->GetComponentLocation(), !Loop->bAllowSpatialization);
		}
	}
	else if (!bWanted && bRunning)
	{
		Loop->ComponentTags.Remove(TEXT("Running"));
		Loop->FadeOut(0.12f, 0.0f);
	}
	if (bWanted)
	{
		const UTankAudioSubsystem* Audio = Get(Loop);
		Loop->SetVolumeMultiplier(VolumeScale * (Audio ? Audio->Volume : 1.0f));
		Loop->SetPitchMultiplier(FMath::Clamp(Pitch, 0.2f, 3.0f));
	}
}
