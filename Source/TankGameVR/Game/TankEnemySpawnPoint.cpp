#include "Game/TankEnemySpawnPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"

ATankEnemySpawnPoint::ATankEnemySpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
#if WITH_EDITORONLY_DATA
	Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (Sprite)
	{
		Sprite->SetupAttachment(GetRootComponent());
	}
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(GetRootComponent());
		Arrow->ArrowColor = FColor(220, 60, 40);
		Arrow->ArrowSize = 3.0f;
	}
#endif
}
