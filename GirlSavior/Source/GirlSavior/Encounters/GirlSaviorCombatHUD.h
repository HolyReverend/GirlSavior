#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GirlSaviorCombatHUD.generated.h"

class AGirlSaviorCombatEncounter;
class UGameViewportClient;
class SButton;
class SWidget;
class FReply;
struct EVisibility;

UCLASS()
class GIRLSAVIOR_API AGirlSaviorCombatHUD : public AHUD
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    void FocusGameplayViewport();
    FText GetVitalsText() const;
    FText GetDashText() const;
    FText GetOutcomeTitle() const;
    FText GetOutcomeCause() const;
    EVisibility GetOutcomeVisibility() const;
    FReply HandleRetry();

    TWeakObjectPtr<AGirlSaviorCombatEncounter> Encounter;
    TWeakObjectPtr<UGameViewportClient> Viewport;
    TSharedPtr<SWidget> Overlay;
    TSharedPtr<SButton> RetryButton;
    bool bOutcomeFocused = false;
};
