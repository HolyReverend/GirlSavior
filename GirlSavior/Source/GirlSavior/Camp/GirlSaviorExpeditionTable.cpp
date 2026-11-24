#include "Camp/GirlSaviorExpeditionTable.h"

#include "Camp/GirlSaviorCampPlayerController.h"
#include "Camp/GirlSaviorCampPrototypeDefinition.h"

// The table is available once its mission catalog is assigned.
bool AGirlSaviorExpeditionTable::IsInteractionEnabled() const
{
    return Super::IsInteractionEnabled() && IsValid(Catalog);
}

// Dispatch through the controller, which owns the table visit and input focus.
void AGirlSaviorExpeditionTable::Interact(AGirlSaviorCampPlayerController& Controller) const
{
    if (IsValid(Catalog))
    {
        Controller.OpenExpeditionTable(*Catalog);
    }
}

// The prompt uses the same name as the map marker.
FText AGirlSaviorExpeditionTable::GetInteractionName() const
{
    return NSLOCTEXT("GirlSaviorCampInteraction", "TableName", "Стол экспедиций");
}
