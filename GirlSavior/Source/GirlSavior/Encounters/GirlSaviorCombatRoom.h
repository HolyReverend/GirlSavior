#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavModifierComponent.h"
#include "GirlSaviorCombatRoom.generated.h"

class AGirlSaviorCombatRoom;
class UBoxComponent;
class UGirlSaviorWaveProfile;
class UStaticMeshComponent;

// Keep the modifier's exact door bounds independent of the collision export.
UCLASS()
class UGirlSaviorDoorNavModifier : public UNavModifierComponent
{
    GENERATED_BODY()

    virtual void CalculateBounds() const override;
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API AGirlSaviorCombatDoor : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatDoor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Door")
    TObjectPtr<UBoxComponent> Bounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Door")
    TObjectPtr<UStaticMeshComponent> Visual;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat Door")
    bool bStartsOpen = true;

    UFUNCTION(BlueprintPure, Category = "Combat Door")
    bool IsOpen() const { return bOpen; }

    UFUNCTION(BlueprintCallable, Category = "Combat Door")
    void SetOpen(bool bNewOpen);

    bool Crosses(const FVector& Start, const FVector& End, float Radius = 0.f) const;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere, Category = "Combat Door")
    TObjectPtr<UGirlSaviorDoorNavModifier> NavModifier;

    bool bOpen = true;
};

USTRUCT(BlueprintType)
struct FGirlSaviorRoomLink
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room Link")
    TObjectPtr<AGirlSaviorCombatRoom> Room;

    // A link with no door remains open.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room Link")
    TObjectPtr<AGirlSaviorCombatDoor> Door;
};

USTRUCT(BlueprintType)
struct FGirlSaviorWaveEntrance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Entrance")
    FName EntranceId = NAME_None;

    // Direction of entry toward the combat space, relative to the room actor.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Entrance")
    FVector ApproachDirection = FVector::ForwardVector;

    // Exact capsule-center transforms relative to the room actor.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave Entrance", meta = (MakeEditWidget))
    TArray<FTransform> Slots;
};

UCLASS(BlueprintType)
class GIRLSAVIOR_API AGirlSaviorCombatRoom : public AActor
{
    GENERATED_BODY()

public:
    AGirlSaviorCombatRoom();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat Room")
    TObjectPtr<UBoxComponent> Volume;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat Room")
    FName RoomId = NAME_None;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat Room")
    TObjectPtr<UGirlSaviorWaveProfile> Profile;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat Room")
    TArray<FGirlSaviorRoomLink> Neighbors;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Combat Room")
    TArray<FGirlSaviorWaveEntrance> Entrances;

    bool ContainsPoint(const FVector& WorldPoint) const;
    bool IsAdjacentTo(const AGirlSaviorCombatRoom& Other) const;
    bool HasOpenLinkTo(const AGirlSaviorCombatRoom& Other) const;
};
