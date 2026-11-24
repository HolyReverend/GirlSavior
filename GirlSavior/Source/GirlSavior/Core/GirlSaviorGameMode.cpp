#include "Core/GirlSaviorGameMode.h"
#include "Characters/GirlSaviorCharacter.h"
#include "Core/GirlSaviorPlayerController.h"

AGirlSaviorGameMode::AGirlSaviorGameMode()
{
	DefaultPawnClass = AGirlSaviorCharacter::StaticClass();
	PlayerControllerClass = AGirlSaviorPlayerController::StaticClass();
}
