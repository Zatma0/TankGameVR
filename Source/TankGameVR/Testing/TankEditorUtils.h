#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TankEditorUtils.generated.h"

class UUserWidget;

// Editor helpers for the Tools/TankSetup scripts (things Python can't do).
UCLASS()
class TANKGAMEVR_API UTankEditorUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Adds a widget to a Widget Blueprint panel, styled like the panel's first button. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Editor")
	static bool AddWidgetToWidgetBlueprint(UObject* WidgetBlueprint, FName PanelName, TSubclassOf<UUserWidget> WidgetClass, FName NewWidgetName, bool bBeforeLast);

	/** Sets Swizzle (YXZ) / Negate modifiers on a key's mappings so they get saved with the asset. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Editor")
	static bool SetKeyMappingModifiers(UObject* MappingContext, FName KeyName, bool bSwizzleToY, bool bNegate);

	/** Name:Class of every widget in the tree. */
	UFUNCTION(BlueprintCallable, Category = "Tank|Editor")
	static TArray<FString> ListWidgetBlueprintWidgets(UObject* WidgetBlueprint);
};
