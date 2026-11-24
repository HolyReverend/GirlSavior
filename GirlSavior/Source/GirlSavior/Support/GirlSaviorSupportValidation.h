#pragma once

#include "CoreMinimal.h"

namespace GirlSavior::SupportValidation
{
// Include the full object path so the same message identifies the asset in the editor and the log.
inline bool Fail(const UObject& Object, const FString& Field, const FText& Reason, FText& OutError)
{
    OutError = FText::Format(NSLOCTEXT("GirlSaviorSupportValidation", "AssetField", "{0} · {1}: {2}"),
        FText::FromString(Object.GetPathName()), FText::FromString(Field), Reason);
    return false;
}
}
