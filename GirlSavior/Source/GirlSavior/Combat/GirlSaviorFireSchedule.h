#pragma once

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
private:
	double NextShotGameTime = 0.;
	bool bHeld = false;
	bool bReadyPress = false;
};
