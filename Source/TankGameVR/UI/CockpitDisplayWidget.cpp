#include "UI/CockpitDisplayWidget.h"
#include "Cockpit/CockpitRadioComponent.h"
#include "Targeting/TankTargetingComponent.h"
#include "Targeting/TargetRegistrySubsystem.h"
#include "Targeting/TargetableComponent.h"
#include "Targeting/TargetSelectionMath.h"
#include "Weapons/TankBreechComponent.h"
#include "Cockpit/CockpitFireComponent.h"
#include "Cockpit/CockpitSmokeLauncherComponent.h"
#include "Combat/TankDamageReactionComponent.h"
#include "Combat/TankDamageTypes.h"
#include "Combat/TankGroundFire.h"
#include "Weapons/TankCannonComponent.h"
#include "Combat/TankHealthComponent.h"
#include "Enemies/TankEnemy.h"
#include "Game/TankArenaDirector.h"
#include "Tank/TankVehicle.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace
{
	const FVector2D CrosshairSize(96.0, 96.0);
	const FVector2D SelectedMarkerSize(120.0, 120.0);
	const FVector2D MarkerSize(64.0, 64.0);
}

bool UCockpitDisplayWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bResult;
}

UBorder* UCockpitDisplayWidget::MakeRing(const FLinearColor& Color, float Thickness, bool bCircle)
{
	UBorder* Ring = WidgetTree->ConstructWidget<UBorder>();
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor::Transparent);
	Brush.OutlineSettings.Width = Thickness;
	Brush.OutlineSettings.Color = FSlateColor(Color);
	Brush.OutlineSettings.RoundingType = bCircle ? ESlateBrushRoundingType::HalfHeightRadius : ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(6.0, 6.0, 6.0, 6.0);
	Ring->SetBrush(Brush);
	return Ring;
}

void UCockpitDisplayWidget::SetRingColor(UBorder* Ring, const FLinearColor& Color) const
{
	if (Ring && !Ring->Background.OutlineSettings.Color.GetSpecifiedColor().Equals(Color))
	{
		FSlateBrush Brush = Ring->Background;
		Brush.OutlineSettings.Color = FSlateColor(Color);
		Ring->SetBrush(Brush);
	}
}

UTextBlock* UCockpitDisplayWidget::MakeText(int32 Size)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(IdleColor));
	return Text;
}

void UCockpitDisplayWidget::BuildDefaultLayout()
{
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.01f, 0.03f, 0.02f, 0.95f));
	UCanvasPanelSlot* BackgroundSlot = Canvas->AddChildToCanvas(Background);
	BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackgroundSlot->SetOffsets(FMargin(0.0f));

	// Fixed boresight (hull forward).
	UBorder* Boresight = MakeRing(InactiveColor, 2.0f, false);
	UCanvasPanelSlot* BoresightSlot = Canvas->AddChildToCanvas(Boresight);
	BoresightSlot->SetSize(FVector2D(24.0, 24.0));
	BoresightSlot->SetPosition(DisplaySize * 0.5 - FVector2D(12.0, 12.0));

	for (int32 Index = 0; Index < MaxTargetMarkers; ++Index)
	{
		UBorder* Marker = MakeRing(SelectableColor, 4.0f, false);
		Canvas->AddChildToCanvas(Marker)->SetSize(MarkerSize);
		Marker->SetVisibility(ESlateVisibility::Collapsed);
		Markers.Add(Marker);
	}

	Crosshair = MakeRing(IdleColor, 6.0f, true);
	Canvas->AddChildToCanvas(Crosshair)->SetSize(CrosshairSize);

	StatusText = MakeText(30);
	UCanvasPanelSlot* StatusSlot = Canvas->AddChildToCanvas(StatusText);
	StatusSlot->SetPosition(FVector2D(28.0, 20.0));
	StatusSlot->SetAutoSize(true);

	TargetText = MakeText(34);
	UCanvasPanelSlot* TargetSlot = Canvas->AddChildToCanvas(TargetText);
	TargetSlot->SetPosition(FVector2D(28.0, DisplaySize.Y - 70.0));
	TargetSlot->SetAutoSize(true);

	// Gun state, top right.
	GunText = MakeText(30);
	UCanvasPanelSlot* GunSlot = Canvas->AddChildToCanvas(GunText);
	GunSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	GunSlot->SetAlignment(FVector2D(1.0, 0.0));
	GunSlot->SetPosition(FVector2D(-28.0, 20.0));
	GunSlot->SetAutoSize(true);

	// Rack and smoke under the gun line.
	AmmoText = MakeText(26);
	UCanvasPanelSlot* AmmoSlot = Canvas->AddChildToCanvas(AmmoText);
	AmmoSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	AmmoSlot->SetAlignment(FVector2D(1.0, 0.0));
	AmmoSlot->SetPosition(FVector2D(-28.0, 60.0));
	AmmoSlot->SetAutoSize(true);

	HullText = MakeText(30);
	UCanvasPanelSlot* HullSlot = Canvas->AddChildToCanvas(HullText);
	HullSlot->SetPosition(FVector2D(28.0, 60.0));
	HullSlot->SetAutoSize(true);

	// Wave and score, bottom right.
	ArenaText = MakeText(26);
	UCanvasPanelSlot* ArenaSlot = Canvas->AddChildToCanvas(ArenaText);
	ArenaSlot->SetAnchors(FAnchors(1.0f, 1.0f));
	ArenaSlot->SetAlignment(FVector2D(1.0, 1.0));
	ArenaSlot->SetPosition(FVector2D(-28.0, -112.0));
	ArenaSlot->SetAutoSize(true);

	// Big warning line in the upper middle.
	WarningText = MakeText(34);
	WarningText->SetJustification(ETextJustify::Center);
	UCanvasPanelSlot* WarningSlot = Canvas->AddChildToCanvas(WarningText);
	WarningSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	WarningSlot->SetAlignment(FVector2D(0.5, 0.0));
	WarningSlot->SetPosition(FVector2D(0.0, 120.0));
	WarningSlot->SetAutoSize(true);

	HitFrame = MakeRing(LockedColor, 14.0f, false);
	UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(HitFrame);
	FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	FrameSlot->SetOffsets(FMargin(4.0f));
	HitFrame->SetRenderOpacity(0.0f);
}

