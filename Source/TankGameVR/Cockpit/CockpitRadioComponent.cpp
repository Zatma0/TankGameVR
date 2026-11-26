#include "Cockpit/CockpitRadioComponent.h"
#include "Audio/TankAudio.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CableComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Components/AudioComponent.h"
#include "Enemies/TankEnemy.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Tank/TankVehicle.h"
#include "Tank/TankVisionConeComponent.h"

namespace
{
	const TCHAR* SectorName(ETankRadioSector Sector)
	{
		switch (Sector)
		{
		case ETankRadioSector::Front: return TEXT("Front");
		case ETankRadioSector::Left: return TEXT("Left");
		case ETankRadioSector::Right: return TEXT("Right");
		case ETankRadioSector::Rear: return TEXT("Rear");
		default: return TEXT("");
		}
	}

	bool IsHiddenEnemy(const ATankEnemy* Enemy, const UTankVisionConeComponent* Vision)
	{
		const UTankHealthComponent* Health = Enemy ? Enemy->GetHealth() : nullptr;
		return Health && !Health->IsDead() && (!Vision || !Vision->CanSeeActor(Enemy));
	}
}

UCockpitRadioComponent::UCockpitRadioComponent()
{
	CallSound = TankAudio::Sound(TEXT("SW_Radio_Call_Loop"));
	PickupSound = TankAudio::Sound(TEXT("SW_Radio_Pickup"));
	SlipSound = TankAudio::Sound(TEXT("SW_Switch_Click"));
}

void UCockpitRadioComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AActor* Owner = GetOwner())
	{
		TArray<UCableComponent*> Cables;
		Owner->GetComponents(Cables);
		for (UCableComponent* Candidate : Cables)
		{
			if (!Cable || Candidate->GetFName() == CableComponentName)
			{
				Cable = Candidate;
			}
		}
	}
	if (Cable)
	{
		Cable->SetAttachEndToComponent(this);
	}
	Ring = UTankAudioSubsystem::MakeLoop(this, CallSound.LoadSynchronous(), false, 700.0f);
}

void UCockpitRadioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Voice)
	{
		Voice->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

bool UCockpitRadioComponent::GetHead(FTransform& OutHead) const
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

ETankRadioSector UCockpitRadioComponent::GetHiddenSector(FString& OutWhat, int32& OutCount) const
{
	OutWhat.Reset();
	OutCount = 0;
	const ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	if (!Tank || !GetWorld())
	{
		return ETankRadioSector::None;
	}
	const UTankVisionConeComponent* Vision = Tank->GetVisionCone();
	const FTransform Hull = Tank->GetActorTransform();

	// Count the unseen enemies per quarter around the hull and keep the busiest one.
	int32 Counts[5] = {};
	TArray<ETankEnemyType> Types[5];
	for (TActorIterator<ATankEnemy> It(GetWorld()); It; ++It)
	{
		if (!IsHiddenEnemy(*It, Vision))
		{
			continue;
		}
		const FVector Local = Hull.InverseTransformPositionNoScale(It->GetActorLocation());
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
		const ETankRadioSector Sector = FMath::Abs(Angle) <= 45.0f ? ETankRadioSector::Front
			: Angle > 45.0f && Angle <= 135.0f ? ETankRadioSector::Right
			: Angle < -45.0f && Angle >= -135.0f ? ETankRadioSector::Left
			: ETankRadioSector::Rear;
		++Counts[static_cast<int32>(Sector)];
		Types[static_cast<int32>(Sector)].Add(It->EnemyType);
	}
	int32 Best = 0;
	for (int32 Index = 1; Index < 5; ++Index)
	{
		if (Counts[Index] > Counts[Best])
		{
			Best = Index;
		}
	}
	if (Counts[Best] == 0)
	{
		return ETankRadioSector::None;
	}
	const TArray<ETankEnemyType>& Kinds = Types[Best];
	const bool bSame = !Kinds.ContainsByPredicate([&Kinds](ETankEnemyType Type) { return Type != Kinds[0]; });
	OutWhat = !bSame ? TEXT("Mixed")
		: Kinds[0] == ETankEnemyType::Tank ? TEXT("Tanks")
		: Kinds[0] == ETankEnemyType::ArmoredCar ? TEXT("Cars")
		: TEXT("ATGun");
	OutCount = Counts[Best];
	return static_cast<ETankRadioSector>(Best);
}

void UCockpitRadioComponent::SetRinging(bool bRing)
{
	bCalling = bRing;
	CallAge = 0.0f;
	UTankAudioSubsystem::SetLoop(Ring, bRing ? 0.8f : 0.0f);
}

void UCockpitRadioComponent::CheckForNewContacts()
{
	if (bCalling || IsTalking() || SinceLastCall < TimeBetweenCalls)
	{
		return;
	}
	const ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	const UTankVisionConeComponent* Vision = Tank ? Tank->GetVisionCone() : nullptr;
	for (TActorIterator<ATankEnemy> It(GetWorld()); It; ++It)
	{
		if (IsHiddenEnemy(*It, Vision) && !Reported.Contains(*It))
		{
			SetRinging(true);
			return;
		}
	}
}

void UCockpitRadioComponent::Answer()
{
	if (!IsTalking())
	{
		StartReport(true);
	}
}

void UCockpitRadioComponent::StartReport(bool bButton)
{
	FString What;
	int32 Count = 0;
	const ETankRadioSector Sector = GetHiddenSector(What, Count);
	const FString Name = Sector == ETankRadioSector::None ? MessagePrefix + TEXT("Clear") : FString::Printf(TEXT("%s%s_%s"), *MessagePrefix, SectorName(Sector), *What);
	PendingMessage = TankAudio::Sound(*Name).LoadSynchronous();

	static const TMap<FString, FString> Words = {
		{TEXT("Tanks"), TEXT("ENEMY TANKS")}, {TEXT("Cars"), TEXT("ARMORED CARS")}, {TEXT("ATGun"), TEXT("AT GUN")}, {TEXT("Mixed"), TEXT("MIXED ARMOR")}};
	Transcript = Sector == ETankRadioSector::None ? TEXT("RADIO: NO CONTACTS AROUND YOU")
		: FString::Printf(TEXT("RADIO: CONTACT %s - %s"), *FString(SectorName(Sector)).ToUpper(), *Words.FindRef(What));

	// Everything hidden right now has been told; only newcomers ring again.
	const ATankVehicle* Tank = Cast<ATankVehicle>(GetOwner());
	const UTankVisionConeComponent* Vision = Tank ? Tank->GetVisionCone() : nullptr;
	for (TActorIterator<ATankEnemy> It(GetWorld()); It; ++It)
	{
		if (IsHiddenEnemy(*It, Vision))
		{
			Reported.Add(*It);
		}
	}

	if (Voice)
	{
		Voice->Stop();
		Voice = nullptr;
	}
	VoiceLeft = 0.0f;
	bButtonAnswer = bButton;
	SetRinging(false);
	SinceLastCall = 0.0f;
	SinceReport = 0.0f;
	++ReportCount;
	UTankAudioSubsystem::PlayInside(this, PickupSound.LoadSynchronous(), 0.6f);
	VoiceTimer = VoiceDelay;
}

bool UCockpitRadioComponent::IsTalking() const
{
	// By the message length, not the audio component: it also holds with no audio device.
	return VoiceTimer >= 0.0f || VoiceLeft > 0.0f;
}

FString UCockpitRadioComponent::GetTranscript() const
{
	return SinceReport < 6.0f ? Transcript : FString();
}

void UCockpitRadioComponent::HandleReleased()
{
	// Hung up: the line goes dead.
	if (!bButtonAnswer)
	{
		if (Voice)
		{
			Voice->Stop();
		}
		VoiceTimer = -1.0f;
		VoiceLeft = 0.0f;
	}
	bAtEar = false;
}

void UCockpitRadioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SinceLastCall += DeltaTime;
	SinceReport += DeltaTime;
	VoiceLeft = FMath::Max(0.0f, VoiceLeft - DeltaTime);

	// A cord only stretches so far.
	if (IsHeld() && Cable && FVector::Dist(GetComponentLocation(), Cable->GetComponentLocation()) > MaxCableReach)
	{
		++SlipCount;
		UTankAudioSubsystem::PlayInside(this, SlipSound.LoadSynchronous(), 0.5f, 0.8f);
		ReturnHome();
		HandleReleased();
		OnReleased.Broadcast(this);
	}

	bool bNowAtEar = false;
	FTransform Head;
	if (IsHeld() && GetHead(Head))
	{
		for (const float Side : {1.0f, -1.0f})
		{
			const FVector Ear = Head.TransformPositionNoScale(FVector(EarOffset.X, EarOffset.Y * Side, EarOffset.Z));
			bNowAtEar |= FVector::Dist(Ear, GetComponentLocation()) <= EarDistance;
		}
	}
	if (bNowAtEar && !bAtEar && !IsTalking())
	{
		StartReport(false);
	}
	bAtEar = bNowAtEar;

	if (VoiceTimer >= 0.0f)
	{
		VoiceTimer -= DeltaTime;
		if (VoiceTimer < 0.0f && PendingMessage)
		{
			Voice = UGameplayStatics::SpawnSound2D(this, PendingMessage, 1.0f);
			VoiceLeft = PendingMessage->GetDuration();
			if (UTankAudioSubsystem* Audio = UTankAudioSubsystem::Get(this))
			{
				Audio->Note(this, PendingMessage, GetComponentLocation(), true);
			}
			PendingMessage = nullptr;
		}
	}
	if (Voice && VoiceLeft > 0.0f)
	{
		const UTankAudioSubsystem* Audio = UTankAudioSubsystem::Get(this);
		Voice->SetVolumeMultiplier((bAtEar || bButtonAnswer ? 1.0f : AwayVolume) * (Audio ? Audio->Volume : 1.0f));
	}

	ScanTimer -= DeltaTime;
	if (ScanTimer <= 0.0f)
	{
		ScanTimer = 0.5f;
		CheckForNewContacts();
	}
	if (bCalling && (CallAge += DeltaTime) > CallTimeout)
	{
		SetRinging(false);
		SinceLastCall = 0.0f;
	}
}
