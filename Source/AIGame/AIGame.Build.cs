// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AIGame : ModuleRules
{
	public AIGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// 폴더를 넘는 include를 모듈 루트 기준 경로("Shop/ShopProgress.h")로 쓰려고 모듈 루트를 탐색 경로에 넣는다. 같은 폴더끼리는 이름만 쓴다.
		// BuildSettingsVersion.V5에서는 레거시 경로가 꺼져(ModuleRules.cs 1418행) 모듈 루트가 기본 경로에 없다(UEBuildModuleCPP.cs 259행)
		PrivateIncludePaths.Add(ModuleDirectory);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
