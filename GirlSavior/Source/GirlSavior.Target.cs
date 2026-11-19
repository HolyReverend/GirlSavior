// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class GirlSaviorTarget : TargetRules
{
	public GirlSaviorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Match the installed UnrealGame target's rules for compiling the Tests folder.
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("GirlSavior");
	}
}
