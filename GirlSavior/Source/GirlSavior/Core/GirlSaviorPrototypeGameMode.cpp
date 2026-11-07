#include "Core/GirlSaviorPrototypeGameMode.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Core/GirlSaviorPlayerController.h"

AGirlSaviorPrototypeGameMode::AGirlSaviorPrototypeGameMode()
{
	DefaultPawnClass = AGirlSaviorCharacter::StaticClass();
	PlayerControllerClass = AGirlSaviorPlayerController::StaticClass();
}