void UCockpitDisplayWidget::SetTank(ATankVehicle* InTank)
{
	Tank = InTank;
	UpdateTankLines();
}

void UCockpitDisplayWidget::UpdateTankLines()
{
	const ATankVehicle* Tank_ = Tank.Get();
	if (!Tank_)
	{
		return;
	}
	const UTankHealthComponent* Health = Tank_->GetHealth();
	const float Hull = Health ? Health->GetHealthFraction() : 1.0f;
	LastHullLine = FString::Printf(TEXT("HULL %d%%%s"), FMath::RoundToInt(Hull * 100.0f), Health && Health->IsRepairing() ? TEXT("  REPAIRING") : TEXT(""));

	const UTankBreechComponent* Breech_ = Tank_->GetBreech();
	const UCockpitSmokeLauncherComponent* Smoke = Tank_->GetSmokeLauncher();
	LastAmmoLine = FString::Printf(TEXT("AP %d  HE %d   %s"), Breech_ ? Breech_->CountAvailable(ETankRoundType::AP) : 0,
		Breech_ ? Breech_->CountAvailable(ETankRoundType::HE) : 0, Smoke ? *Smoke->GetStatusText() : TEXT(""));

	const ATankArenaDirector* Director = ATankArenaDirector::GetDirector(Tank_);
	LastArenaLine = Director ? Director->GetStatusLine() : FString();

	// Most urgent first.
	const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bBlink = FMath::Frac(Time * 2.5f) < 0.6f;
	float Threat = 0.0f;
	for (TActorIterator<ATankEnemy> It(GetWorld()); It; ++It)
	{
		if (It->EnemyType == ETankEnemyType::AntiTank)
		{
			Threat = FMath::Max(Threat, It->GetThreatTo(Tank_));
		}
	}
	bool bFire = false;
	for (const UCockpitFireComponent* Spot : Tank_->GetFireSpots())
	{
		bFire |= Spot && Spot->IsBurning();
	}
	const UTankDamageReactionComponent* Reaction = Tank_->GetDamageReaction();
	FLinearColor WarningColor = LockedColor;
	if (Tank_->IsDestroyed() || (Director && Director->GetPhase() == ETankArenaPhase::GameOver))
	{
		LastWarning = Director ? Director->GetMessage() : FString(TEXT("TANK DESTROYED"));
	}
	else if (Threat > 0.05f)
	{
		LastWarning = bBlink ? TEXT("AT GUN AIMING - MOVE OR SMOKE!") : TEXT(" ");
	}
	else if (ATankGroundFire::IsBurningAt(GetWorld(), Tank_->GetActorLocation()))
	{
		LastWarning = bBlink ? TEXT("BURNING GROUND - DRIVE OUT!") : TEXT(" ");
	}
	else if (bFire)
	{
		LastWarning = bBlink ? TEXT("FIRE! USE THE EXTINGUISHER") : TEXT(" ");
		WarningColor = SelectableColor;
	}
	else if (Reaction && Reaction->GetTimeSinceHit() < 1.5f && !Reaction->GetLastHitSide().IsEmpty())
	{
		LastWarning = FString::Printf(TEXT("HIT - %s"), *Reaction->GetLastHitSide());
	}
	else if (Tank_->GetRadio() && !Tank_->GetRadio()->GetTranscript().IsEmpty())
	{
		// Subtitle of what command just said.
		LastWarning = Tank_->GetRadio()->GetTranscript();
		WarningColor = ReadyColor;
	}
	else if (Tank_->GetRadio() && Tank_->GetRadio()->IsCalling())
	{
		LastWarning = bBlink ? TEXT("RADIO CALL - PICK UP THE HANDSET") : TEXT(" ");
		WarningColor = ReadyColor;
	}
	else if (Tank_->GetCannon() && Tank_->GetCannon()->GetTimeSinceShotReport() < 2.5f)
	{
		// What our last round did.
		LastWarning = Tank_->GetCannon()->GetShotReport();
		WarningColor = LastWarning.Contains(TEXT("WEAK")) || LastWarning.Contains(TEXT("MISSED")) ? SelectableColor : ReadyColor;
	}
	else
	{
		LastWarning = Director ? Director->GetMessage() : FString();
		WarningColor = IdleColor;
	}

	if (HullText)
	{
		HullText->SetText(FText::FromString(LastHullLine));
		HullText->SetColorAndOpacity(FSlateColor(Hull > 0.6f ? ReadyColor : (Hull > 0.3f ? SelectableColor : LockedColor)));
	}
	if (AmmoText)
	{
		AmmoText->SetText(FText::FromString(LastAmmoLine));
	}
	if (ArenaText)
	{
		ArenaText->SetText(FText::FromString(LastArenaLine));
	}
	if (WarningText)
	{
		WarningText->SetText(FText::FromString(LastWarning));
		WarningText->SetColorAndOpacity(FSlateColor(WarningColor));
	}
	if (HitFrame)
	{
		HitFrame->SetRenderOpacity(Reaction ? Reaction->GetHitFlash() : 0.0f);
	}
}

