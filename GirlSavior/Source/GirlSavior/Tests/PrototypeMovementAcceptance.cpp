#if WITH_EDITOR
#include "Core/GirlSaviorPlayerController.h"
#include "Camera/GirlSaviorPlayerCameraManager.h"
#include "Characters/GirlSaviorCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace GirlSaviorMovementAcceptance
{
// Editor-only fixture: real key mappings and stock walking, sampled after movement.
struct FPhase
{
	const TCHAR* Name;
	float Duration;
	TArray<FKey> Keys;
	bool bReset;
};

class FRun
{
public:
	FRun(UWorld* InWorld, int32 InFPS)
		: World(InWorld), FPS(InFPS)
	{
		Controller = Cast<AGirlSaviorPlayerController>(InWorld->GetFirstPlayerController());
		Character = Controller.IsValid() ? Cast<AGirlSaviorCharacter>(Controller->GetPawn()) : nullptr;
		if (!Character.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("Stage1 acceptance requires a possessed prototype character in PIE."));
			return;
		}
		Phases = {
			{TEXT("settle"), .25f, {}, true},
			{TEXT("start"), .30f, {EKeys::W}, false},
			{TEXT("reverse"), .30f, {EKeys::S}, false},
			{TEXT("stop"), .25f, {}, false},
			{TEXT("right"), .30f, {EKeys::D}, true},
			{TEXT("left"), .30f, {EKeys::A}, true},
			{TEXT("diagonal"), .30f, {EKeys::W, EKeys::D}, true},
			{TEXT("cancel_forward"), .30f, {EKeys::W, EKeys::S, EKeys::D}, true},
			{TEXT("cancel_both"), .25f, {EKeys::W, EKeys::S, EKeys::A, EKeys::D}, true},
			{TEXT("slide"), 1.70f, {EKeys::W, EKeys::D}, true},
			{TEXT("head_on"), 1.20f, {EKeys::W}, true},
			{TEXT("escape"), .30f, {EKeys::S}, false},
			{TEXT("flush"), .20f, {EKeys::S}, false}
		};
		MaxFPS = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
		PreviousFPS = MaxFPS->GetFloat();
		MaxFPS->Set(static_cast<float>(FPS), ECVF_SetByConsole);
		CSV = TEXT("phase,dt,elapsed,input_x,input_y,velocity_x,velocity_y,position_x,position_y,position_z\n");
		PreHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FRun::PreTick);
		PostHandle = FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &FRun::PostTick);
		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FRun::Cleanup);
		bActive = true;
	}

	~FRun() { Stop(); }

	void Stop()
	{
		if (Controller.IsValid())
		{
			for (const FKey& Key : HeldKeys)
			{
				Send(Key, IE_Released);
			}
		}
		HeldKeys.Empty();
		FWorldDelegates::OnWorldPreActorTick.Remove(PreHandle);
		FWorldDelegates::OnWorldPostActorTick.Remove(PostHandle);
		FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		if (bActive && MaxFPS)
		{
			MaxFPS->Set(PreviousFPS, ECVF_SetByConsole);
		}
		bActive = false;
	}

private:
	void Send(const FKey& Key, EInputEvent Event)
	{
		Controller->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Key, Event, FPlatformTime::Cycles64()));
	}

	void PreTick(UWorld* TickWorld, ELevelTick, float DeltaTime)
	{
		if (!bActive || TickWorld != World.Get())
		{
			return;
		}
		if (!Character.IsValid() || !Controller.IsValid() || ++Frames > 2000)
		{
			Stop();
			return;
		}
		if (PhaseElapsed < 0.f)
		{
			const FPhase& Phase = Phases[PhaseIndex];
			for (const FKey& Key : HeldKeys)
			{
				if (!Phase.Keys.Contains(Key)) Send(Key, IE_Released);
			}
			for (const FKey& Key : Phase.Keys)
			{
				if (!HeldKeys.Contains(Key)) Send(Key, IE_Pressed);
			}
			HeldKeys = Phase.Keys;
			if (Phase.bReset)
			{
				UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
				Movement->StopMovementImmediately();
				Movement->SetMovementMode(MOVE_None);
				const AGirlSaviorPlayerCameraManager* Camera =
					CastChecked<AGirlSaviorPlayerCameraManager>(Controller->PlayerCameraManager);
				FVector Position(-450.f, -450.f, 90.f);
				if (PhaseIndex == 9 || PhaseIndex == 10)
				{
					Position = Camera->ToWorldMoveIntent(FVector2D(PhaseIndex == 9 ? -300.f : 0.f, 0.f));
					Position.Z = 90.f;
				}
				Character->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
				Movement->SetMovementMode(MOVE_Walking);
			}
			if (PhaseIndex == 12)
			{
				Controller->FlushPressedKeys();
			}
			PhaseElapsed = 0.f;
		}
	}

	void PostTick(UWorld* TickWorld, ELevelTick, float DeltaTime)
	{
		if (!bActive || TickWorld != World.Get() || !Character.IsValid())
		{
			return;
		}
		PhaseElapsed += DeltaTime;
		const FVector2D Intent = Controller->GetScreenMoveIntent();
		const FVector Velocity = Character->GetVelocity();
		const FVector Position = Character->GetActorLocation();
		CSV += FString::Printf(TEXT("%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),
			Phases[PhaseIndex].Name, DeltaTime, PhaseElapsed, Intent.X, Intent.Y,
			Velocity.X, Velocity.Y, Position.X, Position.Y, Position.Z);
		if (PhaseElapsed >= Phases[PhaseIndex].Duration)
		{
			++PhaseIndex;
			PhaseElapsed = -1.f;
			if (PhaseIndex == Phases.Num())
			{
				const FString Folder = FPaths::ProjectSavedDir() / TEXT("Stage1");
				IFileManager::Get().MakeDirectory(*Folder, true);
				const FString Path = Folder / FString::Printf(TEXT("Movement%d.csv"), FPS);
				const bool bSaved = FFileHelper::SaveStringToFile(CSV, *Path);
				UE_LOG(LogTemp, Display, TEXT("Stage1 movement capture: %s saved=%d frames=%d"), *Path, bSaved, Frames);
				Stop();
			}
		}
	}

	void Cleanup(UWorld* InWorld, bool, bool)
	{
		if (InWorld == World.Get()) Stop();
	}

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AGirlSaviorPlayerController> Controller;
	TWeakObjectPtr<AGirlSaviorCharacter> Character;
	TArray<FPhase> Phases;
	TArray<FKey> HeldKeys;
	FString CSV;
	FDelegateHandle PreHandle, PostHandle, CleanupHandle;
	IConsoleVariable* MaxFPS = nullptr;
	float PreviousFPS = 0.f;
	float PhaseElapsed = -1.f;
	int32 PhaseIndex = 0;
	int32 Frames = 0;
	int32 FPS;
	bool bActive = false;
};

static TUniquePtr<FRun> Run;
static FAutoConsoleCommandWithWorldAndArgs CaptureCommand(
	TEXT("GirlSavior.TestMovement"), TEXT("Capture Stage 1 PIE movement acceptance at 60 or 30 FPS."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->WorldType != EWorldType::PIE) return;
		const int32 FPS = Args.Num() ? FCString::Atoi(*Args[0]) : 60;
		if (FPS != 60 && FPS != 30) return;
		Run.Reset();
		Run = MakeUnique<FRun>(World, FPS);
	}));
}
#endif