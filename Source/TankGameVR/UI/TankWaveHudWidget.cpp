#include "UI/TankWaveHudWidget.h"
#include "Game/TankArenaDirector.h"
#include "Enemies/TankEnemyStats.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UTankWaveHudWidget::Initialize()
{
	const bool bResult = Super::Initialize();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
	return bResult;
}

UTextBlock* UTankWaveHudWidget::MakeText(FName Name, int32 Size)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = Size;
	Text->SetFont(Font);
	Text->SetColorAndOpacity(FSlateColor(TextColor));
	Text->SetJustification(ETextJustify::Center);
	return Text;
}

void UTankWaveHudWidget::BuildDefaultLayout()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(BackgroundColor);
	Panel->SetPadding(FMargin(22.0f, 8.0f));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Lines);

	WaveText = MakeText(TEXT("WaveText"), 26);
	TimerText = MakeText(TEXT("TimerText"), 20);
	EnemiesText = MakeText(TEXT("EnemiesText"), 14);
	for (UTextBlock* Text : { WaveText.Get(), TimerText.Get(), EnemiesText.Get() })
	{
		Lines->AddChildToVerticalBox(Text)->SetHorizontalAlignment(HAlign_Center);
	}

	// Top middle; the overview status panel sits top left.
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	PanelSlot->SetAlignment(FVector2D(0.5, 0.0));
	PanelSlot->SetPosition(FVector2D(0.0, 20.0));
	PanelSlot->SetAutoSize(true);
}

void UTankWaveHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateTexts();
}

void UTankWaveHudWidget::UpdateTexts()
{
	const ATankArenaDirector* Director = ATankArenaDirector::GetDirector(this);
	if (!Director)
	{
		WaveLine = TEXT("NO WAVES");
		TimerLine.Reset();
		EnemiesLine.Reset();
		bSoon = false;
	}
	else
	{
		const int32 Wave = Director->GetWave();
		WaveLine = Wave > 0 ? FString::Printf(TEXT("WAVE %d"), Wave) : FString(TEXT("GET READY"));

		const float Next = Director->GetTimeToNextWave();
		bSoon = Next >= 0.0f && Next <= SoonTime;
		if (Next >= 0.0f)
		{
			const int32 Seconds = FMath::CeilToInt(Next);
			TimerLine = FString::Printf(TEXT("NEXT WAVE %d:%02d"), Seconds / 60, Seconds % 60);
		}
		else
		{
			TimerLine.Reset();
		}

		const ETankArenaPhase Phase = Director->GetPhase();
		if (Phase == ETankArenaPhase::GameOver || Phase == ETankArenaPhase::Victory)
		{
			EnemiesLine = Director->GetMessage();
		}
		else
		{
			EnemiesLine = FString::Printf(TEXT("TANKS %d   CARS %d   AT GUNS %d   SCORE %d"),
				Director->CountEnemies(ETankEnemyType::Tank), Director->CountEnemies(ETankEnemyType::ArmoredCar),
				Director->CountEnemies(ETankEnemyType::AntiTank), Director->GetScore());
		}
	}

	if (WaveText)
	{
		WaveText->SetText(GetWaveText());
	}
	if (TimerText)
	{
		TimerText->SetText(GetTimerText());
		TimerText->SetColorAndOpacity(FSlateColor(bSoon ? SoonColor : TextColor));
		TimerText->SetVisibility(TimerLine.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (EnemiesText)
	{
		EnemiesText->SetText(GetEnemiesText());
	}
}

UTankWaveHudWidget* UTankWaveHudWidget::ShowWaveHud(const UObject* WorldContext, TSubclassOf<UTankWaveHudWidget> WidgetClass)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	APlayerController* Player = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!Player)
	{
		return nullptr;
	}
	if (!WidgetClass)
	{
		WidgetClass = StaticClass();
	}
	TArray<UUserWidget*> Shown;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Shown, WidgetClass, false);
	if (Shown.Num() > 0)
	{
		return Cast<UTankWaveHudWidget>(Shown[0]);
	}
	UTankWaveHudWidget* Hud = CreateWidget<UTankWaveHudWidget>(Player, WidgetClass);
	if (Hud)
	{
		Hud->SetVisibility(ESlateVisibility::HitTestInvisible);
		Hud->AddToViewport(5);
	}
	return Hud;
}
