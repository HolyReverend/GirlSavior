#pragma once

#include "Math/UnrealMathUtility.h"

// Ordered edges preserve short ready taps without buffering clicks through cooldown.
struct FGirlSaviorFireSchedule
{
	void Press(double Now)
	{
		if (!bHeld) bReadyPress = bReadyPress || Now >= NextShotGameTime;
		bHeld = true;
	}
	void Release() { bHeld = false; }
	// Cancellation deliberately preserves the interval already paid by the last shot.
	void Cancel() { bHeld = false; bReadyPress = false; }
	bool ConsumeDue(double Now)
	{
		const bool bPressed = bReadyPress;
		bReadyPress = false;
		return Now >= NextShotGameTime && (bHeld || bPressed);
	}
	void Launched(double Now, double Rate) { NextShotGameTime = Now + 1. / Rate; }
	double GetNextShotGameTime() const { return NextShotGameTime; }
	// A replacement pawn inherits the run's already-paid deadline.
	void PreserveDeadline(double Deadline) { NextShotGameTime = FMath::Max(NextShotGameTime, Deadline); }
private:
	double NextShotGameTime = 0.;
	bool bHeld = false;
	bool bReadyPress = false;
};