void UCockpitDisplayWidget::SetTargeting(UTankTargetingComponent* InTargeting)
{
	Targeting = InTargeting;
	UpdateDisplay();
}

void UCockpitDisplayWidget::SetBreech(UTankBreechComponent* InBreech)
{
	Breech = InBreech;
	UpdateGunLine();
}

void UCockpitDisplayWidget::UpdateGunLine()
{
	const UTankBreechComponent* Breech_ = Breech.Get();
	LastGunLine = Breech_ ? Breech_->GetStatusText() : FString();
	const bool bReady = Breech_ && Breech_->IsLoaded();
	bool bWeak = false;
	const UTargetableComponent* Target = Targeting.IsValid() ? Targeting->GetCurrentTarget() : nullptr;
	if (bReady && Target && Target->GetOwner())
	{
		// Say whether the loaded round suits the locked target's armour.
		if (const UTankHealthComponent* Armour = Target->GetOwner()->FindComponentByClass<UTankHealthComponent>())
		{
			const bool bHE = Breech_->GetLoadedRoundType() == ETankRoundType::HE;
			const TSubclassOf<UDamageType> Type = bHE ? TSubclassOf<UDamageType>(UTankDamageType_HE::StaticClass()) : TSubclassOf<UDamageType>(UTankDamageType_AP::StaticClass());
			bWeak = Armour->GetDamageMultiplier(Type) < 0.95f;
			LastGunLine += FString::Printf(TEXT("  %s VS %s"), bWeak ? TEXT("WEAK") : TEXT("GOOD"), *Target->GetTargetDisplayName().ToString().ToUpper());
		}
	}
	if (GunText)
	{
		GunText->SetText(FText::FromString(LastGunLine));
		GunText->SetColorAndOpacity(FSlateColor(bWeak ? LockedColor : (bReady ? ReadyColor : SelectableColor)));
	}
}

void UCockpitDisplayWidget::PlaceCentered(UWidget* Widget, const FVector2D& UV, const FVector2D& Size) const
{
	if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetPosition(UV * DisplaySize - Size * 0.5);
	}
}

void UCockpitDisplayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateDisplay();
}

