#include "Camp/GirlSaviorCampHeroine.h"

#include "Camp/GirlSaviorCampPlayerController.h"
#include "Support/GirlSaviorHeroineDefinition.h"

// Keep unnamed or unauthored placeholders out of the interaction picker.
bool AGirlSaviorCampHeroine::IsInteractionEnabled() const
{
    return Super::IsInteractionEnabled() && IsValid(HeroineDefinition)
        && !HeroineDefinition->DisplayName.IsEmptyOrWhitespace();
}

// Pass the same profile used to name the target to the read-only panel.
void AGirlSaviorCampHeroine::Interact(AGirlSaviorCampPlayerController& Controller) const
{
    if (IsValid(HeroineDefinition))
    {
        Controller.OpenHeroineInfo(*HeroineDefinition);
    }
}

// Keep the prompt in sync with the localized heroine name.
FText AGirlSaviorCampHeroine::GetInteractionName() const
{
    return IsValid(HeroineDefinition) ? HeroineDefinition->DisplayName : FText::GetEmpty();
}