void UCockpitDisplayWidget::UpdateDisplay()
{
	UpdateGunLine();
	UpdateTankLines();
	const UTankTargetingComponent* Targeting_ = Targeting.Get();
	if (!Targeting_)
	{
		LastStatus = TEXT("NO SIGNAL");
		LastTargetLine.Reset();
		if (StatusText)
		{
			StatusText->SetText(FText::FromString(LastStatus));
		}
		return;
	}

	const FTransform Sight = Targeting_->GetSightTransform();
	const float Aspect = static_cast<float>(DisplaySize.X / FMath::Max(DisplaySize.Y, 1.0));
	const UTargetableComponent* Current = Targeting_->GetCurrentTarget();
	const bool bAssist = Targeting_->IsAimAssistEnabled();

	// Crosshair follows the real (smoothed) aim direction, at the target's range: the gun sits above the sight,
	// so a point far away would show too low for a close target.
	const double AimRange = Current ? FMath::Max(500.0, FVector::Dist(Targeting_->GetAimOrigin(), Current->GetTargetLocation())) : 100000.0;
	bool bCrosshairOnScreen = false;
	UTankSightMath::ProjectToSight(Sight, SightFieldOfView, Aspect, Targeting_->GetAimOrigin() + Targeting_->GetAimDirection() * AimRange, CrosshairUV, bCrosshairOnScreen);
	PlaceCentered(Crosshair, CrosshairUV, CrosshairSize);
	// Yellow while the turret is still swinging onto the target, red once the gun is laid.
	const FLinearColor LockColor = Targeting_->IsOnTarget() ? LockedColor : SelectableColor;
	SetRingColor(Crosshair, Current ? LockColor : (bAssist ? IdleColor : ManualColor));

	TArray<UTargetableComponent*> Targets;
	if (UTargetRegistrySubsystem* Registry = UTargetRegistrySubsystem::Get(Targeting_))
	{
		Registry->GetValidTargets(Targets);
	}
	const FVector Origin = Targeting_->GetAimOrigin();
	bool bCurrentOnScreen = false;
	int32 Used = 0;
	for (UTargetableComponent* Target : Targets)
	{
		if (Used >= Markers.Num() || Target->GetOwner() == Targeting_->GetOwner())
		{
			continue;
		}
		FVector2D UV;
		bool bOnScreen = false;
		UTankSightMath::ProjectToSight(Sight, SightFieldOfView, Aspect, Target->GetTargetLocation(), UV, bOnScreen);

		const bool bSelected = Target == Current;
		bCurrentOnScreen |= bSelected && bOnScreen;
		FLinearColor Color = InactiveColor;
		if (bSelected)
		{
			Color = bOnScreen ? LockedColor : LockedColor * 0.6f;
		}
		else if (bAssist && bOnScreen && Targeting_->IsTargetEligible(Target))
		{
			Color = SelectableColor;
		}

		UBorder* Marker = Markers[Used++];
		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
		SetRingColor(Marker, Color);
		PlaceCentered(Marker, UV, bSelected ? SelectedMarkerSize : (bOnScreen ? MarkerSize : MarkerSize * 0.6));
	}
	for (int32 Index = Used; Index < Markers.Num(); ++Index)
	{
		Markers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}

	LastStatus = FString::Printf(TEXT("AIM ASSIST: %s"), bAssist ? TEXT("ON") : TEXT("OFF"));
	if (Current)
	{
		const float Meters = static_cast<float>(FVector::Dist(Origin, Current->GetTargetLocation()) / 100.0);
		LastTargetLine = FString::Printf(TEXT("TARGET: %s  %.0f m%s%s"), *Current->GetTargetDisplayName().ToString(), Meters,
			bCurrentOnScreen ? TEXT("") : TEXT("  (OFF SCREEN)"), Targeting_->IsOnTarget() ? TEXT("") : TEXT("  LAYING GUN..."));
	}
	else if (!bAssist)
	{
		LastTargetLine = TEXT("MANUAL AIM");
	}
	else
	{
		LastTargetLine = Targets.Num() > 0 ? TEXT("NO TARGET IN VIEW") : TEXT("NO TARGET");
	}
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(LastStatus));
		StatusText->SetColorAndOpacity(FSlateColor(bAssist ? SelectableColor : ManualColor));
	}
	if (TargetText)
	{
		TargetText->SetText(FText::FromString(LastTargetLine));
		TargetText->SetColorAndOpacity(FSlateColor(Current ? LockedColor : IdleColor));
	}
}
