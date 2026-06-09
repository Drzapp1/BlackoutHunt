// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHGameMode.h"
#include "HAL/IConsoleManager.h"
#include "BHAlarmTrap.h"
#include "BHAtmosphereDirector.h"
#include "BHAmbientEmitter.h"
#include "BHBatteryPickup.h"
#include "BHCollectable.h"
#include "BHBreakableGlassPane.h"
#include "BHEscapeStationManager.h"
#include "BHBlockActor.h"
#include "BHBotController.h"
#include "BHBotPolicySubsystem.h"
#include "BHBreaker.h"
#include "BHCCTVZone.h"
#include "BHCharacter.h"
#include "BHCrawlSpaceVolume.h"
#include "BHDoor.h"
#include "BHExitGate.h"
#include "BHFlickerLight.h"
#include "BHFootstepSurfaceVolume.h"
#include "BHGameInstance.h"
#include "BHGameState.h"
#include "BHGameSettings.h"
#include "BHHUD.h"
#include "BHJumpscareMonster.h"
#include "BHJumpscarePresentation.h"
#include "BHJumpscareVariantLibrary.h"
#include "BHLessonPreset.h"
#include "BHLevelMarker.h"
#include "BHLocker.h"
#include "BHTutorialDirector.h"
#include "BHNoiseDecoy.h"
#include "BHObjectiveStation.h"
#include "BHPanicAlarm.h"
#include "BHPowerupLibrary.h"
#include "BHPowerSwitch.h"
#include "BHPlayerController.h"
#include "BHPlayerState.h"
#include "BHPowerupShopTerminal.h"
#include "BHRevisionQuestionBank.h"
#include "BHRuntimeMeshPropActor.h"
#include "BHSecurityCamera.h"
#include "BHSecurityMonitor.h"
#include "BHSecurityShutter.h"
#include "BHSecurityTerminal.h"
#include "BHSlidingGate.h"
#include "BHStaticBlockField.h"
#include "BHStudentScareSwitch.h"
#include "BHTrainActivityStation.h"
#include "BHTrainBonusQuestionTerminal.h"
#include "BHTrainDisplayActor.h"
#include "BHTrainDoor.h"
#include "BHRoofStarfield.h"
#include "BHTrainIntermissionManager.h"
#include "BHTrainRoofBreaker.h"
#include "BHTrainRoofHatch.h"
#include "BHTrainRoofParkourGate.h"
#include "BHTrainServiceLight.h"
#include "BHTrainBlackjackTable.h"
#include "BHTrainChessTable.h"
#include "BHTrainTicTacToeTable.h"
#include "BHTrainConnectFourTable.h"
#include "BHTrainSlotMachine.h"
#include "BHTrainDartboard.h"
#include "BHTrainCat.h"
#include "BHTrainAquarium.h"
#include "BHTrainDanceFloor.h"
#include "BHTrainJukebox.h"
#include "BHTrainGhost.h"
#include "BHTrainSecretSwitch.h"
#include "BHTrainFireplace.h"
#include "BHTrainOthelloTable.h"
#include "BHTrainVendingMachine.h"
#include "BHTrainToyTrain.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "BHTrainTunnelMotionActor.h"
#include "BHTrainSeat.h"
#include "BHTrainStopCord.h"
#include "Components/BoxComponent.h"
#include "Components/BrushComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LocalFogVolumeComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LocalFogVolume.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "AI/Navigation/NavigationDirtyArea.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

#include <initializer_list>

namespace
{
float CenterZForBlockBottom(float BottomZ, float ScaleZ)
{
	return BottomZ + ScaleZ * 50.0f;
}

float CenterZForBlockTop(float TopZ, float ScaleZ)
{
	return TopZ - ScaleZ * 50.0f;
}

int32 CountActiveRevisionStudents(const AGameStateBase* GameState, bool bHumansOnly)
{
	if (!GameState)
	{
		return 0;
	}

	int32 Count = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		const bool bRevisionParticipant = BHPS
			&& BHPS->LifeState == EBHPlayerLifeState::Alive
			&& ABHGameMode::IsRevisionParticipantRole(BHPS->PlayerRole);
		if (bRevisionParticipant && (!bHumansOnly || !BHPS->IsABot()))
		{
			++Count;
		}
	}
	return Count;
}

bool IsRevisionParticipantState(const ABHPlayerState* BHPS)
{
	return BHPS
		&& BHPS->LifeState == EBHPlayerLifeState::Alive
		&& ABHGameMode::IsRevisionParticipantRole(BHPS->PlayerRole);
}

uint8 ObjectiveBeatRoleBit(EBHPlayerRole Role)
{
	const int32 RoleIndex = static_cast<int32>(Role);
	return RoleIndex >= 0 && RoleIndex < 8 ? static_cast<uint8>(1u << RoleIndex) : 0;
}

uint8 ObjectiveBeatRoleMask(std::initializer_list<EBHPlayerRole> Roles)
{
	uint8 Mask = 0;
	for (const EBHPlayerRole Role : Roles)
	{
		Mask |= ObjectiveBeatRoleBit(Role);
	}
	return Mask;
}

FLinearColor AvatarColorForIndex(int32 Index)
{
	static const FLinearColor Palette[] = {
		FLinearColor(0.22f, 0.58f, 0.74f, 1.0f),
		FLinearColor(0.78f, 0.46f, 0.18f, 1.0f),
		FLinearColor(0.46f, 0.72f, 0.28f, 1.0f),
		FLinearColor(0.70f, 0.30f, 0.42f, 1.0f),
		FLinearColor(0.52f, 0.44f, 0.86f, 1.0f),
		FLinearColor(0.84f, 0.75f, 0.24f, 1.0f),
		FLinearColor(0.28f, 0.68f, 0.62f, 1.0f),
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f),
		// Hidden prestige tints (indices 8..17) -- must match the ShirtColor entries in BHCosmeticUnlocks.cpp
		// and the identical palettes in BHCharacter/BHPlayerController.
		FLinearColor(0.86f, 0.90f, 0.82f, 1.0f), // Chalk
		FLinearColor(0.95f, 0.22f, 0.62f, 1.0f), // Arcade
		FLinearColor(0.18f, 0.92f, 0.45f, 1.0f), // Exit Sign
		FLinearColor(0.42f, 0.86f, 1.00f, 1.0f), // Afterimage
		FLinearColor(0.64f, 0.44f, 0.22f, 1.0f), // Veteran
		FLinearColor(0.40f, 0.12f, 0.20f, 1.0f), // Faculty
		FLinearColor(0.55f, 0.86f, 0.92f, 1.0f), // Slipstream
		FLinearColor(0.80f, 0.20f, 0.18f, 1.0f), // Detention
		FLinearColor(0.52f, 0.10f, 0.14f, 1.0f), // Apex
		FLinearColor(0.40f, 0.54f, 0.56f, 1.0f), // Commuter
		FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)  // Black (index 18; recolour-only)
	};

	return Palette[FMath::Abs(Index) % UE_ARRAY_COUNT(Palette)];
}

bool BHSoftObjectPathExists(const FSoftObjectPath& ObjectPath)
{
	if (ObjectPath.IsNull())
	{
		return false;
	}

	const FString PackageName = ObjectPath.GetLongPackageName();
	return !PackageName.IsEmpty() && FPackageName::DoesPackageExist(PackageName);
}

bool BHJumpscareVariantHasExistingAudio(const FBHJumpscareVariant& Variant)
{
	return BHSoftObjectPathExists(Variant.LaunchSound.ToSoftObjectPath());
}

bool BHJumpscareVariantHasProxyVisual(const FBHJumpscareVariant& Variant)
{
	return Variant.VisualActorClass.IsNull() && Variant.SkeletalMesh.IsNull() && Variant.StaticMesh.IsNull();
}

bool BHJumpscareVariantHasExistingVisual(const FBHJumpscareVariant& Variant)
{
	return BHJumpscareVariantHasProxyVisual(Variant)
		|| BHSoftObjectPathExists(Variant.VisualActorClass.ToSoftObjectPath())
		|| BHSoftObjectPathExists(Variant.SkeletalMesh.ToSoftObjectPath())
		|| BHSoftObjectPathExists(Variant.StaticMesh.ToSoftObjectPath());
}

bool BHIsFabJumpscarePackVariant(const FBHJumpscareVariant& Variant)
{
	const FString VariantId = Variant.VariantId.ToString();
	return VariantId.Equals(TEXT("FabMonster01"), ESearchCase::IgnoreCase)
		|| VariantId.Equals(TEXT("FabMonster02"), ESearchCase::IgnoreCase)
		|| VariantId.Equals(TEXT("FabMonster03"), ESearchCase::IgnoreCase);
}

bool BHIsLegacyScp096JumpscareVariant(const FBHJumpscareVariant& Variant)
{
	return Variant.VariantId.ToString().Equals(TEXT("SCP096"), ESearchCase::IgnoreCase);
}

constexpr float BHJumpscareMinViewDot = 0.50f;
constexpr float BHJumpscareSpawnCapsuleRadius = 62.0f;
constexpr float BHJumpscareSpawnCapsuleHalfHeight = 142.0f;

// Radius around a question node within which a revision participant counts as part of that node's
// co-located answer team (see BuildRevisionAnswerTeam). Comfortably larger than the ~3.75 m answer
// reach so everyone who could press 1-4 at the node is on the team, but local enough that students
// across the level are not pulled into a vote they can't see.
constexpr float BHRevisionAnswerTeamRadius = 900.0f;

bool BHResolveJumpscareView(ABHCharacter* Target, ABHPlayerController* TargetPC, FVector& OutViewLocation, FVector& OutViewForward)
{
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (TargetPC)
	{
		TargetPC->GetPlayerViewPoint(OutViewLocation, ViewRotation);
	}
	else if (Target)
	{
		OutViewLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 95.0f);
		ViewRotation = Target->GetActorRotation();
	}
	else
	{
		return false;
	}

	OutViewForward = ViewRotation.Vector().GetSafeNormal();
	if (OutViewForward.IsNearlyZero() && Target)
	{
		OutViewForward = Target->GetActorForwardVector().GetSafeNormal();
	}
	if (OutViewForward.IsNearlyZero())
	{
		OutViewForward = FVector::ForwardVector;
	}
	return true;
}

bool BHJumpscareFocusInsideViewCone(const FVector& ViewLocation, const FVector& ViewForward, const FVector& FocusLocation)
{
	const FVector ToFocus = (FocusLocation - ViewLocation).GetSafeNormal();
	if (ToFocus.IsNearlyZero() || ViewForward.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(ViewForward.GetSafeNormal(), ToFocus) >= BHJumpscareMinViewDot;
}

bool BHJumpscareTraceToFocusClear(UWorld* World, const FVector& ViewLocation, const FVector& FocusLocation, const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	return !World->LineTraceSingleByChannel(Hit, ViewLocation, FocusLocation, ECC_Visibility, Params);
}

bool BHJumpscareSpawnSpaceClear(UWorld* World, const FVector& SpawnLocation, const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}

	const FCollisionShape Shape = FCollisionShape::MakeCapsule(BHJumpscareSpawnCapsuleRadius, BHJumpscareSpawnCapsuleHalfHeight);
	return !World->OverlapBlockingTestByChannel(SpawnLocation + FVector(0.0f, 0.0f, BHJumpscareSpawnCapsuleHalfHeight), FQuat::Identity, ECC_WorldStatic, Shape, Params);
}

bool BHJumpscareChargePathClear(UWorld* World, const FVector& SpawnLocation, const FVector& TargetLocation, float PathRadius, const FCollisionQueryParams& Params)
{
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(24.0f, PathRadius));
	return !World->SweepSingleByChannel(Hit,
		SpawnLocation + FVector(0.0f, 0.0f, 108.0f),
		TargetLocation + FVector(0.0f, 0.0f, 108.0f),
		FQuat::Identity,
		ECC_Visibility,
		Shape,
		Params);
}

bool BHValidateVisibleJumpscareCandidate(UWorld* World, const FVector& ViewLocation, const FVector& ViewForward, const FVector& TargetLocation, const FVector& CandidateLocation, float FocusHeight, float PathRadius, const FCollisionQueryParams& Params, float& OutScore)
{
	const FVector FocusLocation = CandidateLocation + FVector(0.0f, 0.0f, FMath::Clamp(FocusHeight, 80.0f, 320.0f));
	if (!BHJumpscareFocusInsideViewCone(ViewLocation, ViewForward, FocusLocation))
	{
		return false;
	}
	if (!BHJumpscareTraceToFocusClear(World, ViewLocation, FocusLocation, Params))
	{
		return false;
	}
	if (!BHJumpscareSpawnSpaceClear(World, CandidateLocation, Params))
	{
		return false;
	}
	if (!BHJumpscareChargePathClear(World, CandidateLocation, TargetLocation, PathRadius, Params))
	{
		return false;
	}

	const float Distance = FVector::Dist2D(TargetLocation, CandidateLocation);
	const float ViewDot = FVector::DotProduct(ViewForward.GetSafeNormal(), (FocusLocation - ViewLocation).GetSafeNormal());
	OutScore = Distance + ViewDot * 1600.0f;
	return true;
}

ABHJumpscareMonster* BHSpawnScriptedJumpscareStage(UWorld* World, AActor* Owner, const FBHJumpscareVariant& Variant, const TArray<FVector>& PathPoints, float Speed, float Lifetime, AActor* LookAtTarget = nullptr, bool bFaceLookAtTarget = false, bool bPlayChargeEffects = true)
{
	if (!World || PathPoints.Num() < 2)
	{
		return nullptr;
	}

	const FVector InitialDirection = (PathPoints[1] - PathPoints[0]).GetSafeNormal2D();
	const FRotator SpawnRotation = InitialDirection.IsNearlyZero() ? FRotator::ZeroRotator : InitialDirection.Rotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHJumpscareMonster* Monster = World->SpawnActor<ABHJumpscareMonster>(PathPoints[0], SpawnRotation, SpawnParams);
	if (Monster)
	{
		Monster->ConfigureScriptedPath(PathPoints, Speed, Lifetime, LookAtTarget, bFaceLookAtTarget, bPlayChargeEffects);
		Monster->ConfigureVariant(Variant);
	}
	return Monster;
}

FVector BHResolveSpawnLocation(const UWorld* World, const FVector& DesiredLocation, int32 PlayerIndex, const AActor* IgnoreActor = nullptr)
{
	if (!World)
	{
		return DesiredLocation;
	}

	static const FVector2D Rings[] = {
		FVector2D(0.0f, 0.0f),
		FVector2D(260.0f, 0.0f),
		FVector2D(-260.0f, 0.0f),
		FVector2D(0.0f, 260.0f),
		FVector2D(0.0f, -260.0f),
		FVector2D(260.0f, 260.0f),
		FVector2D(-260.0f, 260.0f),
		FVector2D(260.0f, -260.0f),
		FVector2D(-260.0f, -260.0f),
		FVector2D(520.0f, 0.0f),
		FVector2D(-520.0f, 0.0f),
		FVector2D(0.0f, 520.0f),
		FVector2D(0.0f, -520.0f)
	};

	const int32 StartOffset = FMath::Abs(PlayerIndex) % UE_ARRAY_COUNT(Rings);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(42.0f, 98.0f);
	// IgnoreActor lets a caller exclude one pawn from the overlap test. The fresh-spawn callers pass nullptr (the
	// pawn isn't placed yet, so there's nothing to ignore). The cabin-reset caller passes the pawn being moved so
	// the resolve doesn't treat the player's OWN current capsule as a blocker -- otherwise, on the lobby train
	// (where pawns still block ECC_Pawn) an already-centred O press would be nudged sideways off the exact centre.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BHResolveSpawnLocation), false);
	if (IgnoreActor)
	{
		QueryParams.AddIgnoredActor(IgnoreActor);
	}
	auto IsClear = [&](const FVector& P) -> bool
	{
		// Tests against pawns that have ALREADY spawned (and any world geometry that blocks the Pawn channel, e.g.
		// train walls/furniture), so sequential joins stack OUTWARD instead of on top of each other or inside a
		// prop. (Two players who modulo to the same SurvivorSpawns index still get separated here.)
		return !World->OverlapBlockingTestByChannel(P, FQuat::Identity, ECC_Pawn, Capsule, QueryParams);
	};

	// 1) The fixed close-in ring first: keeps everyone in the intended cluster when there is room.
	for (int32 Attempt = 0; Attempt < UE_ARRAY_COUNT(Rings); ++Attempt)
	{
		const FVector2D Offset2D = Rings[(StartOffset + Attempt) % UE_ARRAY_COUNT(Rings)];
		const FVector Candidate = DesiredLocation + FVector(Offset2D.X, Offset2D.Y, 0.0f);
		if (IsClear(Candidate))
		{
			return Candidate;
		}
	}

	// 2) Ring full -> spiral outward in widening rings until a genuinely clear spot is found. This is the real
	//    guard against "everyone spawned on the same point and the physics exploded": we never knowingly return an
	//    occupied transform. Up to ~20 m out is ample even for a full 32-player lobby. Each ring's start angle is
	//    offset by the player index so two players resolving at once probe different spokes first.
	for (int32 Ring = 2; Ring <= 8; ++Ring)
	{
		const float Radius = 260.0f * static_cast<float>(Ring);
		const int32 Spokes = 8 + Ring * 4;
		const float StartAngle = (static_cast<float>(PlayerIndex) * 0.61803f + static_cast<float>(Ring) * 0.27f) * 2.0f * PI;
		for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
		{
			const float Angle = StartAngle + (2.0f * PI * static_cast<float>(Spoke)) / static_cast<float>(Spokes);
			const FVector Candidate = DesiredLocation + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
			if (IsClear(Candidate))
			{
				return Candidate;
			}
		}
	}

	// 3) Everything within 20 m is boxed in (pathological). Last resort: a per-player vertical + lateral stagger so
	//    no two pawns can ever share the exact transform and interpenetrate -- a brief drop-in is far better than
	//    two fused capsules, which is what hitches/crashes the physics.
	const float LateralStagger = static_cast<float>((PlayerIndex % 6) - 3) * 110.0f;
	const float HeightStagger = 220.0f + static_cast<float>(PlayerIndex % 12) * 95.0f;
	return DesiredLocation + FVector(LateralStagger, -LateralStagger, HeightStagger);
}

FString CompassFromDelta(const FVector& Delta)
{
	if (Delta.IsNearlyZero())
	{
		return TEXT("nearby");
	}

	const float AbsX = FMath::Abs(Delta.X);
	const float AbsY = FMath::Abs(Delta.Y);
	if (AbsX > AbsY * 1.65f)
	{
		return Delta.X >= 0.0f ? TEXT("east") : TEXT("west");
	}
	if (AbsY > AbsX * 1.65f)
	{
		return Delta.Y >= 0.0f ? TEXT("north") : TEXT("south");
	}

	const FString NS = Delta.Y >= 0.0f ? TEXT("north") : TEXT("south");
	const FString EW = Delta.X >= 0.0f ? TEXT("east") : TEXT("west");
	return FString::Printf(TEXT("%s-%s"), *NS, *EW);
}

FString NormalizeBHLevelName(FString LevelName)
{
	LevelName.TrimStartAndEndInline();
	if (LevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase) || LevelName.Equals(TEXT("Fog"), ESearchCase::IgnoreCase))
	{
		return TEXT("Foggrounds");
	}
	if (LevelName.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		return TEXT("Tutorial");
	}
	return LevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? TEXT("Substation") : TEXT("Facility");
}

bool IsTrueOption(const FString& Value)
{
	return Value.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("yes"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("bot"), ESearchCase::IgnoreCase)
		|| Value.Equals(TEXT("bots"), ESearchCase::IgnoreCase);
}

FString RoundPhaseToUrlValue(EBHRoundPhase Phase)
{
	switch (Phase)
	{
	case EBHRoundPhase::SurvivorsWin:
		return TEXT("SurvivorsWin");
	case EBHRoundPhase::HunterWin:
		return TEXT("HunterWin");
	default:
		return TEXT("None");
	}
}

EBHRoundPhase RoundPhaseFromUrlValue(const FString& Value)
{
	if (Value.Equals(TEXT("SurvivorsWin"), ESearchCase::IgnoreCase))
	{
		return EBHRoundPhase::SurvivorsWin;
	}
	if (Value.Equals(TEXT("HunterWin"), ESearchCase::IgnoreCase))
	{
		return EBHRoundPhase::HunterWin;
	}
	return EBHRoundPhase::Lobby;
}

EBHFogPreset ParseFogPreset(const FString& Value, EBHFogPreset DefaultPreset)
{
	if (Value.Equals(TEXT("Light"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Low"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Light;
	}
	if (Value.Equals(TEXT("Extreme"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Max"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Extreme;
	}
	if (Value.Equals(TEXT("Heavy"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("Medium"), ESearchCase::IgnoreCase))
	{
		return EBHFogPreset::Heavy;
	}
	return DefaultPreset;
}

FString FogPresetToString(EBHFogPreset Preset)
{
	switch (Preset)
	{
	case EBHFogPreset::Light:
		return TEXT("Light");
	case EBHFogPreset::Extreme:
		return TEXT("Extreme");
	case EBHFogPreset::Heavy:
	default:
		return TEXT("Heavy");
	}
}

EBHBotDifficulty ParseBotDifficulty(const FString& Value, EBHBotDifficulty DefaultDifficulty)
{
	if (Value.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Easy;
	}
	if (Value.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Hard;
	}
	if (Value.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		return EBHBotDifficulty::Normal;
	}
	return DefaultDifficulty;
}

FString BotDifficultyToString(EBHBotDifficulty Difficulty)
{
	switch (Difficulty)
	{
	case EBHBotDifficulty::Easy:
		return TEXT("Easy");
	case EBHBotDifficulty::Hard:
		return TEXT("Hard");
	case EBHBotDifficulty::Normal:
	default:
		return TEXT("Normal");
	}
}

EBHRevisionDifficultyMix ParseRevisionDifficultyMix(const FString& Value, EBHRevisionDifficultyMix DefaultMix)
{
	if (Value.Equals(TEXT("Easy"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Easy;
	}
	if (Value.Equals(TEXT("Hard"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Hard;
	}
	if (Value.Equals(TEXT("Adaptive"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Adaptive;
	}
	if (Value.Equals(TEXT("Balanced"), ESearchCase::IgnoreCase))
	{
		return EBHRevisionDifficultyMix::Balanced;
	}
	return DefaultMix;
}

FString RevisionDifficultyMixToString(EBHRevisionDifficultyMix Mix)
{
	switch (Mix)
	{
	case EBHRevisionDifficultyMix::Easy:
		return TEXT("Easy");
	case EBHRevisionDifficultyMix::Hard:
		return TEXT("Hard");
	case EBHRevisionDifficultyMix::Balanced:
		return TEXT("Balanced");
	case EBHRevisionDifficultyMix::Adaptive:
	default:
		return TEXT("Adaptive");
	}
}

FString SpectatorRolePreferenceToString(EBHPlayerRole Role)
{
	switch (ABHGameMode::SanitizeSpectatorRolePreference(Role))
	{
	case EBHPlayerRole::Hunter:
		return TEXT("Teacher");
	case EBHPlayerRole::Survivor:
		return TEXT("Survivor");
	case EBHPlayerRole::FakeHunter:
		return TEXT("Hall Monitor");
	case EBHPlayerRole::Unassigned:
	default:
		return TEXT("No preference");
	}
}

int32 ReportTopicIndex(EBHPhysicsTopic Topic)
{
	return FMath::Clamp(static_cast<int32>(Topic), 0, 3);
}

float RevisionTopicMastery(const FBHPlayerRevisionStats& Stats, EBHPhysicsTopic Topic)
{
	switch (Topic)
	{
	case EBHPhysicsTopic::ForcesAndMotion:
		return Stats.ForcesMastery;
	case EBHPhysicsTopic::Electricity:
		return Stats.ElectricityMastery;
	case EBHPhysicsTopic::Waves:
		return Stats.WavesMastery;
	case EBHPhysicsTopic::Energy:
		return Stats.EnergyMastery;
	default:
		return 0.0f;
	}
}

void SetRevisionTopicMastery(FBHPlayerRevisionStats& Stats, EBHPhysicsTopic Topic, float Value)
{
	const float Clamped = FMath::Clamp(Value, 0.0f, 100.0f);
	switch (Topic)
	{
	case EBHPhysicsTopic::ForcesAndMotion:
		Stats.ForcesMastery = Clamped;
		break;
	case EBHPhysicsTopic::Electricity:
		Stats.ElectricityMastery = Clamped;
		break;
	case EBHPhysicsTopic::Waves:
		Stats.WavesMastery = Clamped;
		break;
	case EBHPhysicsTopic::Energy:
		Stats.EnergyMastery = Clamped;
		break;
	default:
		break;
	}
}

// Overall mastery is the mean of the ENABLED topics' mastery (RevisionTopicMask). Disabled
// topics are excluded so a class focused on one topic is not penalised for the others.
float MeanEnabledTopicMastery(const FBHPlayerRevisionStats& Stats, int32 TopicMask)
{
	float Sum = 0.0f;
	int32 Count = 0;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(Index);
		if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
		{
			continue;
		}
		Sum += RevisionTopicMastery(Stats, Topic);
		++Count;
	}
	if (Count <= 0)
	{
		// No enabled topics in the mask (shouldn't happen) — fall back to all four.
		return (Stats.ForcesMastery + Stats.ElectricityMastery + Stats.WavesMastery + Stats.EnergyMastery) * 0.25f;
	}
	return Sum / static_cast<float>(Count);
}

// True if the student still has an unresolved missed question queued for the given topic.
// Used to hold a topic's mastery below the review ceiling until the student clears it.
bool HasOutstandingReviewInTopic(const ABHPlayerState* BHPS, EBHPhysicsTopic Topic)
{
	if (!BHPS)
	{
		return false;
	}
	for (const FString& QueuedId : BHPS->RevisionReviewQueue)
	{
		FBHRevisionQuestion Queued;
		if (FBHRevisionQuestionBank::FindQuestion(QueuedId, Queued) && Queued.Topic == Topic)
		{
			return true;
		}
	}
	return false;
}

EBHPhysicsTopic WeakestTopicForStats(const FBHPlayerRevisionStats& Stats, int32 TopicMask)
{
	EBHPhysicsTopic WeakTopic = EBHPhysicsTopic::ForcesAndMotion;
	float WeakScore = TNumericLimits<float>::Max();
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
		if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
		{
			continue;
		}

		const float Score = RevisionTopicMastery(Stats, Topic);
		if (Score < WeakScore)
		{
			WeakScore = Score;
			WeakTopic = Topic;
		}
	}
	return WeakTopic;
}

EBHPhysicsTopic StrongestTopicForStats(const FBHPlayerRevisionStats& Stats, int32 TopicMask)
{
	EBHPhysicsTopic StrongTopic = EBHPhysicsTopic::ForcesAndMotion;
	float StrongScore = -1.0f;
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
		if ((TopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
		{
			continue;
		}

		const float Score = RevisionTopicMastery(Stats, Topic);
		if (Score > StrongScore)
		{
			StrongScore = Score;
			StrongTopic = Topic;
		}
	}
	return StrongTopic;
}

// Difficulty is judged against DecisionMastery — the mastery of the *specific topic the next
// question will target* — not the student's cross-topic average. A student who is strong overall
// but weak in one topic should still be eased into that weak topic rather than thrown a Hard
// question on it. Callers on the live path pass the targeted topic's mastery; report/follow-up
// callers that only have a synthesized overall figure pass that instead.
EBHQuestionDifficulty AdaptiveDifficultyForStats(const FBHPlayerRevisionStats& Stats, float DecisionMastery, bool bLastAnswerCorrect)
{
	if (Stats.Attempts <= 0)
	{
		return EBHQuestionDifficulty::Easy;
	}
	if (!bLastAnswerCorrect && DecisionMastery < 55.0f)
	{
		return EBHQuestionDifficulty::Easy;
	}
	if (DecisionMastery >= 78.0f && bLastAnswerCorrect)
	{
		return EBHQuestionDifficulty::Hard;
	}
	if (DecisionMastery >= 45.0f)
	{
		return EBHQuestionDifficulty::Medium;
	}
	return EBHQuestionDifficulty::Easy;
}

FString CsvEscape(const FString& Input)
{
	FString Value = Input;
	Value.ReplaceInline(TEXT("\r"), TEXT(" "));
	Value.ReplaceInline(TEXT("\n"), TEXT(" "));
	Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
	return FString::Printf(TEXT("\"%s\""), *Value);
}

FString MarkdownInline(FString Input)
{
	Input.ReplaceInline(TEXT("\r"), TEXT(" "));
	Input.ReplaceInline(TEXT("\n"), TEXT(" "));
	Input.ReplaceInline(TEXT("|"), TEXT("\\|"));
	Input.TrimStartAndEndInline();
	return Input.IsEmpty() ? FString(TEXT("Not recorded")) : Input;
}

FString ReportDurationText(int32 Seconds)
{
	Seconds = FMath::Max(0, Seconds);
	const int32 Minutes = Seconds / 60;
	const int32 RemainderSeconds = Seconds % 60;
	if (Minutes <= 0)
	{
		return FString::Printf(TEXT("%d sec"), Seconds);
	}
	if (RemainderSeconds <= 0)
	{
		return FString::Printf(TEXT("%d min"), Minutes);
	}
	return FString::Printf(TEXT("%d min %d sec"), Minutes, RemainderSeconds);
}

FString SanitizeReportToken(FString Token)
{
	Token.TrimStartAndEndInline();
	if (Token.IsEmpty())
	{
		return TEXT("Classroom");
	}

	for (int32 Index = 0; Index < Token.Len(); ++Index)
	{
		TCHAR& Character = Token[Index];
		const bool bSafe = FChar::IsAlnum(Character) || Character == TCHAR('_') || Character == TCHAR('-');
		if (!bSafe)
		{
			Character = TCHAR('_');
		}
	}
	return Token;
}

int32 ParsePhysicsTopicMask(const FString& Value, int32 DefaultMask)
{
	if (Value.IsEmpty() || Value.Equals(TEXT("All"), ESearchCase::IgnoreCase))
	{
		return 0x0F;
	}
	if (Value.IsNumeric())
	{
		return FMath::Clamp(FCString::Atoi(*Value), 1, 0x0F);
	}

	int32 Mask = 0;
	TArray<FString> Parts;
	Value.ParseIntoArray(Parts, TEXT(","), true);
	if (Parts.Num() == 0)
	{
		Parts.Add(Value);
	}
	for (FString Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (Part.Equals(TEXT("Forces"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("Motion"), ESearchCase::IgnoreCase) || Part.Equals(TEXT("ForcesAndMotion"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::ForcesAndMotion);
		}
		else if (Part.Equals(TEXT("Electricity"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Electricity);
		}
		else if (Part.Equals(TEXT("Waves"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Waves);
		}
		else if (Part.Equals(TEXT("Energy"), ESearchCase::IgnoreCase))
		{
			Mask |= FBHRevisionQuestionBank::TopicMaskBit(EBHPhysicsTopic::Energy);
		}
	}
	return Mask == 0 ? DefaultMask : (Mask & 0x0F);
}

FString BotIntentToString(EBHBotIntent Intent)
{
	const UEnum* Enum = StaticEnum<EBHBotIntent>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Intent)) : TEXT("Unknown");
}

FString BotStimulusToString(EBHBotStimulusType Type)
{
	const UEnum* Enum = StaticEnum<EBHBotStimulusType>();
	return Enum ? Enum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Unknown");
}

FString TelemetryRoleName(const ABHPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return TEXT("");
	}

	const UEnum* Enum = StaticEnum<EBHPlayerRole>();
	FString RoleName = Enum ? Enum->GetNameStringByValue(static_cast<int64>(PlayerState->PlayerRole)) : TEXT("Unknown");
	if (PlayerState->IsABot())
	{
		RoleName += TEXT("Bot");
	}
	return RoleName;
}

FString TelemetryRoundPhaseName(const ABHGameState* GameState)
{
	const UEnum* Enum = StaticEnum<EBHRoundPhase>();
	return (Enum && GameState) ? Enum->GetNameStringByValue(static_cast<int64>(GameState->RoundPhase)) : TEXT("Unknown");
}

FString TelemetryLocationKey(const FVector& Location)
{
	const int32 X = FMath::RoundToInt(Location.X / 100.0f);
	const int32 Y = FMath::RoundToInt(Location.Y / 100.0f);
	const int32 Z = FMath::RoundToInt(Location.Z / 100.0f);
	return FString::Printf(TEXT("%d:%d:%d"), X, Y, Z);
}

const TCHAR* SelectPromptLine(const TCHAR* const* Lines, int32 LineCount, int32 Salt)
{
	if (!Lines || LineCount <= 0)
	{
		return TEXT("");
	}

	// Unsigned modulo: FMath::Abs(INT32_MIN) is UB and stays negative, which would index out of bounds.
	return Lines[static_cast<int32>(static_cast<uint32>(Salt) % static_cast<uint32>(LineCount))];
}

int32 BHVariationSeedForLevel(const FString& LevelName, int32 StageIndex, int32 RoundSeed)
{
	const int32 StableRoundSeed = RoundSeed != 0 ? RoundSeed : 24681357;
	return StableRoundSeed ^ (static_cast<int32>(GetTypeHash(LevelName)) * 31) ^ (StageIndex + 1) * 7919;
}
}

ABHGameMode::ABHGameMode()
{
	PlayerControllerClass = ABHPlayerController::StaticClass();
	PlayerStateClass = ABHPlayerState::StaticClass();
	GameStateClass = ABHGameState::StaticClass();
	DefaultPawnClass = ABHCharacter::StaticClass();
	HUDClass = ABHHUD::StaticClass();

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	MinPlayers = FMath::Max(1, Settings->MinPlayers);
	// Clamp to the engine-valid connection range so the GameMode's own login cap can never exceed the
	// AGameSession / EOS caps (both clamped to 64). Keeps all three sources of "max players" consistent.
	MaxPlayers = FMath::Clamp(FMath::Max(MinPlayers, Settings->MaxPlayers), 2, 64);
	PrepSeconds = FMath::Max(0, Settings->PrepSeconds);
	bExtendedFirstWarmup = Settings->bExtendedFirstWarmup;
	ExtendedPrepSeconds = FMath::Max(0, Settings->ExtendedPrepSeconds);
	bHasHostedClassBefore = Settings->bHasHostedClassBefore;
	HuntSeconds = FMath::Max(60, Settings->HuntSeconds);
	RequiredBreakers = FMath::Max(1, Settings->RequiredBreakers);
	bAllowHostForceStart = Settings->bAllowHostForceStart;
	HunterSpawn = FVector(-4050.0f, 0.0f, 120.0f);
	RuntimeLevelName = TEXT("Facility");
	NextRuntimeLevelName = TEXT("Facility");
	RuntimeFogPreset = EBHFogPreset::Heavy;
	NextFogPreset = EBHFogPreset::Heavy;
	bFogPresetOverride = false;
	bFacilityBuilt = false;
	RoundSeed = 0;
	TargetHunterCount = 1;
	ObjectiveIntensity = 2;
	ActiveBreakerCount = RequiredBreakers;
	ActiveSideObjectiveCount = 0;
	bInfectionMode = false;
	bPartyPace = false;
	bPracticeMode = false;
	bTutorialMode = false;
	bTestMode = false;
	RevisionMode = EBHRevisionMode::None;
	bRevisionMode = false;
	RevisionTopicMask = 0x0F;
	RevisionDifficultyMix = EBHRevisionDifficultyMix::Adaptive;
	RevisionClassThreshold = Settings->RevisionClassThreshold;
	RevisionIndividualThreshold = Settings->RevisionIndividualThreshold;
	RevisionRoundDuration = FMath::Max(60, Settings->RevisionRoundSeconds);
	RevisionScareIntensity = FMath::Clamp(Settings->RevisionScareIntensity, 0, 3);
	RevisionReviewTimeRemaining = 0;
	bRevisionReportExported = false;
	bBotMode = false;
	TargetBotCount = 0;
	BotDifficulty = Settings->DefaultBotDifficulty;
	PracticeRoundModifier = EBHRoundModifier::None;
	NoiseRadiusMultiplier = 1.0f;
	LastDirectorScareTime = -999.0f;
	LastMonsterChargeTime = -999.0f;
	LastColdCallTime = -999.0f;
	LastDecoyHandoutTime = -999.0f;
	LastPresenceWhisperTime = -999.0f;
	LastWhisperJumpscareTime = -999.0f;
	LastPresenceSpikeTime = -999.0f;
	LastTeacherCounterScareTime = -999.0f;
	LastObjectiveDangerBeatTime = -999.0f;
	LastSpectatorEncouragementTime = -999.0f;
	LastBotNoiseLocation = FVector::ZeroVector;
	LastBotNoiseTime = -9999.0f;
	bRuntimeNavigationReady = false;
	bTrainIntermissionLevel = false;
	bLobbyLevel = false;
	LobbyFirstLevelName.Reset();
	RuntimeStageIndex = 0;
	RevisionContributionGateTarget = 1;
	PendingIntermissionResult = EBHRoundPhase::Lobby;
	RuntimeNavBounds = nullptr;
	TrainIntermissionManager = nullptr;
	StaticBlockField = nullptr;
	StaticBlockFields.Reset();
	AtmosphereDirector = nullptr;
}

void ABHGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		AtmosphereDirector = NewObject<UBHAtmosphereDirector>(this, TEXT("AtmosphereDirector"));
		if (AtmosphereDirector)
		{
			AtmosphereDirector->Initialize(this);
		}
	}

	BuildRuntimeFacility();

	if (HasAuthority() && GetWorld())
	{
		GetWorldTimerManager().SetTimer(VoidRecoveryTimerHandle, this, &ABHGameMode::RecoverPlayersFromVoid, 0.5f, true, 0.5f);
	}

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(bTrainIntermissionLevel ? EBHRoundPhase::Intermission : EBHRoundPhase::Lobby);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, RequiredBreakers);
		BHGS->SetSideObjectiveCounts(0, 0);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		BHGS->SetDirectorState(RoundSeed, bTrainIntermissionLevel ? TEXT("Board the subway, review the class recap, and prepare for the next stop.") : TEXT("Reach the lobby and ready up."), NextRuntimeLevelName);
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, EBHRoundModifier::None);
		BHGS->SetFogOptions(NextFogPreset, bFogPresetOverride);
		BHGS->SetActiveFogPreset(RuntimeFogPreset);
		BHGS->SetActiveLevelName(RuntimeLevelName);
		BHGS->SetPresenceState(0.0f, TEXT("The building is listening."), 0);
		PublishObjectiveBeats();
		BHGS->SetPracticeMode(bPracticeMode);
		BHGS->SetTestMode(bTestMode);
		// Prop Hunt: replicate the mode flag early so clients' HUDs switch to the prop-hunt readout from the lobby on.
		BHGS->SetPropHuntState(bPropHuntMode, 0, 0, 0.0f);
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		RefreshRevisionContributionGateTarget();
		BHGS->SetRevisionSummary(0.0f, EBHPhysicsTopic::ForcesAndMotion, 0, TEXT(""));
	}

	if (bRevisionMode)
	{
		FString BankSummary;
		const bool bBankValid = FBHRevisionQuestionBank::Validate(BankSummary);
		UE_LOG(LogTemp, Log, TEXT("%s"), *BankSummary);
		if (!bBankValid)
		{
			BroadcastStatus(TEXT("Physics question bank validation failed. Check logs."), 8.0f);
		}
	}

	if (bBotMode)
	{
		RefreshBotRoster(nullptr);
	}

	const FString ForceHuntOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHForceHunt="), TEXT("")) : FString();
	if (!bPracticeMode && !bTestMode && IsTrueOption(ForceHuntOption))
	{
		FTimerDelegate ForceHuntDelegate;
		ForceHuntDelegate.BindWeakLambda(this, [this]()
		{
			if (ABHGameState* BHGS = GetGameState<ABHGameState>())
			{
				if (BHGS->RoundPhase == EBHRoundPhase::Lobby || BHGS->RoundPhase == EBHRoundPhase::Prep)
				{
					StartHuntPhaseImmediately();
				}
			}
		});
		FTimerHandle ForceHuntHandle;
		GetWorldTimerManager().SetTimer(ForceHuntHandle, ForceHuntDelegate, 1.0f, false);
	}

	// Arriving from the train lobby: kick off the normal role-warmup (Prep) on load instead of re-entering a
	// Lobby phase on the hunt map (the social waiting already happened on the train).
	const FString AutoPrepOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHAutoPrep="), TEXT("")) : FString();
	if (!bPracticeMode && !bTestMode && IsTrueOption(AutoPrepOption))
	{
		FTimerDelegate AutoPrepDelegate;
		AutoPrepDelegate.BindWeakLambda(this, [this]()
		{
			if (const ABHGameState* BHGS = GetGameState<ABHGameState>())
			{
				if (BHGS->RoundPhase == EBHRoundPhase::Lobby)
				{
					StartPrepPhase();
				}
			}
		});
		FTimerHandle AutoPrepHandle;
		GetWorldTimerManager().SetTimer(AutoPrepHandle, AutoPrepDelegate, 1.0f, false);
	}

	const FString NavCheckOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHRunBotNavCheck="), TEXT("")) : FString();
	if (IsTrueOption(NavCheckOption))
	{
		FTimerDelegate NavCheckDelegate;
		NavCheckDelegate.BindWeakLambda(this, [this]()
		{
			FString Summary;
			RunBotNavCheck(Summary);
		});
		FTimerHandle NavCheckHandle;
		GetWorldTimerManager().SetTimer(NavCheckHandle, NavCheckDelegate, 1.75f, false);
	}

	const FString ReportOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHBotReport="), TEXT("")) : FString();
	if (IsTrueOption(ReportOption))
	{
		FTimerDelegate ReportDelegate;
		ReportDelegate.BindWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Log, TEXT("%s"), *GetBotMemoryReport());
		});
		FTimerHandle ReportHandle;
		GetWorldTimerManager().SetTimer(ReportHandle, ReportDelegate, 10.0f, true, 5.0f);
	}
}

void ABHGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(RoundTimerHandle);
		GetWorldTimerManager().ClearTimer(DirectorTimerHandle);
		GetWorldTimerManager().ClearTimer(VoidRecoveryTimerHandle);
		GetWorldTimerManager().ClearTimer(FinalEscapeFallbackTimerHandle);
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

FString ABHGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Result = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	// Capture the secret reconnect token the client echoed in its join URL (if any) BEFORE PostLogin's
	// reconnect check runs, so TryGetReconnectProgress can match on it. A blank token (first join, or a
	// client that lost it on a crash / manual rejoin) leaves this empty and PostLogin issues a fresh one.
	if (ABHPlayerState* BHPS = NewPlayerController ? NewPlayerController->GetPlayerState<ABHPlayerState>() : nullptr)
	{
		BHPS->ReconnectToken = UGameplayStatics::ParseOption(Options, TEXT("BHReconnect"));
	}
	return Result;
}

void ABHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const bool bCanJoinRound = bPracticeMode || bTestMode || !BHGS || BHGS->RoundPhase == EBHRoundPhase::Lobby;
	bool bReconnected = false;

	if (ABHPlayerState* BHPS = NewPlayer ? NewPlayer->GetPlayerState<ABHPlayerState>() : nullptr)
	{
		const int32 PlayerIndex = GameState ? FMath::Max(0, GameState->PlayerArray.IndexOfByKey(BHPS)) : 0;
		const FString HumanRoleOption = GetWorld() ? GetWorld()->URL.GetOption(TEXT("BHHumanRole="), TEXT("")) : FString();
		EBHPlayerRole RequestedRole = bTestMode ? EBHPlayerRole::Tester : ((!bRevisionMode && bBotMode) ? EBHPlayerRole::Survivor : EBHPlayerRole::Unassigned);
		if (!bTestMode && (HumanRoleOption.Equals(TEXT("Hunter"), ESearchCase::IgnoreCase) || HumanRoleOption.Equals(TEXT("Teacher"), ESearchCase::IgnoreCase)))
		{
			RequestedRole = EBHPlayerRole::Hunter;
		}
		else if (!bTestMode && (HumanRoleOption.Equals(TEXT("FakeHunter"), ESearchCase::IgnoreCase) || HumanRoleOption.Equals(TEXT("Monitor"), ESearchCase::IgnoreCase)))
		{
			RequestedRole = EBHPlayerRole::FakeHunter;
		}
		const bool bLateJoinSpectator = !bPracticeMode && !bTestMode && !bCanJoinRound;

		// Mid-round reconnect: a student who dropped during an active round and returns within the
		// grace window is restored to their exact role/state (a caught student returns as their
		// Hall Monitor/captured state, an active survivor/teacher returns to that role) instead of
		// being forced to spectate until the next lobby.
		FBHTravelPlayerProgress ReconnectProgress;
		if (bLateJoinSpectator)
		{
			const UBHGameSettings* RejoinSettings = GetDefault<UBHGameSettings>();
			const float GraceSeconds = RejoinSettings ? RejoinSettings->ReconnectGraceSeconds : 0.0f;
			const float NowServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
			{
				bReconnected = GraceSeconds > 0.0f
					&& BHGI->TryGetReconnectProgress(BHPS, NowServerTime, GraceSeconds, ReconnectProgress);
			}
		}

		BHPS->SetReady(bPracticeMode || bTestMode || bReconnected);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetAvatarIndex(PlayerIndex);
		BHPS->SetAvatarColor(AvatarColorForIndex(PlayerIndex));
		BHPS->SetMapVote(TEXT(""));
		BHPS->ClearFogPresetVote();
		BHPS->ClearSpectatorSupportState();

		if (bReconnected)
		{
			BHPS->SetRole(ReconnectProgress.PlayerRole);
			BHPS->SetDesiredRole(ReconnectProgress.DesiredRole);
			BHPS->SetSpectatorRolePreference(ReconnectProgress.SpectatorRolePreference);
			BHPS->SetLifeState(ReconnectProgress.LifeState);
			BHPS->SetFakeHunterEligible(ReconnectProgress.bFakeHunterEligible);
			BHPS->RevisionStats = ReconnectProgress.RevisionStats;
			BHPS->QuestionPoints = FMath::Max(0, ReconnectProgress.QuestionPoints);
			BHPS->LifetimeQuestionPoints = FMath::Max(BHPS->QuestionPoints, ReconnectProgress.LifetimeQuestionPoints);
			BHPS->HunterPoints = FMath::Max(0, ReconnectProgress.HunterPoints);
			BHPS->LifetimeHunterPoints = FMath::Max(BHPS->HunterPoints, ReconnectProgress.LifetimeHunterPoints);
			BHPS->Powerups = ReconnectProgress.Powerups;
			for (FBHPowerupInventoryEntry& Entry : BHPS->Powerups)
			{
				Entry.CooldownEndServerTime = 0.0f;
			}

			// Reconnected into an ALREADY-RESOLVED round (EndRound latched a *Win phase, within its ~8s
			// pre-travel window). Don't put a restored-Alive survivor back in play as a capturable target
			// in a decided round; mark them out of play. (An Escaped/Captured outcome is preserved.) Banked
			// points/stats restored above still carry to the next round.
			const ABHGameState* ResolvedCheckBHGS = GetGameState<ABHGameState>();
			if (ResolvedCheckBHGS
				&& (ResolvedCheckBHGS->RoundPhase == EBHRoundPhase::HunterWin || ResolvedCheckBHGS->RoundPhase == EBHRoundPhase::SurvivorsWin))
			{
				if (BHPS->LifeState == EBHPlayerLifeState::Alive)
				{
					BHPS->SetLifeState(EBHPlayerLifeState::Captured);
				}
				if (ABHPlayerController* ResultPC = Cast<ABHPlayerController>(NewPlayer))
				{
					ResultPC->ClientRecordRoundResult(BHPS->PlayerRole, BHPS->LifeState, ResolvedCheckBHGS->RoundPhase);
				}
			}
			if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
			{
				BHGI->ClearReconnectMark(BHPS);
			}
		}
		else
		{
			BHPS->SetDesiredRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode || bLateJoinSpectator ? EBHPlayerRole::Survivor : RequestedRole));
			BHPS->SetRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode ? EBHPlayerRole::Survivor : (bCanJoinRound ? EBHPlayerRole::Unassigned : EBHPlayerRole::Spectator)));
			BHPS->SetLifeState(bCanJoinRound ? EBHPlayerLifeState::Alive : EBHPlayerLifeState::Captured);
			if (bRevisionMode && !bTrainIntermissionLevel)
			{
				BHPS->ResetRevisionStats();
			}
			RestorePlayersAfterTravel(NewPlayer);
		}

		// Issue a fresh per-client reconnect token to anyone who didn't arrive with one (first join, or a
		// post-ServerTravel relogin whose engine-driven travel URL carries no token). It's pushed to the
		// owning client, which echoes it on a later rejoin so the mid-round reconnect above is matched by
		// this unguessable token, never the spoofable display name. A successful grace reconnect already
		// presented a matching token, so leave it untouched.
		if (BHPS->ReconnectToken.IsEmpty())
		{
			BHPS->ReconnectToken = FGuid::NewGuid().ToString(EGuidFormats::Digits);
			if (ABHPlayerController* TokenPC = Cast<ABHPlayerController>(NewPlayer))
			{
				TokenPC->ClientReceiveReconnectToken(BHPS->ReconnectToken);
			}
		}
	}

	if (bBotMode)
	{
		TrimBotRosterToCapacity();
	}

	if (GameState && GameState->PlayerArray.Num() > MaxPlayers)
	{
		UE_LOG(LogTemp, Warning, TEXT("BHGameMode: rejected join, class is full (%d/%d). Returning player to menu."),
			GameState->PlayerArray.Num(), MaxPlayers);
		if (ABHPlayerController* FullBHPC = Cast<ABHPlayerController>(NewPlayer))
		{
			// Best-effort: tell the bounced student why before they travel back to the menu.
			FullBHPC->ClientShowStatusMessage(
				FString::Printf(TEXT("This class is full (%d/%d). Returning you to the menu."), MaxPlayers, MaxPlayers),
				4.0f);
		}
		NewPlayer->ClientTravel(TEXT("/Engine/Maps/Entry"), TRAVEL_Absolute);
		return;
	}

	RestartPlayer(NewPlayer);

	if (bTestMode && !bTrainIntermissionLevel)
	{
		StartTestMode(Cast<ABHPlayerController>(NewPlayer));
	}
	else if (bPracticeMode)
	{
		StartPracticeMode(Cast<ABHPlayerController>(NewPlayer));
	}
	else if (bBotMode)
	{
		RefreshBotRoster(Cast<ABHPlayerController>(NewPlayer));
	}

	if (bReconnected)
	{
		if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(NewPlayer))
		{
			BHPC->ClientShowStatusMessage(TEXT("Reconnected to your round. Your role and progress were restored."), 6.0f);
		}
		if (const ABHPlayerState* BHPS = NewPlayer ? NewPlayer->GetPlayerState<ABHPlayerState>() : nullptr)
		{
			BroadcastStatus(FString::Printf(TEXT("%s reconnected to the round."), *BHPS->GetPlayerName()), 3.5f);
		}
	}
	else if (!bCanJoinRound)
	{
		if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(NewPlayer))
		{
			BHPC->ClientShowStatusMessage(TEXT("Round already started. Spectator support is available: H encourages the team; T/Y/U request next-round roles."), 8.0f);
		}
	}
}

void ABHGameMode::Logout(AController* Exiting)
{
	// Purge bot AI references to the departing player before the engine tears down
	// their controller/pawn. The arrays hold weak pointers so they would not crash,
	// but stale claims/cooldowns/stimuli can briefly misdirect bot planning (e.g. a
	// bot holding a chase claim on a player who already left). Clear them now so bots
	// re-plan immediately instead of waiting for the entries to expire.
	const AActor* ExitingPawn = Exiting ? Exiting->GetPawn() : nullptr;
	auto MatchesExiting = [Exiting, ExitingPawn](const TWeakObjectPtr<AActor>& Actor)
	{
		const AActor* Resolved = Actor.Get();
		return Resolved && (Resolved == ExitingPawn || Resolved->GetInstigatorController() == Exiting);
	};
	BotObjectiveClaims.RemoveAll([Exiting, &MatchesExiting](const FBHBotObjectiveClaim& Claim)
	{
		return !Claim.Claimant.IsValid() || Claim.Claimant.Get() == Exiting || MatchesExiting(Claim.Target);
	});
	BotTargetCooldowns.RemoveAll([Exiting, &MatchesExiting](const FBHBotTargetCooldown& Cooldown)
	{
		return !Cooldown.Claimant.IsValid() || Cooldown.Claimant.Get() == Exiting || MatchesExiting(Cooldown.Target);
	});
	BotWorldStimuli.RemoveAll([&MatchesExiting](const FBHBotStimulus& Stimulus)
	{
		return MatchesExiting(Stimulus.SourceActor) || MatchesExiting(Stimulus.TargetActor);
	});

	// Mid-round reconnect: snapshot the leaving player's state (before the engine detaches it in
	// Super::Logout) so they can rejoin their role within the grace window instead of being forced
	// to spectate until the next lobby.
	if (const UBHGameSettings* Settings = GetDefault<UBHGameSettings>())
	{
		const ABHGameState* BHGS = GetGameState<ABHGameState>();
		const bool bRoundActive = BHGS
			&& BHGS->RoundPhase != EBHRoundPhase::Lobby
			&& BHGS->RoundPhase != EBHRoundPhase::SurvivorsWin
			&& BHGS->RoundPhase != EBHRoundPhase::HunterWin;
		if (!bPracticeMode && !bTestMode && bRoundActive && Settings->ReconnectGraceSeconds > 0.0f)
		{
			if (ABHPlayerState* BHPS = Exiting ? Exiting->GetPlayerState<ABHPlayerState>() : nullptr)
			{
				if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
				{
					const float NowServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
					BHGI->MarkTravelPlayerLeftForReconnect(BHPS, NowServerTime);
				}
			}
		}
	}

	// Drop the leaving player's spectator-encouragement cooldown entry so the map does not grow
	// unbounded over a long-lived listen server, and a recycled APlayerState address can't inherit
	// a stale cooldown.
	if (const APlayerState* ExitingPS = Exiting ? Exiting->GetPlayerState<APlayerState>() : nullptr)
	{
		SpectatorEncouragementTimes.Remove(TObjectKey<APlayerState>(ExitingPS));
	}

	// Same rationale for the host-admin denial throttle keyed by controller.
	if (const APlayerController* ExitingPC = Cast<APlayerController>(Exiting))
	{
		HostAdminDenialReplyTimes.Remove(TObjectKey<APlayerController>(ExitingPC));
	}

	Super::Logout(Exiting);

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		const bool bRoundActive = BHGS->RoundPhase == EBHRoundPhase::Hunt
			|| BHGS->RoundPhase == EBHRoundPhase::FinalEscape;
		if (!bPracticeMode && !bTestMode && bRoundActive)
		{
			if (CountAliveSurvivors() <= 0)
			{
				// Don't hand the Teacher an instant win if the survivor(s) who just dropped still have a live
				// reconnect window — a transient classroom Wi-Fi blip can knock out the last survivors together.
				// Defer: if they don't return, the Hunt round timer (or the final-escape window) still resolves
				// the round, so this can only delay a HunterWin, never prevent a legitimate one.
				const UBHGameSettings* GraceSettings = GetDefault<UBHGameSettings>();
				const float GraceSeconds = GraceSettings ? GraceSettings->ReconnectGraceSeconds : 0.0f;
				const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
				const int32 PendingSurvivors = (BHGI && GraceSeconds > 0.0f) ? BHGI->CountReconnectableAliveSurvivors(GraceSeconds) : 0;
				if (PendingSurvivors <= 0)
				{
					// Evacuate-together: an already-escaped survivor (LifeState Escaped, so not counted as
					// "alive") still wins the round for the class. Mirror the Hunt-tick and standard-capture
					// resolution rather than handing the Teacher a win the class actually earned.
					EndRound(CountEscapedSurvivors() > 0 ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
				}
				else
				{
					BroadcastStatus(TEXT("Survivor connection dropped - holding the round for a reconnect."), 4.0f);
				}
			}
			else if (CountAliveHunters() <= 0)
			{
				// The last hunter (human or bot) disconnected, so no one can capture the survivors.
				// Resolve in the survivors' favor now instead of letting the match hang until the round
				// timer expires — which would otherwise wrongly credit the (now absent) hunters with a win.
				EndRound(EBHRoundPhase::SurvivorsWin);
			}
		}
	}
}

void ABHGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		ExistingPawn->Destroy();
	}

	RestartPlayerAtTransform(NewPlayer, GetSpawnTransformFor(NewPlayer));
}

void ABHGameMode::SetPlayerReady(ABHPlayerController* Controller, bool bReady)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
	if (bTestMode)
	{
		if (BHPS)
		{
			BHPS->SetReady(true);
			BHPS->SetRole(EBHPlayerRole::Tester);
			BHPS->SetDesiredRole(EBHPlayerRole::Tester);
			BHPS->SetLifeState(EBHPlayerLifeState::Alive);
			BHPS->SetHiddenInLocker(false);
		}
		if (bTrainIntermissionLevel)
		{
			return;
		}
		if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
		{
			StartTestMode(Controller);
		}
		else if (Controller)
		{
			Controller->ClientShowStatusMessage(TEXT("Test Round is already open. No ready-up needed."), 2.5f);
		}
		return;
	}

	if (!BHGS || !BHPS || BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		return;
	}

	BHPS->SetReady(bReady);

	if (AreAllReady())
	{
		// From the train lobby, the round starts by departing to the first hunt level (the warmup happens there);
		// on a normal hunt map it warms up in place.
		if (bLobbyLevel)
		{
			TravelFromLobbyToFirstHunt();
		}
		else
		{
			StartPrepPhase();
		}
	}
	else if (Controller)
	{
		const int32 PlayerCount = GameState ? GameState->PlayerArray.Num() : 0;
		if (PlayerCount < MinPlayers)
		{
			Controller->ClientShowStatusMessage(FString::Printf(TEXT("Ready set. Waiting for at least %d players."), MinPlayers), 3.0f);
		}
		else
		{
			Controller->ClientShowStatusMessage(TEXT("Ready set. Waiting for every player to press Enter."), 3.0f);
		}
	}
}

void ABHGameMode::NotifyBreakerRepaired(const FVector& BreakerLocation)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || (BHGS->bExitUnlocked && !bTestMode))
	{
		return;
	}

	const int32 ActiveRequiredBreakers = FMath::Max(1, BHGS->BreakersRequired);
	const int32 NewCompleted = FMath::Min(BHGS->BreakersCompleted + 1, ActiveRequiredBreakers);
	BHGS->SetBreakerCounts(NewCompleted, ActiveRequiredBreakers);
	RecordPlaytestTelemetryMarker(TEXT("breaker_repaired"), BreakerLocation, FString::Printf(TEXT("%d/%d"), NewCompleted, ActiveRequiredBreakers));
	ReportBotStimulus(EBHBotStimulusType::Objective, BreakerLocation, nullptr, nullptr, TEXT("breaker repaired"), 1.2f);
	ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Power, BreakerLocation, nullptr, nullptr, 1.15f, TEXT("breaker repaired"));
	BroadcastStatus(FString::Printf(TEXT("Breaker repaired: %d/%d."), NewCompleted, ActiveRequiredBreakers), 3.0f);
	if (!FlickerLights.IsEmpty() && FMath::FRand() < 0.35f)
	{
		if (ABHFlickerLight* Light = FlickerLights[FMath::RandRange(0, FlickerLights.Num() - 1)])
		{
			Light->TriggerFlickerBurst(2.8f, 1.75f, FLinearColor(0.92f, 0.68f, 0.32f, 1.0f), 14.0f);
		}
	}
	ApplyPresenceSpike(BreakerLocation, 54.0f + NewCompleted * 4.0f, TEXT("The Shape noticed the power coming back."));
	UpdateExitUnlockState();
	PublishObjectiveBeats();
}

void ABHGameMode::NotifyObjectiveStationCompleted(ABHObjectiveStation* Station)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || (BHGS->bExitUnlocked && !bTestMode) || !Station || !Station->IsDirectorActive())
	{
		return;
	}

	const int32 RequiredSideObjectives = FMath::Max(0, BHGS->SideObjectivesRequired);
	const int32 NewCompleted = FMath::Min(BHGS->SideObjectivesCompleted + 1, RequiredSideObjectives);
	BHGS->SetSideObjectiveCounts(NewCompleted, RequiredSideObjectives);
	const UEnum* StationEnum = StaticEnum<EBHObjectiveStationType>();
	const FString StationType = StationEnum ? StationEnum->GetNameStringByValue(static_cast<int64>(Station->GetStationType())) : TEXT("Station");
	RecordPlaytestTelemetryMarker(TEXT("objective_completed"), Station->GetActorLocation(), FString::Printf(TEXT("%s %d/%d"), *StationType, NewCompleted, RequiredSideObjectives));
	ReportBotStimulus(EBHBotStimulusType::Objective, Station->GetActorLocation(), Station, Station, TEXT("completed station"), 1.3f);
	ReportAtmosphereStimulus(EBHAtmosphereStimulusType::Objective, Station->GetActorLocation(), Station, Station, 1.25f, TEXT("completed station"));
	BroadcastStatus(FString::Printf(TEXT("Side objective complete: %d/%d."), NewCompleted, RequiredSideObjectives), 3.5f);
	NotifyLoudNoise(Station->GetActorLocation(), TEXT("completed station"));
	ApplyPresenceSpike(Station->GetActorLocation(), 58.0f + NewCompleted * 5.0f, TEXT("The next task made too much noise."));
	if (bRevisionMode)
	{
		RevisionReviewTimeRemaining = 60;
		UpdateRevisionSummary(FString::Printf(TEXT("Stage review %d/%d: discuss the explanation, exam method, and weakest topic before moving."), NewCompleted, RequiredSideObjectives));
	}
	// Never fire a scare from answering a station question in the tutorial (a teaching beat). Today this is also
	// blocked because party-pace/panic are off in the tutorial, but guard it explicitly so a future modifier change
	// can't leak a scare into the lesson.
	if ((bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge) && !bTutorialMode)
	{
		TriggerScareEvent();
	}
	UpdateExitUnlockState();
	PublishObjectiveBeats();
}

void ABHGameMode::NotifySurvivorCaptured(ABHCharacter* Survivor, ABHCharacter* CapturingHunter)
{
	if (!Survivor)
	{
		return;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	ABHPlayerState* SurvivorPS = Survivor->GetPlayerState<ABHPlayerState>();
	ABHPlayerState* CapturingHunterPS = CapturingHunter ? CapturingHunter->GetPlayerState<ABHPlayerState>() : nullptr;
	AController* SurvivorController = Survivor->GetController();
	if (bPracticeMode || bTestMode)
	{
		const FVector CaptureLocation = Survivor->GetActorLocation();
		// A scripted AI Hunter tagging an AI student (e.g. the Monitor tutorial's Teacher catching the Student)
		// is just background scene for the human to mislead - don't spike their presence meter or announce a
		// "capture registered" they didn't make. Human-driven practice captures still get the confirmation below.
		const bool bAICapture = CapturingHunterPS && CapturingHunterPS->IsABot();
		RecordPlaytestTelemetryMarker(TEXT("capture"), CaptureLocation, bTestMode ? TEXT("test") : TEXT("practice"), CapturingHunterPS, SurvivorPS);
		Survivor->MarkCaptured();
		if (!bAICapture)
		{
			ApplyPresenceSpike(CaptureLocation, 70.0f, bTestMode ? TEXT("Test capture registered.") : TEXT("Practice capture registered."));
		}
		if (SurvivorPS)
		{
			SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
			SurvivorPS->SetHiddenInLocker(false);
			if (bTestMode)
			{
				SurvivorPS->SetRole(EBHPlayerRole::Tester);
				SurvivorPS->SetDesiredRole(EBHPlayerRole::Tester);
			}
		}
		if (SurvivorController)
		{
			RestartPlayer(SurvivorController);
		}
		if (!bAICapture)
		{
			BroadcastStatus(bTestMode ? TEXT("Test capture registered. The round stays open.") : TEXT("Practice capture registered. The round stays open."), 3.5f);
		}
		return;
	}

	if (bInfectionMode && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && SurvivorPS && SurvivorPS->IsAliveSurvivor())
	{
		RecordPlaytestTelemetryMarker(TEXT("capture"), Survivor->GetActorLocation(), TEXT("infection"), CapturingHunterPS, SurvivorPS);
		SurvivorPS->SetRole(EBHPlayerRole::Hunter);
		SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
		SurvivorPS->SetHiddenInLocker(false);
		BroadcastStatus(FString::Printf(TEXT("%s was sent to detention and joined the Teacher."), *SurvivorPS->GetPlayerName()), 4.0f);
		RestartPlayer(Survivor->GetController());

		if (CountAliveSurvivors() <= 0)
		{
			// Evacuate-together: a survivor who already escaped still wins for the class even if the last
			// inside survivor is infected here. Mirror the standard capture path's resolution below.
			EndRound(CountEscapedSurvivors() > 0 ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
		}
		return;
	}

	// Defense in depth: the live capture path below penalises points, credits the hunter, and MarkCaptured()s
	// the survivor -- all wrong if they are already out of play (a survivor who reached the exit being
	// "captured" here would lose their escape). Every current caller only fires for an in-play survivor;
	// guard so a future caller can't double-process one. (Practice/test/infection paths above handle their
	// own non-alive cases and have already returned.)
	if (!SurvivorPS || !SurvivorPS->IsAliveSurvivor())
	{
		return;
	}

	// Prop Hunt: a "found" prop is OUT (spectating), never a returning Hall Monitor -- suppress the conversion so the
	// seeker's win condition (all props caught) resolves cleanly and "props found" math stays exact.
	const bool bCanReturnAsFakeHunter = !bPropHuntMode && BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && SurvivorPS && SurvivorPS->IsAliveSurvivor() && SurvivorController;
	const FVector CaptureLocation = Survivor->GetActorLocation();
	const int32 LostQuestionPoints = SurvivorPS ? SurvivorPS->ApplyCaughtQuestionPointPenalty(0.25f) : 0;
	RecordPlaytestTelemetryMarker(TEXT("capture"), CaptureLocation, bCanReturnAsFakeHunter ? TEXT("hall_monitor_return") : TEXT("eliminated"), CapturingHunterPS, SurvivorPS);
	if (CapturingHunterPS && CapturingHunterPS->PlayerRole == EBHPlayerRole::Hunter)
	{
		CapturingHunterPS->AddHunterPoints(40);
		if (ABHPlayerController* HunterPC = Cast<ABHPlayerController>(CapturingHunter ? CapturingHunter->GetController() : nullptr))
		{
			HunterPC->ClientShowStatusMessage(FString::Printf(TEXT("Axe capture registered. +40 capture points (%d total)."), CapturingHunterPS->HunterPoints), 3.25f);
		}
		// Cosmetic achievement: a real player's first capture as Teacher (-> the Detention tint).
		if (CapturingHunter && !CapturingHunterPS->IsABot())
		{
			CapturingHunter->ClientGrantAchievement(FName(TEXT("first_blood")), TEXT("First capture. The hunt is on."));
			// Count toward the Truant Officer milestone (50 lifetime captures).
			CapturingHunter->ClientRecordCapture();
			// Secret: a capture in the first 20 seconds of the Hunt is a Pop Quiz.
			if (BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && (HuntSeconds - BHGS->RemainingTime) <= 20)
			{
				CapturingHunter->ClientGrantAchievement(FName(TEXT("pop_quiz")), TEXT("Pop quiz! Caught one in the opening seconds."));
			}
		}
	}
	if (ABHPlayerController* SurvivorPC = Cast<ABHPlayerController>(SurvivorController))
	{
		if (LostQuestionPoints > 0)
		{
			SurvivorPC->ClientShowStatusMessage(FString::Printf(TEXT("Caught by the Teacher's axe. Lost %d unspent question points."), LostQuestionPoints), 3.5f);
		}
		else
		{
			SurvivorPC->ClientShowStatusMessage(TEXT("Caught by the Teacher's axe."), 3.0f);
		}

		FBHClientHorrorCue CaptureImpactCue;
		CaptureImpactCue.EventType = EBHScareEventType::Ambient;
		CaptureImpactCue.FocusLocation = CapturingHunter
			? CapturingHunter->GetActorLocation() + FVector(0.0f, 0.0f, 92.0f)
			: CaptureLocation + FVector(0.0f, 0.0f, 92.0f);
		// Getting caught is the single most important horror beat, but it used to be a faint amber blink (0.24) then
		// an instant vanish -- it barely read as an event. Punch it up to a sharp red hit-flash + hard shake so the
		// axe landing actually lands on screen. (Visual only; EventType stays Ambient to avoid any asset-gated path.)
		CaptureImpactCue.DurationSeconds = 0.60f;
		CaptureImpactCue.ShakeIntensity = 0.72f;
		CaptureImpactCue.CameraJitterDuration = 0.45f;
		CaptureImpactCue.CameraJitterFrequency = 55.0f;
		CaptureImpactCue.FlashIntensity = 0.85f;
		CaptureImpactCue.FlashColor = FLinearColor(1.0f, 0.13f, 0.07f, 1.0f);
		SurvivorPC->ClientPlayHorrorCue(CaptureImpactCue);
	}
	Survivor->MarkCaptured();
	ReportBotStimulus(EBHBotStimulusType::Capture, CaptureLocation, Survivor, Survivor, TEXT("survivor captured"), 1.6f);
	ApplyPresenceSpike(CaptureLocation, 92.0f, TEXT("The Teacher found someone."));
	// Only flag hall-monitor eligibility when the conversion below will actually run (same guard). Setting it
	// unconditionally left the LAST-caught survivor (CountAliveSurvivors()==0, round ending) with a lingering
	// eligible flag that would force them into FakeHunter next round if a non-travel restart ever occurred.
	if (SurvivorPS && bCanReturnAsFakeHunter && CountAliveSurvivors() > 0)
	{
		SurvivorPS->SetFakeHunterEligible(true);
	}

	if (bCanReturnAsFakeHunter && CountAliveSurvivors() > 0)
	{
		SurvivorPS->SetRole(EBHPlayerRole::FakeHunter);
		SurvivorPS->SetLifeState(EBHPlayerLifeState::Alive);
		SurvivorPS->SetHiddenInLocker(false);
		SurvivorPS->SetFakeHunterEligible(false);
		if (bRevisionMode)
		{
			BroadcastStatus(FString::Printf(TEXT("%s returned as a hall monitor. They still count toward 70/50; traps, hints, and false corridor markers unlock after %d answer-team contribution(s)."), *SurvivorPS->GetPlayerName(), GetRevisionMinimumContributionTarget()), 5.0f);
			if (ABHPlayerController* SurvivorPC = Cast<ABHPlayerController>(SurvivorController))
			{
				SurvivorPC->ClientShowStatusMessage(TEXT("Hall monitor tools are locked. You still count toward 70/50. Aim at a station and answer with 1-4."), 5.0f);
			}
		}
		else
		{
			BroadcastStatus(FString::Printf(TEXT("%s returned as a hall monitor. They can trap and mislead, but cannot capture."), *SurvivorPS->GetPlayerName()), 5.0f);
		}
		RestartPlayer(SurvivorController);
		return;
	}

	if (CountAliveSurvivors() <= 0)
	{
		// Cosmetic achievement: caught everyone with nobody escaping is a Flawless Hunt (-> the Apex tint).
		if (CountEscapedSurvivors() <= 0 && CapturingHunter && CapturingHunterPS
			&& CapturingHunterPS->PlayerRole == EBHPlayerRole::Hunter && !CapturingHunterPS->IsABot())
		{
			CapturingHunter->ClientGrantAchievement(FName(TEXT("flawless_hunt")), TEXT("Flawless hunt -- nobody escaped."));
		}
		// Revision mode: the Teacher catching everyone must NOT skip the class's revision. If hall monitors still
		// owe contributions, hold the round open (the per-second TickRoundTimer drives the grace countdown and the
		// eventual end + dock). Returns false (resolve now) when not in revision mode or monitors are already done.
		if (TickAllCaughtRevisionGrace())
		{
			return;
		}
		// Evacuate-together: when the last alive survivor is caught, the class still wins if anyone already
		// reached the exit. Hard-coding HunterWin here would discard survivors who escaped and are waiting.
		// Mirrors the Hunt-tick resolution.
		EndRound(CountEscapedSurvivors() > 0 ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
	}
}

void ABHGameMode::NotifySurvivorEscaped(ABHCharacter* Survivor)
{
	if (!Survivor)
	{
		return;
	}

	const ABHGameState* CurrentBHGS = GetGameState<ABHGameState>();
	const bool bFinalEscapeInProgress = CurrentBHGS
		&& (CurrentBHGS->RoundPhase == EBHRoundPhase::FinalEscape || CurrentBHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive);
	if ((bPracticeMode || bTestMode) && !bFinalEscapeInProgress)
	{
		RecordPlaytestTelemetryMarker(TEXT("escape_reached"), Survivor->GetActorLocation(), bTestMode ? TEXT("test") : TEXT("practice"), Survivor->GetPlayerState<ABHPlayerState>());
		BroadcastStatus(bTestMode ? TEXT("Test escape reached. The round stays open.") : TEXT("Practice escape reached. The lab stays open."), 3.5f);
		if (ABHGameState* BHGS = GetGameState<ABHGameState>())
		{
			BHGS->SetExitUnlocked(bTestMode);
			BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 64.0f), bTestMode ? TEXT("Test escape complete. Exit remains open.") : TEXT("Practice escape complete. Resetting the exit."), BHGS->PresencePulse + 1);
		}
		return;
	}

	// Defense in depth: MarkEscaped() + the win check below assume an in-play survivor. Every current caller
	// only fires for one, but guard so a future caller can't re-escape someone already out of play (which
	// would re-run the round-end check). The practice/test path above handles its own case and returned.
	const ABHPlayerState* EscapingPS = Survivor->GetPlayerState<ABHPlayerState>();
	if (!EscapingPS || !EscapingPS->IsAliveSurvivor())
	{
		return;
	}

	ReportBotStimulus(EBHBotStimulusType::Escape, Survivor->GetActorLocation(), Survivor, Survivor, TEXT("survivor escaped"), 1.6f);
	RecordPlaytestTelemetryMarker(TEXT("escape"), Survivor->GetActorLocation(), bFinalEscapeInProgress ? TEXT("final_escape") : TEXT("standard"), Survivor->GetPlayerState<ABHPlayerState>());
	Survivor->MarkEscaped();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		if (BHGS->RoundPhase == EBHRoundPhase::FinalEscape || BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive)
		{
			BroadcastStatus(TEXT("A student boarded the evacuation train."), 3.0f);
			// Secret: boarding the evacuation train during the final escape is Saved by the Bell.
			if (!Survivor->GetPlayerState<ABHPlayerState>() || !Survivor->GetPlayerState<ABHPlayerState>()->IsABot())
			{
				Survivor->ClientGrantAchievement(FName(TEXT("saved_by_bell")), TEXT("Saved by the bell -- boarded the last train out."));
			}
			if (CountAliveSurvivors() <= 0)
			{
				BHGS->SetFinalEscapeState(EBHFinalEscapeState::Departed, 0.0f, 0.0f, 0.0f);
				EndRound(EBHRoundPhase::SurvivorsWin);
			}
			return;
		}
	}

	// Standard exit: evacuate together. The student reaches the exit (already MarkEscaped above, so they no
	// longer count as an alive survivor) and waits; the round only ends once no alive survivor remains
	// inside — everyone has either escaped or been caught. Previously the first survivor to touch the exit
	// ended the round for the whole class, cutting the others off.
	const ABHPlayerState* EscapedPS = Survivor->GetPlayerState<ABHPlayerState>();
	const FString EscapedName = EscapedPS ? EscapedPS->GetPlayerName() : FString(TEXT("A student"));
	const int32 StillInside = CountAliveSurvivors();
	if (StillInside <= 0)
	{
		BroadcastStatus(FString::Printf(TEXT("%s reached the exit. Everyone is out — the class escaped!"), *EscapedName), 4.0f);
		// Secret: this survivor was the last one out AND at least one classmate was caught this round -> Comeback Kid.
		if (GameState && (!EscapedPS || !EscapedPS->IsABot()))
		{
			int32 CapturedThisRound = 0;
			for (APlayerState* RawPS : GameState->PlayerArray)
			{
				const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
				if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Survivor && BHPS->LifeState == EBHPlayerLifeState::Captured)
				{
					++CapturedThisRound;
				}
			}
			if (CapturedThisRound > 0)
			{
				Survivor->ClientGrantAchievement(FName(TEXT("comeback_kid")), TEXT("Comeback kid -- last one out while the rest were caught."));
			}
		}
		EndRound(EBHRoundPhase::SurvivorsWin);
	}
	else
	{
		BroadcastStatus(FString::Printf(TEXT("%s reached the exit. %d still inside."), *EscapedName, StillInside), 3.5f);
	}
}

void ABHGameMode::ToggleLightCircuit(int32 CircuitId)
{
	if (CircuitId <= 0)
	{
		return;
	}

	bool bFoundPoweredLight = false;
	for (TActorIterator<ABHFlickerLight> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			bFoundPoweredLight = It->IsPowered();
			break;
		}
	}

	const bool bNewPowered = !bFoundPoweredLight;
	for (TActorIterator<ABHFlickerLight> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			It->SetPowered(bNewPowered);
		}
	}
}

void ABHGameMode::OpenSecurityCircuit(int32 CircuitId)
{
	for (TActorIterator<ABHSecurityShutter> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			It->SetOpen(true);
		}
	}
	SetSecurityCircuitCCTVActive(CircuitId, true);
}

bool ABHGameMode::ToggleSecurityCircuit(int32 CircuitId)
{
	if (CircuitId <= 0)
	{
		return false;
	}

	bool bFoundClosedShutter = false;
	bool bFoundCircuitShutter = false;
	for (TActorIterator<ABHSecurityShutter> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() != CircuitId)
		{
			continue;
		}

		bFoundCircuitShutter = true;
		if (!It->IsOpen())
		{
			bFoundClosedShutter = true;
			break;
		}
	}

	const bool bNewOpen = bFoundClosedShutter;
	bool bAllApplied = bFoundCircuitShutter;
	bool bAnyOpenAfterToggle = false;
	for (TActorIterator<ABHSecurityShutter> It(GetWorld()); It; ++It)
	{
		if (It->GetCircuitId() == CircuitId)
		{
			if (!It->TrySetOpen(bNewOpen))
			{
				bAllApplied = false;
			}
			bAnyOpenAfterToggle = bAnyOpenAfterToggle || It->IsOpen();
		}
	}
	SetSecurityCircuitCCTVActive(CircuitId, bAnyOpenAfterToggle);
	return bAllApplied;
}

void ABHGameMode::SetSecurityCircuitCCTVActive(int32 CircuitId, bool bCircuitOpen)
{
	if (CircuitId <= 0 || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ABHSecurityCamera> It(GetWorld()); It; ++It)
	{
		ABHSecurityCamera* Camera = *It;
		if (Camera && Camera->GetCircuitId() == CircuitId)
		{
			Camera->SetCameraDisabled(bCircuitOpen || Camera->IsRoundOffline());
		}
	}

	for (TActorIterator<ABHCCTVZone> It(GetWorld()); It; ++It)
	{
		ABHCCTVZone* Zone = *It;
		if (Zone && Zone->GetCircuitId() == CircuitId)
		{
			Zone->SetZoneEnabled(!bCircuitOpen && !Zone->IsRoundOffline());
		}
	}
}

void ABHGameMode::ApplyRoundCCTVVisibility(EBHRoundModifier ActiveModifier)
{
	if (!GetWorld())
	{
		return;
	}

	for (TActorIterator<ABHSecurityCamera> It(GetWorld()); It; ++It)
	{
		if (ABHSecurityCamera* Camera = *It)
		{
			Camera->SetRoundOffline(false);
			Camera->ResetCCTVState();
		}
	}

	TArray<ABHCCTVZone*> Zones;
	for (TActorIterator<ABHCCTVZone> It(GetWorld()); It; ++It)
	{
		if (ABHCCTVZone* Zone = *It)
		{
			Zones.Add(Zone);
		}
	}

	if (Zones.IsEmpty())
	{
		return;
	}

	Zones.Sort([](const ABHCCTVZone& A, const ABHCCTVZone& B)
	{
		const FVector ALocation = A.GetActorLocation();
		const FVector BLocation = B.GetActorLocation();
		if (!FMath::IsNearlyEqual(ALocation.X, BLocation.X))
		{
			return ALocation.X < BLocation.X;
		}
		if (!FMath::IsNearlyEqual(ALocation.Y, BLocation.Y))
		{
			return ALocation.Y < BLocation.Y;
		}
		return ALocation.Z < BLocation.Z;
	});

	for (ABHCCTVZone* Zone : Zones)
	{
		if (Zone)
		{
			Zone->SetRoundOffline(false);
		}
	}

	int32 Salt = 10017;
	if (RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		Salt = 20017;
	}
	else if (RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		Salt = 30017;
	}

	TSet<int32> OfflineIndexes;
	if (ActiveModifier == EBHRoundModifier::DeadCCTV)
	{
		TArray<int32> OfflineOrder;
		for (int32 ZoneIndex = 0; ZoneIndex < Zones.Num(); ++ZoneIndex)
		{
			OfflineOrder.Add(ZoneIndex);
		}

		FRandomStream OfflineStream(RoundSeed + Salt + 7919);
		for (int32 Index = OfflineOrder.Num() - 1; Index > 0; --Index)
		{
			OfflineOrder.Swap(Index, OfflineStream.RandRange(0, Index));
		}

		const int32 MaxOfflineCount = Zones.Num() > 1 ? Zones.Num() - 1 : 1;
		const int32 DesiredOfflineCount = FMath::Clamp(FMath::CeilToInt(static_cast<float>(Zones.Num()) * 0.45f), 1, MaxOfflineCount);
		for (int32 Index = 0; Index < DesiredOfflineCount && OfflineOrder.IsValidIndex(Index); ++Index)
		{
			OfflineIndexes.Add(OfflineOrder[Index]);
		}
	}

	TArray<int32> VisibleZoneIndexes;
	for (int32 ZoneIndex = 0; ZoneIndex < Zones.Num(); ++ZoneIndex)
	{
		if (!OfflineIndexes.Contains(ZoneIndex))
		{
			VisibleZoneIndexes.Add(ZoneIndex);
		}
	}

	int32 HiddenIndex = INDEX_NONE;
	if (!VisibleZoneIndexes.IsEmpty())
	{
		const int32 HiddenCandidateIndex = SelectHiddenCCTVIndex(VisibleZoneIndexes.Num(), Salt);
		HiddenIndex = VisibleZoneIndexes.IsValidIndex(HiddenCandidateIndex) ? VisibleZoneIndexes[HiddenCandidateIndex] : INDEX_NONE;
	}

	for (int32 ZoneIndex = 0; ZoneIndex < Zones.Num(); ++ZoneIndex)
	{
		if (ABHCCTVZone* Zone = Zones[ZoneIndex])
		{
			const bool bOffline = OfflineIndexes.Contains(ZoneIndex);
			if (ABHSecurityCamera* Camera = Zone->GetLinkedCamera())
			{
				Camera->SetRoundOffline(bOffline);
			}
			Zone->SetRoundOffline(bOffline);
			Zone->SetZoneVisible(!bOffline && ZoneIndex != HiddenIndex);
		}
	}
}

int32 ABHGameMode::SelectHiddenCCTVIndex(int32 CandidateCount, int32 Salt) const
{
	if (CandidateCount <= 0)
	{
		return INDEX_NONE;
	}

	FRandomStream Stream(RoundSeed + Salt);
	return Stream.RandRange(0, CandidateCount - 1);
}

ABHCCTVZone* ABHGameMode::SpawnCCTVZoneForCamera(ABHSecurityCamera* Camera, int32 CircuitId, const FString& AlertLabel, bool bVisible)
{
	if (!Camera || !GetWorld())
	{
		return nullptr;
	}

	FVector Forward = Camera->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const float DetectionRange = Camera->GetDetectionRange();
	const float ConeRadians = FMath::DegreesToRadians(Camera->GetDetectionConeDegrees() * 0.5f);
	const float HalfLength = FMath::Clamp(DetectionRange * 0.30f, 900.0f, 1750.0f);
	const float HalfWidth = FMath::Clamp(DetectionRange * FMath::Tan(ConeRadians) * 0.28f, 360.0f, 950.0f);
	const float CenterDistance = FMath::Clamp(DetectionRange * 0.42f, 1100.0f, 2450.0f);
	const FVector ZoneCenter = Camera->GetActorLocation() + Forward * CenterDistance;
	const FVector ZoneLocation(ZoneCenter.X, ZoneCenter.Y, 120.0f);
	const FRotator ZoneRotation(0.0f, Camera->GetActorRotation().Yaw, 0.0f);

	ABHCCTVZone* Zone = GetWorld()->SpawnActor<ABHCCTVZone>(ZoneLocation, ZoneRotation);
	if (Zone)
	{
		Zone->ConfigureZone(Camera, CircuitId, AlertLabel, FVector(HalfLength, HalfWidth, 120.0f), bVisible);
	}
	return Zone;
}

ABHRuntimeMeshPropActor* ABHGameMode::SpawnRuntimeMeshProp(
	const FVector& Location,
	const FRotator& Rotation,
	const FString& MeshAssetPath,
	const FString& MaterialAssetPath,
	const FVector& MeshScale,
	bool bCollides,
	const FVector& FallbackScale,
	const FLinearColor& FallbackTint,
	EBHBlockMaterial FallbackMaterial,
	bool bGroundToMeshBounds)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	UStaticMesh* MeshAsset = MeshAssetPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);

	// Imported meshes (e.g. the CC0 Kenney roof props) carry an arbitrary FBX pivot, so planting that pivot
	// at the requested point leaves the model floating off-centre and above the surface. When grounding is
	// requested, shift the spawn so the mesh's *bounds* land where the caller meant: bounds centre over the
	// XY, bounds bottom resting on the Z. Yaw-only is assumed (all roof decor), so Z extents are unaffected
	// by rotation and only the XY centre needs rotating. Skipped for the fallback block (its scale is exact).
	FVector PlacedLocation = Location;
	if (bGroundToMeshBounds && MeshAsset)
	{
		const FBox LocalBox = MeshAsset->GetBoundingBox();
		const FVector ScaledOrigin = LocalBox.GetCenter() * MeshScale;
		const FVector ScaledExtent = LocalBox.GetExtent() * MeshScale;
		const FVector RotatedOrigin = Rotation.RotateVector(ScaledOrigin);
		PlacedLocation.X = Location.X - RotatedOrigin.X;
		PlacedLocation.Y = Location.Y - RotatedOrigin.Y;
		PlacedLocation.Z = Location.Z - (ScaledOrigin.Z - ScaledExtent.Z);
	}

#if WITH_EDITOR
	// Authored export: bake the prop as a real AStaticMeshActor with a hard mesh ref (so it serializes,
	// cooks with the map, and is editable), falling back to an authored block actor if the mesh is missing.
	if (bAuthoringExport)
	{
		if (!SpawnAuthoredMeshActor(PlacedLocation, Rotation, MeshAssetPath, MaterialAssetPath, MeshScale, bCollides))
		{
			SpawnAuthoredBlockActor(Location, FallbackScale, FallbackTint, Rotation, bCollides, FallbackMaterial, false);
		}
		return nullptr;
	}
#endif

	if (!MeshAsset)
	{
		SpawnBlock(Location, FallbackScale, FallbackTint, Rotation, bCollides, FallbackMaterial);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHRuntimeMeshPropActor* Prop = GetWorld()->SpawnActor<ABHRuntimeMeshPropActor>(PlacedLocation, Rotation, SpawnParams);
	if (!Prop)
	{
		SpawnBlock(Location, FallbackScale, FallbackTint, Rotation, bCollides, FallbackMaterial);
		return nullptr;
	}

	Prop->ConfigureProp(MeshAssetPath, MaterialAssetPath, MeshScale, bCollides);
	return Prop;
}

void ABHGameMode::NotifyLoudNoise(const FVector& Location, const FString& Reason)
{
	LastBotNoiseLocation = Location;
	LastBotNoiseTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastBotNoiseTime;
	const bool bAntiCampAlert = Reason.Contains(TEXT("restless breathing"), ESearchCase::IgnoreCase)
		|| Reason.Contains(TEXT("camp"), ESearchCase::IgnoreCase);
	if (Reason.Contains(TEXT("wrong answer"), ESearchCase::IgnoreCase)
		|| Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase)
		|| Reason.Contains(TEXT("trap"), ESearchCase::IgnoreCase)
		|| Reason.Contains(TEXT("glass"), ESearchCase::IgnoreCase)
		|| bAntiCampAlert)
	{
		RecordPlaytestTelemetryMarker(TEXT("loud_noise"), Location, Reason);
	}
	ReportBotStimulus(EBHBotStimulusType::Noise, Location, nullptr, nullptr, Reason, Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase) ? 0.7f : 1.0f);
	ReportAtmosphereStimulus(Reason.Contains(TEXT("footstep"), ESearchCase::IgnoreCase) ? EBHAtmosphereStimulusType::Footstep : EBHAtmosphereStimulusType::Noise, Location, nullptr, nullptr, Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase) ? 0.7f : 1.0f, Reason);
	SpawnAmbient(Location + FVector(0.0f, 0.0f, 80.0f), 150.0f, 0.18f, 0.08f, 3.2f, 3.5f);
	const float Spike = Reason.Contains(TEXT("decoy"), ESearchCase::IgnoreCase) ? 42.0f : 50.0f;
	ApplyPresenceSpike(Location, Spike, FString::Printf(TEXT("Something turned toward the %s."), *Reason));
	PublishDangerObjectiveBeat(Location, Reason.Contains(TEXT("glass"), ESearchCase::IgnoreCase) ? TEXT("Glass") : TEXT("Noise"));

	const bool bLearningCriticalAlert =
		Reason.Contains(TEXT("completed station"), ESearchCase::IgnoreCase) ||
		Reason.Contains(TEXT("wrong answer"), ESearchCase::IgnoreCase) ||
		bAntiCampAlert;
	const bool bShowHunterNoiseAlert = !bRevisionMode || bLearningCriticalAlert;
	if (!bShowHunterNoiseAlert || !GetWorld())
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !BHPS || !Pawn || !BHPS->IsAliveHunter())
		{
			continue;
		}

		const FVector Delta = Location - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size() / 100.0f;
		if (DistanceMeters <= 75.0f * FMath::Max(0.25f, NoiseRadiusMultiplier))
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Noise: %s %s, %.0fm away."), *Reason, *CompassFromDelta(Delta), DistanceMeters), 2.5f);
		}
	}
}

void ABHGameMode::BroadcastGameplayAudioCue(const FBHGameplayAudioCue& Cue)
{
	if (!GetWorld() || (Cue.AudioAsset.IsNull() && Cue.Caption.IsEmpty()))
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientPlayGameplayAudioCue(Cue);
		}
	}
}

void ABHGameMode::NotifyCCTVDetection(ABHSecurityCamera* Camera, AActor* ZoneActor, ABHCharacter* Survivor, const FString& AlertLabel)
{
	if (!GetWorld() || !Survivor)
	{
		return;
	}

	const ABHGameState* BHGS = GetWorld()->GetGameState<ABHGameState>();
	const ABHPlayerState* SurvivorPS = Survivor->GetPlayerState<ABHPlayerState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt || !SurvivorPS || !SurvivorPS->IsAliveSurvivor())
	{
		return;
	}

	const FString Reason = AlertLabel.IsEmpty() ? TEXT("CCTV zone detection") : AlertLabel;
	AActor* SourceActor = Camera ? static_cast<AActor*>(Camera) : ZoneActor;
	const FVector SurvivorLocation = Survivor->GetActorLocation();
	const FVector SourceLocation = SourceActor ? SourceActor->GetActorLocation() : SurvivorLocation;
	RecordPlaytestTelemetryMarker(TEXT("cctv_detection"), SurvivorLocation, Reason, nullptr, SurvivorPS);
	ReportBotStimulus(EBHBotStimulusType::Sight, SurvivorLocation, SourceActor, Survivor, Reason, 1.0f);
	ReportAtmosphereStimulus(EBHAtmosphereStimulusType::CCTV, SourceLocation, SourceActor, Survivor, 0.95f, TEXT("CCTV zone detection"));

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !BHPS || !Pawn || !BHPS->IsAliveHunter())
		{
			continue;
		}

		const FVector Delta = SourceLocation - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size() / 100.0f;
		PC->ClientShowStatusMessage(FString::Printf(TEXT("CCTV motion: camera %s, %.0fm away. Verify on monitor."), *CompassFromDelta(Delta), DistanceMeters), 3.0f);
	}
}

int32 ABHGameMode::NotifyHallMonitorMisdirection(const FVector& Location, const FString& SenderName)
{
	if (!GetWorld())
	{
		return 0;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return 0;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	LastBotNoiseLocation = Location;
	LastBotNoiseTime = Now;
	ReportBotStimulus(EBHBotStimulusType::Noise, Location, nullptr, nullptr, TEXT("hall monitor false route marker"), 0.55f);
	ReportAtmosphereStimulus(EBHAtmosphereStimulusType::CCTV, Location, nullptr, nullptr, 0.35f, TEXT("hall monitor false CCTV alert"));

	const float ServerNow = BHGS->GetServerWorldTimeSeconds();
	const FName MonitorBeatId(TEXT("MonitorPing"));
	TArray<FBHObjectiveBeat> Beats = BHGS->ObjectiveBeats;
	Beats.RemoveAll([ServerNow, MonitorBeatId](const FBHObjectiveBeat& Beat)
	{
		return Beat.BeatId == MonitorBeatId || (Beat.ExpireServerTime > 0.0f && ServerNow > Beat.ExpireServerTime);
	});

	FBHObjectiveBeat MonitorBeat;
	MonitorBeat.BeatId = MonitorBeatId;
	MonitorBeat.Label = TEXT("Monitor");
	MonitorBeat.Location = Location;
	MonitorBeat.Radius = 7200.0f;
	MonitorBeat.ExpireServerTime = ServerNow + 12.0f;
	MonitorBeat.bPrimary = false;
	MonitorBeat.bDanger = true;
	MonitorBeat.AudienceRoleMask = ObjectiveBeatRoleMask({EBHPlayerRole::Hunter, EBHPlayerRole::FakeHunter, EBHPlayerRole::Tester});
	Beats.Add(MonitorBeat);
	BHGS->SetObjectiveBeats(Beats);

	int32 TeachersNotified = 0;
	const FString SenderLabel = SenderName.IsEmpty() ? FString(TEXT("Hall monitor")) : SenderName.Left(22);
	// Iterate ALL controllers (bot Teachers are AAIControllers, not player controllers): the old player-only loop never
	// counted a bot Teacher, so the Monitor saw "sent to 0 Teacher(s)." even though the bot WAS steered by the
	// ReportBotStimulus above. Count every alive Teacher; gate the client toast (a player-only RPC) behind a PC cast.
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Ctrl = It->Get();
		ABHPlayerState* BHPS = Ctrl ? Ctrl->GetPlayerState<ABHPlayerState>() : nullptr;
		APawn* Pawn = Ctrl ? Ctrl->GetPawn() : nullptr;
		if (!Ctrl || !BHPS || !Pawn || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const FVector Delta = Location - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size2D() / 100.0f;
		ABHPlayerController* PC = Cast<ABHPlayerController>(Ctrl);
		if (BHPS->IsAliveHunter())
		{
			if (PC)
			{
				PC->ClientShowStatusMessage(FString::Printf(TEXT("%s marker: unverified corridor %s, %.0fm away."), *SenderLabel, *CompassFromDelta(Delta), DistanceMeters), 4.0f);
			}
			++TeachersNotified;
		}
		else if (PC && BHPS->IsAliveSurvivor() && DistanceMeters <= 22.0f)
		{
			PC->ClientShowStatusMessage(TEXT("A Hall Monitor marked this corridor. Rotate, hide, or bait the Teacher."), 3.0f);
		}
	}

	return TeachersNotified;
}

void ABHGameMode::ReportAtmosphereStimulus(EBHAtmosphereStimulusType Type, const FVector& Location, AActor* SourceActor, AActor* TargetActor, float Strength, const FString& Reason)
{
	if (Type == EBHAtmosphereStimulusType::CCTV
		|| Type == EBHAtmosphereStimulusType::Locker
		|| Type == EBHAtmosphereStimulusType::Monster
		|| Type == EBHAtmosphereStimulusType::Manual)
	{
		const UEnum* StimulusEnum = StaticEnum<EBHAtmosphereStimulusType>();
		const FString StimulusName = StimulusEnum ? StimulusEnum->GetNameStringByValue(static_cast<int64>(Type)) : TEXT("Atmosphere");
		const ABHCharacter* TargetCharacter = Cast<ABHCharacter>(TargetActor);
		RecordPlaytestTelemetryMarker(TEXT("atmosphere_stimulus"), Location, FString::Printf(TEXT("%s %.2f %s"), *StimulusName, Strength, *Reason.Left(72)), nullptr, TargetCharacter ? TargetCharacter->GetPlayerState<ABHPlayerState>() : nullptr);
	}
	// Suppress stimulus-reactive atmosphere cues (whisper / locker-knock / light-cut / footstep-echo) during calm
	// tutorial teaching beats; they resume for the scripted chase climax (bTutorialHorrorAllowed). Telemetry above
	// still records. No effect outside tutorial mode.
	if (AtmosphereDirector && !IsTutorialHorrorSuppressed())
	{
		AtmosphereDirector->ReportAtmosphereStimulus(Type, Location, SourceActor, TargetActor, Strength, Reason);
	}
}

bool ABHGameMode::TriggerAtmosphereCue(const FBHScareEventSpec& Spec)
{
	return AtmosphereDirector ? AtmosphereDirector->TriggerAtmosphereCue(Spec) : false;
}

bool ABHGameMode::TriggerBlackoutPulse(const FVector& Location, float Radius, float DurationSeconds)
{
	return AtmosphereDirector ? AtmosphereDirector->TriggerBlackoutPulse(Location, Radius, DurationSeconds) : false;
}

bool ABHGameMode::TriggerManualScare(ABHCharacter* Target, EBHScareEventType ScareType)
{
	return AtmosphereDirector ? AtmosphereDirector->TriggerManualScare(Target, ScareType) : false;
}

int32 ABHGameMode::GetEffectiveScareIntensity() const
{
	return bRevisionMode ? FMath::Clamp(RevisionScareIntensity, 0, 3) : 3;
}

float ABHGameMode::GetScareSensoryScale() const
{
	switch (GetEffectiveScareIntensity())
	{
	case 0:
		return 0.0f;
	case 1:
		return 0.45f;
	case 2:
		return 0.72f;
	case 3:
	default:
		return 1.0f;
	}
}

FBHJumpscareVariant ABHGameMode::ChooseJumpscareVariant(EBHScareEventType EventType, bool bPreferRealMesh) const
{
	const int32 EffectiveScareIntensity = GetEffectiveScareIntensity();
	const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
	TArray<const FBHJumpscareVariant*> Candidates;
	float TotalWeight = 0.0f;

	for (const FBHJumpscareVariant& Variant : Variants)
	{
		if (Variant.VariantId.IsNone() || Variant.Weight <= 0.0f || Variant.MinimumScareIntensity > EffectiveScareIntensity)
		{
			continue;
		}

		const bool bHasExistingAudio = BHJumpscareVariantHasExistingAudio(Variant);
		const bool bHasExistingVisual = BHJumpscareVariantHasExistingVisual(Variant);
		const bool bUsefulForCue = EventType == EBHScareEventType::MonsterCharge
			? bHasExistingVisual
			: (bHasExistingAudio || bHasExistingVisual);
		if (!bUsefulForCue)
		{
			continue;
		}

		Candidates.Add(&Variant);
		TotalWeight += FMath::Max(0.01f, Variant.Weight);
	}

	// Showcased close-up scares demand a genuine creature: if any imported-mesh variant is eligible, drop the
	// procedural "simple render" proxies from the pool entirely so the in-your-face / behind-you / peek /
	// SCP-096 scares never present a low-poly proxy.
	if (bPreferRealMesh && Candidates.Num() > 1)
	{
		bool bHasRealMesh = false;
		for (const FBHJumpscareVariant* Candidate : Candidates)
		{
			if (Candidate && !BHJumpscareVariantHasProxyVisual(*Candidate))
			{
				bHasRealMesh = true;
				break;
			}
		}
		if (bHasRealMesh)
		{
			for (int32 Index = Candidates.Num() - 1; Index >= 0; --Index)
			{
				if (Candidates[Index] && BHJumpscareVariantHasProxyVisual(*Candidates[Index]))
				{
					TotalWeight -= FMath::Max(0.01f, Candidates[Index]->Weight);
					Candidates.RemoveAt(Index);
				}
			}
		}
	}

	// Anti-repetition: when more than one variant is eligible, drop the one used last time so
	// back-to-back scares never reuse the same entity.
	if (Candidates.Num() > 1 && !LastJumpscareVariantId.IsNone())
	{
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index] && Candidates[Index]->VariantId == LastJumpscareVariantId)
			{
				TotalWeight -= FMath::Max(0.01f, Candidates[Index]->Weight);
				Candidates.RemoveAt(Index);
				break;
			}
		}
	}

	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		FBHJumpscareVariant Fallback;
		Fallback.VariantId = TEXT("ProxyRed");
		Fallback.DisplayName = TEXT("Procedural Red Proxy");
		Fallback.Weight = 1.0f;
		Fallback.LaunchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint")));
		Fallback.FocusHeight = 145.0f;
		Fallback.LightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);
		Fallback.CameraShakeIntensity = 0.96f;
		Fallback.FlashIntensity = 0.70f;
		Fallback.CameraJitterDuration = 1.10f;
		return Fallback;
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	for (const FBHJumpscareVariant* Candidate : Candidates)
	{
		const float CandidateWeight = FMath::Max(0.01f, Candidate ? Candidate->Weight : 0.0f);
		if (Pick <= CandidateWeight)
		{
			return *Candidate;
		}
		Pick -= CandidateWeight;
	}

	return *Candidates.Last();
}

FString ABHGameMode::GetJumpscareVariantTestReport() const
{
	const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
	FString Report = TEXT("Jumpscare variants are available from Escape > Round > Test Commands.\n");
	Report += TEXT("Use SUPER JUMPSCARE CHAIN for peek, cross-screen sprint, corner sprint, final impact close-up. Use SUPER1, SUPER2, SUPER3 to force a pack chain.\n");
	Report += TEXT("New scares: type 'face' (full-screen image in your face), 'peek' (figure leans out from a wall), 'behind' (turn-around lock that springs a payoff jumpscare).\n");
	Report += TEXT("Configured jumpscare variants:\n");

	if (Variants.IsEmpty())
	{
		Report += TEXT("No configured variants. Runtime scares will use the procedural fallback proxy.");
		return Report;
	}

	for (int32 Index = 0; Index < Variants.Num(); ++Index)
	{
		const FBHJumpscareVariant& Variant = Variants[Index];
		const FString VariantId = Variant.VariantId.IsNone() ? TEXT("None") : Variant.VariantId.ToString();
		const FString DisplayName = Variant.DisplayName.IsEmpty() ? VariantId : Variant.DisplayName;
		const bool bHasPackVisual = !Variant.VisualActorClass.IsNull() || !Variant.SkeletalMesh.IsNull() || !Variant.StaticMesh.IsNull();
		const bool bHasPackAudio = !Variant.LaunchSound.IsNull();
		Report += FString::Printf(TEXT("%d. %s (%s) minIntensity=%d weight=%.2f visuals=%s audio=%s\n"),
			Index + 1,
			*DisplayName,
			*VariantId,
			Variant.MinimumScareIntensity,
			Variant.Weight,
			bHasPackVisual ? TEXT("yes") : TEXT("proxy"),
			bHasPackAudio ? TEXT("yes") : TEXT("fallback"));
	}

	return Report;
}

bool ABHGameMode::ResolveJumpscareVariantToken(const FString& VariantToken, FBHJumpscareVariant& OutVariant, int32& OutVariantIndex, FString& OutError) const
{
	const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
	const FString Token = VariantToken.TrimStartAndEnd();
	OutVariantIndex = INDEX_NONE;

	if (Variants.IsEmpty())
	{
		OutError = TEXT("No jumpscare variants are configured.");
		return false;
	}

	if (Token.IsEmpty())
	{
		OutError = TEXT("Missing jumpscare variant. Use Escape > Round > Test Commands.");
		return false;
	}

	if (Token.Equals(TEXT("SCP096"), ESearchCase::IgnoreCase)
		|| Token.Equals(TEXT("SCP-096"), ESearchCase::IgnoreCase))
	{
		OutVariant = MakeLegacyScp096ProxyJumpscareVariant();
		OutVariantIndex = INDEX_NONE;
		return true;
	}

	if (Token.IsNumeric())
	{
		const int32 OneBasedIndex = FCString::Atoi(*Token);
		if (OneBasedIndex >= 1 && OneBasedIndex <= Variants.Num())
		{
			OutVariantIndex = OneBasedIndex - 1;
			OutVariant = Variants[OutVariantIndex];
			return true;
		}

		OutError = FString::Printf(TEXT("Jumpscare variant number %d is out of range. Use Escape > Round > Test Commands."), OneBasedIndex);
		return false;
	}

	for (int32 Index = 0; Index < Variants.Num(); ++Index)
	{
		const FBHJumpscareVariant& Variant = Variants[Index];
		if (Variant.VariantId.ToString().Equals(Token, ESearchCase::IgnoreCase))
		{
			OutVariantIndex = Index;
			OutVariant = Variant;
			return true;
		}
	}

	for (int32 Index = 0; Index < Variants.Num(); ++Index)
	{
		const FBHJumpscareVariant& Variant = Variants[Index];
		if (!Variant.DisplayName.IsEmpty() && Variant.DisplayName.Contains(Token, ESearchCase::IgnoreCase))
		{
			OutVariantIndex = Index;
			OutVariant = Variant;
			return true;
		}
	}

	OutError = FString::Printf(TEXT("Unknown jumpscare variant '%s'. Use Escape > Round > Test Commands."), *Token);
	return false;
}

bool ABHGameMode::ResolveVisibleJumpscareSpawn(ABHCharacter* Target, ABHPlayerController* TargetPC, const TArray<FVector>& Candidates, float FocusHeight, float MinDistance, float MaxDistance, float PathRadius, FBHResolvedJumpscareSpawn& OutSpawn) const
{
	UWorld* World = GetWorld();
	if (!World || !Target || Candidates.IsEmpty())
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	if (!BHResolveJumpscareView(Target, TargetPC, ViewLocation, ViewForward))
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float TargetFloorZ = TargetLocation.Z - Target->GetSimpleCollisionHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHVisibleJumpscareSpawn), false, Target);
	Params.AddIgnoredActor(Target);

	OutSpawn = FBHResolvedJumpscareSpawn();
	for (const FVector& RawCandidate : Candidates)
	{
		FVector Candidate = RawCandidate;
		Candidate.Z = TargetFloorZ + 4.0f;

		const float Distance = FVector::Dist2D(TargetLocation, Candidate);
		if ((MinDistance > 0.0f && Distance < MinDistance) || (MaxDistance > 0.0f && Distance > MaxDistance))
		{
			continue;
		}

		float Score = -1.0f;
		if (!BHValidateVisibleJumpscareCandidate(World, ViewLocation, ViewForward, TargetLocation, Candidate, FocusHeight, PathRadius, Params, Score))
		{
			continue;
		}

		if (Score > OutSpawn.Score)
		{
			const FVector DirectionToTarget = (TargetLocation - Candidate).GetSafeNormal2D();
			OutSpawn.SpawnLocation = Candidate;
			OutSpawn.FocusLocation = Candidate + FVector(0.0f, 0.0f, FMath::Clamp(FocusHeight, 80.0f, 320.0f));
			OutSpawn.SpawnRotation = DirectionToTarget.IsNearlyZero() ? ViewForward.Rotation() : DirectionToTarget.Rotation();
			OutSpawn.Score = Score;
		}
	}

	return OutSpawn.Score >= 0.0f;
}

bool ABHGameMode::ResolveDirectionalJumpscareSpawn(ABHCharacter* Target, const TArray<FVector>& Candidates, float FocusHeight, float MinDistance, float MaxDistance, float PathRadius, float ZOffset, bool bPreferClose, FBHResolvedJumpscareSpawn& OutSpawn) const
{
	UWorld* World = GetWorld();
	if (!World || !Target || Candidates.IsEmpty())
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float TargetFloorZ = TargetLocation.Z - Target->GetSimpleCollisionHalfHeight();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHDirectionalJumpscareSpawn), false, Target);
	Params.AddIgnoredActor(Target);

	OutSpawn = FBHResolvedJumpscareSpawn();
	float BestMetric = -FLT_MAX;
	for (const FVector& RawCandidate : Candidates)
	{
		FVector Candidate = RawCandidate;
		Candidate.Z = TargetFloorZ + 4.0f + FMath::Max(0.0f, ZOffset);

		const float Distance = FVector::Dist2D(TargetLocation, Candidate);
		if ((MinDistance > 0.0f && Distance < MinDistance) || (MaxDistance > 0.0f && Distance > MaxDistance))
		{
			continue;
		}

		// No view-cone requirement here — the reveal happens on contact, so we only need the spawn
		// space and the charge path toward the target to be clear.
		if (!BHJumpscareSpawnSpaceClear(World, Candidate, Params))
		{
			continue;
		}
		if (!BHJumpscareChargePathClear(World, Candidate, TargetLocation, PathRadius, Params))
		{
			continue;
		}

		// Closer spawns give a quicker, more startling reveal; allow scoring either way.
		const float Metric = bPreferClose ? -Distance : Distance;
		if (Metric > BestMetric)
		{
			BestMetric = Metric;
			const FVector DirectionToTarget = (TargetLocation - Candidate).GetSafeNormal();
			OutSpawn.SpawnLocation = Candidate;
			OutSpawn.FocusLocation = Candidate + FVector(0.0f, 0.0f, FMath::Clamp(FocusHeight, 80.0f, 320.0f));
			OutSpawn.SpawnRotation = DirectionToTarget.IsNearlyZero() ? FRotator::ZeroRotator : DirectionToTarget.Rotation();
			OutSpawn.Score = 1.0f;
		}
	}

	return OutSpawn.Score >= 0.0f;
}

EBHJumpscareApproach ABHGameMode::ChooseMonsterChargeApproach()
{
	// Even mix across the archetypes, but pick only from those other than the last one used so two
	// monster scares in a row never share an approach.
	const EBHJumpscareApproach Approaches[] = {
		EBHJumpscareApproach::HeadOn,
		EBHJumpscareApproach::Behind,
		EBHJumpscareApproach::AlreadyThere,
		EBHJumpscareApproach::CeilingDrop
	};

	TArray<EBHJumpscareApproach, TInlineAllocator<4>> Pool;
	for (EBHJumpscareApproach Approach : Approaches)
	{
		if (Approach != LastJumpscareApproach)
		{
			Pool.Add(Approach);
		}
	}

	const EBHJumpscareApproach Chosen = Pool.IsEmpty() ? EBHJumpscareApproach::HeadOn : Pool[FMath::RandRange(0, Pool.Num() - 1)];
	LastJumpscareApproach = Chosen;
	return Chosen;
}

void ABHGameMode::SendJumpscareChargeCue(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FVector& FocusLocation, const FString& Message, float HoldDuration, float AudioVolume, bool bCloseRangeFocus) const
{
	if (ABHPlayerController* TargetPC = Target ? Cast<ABHPlayerController>(Target->GetController()) : nullptr)
	{
		if (!bCloseRangeFocus)
		{
			TargetPC->ClientSnapViewToFlatFocus(FocusLocation);
		}

		const float SensoryScale = GetScareSensoryScale();
		const float AudioSensoryScale = SensoryScale <= 0.0f ? 0.0f : FMath::Lerp(0.35f, 1.0f, SensoryScale);
		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = FocusLocation;
		Cue.Message = Message;
		Cue.DurationSeconds = HoldDuration + 1.0f;
		Cue.LockSeconds = HoldDuration;
		Cue.ShakeIntensity = FMath::Clamp(Variant.CameraShakeIntensity * SensoryScale, 0.0f, 1.0f);
		Cue.CameraJitterDuration = FMath::Max(Variant.CameraJitterDuration, bCloseRangeFocus ? 1.15f : 0.85f) * SensoryScale;
		Cue.FlashIntensity = FMath::Clamp(Variant.FlashIntensity * SensoryScale, 0.0f, 1.0f);
		Cue.FlashColor = Variant.LightColor;
		Cue.AudioAsset = Variant.LaunchSound;
		Cue.AudioVolume = FMath::Clamp(AudioVolume * AudioSensoryScale, 0.0f, 2.0f);
		Cue.VisualActorClass = Variant.VisualActorClass;
		Cue.CloseVisualOffset = Variant.CloseVisualOffset;
		Cue.CloseVisualRotation = Variant.CloseVisualRotation;
		Cue.CloseVisualScale = Variant.CloseVisualScale;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		Cue.bCloseRangeFocus = bCloseRangeFocus;
		// Full body on every close-range scare (legs included) instead of cropping to the upper torso.
		Cue.bUpperBodyCloseVisual = false;
		Cue.FOVPunch = FMath::Clamp(Variant.ImpactFOVPunch * SensoryScale, 0.0f, 30.0f);
		Cue.HitStopSeconds = bCloseRangeFocus ? FMath::Clamp(Variant.ImpactHitStopSeconds * SensoryScale, 0.0f, 0.25f) : 0.0f;
		Cue.RumbleIntensity = FMath::Clamp(Variant.ImpactRumbleIntensity * SensoryScale, 0.0f, 1.0f);
		Cue.ImpactStinger = Variant.ImpactStinger;
		Cue.bLayeredImpactAudio = bCloseRangeFocus;
		TargetPC->ClientPlayHorrorCue(Cue);
	}
}

void ABHGameMode::TriggerCloseOverlayJumpscare(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float HoldDuration, float FearAmount, float DreadAmount)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	FVector ViewLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 95.0f);
	FVector ViewForward = Target->GetActorForwardVector().GetSafeNormal();
	BHResolveJumpscareView(Target, TargetPC, ViewLocation, ViewForward);
	const FVector FocusLocation = BHResolveJumpscareCloseFocusLocation(ViewLocation, ViewForward, Variant);
	const float CloseHoldDuration = FMath::Clamp(HoldDuration, 0.8f, 2.0f);

	RecordPlaytestTelemetryMarker(TEXT("jumpscare"), Target->GetActorLocation(), FString::Printf(TEXT("close_overlay variant=%s"), *Variant.VariantId.ToString()), nullptr, Target->GetPlayerState<ABHPlayerState>());
	SpawnAmbient(FocusLocation, 260.0f, 0.26f, 0.16f, 5.4f, 3.4f);
	SendJumpscareChargeCue(Target, Variant, FocusLocation, Message, CloseHoldDuration, 1.25f, true);
	FreezeTargetForJumpscare(Target, CloseHoldDuration);
	CutLightsForJumpscare(Target->GetActorLocation(), FocusLocation, 950.0f, 4.8f);
	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(FearAmount);
	Target->AddDread(DreadAmount);
}

#if WITH_DEV_AUTOMATION_TESTS
bool ABHGameMode::DebugValidateJumpscareSpawnCandidate(const FVector& ViewLocation, const FVector& ViewForward, const FVector& TargetLocation, const FVector& CandidateLocation, float FocusHeight, float PathRadius, float* OutScore) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BHJumpscareSpawnDebug), false);
	float Score = -1.0f;
	const bool bValid = BHValidateVisibleJumpscareCandidate(GetWorld(), ViewLocation, ViewForward, TargetLocation, CandidateLocation, FocusHeight, PathRadius, Params, Score);
	if (OutScore)
	{
		*OutScore = Score;
	}
	return bValid;
}

bool ABHGameMode::DebugReserveDirectorCue(ABHCharacter* Target, EBHScareEventType ScareType, float Intensity, FString& OutReason)
{
	if (!AtmosphereDirector)
	{
		AtmosphereDirector = NewObject<UBHAtmosphereDirector>(this, TEXT("AtmosphereDirector_Automation"));
		if (AtmosphereDirector)
		{
			AtmosphereDirector->Initialize(this);
		}
	}

	return AtmosphereDirector
		? AtmosphereDirector->ReserveDirectorCue(Target, ScareType, Target ? Target->GetActorLocation() : FVector::ZeroVector, Intensity, TEXT("automation budget probe"), OutReason)
		: false;
}
#endif

void ABHGameMode::TestJumpscareVariant(ABHPlayerController* RequestingController, const FString& VariantToken)
{
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	const bool bTesterContext = IsHostAdminController(RequestingController)
		|| bTestMode
		|| (BHGS && BHGS->bTestMode)
		|| (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester);
	if (!bTesterContext)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Jumpscare testing requires host admin, Test Round, or the Tester role."), 4.0f);
		}
		return;
	}

	const FString Token = VariantToken.TrimStartAndEnd();
	const FString Normalized = Token.ToLower();
	if (Token.IsEmpty() || Normalized == TEXT("list") || Normalized == TEXT("help") || Normalized == TEXT("?"))
	{
		const FString Report = GetJumpscareVariantTestReport();
		UE_LOG(LogTemp, Display, TEXT("%s"), *Report);
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(Report.Left(900), 10.0f);
		}
		return;
	}

	if (Normalized == TEXT("super") || Normalized == TEXT("chain") || Normalized == TEXT("superchain"))
	{
		TriggerTesterSuperJumpscare(RequestingController);
		return;
	}

	// New scare types: fire them on the requesting tester's own pawn for quick in-engine checks.
	if (Normalized == TEXT("face") || Normalized == TEXT("faceimage") || Normalized == TEXT("png")
		|| Normalized == TEXT("peek") || Normalized == TEXT("corner") || Normalized == TEXT("cornerpeek")
		|| Normalized == TEXT("behind") || Normalized == TEXT("behindyou") || Normalized == TEXT("turn") || Normalized == TEXT("turnaround")
		|| Normalized == TEXT("scp") || Normalized == TEXT("scp096") || Normalized == TEXT("scp-096") || Normalized == TEXT("reveal")
		|| Normalized == TEXT("realscp") || Normalized == TEXT("scpreal") || Normalized == TEXT("scp096real") || Normalized == TEXT("096") || Normalized == TEXT("scpmodel"))
	{
		ABHCharacter* Target = RequestingController ? Cast<ABHCharacter>(RequestingController->GetPawn()) : nullptr;
		const ABHPlayerState* TargetPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Target || !TargetPS || TargetPS->LifeState != EBHPlayerLifeState::Alive)
		{
			if (RequestingController)
			{
				RequestingController->ClientShowStatusMessage(TEXT("Jumpscare testing needs an alive player pawn."), 3.0f);
			}
			return;
		}

		if (Normalized == TEXT("face") || Normalized == TEXT("faceimage") || Normalized == TEXT("png"))
		{
			TriggerFaceImageJumpscare(Target);
			RequestingController->ClientShowStatusMessage(TEXT("Face-image jumpscare fired."), 2.0f);
		}
		else if (Normalized == TEXT("peek") || Normalized == TEXT("corner") || Normalized == TEXT("cornerpeek"))
		{
			TriggerCornerPeek(Target);
			RequestingController->ClientShowStatusMessage(TEXT("Corner-peek spawned to one side - look around to catch it."), 3.0f);
		}
		else if (Normalized == TEXT("realscp") || Normalized == TEXT("scpreal") || Normalized == TEXT("scp096real") || Normalized == TEXT("096") || Normalized == TEXT("scpmodel"))
		{
			TriggerRealScp096Scare(Target);
			RequestingController->ClientShowStatusMessage(TEXT("SCP-096 (real model) armed - far reveal, pause, then it charges."), 3.0f);
		}
		else if (Normalized == TEXT("scp") || Normalized == TEXT("scp096") || Normalized == TEXT("scp-096") || Normalized == TEXT("reveal"))
		{
			TriggerScp096Scare(Target);
			RequestingController->ClientShowStatusMessage(TEXT("SCP-096 reveal armed - it appears far off, pauses, then charges."), 3.0f);
		}
		else
		{
			TriggerBehindYouScare(Target);
			RequestingController->ClientShowStatusMessage(TEXT("Behind-you scare armed - turn around."), 3.0f);
		}
		return;
	}
	if (Normalized == TEXT("super1") || Normalized == TEXT("super01")
		|| Normalized == TEXT("super2") || Normalized == TEXT("super02")
		|| Normalized == TEXT("super3") || Normalized == TEXT("super03")
		|| Normalized.StartsWith(TEXT("super:"))
		|| Normalized.StartsWith(TEXT("super ")))
	{
		FString ForcedToken;
		if (Normalized == TEXT("super1") || Normalized == TEXT("super01"))
		{
			ForcedToken = TEXT("FabMonster01");
		}
		else if (Normalized == TEXT("super2") || Normalized == TEXT("super02"))
		{
			ForcedToken = TEXT("FabMonster02");
		}
		else if (Normalized == TEXT("super3") || Normalized == TEXT("super03"))
		{
			ForcedToken = TEXT("FabMonster03");
		}
		else if (Normalized.StartsWith(TEXT("super:")) || Normalized.StartsWith(TEXT("super ")))
		{
			ForcedToken = Token.RightChop(6).TrimStartAndEnd();
		}

		FBHJumpscareVariant ForcedVariant;
		int32 ForcedVariantIndex = INDEX_NONE;
		FString Error;
		if (!ResolveJumpscareVariantToken(ForcedToken, ForcedVariant, ForcedVariantIndex, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Error);
			if (RequestingController)
			{
				RequestingController->ClientShowStatusMessage(Error, 4.5f);
			}
			return;
		}

		TriggerTesterSuperJumpscare(RequestingController, &ForcedVariant);
		return;
	}

	if (Normalized == TEXT("all"))
	{
		const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
		if (Variants.IsEmpty() || !GetWorld())
		{
			if (RequestingController)
			{
				RequestingController->ClientShowStatusMessage(TEXT("No jumpscare variants are configured."), 3.0f);
			}
			return;
		}

		TWeakObjectPtr<ABHGameMode> WeakGameMode(this);
		TWeakObjectPtr<ABHPlayerController> WeakController(RequestingController);
		for (int32 Index = 0; Index < Variants.Num(); ++Index)
		{
			const FBHJumpscareVariant Variant = Variants[Index];
			const FString VariantName = Variant.DisplayName.IsEmpty() ? Variant.VariantId.ToString() : Variant.DisplayName;
			const FString Label = FString::Printf(TEXT("%d/%d %s"), Index + 1, Variants.Num(), *VariantName);
			FTimerDelegate TestDelegate;
			TestDelegate.BindLambda([WeakGameMode, WeakController, Variant, Label]()
			{
				if (WeakGameMode.IsValid() && WeakController.IsValid())
				{
					WeakGameMode->TriggerTesterJumpscareVariant(WeakController.Get(), Variant, Label);
				}
			});

			FTimerHandle TestTimerHandle;
			GetWorldTimerManager().SetTimer(TestTimerHandle, TestDelegate, 0.15f + static_cast<float>(Index) * 6.5f, false);
		}

		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Queued %d jumpscare variants. Stand still until the sequence finishes."), Variants.Num()), 5.0f);
		}
		UE_LOG(LogTemp, Display, TEXT("BlackoutHunt queued %d jumpscare variant tests for %s."), Variants.Num(), *GetNameSafe(RequestingController));
		return;
	}

	FBHJumpscareVariant Variant;
	int32 VariantIndex = INDEX_NONE;
	FString Error;
	if (!ResolveJumpscareVariantToken(Token, Variant, VariantIndex, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Error);
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(Error, 4.5f);
		}
		return;
	}

	const FString VariantName = Variant.DisplayName.IsEmpty() ? Variant.VariantId.ToString() : Variant.DisplayName;
	const TArray<FBHJumpscareVariant> Variants = GetResolvedJumpscareVariants();
	const FString Label = VariantIndex == INDEX_NONE
		? VariantName
		: FString::Printf(TEXT("%d/%d %s"),
			VariantIndex + 1,
			FMath::Max(1, Variants.Num()),
			*VariantName);
	TriggerTesterJumpscareVariant(RequestingController, Variant, Label);
}

void ABHGameMode::TriggerTesterJumpscareVariant(ABHPlayerController* RequestingController, const FBHJumpscareVariant& Variant, const FString& TestLabel)
{
	if (!RequestingController || !GetWorld())
	{
		return;
	}

	ABHCharacter* Target = Cast<ABHCharacter>(RequestingController->GetPawn());
	const ABHPlayerState* BHPS = RequestingController->GetPlayerState<ABHPlayerState>();
	if (!Target || !BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Jumpscare testing needs an alive player pawn."), 3.0f);
		return;
	}
	if (BHIsLegacyScp096JumpscareVariant(Variant))
	{
		TriggerMonsterChargeJumpscareWithVariant(Target, Variant, FString::Printf(TEXT("Jumpscare test: %s"), *TestLabel), 44.0f, 44.0f);
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	FVector Forward = RequestingController->GetControlRotation().Vector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = Target->GetActorForwardVector().GetSafeNormal2D();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const FVector Directions[] = { Forward, Right, -Right, -Forward };
	const float Distances[] = { 2600.0f, 2100.0f, 1600.0f, 1200.0f };
	TArray<FVector> Candidates;
	Candidates.Reserve(UE_ARRAY_COUNT(Directions) * UE_ARRAY_COUNT(Distances));
	for (int32 DirectionIndex = 0; DirectionIndex < UE_ARRAY_COUNT(Directions); ++DirectionIndex)
	{
		const FVector Direction = Directions[DirectionIndex];
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		for (float Distance : Distances)
		{
			Candidates.Add(TargetLocation + Direction * Distance);
		}
	}

	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const float HoldDuration = 3.05f;
	FBHResolvedJumpscareSpawn ResolvedSpawn;
	if (!ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 900.0f, 2800.0f, 58.0f, ResolvedSpawn))
	{
		RequestingController->ClientShowStatusMessage(TEXT("No camera-visible test route found; using close visual."), 2.75f);
		TriggerCloseOverlayJumpscare(Target, Variant, FString::Printf(TEXT("Jumpscare test: %s"), *TestLabel), HoldDuration, 36.0f, 38.0f);
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = RequestingController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(ResolvedSpawn.SpawnLocation, ResolvedSpawn.SpawnRotation, SpawnParams);
	if (!Monster)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Could not spawn the jumpscare test monster; using close visual."), 3.0f);
		TriggerCloseOverlayJumpscare(Target, Variant, FString::Printf(TEXT("Jumpscare test: %s"), *TestLabel), HoldDuration, 36.0f, 38.0f);
		return;
	}

	Monster->Configure(Target, 9400.0f, 8.4f, HoldDuration);
	Monster->ConfigureVariant(Variant);

	if (TargetPC)
	{
		SendJumpscareChargeCue(Target, Variant, ResolvedSpawn.FocusLocation, FString::Printf(TEXT("Jumpscare test: %s"), *TestLabel), HoldDuration, 1.0f, false);
		TargetPC->ClientShowStatusMessage(FString::Printf(TEXT("Jumpscare test: %s"), *TestLabel), 2.75f);
	}

	FreezeTargetForJumpscare(Target, HoldDuration);
	CutLightsForJumpscare(TargetLocation, ResolvedSpawn.SpawnLocation, 2200.0f, 8.5f);
	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(44.0f);
	Target->AddDread(44.0f);

	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt jumpscare test fired: %s id=%s target=%s"),
		*TestLabel,
		*Variant.VariantId.ToString(),
		*GetNameSafe(Target));
}

bool ABHGameMode::ResolveSuperJumpscareRoute(ABHCharacter* Target, ABHPlayerController* TargetPC, FVector& OutPeekStart, FVector& OutPeekEnd, FVector& OutCrossStart, FVector& OutCrossEnd, FVector& OutChargeBendStart, FVector& OutChargeStart) const
{
	UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	FVector Forward = TargetPC ? TargetPC->GetControlRotation().Vector() : Target->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = Target->GetActorForwardVector().GetSafeNormal2D();
	}
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		return false;
	}

	const float TargetFloorZ = TargetLocation.Z - Target->GetSimpleCollisionHalfHeight();
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewForward = FVector::ForwardVector;
	if (!BHResolveJumpscareView(Target, TargetPC, ViewLocation, ViewForward))
	{
		return false;
	}
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(BHSuperJumpscareRoute), false, Target);
	TraceParams.AddIgnoredActor(Target);

	auto Grounded = [TargetFloorZ](const FVector& Location)
	{
		return FVector(Location.X, Location.Y, TargetFloorZ + 4.0f);
	};
	auto HasVisibleFocus = [World, &TraceParams, ViewLocation, ViewForward](const FVector& Location)
	{
		const FVector FocusLocation = Location + FVector(0.0f, 0.0f, 150.0f);
		return BHJumpscareFocusInsideViewCone(ViewLocation, ViewForward, FocusLocation)
			&& BHJumpscareTraceToFocusClear(World, ViewLocation, FocusLocation, TraceParams);
	};
	auto HasClearSweep = [World, &TraceParams](const FVector& Start, const FVector& End, float Radius)
	{
		FHitResult Hit;
		const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);
		return !World->SweepSingleByChannel(Hit, Start + FVector(0.0f, 0.0f, 108.0f), End + FVector(0.0f, 0.0f, 108.0f), FQuat::Identity, ECC_Visibility, Shape, TraceParams);
	};
	auto HasSpawnSpace = [World, &TraceParams](const FVector& Location)
	{
		// Slimmer than the monster's own capsule so the chain can route through tighter rooms/corridors; the
		// figure is only on-screen for a beat, so a snug fit reads fine.
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(40.0f, 110.0f);
		return !World->OverlapBlockingTestByChannel(Location + FVector(0.0f, 0.0f, 110.0f), FQuat::Identity, ECC_WorldStatic, Shape, TraceParams);
	};

	const int32 FirstSide = FMath::RandBool() ? 1 : -1;
	const int32 SideOptions[] = { FirstSide, -FirstSide };
	// Include short distances + tighter side scales so a route is found in small rooms, not just open halls —
	// otherwise the whole chain collapses to the single direct-charge fallback.
	const float ForwardDistances[] = { 850.0f, 1100.0f, 1400.0f, 1700.0f, 2050.0f, 2450.0f };
	const float SideScales[] = { 1.0f, 0.78f, 0.58f, 0.42f, 0.30f };
	for (int32 Side : SideOptions)
	{
		for (float ForwardDistance : ForwardDistances)
		{
			for (float SideScale : SideScales)
			{
				const float CrossDistance = FMath::Clamp(ForwardDistance * 0.78f, 1180.0f, 1900.0f);
				FVector PeekStart = Grounded(TargetLocation + Forward * ForwardDistance + Right * (static_cast<float>(Side) * 1140.0f * SideScale));
				FVector PeekEnd = Grounded(TargetLocation + Forward * ForwardDistance + Right * (static_cast<float>(Side) * 720.0f * SideScale));
				FVector CrossStart = Grounded(TargetLocation + Forward * CrossDistance + Right * (static_cast<float>(Side) * 1280.0f * SideScale));
				FVector CrossEnd = Grounded(TargetLocation + Forward * CrossDistance - Right * (static_cast<float>(Side) * 1120.0f * SideScale));
				FVector ChargeBendStart = Grounded(TargetLocation + Forward * (ForwardDistance + 1240.0f) - Right * (static_cast<float>(Side) * 1280.0f * SideScale));
				FVector ChargeStart = Grounded(TargetLocation + Forward * (ForwardDistance + 760.0f) - Right * (static_cast<float>(Side) * 560.0f * SideScale));
				const FVector ChargeEnd = Grounded(TargetLocation + Forward * 170.0f);

				if (!HasSpawnSpace(PeekStart) || !HasSpawnSpace(PeekEnd) || !HasSpawnSpace(CrossStart) || !HasSpawnSpace(CrossEnd) || !HasSpawnSpace(ChargeBendStart) || !HasSpawnSpace(ChargeStart))
				{
					continue;
				}
				// Require the two beats the player must actually SEE — the peek reveal and the final charge start.
				// The wide cross-screen sprint endpoints sit far to the sides and often fall outside the view
				// cone in tight rooms; demanding them too was the main reason the whole chain fell back.
				if (!HasVisibleFocus(PeekEnd) || !HasVisibleFocus(ChargeStart))
				{
					continue;
				}
				if (!HasClearSweep(PeekStart, PeekEnd, 34.0f) || !HasClearSweep(CrossStart, CrossEnd, 40.0f) || !HasClearSweep(ChargeBendStart, ChargeStart, 46.0f) || !HasClearSweep(ChargeStart, ChargeEnd, 52.0f))
				{
					continue;
				}

				OutPeekStart = PeekStart;
				OutPeekEnd = PeekEnd;
				OutCrossStart = CrossStart;
				OutCrossEnd = CrossEnd;
				OutChargeBendStart = ChargeBendStart;
				OutChargeStart = ChargeStart;
				return true;
			}
		}
	}

	return false;
}

void ABHGameMode::TriggerTesterSuperJumpscare(ABHPlayerController* RequestingController, const FBHJumpscareVariant* ForcedVariant)
{
	if (!RequestingController || !GetWorld())
	{
		return;
	}

	ABHCharacter* Target = Cast<ABHCharacter>(RequestingController->GetPawn());
	const ABHPlayerState* BHPS = RequestingController->GetPlayerState<ABHPlayerState>();
	if (!Target || !BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Super jumpscare testing needs an alive player pawn."), 3.0f);
		return;
	}

	FBHJumpscareVariant Variant = ForcedVariant ? *ForcedVariant : ChooseJumpscareVariant(EBHScareEventType::MonsterCharge);
	TArray<FBHJumpscareVariant> ImportedVariants;
	if (!ForcedVariant)
	{
		for (const FBHJumpscareVariant& Candidate : GetResolvedJumpscareVariants())
		{
			if ((BHIsFabJumpscarePackVariant(Candidate) || IsWhisperJumpscareVariant(Candidate)) && BHJumpscareVariantHasExistingVisual(Candidate))
			{
				ImportedVariants.Add(Candidate);
			}
		}
	}
	if (!ImportedVariants.IsEmpty())
	{
		Variant = ImportedVariants[FMath::RandRange(0, ImportedVariants.Num() - 1)];
	}

	FVector PeekStart;
	FVector PeekEnd;
	FVector CrossStart;
	FVector CrossEnd;
	FVector ChargeBendStart;
	FVector ChargeStart;
	if (!ResolveSuperJumpscareRoute(Target, RequestingController, PeekStart, PeekEnd, CrossStart, CrossEnd, ChargeBendStart, ChargeStart))
	{
		RequestingController->ClientShowStatusMessage(TEXT("No clear super-jumpscare route found; falling back to direct impact test."), 3.5f);
		const FString VariantName = Variant.DisplayName.IsEmpty() ? Variant.VariantId.ToString() : Variant.DisplayName;
		TriggerTesterJumpscareVariant(RequestingController, Variant, FString::Printf(TEXT("Super fallback %s"), *VariantName));
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const float StageFocusHeight = FMath::Clamp(FocusHeight - 20.0f, 110.0f, 150.0f);
	const float TotalLockSeconds = 6.35f;
	const float CrossDelaySeconds = 1.55f;
	const float BendDelaySeconds = 3.05f;
	const float ChargeDelaySeconds = 4.55f;
	const FLinearColor StageColor = Variant.LightColor.A > 0.0f ? Variant.LightColor : FLinearColor(1.0f, 0.04f, 0.02f, 1.0f);
	TWeakObjectPtr<ABHGameMode> WeakGameMode(this);
	TWeakObjectPtr<ABHPlayerController> WeakController(RequestingController);
	TWeakObjectPtr<ABHCharacter> WeakTarget(Target);

	auto SendStageCue = [](ABHPlayerController* TargetPC, const FVector& FocusLocation, const FString& Message, const FBHJumpscareVariant& CueVariant, const FLinearColor& CueColor, float Duration, float LockSeconds, float Shake, float Flash, float Jitter, float AudioVolume)
	{
		if (!TargetPC)
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = FocusLocation;
		Cue.Message = Message;
		Cue.DurationSeconds = Duration;
		Cue.LockSeconds = LockSeconds;
		Cue.ShakeIntensity = FMath::Clamp(Shake, 0.0f, 1.0f);
		Cue.CameraJitterDuration = FMath::Clamp(Jitter, 0.0f, 3.0f);
		Cue.CameraJitterFrequency = 34.0f;
		Cue.FlashIntensity = FMath::Clamp(Flash, 0.0f, 1.0f);
		Cue.FlashColor = CueColor;
		if (AudioVolume > 0.0f)
		{
			Cue.AudioAsset = CueVariant.LaunchSound;
			Cue.AudioVolume = AudioVolume;
		}
		Cue.VariantId = CueVariant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		Cue.bCloseRangeFocus = false;
		TargetPC->ClientSnapViewToFlatFocus(FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);
		if (!Message.IsEmpty())
		{
			TargetPC->ClientShowStatusMessage(Message, FMath::Min(Duration, 2.6f));
		}
	};

	SendStageCue(RequestingController, PeekEnd + FVector(0.0f, 0.0f, StageFocusHeight), TEXT("Something sees you."), Variant, StageColor, 1.20f, TotalLockSeconds, 0.34f, 0.08f, 0.75f, 0.0f);
	BHSpawnScriptedJumpscareStage(GetWorld(), RequestingController, Variant, TArray<FVector>{ PeekStart, PeekEnd }, 720.0f, 1.05f, Target, true, false);

	FTimerDelegate CrossDelegate;
	CrossDelegate.BindLambda([WeakController, Variant, StageColor, CrossStart, CrossEnd, StageFocusHeight, TotalLockSeconds, CrossDelaySeconds]()
	{
		ABHPlayerController* TargetPC = WeakController.Get();
		if (!TargetPC)
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = (CrossStart + CrossEnd) * 0.5f + FVector(0.0f, 0.0f, StageFocusHeight);
		Cue.Message = TEXT("");
		Cue.DurationSeconds = 0.95f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - CrossDelaySeconds);
		Cue.ShakeIntensity = 0.52f;
		Cue.CameraJitterDuration = 0.90f;
		Cue.CameraJitterFrequency = 40.0f;
		Cue.FlashIntensity = 0.18f;
		Cue.FlashColor = StageColor;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		TargetPC->ClientSnapViewToFlatFocus(Cue.FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);

		BHSpawnScriptedJumpscareStage(TargetPC->GetWorld(), TargetPC, Variant, TArray<FVector>{ CrossStart, CrossEnd }, 5600.0f, 0.95f, nullptr, false, false);
	});
	FTimerHandle CrossTimerHandle;
	GetWorldTimerManager().SetTimer(CrossTimerHandle, CrossDelegate, CrossDelaySeconds, false);

	FTimerDelegate ChargeDelegate;
	FTimerDelegate BendDelegate;
	BendDelegate.BindLambda([WeakController, Variant, StageColor, ChargeBendStart, ChargeStart, StageFocusHeight, TotalLockSeconds, BendDelaySeconds]()
	{
		ABHPlayerController* TargetPC = WeakController.Get();
		if (!TargetPC)
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = ChargeStart + FVector(0.0f, 0.0f, StageFocusHeight);
		Cue.Message = TEXT("It moved.");
		Cue.DurationSeconds = 0.95f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - BendDelaySeconds);
		Cue.ShakeIntensity = 0.56f;
		Cue.CameraJitterDuration = 0.90f;
		Cue.CameraJitterFrequency = 42.0f;
		Cue.FlashIntensity = 0.18f;
		Cue.FlashColor = StageColor;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		TargetPC->ClientSnapViewToFlatFocus(Cue.FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);
		TargetPC->ClientShowStatusMessage(Cue.Message, 1.35f);

		BHSpawnScriptedJumpscareStage(TargetPC->GetWorld(), TargetPC, Variant, TArray<FVector>{ ChargeBendStart, ChargeStart }, 3300.0f, 0.95f, nullptr, false, false);
	});
	FTimerHandle BendTimerHandle;
	GetWorldTimerManager().SetTimer(BendTimerHandle, BendDelegate, BendDelaySeconds, false);

	ChargeDelegate.BindLambda([WeakGameMode, WeakController, WeakTarget, Variant, StageColor, ChargeStart, StageFocusHeight, TotalLockSeconds, ChargeDelaySeconds]()
	{
		ABHGameMode* GameMode = WeakGameMode.Get();
		ABHPlayerController* TargetPC = WeakController.Get();
		ABHCharacter* TargetCharacter = WeakTarget.Get();
		if (!GameMode || !TargetPC || !TargetCharacter || !GameMode->GetWorld())
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = ChargeStart + FVector(0.0f, 0.0f, StageFocusHeight);
		Cue.Message = TEXT("It is coming.");
		Cue.DurationSeconds = 1.55f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - ChargeDelaySeconds);
		Cue.ShakeIntensity = 0.78f;
		Cue.CameraJitterDuration = 1.35f;
		Cue.CameraJitterFrequency = 48.0f;
		Cue.FlashIntensity = 0.32f;
		Cue.FlashColor = StageColor;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		TargetPC->ClientSnapViewToFlatFocus(Cue.FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);
		TargetPC->ClientShowStatusMessage(Cue.Message, 2.0f);

		const FVector DirectionToTarget = (TargetCharacter->GetActorLocation() - ChargeStart).GetSafeNormal2D();
		const FRotator SpawnRotation = DirectionToTarget.IsNearlyZero() ? FRotator::ZeroRotator : DirectionToTarget.Rotation();
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = TargetPC;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABHJumpscareMonster* Monster = GameMode->GetWorld()->SpawnActor<ABHJumpscareMonster>(ChargeStart, SpawnRotation, SpawnParams);
		if (Monster)
		{
			Monster->Configure(TargetCharacter, 8200.0f, 4.30f, 0.0f, false);
			Monster->ConfigureVariant(Variant);
		}
	});
	FTimerHandle ChargeTimerHandle;
	GetWorldTimerManager().SetTimer(ChargeTimerHandle, ChargeDelegate, ChargeDelaySeconds, false);

	FreezeTargetForJumpscare(Target, TotalLockSeconds);
	CutLightsForJumpscare(TargetLocation, ChargeBendStart, 3200.0f, 10.5f);
	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(72.0f);
	Target->AddDread(82.0f);

	const FString VariantName = Variant.DisplayName.IsEmpty() ? Variant.VariantId.ToString() : Variant.DisplayName;
	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Super jumpscare chain armed: %s"), *VariantName), 3.0f);
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt super jumpscare chain fired: %s id=%s target=%s"),
		*VariantName,
		*Variant.VariantId.ToString(),
		*GetNameSafe(Target));
}

bool ABHGameMode::IsRevisionMode() const
{
	return bRevisionMode;
}

EBHRevisionDifficultyMix ABHGameMode::GetRevisionDifficultyMix() const
{
	return RevisionDifficultyMix;
}

TArray<EBHPhysicsTopic> ABHGameMode::GetRevisionWeakTopics() const
{
	return {GetWeakestRevisionTopic()};
}

int32 ABHGameMode::ResolveRevisionQuestionTargetFor(int32 StudentCount, int32 StageIndex)
{
	// Raised by one across the board (was 2-3). With mastery now cumulative across the session's stages,
	// more questions per node simply means more genuine retrievals toward each student's durable mastery
	// rather than a steeper one-off bar.
	if (StudentCount >= 6)
	{
		return StageIndex <= 0 ? 3 : 4;
	}
	return 3;
}

int32 ABHGameMode::ResolveRevisionNodeTargetFor(int32 StudentCount, int32 StageIndex, int32 StationCount)
{
	const int32 StageNodeFloor = StageIndex <= 0 ? 8 : (StageIndex == 1 ? 10 : 12);
	const int32 StudentNodeTarget = StudentCount >= 10 ? 18 : (StudentCount >= 6 ? 12 : 8);
	const int32 RevisionNodeTarget = FMath::Max(StageNodeFloor, StudentNodeTarget);
	return FMath::Clamp(RevisionNodeTarget, 0, FMath::Max(0, StationCount));
}

bool ABHGameMode::RevisionNodeAnswerCapApplies(int32 HumanStudentCount, int32 PerNodeAnswerTarget)
{
	// A node needs PerNodeAnswerTarget DISTINCT correct answers, and each student may answer it once. So the
	// once-per-node cap is satisfiable (and therefore worth enforcing for coverage) only when at least that
	// many live humans are present. With fewer humans the node can never reach the target, so relax the cap.
	return HumanStudentCount >= FMath::Max(1, PerNodeAnswerTarget);
}

bool ABHGameMode::ShouldEnforceRevisionNodeAnswerCap() const
{
	if (!bRevisionMode)
	{
		// The once-per-node spread rule only exists in revision mode; standard Hunt nodes are unaffected.
		return true;
	}
	// Humans-only count (bots are excluded from every revision score, so they cannot fill a node's quota).
	const int32 HumanStudents = CountActiveRevisionStudents(GameState, true);
	return RevisionNodeAnswerCapApplies(HumanStudents, GetRevisionQuestionTargetPerNode());
}

FVector ABHGameMode::ResolveFoggroundsDoorFrameOrigin(const FVector& DoorLocation)
{
	return FVector(DoorLocation.X, DoorLocation.Y, 0.0f);
}

int32 ABHGameMode::GetRevisionQuestionTargetPerNode() const
{
	int32 StudentCount = CountActiveRevisionStudents(GameState, true);
	if (StudentCount <= 0)
	{
		StudentCount = CountActiveRevisionStudents(GameState, false);
	}

	return ResolveRevisionQuestionTargetFor(StudentCount, RuntimeStageIndex);
}

int32 ABHGameMode::GetRevisionAnswerTeamTargetSize() const
{
	int32 StudentCount = CountActiveRevisionStudents(GameState, true);
	if (StudentCount <= 0)
	{
		StudentCount = CountActiveRevisionStudents(GameState, false);
	}

	if (StudentCount <= 0)
	{
		return 1;
	}
	if (StudentCount <= 3)
	{
		return StudentCount;
	}
	// Smaller answer teams (was 4 for 4-9 students, 5 for 10+). A team of 3 means each student must
	// personally cast more votes, and a single vote swings the majority more, so individual answering
	// carries more weight -- reinforcing the no-free-rider per-voter grading.
	return 3;
}

int32 ABHGameMode::ComputeLiveRevisionContributionTarget() const
{
	// Raised from 5-6. The per-round contribution gate now measures genuine individual work: with per-voter
	// grading a vote only counts when the student is personally right, and mastery is durable across stages,
	// so each round can fairly ask for a larger, hand-earned set of correct answers. Ramps 8 -> 10 -> 12.
	return FMath::Clamp(8 + 2 * FMath::Clamp(RuntimeStageIndex, 0, 2), 8, 12);
}

int32 ABHGameMode::GetRevisionMinimumContributionTarget() const
{
	// Return the frozen per-round snapshot, NOT the live formula. This keeps the enforced gate
	// (CanUseHallMonitorTools) and every displayed/replicated contribution requirement identical and
	// stable for the whole round, so students joining/leaving cannot shift the threshold mid-round.
	return FMath::Clamp(RevisionContributionGateTarget, 1, 6);
}

// --- Revision-mode round-end enforcement (docs/ROADMAP_MOVEMENT_REVISION.md WS2) ----------------------------
static TAutoConsoleVariable<float> CVarBHAllCaughtGraceSeconds(
	TEXT("bh.AllCaughtGraceSeconds"),
	75.0f,
	TEXT("Revision mode: when ALL survivors are caught, hold the round open this many seconds so hall monitors can finish their revision contributions before it ends. 0 = end immediately. (Default 75)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHMonitorDockPctPerQuestion(
	TEXT("bh.MonitorDockPctPerQuestion"),
	0.12f,
	TEXT("Revision mode: fraction of a monitor's question points docked per contribution they are SHORT of the round target when the round ends unfinished (scales with how much they missed, so a monitor who could not reach the nodes is not over-punished). (Default 0.12)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarBHMonitorDockMaxPct(
	TEXT("bh.MonitorDockMaxPct"),
	0.6f,
	TEXT("Revision mode: hard cap on the unfinished-revision monitor point dock. (Default 0.6)"),
	ECVF_Default);

// --- Catch-pressure loop CVars (docs/CATCH_PRESSURE.md) ------------------------------------------------------
// Escalating recapture tax: fraction of unspent question points lost, climbing per catch from Base by Step, capped.
static TAutoConsoleVariable<float> CVarBHRecaptureTaxBase(
	TEXT("bh.RecaptureTaxBase"), 0.25f,
	TEXT("Catch-pressure: question-point penalty fraction on a player's FIRST capture of the round. (Default 0.25 = today's value)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHRecaptureTaxStep(
	TEXT("bh.RecaptureTaxStep"), 0.15f,
	TEXT("Catch-pressure: extra penalty fraction added per prior capture this round (escalating recapture tax). (Default 0.15)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHRecaptureTaxMax(
	TEXT("bh.RecaptureTaxMax"), 0.60f,
	TEXT("Catch-pressure: hard cap on the recapture tax fraction. (Default 0.60)"),
	ECVF_Default);

// Closing dark: each capture bumps the escalation level (capped); the level feeds a Hunter sprint multiplier.
static TAutoConsoleVariable<int32> CVarBHCatchEscalationCap(
	TEXT("bh.CatchEscalationCap"), 5,
	TEXT("Catch-pressure: max closing-dark escalation level (captures beyond this stop ramping). (Default 5)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHCatchEscalationSprintPerStep(
	TEXT("bh.CatchEscalationSprintPerStep"), 0.02f,
	TEXT("Catch-pressure: Hunter sprint-speed bonus added per escalation level. (Default 0.02 = +2%/catch)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHCatchEscalationSprintMaxBonus(
	TEXT("bh.CatchEscalationSprintMaxBonus"), 0.10f,
	TEXT("Catch-pressure: hard cap on the closing-dark Hunter sprint bonus. (Default 0.10 = +10%)"),
	ECVF_Default);

// Class lifeline pool: shared lives. Pool = clamp(Base + floor(PerSurvivor * survivors), 1, Max). At 0 the next
// un-rescued capture is a true elimination (spectate) instead of a free Hall-Monitor respawn.
static TAutoConsoleVariable<int32> CVarBHClassLifelinesEnabled(
	TEXT("bh.ClassLifelinesEnabled"), 1,
	TEXT("Catch-pressure: 1 = arm the shared class lifeline pool (true-elimination at zero); 0 = always convert to monitor. (Default 1)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHClassLifelinesBase(
	TEXT("bh.ClassLifelinesBase"), 2,
	TEXT("Catch-pressure: base class lifelines before the per-survivor bonus. (Default 2)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHClassLifelinesPerSurvivor(
	TEXT("bh.ClassLifelinesPerSurvivor"), 0.5f,
	TEXT("Catch-pressure: extra class lifelines per starting survivor (floored). (Default 0.5)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHClassLifelinesMax(
	TEXT("bh.ClassLifelinesMax"), 6,
	TEXT("Catch-pressure: hard cap on the class lifeline pool. (Default 6)"),
	ECVF_Default);

// Climb-back: a monitor who reaches (monitor gate + this many) contributions returns to Survivor mid-round.
static TAutoConsoleVariable<int32> CVarBHClimbBackExtraContributions(
	TEXT("bh.ClimbBackExtraContributions"), 3,
	TEXT("Catch-pressure: contributions ABOVE the monitor tool-unlock gate needed for a monitor to climb back to Survivor. (Default 3)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHClimbBackToolActions(
	TEXT("bh.ClimbBackToolActions"), 4,
	TEXT("Catch-pressure: NON-revision climb-back currency -- successful monitor tool uses needed to return to Survivor. (Default 4)"),
	ECVF_Default);

// Detention hold + rescue. SHIPS OFF: needs an in-engine + 3-client smoke test of the cell-camp balance and the
// un-hide/collision ordering before it goes live. When off, a capture converts straight to monitor as today.
static TAutoConsoleVariable<int32> CVarBHDetentionEnabled(
	TEXT("bh.DetentionEnabled"), 0,
	TEXT("Catch-pressure: 1 = caught survivors are held in a detention cell (teammate can rescue) before converting; 0 = instant convert (today's behaviour). (Default 0)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHDetentionHoldSeconds(
	TEXT("bh.DetentionHoldSeconds"), 12.0f,
	TEXT("Catch-pressure: base detention/ransom window in seconds before an un-rescued captive converts. (Default 12)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHDetentionHoldReducePerCatch(
	TEXT("bh.DetentionHoldReducePerCatch"), 2.5f,
	TEXT("Catch-pressure: seconds shaved off the detention window per prior capture this round (repeat offenders convert faster). (Default 2.5)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHDetentionHoldMinSeconds(
	TEXT("bh.DetentionHoldMinSeconds"), 6.0f,
	TEXT("Catch-pressure: floor on the detention window however many times a player has been caught. (Default 6)"),
	ECVF_Default);
static TAutoConsoleVariable<float> CVarBHDetentionRescueHoldSeconds(
	TEXT("bh.DetentionRescueHoldSeconds"), 1.75f,
	TEXT("Catch-pressure: how long a teammate must hold the rescue interact to free a captive. (Default 1.75)"),
	ECVF_Default);
static TAutoConsoleVariable<int32> CVarBHDetentionRescueReward(
	TEXT("bh.DetentionRescueReward"), 15,
	TEXT("Catch-pressure: question points awarded to a teammate who frees a captive from detention. (Default 15)"),
	ECVF_Default);

float ABHGameMode::ComputeRecaptureTaxFraction(int32 InTimesCaught) const
{
	return BHComputeRecaptureTaxFraction(
		InTimesCaught,
		CVarBHRecaptureTaxBase.GetValueOnGameThread(),
		CVarBHRecaptureTaxStep.GetValueOnGameThread(),
		CVarBHRecaptureTaxMax.GetValueOnGameThread());
}

float ABHGameMode::ComputeDetentionHoldSeconds(int32 InTimesCaught) const
{
	return BHComputeDetentionHoldSeconds(
		CVarBHDetentionHoldSeconds.GetValueOnGameThread(),
		InTimesCaught,
		CVarBHDetentionHoldReducePerCatch.GetValueOnGameThread(),
		CVarBHDetentionHoldMinSeconds.GetValueOnGameThread());
}

int32 ABHGameMode::GetClimbBackContributionTarget() const
{
	// One clear bar above the monitor tool-unlock gate: unlock tools first, then keep working to earn a return
	// to Survivor. Clamped so it can never dip to/under the gate (which would make climb-back instant).
	const int32 Gate = GetRevisionMinimumContributionTarget();
	const int32 Extra = FMath::Max(1, CVarBHClimbBackExtraContributions.GetValueOnGameThread());
	return Gate + Extra;
}

int32 ABHGameMode::CountMonitorsBelowRevisionTarget() const
{
	if (!bRevisionMode)
	{
		return 0;
	}
	const int32 Target = GetRevisionMinimumContributionTarget();
	int32 Count = 0;
	if (!GetWorld())
	{
		return 0;
	}
	// Iterate CONNECTED player controllers (not GameState->PlayerArray) so this stays symmetric with
	// ApplyUnfinishedMonitorRevisionDock, which can only message/dock players that have a controller. A monitor who
	// disconnected mid-revision is then neither counted (so they can't hold the grace window open for its full
	// duration) nor docked. Bots have AIControllers, not player controllers, so they are excluded here too.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter
			&& BHPS->LifeState == EBHPlayerLifeState::Alive && !BHPS->IsABot()
			&& BHPS->RevisionStats.ContributionCount < Target)
		{
			++Count;
		}
	}
	return Count;
}

bool ABHGameMode::TickAllCaughtRevisionGrace()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!bRevisionMode || !BHGS)
	{
		if (BHGS)
		{
			BHGS->SetAllCaughtGraceRemaining(-1);
		}
		return false;
	}

	const int32 GraceSeconds = FMath::Max(0, FMath::RoundToInt(CVarBHAllCaughtGraceSeconds.GetValueOnGameThread()));
	const int32 Unfinished = CountMonitorsBelowRevisionTarget();
	if (Unfinished <= 0 || GraceSeconds <= 0)
	{
		// Monitors are done (end early -- never waste class time) or the grace is disabled: allow the end.
		BHGS->SetAllCaughtGraceRemaining(-1);
		return false;
	}

	int32 Remaining = BHGS->AllCaughtGraceRemaining;
	if (Remaining < 0)
	{
		// First detection of all-caught + unfinished: open the window. No decrement this call, so monitors get the
		// full window whether it was first hit on the capture event or on the 1s tick.
		Remaining = GraceSeconds;
		BroadcastStatus(FString::Printf(TEXT("All students caught -- hall monitors must finish revision (%d still short). %ds left, then a point dock."), Unfinished, Remaining), 5.0f);
	}
	else
	{
		// Subsequent (1s-cadence) calls tick the window down.
		Remaining = FMath::Max(0, Remaining - 1);
	}
	BHGS->SetAllCaughtGraceRemaining(Remaining);
	// Hold the round open while time remains; once it expires, allow the end (EndRound applies the dock).
	return Remaining > 0;
}

void ABHGameMode::ApplyUnfinishedMonitorRevisionDock()
{
	if (!bRevisionMode || !GetWorld())
	{
		return;
	}
	const int32 Target = GetRevisionMinimumContributionTarget();
	const float PctPerQuestion = FMath::Max(0.0f, CVarBHMonitorDockPctPerQuestion.GetValueOnGameThread());
	const float MaxPct = FMath::Clamp(CVarBHMonitorDockMaxPct.GetValueOnGameThread(), 0.0f, 1.0f);
	if (PctPerQuestion <= 0.0f || MaxPct <= 0.0f)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		// Mirror CountMonitorsBelowRevisionTarget exactly (incl. the alive check) so we only ever dock a monitor the
		// gate actually counted as holding the round open -- never an eliminated/converted one.
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::FakeHunter || BHPS->IsABot()
			|| BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		const int32 ShortBy = Target - BHPS->RevisionStats.ContributionCount;
		if (ShortBy <= 0)
		{
			continue;
		}
		// Proportional to the shortfall (per missing contribution), capped -- fair to a monitor who genuinely
		// could not reach enough nodes, harsh on one who ignored revision entirely.
		const float DockPct = FMath::Clamp(PctPerQuestion * static_cast<float>(ShortBy), 0.0f, MaxPct);
		if (DockPct <= 0.0f)
		{
			continue;
		}
		const int32 Lost = BHPS->ApplyCaughtQuestionPointPenalty(DockPct);
		if (Lost > 0)
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("Revision unfinished (%d/%d) -- docked %d question points."), BHPS->RevisionStats.ContributionCount, Target, Lost), 5.0f);
		}
	}
}

void ABHGameMode::RefreshRevisionContributionGateTarget()
{
	RevisionContributionGateTarget = ComputeLiveRevisionContributionTarget();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionContributionTarget(RevisionContributionGateTarget);
	}
}

bool ABHGameMode::IsRevisionParticipantRole(EBHPlayerRole Role)
{
	return Role == EBHPlayerRole::Survivor || Role == EBHPlayerRole::FakeHunter;
}

bool ABHGameMode::IsValidSpectatorRolePreference(EBHPlayerRole Role)
{
	return Role == EBHPlayerRole::Hunter || Role == EBHPlayerRole::Survivor || Role == EBHPlayerRole::FakeHunter;
}

EBHPlayerRole ABHGameMode::SanitizeSpectatorRolePreference(EBHPlayerRole Role)
{
	return IsValidSpectatorRolePreference(Role) ? Role : EBHPlayerRole::Unassigned;
}

bool ABHGameMode::CanUseHallMonitorTools(const ABHPlayerState* PlayerState, FString& OutBlockReason) const
{
	OutBlockReason.Reset();
	if (!bRevisionMode
		|| !PlayerState
		|| PlayerState->PlayerRole != EBHPlayerRole::FakeHunter
		|| PlayerState->LifeState != EBHPlayerLifeState::Alive
		|| PlayerState->IsABot())
	{
		return true;
	}

	const int32 RequiredContributions = GetRevisionMinimumContributionTarget();
	const int32 CurrentContributions = PlayerState->RevisionStats.ContributionCount;
	if (CurrentContributions >= RequiredContributions)
	{
		return true;
	}

	OutBlockReason = FString::Printf(
		TEXT("Hall monitor tools locked: join an answer team at a station (%d/%d contributions). Monitors still count toward 70/50."),
		CurrentContributions,
		RequiredContributions);
	return false;
}

bool ABHGameMode::IsRevisionAdmin(const ABHPlayerController* RequestingController) const
{
	return IsHostAdminController(RequestingController);
}

void ABHGameMode::GetAdaptiveRevisionPlan(const ABHPlayerState* PlayerState, bool bLastAnswerCorrect, EBHPhysicsTopic& OutTopic, EBHQuestionDifficulty& OutDifficulty, FString& OutReason) const
{
	if (!PlayerState)
	{
		OutTopic = GetWeakestRevisionTopic();
		OutDifficulty = ClampRevisionDifficulty(RevisionStartingDifficulty);
		OutReason = TEXT("No student profile yet; using the current class weak topic.");
		return;
	}

	const FBHPlayerRevisionStats& Stats = PlayerState->RevisionStats;
	OutTopic = WeakestTopicForStats(Stats, RevisionTopicMask);
	// Calibrate difficulty to the targeted (weakest) topic's mastery, not the cross-topic average,
	// so a student who is strong overall but weak here is eased in rather than over-faced.
	OutDifficulty = AdaptiveDifficultyForStats(Stats, RevisionTopicMastery(Stats, OutTopic), bLastAnswerCorrect);
	// Host "starting difficulty": the student's first question starts at the configured tier instead of
	// always Easy; every adaptive pick is then clamped into the host [Min, Max] band.
	if (Stats.Attempts <= 0)
	{
		OutDifficulty = RevisionStartingDifficulty;
	}
	OutDifficulty = ClampRevisionDifficulty(OutDifficulty);
	OutReason = FString::Printf(TEXT("Mastery %.0f%% after %d attempt(s); next question targets %s at %s difficulty."),
		Stats.MasteryPercent,
		Stats.Attempts,
		*FBHRevisionQuestionBank::TopicToString(OutTopic),
		*FBHRevisionQuestionBank::DifficultyToString(OutDifficulty));
}

int32 ABHGameMode::RecordRevisionAnswer(ABHCharacter* Character, const FBHRevisionQuestion& Question, bool bCorrect, bool bCorrection, const FString& SelectedAnswer, const FString& TeamSummary, bool bCountsAsContribution, bool bBonusPoints)
{
	ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!bRevisionMode || !IsRevisionParticipantState(BHPS))
	{
		return 0;
	}

	FBHPlayerRevisionStats Stats = BHPS->RevisionStats;
	const int32 PreviousContributionCount = Stats.ContributionCount;
	Stats.Attempts = FMath::Max(0, Stats.Attempts + 1);
	int32 PointsEarned = 0;

	// Demonstrated + durable mastery (see Docs/REVISION_QUALITY_PLAN.md): harder questions move
	// the needle more, gains shrink as a topic approaches mastery (so it cannot be trivially
	// capped by a few lucky answers), and a miss decays the topic — so blind guessing on
	// 25%-odds multiple choice nets a negative trend rather than steady progress.
	float TopicMastery = RevisionTopicMastery(Stats, Question.Topic);
	if (bCorrect)
	{
		Stats.CorrectAnswers = FMath::Max(0, Stats.CorrectAnswers + 1);
		if (bCountsAsContribution)
		{
			Stats.ContributionCount = FMath::Max(0, Stats.ContributionCount + 1);
		}
		// First-time-correct pays full shop points; recovering a previously-missed question pays
		// half (you earn full value by knowing it first time) but still grants full mastery.
		const int32 BasePoints = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, bBonusPoints);
		PointsEarned = bCorrection ? FMath::Max(1, BasePoints / 2) : BasePoints;
		BHPS->AddQuestionPoints(PointsEarned);
		if (bCorrection)
		{
			Stats.CorrectionsCompleted = FMath::Max(0, Stats.CorrectionsCompleted + 1);
		}

		// Ceiling raised from 1.5 so a visually-answered question's bonus isn't clipped: the caller
		// (ABHObjectiveStation::FinalizeRevisionAnswer) scales Question.MasteryWeight up when the answer
		// was given via the interactive visual method (drag arrangement / diagram element) rather than
		// picking a multiple-choice option, so visual answers build mastery faster. Authored weights are
		// difficulty-based and stay <= 1.5, so non-visual answers are unaffected by the higher ceiling.
		const float DiffMult = FMath::Clamp(Question.MasteryWeight, 1.0f, 3.0f);
		const float Headroom = FMath::Max(1.0f - (TopicMastery / 100.0f) * 0.6f, 0.25f);
		TopicMastery = TopicMastery + 15.0f * DiffMult * Headroom;
	}
	else
	{
		Stats.HintCount = FMath::Max(0, Stats.HintCount + 1);
		// Decay on a miss. A careless easy miss costs the most; a hard miss the least.
		float MissDiffMult = 1.0f;
		switch (Question.Difficulty)
		{
		case EBHQuestionDifficulty::Easy:
			MissDiffMult = 1.2f;
			break;
		case EBHQuestionDifficulty::Hard:
			MissDiffMult = 0.8f;
			break;
		default:
			MissDiffMult = 1.0f;
			break;
		}
		TopicMastery = TopicMastery - 7.0f * MissDiffMult;
	}

	// Spaced-repetition review loop: a miss queues the exact question to be re-asked later;
	// answering it correctly clears it. Done before the review-gate check below so a topic the
	// student has just fully cleared can rise above the review ceiling on this same answer.
	if (bCorrect)
	{
		BHPS->DequeueRevisionReview(Question.Id);
	}
	else
	{
		BHPS->EnqueueRevisionReview(Question.Id);
	}

	// Review-gated ceiling: a topic is held at <= 80% while the student still has an unresolved
	// missed question in it. You must clear your own mistakes to fully master a topic, which
	// keeps the spaced-repetition loop meaningful instead of optional.
	if (HasOutstandingReviewInTopic(BHPS, Question.Topic))
	{
		TopicMastery = FMath::Min(TopicMastery, 80.0f);
	}
	SetRevisionTopicMastery(Stats, Question.Topic, TopicMastery);

	// Unified overall mastery = mean of the enabled topics' mastery (removes the old
	// ratio-vs-additive desync; every HUD/report/gate reader keeps working with a better value).
	Stats.MasteryPercent = MeanEnabledTopicMastery(Stats, RevisionTopicMask);
	BHPS->RevisionStats = Stats;

	if (bCorrect && bCountsAsContribution && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
	{
		const int32 RequiredContributions = GetRevisionMinimumContributionTarget();
		if (PreviousContributionCount < RequiredContributions && Stats.ContributionCount >= RequiredContributions)
		{
			if (ABHPlayerController* PC = Character ? Cast<ABHPlayerController>(Character->GetController()) : nullptr)
			{
				PC->ClientShowStatusMessage(TEXT("Hall monitor tools unlocked. Traps, real hints, and false corridor markers are available."), 3.5f);
			}
		}
	}

	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		FBHQuestionAttemptRecord Attempt;
		Attempt.PlayerName = BHPS->GetPlayerName();
		Attempt.QuestionId = Question.Id;
		Attempt.TopicName = Question.TopicName;
		Attempt.QuestionText = Question.Prompt;
		Attempt.QuestionSubtopic = Question.Subtopic;
		Attempt.SelectedAnswer = SelectedAnswer.IsEmpty()
			? (bCorrect ? TEXT("Correct response") : TEXT("Incorrect response"))
			: SelectedAnswer;
		if (Question.Answer.Choices.IsValidIndex(Question.Answer.CorrectChoiceIndex))
		{
			Attempt.CorrectAnswer = Question.Answer.Choices[Question.Answer.CorrectChoiceIndex];
		}
		Attempt.Explanation = Question.Explanation.IsEmpty() ? Question.Hint : Question.Explanation;
		Attempt.Difficulty = Question.Difficulty;
		Attempt.QuestionType = Question.Type;
		Attempt.Topic = Question.Topic;
		Attempt.bCorrect = bCorrect;
		Attempt.bCorrection = bCorrection;
		Attempt.StageIndex = RuntimeStageIndex;
		Attempt.TimestampSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		Attempt.PointsEarned = PointsEarned;
		Attempt.TeamSummary = TeamSummary;
		GetAdaptiveRevisionPlan(BHPS, bCorrect, Attempt.AdaptiveRecommendedTopic, Attempt.AdaptiveRecommendedDifficulty, Attempt.AdaptiveReason);
		BHGI->RecordQuestionAttempt(Attempt);
	}
	RecordPlaytestTelemetryMarker(
		TEXT("question_answer"),
		Character ? Character->GetActorLocation() : FVector::ZeroVector,
		FString::Printf(TEXT("%s selected=%s"), bCorrect ? TEXT("correct") : TEXT("wrong"), *SelectedAnswer.Left(64)),
		BHPS,
		nullptr,
		Question.Id,
		Question.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(Question.Topic) : Question.TopicName,
		FBHRevisionQuestionBank::DifficultyToString(Question.Difficulty));

	UpdateRevisionSummary(bCorrect
		? FString::Printf(TEXT("%s banked %s mastery."), *BHPS->GetPlayerName(), *Question.TopicName)
		: FString::Printf(TEXT("%s needs a correction in %s."), *BHPS->GetPlayerName(), *Question.TopicName));
	UpdateDirectorGameState(GetRevisionObjectiveText());
	return PointsEarned;
}

bool ABHGameMode::BuildRevisionAnswerTeam(ABHObjectiveStation* Station, ABHCharacter* RequestingCharacter, TSet<int32>& OutPlayerIds, FString& OutSummary) const
{
	OutPlayerIds.Reset();
	OutSummary = TEXT("");
	if (!bRevisionMode || !GameState || !Station)
	{
		return false;
	}

	// Co-located answer team: a question node is answered by the players actually gathered AT this node,
	// NOT a class-wide committee. This is what lets a lone student — or a Hall Monitor working a node by
	// themselves — clear it and earn the contribution, while a cluster still votes together. Because the
	// caller rebuilds this set on every vote, a teammate who wanders off can never soft-lock the node
	// (the old class-wide committee could leave a present player stuck at "Vote recorded (1/5)" forever).
	// Mirrors the multi-repairer breaker model, which already keys progress off who is physically present.
	const FVector StationLocation = Station->GetActorLocation();
	const float RadiusSq = FMath::Square(BHRevisionAnswerTeamRadius);
	const ABHPlayerState* RequestingPS = RequestingCharacter ? RequestingCharacter->GetPlayerState<ABHPlayerState>() : nullptr;

	TArray<FString> Names;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!IsRevisionParticipantState(BHPS) || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}
		// Bots are never team members: an idle bot wandering through the node's radius must not inflate the
		// majority a present human needs to clear the node. A bot that is genuinely working a node alone
		// still resolves it via the lone-voter fallback in ABHObjectiveStation::SubmitAnswer (TeamSize>=1).
		if (BHPS->IsABot())
		{
			continue;
		}
		const bool bIsRequester = RequestingPS && BHPS == RequestingPS;
		const APawn* Pawn = BHPS->GetPawn();
		const bool bPresent = bIsRequester
			|| (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), StationLocation) <= RadiusSq);
		if (bPresent)
		{
			OutPlayerIds.Add(BHPS->GetPlayerId());
			Names.Add(BHPS->GetPlayerName());
		}
	}

	OutSummary = FString::Join(Names, TEXT(", "));
	return OutPlayerIds.Num() > 0;
}

void ABHGameMode::SetPhysicsTopics(ABHPlayerController* RequestingController, const FString& TopicList)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change classroom topics")))
	{
		return;
	}

	RevisionTopicMask = ParsePhysicsTopicMask(TopicList, RevisionTopicMask);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Admin intentionally changed an input that affects the gate target: re-snapshot the frozen value.
		RefreshRevisionContributionGateTarget();
	}
	BroadcastStatus(FString::Printf(TEXT("Physics topics updated. Mask=%d."), RevisionTopicMask), 3.0f);
}

void ABHGameMode::SetRevisionDifficultyMix(ABHPlayerController* RequestingController, const FString& DifficultyMix)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change difficulty mix")))
	{
		return;
	}

	RevisionDifficultyMix = ParseRevisionDifficultyMix(DifficultyMix, RevisionDifficultyMix);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Admin intentionally changed an input that affects the gate target: re-snapshot the frozen value.
		RefreshRevisionContributionGateTarget();
	}
	BroadcastStatus(FString::Printf(TEXT("Revision difficulty mix: %s."), *RevisionDifficultyMixToString(RevisionDifficultyMix)), 3.0f);
}

void ABHGameMode::SetRevisionThresholds(ABHPlayerController* RequestingController, float NewClassThreshold, float NewIndividualThreshold)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change revision thresholds")))
	{
		return;
	}

	RevisionClassThreshold = FMath::Clamp(NewClassThreshold, 0.0f, 100.0f);
	RevisionIndividualThreshold = FMath::Clamp(NewIndividualThreshold, 0.0f, 100.0f);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Admin intentionally changed an input that affects the gate target: re-snapshot the frozen value.
		RefreshRevisionContributionGateTarget();
	}
	UpdateRevisionSummary(TEXT("Thresholds changed by classroom admin."));
	BroadcastStatus(FString::Printf(TEXT("Revision thresholds set: class %.0f%%, individual %.0f%%."), RevisionClassThreshold, RevisionIndividualThreshold), 3.5f);
}

void ABHGameMode::SetRevisionRoundDuration(ABHPlayerController* RequestingController, int32 NewRoundSeconds)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change revision round duration")))
	{
		return;
	}

	RevisionRoundDuration = FMath::Clamp(NewRoundSeconds, 60, 3600);
	HuntSeconds = RevisionRoundDuration;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Admin intentionally changed an input that affects the gate target: re-snapshot the frozen value.
		RefreshRevisionContributionGateTarget();
		if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
		{
			BroadcastStatus(FString::Printf(TEXT("Next classroom hunt duration set to %d seconds."), RevisionRoundDuration), 3.0f);
			return;
		}
	}
	BroadcastStatus(FString::Printf(TEXT("Classroom round duration set to %d seconds."), RevisionRoundDuration), 3.0f);
}

void ABHGameMode::SetScareIntensity(ABHPlayerController* RequestingController, int32 NewScareIntensity)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change scare intensity")))
	{
		return;
	}

	RevisionScareIntensity = FMath::Clamp(NewScareIntensity, 0, 3);
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Admin intentionally changed an input that affects the gate target: re-snapshot the frozen value.
		RefreshRevisionContributionGateTarget();
	}
	BroadcastStatus(FString::Printf(TEXT("Scare intensity set to %d."), RevisionScareIntensity), 3.0f);
}

bool ABHGameMode::ApplyLessonPreset(ABHPlayerController* RequestingController, const FBHLessonPreset& Preset, FString& OutMessage)
{
	if (!RequireHostAdmin(RequestingController, TEXT("apply lesson presets")))
	{
		OutMessage = TEXT("Only the host machine can apply lesson presets.");
		return false;
	}

	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !BHGS->bRevisionMode)
	{
		OutMessage = TEXT("Host Live Classroom before applying a lesson preset to the lobby.");
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(OutMessage, 4.0f);
		}
		return false;
	}

	if (BHGS->RoundPhase != EBHRoundPhase::Lobby)
	{
		OutMessage = TEXT("Finish the active round before applying a lesson preset to the live lobby.");
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(OutMessage, 4.0f);
		}
		return false;
	}

	bool bAdjusted = false;
	const FBHLessonPreset CleanPreset = FBHLessonPresetStore::ValidatePreset(Preset, &bAdjusted);
	RevisionTopicMask = CleanPreset.TopicMask;
	RevisionDifficultyMix = CleanPreset.DifficultyMix;
	RevisionClassThreshold = CleanPreset.ClassThreshold;
	RevisionIndividualThreshold = CleanPreset.IndividualThreshold;
	RevisionRoundDuration = CleanPreset.RoundSeconds;
	RevisionScareIntensity = CleanPreset.ScareIntensity;
	HuntSeconds = RevisionRoundDuration;
	NextRuntimeLevelName = NormalizeBHLevelName(CleanPreset.MapName);

	// Host lobby customization knobs (live in-lobby apply; launch path mirrors this in BuildRuntimeFacility).
	RevisionStartingDifficulty = CleanPreset.StartingDifficulty;
	RevisionMinDifficulty = CleanPreset.MinDifficulty;
	RevisionMaxDifficulty = CleanPreset.MaxDifficulty;
	RevisionQuestionSetId = CleanPreset.QuestionSetId;
	ResolveAllowedQuestionIds();
	RuntimeMapRoute = CleanPreset.MapRoute;
	if (RuntimeMapRoute.Num() > 0)
	{
		// The route's first leg becomes the map the lobby departs to (and the displayed "next" map).
		LobbyFirstLevelName = RuntimeMapRoute[0];
		NextRuntimeLevelName = RuntimeMapRoute[0];
	}
	LayoutSeed = CleanPreset.LayoutSeed;
	LayoutBreakerCount = CleanPreset.BreakerCount;
	LayoutDensity = CleanPreset.LayoutDensity;
	bForceProceduralLayout = CleanPreset.bForceProcedural;

	TargetBotCount = FMath::Clamp(CleanPreset.BotCount, 0, FMath::Max(0, MaxPlayers - 1));
	BotDifficulty = CleanPreset.BotDifficulty;
	bBotMode = TargetBotCount > 0;
	if (!bBotMode)
	{
		while (RemoveOneBot())
		{
		}
	}

	UpdateDirectorGameState(BHGS->ObjectiveText);
	BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	// Lesson preset intentionally changes gate-target inputs: re-snapshot the frozen value.
	RefreshRevisionContributionGateTarget();
	BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
	UpdateRevisionSummary(TEXT("Lesson preset applied by classroom admin."));
	if (bBotMode)
	{
		RefreshBotRoster(RequestingController);
	}

	OutMessage = FString::Printf(TEXT("Applied lesson preset '%s' to the live lobby."), *CleanPreset.DisplayName);
	if (bAdjusted)
	{
		OutMessage += TEXT(" Stale values were clamped to current limits.");
	}
	BroadcastStatus(OutMessage, 4.0f);
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt lesson preset applied: %s"), *FBHLessonPresetStore::DescribePreset(CleanPreset));
	return true;
}

void ABHGameMode::ForceReview(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("force review")))
	{
		return;
	}

	RevisionReviewTimeRemaining = 60;
	UpdateRevisionSummary(TEXT("Review forced: discuss weak topics, exam methods, and corrected mistakes."));
	BroadcastStatus(TEXT("Physics review screen forced for 60 seconds."), 3.5f);
}

bool ABHGameMode::CanUseTesterShortcut(ABHPlayerController* RequestingController, const TCHAR* ActionDescription) const
{
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;

	// Tester shortcuts force session-wide travel/escape/train-phase changes for everyone, so they
	// must stay on the host machine. In a Test Round every joined client is temporarily granted the
	// Tester role and bTestMode is true server-wide, so the tester-context check below is not enough
	// on its own: without this host gate a student client could press a tester key and hijack the
	// shared session (force final-station travel, trigger the escape, advance the train phase, ...).
	if (!IsHostAdminController(RequestingController))
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Tester train shortcuts are host-only."), 4.0f);
		}
		return false;
	}

	// Dev/playtest bypass: in any non-Shipping build the HOST may use tester shortcuts outside a Test Round, so they
	// work in an ordinary dev playtest (this is why F9 / Ctrl+Alt+F9 now work in dev). Host-only is already enforced
	// above, so this never lets a joined client hijack. Compiled to false in the public Shipping build.
#if !UE_BUILD_SHIPPING
	const bool bDevBypass = true;
#else
	const bool bDevBypass = false;
#endif

	const bool bTesterContext = bDevBypass
		|| bTestMode
		|| (BHGS && BHGS->bTestMode)
		|| (BHPS && BHPS->PlayerRole == EBHPlayerRole::Tester);
	if (!bTesterContext)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Tester train shortcuts only work in Test Round or for the Tester role."), 4.0f);
		}
		return false;
	}

	return true;
}

void ABHGameMode::TesterGrantTrainResources(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("grant tester train resources")) || !GameState)
	{
		return;
	}

	int32 GrantedPlayers = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || BHPS->PlayerRole == EBHPlayerRole::Spectator || BHPS->PlayerRole == EBHPlayerRole::Unassigned)
		{
			continue;
		}

		BHPS->AddQuestionPoints(250);
		BHPS->AddHunterPoints(160);
		for (const FBHPowerupDefinition& Definition : FBHPowerupLibrary::GetDefaultPowerups())
		{
			const int32 TargetCharges = FMath::Max(1, Definition.MaxCharges);
			while (BHPS->GetPowerupCharges(Definition.Type) < TargetCharges)
			{
				if (!BHPS->AddPowerupCharge(Definition.Type, TargetCharges))
				{
					break;
				}
			}
		}
		++GrantedPlayers;
	}

	BroadcastStatus(FString::Printf(TEXT("Tester resources granted: 250 question points, 160 capture points, and max train powerups for %d player(s)."), GrantedPlayers), 4.0f);
}

void ABHGameMode::TesterOpenTrainIntermission(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("open the train intermission")) || !GetWorld())
	{
		return;
	}

	if (bTrainIntermissionLevel)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Already in the train intermission. Press F9 to advance phases."), 3.5f);
		}
		return;
	}

	BroadcastStatus(TEXT("Tester shortcut: opening subway intermission."), 2.5f);
	ConvertMonitorsBackToSurvivors(TEXT("tester subway intermission"));
	PersistPlayersForTravel();
	FString TravelURL = BuildTravelOptionsForLevel(TEXT("TrainIntermission"), true, RuntimeStageIndex, EBHRoundPhase::SurvivorsWin);
	TravelURL += TEXT("?BHTestMode=1");
	RequestServerTravel(TravelURL, true);
}

void ABHGameMode::TesterAdvanceTrainPhase(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("advance the train phase")))
	{
		return;
	}

	if (!bTrainIntermissionLevel || !TrainIntermissionManager)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("F9 advances train phases only while inside the train intermission."), 3.5f);
		}
		return;
	}

	TrainIntermissionManager->TesterAdvancePhase();
	BroadcastStatus(TEXT("Tester shortcut: train phase advanced."), 2.0f);
}

void ABHGameMode::TesterLoadFinalStation(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("load Foggrounds final station")) || !GetWorld())
	{
		return;
	}

	ConvertMonitorsBackToSurvivors(TEXT("tester final station load"));
	PersistPlayersForTravel();
	RuntimeStageIndex = 2;
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->SetPersistentStageIndex(RuntimeStageIndex);
	}

	FString TravelURL = FString::Printf(TEXT("%s?listen?BHLevel=Foggrounds?BHFogPreset=%s?BHStageIndex=2?BHTestMode=1"),
		*ResolveTravelMapForLevel(TEXT("Foggrounds")),
		*FogPresetToString(NextFogPreset));
	if (bFogPresetOverride)
	{
		TravelURL += TEXT("?BHFogOverride=1");
	}
	UBHGameSettings::AppendMaxPlayersOption(TravelURL);
	RequestServerTravel(TravelURL, true);
}

void ABHGameMode::TesterTriggerFinalEscape(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("trigger final escape")))
	{
		return;
	}

	if (!IsFinalStage())
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Final escape needs Foggrounds stage 3. Press End or Numpad 8 first."), 4.0f);
		}
		return;
	}

	TriggerFinalEscapeIfNeeded();
}

void ABHGameMode::TesterForceFinalRecap(ABHPlayerController* RequestingController)
{
	if (!CanUseTesterShortcut(RequestingController, TEXT("force the final train recap")) || !GetWorld())
	{
		return;
	}

	if (bTrainIntermissionLevel && TrainIntermissionManager)
	{
		TrainIntermissionManager->TesterFinishIntermission();
		return;
	}

	ConvertMonitorsBackToSurvivors(TEXT("tester final recap"));
	PersistPlayersForTravel();
	RuntimeStageIndex = 2;
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->SetPersistentStageIndex(RuntimeStageIndex);
	}

	FString TravelURL = BuildTravelOptionsForLevel(TEXT("TrainIntermission"), true, RuntimeStageIndex, EBHRoundPhase::SurvivorsWin);
	TravelURL += TEXT("?BHTestMode=1");
	RequestServerTravel(TravelURL, true);
}

void ABHGameMode::ResetRevisionStats()
{
	if (!GameState || !bRevisionMode)
	{
		return;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		if (ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS))
		{
			BHPS->ResetRevisionStats();
		}
	}
	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->ClearQuestionAttemptHistory();
	}
	RevisionReviewTimeRemaining = 0;
	bRevisionReportExported = false;
	UpdateRevisionSummary();
}

FBHClassRevisionSummary ABHGameMode::ComputeRevisionSummary() const
{
	FBHClassRevisionSummary Summary;
	Summary.LowestStudentMastery = 100.0f;
	if (!GameState)
	{
		Summary.LowestStudentMastery = 0.0f;
		return Summary;
	}

	float MasterySum = 0.0f;
	float TopicSums[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	const int32 MinimumContributionTarget = GetRevisionMinimumContributionTarget();
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!IsRevisionParticipantState(BHPS) || BHPS->IsABot())
		{
			continue;
		}
		++Summary.ActiveStudentCount;
		MasterySum += BHPS->RevisionStats.MasteryPercent;
		Summary.LowestStudentMastery = FMath::Min(Summary.LowestStudentMastery, BHPS->RevisionStats.MasteryPercent);
		if (BHPS->RevisionStats.MasteryPercent < RevisionIndividualThreshold)
		{
			++Summary.StudentsBelowIndividualThreshold;
		}
		if (BHPS->RevisionStats.ContributionCount < MinimumContributionTarget)
		{
			++Summary.StudentsWithoutContribution;
		}
		TopicSums[0] += BHPS->RevisionStats.ForcesMastery;
		TopicSums[1] += BHPS->RevisionStats.ElectricityMastery;
		TopicSums[2] += BHPS->RevisionStats.WavesMastery;
		TopicSums[3] += BHPS->RevisionStats.EnergyMastery;
	}

	if (Summary.ActiveStudentCount <= 0)
	{
		Summary.LowestStudentMastery = 0.0f;
		return Summary;
	}

	Summary.ClassMasteryAverage = MasterySum / Summary.ActiveStudentCount;
	int32 WeakIndex = 0;
	float WeakScore = TNumericLimits<float>::Max();
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		const EBHPhysicsTopic Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
		if ((RevisionTopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
		{
			continue;
		}
		const float TopicAverage = TopicSums[TopicIndex] / Summary.ActiveStudentCount;
		if (TopicAverage < WeakScore)
		{
			WeakScore = TopicAverage;
			WeakIndex = TopicIndex;
		}
	}
	Summary.WeakTopic = static_cast<EBHPhysicsTopic>(WeakIndex);
	return Summary;
}

void ABHGameMode::UpdateRevisionSummary(const FString& ReviewText)
{
	if (!bRevisionMode)
	{
		return;
	}

	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		const FString Text = ReviewText.IsEmpty()
			? FString::Printf(TEXT("Weak topic: %s. Class %.0f%%, lowest %.0f%%, below contribution target %d."),
				*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic),
				Summary.ClassMasteryAverage,
				Summary.LowestStudentMastery,
				Summary.StudentsWithoutContribution)
			: ReviewText;
		// Re-publish the FROZEN gate target (do NOT recompute it here). UpdateRevisionSummary runs on every
		// answer/join/leave; recomputing here is exactly what made the threshold drift mid-round. The frozen
		// value only changes at round start or on an admin change (RefreshRevisionContributionGateTarget).
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
		BHGS->SetRevisionSummary(Summary.ClassMasteryAverage, Summary.WeakTopic, RevisionReviewTimeRemaining, Text);
	}
}

bool ABHGameMode::CanUnlockRevisionExit(FString& OutReason) const
{
	if (!bRevisionMode)
	{
		return true;
	}

	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	if (Summary.ActiveStudentCount <= 0)
	{
		OutReason = TEXT("Classroom exit locked: no active students.");
		return false;
	}
	if (Summary.ClassMasteryAverage < RevisionClassThreshold)
	{
		OutReason = FString::Printf(TEXT("Exit locked: class mastery %.0f%% needs %.0f%%."), Summary.ClassMasteryAverage, RevisionClassThreshold);
		return false;
	}
	if (Summary.StudentsBelowIndividualThreshold > 0)
	{
		OutReason = FString::Printf(TEXT("Exit locked: %d student/monitor(s) below %.0f%% mastery."), Summary.StudentsBelowIndividualThreshold, RevisionIndividualThreshold);
		return false;
	}
	if (Summary.StudentsWithoutContribution > 0)
	{
		OutReason = FString::Printf(TEXT("Exit locked: %d student/monitor(s) still need %d answer-team or correction contribution(s)."), Summary.StudentsWithoutContribution, GetRevisionMinimumContributionTarget());
		return false;
	}
	return true;
}

FString ABHGameMode::GetRevisionObjectiveText() const
{
	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	return FString::Printf(TEXT("Physics Classroom: complete %d team nodes (%d questions each, %d-student teams). Escape needs class %.0f%%/%.0f%%, every student/monitor %.0f%%/%.0f%%, %d contribution(s) each. Weak topic: %s."),
		ActiveSideObjectiveCount,
		GetRevisionQuestionTargetPerNode(),
		GetRevisionAnswerTeamTargetSize(),
		Summary.ClassMasteryAverage,
		RevisionClassThreshold,
		Summary.LowestStudentMastery,
		RevisionIndividualThreshold,
		GetRevisionMinimumContributionTarget(),
		*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic));
}

EBHPhysicsTopic ABHGameMode::GetWeakestRevisionTopic() const
{
	return ComputeRevisionSummary().WeakTopic;
}

FString ABHGameMode::GetRevisionStatusReport() const
{
	const FBHClassRevisionSummary Summary = ComputeRevisionSummary();
	return FString::Printf(TEXT("RevisionStatus mode=%s class=%.0f threshold=%.0f lowest=%.0f individual=%.0f students=%d below=%d belowContrib=%d minContrib=%d questionsPerNode=%d teamSize=%d weak=%s mix=%s topicsMask=%d review=%ds scare=%d"),
		bRevisionMode ? TEXT("PhysicsClassroom") : TEXT("off"),
		Summary.ClassMasteryAverage,
		RevisionClassThreshold,
		Summary.LowestStudentMastery,
		RevisionIndividualThreshold,
		Summary.ActiveStudentCount,
		Summary.StudentsBelowIndividualThreshold,
		Summary.StudentsWithoutContribution,
		GetRevisionMinimumContributionTarget(),
		GetRevisionQuestionTargetPerNode(),
		GetRevisionAnswerTeamTargetSize(),
		*FBHRevisionQuestionBank::TopicToString(Summary.WeakTopic),
		*RevisionDifficultyMixToString(RevisionDifficultyMix),
		RevisionTopicMask,
		RevisionReviewTimeRemaining,
		RevisionScareIntensity);
}

bool ABHGameMode::ExportRevisionReport(ABHPlayerController* RequestingController, FString& OutMessage)
{
	if (!RequireHostAdmin(RequestingController, TEXT("export classroom performance data")))
	{
		OutMessage = TEXT("Only the host machine can export classroom performance data.");
		return false;
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const EBHRoundPhase ResultPhase = BHGS ? BHGS->RoundPhase : EBHRoundPhase::Lobby;
	const bool bExported = ExportRevisionReportToDisk(ResultPhase, false, OutMessage);
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(OutMessage, bExported ? 8.0f : 5.0f);
	}
	return bExported;
}

void ABHGameMode::RecordPlaytestTelemetryMarker(const FString& EventType, const FVector& Location, const FString& Detail, const ABHPlayerState* PlayerState, const ABHPlayerState* TargetState, const FString& QuestionId, const FString& Topic, const FString& Difficulty, int32 Count)
{
	if (!HasAuthority())
	{
		return;
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI)
	{
		return;
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	FBHPlaytestTelemetryEvent Event;
	Event.EventType = EventType;
	Event.EventDetail = Detail;
	Event.RuntimeLevelName = RuntimeLevelName.IsEmpty() ? TEXT("Unknown") : RuntimeLevelName;
	Event.RoundPhase = TelemetryRoundPhaseName(BHGS);
	Event.PlayerTag = BHGI->GetAnonymousTelemetryPlayerTag(PlayerState);
	Event.PlayerRole = TelemetryRoleName(PlayerState);
	Event.TargetTag = BHGI->GetAnonymousTelemetryPlayerTag(TargetState);
	Event.TargetRole = TelemetryRoleName(TargetState);
	Event.QuestionId = QuestionId;
	Event.Topic = Topic;
	Event.Difficulty = Difficulty;
	Event.Location = Location;
	Event.ServerTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	Event.StageIndex = RuntimeStageIndex;
	Event.RoundSeed = RoundSeed;
	Event.Count = FMath::Max(1, Count);
	Event.bRevisionMode = bRevisionMode;
	BHGI->RecordPlaytestTelemetryEvent(Event);

	const FString Key = TelemetryLocationKey(Location);
	if (EventType.Equals(TEXT("locker_entered"), ESearchCase::IgnoreCase))
	{
		TelemetryUsedLockerKeys.Add(Key);
	}
	else if (EventType.Equals(TEXT("objective_work_started"), ESearchCase::IgnoreCase))
	{
		TelemetryStartedObjectiveKeys.Add(Key);
	}
	else if (EventType.Equals(TEXT("objective_completed"), ESearchCase::IgnoreCase))
	{
		TelemetryCompletedObjectiveKeys.Add(Key);
	}
}

void ABHGameMode::RecordPlaytestTelemetryMapSnapshot()
{
	if (!HasAuthority() || !GetWorld())
	{
		return;
	}

	for (TActorIterator<ABHLocker> It(GetWorld()); It; ++It)
	{
		const ABHLocker* Locker = *It;
		if (!Locker)
		{
			continue;
		}

		const FVector Location = Locker->GetActorLocation();
		const FString Key = TelemetryLocationKey(Location);
		const FString SnapshotKey = TEXT("locker_unused|") + Key;
		if (!TelemetryUsedLockerKeys.Contains(Key) && !TelemetrySnapshotKeys.Contains(SnapshotKey))
		{
			TelemetrySnapshotKeys.Add(SnapshotKey);
			RecordPlaytestTelemetryMarker(TEXT("locker_unused"), Location, TEXT("no_entry_recorded"));
		}
	}

	for (ABHObjectiveStation* Station : ObjectiveStations)
	{
		if (!Station || !Station->IsDirectorActive())
		{
			continue;
		}

		const FVector Location = Station->GetActorLocation();
		const FString Key = TelemetryLocationKey(Location);
		if (Station->IsCompleted() || TelemetryCompletedObjectiveKeys.Contains(Key))
		{
			continue;
		}

		const UEnum* StationEnum = StaticEnum<EBHObjectiveStationType>();
		const FString StationType = StationEnum ? StationEnum->GetNameStringByValue(static_cast<int64>(Station->GetStationType())) : TEXT("Station");
		const bool bWasStarted = TelemetryStartedObjectiveKeys.Contains(Key);
		const FString SnapshotKey = FString::Printf(TEXT("%s|%s"), bWasStarted ? TEXT("objective_stalled") : TEXT("objective_unstarted"), *Key);
		if (TelemetrySnapshotKeys.Contains(SnapshotKey))
		{
			continue;
		}
		TelemetrySnapshotKeys.Add(SnapshotKey);
		RecordPlaytestTelemetryMarker(
			bWasStarted ? TEXT("objective_stalled") : TEXT("objective_unstarted"),
			Location,
			StationType);
	}
}

bool ABHGameMode::ExportPlaytestTelemetry(ABHPlayerController* RequestingController, FString& OutMessage)
{
	if (!RequireHostAdmin(RequestingController, TEXT("export playtest telemetry")))
	{
		OutMessage = TEXT("Only the host machine can export playtest telemetry.");
		return false;
	}

	UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available for playtest telemetry export.");
		return false;
	}

	RecordPlaytestTelemetryMapSnapshot();
	const bool bExported = BHGI->ExportPlaytestTelemetry(OutMessage, false);
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(OutMessage, bExported ? 8.0f : 4.0f);
	}
	return bExported;
}

bool ABHGameMode::ExportRevisionReportToDisk(EBHRoundPhase ResultPhase, bool bAutomatic, FString& OutMessage)
{
	if (!bRevisionMode)
	{
		OutMessage = TEXT("No revision classroom is active; no classroom report was exported.");
		return false;
	}
	if (bAutomatic && bRevisionReportExported)
	{
		OutMessage = TEXT("Revision classroom report was already exported for this round.");
		return true;
	}

	const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
	if (!BHGI)
	{
		OutMessage = TEXT("No game instance was available for classroom report export.");
		return false;
	}

	const TArray<FBHQuestionAttemptRecord>& Attempts = BHGI->GetQuestionAttemptHistory();
	if (Attempts.IsEmpty() && (!GameState || GameState->PlayerArray.IsEmpty()))
	{
		OutMessage = TEXT("No classroom performance data has been recorded yet.");
		return false;
	}

	struct FStudentReportRow
	{
		FString PlayerName;
		FString RoleName;
		FBHPlayerRevisionStats Stats;
		bool bHasPlayerState = false;
		int32 Attempts = 0;
		int32 Correct = 0;
		int32 Points = 0;
		int32 TopicAttempts[4] = {0, 0, 0, 0};
		int32 TopicCorrect[4] = {0, 0, 0, 0};
		TArray<FString> CorrectQuestionIds;
		TArray<FString> WrongQuestionIds;
	};

	struct FQuestionReportRow
	{
		FBHQuestionAttemptRecord FirstAttempt;
		int32 Attempts = 0;
		int32 Correct = 0;
		TSet<FString> MissedBy;
		// Distractor analysis: how many times each specific wrong answer was chosen,
		// so the report can name the actual misconception, not just "this was missed".
		TMap<FString, int32> WrongAnswerCounts;
	};

	struct FTopicReportRow
	{
		EBHPhysicsTopic Topic = EBHPhysicsTopic::ForcesAndMotion;
		int32 Attempts = 0;
		int32 Correct = 0;
		int32 Wrong = 0;
		TMap<FString, int32> MissesBySubtopic;
	};

	FTopicReportRow TopicRows[4];
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		TopicRows[TopicIndex].Topic = static_cast<EBHPhysicsTopic>(TopicIndex);
	}
	int32 DifficultyAttempts[3] = {0, 0, 0};
	float FirstAttemptSeconds = 0.0f;
	float LastAttemptSeconds = 0.0f;
	bool bHasAttemptTime = false;

	TMap<FString, FStudentReportRow> StudentsByName;
	if (GameState)
	{
		const UEnum* RoleEnum = StaticEnum<EBHPlayerRole>();
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
			if (!BHPS || !IsRevisionParticipantRole(BHPS->PlayerRole))
			{
				continue;
			}

			const FString PlayerName = BHPS->GetPlayerName().IsEmpty() ? TEXT("Player") : BHPS->GetPlayerName();
			FStudentReportRow& Row = StudentsByName.FindOrAdd(PlayerName);
			Row.PlayerName = PlayerName;
			Row.RoleName = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(BHPS->PlayerRole)) : TEXT("Student");
			if (BHPS->IsABot())
			{
				Row.RoleName += TEXT(" Bot");
			}
			Row.Stats = BHPS->RevisionStats;
			Row.bHasPlayerState = true;
		}
	}

	TMap<FString, FQuestionReportRow> QuestionsByKey;
	for (const FBHQuestionAttemptRecord& Attempt : Attempts)
	{
		const FString PlayerName = Attempt.PlayerName.IsEmpty() ? TEXT("Unknown") : Attempt.PlayerName;
		FStudentReportRow& StudentRow = StudentsByName.FindOrAdd(PlayerName);
		if (StudentRow.PlayerName.IsEmpty())
		{
			StudentRow.PlayerName = PlayerName;
			StudentRow.RoleName = TEXT("Student");
		}
		++StudentRow.Attempts;
		StudentRow.Points += FMath::Max(0, Attempt.PointsEarned);
		const int32 TopicIndex = ReportTopicIndex(Attempt.Topic);
		const int32 DifficultyIndex = FMath::Clamp(static_cast<int32>(Attempt.Difficulty), 0, 2);
		++DifficultyAttempts[DifficultyIndex];
		if (!bHasAttemptTime)
		{
			FirstAttemptSeconds = Attempt.TimestampSeconds;
			LastAttemptSeconds = Attempt.TimestampSeconds;
			bHasAttemptTime = true;
		}
		else
		{
			FirstAttemptSeconds = FMath::Min(FirstAttemptSeconds, Attempt.TimestampSeconds);
			LastAttemptSeconds = FMath::Max(LastAttemptSeconds, Attempt.TimestampSeconds);
		}

		++StudentRow.TopicAttempts[TopicIndex];
		++TopicRows[TopicIndex].Attempts;
		if (Attempt.bCorrect)
		{
			++StudentRow.Correct;
			++StudentRow.TopicCorrect[TopicIndex];
			++TopicRows[TopicIndex].Correct;
			StudentRow.CorrectQuestionIds.Add(Attempt.QuestionId.IsEmpty() ? Attempt.QuestionText.Left(48) : Attempt.QuestionId);
		}
		else
		{
			++TopicRows[TopicIndex].Wrong;
			const FString SubtopicKey = Attempt.QuestionSubtopic.IsEmpty()
				? (Attempt.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(Attempt.Topic) : Attempt.TopicName)
				: Attempt.QuestionSubtopic;
			++TopicRows[TopicIndex].MissesBySubtopic.FindOrAdd(SubtopicKey);
			StudentRow.WrongQuestionIds.Add(Attempt.QuestionId.IsEmpty() ? Attempt.QuestionText.Left(48) : Attempt.QuestionId);
		}

		const FString QuestionKey = Attempt.QuestionId.IsEmpty() ? Attempt.QuestionText.Left(96) : Attempt.QuestionId;
		FQuestionReportRow& QuestionRow = QuestionsByKey.FindOrAdd(QuestionKey);
		if (QuestionRow.Attempts == 0)
		{
			QuestionRow.FirstAttempt = Attempt;
		}
		++QuestionRow.Attempts;
		if (Attempt.bCorrect)
		{
			++QuestionRow.Correct;
		}
		else
		{
			QuestionRow.MissedBy.Add(PlayerName);
			// Record which distractor was chosen so the misconception is identifiable.
			// Skip the generic placeholder text used when no specific choice was captured.
			if (!Attempt.SelectedAnswer.IsEmpty()
				&& !Attempt.SelectedAnswer.Equals(TEXT("Incorrect response"), ESearchCase::IgnoreCase)
				&& !Attempt.SelectedAnswer.Equals(Attempt.CorrectAnswer, ESearchCase::IgnoreCase))
			{
				++QuestionRow.WrongAnswerCounts.FindOrAdd(Attempt.SelectedAnswer);
			}
		}
	}

	const auto MakeCsvRow = [](std::initializer_list<FString> Cells)
	{
		TArray<FString> Escaped;
		for (const FString& Cell : Cells)
		{
			Escaped.Add(CsvEscape(Cell));
		}
		return FString::Join(Escaped, TEXT(","));
	};

	const FString ReportDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ClassReports")));
	IFileManager::Get().MakeDirectory(*ReportDir, true);
	const FDateTime ExportedUtc = FDateTime::UtcNow();
	const FString ExportedUtcIso = ExportedUtc.ToIso8601();
	const FString Timestamp = ExportedUtc.ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString LevelToken = SanitizeReportToken(RuntimeLevelName);
	const FString Prefix = FString::Printf(TEXT("BlackoutHunt_%s_stage%d_%s"), *LevelToken, RuntimeStageIndex, *Timestamp);
	const FString SummaryPath = FPaths::Combine(ReportDir, Prefix + TEXT("_summary.csv"));
	const FString StudentPath = FPaths::Combine(ReportDir, Prefix + TEXT("_students.csv"));
	const FString AttemptsPath = FPaths::Combine(ReportDir, Prefix + TEXT("_attempts.csv"));
	const FString QuestionsPath = FPaths::Combine(ReportDir, Prefix + TEXT("_questions.csv"));
	const FString TeacherRecapPath = FPaths::Combine(ReportDir, Prefix + TEXT("_teacher_recap.md"));

	const FBHClassRevisionSummary ClassSummary = ComputeRevisionSummary();
	const FString ResultText = StaticEnum<EBHRoundPhase>() ? StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)) : TEXT("Unknown");
	TArray<FString> SummaryLines;
	SummaryLines.Add(MakeCsvRow({TEXT("Metric"), TEXT("Value")}));
	SummaryLines.Add(MakeCsvRow({TEXT("ExportedUtc"), ExportedUtcIso}));
	SummaryLines.Add(MakeCsvRow({TEXT("Result"), ResultText}));
	SummaryLines.Add(MakeCsvRow({TEXT("Level"), RuntimeLevelName}));
	SummaryLines.Add(MakeCsvRow({TEXT("StageIndex"), FString::FromInt(RuntimeStageIndex)}));
	SummaryLines.Add(MakeCsvRow({TEXT("ClassMasteryAverage"), FString::Printf(TEXT("%.1f"), ClassSummary.ClassMasteryAverage)}));
	SummaryLines.Add(MakeCsvRow({TEXT("LowestStudentMastery"), FString::Printf(TEXT("%.1f"), ClassSummary.LowestStudentMastery)}));
	SummaryLines.Add(MakeCsvRow({TEXT("ActiveStudentCount"), FString::FromInt(ClassSummary.ActiveStudentCount)}));
	SummaryLines.Add(MakeCsvRow({TEXT("StudentsBelowIndividualThreshold"), FString::FromInt(ClassSummary.StudentsBelowIndividualThreshold)}));
	SummaryLines.Add(MakeCsvRow({TEXT("StudentsBelowContributionTarget"), FString::FromInt(ClassSummary.StudentsWithoutContribution)}));
	SummaryLines.Add(MakeCsvRow({TEXT("ClassWeakTopic"), FBHRevisionQuestionBank::TopicToString(ClassSummary.WeakTopic)}));
	SummaryLines.Add(MakeCsvRow({TEXT("RevisionDifficultyMix"), RevisionDifficultyMixToString(RevisionDifficultyMix)}));
	SummaryLines.Add(MakeCsvRow({TEXT("QuestionAttemptsLogged"), FString::FromInt(Attempts.Num())}));

	TArray<FString> StudentLines;
	StudentLines.Add(MakeCsvRow({
		TEXT("Player"), TEXT("Role"), TEXT("Attempts"), TEXT("Correct"), TEXT("AccuracyPercent"), TEXT("Points"),
		TEXT("MasteryPercent"), TEXT("StrongSuit"), TEXT("WeakPoint"),
		TEXT("ForcesCorrect"), TEXT("ForcesAttempts"), TEXT("ElectricityCorrect"), TEXT("ElectricityAttempts"),
		TEXT("WavesCorrect"), TEXT("WavesAttempts"), TEXT("EnergyCorrect"), TEXT("EnergyAttempts"),
		TEXT("CorrectQuestions"), TEXT("WrongQuestions"), TEXT("RecommendedNextTopic"), TEXT("RecommendedDifficulty"), TEXT("AdaptiveReason")
	}));

	TArray<FString> StudentNames;
	StudentsByName.GetKeys(StudentNames);
	StudentNames.Sort();
	for (const FString& StudentName : StudentNames)
	{
		const FStudentReportRow* Row = StudentsByName.Find(StudentName);
		if (!Row)
		{
			continue;
		}

		FBHPlayerRevisionStats EffectiveStats = Row->Stats;
		if (!Row->bHasPlayerState && Row->Attempts > 0)
		{
			EffectiveStats.Attempts = Row->Attempts;
			EffectiveStats.CorrectAnswers = Row->Correct;
			EffectiveStats.MasteryPercent = 100.0f * static_cast<float>(Row->Correct) / static_cast<float>(FMath::Max(1, Row->Attempts));
		}

		const EBHPhysicsTopic StrongTopic = StrongestTopicForStats(EffectiveStats, RevisionTopicMask);
		const EBHPhysicsTopic WeakTopic = WeakestTopicForStats(EffectiveStats, RevisionTopicMask);
		// Report rows may be reconstructed from CSV without per-topic mastery, so gauge the coarse
		// follow-up difficulty against the overall figure that is always present here.
		const EBHQuestionDifficulty RecommendedDifficulty = AdaptiveDifficultyForStats(EffectiveStats, EffectiveStats.MasteryPercent, Row->Attempts <= 0 || Row->Correct >= Row->Attempts);
		const FString AdaptiveReason = FString::Printf(TEXT("Mastery %.0f%% over %d attempt(s); target %s at %s."),
			EffectiveStats.MasteryPercent,
			Row->Attempts,
			*FBHRevisionQuestionBank::TopicToString(WeakTopic),
			*FBHRevisionQuestionBank::DifficultyToString(RecommendedDifficulty));
		const float Accuracy = Row->Attempts > 0 ? 100.0f * static_cast<float>(Row->Correct) / static_cast<float>(Row->Attempts) : 0.0f;

		StudentLines.Add(MakeCsvRow({
			Row->PlayerName,
			Row->RoleName,
			FString::FromInt(Row->Attempts),
			FString::FromInt(Row->Correct),
			FString::Printf(TEXT("%.1f"), Accuracy),
			FString::FromInt(Row->Points),
			FString::Printf(TEXT("%.1f"), EffectiveStats.MasteryPercent),
			FBHRevisionQuestionBank::TopicToString(StrongTopic),
			FBHRevisionQuestionBank::TopicToString(WeakTopic),
			FString::FromInt(Row->TopicCorrect[0]),
			FString::FromInt(Row->TopicAttempts[0]),
			FString::FromInt(Row->TopicCorrect[1]),
			FString::FromInt(Row->TopicAttempts[1]),
			FString::FromInt(Row->TopicCorrect[2]),
			FString::FromInt(Row->TopicAttempts[2]),
			FString::FromInt(Row->TopicCorrect[3]),
			FString::FromInt(Row->TopicAttempts[3]),
			FString::Join(Row->CorrectQuestionIds, TEXT("; ")),
			FString::Join(Row->WrongQuestionIds, TEXT("; ")),
			FBHRevisionQuestionBank::TopicToString(WeakTopic),
			FBHRevisionQuestionBank::DifficultyToString(RecommendedDifficulty),
			AdaptiveReason
		}));
	}

	TArray<FString> AttemptLines;
	AttemptLines.Add(MakeCsvRow({
		TEXT("Player"), TEXT("Stage"), TEXT("TimeSeconds"), TEXT("QuestionId"), TEXT("Topic"), TEXT("Subtopic"),
		TEXT("Difficulty"), TEXT("Type"), TEXT("Question"), TEXT("SelectedAnswer"), TEXT("CorrectAnswer"), TEXT("Result"),
		TEXT("Explanation"), TEXT("Points"), TEXT("Correction"), TEXT("Team"), TEXT("AdaptiveNextTopic"),
		TEXT("AdaptiveNextDifficulty"), TEXT("AdaptiveReason")
	}));
	for (const FBHQuestionAttemptRecord& Attempt : Attempts)
	{
		AttemptLines.Add(MakeCsvRow({
			Attempt.PlayerName,
			FString::FromInt(Attempt.StageIndex),
			FString::Printf(TEXT("%.1f"), Attempt.TimestampSeconds),
			Attempt.QuestionId,
			Attempt.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(Attempt.Topic) : Attempt.TopicName,
			Attempt.QuestionSubtopic,
			FBHRevisionQuestionBank::DifficultyToString(Attempt.Difficulty),
			FBHRevisionQuestionBank::QuestionTypeToString(Attempt.QuestionType),
			Attempt.QuestionText,
			Attempt.SelectedAnswer,
			Attempt.CorrectAnswer,
			Attempt.bCorrect ? TEXT("Correct") : TEXT("Wrong"),
			Attempt.Explanation,
			FString::FromInt(Attempt.PointsEarned),
			Attempt.bCorrection ? TEXT("Yes") : TEXT("No"),
			Attempt.TeamSummary,
			FBHRevisionQuestionBank::TopicToString(Attempt.AdaptiveRecommendedTopic),
			FBHRevisionQuestionBank::DifficultyToString(Attempt.AdaptiveRecommendedDifficulty),
			Attempt.AdaptiveReason
		}));
	}

	TArray<FString> QuestionLines;
	QuestionLines.Add(MakeCsvRow({
		TEXT("QuestionId"), TEXT("Topic"), TEXT("Subtopic"), TEXT("Difficulty"), TEXT("Type"), TEXT("Question"),
		TEXT("CorrectAnswer"), TEXT("Attempts"), TEXT("Correct"), TEXT("Wrong"), TEXT("AccuracyPercent"),
		TEXT("MissedBy"), TEXT("TopWrongAnswer"), TEXT("TopWrongAnswerCount"), TEXT("Explanation")
	}));

	TArray<FString> QuestionKeys;
	QuestionsByKey.GetKeys(QuestionKeys);
	QuestionKeys.Sort();
	for (const FString& QuestionKey : QuestionKeys)
	{
		const FQuestionReportRow* Row = QuestionsByKey.Find(QuestionKey);
		if (!Row)
		{
			continue;
		}

		TArray<FString> MissedBy = Row->MissedBy.Array();
		MissedBy.Sort();
		const int32 Wrong = FMath::Max(0, Row->Attempts - Row->Correct);
		const float Accuracy = Row->Attempts > 0 ? 100.0f * static_cast<float>(Row->Correct) / static_cast<float>(Row->Attempts) : 0.0f;
		const FBHQuestionAttemptRecord& First = Row->FirstAttempt;

		// Most-chosen distractor for this question, for spreadsheet misconception analysis.
		FString TopWrongAnswer;
		int32 TopWrongCount = 0;
		for (const TPair<FString, int32>& WrongPair : Row->WrongAnswerCounts)
		{
			if (WrongPair.Value > TopWrongCount)
			{
				TopWrongCount = WrongPair.Value;
				TopWrongAnswer = WrongPair.Key;
			}
		}

		QuestionLines.Add(MakeCsvRow({
			First.QuestionId,
			First.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(First.Topic) : First.TopicName,
			First.QuestionSubtopic,
			FBHRevisionQuestionBank::DifficultyToString(First.Difficulty),
			FBHRevisionQuestionBank::QuestionTypeToString(First.QuestionType),
			First.QuestionText,
			First.CorrectAnswer,
			FString::FromInt(Row->Attempts),
			FString::FromInt(Row->Correct),
			FString::FromInt(Wrong),
			FString::Printf(TEXT("%.1f"), Accuracy),
			FString::Join(MissedBy, TEXT("; ")),
			TopWrongAnswer,
			FString::FromInt(TopWrongCount),
			First.Explanation
		}));
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	FBHLessonPreset SelectedPreset;
	FString PresetMessage;
	const bool bHasSelectedPreset = FBHLessonPresetStore::TryFindPreset(FBHLessonPresetStore::GetSelectedPresetId(), SelectedPreset, PresetMessage);
	const FString PresetSummary = bHasSelectedPreset
		? FBHLessonPresetStore::DescribePreset(SelectedPreset)
		: FString(TEXT("Manual/live classroom settings"));
	const int32 ActivityWindowSeconds = bHasAttemptTime
		? FMath::RoundToInt(FMath::Max(0.0f, LastAttemptSeconds - FirstAttemptSeconds))
		: 0;
	int32 TotalCorrect = 0;
	int32 TotalWrong = 0;
	for (int32 TopicIndex = 0; TopicIndex < 4; ++TopicIndex)
	{
		TotalCorrect += TopicRows[TopicIndex].Correct;
		TotalWrong += TopicRows[TopicIndex].Wrong;
	}
	int32 TotalPoints = 0;
	for (const TPair<FString, FStudentReportRow>& Pair : StudentsByName)
	{
		TotalPoints += FMath::Max(0, Pair.Value.Points);
	}

	struct FTopicMarkdownRow
	{
		EBHPhysicsTopic Topic = EBHPhysicsTopic::ForcesAndMotion;
		int32 Attempts = 0;
		int32 Correct = 0;
		int32 Wrong = 0;
		float Accuracy = 0.0f;
		FString TopMissedSubtopic;
		int32 TopMissedSubtopicCount = 0;
	};

	TArray<FTopicMarkdownRow> TopicMarkdownRows;
	for (const FTopicReportRow& TopicRow : TopicRows)
	{
		if (TopicRow.Attempts <= 0)
		{
			continue;
		}

		FTopicMarkdownRow MarkdownRow;
		MarkdownRow.Topic = TopicRow.Topic;
		MarkdownRow.Attempts = TopicRow.Attempts;
		MarkdownRow.Correct = TopicRow.Correct;
		MarkdownRow.Wrong = TopicRow.Wrong;
		MarkdownRow.Accuracy = 100.0f * static_cast<float>(TopicRow.Correct) / static_cast<float>(FMath::Max(1, TopicRow.Attempts));
		for (const TPair<FString, int32>& SubtopicPair : TopicRow.MissesBySubtopic)
		{
			if (SubtopicPair.Value > MarkdownRow.TopMissedSubtopicCount)
			{
				MarkdownRow.TopMissedSubtopic = SubtopicPair.Key;
				MarkdownRow.TopMissedSubtopicCount = SubtopicPair.Value;
			}
		}
		TopicMarkdownRows.Add(MarkdownRow);
	}
	TopicMarkdownRows.Sort([](const FTopicMarkdownRow& A, const FTopicMarkdownRow& B)
	{
		if (!FMath::IsNearlyEqual(A.Accuracy, B.Accuracy))
		{
			return A.Accuracy < B.Accuracy;
		}
		if (A.Wrong != B.Wrong)
		{
			return A.Wrong > B.Wrong;
		}
		return static_cast<int32>(A.Topic) < static_cast<int32>(B.Topic);
	});

	TArray<FQuestionReportRow> MissedQuestionRows;
	for (const TPair<FString, FQuestionReportRow>& Pair : QuestionsByKey)
	{
		const int32 Wrong = FMath::Max(0, Pair.Value.Attempts - Pair.Value.Correct);
		if (Wrong > 0)
		{
			MissedQuestionRows.Add(Pair.Value);
		}
	}
	MissedQuestionRows.Sort([](const FQuestionReportRow& A, const FQuestionReportRow& B)
	{
		const int32 WrongA = FMath::Max(0, A.Attempts - A.Correct);
		const int32 WrongB = FMath::Max(0, B.Attempts - B.Correct);
		if (WrongA != WrongB)
		{
			return WrongA > WrongB;
		}
		const float AccuracyA = A.Attempts > 0 ? 100.0f * static_cast<float>(A.Correct) / static_cast<float>(A.Attempts) : 0.0f;
		const float AccuracyB = B.Attempts > 0 ? 100.0f * static_cast<float>(B.Correct) / static_cast<float>(B.Attempts) : 0.0f;
		if (!FMath::IsNearlyEqual(AccuracyA, AccuracyB))
		{
			return AccuracyA < AccuracyB;
		}
		return A.FirstAttempt.QuestionId < B.FirstAttempt.QuestionId;
	});

	struct FStudentFollowupRow
	{
		FString PlayerName;
		float Mastery = 0.0f;
		float Accuracy = 0.0f;
		int32 Attempts = 0;
		int32 Correct = 0;
		int32 Contributions = 0;
		FString WeakTopic;
		FString RecommendedDifficulty;
		FString Reason;
	};

	auto EffectiveStatsForRow = [](const FStudentReportRow& Row)
	{
		FBHPlayerRevisionStats EffectiveStats = Row.Stats;
		if (!Row.bHasPlayerState && Row.Attempts > 0)
		{
			EffectiveStats.Attempts = Row.Attempts;
			EffectiveStats.CorrectAnswers = Row.Correct;
			EffectiveStats.MasteryPercent = 100.0f * static_cast<float>(Row.Correct) / static_cast<float>(FMath::Max(1, Row.Attempts));
		}
		return EffectiveStats;
	};

	TArray<FStudentFollowupRow> FollowupRows;
	for (const FString& StudentName : StudentNames)
	{
		const FStudentReportRow* Row = StudentsByName.Find(StudentName);
		if (!Row)
		{
			continue;
		}
		if (Row->RoleName.Contains(TEXT("Bot")))
		{
			continue;
		}

		const FBHPlayerRevisionStats EffectiveStats = EffectiveStatsForRow(*Row);
		const float Accuracy = Row->Attempts > 0 ? 100.0f * static_cast<float>(Row->Correct) / static_cast<float>(Row->Attempts) : EffectiveStats.MasteryPercent;
		const bool bBelowMastery = EffectiveStats.MasteryPercent < RevisionIndividualThreshold;
		const bool bLowAccuracy = Row->Attempts > 0 && Accuracy < 60.0f;
		const bool bBelowContribution = Row->bHasPlayerState && EffectiveStats.ContributionCount < GetRevisionMinimumContributionTarget();
		if (!bBelowMastery && !bLowAccuracy && !bBelowContribution && Row->WrongQuestionIds.IsEmpty())
		{
			continue;
		}

		TArray<FString> Reasons;
		if (bBelowMastery)
		{
			Reasons.Add(FString::Printf(TEXT("mastery %.0f%% below %.0f%%"), EffectiveStats.MasteryPercent, RevisionIndividualThreshold));
		}
		if (bLowAccuracy)
		{
			Reasons.Add(FString::Printf(TEXT("accuracy %.0f%%"), Accuracy));
		}
		if (bBelowContribution)
		{
			Reasons.Add(FString::Printf(TEXT("contributions %d/%d"), EffectiveStats.ContributionCount, GetRevisionMinimumContributionTarget()));
		}
		if (!Row->WrongQuestionIds.IsEmpty())
		{
			Reasons.Add(FString::Printf(TEXT("%d missed question(s)"), Row->WrongQuestionIds.Num()));
		}

		const EBHPhysicsTopic WeakTopic = WeakestTopicForStats(EffectiveStats, RevisionTopicMask);
		// Coarse follow-up hint from CSV-reconstructed stats: gauge against the always-present overall.
		const EBHQuestionDifficulty RecommendedDifficulty = AdaptiveDifficultyForStats(EffectiveStats, EffectiveStats.MasteryPercent, Row->Attempts <= 0 || Row->Correct >= Row->Attempts);
		FStudentFollowupRow Followup;
		Followup.PlayerName = Row->PlayerName;
		Followup.Mastery = EffectiveStats.MasteryPercent;
		Followup.Accuracy = Accuracy;
		Followup.Attempts = Row->Attempts;
		Followup.Correct = Row->Correct;
		Followup.Contributions = EffectiveStats.ContributionCount;
		Followup.WeakTopic = FBHRevisionQuestionBank::TopicToString(WeakTopic);
		Followup.RecommendedDifficulty = FBHRevisionQuestionBank::DifficultyToString(RecommendedDifficulty);
		Followup.Reason = FString::Join(Reasons, TEXT("; "));
		FollowupRows.Add(Followup);
	}
	FollowupRows.Sort([](const FStudentFollowupRow& A, const FStudentFollowupRow& B)
	{
		if (!FMath::IsNearlyEqual(A.Mastery, B.Mastery))
		{
			return A.Mastery < B.Mastery;
		}
		if (!FMath::IsNearlyEqual(A.Accuracy, B.Accuracy))
		{
			return A.Accuracy < B.Accuracy;
		}
		if (A.Attempts != B.Attempts)
		{
			return A.Attempts > B.Attempts;
		}
		return A.PlayerName < B.PlayerName;
	});

	const EBHPhysicsTopic RecommendedTopic = ClassSummary.WeakTopic;
	EBHQuestionDifficulty RecommendedClassDifficulty = EBHQuestionDifficulty::Medium;
	if (Attempts.IsEmpty() || ClassSummary.ClassMasteryAverage < 55.0f || ClassSummary.StudentsBelowIndividualThreshold > 0)
	{
		RecommendedClassDifficulty = EBHQuestionDifficulty::Easy;
	}
	else if (ClassSummary.ClassMasteryAverage >= RevisionClassThreshold + 8.0f && ClassSummary.StudentsBelowIndividualThreshold == 0)
	{
		RecommendedClassDifficulty = EBHQuestionDifficulty::Hard;
	}
	const FString RecommendedTopicText = FBHRevisionQuestionBank::TopicToString(RecommendedTopic);
	const FString RecommendedDifficultyText = FBHRevisionQuestionBank::DifficultyToString(RecommendedClassDifficulty);
	const FString SuggestedPresetText = RecommendedTopic == EBHPhysicsTopic::Electricity && RecommendedClassDifficulty == EBHQuestionDifficulty::Easy
		? FString(TEXT("Electricity easy"))
		: FString::Printf(TEXT("Custom: %s focus, %s complexity"), *RecommendedTopicText, *RecommendedDifficultyText);
	const FString TargetRecommendation = ClassSummary.ClassMasteryAverage < RevisionClassThreshold || ClassSummary.StudentsBelowIndividualThreshold > 0
		? FString(TEXT("Use Support 60/40 or repeat current targets until corrections are secure."))
		: FString(TEXT("Keep Default 70/50, or use Stretch 80/60 if the next class needs challenge."));
	const FString StrongTopicText = TopicMarkdownRows.IsEmpty()
		? FString(TEXT("Not enough question data"))
		: FBHRevisionQuestionBank::TopicToString(TopicMarkdownRows.Last().Topic);
	const int32 StudentsMeetingIndividualThreshold = FMath::Max(0, ClassSummary.ActiveStudentCount - ClassSummary.StudentsBelowIndividualThreshold);
	const int32 ObjectiveCompleted = BHGS ? BHGS->SideObjectivesCompleted : 0;
	const int32 ObjectiveRequired = BHGS ? BHGS->SideObjectivesRequired : ActiveSideObjectiveCount;
	const int32 BreakersCompleted = BHGS ? BHGS->BreakersCompleted : 0;
	const int32 BreakersRequired = BHGS ? BHGS->BreakersRequired : RequiredBreakers;
	const FString ExitStateText = BHGS && BHGS->bExitUnlocked ? TEXT("Unlocked") : TEXT("Locked");
	const FString FinalEscapeText = (BHGS && BHGS->FinalEscapeState != EBHFinalEscapeState::Inactive && StaticEnum<EBHFinalEscapeState>())
		? StaticEnum<EBHFinalEscapeState>()->GetNameStringByValue(static_cast<int64>(BHGS->FinalEscapeState))
		: FString(TEXT("Not reached"));
	const FString NextDestinationText = BHGS && !BHGS->NextLevelName.IsEmpty()
		? BHGS->NextLevelName
		: (NextRuntimeLevelName.IsEmpty() ? FString(TEXT("Next classroom route")) : NextRuntimeLevelName);

	TArray<FString> MarkdownLines;
	MarkdownLines.Add(TEXT("# Blackout Hunt Classroom Report"));
	MarkdownLines.Add(TEXT(""));
	MarkdownLines.Add(TEXT("> Teacher-local report. This file may include lobby display names, question text, answer explanations, and follow-up suggestions. Do not project or share it with students without reviewing it first."));
	MarkdownLines.Add(TEXT(""));
	MarkdownLines.Add(TEXT("## Session Summary"));
	MarkdownLines.Add(FString::Printf(TEXT("- Exported UTC: %s"), *ExportedUtcIso));
	MarkdownLines.Add(FString::Printf(TEXT("- Map and round: %s, stage %d, result %s"), *MarkdownInline(RuntimeLevelName), RuntimeStageIndex, *MarkdownInline(ResultText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Selected preset: %s"), *MarkdownInline(PresetSummary)));
	MarkdownLines.Add(FString::Printf(TEXT("- Live settings: focus %s, difficulty %s, class %.0f%%, individual %.0f%%, duration %s, scare %d"),
		*MarkdownInline(FBHLessonPresetStore::TopicMaskToText(RevisionTopicMask)),
		*MarkdownInline(RevisionDifficultyMixToString(RevisionDifficultyMix)),
		RevisionClassThreshold,
		RevisionIndividualThreshold,
		*ReportDurationText(RevisionRoundDuration),
		RevisionScareIntensity));
	MarkdownLines.Add(FString::Printf(TEXT("- Question activity: %d attempt(s), %d correct, %d missed, %d point(s), active window %s%s"),
		Attempts.Num(),
		TotalCorrect,
		TotalWrong,
		TotalPoints,
		*ReportDurationText(ActivityWindowSeconds),
		bHasAttemptTime ? TEXT("") : TEXT(" (no attempts yet)")));
	MarkdownLines.Add(FString::Printf(TEXT("- Difficulty mix attempted: Easy %d, Medium %d, Hard %d"),
		DifficultyAttempts[0],
		DifficultyAttempts[1],
		DifficultyAttempts[2]));
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Class Mastery"));
	MarkdownLines.Add(FString::Printf(TEXT("- Class mastery: %.1f%% against %.0f%% target."), ClassSummary.ClassMasteryAverage, RevisionClassThreshold));
	MarkdownLines.Add(FString::Printf(TEXT("- Individual target: %d/%d active student(s) at or above %.0f%%."), StudentsMeetingIndividualThreshold, ClassSummary.ActiveStudentCount, RevisionIndividualThreshold));
	MarkdownLines.Add(FString::Printf(TEXT("- Contribution target: %d student/monitor(s) still below %d contribution(s)."), ClassSummary.StudentsWithoutContribution, GetRevisionMinimumContributionTarget()));
	MarkdownLines.Add(FString::Printf(TEXT("- Weakest class topic: %s."), *MarkdownInline(RecommendedTopicText)));
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Weak Topics"));
	if (TopicMarkdownRows.IsEmpty())
	{
		MarkdownLines.Add(TEXT("No question attempts were recorded, so weak topics cannot be inferred yet."));
	}
	else
	{
		MarkdownLines.Add(TEXT("| Topic | Accuracy | Attempts | Missed | Most missed subtopic |"));
		MarkdownLines.Add(TEXT("| --- | ---: | ---: | ---: | --- |"));
		for (const FTopicMarkdownRow& Row : TopicMarkdownRows)
		{
			const FString MissedSubtopic = Row.TopMissedSubtopic.IsEmpty()
				? FString(TEXT("No missed subtopic recorded"))
				: FString::Printf(TEXT("%s (%d)"), *Row.TopMissedSubtopic, Row.TopMissedSubtopicCount);
			MarkdownLines.Add(FString::Printf(TEXT("| %s | %.0f%% | %d | %d | %s |"),
				*MarkdownInline(FBHRevisionQuestionBank::TopicToString(Row.Topic)),
				Row.Accuracy,
				Row.Attempts,
				Row.Wrong,
				*MarkdownInline(MissedSubtopic)));
		}
	}
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Common Misconceptions"));
	if (MissedQuestionRows.IsEmpty())
	{
		MarkdownLines.Add(TEXT("No missed questions were recorded. Use the next round to confirm this was genuine mastery rather than low sample size."));
	}
	else
	{
		const int32 MaxMisconceptions = FMath::Min(5, MissedQuestionRows.Num());
		for (int32 Index = 0; Index < MaxMisconceptions; ++Index)
		{
			const FQuestionReportRow& Row = MissedQuestionRows[Index];
			const int32 Wrong = FMath::Max(0, Row.Attempts - Row.Correct);
			const float Accuracy = Row.Attempts > 0 ? 100.0f * static_cast<float>(Row.Correct) / static_cast<float>(Row.Attempts) : 0.0f;
			const FBHQuestionAttemptRecord& First = Row.FirstAttempt;
			MarkdownLines.Add(FString::Printf(TEXT("%d. %s - %s, %s, missed %d/%d (%.0f%% correct)."),
				Index + 1,
				*MarkdownInline(First.QuestionSubtopic.IsEmpty() ? First.QuestionId : First.QuestionSubtopic),
				*MarkdownInline(First.TopicName.IsEmpty() ? FBHRevisionQuestionBank::TopicToString(First.Topic) : First.TopicName),
				*MarkdownInline(FBHRevisionQuestionBank::DifficultyToString(First.Difficulty)),
				Wrong,
				Row.Attempts,
				Accuracy));
			MarkdownLines.Add(FString::Printf(TEXT("   - Prompt: %s"), *MarkdownInline(First.QuestionText.Left(180))));
			if (!First.CorrectAnswer.IsEmpty())
			{
				MarkdownLines.Add(FString::Printf(TEXT("   - Correct answer: %s"), *MarkdownInline(First.CorrectAnswer.Left(160))));
			}
			// Distractor breakdown: name the most-chosen wrong answer(s) so the teacher
			// can target the actual misconception, e.g. "8 chose 'mass times velocity'".
			if (Row.WrongAnswerCounts.Num() > 0)
			{
				TArray<TPair<FString, int32>> SortedWrong;
				for (const TPair<FString, int32>& WrongPair : Row.WrongAnswerCounts)
				{
					SortedWrong.Add(WrongPair);
				}
				SortedWrong.Sort([](const TPair<FString, int32>& A, const TPair<FString, int32>& B)
				{
					return A.Value > B.Value;
				});
				FString WrongSummary;
				const int32 MaxDistractors = FMath::Min(3, SortedWrong.Num());
				for (int32 WrongIndex = 0; WrongIndex < MaxDistractors; ++WrongIndex)
				{
					if (WrongIndex > 0)
					{
						WrongSummary += TEXT("; ");
					}
					WrongSummary += FString::Printf(TEXT("%dx \"%s\""), SortedWrong[WrongIndex].Value, *MarkdownInline(SortedWrong[WrongIndex].Key.Left(80)));
				}
				MarkdownLines.Add(FString::Printf(TEXT("   - Common wrong choices: %s"), *WrongSummary));
			}
			if (!First.Explanation.IsEmpty())
			{
				MarkdownLines.Add(FString::Printf(TEXT("   - Teacher explanation: %s"), *MarkdownInline(First.Explanation.Left(220))));
			}
		}
	}
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Students Needing Follow-up"));
	if (FollowupRows.IsEmpty())
	{
		MarkdownLines.Add(TEXT("No individual follow-up was flagged from the recorded attempts and live mastery state."));
	}
	else
	{
		MarkdownLines.Add(TEXT("| Lobby name | Mastery | Attempts | Contributions | Recommended review | Reason |"));
		MarkdownLines.Add(TEXT("| --- | ---: | ---: | ---: | --- | --- |"));
		const int32 MaxFollowups = FMath::Min(8, FollowupRows.Num());
		for (int32 Index = 0; Index < MaxFollowups; ++Index)
		{
			const FStudentFollowupRow& Row = FollowupRows[Index];
			MarkdownLines.Add(FString::Printf(TEXT("| %s | %.0f%% | %d/%d | %d | %s at %s | %s |"),
				*MarkdownInline(Row.PlayerName),
				Row.Mastery,
				Row.Correct,
				Row.Attempts,
				Row.Contributions,
				*MarkdownInline(Row.WeakTopic),
				*MarkdownInline(Row.RecommendedDifficulty),
				*MarkdownInline(Row.Reason)));
		}
		if (FollowupRows.Num() > MaxFollowups)
		{
			MarkdownLines.Add(FString::Printf(TEXT("_Plus %d additional student(s) in the CSV export._"), FollowupRows.Num() - MaxFollowups));
		}
	}
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Follow-up Plan"));
	MarkdownLines.Add(FString::Printf(TEXT("- Suggested next preset: %s."), *MarkdownInline(SuggestedPresetText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Next focus: review %s at %s level, then run one short correction check before the next hunt."), *MarkdownInline(RecommendedTopicText), *MarkdownInline(RecommendedDifficultyText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Threshold guidance: %s"), *MarkdownInline(TargetRecommendation)));
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Gameplay Notes"));
	MarkdownLines.Add(FString::Printf(TEXT("- Objective nodes completed: %d/%d."), ObjectiveCompleted, ObjectiveRequired));
	MarkdownLines.Add(FString::Printf(TEXT("- Breakers repaired: %d/%d. Exit state: %s."), BreakersCompleted, BreakersRequired, *ExitStateText));
	MarkdownLines.Add(FString::Printf(TEXT("- Final escape state: %s."), *MarkdownInline(FinalEscapeText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Next route: %s."), *MarkdownInline(NextDestinationText)));
	if (BHGI)
	{
		MarkdownLines.Add(TEXT(""));
		MarkdownLines.Add(TEXT("### Train Recap Snapshot"));
		MarkdownLines.Add(TEXT("```text"));
		MarkdownLines.Add(BHGI->BuildTrainRecapOverview());
		MarkdownLines.Add(TEXT(""));
		MarkdownLines.Add(BHGI->BuildTrainRecapTopics());
		MarkdownLines.Add(TEXT(""));
		MarkdownLines.Add(BHGI->BuildTrainRecapTips(NextDestinationText));
		MarkdownLines.Add(TEXT("```"));
	}
	MarkdownLines.Add(TEXT(""));

	MarkdownLines.Add(TEXT("## Printable Class Recap"));
	MarkdownLines.Add(TEXT("This section avoids names and answer keys. Copy only this section if you want a student-facing recap after teacher review."));
	MarkdownLines.Add(FString::Printf(TEXT("- Class result: %.0f%% mastery from %d question attempt(s)."), ClassSummary.ClassMasteryAverage, Attempts.Num()));
	MarkdownLines.Add(FString::Printf(TEXT("- Strongest area: %s."), *MarkdownInline(StrongTopicText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Review next: %s."), *MarkdownInline(RecommendedTopicText)));
	MarkdownLines.Add(FString::Printf(TEXT("- Next target: practise %s questions, then retry one team node."), *MarkdownInline(RecommendedDifficultyText)));
	MarkdownLines.Add(TEXT(""));

	const bool bSavedSummary = FFileHelper::SaveStringToFile(FString::Join(SummaryLines, LINE_TERMINATOR), *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedStudents = FFileHelper::SaveStringToFile(FString::Join(StudentLines, LINE_TERMINATOR), *StudentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedAttempts = FFileHelper::SaveStringToFile(FString::Join(AttemptLines, LINE_TERMINATOR), *AttemptsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedQuestions = FFileHelper::SaveStringToFile(FString::Join(QuestionLines, LINE_TERMINATOR), *QuestionsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedTeacherRecap = FFileHelper::SaveStringToFile(FString::Join(MarkdownLines, LINE_TERMINATOR), *TeacherRecapPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSavedSummary || !bSavedStudents || !bSavedAttempts || !bSavedQuestions || !bSavedTeacherRecap)
	{
		OutMessage = FString::Printf(TEXT("Classroom report export failed. Check write access to %s."), *ReportDir);
		return false;
	}

	bRevisionReportExported = true;
	OutMessage = FString::Printf(TEXT("Classroom report exported to %s (CSV plus teacher recap Markdown)."), *ReportDir);
	UE_LOG(LogTemp, Display, TEXT("%s (%s, %s, %s, %s, %s)"), *OutMessage, *SummaryPath, *StudentPath, *AttemptsPath, *QuestionsPath, *TeacherRecapPath);
	return true;
}

void ABHGameMode::SetPracticeRole(ABHPlayerController* RequestingController, EBHPlayerRole NewRole)
{
	if (!RequireHostAdmin(RequestingController, TEXT("switch Practice Lab roles")))
	{
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice roles are only available in Practice Lab."), 3.0f);
		}
		return;
	}

	if (NewRole != EBHPlayerRole::Survivor && NewRole != EBHPlayerRole::Hunter && NewRole != EBHPlayerRole::FakeHunter)
	{
		NewRole = EBHPlayerRole::Survivor;
	}

	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!BHPS)
	{
		return;
	}

	BHPS->SetRole(NewRole);
	BHPS->SetDesiredRole(NewRole);
	BHPS->SetLifeState(EBHPlayerLifeState::Alive);
	BHPS->SetReady(true);
	BHPS->SetHiddenInLocker(false);
	BHPS->SetFakeHunterEligible(false);
	RestartPlayer(RequestingController);

	FString RoleName = TEXT("Survivor");
	if (NewRole == EBHPlayerRole::Hunter)
	{
		RoleName = TEXT("Teacher");
	}
	else if (NewRole == EBHPlayerRole::FakeHunter)
	{
		RoleName = TEXT("Hall Monitor");
	}

	RequestingController->ClientShowStatusMessage(FString::Printf(TEXT("Practice role: %s. No ready-up or match timer."), *RoleName), 3.5f);
}

void ABHGameMode::SetPracticeModifier(ABHPlayerController* RequestingController, EBHRoundModifier NewModifier)
{
	if (!RequireHostAdmin(RequestingController, TEXT("change Practice Lab modifiers")))
	{
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice modifiers are only available in Practice Lab."), 3.0f);
		}
		return;
	}

	PracticeRoundModifier = NewModifier;
	RefreshPracticeDirector(FString::Printf(TEXT("Practice modifier: %s."), *GetRoundModifierText(PracticeRoundModifier)));
}

void ABHGameMode::RefreshPracticeRound(ABHPlayerController* RequestingController)
{
	if (bTestMode)
	{
		if (!RequireHostAdmin(RequestingController, TEXT("refresh Test Round")))
		{
			return;
		}

		RefreshTestDirector(TEXT("Test round refreshed."));
		return;
	}

	if (!bPracticeMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Practice refresh is only available in Practice Lab."), 3.0f);
		}
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("refresh Practice Lab")))
	{
		return;
	}

	RefreshPracticeDirector(TEXT("Practice round refreshed."));
}

void ABHGameMode::TriggerPracticeJumpscare(ABHPlayerController* RequestingController)
{
	if (!RequireHostAdmin(RequestingController, TEXT("trigger Practice Lab scares")))
	{
		return;
	}

	if (!bPracticeMode && !bTestMode)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Manual jumpscare testing is only available in Practice Lab or Test Round."), 3.0f);
		}
		return;
	}

	ABHCharacter* Target = RequestingController ? Cast<ABHCharacter>(RequestingController->GetPawn()) : nullptr;
	ABHPlayerState* BHPS = RequestingController ? RequestingController->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!Target || !BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("Could not find an alive practice pawn for the jumpscare."), 3.0f);
		}
		return;
	}
	if (!GetWorld())
	{
		return;
	}

	if (TriggerManualScare(Target, EBHScareEventType::MonsterCharge))
	{
		return;
	}

	TriggerMonsterChargeJumpscare(Target);
}

void ABHGameMode::TriggerTargetedJumpscare(ABHPlayerController* RequestingController, APlayerState* TargetPlayerState)
{
	if (!RequestingController || !GetWorld())
	{
		return;
	}

	if (!RequireHostAdmin(RequestingController, TEXT("trigger targeted scares")))
	{
		return;
	}

	ABHCharacter* Target = FindCharacterForPlayerState(TargetPlayerState);
	if (!Target)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Could not find that player for a scare."), 2.75f);
		return;
	}

	// A deliberate teacher scare is intentional, so it bypasses the "teacher nearby" suppression and rolls one
	// of the showcased scares at random rather than always firing a monster charge.
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	TriggerTeacherChosenScare(Target, TargetPC);
	if (TargetPC)
	{
		TargetPC->ClientShowStatusMessage(TEXT("The Teacher chose you."), 2.75f);
	}
}

void ABHGameMode::TriggerHunterBlackout(const FVector& SourceLocation, int32 SurgeCharges)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}
	const int32 ClampedSurgeCharges = FMath::Clamp(SurgeCharges, 0, 2);
	RecordPlaytestTelemetryMarker(TEXT("teacher_blackout"), SourceLocation, FString::Printf(TEXT("surge=%d"), ClampedSurgeCharges));

	TArray<ABHFlickerLight*> PoweredLights;
	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0 && Light->IsPowered())
		{
			PoweredLights.Add(Light);
		}
	}

	float KilledLightSpread = 0.0f;
	if (!PoweredLights.IsEmpty())
	{
		PoweredLights.Sort([&SourceLocation](const ABHFlickerLight& A, const ABHFlickerLight& B)
		{
			return FVector::DistSquared2D(A.GetActorLocation(), SourceLocation) < FVector::DistSquared2D(B.GetActorLocation(), SourceLocation);
		});

		const int32 Limit = FMath::Min(5 + ClampedSurgeCharges * 2, PoweredLights.Num());
		for (int32 Index = 0; Index < Limit; ++Index)
		{
			if (PoweredLights[Index])
			{
				PoweredLights[Index]->SetPowered(false);
				KilledLightSpread = FMath::Max(KilledLightSpread, FVector::Dist2D(PoweredLights[Index]->GetActorLocation(), SourceLocation));
			}
		}
	}

	SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 80.0f), 90.0f + 10.0f * static_cast<float>(ClampedSurgeCharges), 0.32f, 0.22f, 5.0f, 5.5f);
	ApplyPresenceSpike(SourceLocation, 72.0f + 8.0f * static_cast<float>(ClampedSurgeCharges), TEXT("The dark moved with the Teacher."));

	// A nearby student's flashlight is overwhelmed by the dark: open a located blackout over the killed-light
	// area during which their beam flickers down to a near-dead level and their battery drains faster (see
	// ABHCharacter::IsInTeacherBlackout / UpdateFlashlightFeel). Each surge charge lengthens the window.
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float WeakenSeconds = Settings ? FMath::Max(0.0f, Settings->BlackoutFlashlightWeakenSeconds) : 0.0f;
	if (WeakenSeconds > 0.0f)
	{
		const float FallbackRadius = Settings ? FMath::Max(0.0f, Settings->BlackoutFlashlightEffectRadius) : 2600.0f;
		const float EffectRadius = KilledLightSpread > 0.0f ? KilledLightSpread + 700.0f : FallbackRadius;
		const float WindowSeconds = WeakenSeconds + 2.0f * static_cast<float>(ClampedSurgeCharges);
		BHGS->SetTeacherBlackout(SourceLocation, EffectRadius, BHGS->GetServerWorldTimeSeconds() + WindowSeconds);
	}

	BroadcastStatus(TEXT("The Teacher killed a bank of lights."), 3.5f);
}

#if WITH_EDITOR
void ABHGameMode::BuildLevelForExport(const FString& InLevelName)
{
	if (!GetWorld())
	{
		return;
	}

	// Bake all blockout geometry + mesh props as real, hard-referenced AStaticMeshActors for this build so
	// the saved .umap is visible/editable in-editor and the meshes become hard map cook dependencies. Reset
	// at the end so nothing downstream of export keeps emitting placed actors.
	bAuthoringExport = true;

	// Editor export only: produce the same geometry + gameplay actors a live build would, but with none
	// of the round/timer/atmosphere state (those come from BeginPlay at runtime, not the builders). Reset
	// the tracked arrays the way BuildRuntimeFacility would before a dispatch, then run one builder.
	bFacilityBuilt = false;
	bTrainIntermissionLevel = false;
	BreakerActors.Reset();
	DoorActors.Reset();
	ExitGates.Reset();
	FlickerLights.Reset();
	StationSignalBlocks.Reset();
	StationSignalLights.Reset();
	ObjectiveStations.Reset();
	EscapeStationManagers.Reset();
	SurvivorSpawns.Reset();
	ScarePoints.Reset();

	const FString Normalized = NormalizeBHLevelName(InLevelName);
	RuntimeLevelName = Normalized;
	NextRuntimeLevelName = Normalized;
	RuntimeFogPreset = EBHFogPreset::Heavy;
	NextFogPreset = EBHFogPreset::Heavy;

	if (Normalized.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		BuildSubstationLevel();
	}
	else if (Normalized.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		BuildFoggroundsLevel();
	}
	else if (Normalized.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		BuildTutorialLevel();
	}
	else
	{
		RuntimeLevelName = TEXT("Facility");
		BuildFacilityLevel();
	}

	// Discovery reads survivor/teacher spawns from placed APlayerStart actors; the runtime generator uses
	// bare coordinates, so the export must materialize them (one tagged "Hunter" for the teacher).
	SpawnAuthoredPlayerStarts();

	bAuthoringExport = false;
}

AStaticMeshActor* ABHGameMode::SpawnAuthoredBlockActor(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material, bool bStartHidden)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// FogSheet blocks are runtime-only translucent fog cues; the authored map ships real ExponentialHeightFog,
	// so do not bake them into geometry.
	if (Material == EBHBlockMaterial::FogSheet)
	{
		return nullptr;
	}

	// Material selection. Saturated decorative cues (route stripes, signs, pads, beacons, glyphs) keep their
	// route COLOUR via the serializable MI_BH_Route_* palette (the runtime per-instance tint is a dynamic
	// material instance that cannot serialize into the .umap). Near-grey structural surfaces (zone wall/floor
	// tints) keep their PBR M_BH material. Mirrors ABHStaticBlockField::ResolveBaseMaterial for the latter.
	auto MaterialPath = [](EBHBlockMaterial M) -> const TCHAR*
	{
		switch (M)
		{
		case EBHBlockMaterial::Concrete:     return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Concrete.M_BH_Concrete");
		case EBHBlockMaterial::Plaster:      return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Plaster.M_BH_Plaster");
		case EBHBlockMaterial::ConcreteWA:   return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA.M_BH_ConcreteWA");
		case EBHBlockMaterial::ConcreteWACool: return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_ConcreteWA_Cool.M_BH_ConcreteWA_Cool");
		case EBHBlockMaterial::PlasterWA:    return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PlasterWA.M_BH_PlasterWA");
		case EBHBlockMaterial::BluePanel:    return TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1");
		case EBHBlockMaterial::RustedMetal:  return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_RustedMetal.M_BH_RustedMetal");
		case EBHBlockMaterial::DiamondPlate: return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_DiamondPlate.M_BH_DiamondPlate");
		case EBHBlockMaterial::PaintedMetal: return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal");
		case EBHBlockMaterial::Tiles:        return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Tiles.M_BH_Tiles");
		case EBHBlockMaterial::WarningSign:  return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_WarningSign.M_BH_WarningSign");
		case EBHBlockMaterial::Wood:         return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Wood.M_BH_Wood");
		case EBHBlockMaterial::Carpet:       return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Carpet.M_BH_Carpet");
		case EBHBlockMaterial::Leather:      return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Leather.M_BH_Leather");
		case EBHBlockMaterial::Marble:       return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_Marble.M_BH_Marble");
		default:                             return TEXT("/Game/BlackoutHunt/Art/Materials/M_BH_PaintedMetal.M_BH_PaintedMetal");
		}
	};

	// Nearest route-colour MIC for a saturated cue tint; nullptr for near-grey structural tints.
	auto RouteMaterialPath = [](const FLinearColor& T) -> const TCHAR*
	{
		const float Chroma = FMath::Max3(T.R, T.G, T.B) - FMath::Min3(T.R, T.G, T.B);
		if (Chroma < 0.20f)
		{
			return nullptr;
		}
		struct FRoute { const TCHAR* Path; FLinearColor Col; };
		static const FRoute Routes[] = {
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Green.MI_BH_Route_Green"),   FLinearColor(0.10f, 0.85f, 0.42f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Amber.MI_BH_Route_Amber"),   FLinearColor(0.95f, 0.55f, 0.14f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Cyan.MI_BH_Route_Cyan"),     FLinearColor(0.14f, 0.68f, 0.92f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Violet.MI_BH_Route_Violet"), FLinearColor(0.66f, 0.34f, 0.92f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Yellow.MI_BH_Route_Yellow"), FLinearColor(0.95f, 0.80f, 0.16f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Red.MI_BH_Route_Red"),       FLinearColor(0.90f, 0.13f, 0.10f) },
			{ TEXT("/Game/BlackoutHunt/Art/Materials/MI_BH_Route_Blue.MI_BH_Route_Blue"),     FLinearColor(0.16f, 0.55f, 0.92f) },
		};
		const TCHAR* Best = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		for (const FRoute& Route : Routes)
		{
			const float Distance = FMath::Square(T.R - Route.Col.R) + FMath::Square(T.G - Route.Col.G) + FMath::Square(T.B - Route.Col.B);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Route.Path;
			}
		}
		return Best;
	};

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
	if (!MeshActor)
	{
		return nullptr;
	}

	if (UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent())
	{
		Component->SetMobility(EComponentMobility::Static);
		if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Component->SetStaticMesh(Cube);
		}
		const TCHAR* RoutePath = RouteMaterialPath(Tint);
		const TCHAR* ChosenPath = RoutePath ? RoutePath : MaterialPath(Material);
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, ChosenPath))
		{
			Component->SetMaterial(0, Mat);
		}
		Component->SetCollisionEnabled(bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Component->SetCollisionProfileName(bCollides ? TEXT("BlockAll") : TEXT("NoCollision"));
		Component->SetCanEverAffectNavigation(bCollides);
	}

	MeshActor->SetActorScale3D(Scale);
	if (bStartHidden)
	{
		// Containment blockers: collision only, invisible in game, still selectable/visible in-editor.
		MeshActor->SetActorHiddenInGame(true);
	}

	// Pre-organize the bake by role so the ~1800-actor Outliner is navigable for hand-polish: collidable
	// blocks are the shell/structure, non-collidable thin blocks are floor decals/route signage, and
	// hidden blocks are the perimeter containment. Labels auto-number (ShellBlock, ShellBlock1, ...).
	const TCHAR* BakeFolder = bStartHidden ? TEXT("Authored/Containment") : (bCollides ? TEXT("Authored/Shell") : TEXT("Authored/Decals_Signage"));
	const TCHAR* BakeLabel = bStartHidden ? TEXT("Containment") : (bCollides ? TEXT("ShellBlock") : TEXT("DecalBlock"));
	MeshActor->SetFolderPath(FName(BakeFolder));
	MeshActor->SetActorLabel(BakeLabel);

	// v2 industrial look: clad wall-shaped collidable blocks (tall, thin in one horizontal axis, long in
	// the other) with modular metal panels on the industrial levels. Additive/fail-safe -- the cube above
	// keeps collision/nav. Floors/ceilings (thin Z), pillars (not long), and containment are skipped.
	const bool bWallShaped = bCollides && !bStartHidden && Scale.Z >= 2.5f
		&& FMath::Min(Scale.X, Scale.Y) <= 0.6f && FMath::Max(Scale.X, Scale.Y) >= 1.5f;
	const bool bIndustrialLevel = RuntimeLevelName.Equals(TEXT("Facility"), ESearchCase::IgnoreCase)
		|| RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase);
	// Skip the industrial metal-panel cladding for the world-aligned concrete walls (ConcreteWA / ConcreteWACool)
	// so the backrooms hall and the Substation stay uniform bare concrete instead of mixing in metal panels.
	if (bWallShaped && bIndustrialLevel && Material != EBHBlockMaterial::ConcreteWA && Material != EBHBlockMaterial::ConcreteWACool)
	{
		CladAuthoredWall(Location, Scale, Rotation);
	}
	return MeshActor;
}

AStaticMeshActor* ABHGameMode::SpawnAuthoredMeshActor(const FVector& Location, const FRotator& Rotation, const FString& MeshAssetPath, const FString& MaterialAssetPath, const FVector& MeshScale, bool bCollides)
{
	UWorld* World = GetWorld();
	if (!World || MeshAssetPath.IsEmpty())
	{
		return nullptr;
	}

	UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
	if (!MeshAsset)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
	if (!MeshActor)
	{
		return nullptr;
	}

	if (UStaticMeshComponent* Component = MeshActor->GetStaticMeshComponent())
	{
		Component->SetMobility(EComponentMobility::Static);
		Component->SetStaticMesh(MeshAsset);
		if (!MaterialAssetPath.IsEmpty())
		{
			if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialAssetPath))
			{
				Component->SetMaterial(0, Mat);
			}
		}
		Component->SetCollisionEnabled(bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(bCollides);
	}

	MeshActor->SetActorScale3D(MeshScale);
	MeshActor->SetFolderPath(FName(TEXT("Authored/Props")));
	MeshActor->SetActorLabel(MeshAsset->GetName());
	return MeshActor;
}

void ABHGameMode::SpawnAuthoredPlayerStarts()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (const FVector& Spawn : SurvivorSpawns)
	{
		World->SpawnActor<APlayerStart>(Spawn, FRotator::ZeroRotator, SpawnParams);
	}

	if (APlayerStart* HunterStart = World->SpawnActor<APlayerStart>(HunterSpawn, FRotator::ZeroRotator, SpawnParams))
	{
		HunterStart->PlayerStartTag = FName(TEXT("Hunter"));
	}

	UE_LOG(LogTemp, Display, TEXT("[BHExportLevel] Placed %d survivor PlayerStarts + 1 Hunter PlayerStart for authored discovery."), SurvivorSpawns.Num());
}

void ABHGameMode::CladAuthoredWall(const FVector& Center, const FVector& Scale, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	// Facility/Substation walls are axis-aligned (ZeroRotator); skip anything rotated so the placement math
	// below stays correct (the cube wall still reads fine uncladded if we bail).
	if (!World || !FMath::IsNearlyZero(Rotation.Yaw))
	{
		return;
	}

	UStaticMesh* Panel = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ContainersHouseCH/StaticMesh/SM_Wall_Metal_6m.SM_Wall_Metal_6m"));
	if (!Panel)
	{
		return; // kit not present -> the v1 cube shell stands, nothing broken
	}
	UMaterialInterface* PanelMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ContainersHouseCH/Materials/MI_CH_Walls_Metal.MI_CH_Walls_Metal"));

	// SM_Wall_Metal_6m native bounds (Tools/MeshCatalog.json): thickness ~8.5cm along local +X, length
	// ~600cm along local -Y, height ~230cm along local +Z, pivot at the +Y bottom end.
	constexpr float PanelLen = 600.0f;
	constexpr float PanelHeight = 230.0f;

	const float LenX = Scale.X * 100.0f;
	const float LenY = Scale.Y * 100.0f;
	const float Height = Scale.Z * 100.0f;
	const bool bAlongX = LenX >= LenY;
	const float WallLen = bAlongX ? LenX : LenY;
	const float WallThick = bAlongX ? LenY : LenX;
	const float BottomZ = Center.Z - Height * 0.5f;

	const int32 Tiles = FMath::Max(1, FMath::RoundToInt(WallLen / PanelLen));
	const float TileLen = WallLen / Tiles;
	// Local axes: X=thickness, Y=length, Z=height. Keep native ~8.5cm thickness, scale length to the tile,
	// height to the wall.
	const FVector TileScale(1.0f, TileLen / PanelLen, Height / PanelHeight);

	auto SpawnPanel = [&](const FVector& PivotLoc, float Yaw)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Clad = World->SpawnActor<AStaticMeshActor>(PivotLoc, FRotator(0.0f, Yaw, 0.0f), Params);
		if (!Clad)
		{
			return;
		}
		if (UStaticMeshComponent* Component = Clad->GetStaticMeshComponent())
		{
			Component->SetMobility(EComponentMobility::Static);
			Component->SetStaticMesh(Panel);
			if (PanelMat)
			{
				Component->SetMaterial(0, PanelMat);
			}
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision); // visual only; cube wall owns collision/nav
			Component->SetCanEverAffectNavigation(false);
		}
		Clad->SetActorScale3D(TileScale);
		Clad->SetFolderPath(FName(TEXT("Authored/MetalCladding")));
		Clad->SetActorLabel(TEXT("WallClad"));
	};

	// Tile both faces. Pivot sits on the wall face; the panel's 8.5cm depth (both faces modeled) sits just
	// proud of the cube. Yaw maps local -Y (length) onto the wall's long axis and local +X (thickness)
	// outward from each face. Verified against the catalog bounds.
	for (int32 i = 0; i < Tiles; ++i)
	{
		if (bAlongX)
		{
			const float StartX = Center.X - WallLen * 0.5f + i * TileLen; // -X end of this tile (yaw 90 extends +X)
			const float EndX = Center.X + WallLen * 0.5f - i * TileLen;    // +X end (yaw -90 extends -X)
			SpawnPanel(FVector(StartX, Center.Y + WallThick * 0.5f, BottomZ), 90.0f);  // +Y face
			SpawnPanel(FVector(EndX, Center.Y - WallThick * 0.5f, BottomZ), -90.0f);   // -Y face
		}
		else
		{
			const float StartY = Center.Y + WallLen * 0.5f - i * TileLen; // +Y end (yaw 0 extends -Y)
			const float EndY = Center.Y - WallLen * 0.5f + i * TileLen;    // -Y end (yaw 180 extends +Y)
			SpawnPanel(FVector(Center.X + WallThick * 0.5f, StartY, BottomZ), 0.0f);    // +X face
			SpawnPanel(FVector(Center.X - WallThick * 0.5f, EndY, BottomZ), 180.0f);    // -X face
		}
	}
}
#endif

bool ABHGameMode::DiscoverAuthoredLevel()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// A single placed ABHLevelMarker flags the level as hand-authored. No marker => runtime generator.
	ABHLevelMarker* Marker = nullptr;
	for (TActorIterator<ABHLevelMarker> It(World); It; ++It)
	{
		Marker = *It;
		break;
	}
	if (!Marker)
	{
		return false;
	}

	bFacilityBuilt = true;

	// Start from a clean slate, matching how each Build*Level() resets its tracked arrays.
	BreakerActors.Reset();
	DoorActors.Reset();
	ExitGates.Reset();
	FlickerLights.Reset();
	StationSignalBlocks.Reset();
	StationSignalLights.Reset();
	ObjectiveStations.Reset();
	EscapeStationManagers.Reset();
	SurvivorSpawns.Reset();

	// Honour the marker's identity/fog/stage only where the travel URL did not already pin a value, so
	// host-driven travel options keep priority over the asset-baked defaults.
	if ((RuntimeLevelName.IsEmpty() || RuntimeLevelName.Equals(TEXT("Facility"), ESearchCase::IgnoreCase))
		&& !Marker->LevelName.IsEmpty())
	{
		RuntimeLevelName = NormalizeBHLevelName(Marker->LevelName);
	}
	if (!bFogPresetOverride && FString(World->URL.GetOption(TEXT("BHFogPreset="), TEXT(""))).IsEmpty())
	{
		RuntimeFogPreset = Marker->DefaultFogPreset;
		NextFogPreset = Marker->DefaultFogPreset;
	}

	// Collect only the actor types the game mode tracks by pointer. Everything else (lockers, batteries,
	// panic alarms, cameras + CCTV zones, security terminals/monitors, power switches, shutters) is
	// already located via TActorIterator at its use sites, so authored placement of those needs no
	// registration here.
	for (TActorIterator<ABHBreaker> It(World); It; ++It)
	{
		BreakerActors.Add(*It);
	}
	for (TActorIterator<ABHDoor> It(World); It; ++It)
	{
		DoorActors.Add(*It);
	}
	for (TActorIterator<ABHExitGate> It(World); It; ++It)
	{
		ExitGates.Add(*It);
	}
	for (TActorIterator<ABHObjectiveStation> It(World); It; ++It)
	{
		// Exclude the hidden teacher mirror-trap node. The generator spawns it but deliberately keeps it
		// OUT of ObjectiveStations (it is a scare relay, never a completable objective). Discovery must
		// match, or an authored map counts an extra "objective" that can never complete -> SideObjectives
		// can never be satisfied and the exit never unlocks (unwinnable round), and the always-armed trap
		// gets toggled off by the director's active-objective selection.
		if (*It && !(*It)->IsTeacherMirrorTrapNode())
		{
			ObjectiveStations.Add(*It);
		}
	}
	for (TActorIterator<ABHEscapeStationManager> It(World); It; ++It)
	{
		EscapeStationManagers.Add(*It);
	}
	for (TActorIterator<ABHFlickerLight> It(World); It; ++It)
	{
		FlickerLights.Add(*It);
	}

	// Survivor spawns come from placed APlayerStart actors; a PlayerStart with the "Hunter" tag becomes
	// the teacher/hunter spawn. Falls back to the marker transform when no hunter start was authored.
	bool bFoundHunterSpawn = false;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (!Start)
		{
			continue;
		}
		if (Start->PlayerStartTag == FName(TEXT("Hunter")))
		{
			HunterSpawn = Start->GetActorLocation();
			bFoundHunterSpawn = true;
		}
		else
		{
			SurvivorSpawns.Add(Start->GetActorLocation());
		}
	}
	if (!bFoundHunterSpawn)
	{
		HunterSpawn = Marker->GetActorLocation();
	}

	// Authored maps are hand-placed, so fail loudly (and recover where safe) when a required element is
	// missing — these are the mistakes a designer makes. This is all authored-path only; the procedural
	// builders always spawn the right counts, so none of it triggers for the generated levels.
	if (BreakerActors.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlackoutHunt] Authored level '%s' has no ABHBreaker actors; survivors cannot unlock the exit. Place breakers in the level."), *RuntimeLevelName);
	}
	else if (BreakerActors.Num() < RequiredBreakers)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlackoutHunt] Authored level '%s' has %d breakers but RequiredBreakers=%d; clamping to the placed count so the round stays winnable."), *RuntimeLevelName, BreakerActors.Num(), RequiredBreakers);
		RequiredBreakers = FMath::Max(1, BreakerActors.Num());
	}
	if (ExitGates.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[BlackoutHunt] Authored level '%s' has no ABHExitGate actors; survivors have no way to escape. Place at least one exit gate."), *RuntimeLevelName);
	}
	if (SurvivorSpawns.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlackoutHunt] Authored level '%s' has no survivor APlayerStart actors; survivors will use the fallback spawn. Place APlayerStart actors (tag one 'Hunter' for the teacher)."), *RuntimeLevelName);
	}
	if (bRevisionMode && ObjectiveStations.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BlackoutHunt] Authored level '%s' is in revision mode but has no ABHObjectiveStation actors; revision rounds need stations. Place stations or disable revision for this map."), *RuntimeLevelName);
	}

	// Rebuild crawl-space gates from the marker if the loaded map has none of its own. Crawl runs are low
	// GEOMETRY that only blocks standing pawns; the ABHCrawlSpaceVolume is what ejects a prone Teacher (and
	// non-survivors), so a baked map that lost its volumes while being remeshed would let the Teacher follow
	// survivors through. Only spawn when zero volumes exist, so an intact bake is never duplicated.
	bool bHasCrawlVolume = false;
	for (TActorIterator<ABHCrawlSpaceVolume> It(World); It; ++It)
	{
		bHasCrawlVolume = true;
		break;
	}
	if (!bHasCrawlVolume && Marker->CrawlGates.Num() > 0)
	{
		int32 RebuiltGates = 0;
		for (const FBHAuthoredCrawlGate& Gate : Marker->CrawlGates)
		{
			if (ABHCrawlSpaceVolume* CrawlVolume = World->SpawnActor<ABHCrawlSpaceVolume>(Gate.Location, Gate.Rotation))
			{
				CrawlVolume->Configure(Gate.Extent);
				++RebuiltGates;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[BlackoutHunt] Authored level '%s' had no ABHCrawlSpaceVolume actors; rebuilt %d crawl gate(s) from the level marker so non-survivors stay blocked from crawl spaces."), *RuntimeLevelName, RebuiltGates);
	}

	// Authored maps should ship a baked NavMeshBoundsVolume; until then the marker keeps the runtime
	// navigation rebuild enabled so bots have a usable nav mesh.
	if (Marker->bRebuildRuntimeNavigation)
	{
		BuildRuntimeNavigation();
	}

	UE_LOG(LogTemp, Log, TEXT("[BlackoutHunt] Authored level '%s' discovered: %d breakers, %d doors, %d exits, %d objective stations, %d escape managers, %d flicker lights, %d survivor spawns (hunter spawn %s)."),
		*RuntimeLevelName,
		BreakerActors.Num(),
		DoorActors.Num(),
		ExitGates.Num(),
		ObjectiveStations.Num(),
		EscapeStationManagers.Num(),
		FlickerLights.Num(),
		SurvivorSpawns.Num(),
		bFoundHunterSpawn ? TEXT("from tagged PlayerStart") : TEXT("from marker"));

	return true;
}

void ABHGameMode::BuildRuntimeFacility()
{
	if (bFacilityBuilt || !GetWorld())
	{
		return;
	}

	bFacilityBuilt = true;
	BreakerActors.Reset();
	DoorActors.Reset();
	ExitGates.Reset();
	FlickerLights.Reset();
	StationSignalBlocks.Reset();
	StationSignalLights.Reset();
	ObjectiveStations.Reset();
	EscapeStationManagers.Reset();
	ScarePoints.Reset();
	TrainIntermissionManager = nullptr;
	ResetStaticBlockField();
	const FString IntermissionOption = GetWorld()->URL.GetOption(TEXT("BHTrainIntermission="), TEXT(""));
	bTrainIntermissionLevel = IsTrueOption(IntermissionOption);
	const FString LobbyOption = GetWorld()->URL.GetOption(TEXT("BHLobby="), TEXT(""));
	bLobbyLevel = IsTrueOption(LobbyOption);
	// The hunt level the train lobby departs to when the round starts; defaults to Facility if unspecified.
	LobbyFirstLevelName = NormalizeBHLevelName(GetWorld()->URL.GetOption(TEXT("BHFirstLevel="), TEXT("Facility")));
	// Host map route (variable-length stage sequence). Parsed BEFORE the stage index so the stage clamp can
	// use the route length; empty = the default Facility -> Substation -> Foggrounds sequence.
	RuntimeMapRoute = FBHLessonPresetStore::ParseMapRoute(GetWorld()->URL.GetOption(TEXT("BHMapRoute="), TEXT("")));
	const FString StageIndexOption = GetWorld()->URL.GetOption(TEXT("BHStageIndex="), TEXT(""));
	RuntimeStageIndex = StageIndexOption.IsEmpty() ? GetConfiguredStageIndex() : FMath::Clamp(FCString::Atoi(*StageIndexOption), 0, GetMaxStageIndex());
	PendingIntermissionResult = RoundPhaseFromUrlValue(GetWorld()->URL.GetOption(TEXT("BHIntermissionResult="), TEXT("")));
	const FString TestOption = GetWorld()->URL.GetOption(TEXT("BHTestMode="), TEXT(""));
	bTestMode = IsTrueOption(TestOption);
	const FString PracticeOption = GetWorld()->URL.GetOption(TEXT("BHPractice="), TEXT(""));
	bPracticeMode = !bTestMode && (PracticeOption.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("Practice"), ESearchCase::IgnoreCase));
	// The solo tutorial rides on the practice sandbox (no ready-up, no match timer, no forced round end);
	// the map-baked ABHTutorialDirector layers the guided lesson + scripted Teacher on top of it.
	const FString TutorialOption = GetWorld()->URL.GetOption(TEXT("BHTutorial="), TEXT(""));
	if (!bTestMode && (TutorialOption.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| TutorialOption.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| TutorialOption.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase)))
	{
		bPracticeMode = true;
		bTutorialMode = true;
		// Which guided tutorial runs: Survivor (default) -> Teacher -> Monitor, chained on reaching the exit.
		const FString TutorialPhaseOption = GetWorld()->URL.GetOption(TEXT("BHTutorialPhase="), TEXT("Survivor"));
		if (TutorialPhaseOption.Equals(TEXT("Teacher"), ESearchCase::IgnoreCase))
		{
			RuntimeTutorialPhase = EBHTutorialPhase::Teacher;
		}
		else if (TutorialPhaseOption.Equals(TEXT("Monitor"), ESearchCase::IgnoreCase))
		{
			RuntimeTutorialPhase = EBHTutorialPhase::Monitor;
		}
		else if (TutorialPhaseOption.Equals(TEXT("Movement"), ESearchCase::IgnoreCase))
		{
			RuntimeTutorialPhase = EBHTutorialPhase::Movement;
		}
		else if (TutorialPhaseOption.Equals(TEXT("EasterEgg"), ESearchCase::IgnoreCase))
		{
			RuntimeTutorialPhase = EBHTutorialPhase::EasterEgg;
		}
		else
		{
			RuntimeTutorialPhase = EBHTutorialPhase::Survivor;
		}
		// Chain to the next tutorial on exit (full course) unless the player picked one tutorial to practise.
		// Defaults to chained; only an explicit ?BHTutorialChain=0 (or false) makes it a single standalone lesson.
		const FString TutorialChainOption = GetWorld()->URL.GetOption(TEXT("BHTutorialChain="), TEXT("1"));
		bTutorialChain = !(TutorialChainOption.Equals(TEXT("0"), ESearchCase::IgnoreCase)
			|| TutorialChainOption.Equals(TEXT("false"), ESearchCase::IgnoreCase));
	}
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const FString HuntSecondsOption = GetWorld()->URL.GetOption(TEXT("BHHuntSeconds="), TEXT(""));
	if (!HuntSecondsOption.IsEmpty())
	{
		HuntSeconds = FMath::Clamp(FCString::Atoi(*HuntSecondsOption), 30, 3600);
	}
	const FString RevisionModeOption = GetWorld()->URL.GetOption(TEXT("BHRevisionMode="), TEXT(""));
	bRevisionMode = !bPracticeMode && !bTestMode && IsTrueOption(RevisionModeOption);
	RevisionMode = bRevisionMode ? EBHRevisionMode::PhysicsClassroom : EBHRevisionMode::None;
	if (bTrainIntermissionLevel)
	{
		bRevisionMode = true;
		RevisionMode = EBHRevisionMode::PhysicsClassroom;
	}
	// --- Prop Hunt (opt-in, reversible). ?BHPropHunt=1 or the bh.PropHunt cvar flips the live Hunt into a
	// hide-as-a-prop game. Mutually exclusive with the practice/test/train sandboxes and revision mode; when off the
	// game is byte-for-byte unchanged. The cvar is global and survives ServerTravel, so a host that flips it in the
	// console carries prop hunt through every level of the session. See BHGameModePropHunt.cpp. ---
	const FString PropHuntOption = GetWorld()->URL.GetOption(TEXT("BHPropHunt="), TEXT(""));
	bPropHuntMode = !bPracticeMode && !bTestMode && !bTrainIntermissionLevel
		&& (IsTrueOption(PropHuntOption) || BHIsPropHuntCVarEnabled());
	if (bPropHuntMode)
	{
		bRevisionMode = false;
		RevisionMode = EBHRevisionMode::None;
	}
	if (bRevisionMode)
	{
		const FString RevisionTopicsOption = GetWorld()->URL.GetOption(TEXT("BHRevisionTopics="), TEXT("All"));
		const FString RevisionMixOption = GetWorld()->URL.GetOption(TEXT("BHRevisionDifficultyMix="), TEXT("Adaptive"));
		const FString ClassThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionClassThreshold="), *FString::SanitizeFloat(Settings->RevisionClassThreshold));
		const FString IndividualThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionIndividualThreshold="), *FString::SanitizeFloat(Settings->RevisionIndividualThreshold));
		const FString ScareIntensityOption = GetWorld()->URL.GetOption(TEXT("BHScareIntensity="), *FString::FromInt(Settings->RevisionScareIntensity));
		RevisionTopicMask = ParsePhysicsTopicMask(RevisionTopicsOption, 0x0F);
		RevisionDifficultyMix = ParseRevisionDifficultyMix(RevisionMixOption, EBHRevisionDifficultyMix::Adaptive);
		// Host difficulty range: clamp the live selection to [Min,Max] and start each student at StartingDifficulty.
		RevisionStartingDifficulty = FBHLessonPresetStore::ParseQuestionDifficulty(GetWorld()->URL.GetOption(TEXT("BHStartDifficulty="), TEXT("Easy")), EBHQuestionDifficulty::Easy);
		RevisionMinDifficulty = FBHLessonPresetStore::ParseQuestionDifficulty(GetWorld()->URL.GetOption(TEXT("BHMinDifficulty="), TEXT("Easy")), EBHQuestionDifficulty::Easy);
		RevisionMaxDifficulty = FBHLessonPresetStore::ParseQuestionDifficulty(GetWorld()->URL.GetOption(TEXT("BHMaxDifficulty="), TEXT("Hard")), EBHQuestionDifficulty::Hard);
		if (static_cast<uint8>(RevisionMinDifficulty) > static_cast<uint8>(RevisionMaxDifficulty))
		{
			Swap(RevisionMinDifficulty, RevisionMaxDifficulty);
		}
		RevisionStartingDifficulty = ClampRevisionDifficulty(RevisionStartingDifficulty);
		// Exact question set: store the id now; the id list is resolved from the saved set file below
		// (see ResolveAllowedQuestionIds) once the active bank is available.
		RevisionQuestionSetId = GetWorld()->URL.GetOption(TEXT("BHQuestionSet="), TEXT(""));
		ResolveAllowedQuestionIds();
		RevisionClassThreshold = FMath::Clamp(FCString::Atof(*ClassThresholdOption), 0.0f, 100.0f);
		RevisionIndividualThreshold = FMath::Clamp(FCString::Atof(*IndividualThresholdOption), 0.0f, 100.0f);
		RevisionRoundDuration = FMath::Clamp(Settings->RevisionRoundSeconds, 60, 3600);
		HuntSeconds = FMath::Clamp(HuntSecondsOption.IsEmpty() ? Settings->RevisionRoundSeconds : HuntSeconds, 60, 3600);
		RevisionRoundDuration = HuntSeconds;
		RevisionScareIntensity = FMath::Clamp(FCString::Atoi(*ScareIntensityOption), 0, 3);
		RevisionReviewTimeRemaining = 0;
		ObjectiveIntensity = 4;
	}
	if (bRevisionMode && !bTrainIntermissionLevel && HuntSecondsOption.IsEmpty() && Settings && Settings->bUseTrainIntermissions)
	{
		const int32 StageSeconds = RuntimeStageIndex <= 0
			? Settings->StageOneSeconds
			: (RuntimeStageIndex == 1 ? Settings->StageTwoSeconds : Settings->StageThreeSeconds);
		HuntSeconds = FMath::Clamp(StageSeconds, 60, 3600);
		RevisionRoundDuration = HuntSeconds;
	}
	const FString BotModeOption = GetWorld()->URL.GetOption(TEXT("BHBotMode="), TEXT(""));
	const FString BotCountOption = GetWorld()->URL.GetOption(TEXT("BHBotCount="), TEXT(""));
	const FString BotDifficultyOption = GetWorld()->URL.GetOption(TEXT("BHBotDifficulty="), *BotDifficultyToString(Settings->DefaultBotDifficulty));
	const int32 RequestedBotCount = BotCountOption.IsEmpty()
		? FMath::Clamp(Settings->DefaultBotCount, 0, FMath::Max(0, MaxPlayers - 1))
		: FMath::Clamp(FCString::Atoi(*BotCountOption), 0, FMath::Max(0, MaxPlayers - 1));
	bBotMode = !bPracticeMode
		&& !bTestMode
		&& (IsTrueOption(BotModeOption) || (bRevisionMode && !BotCountOption.IsEmpty() && RequestedBotCount > 0));
	if (bBotMode)
	{
		TargetBotCount = RequestedBotCount;
		BotDifficulty = ParseBotDifficulty(BotDifficultyOption, Settings->DefaultBotDifficulty);
	}
	else
	{
		TargetBotCount = 0;
		BotDifficulty = Settings->DefaultBotDifficulty;
	}
	RuntimeLevelName = GetWorld()->URL.GetOption(TEXT("BHLevel="), TEXT(""));
	if (RuntimeLevelName.IsEmpty())
	{
		RuntimeLevelName = bTrainIntermissionLevel ? TEXT("TrainIntermission") : GetDefaultMapForStage(RuntimeStageIndex);
	}
	RuntimeLevelName = bTrainIntermissionLevel ? TEXT("TrainIntermission") : NormalizeBHLevelName(RuntimeLevelName);
	NextRuntimeLevelName = RuntimeLevelName;
	const FString FogPresetOption = GetWorld()->URL.GetOption(TEXT("BHFogPreset="), TEXT("Heavy"));
	RuntimeFogPreset = ParseFogPreset(FogPresetOption, EBHFogPreset::Heavy);
	NextFogPreset = RuntimeFogPreset;
	const FString FogOverrideOption = GetWorld()->URL.GetOption(TEXT("BHFogOverride="), TEXT(""));
	bFogPresetOverride = IsTrueOption(FogOverrideOption);

	// Host procedural layout knobs (0 = generator default; only applied when the round runs the generator).
	LayoutSeed = FMath::Max(0, FCString::Atoi(GetWorld()->URL.GetOption(TEXT("BHLayoutSeed="), TEXT("0"))));
	LayoutBreakerCount = FCString::Atoi(GetWorld()->URL.GetOption(TEXT("BHBreakerCount="), TEXT("0")));
	LayoutDensity = FCString::Atoi(GetWorld()->URL.GetOption(TEXT("BHLayoutDensity="), TEXT("0")));
	bForceProceduralLayout = IsTrueOption(GetWorld()->URL.GetOption(TEXT("BHForceProcedural="), TEXT("")));

	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->SetPersistentStageIndex(RuntimeStageIndex);
	}

	// The pre-game train LOBBY is never authored and never auto-advances; it just parks players in a carriage
	// until the round starts, then travels to LobbyFirstLevelName. Dispatch it first.
	if (bLobbyLevel)
	{
		RuntimeLevelName = TEXT("TrainLobby");
		NextRuntimeLevelName = LobbyFirstLevelName;
		BuildTrainLobbyLevel();
		return;
	}

	// Authored maps (Facility/Substation/Foggrounds shipped as real .umap assets) carry a single
	// ABHLevelMarker. The train intermission is never authored, so it always uses the generator below.
	if (!bTrainIntermissionLevel && DiscoverAuthoredLevel())
	{
		NextRuntimeLevelName = GetNextMapAfterStage(RuntimeStageIndex);
		return;
	}

	if (bTrainIntermissionLevel)
	{
		NextRuntimeLevelName = GetNextMapAfterStage(RuntimeStageIndex);
		BuildTrainIntermissionLevel();
		return;
	}

	if (RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		RuntimeLevelName = TEXT("Substation");
		NextRuntimeLevelName = GetNextMapAfterStage(RuntimeStageIndex);
		BuildSubstationLevel();
		return;
	}
	if (RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		RuntimeLevelName = TEXT("Foggrounds");
		NextRuntimeLevelName = GetNextMapAfterStage(RuntimeStageIndex);
		BuildFoggroundsLevel();
		return;
	}

	RuntimeLevelName = TEXT("Facility");
	NextRuntimeLevelName = GetNextMapAfterStage(RuntimeStageIndex);
	BuildFacilityLevel();
}

void ABHGameMode::SpawnMapCollectables(const FString& MapPrefix, const TArray<FVector>& Locations)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < Locations.Num(); ++Index)
	{
		if (ABHCollectable* Relic = World->SpawnActor<ABHCollectable>(ABHCollectable::StaticClass(), Locations[Index] + FVector(0.0f, 0.0f, 45.0f), FRotator::ZeroRotator, Params))
		{
			Relic->Configure(FName(*FString::Printf(TEXT("%s_%d"), *MapPrefix, Index)));
		}
	}
}

void ABHGameMode::BuildTutorialLevel()
{
	// The Escape-from-menu easter egg reuses this tutorial map slot but builds its own hidden HUB instead of the
	// teaching greybox.
	if (RuntimeTutorialPhase == EBHTutorialPhase::EasterEgg)
	{
		BuildEasterEggRoom();
		return;
	}

	// A larger linear greybox for the self-serve solo tutorial, sized so the Teacher encounter is a REAL
	// chase. Teaching rooms run west->east, separated by partition walls with a central 6 m doorway:
	// (1) entry/move/flashlight, (2) locker alcove, (3) crawl-duct room, (4) breaker/blackout room, then a
	// long open eastern hall holding (5) the visual question station and (6) the chase arena with cover +
	// lockers, ending at (7) the green exit. The lone student spawns far west; the Teacher spawns in the
	// arena, between the student and the exit, so reaching the exit means evading an active pursuer.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	SurvivorSpawns = { FVector(-200.0f, 0.0f, 120.0f) };
	HunterSpawn = FVector(9400.0f, 0.0f, 120.0f);

	const FLinearColor FloorTint(0.12f, 0.14f, 0.15f, 1.0f);
	const FLinearColor CeilingTint(0.06f, 0.07f, 0.08f, 1.0f);
	const FLinearColor WallTint(0.27f, 0.30f, 0.33f, 1.0f);
	const FLinearColor RoomTint(0.20f, 0.24f, 0.27f, 1.0f);

	// Shell: floor + ceiling spanning X[-600,11400] x Y[-1800,1800], ceiling at 340 cm.
	SpawnBlock(FVector(5400.0f, 0.0f, CenterZForBlockTop(0.0f, 0.30f)), FVector(120.0f, 36.0f, 0.30f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(5400.0f, 0.0f, CenterZForBlockBottom(340.0f, 0.15f)), FVector(120.0f, 36.0f, 0.15f), CeilingTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	// Perimeter walls (3.4 m tall).
	SpawnBlock(FVector(-600.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.4f, 36.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(11400.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.4f, 36.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(5400.0f, -1800.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(120.0f, 0.4f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(5400.0f, 1800.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(120.0f, 0.4f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	// Internal partitions for the teaching rooms only: each is two segments leaving a 6 m central doorway
	// (Y[-300,300]). The eastern hall (station + arena + exit) east of X=6400 is left OPEN for the chase.
	const TArray<float> PartitionX = { 1600.0f, 3200.0f, 4800.0f, 6400.0f };
	for (const float WallX : PartitionX)
	{
		SpawnBlock(FVector(WallX, -1050.0f, CenterZForBlockBottom(0.0f, 3.2f)), FVector(0.4f, 15.0f, 3.2f), RoomTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
		SpawnBlock(FVector(WallX, 1050.0f, CenterZForBlockBottom(0.0f, 3.2f)), FVector(0.4f, 15.0f, 3.2f), RoomTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	}

	// Ceiling flicker lights down the centre + arena so the greybox is navigable (the flashlight + breaker
	// blackout lessons read). These are the lights the breaker step cuts and restores.
	const TArray<FVector> LightPoints = {
		FVector(0.0f, 0.0f, 320.0f), FVector(2400.0f, 0.0f, 320.0f), FVector(4000.0f, 0.0f, 320.0f),
		FVector(5600.0f, 0.0f, 320.0f), FVector(7000.0f, 0.0f, 320.0f), FVector(8400.0f, 700.0f, 320.0f),
		FVector(8400.0f, -700.0f, 320.0f), FVector(9800.0f, 0.0f, 320.0f), FVector(11000.0f, 0.0f, 320.0f)
	};
	for (const FVector& LightPoint : LightPoints)
	{
		if (ABHFlickerLight* Light = World->SpawnActor<ABHFlickerLight>(LightPoint, FRotator::ZeroRotator))
		{
			FlickerLights.Add(Light);
		}
	}

	// Reusable prone-only crawl duct: two low side walls + a low roof leave a ~2 m-wide, ~1.5 m-tall tunnel a
	// standing pawn can't enter; the ABHCrawlSpaceVolume ejects anyone who isn't prone/sliding. Mirrors
	// BuildFoggroundsLevel's SpawnCrawlTunnel so the export records the gate onto the ABHLevelMarker for
	// DiscoverAuthoredLevel to rebuild even if the volume actor is later deleted while remeshing.
	auto SpawnTutorialCrawlTunnel = [&](float cx, float cy, float yaw, float lenScale)
	{
		const float Rad = FMath::DegreesToRadians(yaw);
		const float Cs = FMath::Cos(Rad);
		const float Sn = FMath::Sin(Rad);
		auto LP = [&](float lx, float ly, float z) { return FVector(cx + lx * Cs - ly * Sn, cy + lx * Sn + ly * Cs, z); };
		const FRotator Rot(0.0f, yaw, 0.0f);
		const float SideH = 1.55f;
		SpawnBlock(LP(0.0f, -100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), WallTint, Rot, true, EBHBlockMaterial::Concrete);
		SpawnBlock(LP(0.0f, 100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), WallTint, Rot, true, EBHBlockMaterial::Concrete);
		SpawnBlock(LP(0.0f, 0.0f, CenterZForBlockBottom(150.0f, 0.12f)), FVector(lenScale, 2.1f, 0.12f), WallTint, Rot, true, EBHBlockMaterial::Concrete);
		if (ABHCrawlSpaceVolume* Crawl = World->SpawnActor<ABHCrawlSpaceVolume>(LP(0.0f, 0.0f, 80.0f), Rot))
		{
			Crawl->Configure(FVector(lenScale * 50.0f + 40.0f, 95.0f, 110.0f));
		}
	};

	// Zone 2 (X 1600..3200): the LOCKER to hide in (north side of the alcove).
	World->SpawnActor<ABHLocker>(FVector(2400.0f, 1350.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f));

	// Zone 3 (X 3200..4800): the prone-only CRAWL DUCT, south side, running east-west, clear of the doorway.
	SpawnTutorialCrawlTunnel(4000.0f, -1250.0f, 0.0f, 5.0f);

	// Zone 4 (X 4800..6400): the BREAKER the student repairs during the blackout (north side, easy to find by
	// flashlight once the lights cut). The director cuts every flicker light on the Breaker step and restores
	// them when this is repaired.
	if (ABHBreaker* Breaker = World->SpawnActor<ABHBreaker>(FVector(5600.0f, 1250.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		BreakerActors.Add(Breaker);
	}

	// Zone 5 (eastern hall, west end): TWO question STATIONS. The tutorial director sorts stations by X and
	// overrides their questions at runtime: the westerly one (X 6700) becomes the VISUAL diagram question
	// (ConfigureTutorialVisualQuestion), the easterly one (X 7500) becomes the INTERACTIVE drag-drop question
	// (ConfigureTutorialInteractiveQuestion). Placed on opposite walls so both are clearly separate nodes.
	if (ABHObjectiveStation* VisualStation = World->SpawnActor<ABHObjectiveStation>(FVector(6700.0f, -900.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		VisualStation->Configure(EBHObjectiveStationType::Terminal);
		ObjectiveStations.Add(VisualStation);
	}
	if (ABHObjectiveStation* InteractiveStation = World->SpawnActor<ABHObjectiveStation>(FVector(7500.0f, 900.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		InteractiveStation->Configure(EBHObjectiveStationType::Valve);
		ObjectiveStations.Add(InteractiveStation);
	}

	// Zone 6 (X 8000..10800): the open CHASE ARENA. Full-height pillars + waist-high crates give cover to
	// break line of sight, and two lockers give mid-chase hiding spots, staggered with clear running lanes so
	// the Teacher (spawned mid-arena at X=9400) can be juked on the way to the exit.
	const TArray<FVector> CoverPillars = {
		FVector(8200.0f, -700.0f, 0.0f), FVector(8200.0f, 700.0f, 0.0f),
		FVector(8900.0f, 0.0f, 0.0f),
		FVector(9600.0f, -800.0f, 0.0f), FVector(9600.0f, 800.0f, 0.0f),
		FVector(10300.0f, 0.0f, 0.0f), FVector(10300.0f, -1200.0f, 0.0f), FVector(10300.0f, 1200.0f, 0.0f)
	};
	for (const FVector& Pillar : CoverPillars)
	{
		SpawnBlock(FVector(Pillar.X, Pillar.Y, CenterZForBlockBottom(0.0f, 2.8f)), FVector(1.5f, 1.5f, 2.8f), RoomTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}
	const TArray<FVector> Crates = {
		FVector(8600.0f, -1300.0f, 0.0f), FVector(8600.0f, 1300.0f, 0.0f),
		FVector(9200.0f, 1100.0f, 0.0f), FVector(9950.0f, -1150.0f, 0.0f), FVector(10600.0f, 600.0f, 0.0f)
	};
	for (const FVector& Crate : Crates)
	{
		SpawnBlock(FVector(Crate.X, Crate.Y, CenterZForBlockBottom(0.0f, 0.9f)), FVector(1.1f, 1.1f, 0.9f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}
	// Two arena lockers (north + south) so the student can practise hiding mid-chase.
	World->SpawnActor<ABHLocker>(FVector(8500.0f, 1550.0f, 95.0f), FRotator(0.0f, -90.0f, 0.0f));
	World->SpawnActor<ABHLocker>(FVector(10100.0f, -1550.0f, 95.0f), FRotator(0.0f, 90.0f, 0.0f));

	// Zone 7 (X ~11200): the green EXIT at the east end (faces back west toward the approaching student).
	if (ABHExitGate* ExitGate = World->SpawnActor<ABHExitGate>(FVector(11200.0f, 0.0f, 120.0f), FRotator(0.0f, 180.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}

	// The guided-lesson brain, baked into the map so it self-activates on the listen-server host at runtime.
	World->SpawnActor<ABHTutorialDirector>(FVector(5400.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
}

void ABHGameMode::BuildBackroomsFacility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// ---- One large, continuous "constant mesh" hall (Backrooms): a single big space under a low ceiling,
	// filled with a REGULAR grid of identical full-height concrete pillars so every direction looks the same
	// and you can't keep a bearing, then woven through with MANY freestanding angled partitions that carve
	// tight slots, blind corners and dead-end alcoves - cover to break the hunter's line of sight and hide.
	// There are no enclosed rooms; it reads as one endless, oppressive space built for hiding and tension. ----
	constexpr int32 Cols = 26;
	constexpr int32 Rows = 21;
	constexpr float Pitch = 540.0f;        // tight pillar spacing -> uniform, occluding, close-quarters
	constexpr float PillarHalf = 0.90f;    // SpawnBlock scale x100 => 180u (1.8m) square column
	constexpr float WallThick = 0.30f;     // partitions: x100 => 30u thick
	constexpr float CeilZ = 260.0f;        // low, oppressive roof
	const float WallH = CeilZ / 100.0f;
	const float W = Cols * Pitch;
	const float D = Rows * Pitch;
	const float X0 = -W * 0.5f;
	const float Y0 = -D * 0.5f;
	auto NodeXY = [&](int32 c, int32 r) { return FVector(X0 + (c + 0.5f) * Pitch, Y0 + (r + 0.5f) * Pitch, 0.0f); };
	auto IsSpawnNode = [&](int32 c, int32 r) { return c >= Cols - 2 && r >= Rows - 2; };   // SE 2x2 spawn cluster
	auto IsExitNode = [&](int32 c, int32 r) { return c <= 1 && r <= 1; };                  // NW exit corner

	// Host layout seed: a non-zero LayoutSeed yields a fresh-but-reproducible partition/clutter arrangement;
	// 0 keeps the fixed shipping layout. (The pillar grid + objective placement are deterministic index math,
	// so the seed varies cover/partitions, not breaker/exit positions.)
	const int32 EffectiveLayoutSeed = LayoutSeed > 0 ? LayoutSeed : 20260531;
	FRandomStream Rng(EffectiveLayoutSeed);

	// ---- Dark, grimy, mono palette (the WA materials carry the texture colour; tints stay near-black). ----
	const FLinearColor FloorTint(0.09f, 0.09f, 0.10f, 1.0f);
	const FLinearColor CeilTint(0.05f, 0.05f, 0.055f, 1.0f);
	const FLinearColor WallTint(0.13f, 0.13f, 0.14f, 1.0f);

	// Floor + ceiling slabs (world-aligned so the large planes tile properly).
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(W / 100.0f, D / 100.0f, 0.25f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWA);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(CeilZ, 0.12f)), FVector(W / 100.0f, D / 100.0f, 0.12f), CeilTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PlasterWA);
	AddMapContainment(W * 0.5f, D * 0.5f);

	// Helper: keep the spawn cluster and the exit corner clear of partitions/clutter.
	auto InReserved = [&](float x, float y, float pad)
	{
		const FVector Sp = NodeXY(Cols - 1, Rows - 1);
		const FVector Ex = NodeXY(0, 0);
		return FVector2D::Distance(FVector2D(x, y), FVector2D(Sp.X, Sp.Y)) < pad
			|| FVector2D::Distance(FVector2D(x, y), FVector2D(Ex.X, Ex.Y)) < pad;
	};
	auto SpawnPartition = [&](float cx, float cy, float yaw, float len)
	{
		if (len < 40.0f) { return; }
		SpawnBlock(FVector(cx, cy, CenterZForBlockBottom(0.0f, WallH)), FVector(len / 100.0f, WallThick, WallH), WallTint, FRotator(0.0f, yaw, 0.0f), true, EBHBlockMaterial::ConcreteWA);
	};

	// ---- Pre-plan the prone-only crawl tunnels FIRST, so pillars/partitions can be cleared out of each
	// tunnel's footprint (otherwise a pillar/partition sits in the tunnel and blocks it). Entry = (cx,cy,yaw,lenScale). ----
	// DENSER crawl network (16 -> 26): crawl tunnels are ADDITIVE prone passages (they only ever add a
	// route, never seal the hall), so densifying is zero-risk for connectivity. The last 10 are LONGER
	// (lenScale ~6.0-6.5) so the standing hunter's detour around them is bigger -- a real shortcut/escape
	// advantage, not just a hidey-hole. A prone survivor inside is uncapturable (crawl-space immunity).
	const float CrawlSpecs[26][4] = {
		{ 4.0f, 4.0f, 30.0f, 5.0f }, { 9.0f, 3.0f, 90.0f, 4.8f }, { 14.0f, 5.0f, 150.0f, 5.2f },
		{ 20.0f, 4.0f, 60.0f, 4.6f }, { 23.0f, 9.0f, 90.0f, 5.0f }, { 3.0f, 9.0f, 0.0f, 5.2f },
		{ 8.0f, 10.0f, 45.0f, 4.8f }, { 13.0f, 11.0f, 120.0f, 5.0f }, { 18.0f, 12.0f, 75.0f, 4.8f },
		{ 22.0f, 13.0f, 30.0f, 4.6f }, { 5.0f, 15.0f, 90.0f, 5.0f }, { 10.0f, 17.0f, 135.0f, 5.2f },
		{ 15.0f, 16.0f, 15.0f, 4.8f }, { 19.0f, 18.0f, 60.0f, 5.0f }, { 12.0f, 8.0f, 105.0f, 4.8f },
		{ 6.0f, 12.0f, 160.0f, 5.0f },
		// --- denser pass: 10 longer through-tunnels woven across the mid-hall ---
		{ 6.0f, 6.0f, 120.0f, 6.2f }, { 11.0f, 6.0f, 45.0f, 6.0f }, { 16.0f, 7.0f, 90.0f, 6.4f },
		{ 21.0f, 7.0f, 135.0f, 6.0f }, { 4.0f, 13.0f, 60.0f, 6.2f }, { 9.0f, 13.0f, 0.0f, 6.5f },
		{ 16.0f, 14.0f, 105.0f, 6.0f }, { 20.0f, 16.0f, 150.0f, 6.2f }, { 13.0f, 17.0f, 75.0f, 6.0f },
		{ 7.0f, 16.0f, 30.0f, 6.4f }
	};
	TArray<FVector4> CrawlPlacements;
	for (const float (&Cspec)[4] : CrawlSpecs)
	{
		const FVector P = NodeXY(static_cast<int32>(Cspec[0]), static_cast<int32>(Cspec[1]));
		if (InReserved(P.X, P.Y, 2.0f * Pitch)) { continue; }
		CrawlPlacements.Add(FVector4(P.X, P.Y, Cspec[2], Cspec[3]));
	}
	// True if (x,y) is inside any crawl tunnel's footprint (oriented box) - used to clear pillars/partitions.
	auto InCrawlClearZone = [&](float x, float y)
	{
		for (const FVector4& T : CrawlPlacements)
		{
			const float Rad = FMath::DegreesToRadians(T.Z);
			const float Cs = FMath::Cos(Rad);
			const float Sn = FMath::Sin(Rad);
			const float dx = x - T.X;
			const float dy = y - T.Y;
			const float lx = dx * Cs + dy * Sn;     // world -> tunnel-local (inverse yaw)
			const float ly = -dx * Sn + dy * Cs;
			if (FMath::Abs(lx) < T.W * 50.0f + 120.0f && FMath::Abs(ly) < 175.0f) { return true; }
		}
		return false;
	};

	// ---- The constant mesh: a regular grid of identical full-height pillars. ----
	int32 PillarCount = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			if (IsSpawnNode(c, r) || IsExitNode(c, r)) { continue; }   // keep spawn/exit clear
			const FVector P = NodeXY(c, r);
			if (InCrawlClearZone(P.X, P.Y)) { continue; }              // keep crawl tunnels unobstructed
			SpawnBlock(FVector(P.X, P.Y, CenterZForBlockBottom(0.0f, WallH)), FVector(PillarHalf, PillarHalf, WallH), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWA);
			++PillarCount;
		}
	}

	// ---- Tight cover: many freestanding angled partitions woven between the pillars. Finite segments at
	// random angles never seal the hall into rooms (it stays one connected space), but locally they make tight
	// slots, blind corners and dead-end alcoves to break line of sight and hide from the hunter. ----
	// Host layout density scales the partition (cover) count; 0 = default 100%. Clamped so the hall never
	// seals into rooms (too dense) nor becomes an empty box (too sparse).
	const int32 LayoutDensityPercent = LayoutDensity > 0 ? FMath::Clamp(LayoutDensity, 50, 160) : 100;
	const int32 TargetPartitions = (Cols * Rows * 11 * LayoutDensityPercent) / (10 * 100);   // ~1.1 per node at 100%
	int32 PartCount = 0;
	for (int32 i = 0; i < TargetPartitions * 3 && PartCount < TargetPartitions; ++i)
	{
		const float px = Rng.FRandRange(X0 + Pitch, X0 + W - Pitch);
		const float py = Rng.FRandRange(Y0 + Pitch, Y0 + D - Pitch);
		if (InReserved(px, py, 2.2f * Pitch) || InCrawlClearZone(px, py)) { continue; }
		SpawnPartition(px, py, Rng.FRandRange(0.0f, 180.0f), Rng.FRandRange(2.4f, 6.0f) * 100.0f);   // 240..600
		++PartCount;
	}

	// ---- A handful of longer meandering wall runs for stronger occlusion and forced detours. ----
	int32 RunSegs = 0;
	for (int32 i = 0; i < 12; ++i)
	{
		float px = Rng.FRandRange(X0 + 2.0f * Pitch, X0 + W - 2.0f * Pitch);
		float py = Rng.FRandRange(Y0 + 2.0f * Pitch, Y0 + D - 2.0f * Pitch);
		float Yaw = Rng.FRandRange(0.0f, 360.0f);
		const int32 Segs = Rng.RandRange(3, 6);
		for (int32 s = 0; s < Segs; ++s)
		{
			const float Len = Rng.FRandRange(4.0f, 7.0f) * 100.0f;
			const float Rad = FMath::DegreesToRadians(Yaw);
			const float Dx = FMath::Cos(Rad);
			const float Dy = FMath::Sin(Rad);
			const float Cx = px + Dx * Len * 0.5f;
			const float Cy = py + Dy * Len * 0.5f;
			if (!InReserved(Cx, Cy, 2.0f * Pitch) && !InCrawlClearZone(Cx, Cy)
				&& Cx > X0 + Pitch && Cx < X0 + W - Pitch && Cy > Y0 + Pitch && Cy < Y0 + D - Pitch)
			{
				SpawnPartition(Cx, Cy, Yaw, Len);
				++RunSegs;
			}
			px += Dx * Len;
			py += Dy * Len;
			Yaw += Rng.FRandRange(-40.0f, 40.0f);
			px = FMath::Clamp(px, X0 + Pitch, X0 + W - Pitch);
			py = FMath::Clamp(py, Y0 + Pitch, Y0 + D - Pitch);
		}
	}

	// ---- Perimeter: solid concrete on all four sides; one exit doorway in the NW corner (west wall). ----
	auto SpawnSeg = [&](float Cx, float Cy, bool bVertical, float Length)
	{
		if (Length < 40.0f) { return; }
		const FVector Scale = bVertical ? FVector(WallThick, Length / 100.0f, WallH) : FVector(Length / 100.0f, WallThick, WallH);
		SpawnBlock(FVector(Cx, Cy, CenterZForBlockBottom(0.0f, WallH)), Scale, WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWA);
	};
	SpawnSeg(0.0f, Y0, false, W);            // north outer
	SpawnSeg(0.0f, Y0 + D, false, W);        // south outer
	SpawnSeg(X0 + W, 0.0f, true, D);         // east outer
	{
		constexpr float Door = 260.0f;
		const float ExitY = NodeXY(0, 0).Y;
		const float LowLen = (ExitY - Door * 0.5f) - Y0;
		const float HiLen = (Y0 + D) - (ExitY + Door * 0.5f);
		if (LowLen > 40.0f) { SpawnSeg(X0, Y0 + LowLen * 0.5f, true, LowLen); }
		if (HiLen > 40.0f) { SpawnSeg(X0, (Y0 + D) - HiLen * 0.5f, true, HiLen); }
	}

	// ---- Hiding: prone-only crawl tunnels (low concrete ducts). A standing hunter can't follow through the
	// ~1.5m roof; a survivor crawls in to escape or hide. Built as two low side walls + a low roof, open at
	// both ends, with an ABHCrawlSpaceVolume gate that rejects standing characters. ----
	auto SpawnCrawlTunnel = [&](float cx, float cy, float yaw, float lenScale)
	{
		const float Rad = FMath::DegreesToRadians(yaw);
		const float Cs = FMath::Cos(Rad);
		const float Sn = FMath::Sin(Rad);
		auto LP = [&](float lx, float ly, float z) { return FVector(cx + lx * Cs - ly * Sn, cy + lx * Sn + ly * Cs, z); };
		const FRotator Rot(0.0f, yaw, 0.0f);
		const float SideH = 1.55f;
		SpawnBlock(LP(0.0f, -100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), WallTint, Rot, true, EBHBlockMaterial::ConcreteWA);
		SpawnBlock(LP(0.0f, 100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), WallTint, Rot, true, EBHBlockMaterial::ConcreteWA);
		SpawnBlock(LP(0.0f, 0.0f, CenterZForBlockBottom(150.0f, 0.12f)), FVector(lenScale, 2.1f, 0.12f), WallTint, Rot, true, EBHBlockMaterial::ConcreteWA);
		if (ABHCrawlSpaceVolume* Crawl = World->SpawnActor<ABHCrawlSpaceVolume>(LP(0.0f, 0.0f, 80.0f), Rot))
		{
			// Y half-extent 105 (>= the side walls at +/-100) so the whole between-walls passage is inside the
			// shelter volume -- a prone survivor anywhere in the duct is uncapturable, with no edge margin where
			// the Teacher could reach in. (Was 95, which left a 5u sliver outside the immunity volume.)
			Crawl->Configure(FVector(lenScale * 50.0f + 40.0f, 105.0f, 110.0f));
		}
	};
	int32 CrawlCount = 0;
	for (const FVector4& T : CrawlPlacements)   // pre-planned above; pillars/partitions already cleared from these
	{
		SpawnCrawlTunnel(T.X, T.Y, T.Z, T.W);
		++CrawlCount;
	}

	// Lockers dotted through the hall (door faces -Y at yaw 0). Body is ~2.3m tall and centred on the actor,
	// so origin z=116 sits the base on the floor.
	int32 LockerCount = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			if (IsSpawnNode(c, r) || IsExitNode(c, r)) { continue; }
			if (((c % 5) == 0) && ((r % 6) == 3))
			{
				const FVector P = NodeXY(c, r) + FVector(0.0f, Pitch * 0.5f, 116.0f);   // aisle between this node and the next
				if (InReserved(P.X, P.Y, 1.6f * Pitch)) { continue; }
				World->SpawnActor<ABHLocker>(P, FRotator(0.0f, static_cast<float>(((c + r) % 4) * 90), 0.0f));
				++LockerCount;
			}
		}
	}

	// ---- Blue panel graphics: glowing sci-fi screens on some pillar faces + a soft (always-on) blue light,
	// as colour accents and faint orientation landmarks in the grey hall. ----
	int32 PanelCount = 0;
	const FLinearColor PanelBlue(0.20f, 0.55f, 1.0f, 1.0f);
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			if (IsSpawnNode(c, r) || IsExitNode(c, r)) { continue; }
			if (((c % 7) == 2) && ((r % 4) == 1))
			{
				const FVector Node = NodeXY(c, r);
				const float FaceX = Node.X + PillarHalf * 100.0f + 4.0f;   // on the pillar's +X face
				SpawnBlock(FVector(FaceX, Node.Y, CenterZForBlockBottom(120.0f, 1.0f)), FVector(0.06f, 1.4f, 1.0f), PanelBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::BluePanel);
				if (ABHFlickerLight* Light = World->SpawnActor<ABHFlickerLight>(FVector(FaceX + 120.0f, Node.Y, 175.0f), FRotator::ZeroRotator))
				{
					Light->Configure(0, FLinearColor(0.30f, 0.55f, 1.0f, 1.0f), 230.0f, 430.0f);
					FlickerLights.Add(Light);
				}
				++PanelCount;
			}
		}
	}

	// ---- Spawns: 12 survivors clustered in the cleared SE corner. ----
	SurvivorSpawns.Reset();
	const int32 SpawnNodes[4][2] = { {Cols - 1, Rows - 1}, {Cols - 2, Rows - 1}, {Cols - 1, Rows - 2}, {Cols - 2, Rows - 2} };
	const float SpawnOX[3] = { -150.0f, 150.0f, 0.0f };
	const float SpawnOY[3] = { -150.0f, 150.0f, 0.0f };
	for (int32 i = 0; i < 12; ++i)
	{
		const int32 (&Sc)[2] = SpawnNodes[i % 4];
		const int32 k = i / 4;
		const FVector Base = NodeXY(Sc[0], Sc[1]) + FVector(0.0f, 0.0f, 120.0f);
		SurvivorSpawns.Add(Base + FVector(SpawnOX[k], SpawnOY[k], 0.0f));
	}

	// ---- One clear exit in the NW corner. ----
	if (ABHExitGate* ExitGate = World->SpawnActor<ABHExitGate>(NodeXY(0, 0) + FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator))
	{
		ExitGates.Add(ExitGate);
	}

	// ---- Lighting: mostly dark for tension; a sparse dim grid + a few brighter "spot" pools for orientation
	// relief. Kept light-count low (weak lab GPUs) and the map dark so the flashlight matters. ----
	int32 LightCount = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			if (IsSpawnNode(c, r) || IsExitNode(c, r)) { continue; }
			const bool bDim = ((c % 5) == 2) && ((r % 5) == 2);
			const bool bSpot = ((c % 8) == 4) && ((r % 7) == 3);
			if (bDim || bSpot)
			{
				if (ABHFlickerLight* Light = World->SpawnActor<ABHFlickerLight>(NodeXY(c, r) + FVector(0.0f, 0.0f, 235.0f), FRotator::ZeroRotator))
				{
					const FLinearColor Col = bSpot ? FLinearColor(0.95f, 0.86f, 0.70f, 1.0f) : FLinearColor(0.55f, 0.60f, 0.70f, 1.0f);
					Light->Configure((LightCount % 6) + 1, Col, bSpot ? 720.0f : 240.0f, bSpot ? 1000.0f : 650.0f);
					FlickerLights.Add(Light);
					++LightCount;
				}
			}
		}
	}

	// ---- Distribute question stations, breakers, switches, battery pickups across the hall (skip reserved). ----
	const EBHObjectiveStationType StationCycle[4] = { EBHObjectiveStationType::Terminal, EBHObjectiveStationType::Valve, EBHObjectiveStationType::Evidence, EBHObjectiveStationType::Antenna };
	int32 StationCount = 0;
	int32 BreakerCount = 0;
	int32 SwitchCount = 0;
	for (int32 r = 0; r < Rows; ++r)
	{
		for (int32 c = 0; c < Cols; ++c)
		{
			if (IsSpawnNode(c, r) || IsExitNode(c, r)) { continue; }
			if (StationCount < 47 && ((c * 3 + r * 2) % 11) == 0)
			{
				if (ABHObjectiveStation* Station = World->SpawnActor<ABHObjectiveStation>(NodeXY(c, r) + FVector(Pitch * 0.28f, 0.0f, 95.0f), FRotator(0.0f, static_cast<float>((c * 53 + r * 31) % 360), 0.0f)))
				{
					Station->Configure(StationCycle[StationCount % 4]);
					ObjectiveStations.Add(Station);
					++StationCount;
				}
			}
			else if (BreakerCount < (LayoutBreakerCount > 0 ? FMath::Clamp(LayoutBreakerCount, 3, 12) : 7) && ((c + 2 * r) % 17) == 0)
			{
				if (ABHBreaker* Breaker = World->SpawnActor<ABHBreaker>(NodeXY(c, r) + FVector(0.0f, Pitch * 0.28f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)))
				{
					BreakerActors.Add(Breaker);
					++BreakerCount;
				}
			}
			if (SwitchCount < 6 && ((c * 2 + r) % 23) == 0)
			{
				if (ABHPowerSwitch* Switch = World->SpawnActor<ABHPowerSwitch>(NodeXY(c, r) + FVector(-Pitch * 0.28f, 0.0f, 120.0f), FRotator(0.0f, -90.0f, 0.0f)))
				{
					Switch->Configure(SwitchCount + 1, FText::FromString(FString::Printf(TEXT("Toggle Circuit %d"), SwitchCount + 1)));
					++SwitchCount;
				}
			}
			if (((c + r) % 7) == 3)
			{
				World->SpawnActor<ABHBatteryPickup>(NodeXY(c, r) + FVector(0.0f, -Pitch * 0.26f, 70.0f), FRotator(0.0f, 90.0f, 0.0f));
			}
		}
	}

	SpawnMapCollectables(TEXT("tutorial"), {
		NodeXY(Cols / 4, Rows / 4) + FVector(0.0f, 0.0f, 110.0f), NodeXY(Cols / 2, Rows / 4) + FVector(0.0f, 0.0f, 110.0f),
		NodeXY(Cols * 3 / 4, Rows / 2) + FVector(0.0f, 0.0f, 110.0f), NodeXY(Cols / 4, Rows * 3 / 4) + FVector(0.0f, 0.0f, 110.0f),
		NodeXY(Cols / 2, Rows * 3 / 4) + FVector(0.0f, 0.0f, 110.0f)
	});

	UE_LOG(LogTemp, Display, TEXT("[Backrooms] Facility hall %dx%d nodes: %d pillars, %d partitions (+%d run segs), %d lockers, %d crawl tunnels, %d blue panels, %d stations, %d breakers, %d switches, %d lights, %d spawns."),
		Cols, Rows, PillarCount, PartCount, RunSegs, LockerCount, CrawlCount, PanelCount, StationCount, BreakerCount, SwitchCount, LightCount, SurvivorSpawns.Num());
}

void ABHGameMode::BuildFacilityLevel()
{
	// Whole-map backrooms conversion: route the Facility to the dark-liminal maze generator instead of the
	// legacy hand-authored industrial layout retained below. bBuildBackroomsFacility is a (non-const) member
	// so the legacy body stays reachable to the compiler (no unreachable-code warning) and remains a fallback.
	if (bBuildBackroomsFacility)
	{
		BuildBackroomsFacility();
		return;
	}

	SurvivorSpawns = {
		FVector(4320.0f, -1320.0f, 120.0f),
		FVector(4520.0f, -960.0f, 120.0f),
		FVector(4380.0f, -560.0f, 120.0f),
		FVector(4620.0f, -180.0f, 120.0f),
		FVector(4380.0f, 220.0f, 120.0f),
		FVector(4620.0f, 620.0f, 120.0f),
		FVector(4320.0f, 1020.0f, 120.0f),
		FVector(4520.0f, 1380.0f, 120.0f),
		FVector(4040.0f, -840.0f, 120.0f),
		FVector(4040.0f, 840.0f, 120.0f),
		FVector(4860.0f, -1180.0f, 120.0f),
		FVector(4860.0f, 1180.0f, 120.0f)
	};

	const FLinearColor FloorTint(0.12f, 0.14f, 0.15f, 1.0f);
	const FLinearColor CeilingTint(0.06f, 0.07f, 0.08f, 1.0f);
	const FLinearColor WallTint(0.28f, 0.31f, 0.33f, 1.0f);
	const FLinearColor UtilityTint(0.19f, 0.23f, 0.25f, 1.0f);
	const FLinearColor StorageTint(0.30f, 0.27f, 0.19f, 1.0f);
	const FLinearColor LabTint(0.16f, 0.26f, 0.30f, 1.0f);
	const FLinearColor WardTint(0.22f, 0.18f, 0.24f, 1.0f);
	const FLinearColor HazardTint(0.40f, 0.08f, 0.06f, 1.0f);

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(110.0f, 88.0f, 0.25f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(335.0f, 0.12f)), FVector(110.0f, 88.0f, 0.12f), CeilingTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	SpawnBlock(FVector(0.0f, -4400.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(110.0f, 0.35f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(0.0f, 4400.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(110.0f, 0.35f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(-5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(0.35f, 88.0f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.35f)), FVector(0.35f, 88.0f, 3.35f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	AddMapContainment(5500.0f, 4400.0f);

	const TArray<TPair<FVector, FVector>> InternalWalls = {
		{FVector(-3000.0f, -2400.0f, 165.0f), FVector(26.0f, 0.28f, 3.1f)},
		{FVector(650.0f, -2400.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(3300.0f, -2400.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(-3300.0f, -1200.0f, 165.0f), FVector(22.0f, 0.28f, 3.1f)},
		{FVector(0.0f, -1200.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(3200.0f, -1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(-3300.0f, 1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(0.0f, 1200.0f, 165.0f), FVector(18.0f, 0.28f, 3.1f)},
		{FVector(3300.0f, 1200.0f, 165.0f), FVector(20.0f, 0.28f, 3.1f)},
		{FVector(-2800.0f, 2400.0f, 165.0f), FVector(26.0f, 0.28f, 3.1f)},
		{FVector(1200.0f, 2400.0f, 165.0f), FVector(24.0f, 0.28f, 3.1f)},
		{FVector(-3000.0f, -2850.0f, 165.0f), FVector(0.28f, 15.0f, 3.1f)},
		{FVector(-3000.0f, -450.0f, 165.0f), FVector(0.28f, 9.0f, 3.1f)},
		{FVector(-3000.0f, 1850.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(-1500.0f, -2550.0f, 165.0f), FVector(0.28f, 17.0f, 3.1f)},
		{FVector(-1500.0f, 300.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(-1500.0f, 2850.0f, 165.0f), FVector(0.28f, 11.0f, 3.1f)},
		{FVector(0.0f, -2900.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(0.0f, 1750.0f, 165.0f), FVector(0.28f, 15.0f, 3.1f)},
		{FVector(1500.0f, -2200.0f, 165.0f), FVector(0.28f, 20.0f, 3.1f)},
		{FVector(1500.0f, 200.0f, 165.0f), FVector(0.28f, 12.0f, 3.1f)},
		{FVector(1500.0f, 2600.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, -2700.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, -200.0f, 165.0f), FVector(0.28f, 14.0f, 3.1f)},
		{FVector(3000.0f, 2300.0f, 165.0f), FVector(0.28f, 16.0f, 3.1f)},
		{FVector(-5000.0f, -3200.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(-4650.0f, -3550.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(-4650.0f, -1850.0f, 165.0f), FVector(13.0f, 0.28f, 3.1f)},
		{FVector(-5000.0f, 2350.0f, 165.0f), FVector(0.28f, 22.0f, 3.1f)},
		{FVector(-4550.0f, 3500.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(-4550.0f, 1750.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(5000.0f, -2700.0f, 165.0f), FVector(0.28f, 20.0f, 3.1f)},
		{FVector(4550.0f, -3550.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(4700.0f, -1650.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)},
		{FVector(5000.0f, 2450.0f, 165.0f), FVector(0.28f, 18.0f, 3.1f)},
		{FVector(4550.0f, 3550.0f, 165.0f), FVector(15.0f, 0.28f, 3.1f)},
		{FVector(4700.0f, 1700.0f, 165.0f), FVector(12.0f, 0.28f, 3.1f)}
	};

	for (const TPair<FVector, FVector>& Wall : InternalWalls)
	{
		SpawnBlock(FVector(Wall.Key.X, Wall.Key.Y, CenterZForBlockBottom(0.0f, Wall.Value.Z)), Wall.Value, WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	}

	auto SpawnDoorAt = [this](const FVector& Location, const FRotator& Rotation)
	{
		if (ABHDoor* Door = GetWorld()->SpawnActor<ABHDoor>(Location, Rotation))
		{
			DoorActors.Add(Door);
		}
	};

	SpawnDoorAt(FVector(-900.0f, -2400.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(1900.0f, -2400.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-1550.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(1550.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-1600.0f, 1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(1600.0f, 1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-750.0f, 2400.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-3000.0f, -1500.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(-3000.0f, 475.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(-1500.0f, -1000.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(-1500.0f, 1600.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(1500.0f, -800.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(1500.0f, 1350.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(3000.0f, -1450.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(3000.0f, 1000.0f, 120.0f), FRotator::ZeroRotator);
	SpawnDoorAt(FVector(-5000.0f, -1850.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(-5000.0f, 1750.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(5000.0f, -1650.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnDoorAt(FVector(5000.0f, 1700.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f));

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-3950.0f, -2850.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(3850.0f, -2750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-3750.0f, 2800.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(550.0f, -3180.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)},
		{FVector(3650.0f, 2450.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-5200.0f, -3850.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(5150.0f, 3750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};

	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-4200.0f, -500.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(4200.0f, 1000.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-2400.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(2450.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-300.0f, -3050.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-5200.0f, 950.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(5200.0f, -950.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-2150.0f, -720.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-2150.0f, 720.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-650.0f, -720.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-650.0f, 720.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(850.0f, -720.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(850.0f, 720.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(2300.0f, -720.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(2300.0f, 720.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-4700.0f, -2850.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(4700.0f, 2850.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3800.0f, 2300.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(3800.0f, -2300.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-100.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(1200.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-3500.0f, -3050.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3500.0f, 3050.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(500.0f, -50.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-5200.0f, -2200.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(5200.0f, 2200.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-900.0f, -2300.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1850.0f, -2300.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3000.0f, 100.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3050.0f, 100.0f, 95.0f), EBHObjectiveStationType::Terminal}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	const FLinearColor RevisionBlue(0.16f, 0.58f, 0.88f, 1.0f);
	const FLinearColor RevisionYellow(0.95f, 0.78f, 0.18f, 1.0f);
	const FLinearColor RevisionGreen(0.10f, 0.72f, 0.48f, 1.0f);
	const FLinearColor RouteExitGreen(0.18f, 0.86f, 0.48f, 1.0f);
	const FLinearColor RouteStorageAmber(0.95f, 0.58f, 0.18f, 1.0f);
	const FLinearColor RouteLabCyan(0.16f, 0.70f, 0.92f, 1.0f);
	const FLinearColor RouteWardViolet(0.70f, 0.38f, 0.92f, 1.0f);
	const FLinearColor RouteUtilityWhite(0.86f, 0.92f, 0.82f, 1.0f);
	auto SpawnFacilityRouteStripe = [this](const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		SpawnBlock(Location, Scale, Tint, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const FVector RevisionHubCenters[] = {
		FVector(-2150.0f, 0.0f, CenterZForBlockBottom(0.65f, 0.035f)),
		FVector(-650.0f, 0.0f, CenterZForBlockBottom(0.70f, 0.035f)),
		FVector(850.0f, 0.0f, CenterZForBlockBottom(0.75f, 0.035f)),
		FVector(2300.0f, 0.0f, CenterZForBlockBottom(0.80f, 0.035f))
	};
	for (int32 HubIndex = 0; HubIndex < UE_ARRAY_COUNT(RevisionHubCenters); ++HubIndex)
	{
		const FVector Hub = RevisionHubCenters[HubIndex];
		const FLinearColor HubTint = (HubIndex % 3 == 0) ? RevisionBlue : ((HubIndex % 3 == 1) ? RevisionYellow : RevisionGreen);
		SpawnBlock(Hub, FVector(4.6f, 2.2f, 0.035f), HubTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
		SpawnBlock(Hub + FVector(0.0f, -150.0f, 1.0f), FVector(4.2f, 0.08f, 0.045f), RevisionYellow, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(Hub + FVector(0.0f, 150.0f, 1.0f), FVector(4.2f, 0.08f, 0.045f), RevisionYellow, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	}
	SpawnFacilityRouteStripe(FVector(-3650.0f, 0.0f, CenterZForBlockBottom(0.64f, 0.030f)), FVector(18.0f, 0.12f, 0.030f), RouteExitGreen);
	SpawnFacilityRouteStripe(FVector(3650.0f, 0.0f, CenterZForBlockBottom(0.64f, 0.030f)), FVector(18.0f, 0.12f, 0.030f), RouteExitGreen);
	SpawnFacilityRouteStripe(FVector(-2150.0f, -2350.0f, CenterZForBlockBottom(0.66f, 0.030f)), FVector(0.12f, 15.0f, 0.030f), RouteStorageAmber);
	SpawnFacilityRouteStripe(FVector(2450.0f, -2350.0f, CenterZForBlockBottom(0.66f, 0.030f)), FVector(0.12f, 14.5f, 0.030f), RouteLabCyan);
	SpawnFacilityRouteStripe(FVector(-2550.0f, 2350.0f, CenterZForBlockBottom(0.66f, 0.030f)), FVector(0.12f, 14.5f, 0.030f), RouteWardViolet);
	SpawnFacilityRouteStripe(FVector(250.0f, 2950.0f, CenterZForBlockBottom(0.66f, 0.030f)), FVector(18.0f, 0.12f, 0.030f), RouteUtilityWhite);
	SpawnFacilityRouteStripe(FVector(-3900.0f, -3050.0f, CenterZForBlockBottom(0.72f, 0.040f)), FVector(8.6f, 2.4f, 0.040f), RouteStorageAmber);
	SpawnFacilityRouteStripe(FVector(3700.0f, -3050.0f, CenterZForBlockBottom(0.72f, 0.040f)), FVector(8.6f, 2.4f, 0.040f), RouteLabCyan);
	SpawnFacilityRouteStripe(FVector(-3600.0f, 2750.0f, CenterZForBlockBottom(0.72f, 0.040f)), FVector(8.6f, 2.4f, 0.040f), RouteWardViolet);
	SpawnFacilityRouteStripe(FVector(500.0f, 3050.0f, CenterZForBlockBottom(0.72f, 0.040f)), FVector(8.8f, 2.1f, 0.040f), RouteUtilityWhite);
	if (ABHObjectiveStation* HiddenSwitch = GetWorld()->SpawnActor<ABHObjectiveStation>(FVector(-5125.0f, -3825.0f, 88.0f), FRotator(0.0f, 35.0f, 0.0f)))
	{
		HiddenSwitch->ConfigureTeacherMirrorTrapNode();
	}

	for (int32 ShelfIndex = 0; ShelfIndex < 5; ++ShelfIndex)
	{
		SpawnBlock(FVector(-3900.0f + ShelfIndex * 330.0f, -3050.0f, 75.0f), FVector(0.35f, 2.9f, 1.55f), StorageTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(FVector(-3920.0f + ShelfIndex * 350.0f, -2680.0f, 75.0f), FVector(2.5f, 0.35f, 1.35f), StorageTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	}

	for (int32 BenchIndex = 0; BenchIndex < 5; ++BenchIndex)
	{
		SpawnBlock(FVector(3250.0f + (BenchIndex % 2) * 520.0f, -3200.0f + BenchIndex * 260.0f, 20.0f), FVector(2.8f, 0.9f, 0.55f), LabTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	}

	for (int32 BedIndex = 0; BedIndex < 7; ++BedIndex)
	{
		const float X = -4000.0f + (BedIndex % 3) * 520.0f;
		const float Y = 2200.0f + (BedIndex / 3) * 420.0f;
		SpawnBlock(FVector(X, Y, 20.0f), FVector(2.6f, 1.1f, 0.45f), WardTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(X + 170.0f, Y - 70.0f, 105.0f), FVector(0.12f, 1.25f, 1.65f), WardTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}

	const FVector Crates[] = {
		FVector(-600.0f, -3250.0f, 40.0f), FVector(-250.0f, -3050.0f, 40.0f), FVector(150.0f, -3350.0f, 40.0f),
		FVector(1800.0f, -3250.0f, 40.0f), FVector(2200.0f, -2850.0f, 40.0f), FVector(2600.0f, -3200.0f, 40.0f),
		FVector(-750.0f, 3200.0f, 40.0f), FVector(-250.0f, 2920.0f, 40.0f), FVector(250.0f, 3250.0f, 40.0f),
		FVector(2100.0f, 1800.0f, 40.0f), FVector(2450.0f, 2150.0f, 40.0f), FVector(2150.0f, 2600.0f, 40.0f)
	};
	for (const FVector& Crate : Crates)
	{
		SpawnBlock(Crate, FVector(0.85f, 0.85f, 0.85f), UtilityTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}

	const FVector Pillars[] = {
		FVector(-900.0f, -900.0f, 120.0f), FVector(900.0f, -900.0f, 120.0f), FVector(-900.0f, 900.0f, 120.0f), FVector(900.0f, 900.0f, 120.0f),
		FVector(-4200.0f, 0.0f, 120.0f), FVector(4200.0f, 0.0f, 120.0f), FVector(0.0f, -3400.0f, 120.0f), FVector(0.0f, 3400.0f, 120.0f)
	};
	for (const FVector& Pillar : Pillars)
	{
		SpawnBlock(Pillar, FVector(0.75f, 0.75f, 2.4f), UtilityTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}

	const FVector LockerLocations[] = {
		FVector(-4200.0f, -3150.0f, 100.0f), FVector(-3450.0f, -3150.0f, 100.0f), FVector(-2350.0f, -3150.0f, 100.0f),
		FVector(3200.0f, -3320.0f, 100.0f), FVector(3850.0f, -3320.0f, 100.0f), FVector(4100.0f, -1850.0f, 100.0f),
		FVector(-4200.0f, 3150.0f, 100.0f), FVector(-3600.0f, 3150.0f, 100.0f), FVector(-2650.0f, 3150.0f, 100.0f),
		FVector(3600.0f, 3150.0f, 100.0f), FVector(4200.0f, 2700.0f, 100.0f), FVector(4200.0f, 1850.0f, 100.0f),
		FVector(-900.0f, -3300.0f, 100.0f), FVector(300.0f, -3300.0f, 100.0f), FVector(900.0f, -2100.0f, 100.0f),
		FVector(-900.0f, 3300.0f, 100.0f), FVector(300.0f, 3300.0f, 100.0f), FVector(900.0f, 2100.0f, 100.0f),
		FVector(-4300.0f, 500.0f, 100.0f), FVector(-4300.0f, -500.0f, 100.0f), FVector(4300.0f, 500.0f, 100.0f), FVector(4300.0f, -500.0f, 100.0f),
		FVector(-5200.0f, -3900.0f, 100.0f), FVector(-5200.0f, -1600.0f, 100.0f), FVector(-5200.0f, 1500.0f, 100.0f), FVector(-5200.0f, 3900.0f, 100.0f),
		FVector(5200.0f, -3900.0f, 100.0f), FVector(5200.0f, -1500.0f, 100.0f), FVector(5200.0f, 1500.0f, 100.0f), FVector(5200.0f, 3900.0f, 100.0f)
	};

	for (const FVector& Location : LockerLocations)
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	AddIndustrialClutter({
		FVector(-5050.0f, -3650.0f, 45.0f), FVector(-5050.0f, 3650.0f, 45.0f),
		FVector(5050.0f, -3650.0f, 45.0f), FVector(5050.0f, 3650.0f, 45.0f),
		FVector(-4300.0f, -3950.0f, 45.0f), FVector(4300.0f, 3950.0f, 45.0f)
	}, UtilityTint);

	const FVector BatteryLocations[] = {
		FVector(-3600.0f, -2000.0f, 70.0f), FVector(3900.0f, -1900.0f, 70.0f), FVector(-3900.0f, 1700.0f, 70.0f),
		FVector(3650.0f, 1500.0f, 70.0f), FVector(-650.0f, -2750.0f, 70.0f), FVector(550.0f, 2750.0f, 70.0f),
		FVector(1900.0f, 0.0f, 70.0f), FVector(-1950.0f, 0.0f, 70.0f),
		FVector(-5050.0f, -3550.0f, 70.0f), FVector(-5050.0f, 3450.0f, 70.0f),
		FVector(5050.0f, -3450.0f, 70.0f), FVector(5050.0f, 3550.0f, 70.0f)
	};
	for (const FVector& Location : BatteryLocations)
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator(0.0f, 90.0f, 0.0f));
	}

	SpawnMapCollectables(TEXT("facility"), {
		FVector(-3600.0f, -2000.0f, 70.0f), FVector(3900.0f, -1900.0f, 70.0f), FVector(-3900.0f, 1700.0f, 70.0f),
		FVector(3650.0f, 1500.0f, 70.0f), FVector(-650.0f, -2750.0f, 70.0f)
	});

	const FVector AlarmLocations[] = {
		FVector(-4100.0f, -1150.0f, 105.0f), FVector(-4050.0f, 1150.0f, 105.0f),
		FVector(-950.0f, -2350.0f, 105.0f), FVector(950.0f, 2350.0f, 105.0f),
		FVector(4100.0f, -1150.0f, 105.0f), FVector(4050.0f, 1150.0f, 105.0f)
	};
	for (const FVector& Location : AlarmLocations)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	const FVector FacilityEastStationGate(5000.0f, 0.0f, 120.0f);
	const FVector FacilityWestStationGate(-5000.0f, 0.0f, 120.0f);
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FacilityEastStationGate, FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FacilityWestStationGate, FRotator(0.0f, -90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	BuildMapSubwayExitStation(FacilityEastStationGate, 1.0f, TEXT("MECHANICS PLATFORM"), GetNextMapAfterStage(RuntimeStageIndex));
	BuildMapSubwayExitStation(FacilityWestStationGate, -1.0f, TEXT("MAINTENANCE PLATFORM"), GetNextMapAfterStage(RuntimeStageIndex));

	const struct FLightSpec { FVector Location; int32 Circuit; FLinearColor Color; float Intensity; } Lights[] = {
		{FVector(-3650.0f, -2850.0f, 275.0f), 1, FLinearColor(0.95f, 0.75f, 0.45f, 1.0f), 1050.0f},
		{FVector(-2200.0f, -3000.0f, 275.0f), 1, FLinearColor(0.95f, 0.75f, 0.45f, 1.0f), 900.0f},
		{FVector(0.0f, -3000.0f, 275.0f), 2, FLinearColor(0.45f, 0.72f, 1.0f, 1.0f), 850.0f},
		{FVector(1800.0f, -3000.0f, 275.0f), 2, FLinearColor(0.45f, 0.72f, 1.0f, 1.0f), 850.0f},
		{FVector(3650.0f, -2850.0f, 275.0f), 3, FLinearColor(0.62f, 0.98f, 1.0f, 1.0f), 1000.0f},
		{FVector(3750.0f, -1500.0f, 275.0f), 3, FLinearColor(0.62f, 0.98f, 1.0f, 1.0f), 900.0f},
		{FVector(-3600.0f, 2850.0f, 275.0f), 4, FLinearColor(0.85f, 0.58f, 0.95f, 1.0f), 900.0f},
		{FVector(-2200.0f, 2850.0f, 275.0f), 4, FLinearColor(0.85f, 0.58f, 0.95f, 1.0f), 850.0f},
		{FVector(0.0f, 3000.0f, 275.0f), 5, FLinearColor(0.55f, 0.9f, 0.7f, 1.0f), 900.0f},
		{FVector(1800.0f, 2800.0f, 275.0f), 5, FLinearColor(0.55f, 0.9f, 0.7f, 1.0f), 850.0f},
		{FVector(3650.0f, 2600.0f, 275.0f), 6, FLinearColor(1.0f, 0.28f, 0.18f, 1.0f), 750.0f},
		{FVector(-900.0f, 0.0f, 275.0f), 0, FLinearColor(0.60f, 0.80f, 1.0f, 1.0f), 650.0f},
		{FVector(900.0f, 0.0f, 275.0f), 0, FLinearColor(0.60f, 0.80f, 1.0f, 1.0f), 650.0f},
		{FVector(3150.0f, 0.0f, 275.0f), 6, FLinearColor(1.0f, 0.22f, 0.16f, 1.0f), 650.0f}
	};

	for (const FLightSpec& Spec : Lights)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Spec.Location, FRotator::ZeroRotator))
		{
			Light->Configure(Spec.Circuit, Spec.Color, Spec.Intensity, 1050.0f);
			FlickerLights.Add(Light);
		}
	}

	const TArray<TPair<FVector, int32>> Switches = {
		{FVector(-4300.0f, -2350.0f, 120.0f), 1},
		{FVector(-200.0f, -3450.0f, 120.0f), 2},
		{FVector(4300.0f, -2350.0f, 120.0f), 3},
		{FVector(-4300.0f, 2350.0f, 120.0f), 4},
		{FVector(0.0f, 3450.0f, 120.0f), 5},
		{FVector(4300.0f, 1800.0f, 120.0f), 6}
	};

	for (const TPair<FVector, int32>& Switch : Switches)
	{
		if (ABHPowerSwitch* PowerSwitch = GetWorld()->SpawnActor<ABHPowerSwitch>(Switch.Key, FRotator::ZeroRotator))
		{
			PowerSwitch->Configure(Switch.Value, FText::FromString(FString::Printf(TEXT("Toggle Circuit %d"), Switch.Value)));
		}
	}

	const TArray<TPair<FVector, FRotator>> Shutters = {
		{FVector(-1500.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(1500.0f, 0.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(0.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(0.0f, 1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}
	};

	for (const TPair<FVector, FRotator>& Shutter : Shutters)
	{
		if (ABHSecurityShutter* SecurityShutter = GetWorld()->SpawnActor<ABHSecurityShutter>(Shutter.Key, Shutter.Value))
		{
			SecurityShutter->Configure(100);
		}
	}

	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-4200.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(100, FText::FromString(TEXT("Toggle Central Shutters")));
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(4200.0f, 1000.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		Terminal->Configure(100, FText::FromString(TEXT("Toggle Central Shutters")));
	}
	GetWorld()->SpawnActor<ABHSecurityMonitor>(FVector(0.0f, -3650.0f, 115.0f), FRotator(0.0f, 90.0f, 0.0f));
	const TArray<TPair<FVector, FRotator>> CameraSpecs = {
		{FVector(-3800.0f, -2650.0f, 330.0f), FRotator(0.0f, 42.0f, 0.0f)},
		{FVector(3800.0f, 2500.0f, 330.0f), FRotator(0.0f, -138.0f, 0.0f)},
		{FVector(0.0f, 3180.0f, 330.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};
	for (int32 CameraIndex = 0; CameraIndex < CameraSpecs.Num(); ++CameraIndex)
	{
		const TPair<FVector, FRotator>& CameraSpec = CameraSpecs[CameraIndex];
		if (ABHSecurityCamera* Camera = GetWorld()->SpawnActor<ABHSecurityCamera>(CameraSpec.Key, CameraSpec.Value))
		{
			Camera->ConfigureCamera(4300.0f, 48.0f, 100);
			SpawnCCTVZoneForCamera(Camera, 100, TEXT("facility CCTV zone"), true);
		}
	}

	SpawnAmbient(FVector(-3800.0f, -3000.0f, 160.0f), 42.0f, 0.18f, 0.04f, 0.16f);
	SpawnAmbient(FVector(3850.0f, -2850.0f, 160.0f), 68.0f, 0.14f, 0.025f, 0.30f);
	SpawnAmbient(FVector(-3600.0f, 2850.0f, 160.0f), 36.0f, 0.11f, 0.03f, 0.20f);
	SpawnAmbient(FVector(3600.0f, 2450.0f, 160.0f), 88.0f, 0.10f, 0.02f, 0.45f);
	SpawnAmbient(FVector(0.0f, -3150.0f, 160.0f), 52.0f, 0.12f, 0.04f, 0.18f);
	SpawnAmbient(FVector(4300.0f, 0.0f, 160.0f), 30.0f, 0.16f, 0.05f, 0.10f);

	ScarePoints.Append({
		FVector(-3950.0f, -2850.0f, 110.0f),
		FVector(3850.0f, -2750.0f, 110.0f),
		FVector(-3750.0f, 2800.0f, 110.0f),
		FVector(3650.0f, 2450.0f, 110.0f),
		FVector(-900.0f, -3300.0f, 110.0f),
		FVector(900.0f, 3300.0f, 110.0f),
		FVector(-4300.0f, 500.0f, 110.0f),
		FVector(4300.0f, -500.0f, 110.0f),
		FVector(0.0f, -1200.0f, 110.0f),
		FVector(0.0f, 1200.0f, 110.0f),
		FVector(-5150.0f, -3600.0f, 110.0f),
		FVector(-5150.0f, 3600.0f, 110.0f),
		FVector(5150.0f, -3600.0f, 110.0f),
		FVector(5150.0f, 3600.0f, 110.0f),
		FVector(-5150.0f, 0.0f, 110.0f),
		FVector(5150.0f, 0.0f, 110.0f)
	});

	// Faint red, ground-hugging mist for atmosphere: the fog is tinted a dim red but kept very thin and
	// strongly floor-concentrated (see the bFacilityMist branch in AddMoodPass), so the red only reads as a
	// light haze pooled on the ground and fades out by standing height rather than washing the room red.
	AddMoodPass(FLinearColor(0.060f, 0.013f, 0.011f, 1.0f), 0.012f, 0.55f, 0.14f);
	AddFacilityVerticalSlicePass();
	AddFacilityDetailPass();
	// AddClassroomHorrorPass(); // disabled: crude blockout classroom (blackboard + red mark + cube desks) looked bad (playtest)
	{
		FRandomStream VariationStream(BHVariationSeedForLevel(RuntimeLevelName, RuntimeStageIndex, RoundSeed));
		AddHorrorVariationPass(VariationStream);
	}
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildFoggroundsLevel()
{
	SurvivorSpawns = {
		FVector(5920.0f, -1780.0f, 120.0f), FVector(6300.0f, -1780.0f, 120.0f), FVector(6680.0f, -1780.0f, 120.0f),
		FVector(5920.0f, -1080.0f, 120.0f), FVector(6300.0f, -1080.0f, 120.0f), FVector(6680.0f, -1080.0f, 120.0f),
		FVector(5920.0f, -360.0f, 120.0f), FVector(6300.0f, -360.0f, 120.0f), FVector(6680.0f, -360.0f, 120.0f),
		FVector(5920.0f, 420.0f, 120.0f), FVector(6300.0f, 420.0f, 120.0f), FVector(6680.0f, 420.0f, 120.0f)
	};
	HunterSpawn = FVector(-7350.0f, 4550.0f, 120.0f);

	const FLinearColor Ground(0.055f, 0.070f, 0.073f, 1.0f);
	const FLinearColor Wall(0.14f, 0.17f, 0.17f, 1.0f);
	const FLinearColor WetMetal(0.10f, 0.13f, 0.14f, 1.0f);
	const FLinearColor Warning(0.72f, 0.52f, 0.13f, 1.0f);
	const FLinearColor ExitGreen(0.05f, 0.78f, 0.42f, 1.0f);
	const FLinearColor FogBlue(0.14f, 0.35f, 0.42f, 1.0f);
	const FLinearColor Trunk(0.070f, 0.050f, 0.032f, 1.0f);
	const FLinearColor Needles(0.025f, 0.115f, 0.070f, 1.0f);
	const FLinearColor DeadNeedles(0.16f, 0.13f, 0.075f, 1.0f);
	const FLinearColor Road(0.028f, 0.034f, 0.035f, 1.0f);
	const FLinearColor Shed(0.12f, 0.16f, 0.16f, 1.0f);
	const FLinearColor Roof(0.09f, 0.10f, 0.105f, 1.0f);
	const FLinearColor Mast(0.42f, 0.46f, 0.46f, 1.0f);
	const FLinearColor DimRed(0.55f, 0.055f, 0.04f, 1.0f);

	const auto LocalPoint = [](const FVector& Origin, const FRotator& Rotation, const FVector& Offset)
	{
		return Origin + Rotation.RotateVector(Offset);
	};
	const auto SpawnShed = [&](const FVector& Origin, const FRotator& Rotation)
	{
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 78.0f)), FVector(4.4f, 2.7f, 1.55f), Shed, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 168.0f)), FVector(4.9f, 3.1f, 0.24f), Roof, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -142.0f, 102.0f)), FVector(1.05f, 0.045f, 1.05f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnWatchPost = [&](const FVector& Origin)
	{
		const FVector Legs[] = {
			FVector(-130.0f, -130.0f, 150.0f), FVector(130.0f, -130.0f, 150.0f),
			FVector(-130.0f, 130.0f, 150.0f), FVector(130.0f, 130.0f, 150.0f)
		};
		for (const FVector& Leg : Legs)
		{
			SpawnBlock(Origin + Leg, FVector(0.16f, 0.16f, 3.0f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 312.0f), FVector(3.2f, 3.2f, 0.22f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 405.0f), FVector(2.5f, 2.1f, 1.25f), Shed, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Origin + FVector(0.0f, 0.0f, 492.0f), FVector(2.9f, 2.5f, 0.22f), Roof, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Origin + FVector(-190.0f, 0.0f, 235.0f), FVector(0.10f, 2.7f, 0.14f), Warning, FRotator(0.0f, 0.0f, 28.0f), false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnRoadBarrier = [&](const FVector& Origin, const FRotator& Rotation)
	{
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 44.0f)), FVector(3.0f, 0.24f, 0.65f), WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-115.0f, 0.0f, 94.0f)), FVector(0.18f, 0.30f, 0.86f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(115.0f, 0.0f, 94.0f)), FVector(0.18f, 0.30f, 0.86f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};
	const auto SpawnCrawlRun = [&](const FVector& Origin, const FRotator& Rotation)
	{
		const FLinearColor MouthShadow(0.018f, 0.023f, 0.023f, 1.0f);
		const float RoofBottomZ = 148.0f;
		const float RoofScaleZ = 0.20f;
		const float RunScaleX = 5.6f;
		const float SideOffsetY = 96.0f;

		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.54f, 0.035f))), FVector(5.2f, 1.85f, 0.035f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -SideOffsetY, CenterZForBlockBottom(0.0f, 1.14f))), FVector(RunScaleX, 0.12f, 1.14f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, SideOffsetY, CenterZForBlockBottom(0.0f, 1.14f))), FVector(RunScaleX, 0.12f, 1.14f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(RoofBottomZ, RoofScaleZ))), FVector(RunScaleX, 2.12f, RoofScaleZ), Shed, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-292.0f, -74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-292.0f, 74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(292.0f, -74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(292.0f, 74.0f, 72.0f)), FVector(0.045f, 0.22f, 1.08f), MouthShadow, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-240.0f, 0.0f, 136.0f)), FVector(0.08f, 1.72f, 0.08f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(240.0f, 0.0f, 136.0f)), FVector(0.08f, 1.72f, 0.08f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		if (ABHCrawlSpaceVolume* CrawlVolume = GetWorld()->SpawnActor<ABHCrawlSpaceVolume>(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 82.0f)), Rotation))
		{
			CrawlVolume->Configure(FVector(RunScaleX * 50.0f + 45.0f, 118.0f, 118.0f));
		}
	};
	const auto SpawnDoorGateFrame = [&](const FVector& Origin, const FRotator& Rotation, bool bLiftGate)
	{
		const FVector FrameOrigin = ResolveFoggroundsDoorFrameOrigin(Origin);
		const FVector PostScale(0.18f, 0.12f, bLiftGate ? 3.18f : 2.58f);
		SpawnBlock(LocalPoint(FrameOrigin, Rotation, FVector(0.0f, -98.0f, CenterZForBlockBottom(0.0f, PostScale.Z))), PostScale, WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(FrameOrigin, Rotation, FVector(0.0f, 98.0f, CenterZForBlockBottom(0.0f, PostScale.Z))), PostScale, WetMetal, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(FrameOrigin, Rotation, FVector(0.0f, 0.0f, bLiftGate ? 325.0f : 264.0f)), FVector(0.20f, 2.35f, 0.14f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
		if (bLiftGate)
		{
			SpawnBlock(LocalPoint(FrameOrigin, Rotation, FVector(18.0f, 0.0f, 282.0f)), FVector(0.08f, 2.15f, 0.08f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		}
		SpawnBlock(LocalPoint(FrameOrigin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.50f, 0.035f))), FVector(0.32f, 2.15f, 0.035f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
	};
	const auto SpawnHiderCulvert = [&](const FVector& Origin, const FRotator& Rotation, float LengthScale)
	{
		const float HalfLength = LengthScale * 50.0f;
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.52f, 0.035f))), FVector(LengthScale, 1.70f, 0.035f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -86.0f, CenterZForBlockBottom(0.0f, 1.10f))), FVector(LengthScale, 0.12f, 1.10f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 86.0f, CenterZForBlockBottom(0.0f, 1.10f))), FVector(LengthScale, 0.12f, 1.10f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(142.0f, 0.22f))), FVector(LengthScale, 1.95f, 0.22f), Shed, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-HalfLength * 0.22f, 0.0f, 176.0f)), FVector(LengthScale * 0.34f, 2.25f, 0.24f), Ground, Rotation, true, EBHBlockMaterial::Concrete);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(HalfLength * 0.28f, 0.0f, 182.0f)), FVector(LengthScale * 0.30f, 2.10f, 0.22f), Ground, Rotation, true, EBHBlockMaterial::Concrete);
		if (ABHCrawlSpaceVolume* CrawlVolume = GetWorld()->SpawnActor<ABHCrawlSpaceVolume>(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, 80.0f)), Rotation))
		{
			CrawlVolume->Configure(FVector(HalfLength + 45.0f, 108.0f, 116.0f));
		}
	};
	const auto SpawnSecretRoom = [&](const FVector& Origin, const FRotator& Rotation)
	{
		const FLinearColor RoomWall(0.060f, 0.082f, 0.078f, 1.0f);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(0.58f, 0.040f))), FVector(6.5f, 5.1f, 0.040f), Road, Rotation, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(325.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(0.20f, 5.1f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, -255.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(6.7f, 0.20f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-292.5f, 255.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(0.85f, 0.20f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(167.5f, 255.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(3.35f, 0.20f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-125.0f, 255.0f, CenterZForBlockBottom(148.0f, 1.27f))), FVector(2.50f, 0.20f, 1.27f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-325.0f, -190.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(0.20f, 1.3f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-325.0f, 190.0f, CenterZForBlockBottom(0.0f, 2.75f))), FVector(0.20f, 1.3f, 2.75f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(-325.0f, 0.0f, CenterZForBlockBottom(148.0f, 1.27f))), FVector(0.20f, 2.25f, 1.27f), RoomWall, Rotation, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(0.0f, 0.0f, CenterZForBlockBottom(276.0f, 0.18f))), FVector(6.9f, 5.3f, 0.18f), Roof, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(140.0f, -120.0f, 62.0f)), FVector(1.6f, 0.50f, 1.00f), WetMetal, Rotation, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(LocalPoint(Origin, Rotation, FVector(95.0f, 135.0f, 52.0f)), FVector(1.0f, 0.85f, 0.80f), Warning, Rotation, false, EBHBlockMaterial::WarningSign);
	};

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.28f)), FVector(158.0f, 124.0f, 0.28f), Ground, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	SpawnBlock(FVector(0.0f, -6200.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(158.0f, 0.18f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 6200.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(158.0f, 0.18f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-7900.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(0.18f, 124.0f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(7900.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.4f)), FVector(0.18f, 124.0f, 2.4f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	AddMapContainment(7900.0f, 6200.0f);

	SpawnBlock(FVector(5600.0f, -4850.0f, CenterZForBlockBottom(0.62f, 0.035f)), FVector(40.0f, 10.5f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, -4050.0f, CenterZForBlockBottom(0.58f, 0.035f)), FVector(118.0f, 7.0f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(-5750.0f, 4050.0f, CenterZForBlockBottom(0.52f, 0.035f)), FVector(32.0f, 8.0f, 0.035f), Road, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		SpawnRoadBarrier(FVector(5000.0f + Index * 340.0f, -5650.0f, 0.0f), FRotator::ZeroRotator);
	}
	SpawnShed(FVector(-5200.0f, -5050.0f, 0.0f), FRotator(0.0f, 8.0f, 0.0f));
	SpawnShed(FVector(4100.0f, 3050.0f, 0.0f), FRotator(0.0f, 92.0f, 0.0f));
	SpawnShed(FVector(-6150.0f, 2250.0f, 0.0f), FRotator(0.0f, -8.0f, 0.0f));
	SpawnWatchPost(FVector(6900.0f, -1200.0f, 0.0f));
	SpawnWatchPost(FVector(-6950.0f, 3550.0f, 0.0f));
	SpawnBlock(FVector(-980.0f, 5050.0f, 340.0f), FVector(0.20f, 0.20f, 6.8f), Mast, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 830.0f), FVector(0.12f, 0.12f, 3.0f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 610.0f), FVector(2.3f, 0.08f, 0.08f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 760.0f), FVector(0.08f, 2.6f, 0.08f), Mast, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-980.0f, 5050.0f, 1010.0f), FVector(0.34f, 0.34f, 0.34f), DimRed, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-5200.0f, -4580.0f, 78.0f), FVector(3.5f, 1.25f, 1.25f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(-4820.0f, -4580.0f, 78.0f), FVector(2.5f, 1.05f, 1.05f), WetMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(-4430.0f, -4580.0f, 120.0f), FVector(0.26f, 2.8f, 1.65f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(7250.0f, -3150.0f, 125.0f), FVector(0.05f, 2.4f, 0.78f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(-7300.0f, 4700.0f, 125.0f), FVector(0.05f, 2.4f, 0.78f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);

	const TArray<TPair<FVector, FVector>> MazeWalls = {
		{FVector(-5400.0f, -3600.0f, 190.0f), FVector(30.0f, 0.28f, 3.55f)}, {FVector(-2100.0f, -3600.0f, 190.0f), FVector(18.0f, 0.28f, 3.55f)}, {FVector(2500.0f, -3600.0f, 190.0f), FVector(44.0f, 0.28f, 3.55f)},
		{FVector(-4700.0f, -1200.0f, 190.0f), FVector(34.0f, 0.28f, 3.55f)}, {FVector(-700.0f, -1200.0f, 190.0f), FVector(26.0f, 0.28f, 3.55f)}, {FVector(4300.0f, -1200.0f, 190.0f), FVector(32.0f, 0.28f, 3.55f)},
		{FVector(-3900.0f, 1450.0f, 190.0f), FVector(48.0f, 0.28f, 3.55f)}, {FVector(900.0f, 1450.0f, 190.0f), FVector(26.0f, 0.28f, 3.55f)}, {FVector(5200.0f, 1450.0f, 190.0f), FVector(24.0f, 0.28f, 3.55f)},
		{FVector(-5100.0f, 3950.0f, 190.0f), FVector(36.0f, 0.28f, 3.55f)}, {FVector(-900.0f, 3950.0f, 190.0f), FVector(32.0f, 0.28f, 3.55f)}, {FVector(3950.0f, 3950.0f, 190.0f), FVector(36.0f, 0.28f, 3.55f)},
		{FVector(-6100.0f, -2300.0f, 190.0f), FVector(0.28f, 34.0f, 3.55f)}, {FVector(-6100.0f, 2600.0f, 190.0f), FVector(0.28f, 36.0f, 3.55f)},
		{FVector(-3100.0f, -5200.0f, 190.0f), FVector(0.28f, 20.0f, 3.55f)}, {FVector(-3100.0f, 900.0f, 190.0f), FVector(0.28f, 42.0f, 3.55f)},
		{FVector(300.0f, -2400.0f, 190.0f), FVector(0.28f, 32.0f, 3.55f)}, {FVector(300.0f, 3300.0f, 190.0f), FVector(0.28f, 28.0f, 3.55f)},
		{FVector(3500.0f, -5200.0f, 190.0f), FVector(0.28f, 22.0f, 3.55f)}, {FVector(3500.0f, -200.0f, 190.0f), FVector(0.28f, 38.0f, 3.55f)},
		{FVector(6200.0f, -2500.0f, 190.0f), FVector(0.28f, 34.0f, 3.55f)}, {FVector(6200.0f, 2850.0f, 190.0f), FVector(0.28f, 32.0f, 3.55f)},
		{FVector(-7100.0f, -650.0f, 190.0f), FVector(0.28f, 19.0f, 3.30f)}, {FVector(-4850.0f, 650.0f, 190.0f), FVector(25.0f, 0.28f, 3.25f)},
		{FVector(-2350.0f, -2450.0f, 190.0f), FVector(0.28f, 18.0f, 3.25f)}, {FVector(-1400.0f, -2750.0f, 190.0f), FVector(24.0f, 0.28f, 3.25f)},
		{FVector(1900.0f, 2400.0f, 190.0f), FVector(0.28f, 22.0f, 3.25f)}, {FVector(2950.0f, 2500.0f, 190.0f), FVector(23.0f, 0.28f, 3.25f)},
		{FVector(5150.0f, -4550.0f, 190.0f), FVector(0.28f, 14.0f, 3.15f)}, {FVector(6650.0f, 2050.0f, 190.0f), FVector(18.0f, 0.28f, 3.20f)}
	};
	for (const TPair<FVector, FVector>& WallSpec : MazeWalls)
	{
		const FVector FenceScale(WallSpec.Value.X, WallSpec.Value.Y, 2.65f);

		const auto SpawnFenceSegment = [&](const FVector& SegmentLocation, const FVector& SegmentScale)
		{
			SpawnBlock(FVector(SegmentLocation.X, SegmentLocation.Y, CenterZForBlockBottom(0.0f, SegmentScale.Z)), SegmentScale, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(SegmentLocation.X, SegmentLocation.Y, 164.0f), FVector(SegmentScale.X, SegmentScale.Y, 0.045f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		};

		struct FWallCutSpec
		{
			FVector WallLocation;
			float OffsetAlongWall;
			float HalfWidth;
			bool bCrawl;
		};
		const FWallCutSpec WallCuts[] = {
			{FVector(2500.0f, -3600.0f, 190.0f), -700.0f, 132.0f, true},
			{FVector(-4700.0f, -1200.0f, 190.0f), 350.0f, 132.0f, true},
			{FVector(-3900.0f, 1450.0f, 190.0f), -650.0f, 132.0f, true},
			{FVector(3950.0f, 3950.0f, 190.0f), -250.0f, 132.0f, true},
			{FVector(-6100.0f, 2600.0f, 190.0f), -350.0f, 132.0f, true},
			{FVector(-3100.0f, 900.0f, 190.0f), -400.0f, 132.0f, true},
			{FVector(3500.0f, -200.0f, 190.0f), 350.0f, 132.0f, true},
			{FVector(6200.0f, 2850.0f, 190.0f), -150.0f, 132.0f, true},
			{FVector(-6100.0f, -2300.0f, 190.0f), 1200.0f, 118.0f, false},
			{FVector(-700.0f, -1200.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(2500.0f, -3600.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(300.0f, -2400.0f, 190.0f), 1200.0f, 118.0f, false},
			{FVector(6200.0f, -2500.0f, 190.0f), 1800.0f, 118.0f, false},
			{FVector(-3100.0f, 900.0f, 190.0f), 1000.0f, 118.0f, false},
			{FVector(900.0f, 1450.0f, 190.0f), -600.0f, 118.0f, false},
			{FVector(-6100.0f, 2600.0f, 190.0f), 1625.0f, 118.0f, false},
			{FVector(6200.0f, 2850.0f, 190.0f), 1430.0f, 118.0f, false}
		};

		TArray<FWallCutSpec> MatchingCuts;
		for (const FWallCutSpec& Cut : WallCuts)
		{
			if (WallSpec.Key.Equals(Cut.WallLocation, 0.1f))
			{
				MatchingCuts.Add(Cut);
			}
		}

		if (MatchingCuts.IsEmpty())
		{
			SpawnFenceSegment(WallSpec.Key, FenceScale);
			continue;
		}

		const bool bWallRunsAlongX = FenceScale.X >= FenceScale.Y;
		const float LongScale = bWallRunsAlongX ? FenceScale.X : FenceScale.Y;
		const float HalfSpan = LongScale * 50.0f;

		const auto SpawnFenceSpan = [&](float SpanStart, float SpanEnd)
		{
			const float SpanLength = SpanEnd - SpanStart;
			if (SpanLength < 70.0f)
			{
				return;
			}

			FVector SegmentLocation = WallSpec.Key;
			FVector SegmentScale = FenceScale;
			const float SegmentOffset = (SpanStart + SpanEnd) * 0.5f;
			if (bWallRunsAlongX)
			{
				SegmentLocation.X += SegmentOffset;
				SegmentScale.X = SpanLength / 100.0f;
			}
			else
			{
				SegmentLocation.Y += SegmentOffset;
				SegmentScale.Y = SpanLength / 100.0f;
			}
			SpawnFenceSegment(SegmentLocation, SegmentScale);
		};

		MatchingCuts.Sort([](const FWallCutSpec& Left, const FWallCutSpec& Right)
		{
			return Left.OffsetAlongWall < Right.OffsetAlongWall;
		});

		float SpanCursor = -HalfSpan;
		for (const FWallCutSpec& Cut : MatchingCuts)
		{
			const float CutHalfWidth = FMath::Max(90.0f, Cut.HalfWidth);
			const float CutOffset = FMath::Clamp(Cut.OffsetAlongWall, -HalfSpan + CutHalfWidth + 45.0f, HalfSpan - CutHalfWidth - 45.0f);
			const float CutStart = FMath::Max(-HalfSpan, CutOffset - CutHalfWidth);
			const float CutEnd = FMath::Min(HalfSpan, CutOffset + CutHalfWidth);
			if (CutStart > SpanCursor)
			{
				SpawnFenceSpan(SpanCursor, CutStart);
			}

			FVector CutLocation(WallSpec.Key.X, WallSpec.Key.Y, 0.0f);
			if (bWallRunsAlongX)
			{
				CutLocation.X += CutOffset;
			}
			else
			{
				CutLocation.Y += CutOffset;
			}

			if (Cut.bCrawl)
			{
				const float LintelBottomZ = 148.0f;
				const float LintelScaleZ = FMath::Max(0.1f, (FenceScale.Z * 100.0f - LintelBottomZ) / 100.0f);
				const FVector LintelLocation(CutLocation.X, CutLocation.Y, CenterZForBlockBottom(LintelBottomZ, LintelScaleZ));
				const FVector LintelScale = bWallRunsAlongX
					? FVector((CutHalfWidth * 2.0f) / 100.0f, FenceScale.Y, LintelScaleZ)
					: FVector(FenceScale.X, (CutHalfWidth * 2.0f) / 100.0f, LintelScaleZ);
				SpawnBlock(LintelLocation, LintelScale, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
				SpawnBlock(FVector(CutLocation.X, CutLocation.Y, 164.0f), FVector(LintelScale.X, LintelScale.Y, 0.045f), Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
				SpawnCrawlRun(CutLocation, bWallRunsAlongX ? FRotator(0.0f, 90.0f, 0.0f) : FRotator::ZeroRotator);
			}

			SpanCursor = FMath::Max(SpanCursor, CutEnd);
		}
		SpawnFenceSpan(SpanCursor, HalfSpan);
	}

	const auto SpawnSecretRoomWithCulvert = [&](const FVector& Origin, const FRotator& Rotation)
	{
		SpawnSecretRoom(Origin, Rotation);
		SpawnHiderCulvert(LocalPoint(Origin, Rotation, FVector(-650.0f, 0.0f, 0.0f)), Rotation, 6.5f);
		SpawnHiderCulvert(LocalPoint(Origin, Rotation, FVector(-125.0f, 580.0f, 0.0f)), Rotation + FRotator(0.0f, 90.0f, 0.0f), 6.5f);
	};
	SpawnSecretRoomWithCulvert(FVector(-6900.0f, -1700.0f, 0.0f), FRotator::ZeroRotator);
	SpawnSecretRoomWithCulvert(FVector(5450.0f, 4750.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
	SpawnSecretRoomWithCulvert(FVector(-1250.0f, 5250.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f));
	SpawnHiderCulvert(FVector(4700.0f, -4100.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), 7.2f);
	SpawnHiderCulvert(FVector(-3300.0f, 3250.0f, 0.0f), FRotator(0.0f, -12.0f, 0.0f), 6.4f);

	struct FFoggroundsScreenSpec
	{
		FVector Location;
		FVector Scale;
		FRotator Rotation;
		FLinearColor Tint;
		EBHBlockMaterial Material;
	};

	const FLinearColor DarkScreen(0.075f, 0.105f, 0.105f, 1.0f);
	const FLinearColor BrushScreen(0.045f, 0.105f, 0.070f, 1.0f);
	const FFoggroundsScreenSpec SightScreens[] = {
		{FVector(-6900.0f, -2700.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(7.8f, 0.38f, 2.85f), FRotator(0.0f, -12.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-4700.0f, -4725.0f, CenterZForBlockBottom(0.0f, 2.95f)), FVector(0.42f, 11.0f, 2.95f), FRotator(0.0f, 8.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(-2550.0f, -2920.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(12.0f, 0.38f, 2.80f), FRotator(0.0f, 14.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-900.0f, -4875.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(0.42f, 10.2f, 2.90f), FRotator(0.0f, -10.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(1400.0f, -3250.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(12.6f, 0.40f, 2.80f), FRotator(0.0f, -18.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(4800.0f, -3025.0f, CenterZForBlockBottom(0.0f, 2.95f)), FVector(0.42f, 12.0f, 2.95f), FRotator(0.0f, 6.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(6650.0f, -1850.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(8.0f, 0.36f, 2.75f), FRotator(0.0f, 18.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-5550.0f, 525.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(0.42f, 12.4f, 2.90f), FRotator(0.0f, 4.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-3600.0f, 2675.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(11.5f, 0.38f, 2.80f), FRotator(0.0f, -12.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-1350.0f, 4000.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(9.5f, 0.40f, 2.90f), FRotator(0.0f, 16.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(1750.0f, 1180.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 11.6f, 2.85f), FRotator(0.0f, -7.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(4550.0f, 2675.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(10.8f, 0.38f, 2.80f), FRotator(0.0f, 10.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(6350.0f, 4300.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 8.6f, 2.85f), FRotator(0.0f, -15.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(3200.0f, 4725.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(8.6f, 0.36f, 2.75f), FRotator(0.0f, -8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-7050.0f, 1925.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(7.0f, 0.38f, 2.85f), FRotator(0.0f, 22.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(7200.0f, 2550.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 8.4f, 2.85f), FRotator(0.0f, 8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-800.0f, -100.0f, CenterZForBlockBottom(0.0f, 2.60f)), FVector(9.0f, 0.36f, 2.60f), FRotator(0.0f, 24.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(3000.0f, -50.0f, CenterZForBlockBottom(0.0f, 2.60f)), FVector(0.38f, 8.8f, 2.60f), FRotator(0.0f, -18.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		// ---- Extra sightline breakers: this is the running map and it was reading too open, so a denser scatter of
		// freestanding angled screens (NOT maze walls) cuts the long cross-yard diagonals -> the space feels bigger
		// and more dangerous while every cell still has multiple ways through, preserving the chase flow. Kept clear
		// of the secret rooms / culvert mouths so no crawl route gets sealed. ----
		{FVector(-2000.0f, -1500.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(8.5f, 0.40f, 2.70f), FRotator(0.0f, -22.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(2200.0f, -1900.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(0.42f, 9.0f, 2.70f), FRotator(0.0f, 12.0f, 0.0f), BrushScreen, EBHBlockMaterial::RustedMetal},
		{FVector(4800.0f, -1100.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(7.5f, 0.38f, 2.70f), FRotator(0.0f, 16.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-4400.0f, -2600.0f, CenterZForBlockBottom(0.0f, 2.85f)), FVector(0.42f, 8.0f, 2.85f), FRotator(0.0f, -8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-1300.0f, -3100.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(7.0f, 0.40f, 2.75f), FRotator(0.0f, 28.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(3400.0f, -2400.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(0.40f, 7.5f, 2.80f), FRotator(0.0f, -14.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-3000.0f, 300.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(8.0f, 0.38f, 2.70f), FRotator(0.0f, 18.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(1200.0f, -400.0f, CenterZForBlockBottom(0.0f, 2.65f)), FVector(0.40f, 7.0f, 2.65f), FRotator(0.0f, -24.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(5400.0f, 900.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(6.5f, 0.40f, 2.70f), FRotator(0.0f, 10.0f, 0.0f), DarkScreen, EBHBlockMaterial::RustedMetal},
		{FVector(-5800.0f, -3300.0f, CenterZForBlockBottom(0.0f, 2.90f)), FVector(0.42f, 7.0f, 2.90f), FRotator(0.0f, 6.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(0.0f, 2200.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(9.0f, 0.40f, 2.75f), FRotator(0.0f, -16.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(3000.0f, 2000.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(0.40f, 8.0f, 2.70f), FRotator(0.0f, 14.0f, 0.0f), BrushScreen, EBHBlockMaterial::RustedMetal},
		{FVector(-2100.0f, 3800.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(7.5f, 0.38f, 2.70f), FRotator(0.0f, -10.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(5200.0f, 3400.0f, CenterZForBlockBottom(0.0f, 2.80f)), FVector(0.42f, 7.0f, 2.80f), FRotator(0.0f, 8.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(-4200.0f, 4600.0f, CenterZForBlockBottom(0.0f, 2.75f)), FVector(7.0f, 0.40f, 2.75f), FRotator(0.0f, 20.0f, 0.0f), DarkScreen, EBHBlockMaterial::PaintedMetal},
		{FVector(1600.0f, 4200.0f, CenterZForBlockBottom(0.0f, 2.70f)), FVector(0.40f, 6.5f, 2.70f), FRotator(0.0f, -18.0f, 0.0f), BrushScreen, EBHBlockMaterial::RustedMetal}
	};
	for (const FFoggroundsScreenSpec& Screen : SightScreens)
	{
		SpawnBlock(Screen.Location, Screen.Scale, Screen.Tint, Screen.Rotation, true, Screen.Material);
	}

	struct FFoggroundsGateSpec
	{
		FVector Location;
		FRotator Rotation;
		bool bLiftGate;
	};
	const FFoggroundsGateSpec DoorSpecs[] = {
		{FVector(-6100.0f, -1100.0f, 120.0f), FRotator::ZeroRotator, false},
		{FVector(-3100.0f, -3600.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), true},
		{FVector(300.0f, -1200.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), false},
		{FVector(3500.0f, -3600.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), true},
		{FVector(6200.0f, -700.0f, 120.0f), FRotator::ZeroRotator, false},
		{FVector(-3100.0f, 1900.0f, 120.0f), FRotator::ZeroRotator, true},
		{FVector(300.0f, 1450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), false},
		{FVector(3500.0f, 1450.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), true},
		{FVector(-6100.0f, 4230.0f, 120.0f), FRotator::ZeroRotator, false},
		{FVector(6200.0f, 4280.0f, 120.0f), FRotator::ZeroRotator, true}
	};
	for (const FFoggroundsGateSpec& DoorSpec : DoorSpecs)
	{
		SpawnDoorGateFrame(DoorSpec.Location, DoorSpec.Rotation, DoorSpec.bLiftGate);
		if (DoorSpec.bLiftGate)
		{
			GetWorld()->SpawnActor<ABHSlidingGate>(DoorSpec.Location, DoorSpec.Rotation);
			continue;
		}

		if (ABHDoor* Door = GetWorld()->SpawnActor<ABHDoor>(DoorSpec.Location, DoorSpec.Rotation))
		{
			DoorActors.Add(Door);
		}
	}

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-7000.0f, -5200.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-4500.0f, -5000.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(-800.0f, -5050.0f, 80.0f), FRotator::ZeroRotator}, {FVector(2750.0f, -5100.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(7100.0f, -4200.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}, {FVector(-7100.0f, 4200.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-500.0f, 5200.0f, 80.0f), FRotator::ZeroRotator}, {FVector(6900.0f, 5000.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-6900.0f, -800.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(-5200.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-2250.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-1100.0f, 2500.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1100.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(2600.0f, 2700.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(5200.0f, -2500.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(7050.0f, 800.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(4550.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(0.0f, 0.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-7050.0f, -4100.0f, 95.0f), EBHObjectiveStationType::Antenna}, {FVector(6900.0f, -3500.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-3300.0f, 4550.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(6550.0f, 4250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-7350.0f, -5400.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-6000.0f, -5400.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3600.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(-900.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1800.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(4500.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(7200.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(-7350.0f, -2600.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-5200.0f, -2400.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-3300.0f, -1000.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-900.0f, -1000.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(1600.0f, -1000.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(4300.0f, -900.0f, 95.0f), EBHObjectiveStationType::Valve}, {FVector(7200.0f, -1700.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-7350.0f, 1700.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(-5200.0f, 2700.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-2500.0f, 1300.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-800.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(1600.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Evidence}, {FVector(3300.0f, 1300.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(5200.0f, 250.0f, 95.0f), EBHObjectiveStationType::Antenna}, {FVector(7200.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-7350.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(-3600.0f, 5200.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(7200.0f, 2600.0f, 95.0f), EBHObjectiveStationType::Antenna}, {FVector(5200.0f, -5200.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(0.0f, -3600.0f, 95.0f), EBHObjectiveStationType::Terminal}, {FVector(0.0f, 3600.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3000.0f, -3600.0f, 95.0f), EBHObjectiveStationType::Antenna}, {FVector(-3000.0f, 3000.0f, 95.0f), EBHObjectiveStationType::Valve}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	for (int32 Index = 0; Index < 24; ++Index)
	{
		const float X = -6500.0f + (Index % 6) * 2600.0f;
		const float Y = (Index < 9 ? -1.0f : 1.0f) * (2100.0f + (Index % 3) * 1250.0f);
		GetWorld()->SpawnActor<ABHLocker>(FVector(X, Y, 100.0f), FRotator::ZeroRotator);
	}

	for (const FVector& Location : {FVector(-7200.0f, -3100.0f, 70.0f), FVector(-3500.0f, -5200.0f, 70.0f), FVector(1200.0f, -5200.0f, 70.0f), FVector(5200.0f, -4800.0f, 70.0f), FVector(-6300.0f, 3200.0f, 70.0f), FVector(-1500.0f, 5200.0f, 70.0f), FVector(3100.0f, 5200.0f, 70.0f), FVector(7200.0f, 3200.0f, 70.0f)})
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator::ZeroRotator);
	}

	SpawnMapCollectables(TEXT("foggrounds"), {
		FVector(-7200.0f, -3100.0f, 70.0f), FVector(-3500.0f, -5200.0f, 70.0f), FVector(1200.0f, -5200.0f, 70.0f),
		FVector(5200.0f, -4800.0f, 70.0f), FVector(-6300.0f, 3200.0f, 70.0f)
	});

	const FVector FoggroundAlarms[] = {
		FVector(7100.0f, -3100.0f, 105.0f), FVector(5200.0f, -5050.0f, 105.0f),
		FVector(2650.0f, -3650.0f, 105.0f), FVector(-700.0f, -1200.0f, 105.0f),
		FVector(-6200.0f, -800.0f, 105.0f), FVector(-6200.0f, 4050.0f, 105.0f),
		FVector(3600.0f, 3950.0f, 105.0f), FVector(7050.0f, 800.0f, 105.0f)
	};
	for (const FVector& Location : FoggroundAlarms)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(7440.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(FVector(-7440.0f, 0.0f, 120.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	if (IsFinalStage())
	{
		BuildFoggroundsFinalStation();
	}
	const float ExitFloorZ = CenterZForBlockBottom(0.72f, 0.045f);
	SpawnBlock(FVector(7100.0f, 0.0f, ExitFloorZ), FVector(8.4f, 2.8f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-7100.0f, 0.0f, ExitFloorZ), FVector(8.4f, 2.8f, 0.045f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);

	struct FFogLightSpec
	{
		FVector Location;
		int32 Circuit;
		FLinearColor Color;
		float Intensity;
	};
	const FFogLightSpec Lights[] = {
		{FVector(-6200.0f, -4600.0f, 290.0f), 1, FogBlue, 780.0f}, {FVector(-2600.0f, -4700.0f, 290.0f), 1, FogBlue, 730.0f},
		{FVector(1200.0f, -4700.0f, 290.0f), 2, FLinearColor(0.48f, 0.72f, 0.76f, 1.0f), 720.0f}, {FVector(5200.0f, -4300.0f, 290.0f), 2, FLinearColor(0.48f, 0.72f, 0.76f, 1.0f), 720.0f},
		{FVector(-6500.0f, 0.0f, 290.0f), 3, FLinearColor(0.65f, 0.82f, 0.70f, 1.0f), 760.0f}, {FVector(0.0f, 0.0f, 290.0f), 0, FLinearColor(0.56f, 0.65f, 0.70f, 1.0f), 600.0f},
		{FVector(6500.0f, 0.0f, 290.0f), 4, FLinearColor(0.65f, 0.82f, 0.70f, 1.0f), 760.0f}, {FVector(-5200.0f, 4300.0f, 290.0f), 5, FLinearColor(0.84f, 0.64f, 0.45f, 1.0f), 690.0f},
		{FVector(-1200.0f, 4700.0f, 290.0f), 5, FLinearColor(0.84f, 0.64f, 0.45f, 1.0f), 690.0f}, {FVector(4200.0f, 4550.0f, 290.0f), 6, FLinearColor(0.90f, 0.48f, 0.36f, 1.0f), 660.0f}
	};
	for (const FFogLightSpec& Spec : Lights)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Spec.Location, FRotator::ZeroRotator))
		{
			Light->Configure(Spec.Circuit, Spec.Color, Spec.Intensity, 1100.0f);
			FlickerLights.Add(Light);
		}
	}

	struct FFoggroundsScareSwitchSpec
	{
		FVector Location;
		int32 Severity;
		const TCHAR* Title;
		float CooldownSeconds;
	};

	const FFoggroundsScareSwitchSpec FoggroundScareSwitches[] = {
		{FVector(-7000.0f, -4100.0f, 120.0f), 1, TEXT("Shadow Flicker"), 45.0f},
		{FVector(2500.0f, -5300.0f, 120.0f), 2, TEXT("Static Face"), 60.0f},
		{FVector(-7050.0f, 900.0f, 120.0f), 3, TEXT("Hallway Lunge"), 75.0f},
		{FVector(7050.0f, -900.0f, 120.0f), 4, TEXT("Blackout Charge"), 90.0f},
		{FVector(-1800.0f, 5250.0f, 120.0f), 2, TEXT("Breath Behind"), 60.0f},
		{FVector(5200.0f, 5000.0f, 120.0f), 3, TEXT("Glass Face"), 75.0f}
	};
	for (const FFoggroundsScareSwitchSpec& Switch : FoggroundScareSwitches)
	{
		if (ABHStudentScareSwitch* ScareSwitch = GetWorld()->SpawnActor<ABHStudentScareSwitch>(Switch.Location, FRotator::ZeroRotator))
		{
			ScareSwitch->Configure(Switch.Severity, Switch.Title, Switch.CooldownSeconds);
		}
	}

	if (ABHSecurityShutter* Shutter = GetWorld()->SpawnActor<ABHSecurityShutter>(FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator))
	{
		Shutter->Configure(300);
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-7000.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(300, FText::FromString(TEXT("Toggle Fogground Shutter")));
	}
	GetWorld()->SpawnActor<ABHSecurityMonitor>(FVector(-6500.0f, -800.0f, 115.0f), FRotator(0.0f, 90.0f, 0.0f));
	const TArray<TPair<FVector, FRotator>> FoggroundCameras = {
		{FVector(-5200.0f, 2400.0f, 360.0f), FRotator(0.0f, -35.0f, 0.0f)},
		{FVector(5200.0f, -2400.0f, 360.0f), FRotator(0.0f, 145.0f, 0.0f)}
	};
	for (int32 CameraIndex = 0; CameraIndex < FoggroundCameras.Num(); ++CameraIndex)
	{
		const TPair<FVector, FRotator>& CameraSpec = FoggroundCameras[CameraIndex];
		if (ABHSecurityCamera* Camera = GetWorld()->SpawnActor<ABHSecurityCamera>(CameraSpec.Key, CameraSpec.Value))
		{
			Camera->ConfigureCamera(5200.0f, 52.0f, 300);
			SpawnCCTVZoneForCamera(Camera, 300, TEXT("foggrounds CCTV zone"), true);
		}
	}

	AddIndustrialClutter({FVector(-5100.0f, -5050.0f, 55.0f), FVector(-700.0f, -4700.0f, 55.0f), FVector(4300.0f, -4900.0f, 55.0f), FVector(-6400.0f, 2100.0f, 55.0f), FVector(-1800.0f, 3200.0f, 55.0f), FVector(3600.0f, 3000.0f, 55.0f), FVector(6600.0f, 2200.0f, 55.0f)}, WetMetal);
	AddIndustrialClutter({FVector(-7350.0f, -5550.0f, 55.0f), FVector(7350.0f, -5550.0f, 55.0f), FVector(-7350.0f, 5550.0f, 55.0f), FVector(7350.0f, 5550.0f, 55.0f)}, WetMetal);
	AddSurfaceDetailGrid(7600.0f, 5900.0f, FLinearColor(0.08f, 0.12f, 0.13f, 1.0f));

	// Random ground clutter (trees/boulders) is scattered below. It must steer clear of every crawl
	// passage, or a collidable trunk/boulder can spawn on a crawl mouth and seal a route the player is
	// meant to slip through. Gather a keep-out footprint from every crawl volume that already exists in
	// the world — this covers the maze-wall crawl runs, the secret-room/standalone culverts, AND the
	// final-station crawl tubes built in BuildFoggroundsFinalStation() above (which live in a separate
	// function and so cannot be tracked through this builder's local state). Each volume's world XY
	// bounds are padded so a boulder's footprint, not just its centre, stays clear of the opening.
	const float CrawlKeepoutPadding = 140.0f; // widest scattered boulder is ~73 half-width; pad beyond it.
	TArray<FBox2D> CrawlKeepoutBounds;
	for (TActorIterator<ABHCrawlSpaceVolume> It(GetWorld()); It; ++It)
	{
		const FBox WorldBounds = It->GetComponentsBoundingBox(true);
		if (!WorldBounds.IsValid)
		{
			continue;
		}
		CrawlKeepoutBounds.Emplace(
			FVector2D(WorldBounds.Min.X - CrawlKeepoutPadding, WorldBounds.Min.Y - CrawlKeepoutPadding),
			FVector2D(WorldBounds.Max.X + CrawlKeepoutPadding, WorldBounds.Max.Y + CrawlKeepoutPadding));
	}

	const auto HitsCrawlKeepout = [&CrawlKeepoutBounds](float X, float Y)
	{
		const FVector2D Point(X, Y);
		for (const FBox2D& Bounds : CrawlKeepoutBounds)
		{
			if (Bounds.IsInside(Point))
			{
				return true;
			}
		}
		return false;
	};

	FRandomStream NatureStream(73421);
	for (int32 Index = 0; Index < 62; ++Index)
	{
		const float X = NatureStream.FRandRange(-7350.0f, 7350.0f);
		const float Y = NatureStream.FRandRange(-5650.0f, 5650.0f);
		if ((X > 5100.0f && FMath::Abs(Y) < 1600.0f) || FMath::Abs(Y + 3600.0f) < 460.0f || FMath::Abs(Y - 3950.0f) < 360.0f || HitsCrawlKeepout(X, Y))
		{
			continue;
		}

		const float Scale = NatureStream.FRandRange(0.82f, 1.22f);
		SpawnBlock(FVector(X, Y, 88.0f * Scale), FVector(0.16f, 0.16f, 1.75f * Scale), Trunk, FRotator(0.0f, Index * 17.0f, 0.0f), true);
		SpawnBlock(FVector(X, Y, 220.0f * Scale), FVector(0.90f * Scale, 0.90f * Scale, 0.70f * Scale), (Index % 6 == 0) ? DeadNeedles : Needles, FRotator(0.0f, Index * 29.0f, 0.0f), false);
		SpawnBlock(FVector(X, Y, 294.0f * Scale), FVector(0.58f * Scale, 0.58f * Scale, 0.48f * Scale), Needles, FRotator(0.0f, Index * 41.0f, 0.0f), false);
	}

	for (int32 Index = 0; Index < 26; ++Index)
	{
		const float X = NatureStream.FRandRange(-7200.0f, 7200.0f);
		const float Y = NatureStream.FRandRange(-5550.0f, 5550.0f);
		// These boulders are collidable, so apply the same corridor exclusions as the trees above and
		// keep them off every crawl passage.
		if ((X > 5100.0f && FMath::Abs(Y) < 1600.0f) || FMath::Abs(Y + 3600.0f) < 460.0f || FMath::Abs(Y - 3950.0f) < 360.0f || HitsCrawlKeepout(X, Y))
		{
			continue;
		}
		SpawnBlock(FVector(X, Y, 36.0f), FVector(NatureStream.FRandRange(0.65f, 1.45f), NatureStream.FRandRange(0.45f, 1.20f), NatureStream.FRandRange(0.32f, 0.58f)), FLinearColor(0.13f, 0.14f, 0.13f, 1.0f), FRotator(0.0f, Index * 23.0f, 0.0f), true, EBHBlockMaterial::Concrete);
	}

	ScarePoints.Append({
		FVector(-7050.0f, -4900.0f, 110.0f), FVector(-4200.0f, -4550.0f, 110.0f), FVector(-400.0f, -4550.0f, 110.0f), FVector(3500.0f, -4550.0f, 110.0f), FVector(7050.0f, -3600.0f, 110.0f),
		FVector(-7000.0f, -400.0f, 110.0f), FVector(-3200.0f, -300.0f, 110.0f), FVector(0.0f, 300.0f, 110.0f), FVector(3200.0f, -300.0f, 110.0f), FVector(7000.0f, 400.0f, 110.0f),
		FVector(-7050.0f, 4200.0f, 110.0f), FVector(-3300.0f, 4550.0f, 110.0f), FVector(500.0f, 4550.0f, 110.0f), FVector(4200.0f, 4550.0f, 110.0f), FVector(7050.0f, 4200.0f, 110.0f)
	});

	SpawnAmbient(FVector(-6200.0f, -4700.0f, 150.0f), 36.0f, 0.16f, 0.05f, 0.18f);
	SpawnAmbient(FVector(6200.0f, 4700.0f, 150.0f), 52.0f, 0.14f, 0.04f, 0.24f);
	SpawnAmbient(FVector(0.0f, 0.0f, 150.0f), 24.0f, 0.12f, 0.06f, 0.12f);

	AddFoggroundsLightingPass();
	AddFoggroundsMoodPass();
	{
		FRandomStream VariationStream(BHVariationSeedForLevel(RuntimeLevelName, RuntimeStageIndex, RoundSeed));
		AddHorrorVariationPass(VariationStream);
	}
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildMapSubwayExitStation(const FVector& GateLocation, float DirectionSign, const FString& StationName, const FString& DestinationText)
{
	if (!GetWorld())
	{
		return;
	}

	const float Direction = DirectionSign >= 0.0f ? 1.0f : -1.0f;
	const auto StationPoint = [&](float TowardTrain, float AlongPlatform, float Z)
	{
		return FVector(GateLocation.X + Direction * TowardTrain, GateLocation.Y + AlongPlatform, Z);
	};
	const float MapHalfX = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? 6100.0f : 5500.0f;
	const float BoundaryX = Direction > 0.0f ? MapHalfX : -MapHalfX;
	const float TowardBoundaryDistance = FMath::Max(360.0f, FMath::Abs(BoundaryX - GateLocation.X));
	const auto SafeTowardTrain = [&](float TowardTrain, float HalfDepthCm)
	{
		if (TowardTrain <= 0.0f)
		{
			return TowardTrain;
		}
		return FMath::Min(TowardTrain, FMath::Max(0.0f, TowardBoundaryDistance - 38.0f - HalfDepthCm));
	};
	const auto SafeStationPoint = [&](float TowardTrain, float AlongPlatform, float Z, float HalfDepthCm)
	{
		return StationPoint(SafeTowardTrain(TowardTrain, HalfDepthCm), AlongPlatform, Z);
	};

	const FLinearColor PlatformTint(0.115f, 0.130f, 0.135f, 1.0f);
	const FLinearColor ConcourseTint(0.090f, 0.105f, 0.110f, 1.0f);
	const FLinearColor TrackTint(0.018f, 0.021f, 0.023f, 1.0f);
	const FLinearColor RailTint(0.58f, 0.56f, 0.47f, 1.0f);
	const FLinearColor TrainTint(0.130f, 0.170f, 0.180f, 1.0f);
	const FLinearColor DoorTint(0.045f, 0.080f, 0.088f, 1.0f);
	const FLinearColor WarningTint(0.94f, 0.62f, 0.14f, 1.0f);
	const FLinearColor ExitGreen(0.12f, 0.92f, 0.46f, 1.0f);
	const FLinearColor EmergencyRed(0.82f, 0.07f, 0.045f, 1.0f);
	const FLinearColor GlassTint(0.028f, 0.065f, 0.075f, 1.0f);
	const FLinearColor SignalWhite(0.66f, 0.88f, 0.92f, 1.0f);

	SpawnBlock(StationPoint(-360.0f, 0.0f, CenterZForBlockTop(0.0f, 0.16f)), FVector(10.2f, 37.0f, 0.16f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(StationPoint(-1040.0f, 0.0f, CenterZForBlockTop(0.0f, 0.14f)), FVector(6.2f, 22.0f, 0.14f), ConcourseTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	SpawnBlock(StationPoint(-120.0f, 0.0f, CenterZForBlockBottom(296.0f, 0.14f)), FVector(12.0f, 40.0f, 0.14f), FLinearColor(0.055f, 0.064f, 0.067f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(-72.0f, -520.0f, 138.0f), FVector(0.18f, 0.11f, 2.35f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(-72.0f, 520.0f, 138.0f), FVector(0.18f, 0.11f, 2.35f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(-72.0f, 0.0f, 260.0f), FVector(0.20f, 10.8f, 0.12f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(-96.0f, 0.0f, 286.0f), FVector(0.12f, 7.8f, 0.055f), SignalWhite, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	for (int32 GuideIndex = 0; GuideIndex < 4; ++GuideIndex)
	{
		const float TowardGate = -1120.0f + GuideIndex * 230.0f;
		RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(TowardGate, -72.0f, 7.0f), FVector(1.35f, 0.045f, 0.035f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
		RegisterStationSignalBlock(SpawnDynamicBlock(StationPoint(TowardGate, 72.0f, 7.0f), FVector(1.35f, 0.045f, 0.035f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	}

	SpawnBlock(SafeStationPoint(340.0f, 0.0f, CenterZForBlockTop(-36.0f, 0.15f), 205.0f), FVector(4.1f, 40.0f, 0.15f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(SafeStationPoint(245.0f, 0.0f, 16.0f, 5.0f), FVector(0.10f, 38.0f, 0.10f), RailTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(435.0f, 0.0f, 16.0f, 5.0f), FVector(0.10f, 38.0f, 0.10f), RailTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(500.0f, 0.0f, CenterZForBlockBottom(-24.0f, 3.25f), 11.0f), FVector(0.22f, 41.0f, 3.25f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	const float PortalToward = SafeTowardTrain(462.0f, 16.0f);
	SpawnBlock(StationPoint(PortalToward, -1875.0f, 148.0f), FVector(0.32f, 0.26f, 2.95f), FLinearColor(0.070f, 0.082f, 0.086f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(StationPoint(PortalToward, 1875.0f, 148.0f), FVector(0.32f, 0.26f, 2.95f), FLinearColor(0.070f, 0.082f, 0.086f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(StationPoint(PortalToward, 0.0f, 286.0f), FVector(0.34f, 38.0f, 0.18f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);

	SpawnBlock(SafeStationPoint(120.0f, -1220.0f, 152.0f, 31.0f), FVector(0.62f, 10.4f, 2.58f), TrainTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(120.0f, 1220.0f, 152.0f, 31.0f), FVector(0.62f, 10.4f, 2.58f), TrainTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(120.0f, 0.0f, 280.0f, 32.0f), FVector(0.64f, 7.0f, 0.20f), TrainTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(82.0f, -430.0f, 132.0f, 8.0f), FVector(0.16f, 0.18f, 2.15f), DoorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(82.0f, 430.0f, 132.0f, 8.0f), FVector(0.16f, 0.18f, 2.15f), DoorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	RegisterStationSignalBlock(SpawnDynamicBlock(SafeStationPoint(82.0f, 0.0f, 248.0f, 9.0f), FVector(0.18f, 4.7f, 0.13f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	SpawnBlock(SafeStationPoint(68.0f, -240.0f, 96.0f, 6.0f), FVector(0.12f, 0.78f, 1.46f), EmergencyRed, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(SafeStationPoint(68.0f, 240.0f, 96.0f, 6.0f), FVector(0.12f, 0.78f, 1.46f), EmergencyRed, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);

	for (float DoorY : {-1500.0f, 1500.0f})
	{
		SpawnBlock(SafeStationPoint(76.0f, DoorY - 260.0f, 124.0f, 6.0f), FVector(0.12f, 0.06f, 1.90f), DoorTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(SafeStationPoint(76.0f, DoorY + 260.0f, 124.0f, 6.0f), FVector(0.12f, 0.06f, 1.90f), DoorTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(SafeStationPoint(76.0f, DoorY, 232.0f, 6.5f), FVector(0.13f, 2.8f, 0.10f), EmergencyRed, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		RegisterStationSignalBlock(SpawnDynamicBlock(SafeStationPoint(54.0f, DoorY - 180.0f, 194.0f, 4.5f), FVector(0.09f, 0.34f, 0.08f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
		RegisterStationSignalBlock(SpawnDynamicBlock(SafeStationPoint(54.0f, DoorY + 180.0f, 194.0f, 4.5f), FVector(0.09f, 0.34f, 0.08f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign));
	}

	SpawnBlock(SafeStationPoint(12.0f, 0.0f, 25.0f, 6.0f), FVector(0.12f, 36.5f, 0.15f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	for (int32 StripIndex = 0; StripIndex < 11; ++StripIndex)
	{
		const float Y = -1650.0f + StripIndex * 330.0f;
		SpawnBlock(SafeStationPoint(32.0f, Y, 31.0f, 5.5f), FVector(0.11f, 0.42f, 0.055f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	}
	for (int32 BollardIndex = 0; BollardIndex < 3; ++BollardIndex)
	{
		const float Y = -1100.0f + BollardIndex * 1100.0f;
		SpawnBlock(SafeStationPoint(-70.0f, Y, 62.0f, 4.5f), FVector(0.09f, 0.09f, 1.05f), SignalWhite, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	}
	SpawnBlock(StationPoint(-650.0f, -1980.0f, 128.0f), FVector(9.2f, 0.22f, 2.55f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(StationPoint(-650.0f, 1980.0f, 128.0f), FVector(9.2f, 0.22f, 2.55f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(StationPoint(-1260.0f, -1120.0f, 68.0f), FVector(1.0f, 0.16f, 0.88f), WarningTint, FRotator::ZeroRotator, true, EBHBlockMaterial::WarningSign);
	SpawnBlock(StationPoint(-1260.0f, 0.0f, 68.0f), FVector(1.0f, 0.16f, 0.88f), WarningTint, FRotator::ZeroRotator, true, EBHBlockMaterial::WarningSign);
	SpawnBlock(StationPoint(-1260.0f, 1120.0f, 68.0f), FVector(1.0f, 0.16f, 0.88f), WarningTint, FRotator::ZeroRotator, true, EBHBlockMaterial::WarningSign);

	for (int32 PillarIndex = 0; PillarIndex < 4; ++PillarIndex)
	{
		const float Y = -1350.0f + PillarIndex * 900.0f;
		SpawnBlock(StationPoint(-620.0f, Y, 124.0f), FVector(0.32f, 0.32f, 2.50f), FLinearColor(0.080f, 0.092f, 0.096f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}

	for (int32 BenchIndex = 0; BenchIndex < 2; ++BenchIndex)
	{
		const float Y = BenchIndex == 0 ? -1650.0f : 1650.0f;
		SpawnBlock(StationPoint(-720.0f, Y, 52.0f), FVector(2.2f, 0.55f, 0.42f), GlassTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	}

	const float DisplayYaw = Direction > 0.0f ? 0.0f : 180.0f;
	const float ReverseDisplayYaw = FRotator::NormalizeAxis(DisplayYaw + 180.0f);
	if (ABHTrainDisplayActor* DisplayA = GetWorld()->SpawnActor<ABHTrainDisplayActor>(StationPoint(-520.0f, -1180.0f, 226.0f), FRotator(0.0f, DisplayYaw, 0.0f)))
	{
		DisplayA->SetDisplayProfile(EBHTrainDisplayProfile::StationCountdownCompact);
		DisplayA->ConfigureExitCountdownDisplay(StationName, DestinationText, ExitGreen);
	}
	if (ABHTrainDisplayActor* DisplayB = GetWorld()->SpawnActor<ABHTrainDisplayActor>(StationPoint(-520.0f, 1180.0f, 226.0f), FRotator(0.0f, ReverseDisplayYaw, 0.0f)))
	{
		DisplayB->SetDisplayProfile(EBHTrainDisplayProfile::StationCountdownCompact);
		DisplayB->ConfigureExitCountdownDisplay(StationName, DestinationText, ExitGreen);
	}

	for (int32 LightIndex = 0; LightIndex < 4; ++LightIndex)
	{
		const FVector LightLocation = StationPoint(-220.0f, -1350.0f + LightIndex * 900.0f, 268.0f);
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(LightLocation, FRotator::ZeroRotator))
		{
			Light->Configure(0, LightIndex % 2 == 0 ? FLinearColor(0.35f, 0.88f, 0.78f, 1.0f) : FLinearColor(0.90f, 0.54f, 0.30f, 1.0f), 760.0f, 920.0f);
			FlickerLights.Add(Light);
			RegisterStationSignalLight(Light);
		}
	}
	UpdateStationSignalState(false);
}

void ABHGameMode::BuildFoggroundsFinalStation()
{
	if (!GetWorld())
	{
		return;
	}

	const FLinearColor PlatformTint(0.12f, 0.14f, 0.145f, 1.0f);
	const FLinearColor TrackTint(0.025f, 0.028f, 0.030f, 1.0f);
	const FLinearColor TrainTint(0.13f, 0.17f, 0.18f, 1.0f);
	const FLinearColor WarningTint(0.95f, 0.58f, 0.12f, 1.0f);
	const FLinearColor ExitGreen(0.14f, 0.95f, 0.44f, 1.0f);
	const FLinearColor EmergencyRed(0.90f, 0.08f, 0.04f, 1.0f);
	const FLinearColor GlassTint(0.03f, 0.08f, 0.10f, 1.0f);

	SpawnBlock(FVector(6580.0f, 0.0f, CenterZForBlockTop(0.0f, 0.18f)), FVector(27.0f, 72.0f, 0.18f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(7540.0f, 0.0f, CenterZForBlockTop(-42.0f, 0.15f)), FVector(8.5f, 76.0f, 0.15f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(7810.0f, 0.0f, 152.0f), FVector(5.0f, 74.0f, 3.0f), TrainTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(7180.0f, -3725.0f, CenterZForBlockBottom(0.0f, 2.7f)), FVector(23.0f, 0.22f, 2.7f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(7180.0f, 3725.0f, CenterZForBlockBottom(0.0f, 2.7f)), FVector(23.0f, 0.22f, 2.7f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(5350.0f, 0.0f, CenterZForBlockBottom(0.0f, 2.45f)), FVector(0.24f, 58.0f, 2.45f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	for (int32 DoorIndex = 0; DoorIndex < 4; ++DoorIndex)
	{
		const float DoorY = -2250.0f + DoorIndex * 1500.0f;
		SpawnBlock(FVector(7200.0f, DoorY, 232.0f), FVector(0.10f, 3.4f, 0.12f), ExitGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FVector(7235.0f, DoorY, 52.0f), FVector(0.12f, 3.6f, 0.11f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		if (ABHTrainDoor* Door = GetWorld()->SpawnActor<ABHTrainDoor>(FVector(7150.0f, DoorY, 92.0f), FRotator::ZeroRotator))
		{
			Door->ConfigureDoor(true, FString::Printf(TEXT("Evacuation Door %d"), DoorIndex + 1));
			if (EscapeStationManagers.Num() == 0)
			{
				if (ABHEscapeStationManager* Manager = GetWorld()->SpawnActor<ABHEscapeStationManager>(FVector(6500.0f, 0.0f, 120.0f), FRotator::ZeroRotator))
				{
					EscapeStationManagers.Add(Manager);
				}
			}
			if (EscapeStationManagers.Num() > 0 && EscapeStationManagers[0])
			{
				EscapeStationManagers[0]->RegisterEscapeDoor(Door);
			}
		}
	}

	for (int32 SignIndex = 0; SignIndex < 3; ++SignIndex)
	{
		const float SignY = -2500.0f + SignIndex * 2500.0f;
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(5820.0f, SignY, 238.0f), FRotator(0.0f, 90.0f, 0.0f)))
		{
			Display->ConfigureDisplay(TEXT("FINAL TERMINAL LOCKED"), TEXT("Class average, individual mastery, and required questions must clear before evacuation."), EmergencyRed);
			Display->SetActorScale3D(FVector(1.12f, 1.0f, 1.10f));
			if (EscapeStationManagers.Num() == 0)
			{
				if (ABHEscapeStationManager* Manager = GetWorld()->SpawnActor<ABHEscapeStationManager>(FVector(6500.0f, 0.0f, 120.0f), FRotator::ZeroRotator))
				{
					EscapeStationManagers.Add(Manager);
				}
			}
			if (EscapeStationManagers.Num() > 0 && EscapeStationManagers[0])
			{
				EscapeStationManagers[0]->RegisterDisplay(Display);
			}
		}
	}

	for (int32 PillarIndex = 0; PillarIndex < 10; ++PillarIndex)
	{
		const float Y = -3000.0f + PillarIndex * 660.0f;
		const float X = (PillarIndex % 2 == 0) ? 6120.0f : 6620.0f;
		SpawnBlock(FVector(X, Y, 125.0f), FVector(0.42f, 0.42f, 2.55f), TrackTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	}

	for (int32 CoverIndex = 0; CoverIndex < 8; ++CoverIndex)
	{
		const float Y = -2800.0f + CoverIndex * 800.0f;
		SpawnBlock(FVector(5840.0f + (CoverIndex % 2) * 640.0f, Y, 52.0f), FVector(2.4f, 0.75f, 0.58f), CoverIndex % 2 == 0 ? TrainTint : WarningTint, FRotator(0.0f, CoverIndex * 17.0f, 0.0f), true, CoverIndex % 2 == 0 ? EBHBlockMaterial::PaintedMetal : EBHBlockMaterial::WarningSign);
	}

	SpawnBlock(FVector(5500.0f, -2850.0f, CenterZForBlockTop(0.0f, 0.12f)), FVector(14.0f, 7.5f, 0.12f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(5500.0f, 0.0f, CenterZForBlockTop(0.0f, 0.12f)), FVector(16.0f, 9.0f, 0.12f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(5500.0f, 2850.0f, CenterZForBlockTop(0.0f, 0.12f)), FVector(14.0f, 7.5f, 0.12f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(6160.0f, -3120.0f, 112.0f), FVector(8.0f, 0.18f, 1.35f), GlassTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
	SpawnBlock(FVector(6160.0f, 3120.0f, 112.0f), FVector(8.0f, 0.18f, 1.35f), GlassTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);

	const TArray<FVector> CrawlCenters = {
		FVector(5960.0f, -2440.0f, 76.0f),
		FVector(5960.0f, 2440.0f, 76.0f),
		FVector(6420.0f, 620.0f, 76.0f)
	};
	for (int32 Index = 0; Index < CrawlCenters.Num(); ++Index)
	{
		const FVector Center = CrawlCenters[Index];
		SpawnBlock(Center + FVector(0.0f, 0.0f, 38.0f), FVector(5.8f, 1.55f, 0.85f), TrainTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Center + FVector(0.0f, -96.0f, 118.0f), FVector(5.6f, 0.10f, 0.18f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		if (ABHCrawlSpaceVolume* Crawl = GetWorld()->SpawnActor<ABHCrawlSpaceVolume>(Center, FRotator::ZeroRotator))
		{
			Crawl->Configure(FVector(360.0f, 110.0f, 86.0f));
		}
	}

	for (int32 LightIndex = 0; LightIndex < 9; ++LightIndex)
	{
		const FVector LightLocation(6500.0f, -3150.0f + LightIndex * 790.0f, 275.0f);
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(LightLocation, FRotator::ZeroRotator))
		{
			Light->Configure(0, LightIndex % 3 == 0 ? EmergencyRed : FLinearColor(0.34f, 0.88f, 0.76f, 1.0f), 820.0f, 920.0f);
			FlickerLights.Add(Light);
		}
	}

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetFinalEscapeState(EBHFinalEscapeState::Locked, 0.0f, 0.0f, 0.0f);
		BHGS->SetIntermissionLocks(false, false, false);
	}
}

void ABHGameMode::BuildTrainIntermissionLevel()
{
	SurvivorSpawns = {
		FVector(-520.0f, -140.0f, 124.0f), FVector(-400.0f, -20.0f, 124.0f), FVector(-320.0f, 80.0f, 124.0f),
		FVector(-220.0f, 180.0f, 124.0f), FVector(-90.0f, -165.0f, 124.0f), FVector(20.0f, -55.0f, 124.0f),
		FVector(130.0f, 65.0f, 124.0f), FVector(240.0f, 165.0f, 124.0f), FVector(360.0f, -145.0f, 124.0f),
		FVector(470.0f, -45.0f, 124.0f), FVector(580.0f, 65.0f, 124.0f), FVector(690.0f, 165.0f, 124.0f)
	};
	HunterSpawn = FVector(-980.0f, 0.0f, 124.0f);

	const FLinearColor FloorTint(0.055f, 0.065f, 0.068f, 1.0f);
	const FLinearColor WallTint(0.10f, 0.12f, 0.13f, 1.0f);
	const FLinearColor SeatTint(0.10f, 0.36f, 0.42f, 1.0f);
	const FLinearColor PoleTint(0.70f, 0.66f, 0.54f, 1.0f);
	const FLinearColor PlatformTint(0.17f, 0.16f, 0.14f, 1.0f);
	const FLinearColor WarningTint(0.92f, 0.64f, 0.18f, 1.0f);

	// =============================================================================================
	// Sealed five-car subway tube. Previously each car was four loose slabs with open ends and 200cm
	// gaps, so players could walk off the ends / through the gaps into the service trench and see the
	// fake "moving sticks" on a bare platform. It is now ONE continuous, fully enclosed tube
	// (interior y in [-300,300], z in [0,300], x in [-3750,3750]) split into cars by gangway bulkheads
	// with open archways, with glazed full-height side walls (collidable tinted glass), sealed end
	// caps, an opaque tunnel backdrop behind the windows, and architectural trim so it reads as a real
	// carriage. CenterX for car i = -3000 + i*1500.
	// =============================================================================================
	// Extended symmetrically to +/-5250 (was +/-3750) to add a LOUNGE/seating car at each end (CenterX +/-4500). Most
	// tube geometry below is parameterised by TubeMin/Max/Len so it grows automatically; the few hardcoded pieces
	// (walkable substrate floor, outer enclosure, gangways) are widened in step and the two cars are dressed below.
	const float TubeMinX = -5250.0f;
	const float TubeMaxX = 5250.0f;
	const float TubeLen = (TubeMaxX - TubeMinX) / 100.0f;   // 75 units = 7500cm
	// Cabin centres for the O-key unstick (reset to the centre of the car you're in). Floor Z matches the spawn
	// transforms above. Recorded from the tube bounds so all seven cars (incl. the two end lounges) are covered.
	RecordTrainCabinCenters(TubeMinX, TubeMaxX, 124.0f);
	const float WallY = 300.0f;   // interior side-wall plane
	const float CeilZ = 300.0f;   // interior ceiling underside
	const float WinBotZ = 118.0f; // window band bottom
	const float WinTopZ = 212.0f; // window band top
	const float WallTh = 0.18f;   // 18cm wall thickness
	const float BackdropY = 580.0f;

	const FLinearColor GlassTint(0.020f, 0.032f, 0.040f, 0.55f);
	const FLinearColor TrimTint(0.34f, 0.36f, 0.33f, 1.0f);
	const FLinearColor RoofTint(0.085f, 0.10f, 0.105f, 1.0f);
	const FLinearColor TunnelWallTint(0.014f, 0.018f, 0.022f, 1.0f);
	const FLinearColor CoveTint(0.40f, 0.66f, 0.70f, 1.0f);
	const FLinearColor SkirtTint(0.05f, 0.055f, 0.05f, 1.0f);

	// Substrate floor (the COLLIDABLE walkable floor; widened to 110u so it covers the two new end cars -- the finished
	// veneer below it is non-colliding) + a flush finished interior floor for the cars.
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.20f)), FVector(110.0f, 24.0f, 0.20f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(2.0f, 0.05f)), FVector(TubeLen, 6.1f, 0.05f), FLinearColor(0.085f, 0.098f, 0.104f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);

	// Outer enclosure (kept as the far bound; never seen once the tube is sealed). Raised above the backdrop.
	SpawnBlock(FVector(0.0f, -1180.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(110.0f, 0.22f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 1180.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(110.0f, 0.22f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.22f, 24.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(5500.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.4f)), FVector(0.22f, 24.0f, 3.4f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

	// Continuous ceiling over the whole tube + a recessed glowing light cove down the centreline.
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(CeilZ, 0.16f)), FVector(TubeLen, 6.2f, 0.16f), RoofTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 0.0f, CeilZ - 9.0f), FVector(TubeLen, 1.4f, 0.05f), CoveTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);

	// Adds a sealed, full-height window wall (solid wainscot + collidable tinted glass band + solid
	// header) along an X-segment at plane y=YPlane. The glass seals the band so the tunnel strips read
	// through it but cannot be reached or jumped through.
	auto AddWallRun = [&](float YPlane, float X0, float X1)
	{
		const float Len = (X1 - X0) / 100.0f;
		const float Mid = (X0 + X1) * 0.5f;
		if (Len <= 0.02f)
		{
			return;
		}
		SpawnBlock(FVector(Mid, YPlane, CenterZForBlockBottom(0.0f, WinBotZ / 100.0f)), FVector(Len, WallTh, WinBotZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(Mid, YPlane, CenterZForBlockBottom(WinTopZ, (CeilZ - WinTopZ) / 100.0f)), FVector(Len, WallTh, (CeilZ - WinTopZ) / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(Mid, YPlane, CenterZForBlockBottom(WinBotZ, (WinTopZ - WinBotZ) / 100.0f)), FVector(Len, 0.05f, (WinTopZ - WinBotZ) / 100.0f), GlassTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
	};

	// Decorative (non-colliding) framing for one side wall: window sill/header rails, floor skirting,
	// a cant rail under the roof, and vertical window mullions.
	auto AddWallTrim = [&](float YPlane)
	{
		const float InY = YPlane > 0.0f ? YPlane - 5.0f : YPlane + 5.0f;
		SpawnBlock(FVector(0.0f, InY, WinBotZ), FVector(TubeLen, 0.05f, 0.06f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, InY, WinTopZ), FVector(TubeLen, 0.05f, 0.06f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, InY, 16.0f), FVector(TubeLen, 0.06f, 0.16f), SkirtTint, FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
		SpawnBlock(FVector(0.0f, InY, CeilZ - 7.0f), FVector(TubeLen, 0.05f, 0.07f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		for (float MullX = TubeMinX + 150.0f; MullX <= TubeMaxX - 150.0f; MullX += 300.0f)
		{
			SpawnBlock(FVector(MullX, InY, (WinBotZ + WinTopZ) * 0.5f), FVector(0.07f, 0.05f, (WinTopZ - WinBotZ) / 100.0f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		}
	};

	// Both side walls are fully sealed (no openings). The level advances on the intermission timer, not
	// by reaching the platform, so there is no door in the side wall to slip through into the service
	// trench; the station beyond the -Y wall is now scenery viewed through the STATION car's windows.
	AddWallRun(WallY, TubeMinX, TubeMaxX);
	AddWallRun(-WallY, TubeMinX, TubeMaxX);
	AddWallTrim(WallY);
	AddWallTrim(-WallY);

	// Sealed end caps (front & back of the train) with a dark cab window inset.
	for (float EndX : {TubeMinX, TubeMaxX})
	{
		SpawnBlock(FVector(EndX, 0.0f, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, 6.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(EndX > 0.0f ? EndX - 7.0f : EndX + 7.0f, 0.0f, 168.0f), FVector(0.05f, 2.2f, 0.55f), GlassTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
	}

	// Opaque tunnel backdrop just behind the windows so the moving strips read as a real tunnel rather
	// than floating sticks on an open platform. Full length on BOTH walls now: every car (incl. the rear
	// cars + both lounges) shows a moving tunnel through its windows, so the -Y backdrop no longer stops
	// short of the station -- the station-peek on the rear -Y windows is replaced by consistent motion.
	SpawnBlock(FVector(0.0f, BackdropY, CenterZForBlockBottom(0.0f, 3.2f)), FVector(TubeLen, 0.2f, 3.2f), TunnelWallTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, -BackdropY, CenterZForBlockBottom(0.0f, 3.2f)), FVector(TubeLen, 0.2f, 3.2f), TunnelWallTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Concrete);

	// Gangway bulkheads between the five cars: full-height transverse walls with a centred open archway
	// so players can always move car to car (no door that could trap them during the moving phases).
	const float ArchHalf = 95.0f;
	const float ArchTopZ = 215.0f;
	for (float GangwayX : {-3750.0f, -2250.0f, -750.0f, 750.0f, 2250.0f, 3750.0f})
	{
		const float PanelSpan = WallY - ArchHalf;
		const float PanelCtr = ArchHalf + PanelSpan * 0.5f;
		SpawnBlock(FVector(GangwayX, -PanelCtr, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, PanelSpan / 100.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, PanelCtr, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, PanelSpan / 100.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, 0.0f, CenterZForBlockBottom(ArchTopZ, (CeilZ - ArchTopZ) / 100.0f)), FVector(WallTh, (2.0f * ArchHalf) / 100.0f, (CeilZ - ArchTopZ) / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, -ArchHalf, CenterZForBlockBottom(0.0f, ArchTopZ / 100.0f)), FVector(WallTh + 0.05f, 0.08f, ArchTopZ / 100.0f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, ArchHalf, CenterZForBlockBottom(0.0f, ArchTopZ / 100.0f)), FVector(WallTh + 0.05f, 0.08f, ArchTopZ / 100.0f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, 0.0f, CenterZForBlockBottom(ArchTopZ, 0.06f)), FVector(WallTh + 0.05f, (2.0f * ArchHalf) / 100.0f, 0.06f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	}

	const FString Destination = PendingIntermissionResult == EBHRoundPhase::HunterWin
		? TEXT("Remediation Platform")
		: FString::Printf(TEXT("Next Stop: %s"), *GetNextMapAfterStage(RuntimeStageIndex));
	const bool bFinalRecap = IsFinalStage() || GetNextMapAfterStage(RuntimeStageIndex).Equals(TEXT("Final"), ESearchCase::IgnoreCase);

	TrainIntermissionManager = GetWorld()->SpawnActor<ABHTrainIntermissionManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (TrainIntermissionManager)
	{
		TrainIntermissionManager->ConfigureIntermission(RuntimeStageIndex, GetNextMapAfterStage(RuntimeStageIndex), bFinalRecap ? TEXT("Final Station: Leaderboard") : Destination, bFinalRecap);
	}

	// Per-car interior dressing + one recap board (-Y wall) + the tunnel-motion strips. Board #1 is
	// registered here for all cars before the second loop registers board #2, preserving the manager's
	// display-index -> content mapping.
	for (int32 CarIndex = 0; CarIndex < 5; ++CarIndex)
	{
		const float CenterX = -3000.0f + CarIndex * 1500.0f;
		const FString CarName = CarIndex == 0
			? TEXT("BOARDING")
			: (CarIndex == 1 ? TEXT("RECAP") : (CarIndex == 2 ? TEXT("SHOP") : (CarIndex == 3 ? TEXT("SOCIAL") : TEXT("STATION"))));

		// Wall-side bench seating only in the cars without wall props (RECAP, STATION); the BOARDING,
		// SHOP and SOCIAL cars carry teacher / survivor terminals and minigames against the walls.
		// Shorter than before so the cars no longer read as bench-jammed.
		if (CarIndex == 1 || CarIndex == 4)
		{
			for (float BenchXOffset : {-380.0f, 380.0f})
			{
				SpawnBlock(FVector(CenterX + BenchXOffset, -255.0f, 70.0f), FVector(1.55f, 0.42f, 0.38f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
				SpawnBlock(FVector(CenterX + BenchXOffset, 255.0f, 70.0f), FVector(1.55f, 0.42f, 0.38f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
			}
		}

		// Hand poles + longitudinal overhead grab rails + a ceiling warning strip.
		for (int32 PoleIndex = 0; PoleIndex < 2; ++PoleIndex)
		{
			const float PoleX = CenterX - 420.0f + PoleIndex * 840.0f;
			SpawnBlock(FVector(PoleX, -72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(PoleX, 72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(FVector(CenterX, -72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(CenterX, 72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(CenterX, 0.0f, 272.0f), FVector(5.0f, 0.05f, 0.05f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);

		// Recap board #1, mounted just inside the -Y window glass so its frame clears the wall plane.
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(CenterX - 250.0f, -290.0f, 178.0f), FRotator(0.0f, 0.0f, 0.0f)))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(CarName, TEXT("Class performance review in progress."), FLinearColor(0.38f, 0.90f, 0.78f, 1.0f));
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterDisplay(Display);
			}
		}

		// Scrolling tunnel-motion strips seen through the windows: now on BOTH walls of EVERY car. The two
		// rear cars (SOCIAL, STATION) used to be -Y-bare because their windows looked onto the station, but
		// that left "front 3 + last cabin" with no motion on one wall; the full-length -Y backdrop (above)
		// now backs every car so the moving tunnel reads through all windows on both sides.
		auto AddTunnelSide = [&](float SideY)
		{
			// The window glass is permanently sealed now, so the old opaque "shutter" that used to slide
			// over an open window during motion would only blank out the view. It is gone: the scrolling
			// strips plus the dark backdrop carry the moving-tunnel read on their own.
			if (ABHTrainTunnelMotionActor* Tunnel = GetWorld()->SpawnActor<ABHTrainTunnelMotionActor>(FVector(CenterX, SideY, 165.0f), FRotator::ZeroRotator))
			{
				// Loop length == the full car pitch (1500) so the strips/pillars tile the ENTIRE window band of
				// the car edge to edge. A shorter loop only painted the middle panes and left the windows near
				// the gangways static ("the bars don't go all the way").
				Tunnel->ConfigureMotion(1500.0f, 740.0f);
				if (TrainIntermissionManager)
				{
					TrainIntermissionManager->RegisterTunnelMotion(Tunnel);
				}
			}
		};
		AddTunnelSide(430.0f);
		AddTunnelSide(-430.0f);
	}

	for (int32 CarIndex = 0; CarIndex < 5; ++CarIndex)
	{
		const float CenterX = -3000.0f + CarIndex * 1500.0f;
		const FString CarName = CarIndex == 0
			? TEXT("BOARDING")
			: (CarIndex == 1 ? TEXT("RECAP") : (CarIndex == 2 ? TEXT("SHOP") : (CarIndex == 3 ? TEXT("SOCIAL") : TEXT("STATION"))));
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(CenterX + 250.0f, 290.0f, 178.0f), FRotator(0.0f, 180.0f, 0.0f)))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(CarName, TEXT("Class performance review in progress."), FLinearColor(0.38f, 0.90f, 0.78f, 1.0f));
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterDisplay(Display);
			}
		}
	}

	// No side doors: the inter-car connections are open gangway archways and both side walls are fully
	// sealed, so there is nothing to slip through. The level transitions on the intermission timer
	// (FinishIntermission), not by walking to a door, so no platform door is needed.

	const EBHPowerupType ShopTypes[] = {
		EBHPowerupType::StaminaBoost,
		EBHPowerupType::SprintBurst,
		EBHPowerupType::FlashlightBoost,
		EBHPowerupType::QuestionHint,
		EBHPowerupType::DecoySound,
		EBHPowerupType::DoorRush
	};
	// Survivor shop terminals line the SHOP car (CenterX 0) walls but are kept clear of the recap boards'
	// X-spans (board #1 at x[-518,18] on the -Y wall, board #2 at x[-18,518] on the +Y wall) so they no
	// longer stand in front of the boards: three on the +Y wall left of board #2, three on the -Y wall
	// right of board #1, each turned to face the aisle.
	const FVector SurvivorShopSlots[] = {
		FVector(-120.0f, 235.0f, 110.0f), FVector(-300.0f, 235.0f, 110.0f), FVector(-480.0f, 235.0f, 110.0f),
		FVector(120.0f, -235.0f, 110.0f), FVector(300.0f, -235.0f, 110.0f), FVector(480.0f, -235.0f, 110.0f)
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShopTypes); ++Index)
	{
		const FRotator SlotRot(0.0f, SurvivorShopSlots[Index].Y > 0.0f ? 90.0f : -90.0f, 0.0f);
		if (ABHPowerupShopTerminal* Terminal = GetWorld()->SpawnActor<ABHPowerupShopTerminal>(SurvivorShopSlots[Index], SlotRot))
		{
			Terminal->ConfigureShopItem(ShopTypes[Index]);
		}
	}

	const EBHPowerupType TeacherShopTypes[] = {
		EBHPowerupType::TeacherScanFocus,
		EBHPowerupType::TeacherBlackoutSurge,
		EBHPowerupType::TeacherPatrolIntel
	};
	// Teacher terminals move to the BOARDING car (CenterX -3000) -Y wall, clear of that car's recap
	// board (x[-3518,-2982]); previously they were jammed into the SOCIAL car beside the minigames and
	// in front of its board.
	const FVector TeacherShopSlots[] = {
		FVector(-2900.0f, -235.0f, 110.0f), FVector(-2720.0f, -235.0f, 110.0f), FVector(-2540.0f, -235.0f, 110.0f)
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TeacherShopTypes); ++Index)
	{
		if (ABHPowerupShopTerminal* Terminal = GetWorld()->SpawnActor<ABHPowerupShopTerminal>(TeacherShopSlots[Index], FRotator(0.0f, -90.0f, 0.0f)))
		{
			Terminal->ConfigureShopItem(TeacherShopTypes[Index]);
		}
	}

	if (ABHTrainBonusQuestionTerminal* BonusTerminal = GetWorld()->SpawnActor<ABHTrainBonusQuestionTerminal>(FVector(-1450.0f, 0.0f, 118.0f), FRotator(0.0f, 180.0f, 0.0f)))
	{
		if (TrainIntermissionManager)
		{
			TrainIntermissionManager->RegisterBonusTerminal(BonusTerminal);
		}
	}

	struct FTrainActivitySpec
	{
		EBHTrainActivityType Type;
		FVector Location;
		FRotator Rotation;
		int32 SeedOffset;
	};
	// Minigames pushed against the SOCIAL car (CenterX 1500) walls and spread along X, opening a wide
	// central aisle (previously all four sat at y=+/-96, crammed around the hand poles). Each is kept
	// clear of that car's recap boards (board #1 x[982,1518] on -Y, board #2 x[1482,2018] on +Y).
	const FTrainActivitySpec ActivitySpecs[] = {
		{EBHTrainActivityType::ReflexArcade, FVector(1100.0f, 210.0f, 110.0f), FRotator(0.0f, 180.0f, 0.0f), 17},
		{EBHTrainActivityType::DrinkCooler, FVector(1340.0f, 210.0f, 108.0f), FRotator(0.0f, 180.0f, 0.0f), 47},
		{EBHTrainActivityType::SnackCart, FVector(1570.0f, -210.0f, 108.0f), FRotator(0.0f, 0.0f, 0.0f), 31},
		{EBHTrainActivityType::MemoryTable, FVector(1900.0f, -210.0f, 104.0f), FRotator(0.0f, 0.0f, 0.0f), 59}
	};
	for (const FTrainActivitySpec& Spec : ActivitySpecs)
	{
		if (ABHTrainActivityStation* Activity = GetWorld()->SpawnActor<ABHTrainActivityStation>(Spec.Location, Spec.Rotation))
		{
			Activity->ConfigureActivity(Spec.Type, RoundSeed + RuntimeStageIndex * 101 + Spec.SeedOffset);
		}
	}

	// =====================================================================================================
	// EASTER EGG: a hidden "maintenance hatch" in the front cab corner lifts a passenger up onto the TRAIN
	// ROOF (the ceiling slab top, ~z316, is already a walkable collidable surface). Cosmetic only -- the
	// intermission advances on its own timer, so a roof visit never blocks boarding or touches scoring. It
	// awards the "roof_rider" achievement (-> the Top Hat headwear). See Docs/EASTER_EGGS.md.
	// =====================================================================================================
	if (BHAreEasterEggsEnabled())
	{
		// Roof actors carry blocking collision, so force AlwaysSpawn -- otherwise a spawn-overlap test on the
		// roof slab/rails can null them (which previously left the breaker non-existent, so N found nothing).
		FActorSpawnParameters RoofSpawnParams;
		RoofSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FVector HatchLocation(-3600.0f, -282.0f, 110.0f);
		if (ABHTrainRoofHatch* RoofHatch = GetWorld()->SpawnActor<ABHTrainRoofHatch>(HatchLocation, FRotator::ZeroRotator, RoofSpawnParams))
		{
			// Lift straight up above the hatch onto the roof slab top (z316), reusing the surface->capsule
			// offset spawns use (floor top z2 -> survivor spawn z124, i.e. +122).
			RoofHatch->ConfigureHatch(FVector(HatchLocation.X, 0.0f, 438.0f));
		}

		// A matching hatch ON the roof drops a passenger back down into the carriage, so the roof is two-way
		// (the N marker and the O reset-to-train key are extra fallbacks). Placed near the arrival point.
		if (ABHTrainRoofHatch* DownHatch = GetWorld()->SpawnActor<ABHTrainRoofHatch>(FVector(HatchLocation.X + 150.0f, 0.0f, 351.0f), FRotator::ZeroRotator, RoofSpawnParams))
		{
			DownHatch->ConfigureHatch(FVector(HatchLocation.X + 150.0f, -180.0f, 124.0f));
		}

		// Low safety rails around the roof walkway (the slab top spans y[-310,310], x[-3750,3750]) so a curious
		// passenger doesn't stroll off into the tunnel trench. They sit OUTSIDE the tube, so they're never seen
		// from the interior.
		const FLinearColor RoofRailTint(0.10f, 0.11f, 0.12f, 1.0f);
		SpawnBlock(FVector(0.0f, 300.0f, 346.0f), FVector(TubeLen, 0.08f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, -300.0f, 346.0f), FVector(TubeLen, 0.08f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(TubeMaxX - 30.0f, 0.0f, 346.0f), FVector(0.08f, 6.0f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(TubeMinX + 30.0f, 0.0f, 346.0f), FVector(0.08f, 6.0f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

		// Solid roof EAVE / overhang: opaque slab from just inside the rail (y=+/-303) out to the tunnel backdrop
		// (y=+/-580), flush with the walkable roof top (z316). Without it a passenger on the roof looks over the
		// low rail straight down at the moving-tunnel strips (y=+/-430) and the trench -- the "outside mechanics
		// visible from the roof". The underside (z304) is above the window header (z212), so the strips still show
		// through the windows from INSIDE but are capped from ABOVE. Real metro cars overhang their windows the
		// same way. BOTH eaves now run the full length (both backdrops do too): the -Y backdrop was extended
		// to cover every car, so the -Y eave must match or a roof passenger past x2000 would look over the
		// rail straight down at the newly-added rear -Y strips/trench.
		const float EaveCenterY = 0.5f * (303.0f + BackdropY);
		const float EaveScaleY = (BackdropY - 303.0f + 6.0f) / 100.0f;
		SpawnBlock(FVector(0.0f, EaveCenterY, CenterZForBlockTop(316.0f, 0.12f)), FVector(TubeLen, EaveScaleY, 0.12f), RoofTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, -EaveCenterY, CenterZForBlockTop(316.0f, 0.12f)), FVector(TubeLen, EaveScaleY, 0.12f), RoofTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

		// Roof-light breaker: tucked off to the +Y side a fair walk from the hatch (secretish, but reachable and
		// pingable with N). Sits ON the roof slab (top ~z316). Throwing it toggles that player's OWN client-local
		// roof service lights (per-player, not shared).
		GetWorld()->SpawnActor<ABHTrainRoofBreaker>(FVector(HatchLocation.X + 2000.0f, 250.0f, 333.0f), FRotator(0.0f, 180.0f, 0.0f), RoofSpawnParams);

		// Stargazing sky, decorations, and the rooftop parkour course (shared between the intermission + lobby roofs).
		BuildTrainRoofExtras(TubeMinX, TubeMaxX, HatchLocation);
	}

	SpawnBlock(FVector(3300.0f, -760.0f, CenterZForBlockTop(0.0f, 0.22f)), FVector(20.0f, 6.0f, 0.22f), PlatformTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(3300.0f, -1120.0f, 155.0f), FVector(20.0f, 0.28f, 3.1f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(3300.0f, -430.0f, 26.0f), FVector(20.0f, 0.12f, 0.16f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	SpawnBlock(FVector(2030.0f, -760.0f, 140.0f), FVector(0.24f, 6.8f, 2.8f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(4570.0f, -760.0f, 140.0f), FVector(0.24f, 6.8f, 2.8f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(2580.0f, -430.0f, 72.0f), FVector(8.0f, 0.09f, 0.92f), FLinearColor(0.10f, 0.12f, 0.12f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(4140.0f, -430.0f, 72.0f), FVector(6.8f, 0.09f, 0.92f), FLinearColor(0.10f, 0.12f, 0.12f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(3300.0f, -930.0f, 34.0f), FVector(17.5f, 0.08f, 0.18f), FLinearColor(0.07f, 0.075f, 0.07f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(2300.0f, -760.0f, 95.0f), FVector(0.38f, 0.38f, 1.9f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(3100.0f, -760.0f, 95.0f), FVector(0.38f, 0.38f, 1.9f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(3900.0f, -760.0f, 95.0f), FVector(0.38f, 0.38f, 1.9f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);

	// =====================================================================================================
	// Two EXTRA lounge cars, one at each end (CenterX -4500 / +4500), so the intermission train has a bit more
	// room and PLENTY of seating to relax in between stages. The tube was extended symmetrically above, so the
	// floor / ceiling / walls / end caps already cover them and the new gangways at +/-3750 connect them -- here we
	// just dress the interiors as comfortable lounges (benches with backs, a coffee table, planters, warm light).
	// =====================================================================================================
	const FLinearColor LoungeWarm(1.0f, 0.72f, 0.42f, 1.0f);
	const FLinearColor LoungeWood(0.22f, 0.16f, 0.11f, 1.0f);
	const FLinearColor LoungePlant(0.16f, 0.42f, 0.20f, 1.0f);
	for (const float LoungeX : {-4500.0f, 4500.0f})
	{
		// Generous bench seating with low backs down both walls -- this is the "place to sit".
		for (const float BenchXOffset : {-470.0f, 0.0f, 470.0f})
		{
			for (const float BenchY : {-255.0f, 255.0f})
			{
				SpawnBlock(FVector(LoungeX + BenchXOffset, BenchY, 70.0f), FVector(1.7f, 0.46f, 0.40f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);
				SpawnBlock(FVector(LoungeX + BenchXOffset, BenchY > 0.0f ? 286.0f : -286.0f, 104.0f), FVector(1.7f, 0.06f, 0.34f), SeatTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Leather);
			}
		}
		// A low coffee table down the centre with two facing stools.
		SpawnBlock(FVector(LoungeX, 0.0f, 44.0f), FVector(1.4f, 0.9f, 0.06f), LoungeWood, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
		SpawnBlock(FVector(LoungeX, 0.0f, 22.0f), FVector(0.5f, 0.5f, 0.42f), LoungeWood, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
		for (const float StoolX : {-150.0f, 150.0f})
		{
			SpawnBlock(FVector(LoungeX + StoolX, 0.0f, 36.0f), FVector(0.4f, 0.4f, 0.36f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);
		}
		// A leafy planter in each far corner for a relaxed lounge feel.
		for (const float PlantX : {-560.0f, 560.0f})
		{
			SpawnBlock(FVector(LoungeX + PlantX, -250.0f, 30.0f), FVector(0.4f, 0.4f, 0.60f), LoungeWood, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
			SpawnBlock(FVector(LoungeX + PlantX, -250.0f, 92.0f), FVector(0.7f, 0.7f, 0.55f), LoungePlant, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
		}
		// Hand poles + overhead grab rails, matching the other cars.
		for (int32 PoleIndex = 0; PoleIndex < 2; ++PoleIndex)
		{
			const float PoleX = LoungeX - 420.0f + PoleIndex * 840.0f;
			SpawnBlock(FVector(PoleX, -72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(PoleX, 72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(FVector(LoungeX, -72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(LoungeX, 72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		// Warm lounge accent light (the centreline service-light loop already gives the baseline).
		if (APointLight* L = GetWorld()->SpawnActor<APointLight>(FVector(LoungeX, 0.0f, 250.0f), FRotator::ZeroRotator))
		{
			L->SetMobility(EComponentMobility::Movable);
			if (UPointLightComponent* PLC = Cast<UPointLightComponent>(L->GetLightComponent()))
			{
				PLC->SetLightColor(LoungeWarm);
				PLC->SetIntensity(180.0f);
				PLC->SetAttenuationRadius(640.0f);
				PLC->SetCastShadows(false);
			}
		}
		// A friendly LOUNGE board (left static -- NOT registered with the recap manager, so it never disturbs the
		// recap display-index mapping the two car loops above rely on).
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(LoungeX - 250.0f, -290.0f, 178.0f), FRotator(0.0f, 0.0f, 0.0f)))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(TEXT("LOUNGE"), TEXT("Relax and stretch out before the next stop."), FLinearColor(0.95f, 0.72f, 0.42f, 1.0f));
		}
		// Moving-tunnel strips on BOTH walls (both are full-length backdropped now) so the lounge windows
		// read as the train rolling on either side, matching the rest of the cars.
		for (const float LoungeSideY : {430.0f, -430.0f})
		{
			if (ABHTrainTunnelMotionActor* Tunnel = GetWorld()->SpawnActor<ABHTrainTunnelMotionActor>(FVector(LoungeX, LoungeSideY, 165.0f), FRotator::ZeroRotator))
			{
				Tunnel->ConfigureMotion(1500.0f, 740.0f);
				if (TrainIntermissionManager) { TrainIntermissionManager->RegisterTunnelMotion(Tunnel); }
			}
		}

		// Make the lounge BENCHES + STOOLS sittable (invisible interaction volumes; the leather seating is the
		// visual). Press E to sit, Jump to stand. Benches face the aisle; stools face the centre coffee table.
		for (const float SeatBenchX : {-470.0f, 0.0f, 470.0f})
		{
			for (const float SeatWallY : {-255.0f, 255.0f})
			{
				if (ABHTrainSeat* Seat = GetWorld()->SpawnActor<ABHTrainSeat>(FVector(LoungeX + SeatBenchX, SeatWallY, 34.0f), FRotator::ZeroRotator))
				{
					Seat->ConfigureSeat(FVector(1.75f, 0.66f, 0.92f), (SeatWallY > 0.0f) ? -90.0f : 90.0f);
				}
			}
		}
		for (const float SeatStoolX : {-150.0f, 150.0f})
		{
			if (ABHTrainSeat* Seat = GetWorld()->SpawnActor<ABHTrainSeat>(FVector(LoungeX + SeatStoolX, 0.0f, 34.0f), FRotator::ZeroRotator))
			{
				Seat->ConfigureSeat(FVector(0.55f, 0.55f, 0.85f), (SeatStoolX < 0.0f) ? 0.0f : 180.0f);
			}
		}
	}
	// (The old rear-lounge -Y backdrop cap is gone -- the -Y backdrop is now full length, so it already
	// covers the rear lounge; a separate cap would only z-fight with it.)

	// Teal ceiling-cove accents, kept but DIMMED to a subtle wash: the comfortable baseline now comes from the
	// warm service-light modules below, so these just add a bit of transit-car colour without flickering harshly.
	for (int32 LightIndex = 0; LightIndex < 8; ++LightIndex)
	{
		const FVector LightLocation(-3200.0f + LightIndex * 900.0f, 0.0f, 286.0f);
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(LightLocation, FRotator::ZeroRotator))
		{
			Light->Configure(0, FLinearColor(0.42f, 0.84f, 0.78f, 1.0f), 210.0f, 560.0f);
			FlickerLights.Add(Light);
		}
	}

	// Small, frequent "service light" modules down the carriage centreline give a comfortable, flashlight-free
	// baseline ("half on, but not perfect"). Each module powers up on RED backup power, then boots to a warm
	// mains white a few seconds in (the boot + look are driven client-side inside ABHTrainServiceLight). They are
	// shadowless and short-range so a whole train of them stays cheap on classroom hardware.
	for (float ServiceLightX = TubeMinX + 230.0f; ServiceLightX <= TubeMaxX - 230.0f; ServiceLightX += 430.0f)
	{
		GetWorld()->SpawnActor<ABHTrainServiceLight>(FVector(ServiceLightX, 0.0f, 290.0f), FRotator::ZeroRotator);
	}

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Intermission);
		BHGS->SetDirectorState(RoundSeed, bFinalRecap ? TEXT("Final station recap and leaderboard.") : TEXT("Review the recap, answer bonus questions, buy upgrades, and board before departure."), NextRuntimeLevelName);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
	}
	ConvertMonitorsBackToSurvivors(TEXT("subway intermission"));
	{
		FRandomStream VariationStream(BHVariationSeedForLevel(RuntimeLevelName, RuntimeStageIndex, RoundSeed));
		AddHorrorVariationPass(VariationStream);
	}
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildTrainLobbyLevel()
{
	// The pre-game LOBBY: a longer, parked subway train so joining players can see each other and mingle while
	// they wait. Self-contained geometry (no intermission manager / no auto-advance); the round departs to the
	// first hunt level via TravelFromLobbyToFirstHunt() when the class readies up or the host force-starts.
	const FLinearColor FloorTint(0.055f, 0.065f, 0.068f, 1.0f);
	const FLinearColor WallTint(0.10f, 0.12f, 0.13f, 1.0f);
	const FLinearColor SeatTint(0.10f, 0.36f, 0.42f, 1.0f);
	const FLinearColor PoleTint(0.70f, 0.66f, 0.54f, 1.0f);
	const FLinearColor TableTint(0.18f, 0.20f, 0.22f, 1.0f);
	const FLinearColor GlassTint(0.020f, 0.032f, 0.040f, 0.55f);
	const FLinearColor TrimTint(0.34f, 0.36f, 0.33f, 1.0f);
	const FLinearColor RoofTintColor(0.085f, 0.10f, 0.105f, 1.0f);
	const FLinearColor TunnelWallTint(0.014f, 0.018f, 0.022f, 1.0f);
	const FLinearColor CoveTint(0.40f, 0.66f, 0.70f, 1.0f);
	const FLinearColor SkirtTint(0.05f, 0.055f, 0.05f, 1.0f);

	// A long lobby train (15 cars), kept symmetric about x=0 so the full-length shell pieces below stay aligned.
	// Car i centre = TubeMinX + 750 + i*1500. Cars host games up front and relax/social spaces toward the back.
	const int32 NumCars = 15;
	const float TubeMinX = -11250.0f;
	const float TubeMaxX = 11250.0f;
	const float TubeLen = (TubeMaxX - TubeMinX) / 100.0f;
	// Cabin centres for the O-key unstick (reset to the centre of the car you're in). Floor Z matches the per-car
	// SurvivorSpawns below; recorded from the tube bounds so all NumCars carriages are covered. NumCars and the
	// tube bounds are separate constants here, so assert they still agree -- if a future edit drifts one, this
	// fires loudly instead of silently leaving a car without a reset anchor.
	RecordTrainCabinCenters(TubeMinX, TubeMaxX, 124.0f);
	ensureMsgf(TrainCabinCenters.Num() == NumCars,
		TEXT("Lobby cabin-centre count (%d) != NumCars (%d); tube bounds and NumCars have drifted apart."),
		TrainCabinCenters.Num(), NumCars);
	const float WallY = 300.0f;
	const float CeilZ = 300.0f;
	const float WinBotZ = 118.0f;
	const float WinTopZ = 212.0f;
	const float WallTh = 0.18f;
	const float BackdropY = 580.0f;

	// Floor (substrate + flush finished floor) and ceiling + glowing centre cove.
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.30f)), FVector(TubeLen + 6.0f, 26.0f, 0.30f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(2.0f, 0.05f)), FVector(TubeLen, 6.1f, 0.05f), FLinearColor(0.085f, 0.098f, 0.104f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(CeilZ, 0.16f)), FVector(TubeLen, 6.2f, 0.16f), RoofTintColor, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 0.0f, CeilZ - 9.0f), FVector(TubeLen, 1.4f, 0.05f), CoveTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);

	// Sealed, glazed side walls (solid wainscot + collidable tinted glass band + solid header) with trim.
	auto AddWallRun = [&](float YPlane)
	{
		SpawnBlock(FVector(0.0f, YPlane, CenterZForBlockBottom(0.0f, WinBotZ / 100.0f)), FVector(TubeLen, WallTh, WinBotZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, YPlane, CenterZForBlockBottom(WinTopZ, (CeilZ - WinTopZ) / 100.0f)), FVector(TubeLen, WallTh, (CeilZ - WinTopZ) / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, YPlane, CenterZForBlockBottom(WinBotZ, (WinTopZ - WinBotZ) / 100.0f)), FVector(TubeLen, 0.05f, (WinTopZ - WinBotZ) / 100.0f), GlassTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted);
	};
	auto AddWallTrim = [&](float YPlane)
	{
		const float InY = YPlane > 0.0f ? YPlane - 5.0f : YPlane + 5.0f;
		SpawnBlock(FVector(0.0f, InY, WinBotZ), FVector(TubeLen, 0.05f, 0.06f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, InY, WinTopZ), FVector(TubeLen, 0.05f, 0.06f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, InY, 16.0f), FVector(TubeLen, 0.06f, 0.16f), SkirtTint, FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
		SpawnBlock(FVector(0.0f, InY, CeilZ - 7.0f), FVector(TubeLen, 0.05f, 0.07f), TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	};
	AddWallRun(WallY);
	AddWallRun(-WallY);
	AddWallTrim(WallY);
	AddWallTrim(-WallY);

	// Sealed end caps + an opaque tunnel backdrop behind BOTH window walls (full length, so nobody sees void).
	for (float EndX : {TubeMinX, TubeMaxX})
	{
		SpawnBlock(FVector(EndX, 0.0f, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, 6.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}
	SpawnBlock(FVector(0.0f, BackdropY, CenterZForBlockBottom(0.0f, 3.2f)), FVector(TubeLen, 0.2f, 3.2f), TunnelWallTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, -BackdropY, CenterZForBlockBottom(0.0f, 3.2f)), FVector(TubeLen, 0.2f, 3.2f), TunnelWallTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Concrete);

	// Gangway bulkheads between cars: full-height transverse walls with a centred open archway so players can
	// always move car to car.
	const float ArchHalf = 95.0f;
	const float ArchTopZ = 215.0f;
	for (int32 GangwayIdx = 1; GangwayIdx < NumCars; ++GangwayIdx)
	{
		const float GangwayX = TubeMinX + GangwayIdx * 1500.0f;
		const float PanelSpan = WallY - ArchHalf;
		const float PanelCtr = ArchHalf + PanelSpan * 0.5f;
		SpawnBlock(FVector(GangwayX, -PanelCtr, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, PanelSpan / 100.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, PanelCtr, CenterZForBlockBottom(0.0f, CeilZ / 100.0f)), FVector(WallTh, PanelSpan / 100.0f, CeilZ / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(GangwayX, 0.0f, CenterZForBlockBottom(ArchTopZ, (CeilZ - ArchTopZ) / 100.0f)), FVector(WallTh, (2.0f * ArchHalf) / 100.0f, (CeilZ - ArchTopZ) / 100.0f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	}

	// Each car gets its own accent colour + cove glow from these palettes so the long train doesn't read as one
	// repeated carriage. Indexed by car so neighbours always differ.
	const FLinearColor CarAccents[] = {
		FLinearColor(0.10f, 0.36f, 0.42f, 1.0f),  // teal
		FLinearColor(0.42f, 0.20f, 0.34f, 1.0f),  // plum
		FLinearColor(0.18f, 0.34f, 0.20f, 1.0f),  // moss
		FLinearColor(0.44f, 0.30f, 0.12f, 1.0f),  // amber
		FLinearColor(0.16f, 0.24f, 0.44f, 1.0f),  // indigo
		FLinearColor(0.42f, 0.16f, 0.16f, 1.0f)   // brick
	};
	const FLinearColor CarGlow[] = {
		FLinearColor(0.40f, 0.66f, 0.70f, 1.0f),
		FLinearColor(0.86f, 0.42f, 0.70f, 1.0f),
		FLinearColor(0.46f, 0.90f, 0.52f, 1.0f),
		FLinearColor(0.95f, 0.70f, 0.30f, 1.0f),
		FLinearColor(0.42f, 0.56f, 1.00f, 1.0f),
		FLinearColor(1.00f, 0.46f, 0.40f, 1.0f)
	};
	const int32 NumAccents = UE_ARRAY_COUNT(CarAccents);

	// Per-car dressing, seating, moving-tunnel strips, and spawn points.
	SurvivorSpawns.Reset();
	HunterSpawn = FVector(TubeMinX + 200.0f, 0.0f, 124.0f);
	for (int32 CarIndex = 0; CarIndex < NumCars; ++CarIndex)
	{
		const float CenterX = TubeMinX + 750.0f + CarIndex * 1500.0f;
		const FLinearColor CarAccent = CarAccents[CarIndex % NumAccents];
		const FLinearColor CarGlowTint = CarGlow[CarIndex % NumAccents];

		// Hand poles + longitudinal overhead grab rails.
		for (int32 PoleIndex = 0; PoleIndex < 2; ++PoleIndex)
		{
			const float PoleX = CenterX - 420.0f + PoleIndex * 840.0f;
			SpawnBlock(FVector(PoleX, -72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(PoleX, 72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(FVector(CenterX, -72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(CenterX, 72.0f, 248.0f), FVector(8.2f, 0.05f, 0.05f), PoleTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);

		// Wall benches both sides of every car (seating throughout), tinted in this car's accent colour.
		for (float BenchXOffset : {-360.0f, 360.0f})
		{
			SpawnBlock(FVector(CenterX + BenchXOffset, -262.0f, 70.0f), FVector(1.5f, 0.42f, 0.38f), CarAccent, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
			SpawnBlock(FVector(CenterX + BenchXOffset, 262.0f, 70.0f), FVector(1.5f, 0.42f, 0.38f), CarAccent, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		}
		// Per-car colour: a glowing accent band just above the windows on both walls, so each carriage reads as
		// its own from inside and down the aisle.
		for (float BandY : {-296.0f, 296.0f})
		{
			SpawnBlock(FVector(CenterX, BandY, 216.0f), FVector(13.0f, 0.04f, 0.07f), CarGlowTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
		}

		// Moving-tunnel strips on both sides, kept running so the lobby reads as a train heading to the first
		// stop. Not registered with any manager (the lobby has none). The loop length is set to the full car
		// pitch (1500) so each car's strips tile its entire window band edge-to-edge: a shorter loop only
		// painted the middle windows and left the panes near the gangways static (the "covers part of the
		// window sections" gap). Adjacent cars each cover their own 1500 span, so the run is gapless.
		for (float SideY : {430.0f, -430.0f})
		{
			if (ABHTrainTunnelMotionActor* Tunnel = GetWorld()->SpawnActor<ABHTrainTunnelMotionActor>(FVector(CenterX, SideY, 165.0f), FRotator::ZeroRotator))
			{
				Tunnel->ConfigureMotion(1500.0f, 520.0f);
			}
		}

		// Make the WALL BENCHES sittable: an invisible interaction volume on each bench segment, sized just over the
		// bench so the look-trace catches it before the bench's static block. Press E to sit (teleport onto the
		// bench, turn to the aisle) and Jump to stand; the bench geometry is the visual. Stays axis-aligned (facing
		// is applied to the sitter's view, not the box) so the box matches the long bench.
		for (const float SeatBenchX : {-360.0f, 360.0f})
		{
			for (const float SeatWallY : {-262.0f, 262.0f})
			{
				if (ABHTrainSeat* Seat = GetWorld()->SpawnActor<ABHTrainSeat>(FVector(CenterX + SeatBenchX, SeatWallY, 34.0f), FRotator::ZeroRotator))
				{
					Seat->ConfigureSeat(FVector(1.55f, 0.62f, 0.92f), (SeatWallY > 0.0f) ? -90.0f : 90.0f);
				}
			}
		}

		// A subway stop-request pull-cord at the car end (a social flavour toy): tap E to light the STOP REQUESTED
		// sign for the whole carriage to see (replicated); tap again to cancel. One per car on the +Y wall.
		GetWorld()->SpawnActor<ABHTrainStopCord>(FVector(CenterX + 620.0f, 292.0f, 70.0f), FRotator::ZeroRotator);

		// Spawn points along the aisle. The two GAME cars (CarIndex 2 & 4) carry a centred feature table, so skip
		// their centre spawn to avoid materialising a player on top of the table; the side spawns stay clear.
		const bool bGameCar = (CarIndex >= 1 && CarIndex <= NumCars - 2);
		if (!bGameCar)
		{
			SurvivorSpawns.Add(FVector(CenterX, 0.0f, 124.0f));
		}
		SurvivorSpawns.Add(FVector(CenterX, -200.0f, 124.0f));
		SurvivorSpawns.Add(FVector(CenterX, 200.0f, 124.0f));

		// A welcome / waiting board per car (mounted just inside the window glass on alternating walls).
		const FString BoardBody = TEXT("Welcome aboard. Mingle while we wait for everyone.\nReady up (Enter); the host departs when the class is ready.\nThe next stop loads when the train pulls out.\nStuck anywhere on the train? Press O to reset to the centre of your carriage.");
		const float BoardY = (CarIndex % 2 == 0) ? -290.0f : 290.0f;
		const FRotator BoardRot = (CarIndex % 2 == 0) ? FRotator::ZeroRotator : FRotator(0.0f, 180.0f, 0.0f);
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(CenterX, BoardY, 178.0f), BoardRot))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(TEXT("BLACKOUT TRANSIT - LOBBY"), BoardBody, FLinearColor(0.38f, 0.90f, 0.78f, 1.0f));
		}
	}

	// A large social / DECOMPRESS zone: booth seating (facing benches around a low table) through most of the
	// (now longer) carriage so groups can sit together and unwind while they wait, plus planters for a calmer
	// lounge feel. Booths sit +/-300 off each car centre, leaving the centre aisle (and the per-car spawn point)
	// clear; planters tuck near the gangway ends, clear of the wall benches at y=+/-262.
	const FLinearColor PlanterTint(0.16f, 0.18f, 0.17f, 1.0f);
	const FLinearColor FoliageTint(0.14f, 0.42f, 0.20f, 1.0f);

	// A real, PLAYABLE fast-chess table actor (5x5 minichess; solo vs AI or human PvP). Look at a square and
	// TAP to select/move, HOLD to deselect. Stools sit opposite each other so two players can face off.
	auto AddChessTable = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainChessTable>(FVector(TX, 0.0f, 0.0f), FRotator::ZeroRotator);
		SpawnBlock(FVector(TX, -150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[1], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		SpawnBlock(FVector(TX, 150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[1], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	};

	// A blackjack / card table: green felt top on a wood base, chip stacks, a couple of dealt cards, and stools.
	// The blackjack car spawns the real, PLAYABLE table actor (server-authoritative; tap = Hit/Deal, hold =
	// Stand) plus a ring of stools to gather around.
	auto AddBlackjackCar = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainBlackjackTable>(FVector(TX, 0.0f, 0.0f), FRotator::ZeroRotator);
		// Stools on both sides so several players can ring the multi-seat table.
		for (float SX : {-150.0f, 0.0f, 150.0f})
		{
			SpawnBlock(FVector(TX + SX, -165.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[3], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
			SpawnBlock(FVector(TX + SX, 165.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[3], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		}
	};

	// A tic-tac-toe table: same look-and-tap flow as chess (solo vs AI or PvP), with stools either side.
	auto AddTicTacToeTable = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainTicTacToeTable>(FVector(TX, 0.0f, 0.0f), FRotator::ZeroRotator);
		SpawnBlock(FVector(TX, -150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[2], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		SpawnBlock(FVector(TX, 150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[2], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	};

	// Connect Four: an upright board with stools either side.
	auto AddConnectFourTable = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainConnectFourTable>(FVector(TX, 0.0f, 0.0f), FRotator::ZeroRotator);
		SpawnBlock(FVector(TX, -150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[4], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		SpawnBlock(FVector(TX, 150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[4], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	};

	// Othello / Reversi table with stools either side.
	auto AddOthelloTable = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainOthelloTable>(FVector(TX, 0.0f, 0.0f), FRotator::ZeroRotator);
		SpawnBlock(FVector(TX, -150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[5], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		SpawnBlock(FVector(TX, 150.0f, 40.0f), FVector(0.42f, 0.42f, 0.40f), CarAccents[5], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
	};

	// A little casino corner: a row of slot machines players approach from the -Y aisle side.
	auto AddSlotsCar = [&](float TX)
	{
		for (float SX : {-220.0f, 0.0f, 220.0f})
		{
			GetWorld()->SpawnActor<ABHTrainSlotMachine>(FVector(TX + SX, 0.0f, 0.0f), FRotator::ZeroRotator);
		}
	};

	// Soft coloured accent light for the relax/social cars (movable so a runtime spawn lights immediately).
	auto AddAccentLight = [&](const FVector& Loc, const FLinearColor& Color, float Intensity, float Radius)
	{
		if (APointLight* L = GetWorld()->SpawnActor<APointLight>(Loc, FRotator::ZeroRotator))
		{
			L->SetMobility(EComponentMobility::Movable);
			if (UPointLightComponent* PLC = Cast<UPointLightComponent>(L->GetLightComponent()))
			{
				PLC->SetLightColor(Color);
				PLC->SetIntensity(Intensity);
				PLC->SetAttenuationRadius(Radius);
				PLC->SetCastShadows(false);
			}
		}
	};

	const FLinearColor Warm(1.0f, 0.78f, 0.5f, 1.0f);
	const FLinearColor PlantGreen(0.16f, 0.5f, 0.22f, 1.0f);
	const FLinearColor BottleA(0.2f, 0.8f, 0.5f, 1.0f);
	const FLinearColor BottleB(0.85f, 0.5f, 0.2f, 1.0f);
	const FLinearColor BookTints[5] = {
		FLinearColor(0.55f, 0.16f, 0.16f, 1.0f), FLinearColor(0.16f, 0.30f, 0.55f, 1.0f),
		FLinearColor(0.20f, 0.45f, 0.25f, 1.0f), FLinearColor(0.55f, 0.45f, 0.16f, 1.0f),
		FLinearColor(0.35f, 0.22f, 0.45f, 1.0f) };

	// ARCADE: a dartboard on the back wall plus a high-top table and a couple of slot machines.
	auto AddArcadeCar = [&](float TX)
	{
		GetWorld()->SpawnActor<ABHTrainDartboard>(FVector(TX, 250.0f, 0.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainSlotMachine>(FVector(TX - 520.0f, 240.0f, 0.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainSlotMachine>(FVector(TX + 520.0f, 240.0f, 0.0f), FRotator::ZeroRotator);
		// A throw-line stand-up table with stools on the aisle side.
		SpawnBlock(FVector(TX, -120.0f, 95.0f), FVector(0.7f, 0.7f, 0.06f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);
		SpawnBlock(FVector(TX, -120.0f, 47.0f), FVector(0.14f, 0.14f, 0.95f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
		for (float SX : {-110.0f, 110.0f})
		{
			SpawnBlock(FVector(TX + SX, -200.0f, 40.0f), FVector(0.4f, 0.4f, 0.40f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);
		}
		AddAccentLight(FVector(TX, 0.0f, 250.0f), FLinearColor(0.5f, 0.7f, 1.0f, 1.0f), 70.0f, 600.0f);
		GetWorld()->SpawnActor<ABHTrainVendingMachine>(FVector(TX - 600.0f, 250.0f, 0.0f), FRotator::ZeroRotator);
	};

	// BAR / LOUNGE: a marble-topped wood bar with stools, a back-bar of bottles, and leather booths opposite.
	auto AddBarLoungeCar = [&](float TX)
	{
		SpawnBlock(FVector(TX, 0.0f, 2.0f), FVector(14.0f, 5.6f, 0.03f), FLinearColor(0.5f, 0.3f, 0.2f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Carpet);
		// Bar counter along the +Y wall.
		SpawnBlock(FVector(TX, 235.0f, 52.0f), FVector(10.0f, 0.6f, 1.02f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
		SpawnBlock(FVector(TX, 232.0f, 106.0f), FVector(10.4f, 0.8f, 0.10f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);
		SpawnBlock(FVector(TX, 285.0f, 175.0f), FVector(10.0f, 0.25f, 1.4f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::Wood);   // back-bar shelf wall
		for (float BX = TX - 420.0f; BX <= TX + 420.0f; BX += 120.0f)
		{
			SpawnBlock(FVector(BX, 150.0f, 44.0f), FVector(0.36f, 0.36f, 0.86f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // stools
			SpawnBlock(FVector(BX, 278.0f, 150.0f + ((int)BX % 40)), FVector(0.10f, 0.10f, 0.30f), (((int)BX / 120) % 2 ? BottleA : BottleB), FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);   // bottles
		}
		// Leather booths along the -Y wall.
		for (float BX : {TX - 380.0f, TX + 380.0f})
		{
			SpawnBlock(FVector(BX, -230.0f, 40.0f), FVector(1.6f, 0.5f, 0.40f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // bench seat
			SpawnBlock(FVector(BX, -255.0f, 95.0f), FVector(1.6f, 0.18f, 0.70f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // bench back
			SpawnBlock(FVector(BX, -150.0f, 55.0f), FVector(0.8f, 0.8f, 0.06f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);       // table
		}
		AddAccentLight(FVector(TX, 120.0f, 220.0f), Warm, 90.0f, 700.0f);
		AddAccentLight(FVector(TX, -150.0f, 200.0f), Warm, 55.0f, 600.0f);
		GetWorld()->SpawnActor<ABHTrainCat>(FVector(TX, -90.0f, 6.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainFireplace>(FVector(TX, -255.0f, 0.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainVendingMachine>(FVector(TX + 640.0f, 250.0f, 0.0f), FRotator::ZeroRotator);
	};

	// LIBRARY / QUIET car: bookshelves down both walls, leather reading chairs, soft warm light.
	auto AddLibraryCar = [&](float TX)
	{
		SpawnBlock(FVector(TX, 0.0f, 2.0f), FVector(14.0f, 5.6f, 0.03f), FLinearColor(0.3f, 0.2f, 0.25f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Carpet);
		for (float WallY2 : {255.0f, -255.0f})
		{
			SpawnBlock(FVector(TX, WallY2, 120.0f), FVector(11.0f, 0.5f, 2.2f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);   // shelf body
			for (float BX = TX - 520.0f; BX <= TX + 520.0f; BX += 40.0f)
			{
				const float ShelfZ = 60.0f + 50.0f * (((int)BX / 40) % 4);
				SpawnBlock(FVector(BX, WallY2 + (WallY2 > 0 ? -26.0f : 26.0f), ShelfZ), FVector(0.34f, 0.04f, 0.34f), BookTints[((int)BX / 40) % 5], FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);   // books
			}
		}
		for (float CX : {TX - 300.0f, TX + 300.0f})
		{
			SpawnBlock(FVector(CX, 60.0f, 42.0f), FVector(0.55f, 0.55f, 0.42f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // armchair seat
			SpawnBlock(FVector(CX, 85.0f, 90.0f), FVector(0.55f, 0.16f, 0.55f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // chair back
			SpawnBlock(FVector(CX, -60.0f, 50.0f), FVector(0.5f, 0.5f, 0.06f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);       // side table
		}
		AddAccentLight(FVector(TX, 0.0f, 240.0f), FLinearColor(1.0f, 0.86f, 0.62f, 1.0f), 60.0f, 650.0f);
		GetWorld()->SpawnActor<ABHTrainCat>(FVector(TX - 150.0f, 0.0f, 6.0f), FRotator::ZeroRotator);   // a library cat
		GetWorld()->SpawnActor<ABHTrainAquarium>(FVector(TX + 560.0f, 235.0f, 0.0f), FRotator::ZeroRotator);
	};

	// OBSERVATION car: leather benches facing the windows, a glowing skylight strip, cool calm light.
	auto AddObservationCar = [&](float TX)
	{
		for (float WallY2 : {235.0f, -235.0f})
		{
			SpawnBlock(FVector(TX, WallY2, 40.0f), FVector(11.0f, 0.6f, 0.40f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);   // long window bench
		}
		SpawnBlock(FVector(TX, 0.0f, 55.0f), FVector(1.2f, 1.2f, 0.10f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Marble);           // centre table
		SpawnBlock(FVector(TX, 0.0f, 291.0f), FVector(9.0f, 1.6f, 0.04f), FLinearColor(0.6f, 0.85f, 1.0f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);   // glowing skylight
		AddAccentLight(FVector(TX, 0.0f, 250.0f), FLinearColor(0.55f, 0.75f, 1.0f, 1.0f), 65.0f, 700.0f);
		GetWorld()->SpawnActor<ABHTrainAquarium>(FVector(TX - 430.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainGhost>(FVector(TX + 300.0f, 0.0f, 6.0f), FRotator::ZeroRotator);   // easter egg: a rare passenger
		GetWorld()->SpawnActor<ABHTrainToyTrain>(FVector(TX + 480.0f, 150.0f, 0.0f), FRotator::ZeroRotator);   // a model train, inside the train
	};

	// GREENHOUSE / GARDEN: planters, a central tree, benches, grassy floor.
	auto AddGreenhouseCar = [&](float TX)
	{
		SpawnBlock(FVector(TX, 0.0f, 2.0f), FVector(14.0f, 5.6f, 0.03f), FLinearColor(0.18f, 0.40f, 0.20f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Carpet);   // lawn
		for (float WallY2 : {235.0f, -235.0f})
		{
			for (float BX = TX - 500.0f; BX <= TX + 500.0f; BX += 250.0f)
			{
				SpawnBlock(FVector(BX, WallY2, 30.0f), FVector(0.7f, 0.7f, 0.55f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);   // planter box
				SpawnBlock(FVector(BX, WallY2, 95.0f), FVector(0.9f, 0.9f, 0.8f), PlantGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);           // foliage
			}
		}
		// Central tree.
		SpawnBlock(FVector(TX, 0.0f, 70.0f), FVector(0.28f, 0.28f, 1.4f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);
		SpawnBlock(FVector(TX, 0.0f, 170.0f), FVector(1.8f, 1.8f, 1.6f), PlantGreen, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
		// Register the trunk base so the owning client can detect a player who jumps up into the canopy and gets
		// wedged ("Stuck in a Tree" egg + the O-to-reset drop). Replicated via the game state.
		if (ABHGameState* BHGS = GetGameState<ABHGameState>())
		{
			BHGS->LobbyTreeLocations.Add(FVector(TX, 0.0f, 0.0f));
		}
		for (float BX : {TX - 360.0f, TX + 360.0f})
		{
			SpawnBlock(FVector(BX, 0.0f, 40.0f), FVector(1.2f, 0.4f, 0.10f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);   // bench
		}
		AddAccentLight(FVector(TX, 0.0f, 250.0f), FLinearColor(0.85f, 1.0f, 0.8f, 1.0f), 80.0f, 700.0f);
		GetWorld()->SpawnActor<ABHTrainCat>(FVector(TX + 120.0f, 60.0f, 6.0f), FRotator::ZeroRotator);   // a garden cat
		GetWorld()->SpawnActor<ABHTrainSecretSwitch>(FVector(TX + 640.0f, -255.0f, 0.0f), FRotator::ZeroRotator);   // easter egg: tucked in the back corner
	};

	// KARAOKE / SOCIAL: a small stage with a glowing screen and mic, rows of seats, coloured stage lights.
	auto AddKaraokeCar = [&](float TX)
	{
		SpawnBlock(FVector(TX, 0.0f, 2.0f), FVector(14.0f, 5.6f, 0.03f), FLinearColor(0.25f, 0.18f, 0.35f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Carpet);
		// Stage at the +Y wall with a big screen and a mic.
		SpawnBlock(FVector(TX, 235.0f, 20.0f), FVector(5.0f, 1.0f, 0.40f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Wood);     // stage deck
		SpawnBlock(FVector(TX, 288.0f, 170.0f), FVector(3.4f, 0.12f, 1.5f), FLinearColor(0.2f, 0.5f, 1.0f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);   // screen
		SpawnBlock(FVector(TX, 210.0f, 75.0f), FVector(0.06f, 0.06f, 0.7f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);   // mic stand
		SpawnBlock(FVector(TX, 210.0f, 112.0f), FVector(0.14f, 0.14f, 0.14f), FLinearColor(0.05f, 0.05f, 0.05f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);   // mic head
		// Audience seating facing the stage.
		for (float RowY = -60.0f; RowY >= -230.0f; RowY -= 85.0f)
		{
			for (float SX = TX - 360.0f; SX <= TX + 360.0f; SX += 180.0f)
			{
				SpawnBlock(FVector(SX, RowY, 40.0f), FVector(0.42f, 0.42f, 0.40f), FLinearColor::White, FRotator::ZeroRotator, true, EBHBlockMaterial::Leather);
			}
		}
		AddAccentLight(FVector(TX - 200.0f, 150.0f, 250.0f), FLinearColor(1.0f, 0.2f, 0.7f, 1.0f), 80.0f, 600.0f);
		AddAccentLight(FVector(TX + 200.0f, 150.0f, 250.0f), FLinearColor(0.2f, 0.8f, 1.0f, 1.0f), 80.0f, 600.0f);
		GetWorld()->SpawnActor<ABHTrainDanceFloor>(FVector(TX, 90.0f, 0.0f), FRotator::ZeroRotator);
		GetWorld()->SpawnActor<ABHTrainJukebox>(FVector(TX + 330.0f, 235.0f, 0.0f), FRotator::ZeroRotator);
	};

	for (int32 LoungeCar = 1; LoungeCar <= NumCars - 2; ++LoungeCar)
	{
		const float CenterX = TubeMinX + 750.0f + LoungeCar * 1500.0f;

		// A couple of cars are GAME cars (chess, blackjack) so there's something to gather around; they get one
		// feature table down the centre instead of the twin booths so the table has room.
		// Games up front (cars 1-8), relax/social spaces toward the back (cars 9-13).
		if (LoungeCar == 1) { AddConnectFourTable(CenterX); continue; }
		if (LoungeCar == 2) { AddChessTable(CenterX); continue; }
		if (LoungeCar == 7) { AddOthelloTable(CenterX); continue; }
		if (LoungeCar == 3 || LoungeCar == 8) { AddBlackjackCar(CenterX); continue; }
		if (LoungeCar == 4) { AddTicTacToeTable(CenterX); continue; }
		if (LoungeCar == 5) { AddSlotsCar(CenterX); continue; }
		if (LoungeCar == 6) { AddArcadeCar(CenterX); continue; }
		if (LoungeCar == 9) { AddBarLoungeCar(CenterX); continue; }
		if (LoungeCar == 10) { AddLibraryCar(CenterX); continue; }
		if (LoungeCar == 11) { AddObservationCar(CenterX); continue; }
		if (LoungeCar == 12) { AddGreenhouseCar(CenterX); continue; }
		if (LoungeCar == 13) { AddKaraokeCar(CenterX); continue; }

		for (float BoothX : {CenterX - 300.0f, CenterX + 300.0f})
		{
			SpawnBlock(FVector(BoothX, 0.0f, 58.0f), FVector(0.9f, 1.2f, 0.10f), TableTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(BoothX, 0.0f, 30.0f), FVector(0.18f, 0.18f, 0.56f), TableTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(BoothX, -120.0f, 44.0f), FVector(0.95f, 0.4f, 0.30f), CarAccents[LoungeCar % NumAccents], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
			SpawnBlock(FVector(BoothX, 120.0f, 44.0f), FVector(0.95f, 0.4f, 0.30f), CarAccents[LoungeCar % NumAccents], FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		}
		for (float PlanterX : {CenterX - 620.0f, CenterX + 620.0f})
		{
			SpawnBlock(FVector(PlanterX, 200.0f, 30.0f), FVector(0.28f, 0.28f, 0.50f), PlanterTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
			SpawnBlock(FVector(PlanterX, 200.0f, 72.0f), FVector(0.5f, 0.5f, 0.5f), FoliageTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
		}
	}

	// Small, frequent warm "service light" modules down the centreline (comfortable, flashlight-free baseline
	// with the same red->mains boot as the intermission), plus dimmed teal cove accents.
	for (float ServiceLightX = TubeMinX + 230.0f; ServiceLightX <= TubeMaxX - 230.0f; ServiceLightX += 430.0f)
	{
		GetWorld()->SpawnActor<ABHTrainServiceLight>(FVector(ServiceLightX, 0.0f, 290.0f), FRotator::ZeroRotator);
	}
	for (float FlickerX = TubeMinX + 360.0f; FlickerX <= TubeMaxX - 360.0f; FlickerX += 900.0f)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(FVector(FlickerX, 0.0f, 286.0f), FRotator::ZeroRotator))
		{
			Light->Configure(0, FLinearColor(0.42f, 0.84f, 0.78f, 1.0f), 210.0f, 560.0f);
			FlickerLights.Add(Light);
		}
	}

	// Easter egg on the lobby roof too: the maintenance hatch + the hidden per-player roof-light breaker, with
	// the low safety rails so nobody wanders off (matches BuildTrainIntermissionLevel; gated by bh.EasterEggs).
	if (BHAreEasterEggsEnabled())
	{
		// Force AlwaysSpawn so the collidable roof actors are never nulled by a spawn-overlap test.
		FActorSpawnParameters RoofSpawnParams;
		RoofSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		const FVector HatchLocation(TubeMinX + 150.0f, -282.0f, 110.0f);
		if (ABHTrainRoofHatch* RoofHatch = GetWorld()->SpawnActor<ABHTrainRoofHatch>(HatchLocation, FRotator::ZeroRotator, RoofSpawnParams))
		{
			RoofHatch->ConfigureHatch(FVector(HatchLocation.X, 0.0f, 438.0f));
		}
		// A matching hatch on the roof to climb back down into the carriage (two-way roof access).
		if (ABHTrainRoofHatch* DownHatch = GetWorld()->SpawnActor<ABHTrainRoofHatch>(FVector(HatchLocation.X + 150.0f, 0.0f, 351.0f), FRotator::ZeroRotator, RoofSpawnParams))
		{
			DownHatch->ConfigureHatch(FVector(HatchLocation.X + 150.0f, -180.0f, 124.0f));
		}
		const FLinearColor RoofRailTint(0.10f, 0.11f, 0.12f, 1.0f);
		SpawnBlock(FVector(0.0f, 300.0f, 346.0f), FVector(TubeLen, 0.08f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, -300.0f, 346.0f), FVector(TubeLen, 0.08f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(TubeMaxX - 30.0f, 0.0f, 346.0f), FVector(0.08f, 6.0f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(TubeMinX + 30.0f, 0.0f, 346.0f), FVector(0.08f, 6.0f, 0.60f), RoofRailTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

		// Solid roof EAVE / overhang on both long sides: an opaque slab from just inside the safety rail
		// (y=+/-303) out to the opaque tunnel backdrop (y=+/-580). Without it, a passenger on the roof can look
		// over the low rail straight down at the moving-tunnel strips (y=+/-430) and the void behind the
		// carriage -- the "outside mechanics visible from the roof" that breaks the illusion. The eave caps
		// that gap: its top is flush with the walkable roof slab (z316) and its underside (z304) sits above the
		// window header (z212), so the strips are still fully visible through the windows from INSIDE but
		// hidden from ABOVE. Real metro cars overhang their windows the same way. Spans the full train length.
		const float EaveCenterY = 0.5f * (303.0f + BackdropY);          // midpoint between rail and backdrop
		const float EaveScaleY = (BackdropY - 303.0f + 6.0f) / 100.0f;  // +6cm overlap onto rail & backdrop
		SpawnBlock(FVector(0.0f, EaveCenterY, CenterZForBlockTop(316.0f, 0.12f)), FVector(TubeLen, EaveScaleY, 0.12f), RoofTintColor, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(0.0f, -EaveCenterY, CenterZForBlockTop(316.0f, 0.12f)), FVector(TubeLen, EaveScaleY, 0.12f), RoofTintColor, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);

		GetWorld()->SpawnActor<ABHTrainRoofBreaker>(FVector(HatchLocation.X + 2000.0f, 250.0f, 333.0f), FRotator(0.0f, 180.0f, 0.0f), RoofSpawnParams);

		// Stargazing sky, decorations, and the rooftop parkour course (shared between the intermission + lobby roofs).
		BuildTrainRoofExtras(TubeMinX, TubeMaxX, HatchLocation);
	}

	ConvertMonitorsBackToSurvivors(TEXT("train lobby"));
	// Tell clients this is the live lobby carriage (not the bare menu map) so the ride sway plays here too.
	if (ABHGameState* LobbyGS = GetGameState<ABHGameState>())
	{
		LobbyGS->SetLobbyTrainActive(true);
	}
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildTrainRoofExtras(float TubeMinX, float TubeMaxX, const FVector& HatchLocation)
{
	if (!GetWorld())
	{
		return;
	}

	const FLinearColor MetalTint(0.30f, 0.32f, 0.34f, 1.0f);
	const FLinearColor DarkMetal(0.12f, 0.13f, 0.14f, 1.0f);
	const FLinearColor SeatTint(0.10f, 0.36f, 0.42f, 1.0f);
	const float RoofTopZ = 316.0f;   // walkable roof slab top
	// Decoration is spread across the WHOLE roof span (front hatch -> back) so the rear of the train is not a
	// bare deck. The positions below are fractions of the full length, so this scales to both the 75u
	// intermission roof and the 135u lobby roof from the same code.
	const float RoofSpan = TubeMaxX - TubeMinX;
	const float SpanLo = TubeMinX + 0.06f * RoofSpan;
	const float SpanHi = TubeMaxX - 0.06f * RoofSpan;

	// Interactables (the parkour finish) carry blocking collision, so force AlwaysSpawn like the other roof actors.
	FActorSpawnParameters RoofSpawnParams;
	RoofSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// --- Stargazing sky: one starfield actor (dome of emissive stars + a warm festoon string). Centred at origin
	// so its fixed-size dome covers either carriage; built deterministically client-side (see ABHRoofStarfield).
	GetWorld()->SpawnActor<ABHRoofStarfield>(FVector(0.0f, 0.0f, RoofTopZ), FRotator::ZeroRotator);

	// --- Calm stargazing decorations (solid props; lit once a passenger throws the roof-light breaker). Kept on
	// the -Y half, clear of the centreline hatch, the +Y breaker, and the parkour run. This is the FRONT
	// stargazing deck; a second deck + industrial props fill the rear (below).
	const float DecorX = TubeMinX + 0.24f * RoofSpan;
	// Telescope: prefer the imported CC0 telescope mesh; fall back to a built-up procedural one (tripod + hub +
	// tilted tube + dew-shield + eyepiece) -- much more than the old two rectangles -- if the asset isn't present.
	const FString TelescopeMeshPath = TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_Telescope.SM_BH_Telescope");
	if (LoadObject<UStaticMesh>(nullptr, *TelescopeMeshPath))
	{
		// Imported CC0 Kenney satellite-dish (reads as a roof observatory). Kenney FBX import large, so scale it
		// down to ~2.5 m. (Scale/pivot may want a small eyeball tweak -- it's this one number.)
		SpawnRuntimeMeshProp(FVector(DecorX, -150.0f, RoofTopZ), FRotator(0.0f, 180.0f, 0.0f), TelescopeMeshPath, FString(),
			FVector(0.04f, 0.04f, 0.04f), true, FVector(0.3f, 0.3f, 1.4f), MetalTint, EBHBlockMaterial::PaintedMetal, true);
	}
	else
	{
		const FVector ScopeBase(DecorX, -150.0f, RoofTopZ);
		const float HubZ = RoofTopZ + 100.0f;
		for (int32 Leg = 0; Leg < 3; ++Leg)
		{
			const float LegAngle = FMath::DegreesToRadians(90.0f + Leg * 120.0f);
			SpawnBlock(FVector(ScopeBase.X + 26.0f * FMath::Cos(LegAngle), ScopeBase.Y + 26.0f * FMath::Sin(LegAngle), RoofTopZ + 48.0f),
				FVector(0.06f, 0.06f, 1.05f), DarkMetal, FRotator(16.0f, FMath::RadiansToDegrees(LegAngle), 0.0f), true, EBHBlockMaterial::PaintedMetal);
		}
		SpawnBlock(FVector(ScopeBase.X, ScopeBase.Y, HubZ), FVector(0.22f, 0.22f, 0.22f), MetalTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		// Tilted optical tube (pitched up at the sky) + a wider dew-shield at the front and an eyepiece nub at the back.
		const FRotator TubeRot(42.0f, 0.0f, 0.0f);
		const FVector TubeAxis(FMath::Cos(FMath::DegreesToRadians(42.0f)), 0.0f, FMath::Sin(FMath::DegreesToRadians(42.0f)));
		const FVector TubeCenter(ScopeBase.X, ScopeBase.Y, HubZ + 28.0f);
		SpawnBlock(TubeCenter, FVector(0.9f, 0.22f, 0.22f), MetalTint, TubeRot, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(TubeCenter + TubeAxis * 48.0f, FVector(0.14f, 0.30f, 0.30f), DarkMetal, TubeRot, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(TubeCenter - TubeAxis * 46.0f, FVector(0.14f, 0.10f, 0.10f), DarkMetal, TubeRot, false, EBHBlockMaterial::RustedMetal);
	}
	// Stargazing seating: imported CC0 Kenney stools (fall back to a bench block if the mesh is absent). Kenney
	// FBX import large, so scaled down; base-pivot assumed so they sit on the roof (small z tweak if needed).
	const FString StoolPath = TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofStool.SM_BH_RoofStool");
	SpawnRuntimeMeshProp(FVector(DecorX - 240.0f, -215.0f, RoofTopZ), FRotator(0.0f, -90.0f, 0.0f), StoolPath, FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.55f, 0.55f, 0.45f), SeatTint, EBHBlockMaterial::Tiles, true);
	SpawnRuntimeMeshProp(FVector(DecorX + 30.0f, -240.0f, RoofTopZ), FRotator(0.0f, -90.0f, 0.0f), StoolPath, FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.55f, 0.55f, 0.45f), SeatTint, EBHBlockMaterial::Tiles, true);
	SpawnRuntimeMeshProp(FVector(DecorX + 290.0f, -215.0f, RoofTopZ), FRotator(0.0f, -90.0f, 0.0f), StoolPath, FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.55f, 0.55f, 0.45f), SeatTint, EBHBlockMaterial::Tiles, true);
	// Imported CC0 antenna/wireless dish in the back corner (fall back to a mast block).
	const float AntennaX = TubeMinX + 0.84f * RoofSpan;
	SpawnRuntimeMeshProp(FVector(AntennaX, 215.0f, RoofTopZ), FRotator::ZeroRotator, TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofAntenna.SM_BH_RoofAntenna"), FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.06f, 0.06f, 2.6f), DarkMetal, EBHBlockMaterial::PaintedMetal, true);
	// Imported CC0 space rocks + a meteor as stargazing scenery (fall back to plain rock blocks).
	SpawnRuntimeMeshProp(FVector(DecorX - 540.0f, -240.0f, RoofTopZ), FRotator(0.0f, 35.0f, 0.0f), TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofRocks.SM_BH_RoofRocks"), FString(), FVector(0.03f, 0.03f, 0.03f), true, FVector(0.7f, 0.7f, 0.5f), DarkMetal, EBHBlockMaterial::Concrete, true);
	SpawnRuntimeMeshProp(FVector(DecorX + 560.0f, -250.0f, RoofTopZ), FRotator(0.0f, 120.0f, 0.0f), TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofMeteor.SM_BH_RoofMeteor"), FString(), FVector(0.027f, 0.027f, 0.027f), true, FVector(0.7f, 0.7f, 0.6f), DarkMetal, EBHBlockMaterial::Concrete, true);
	// Industrial flavour distributed along the FULL roof so the rear isn't bare: AC units and low vents marching
	// front-to-back on alternating sides (clear of the centre walk/obby), every ~16% of the length.
	int32 VentIndex = 0;
	for (float VentX = SpanLo; VentX <= SpanHi + 1.0f; VentX += 0.16f * RoofSpan)
	{
		const float SideY = (VentIndex % 2 == 0) ? 235.0f : -235.0f;
		if (VentIndex % 2 == 0)
		{
			SpawnBlock(FVector(VentX, SideY, CenterZForBlockBottom(RoofTopZ, 0.9f)), FVector(1.4f, 1.1f, 0.9f), MetalTint, FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);
		}
		else
		{
			SpawnBlock(FVector(VentX, SideY, CenterZForBlockBottom(RoofTopZ, 0.35f)), FVector(0.8f, 0.8f, 0.35f), MetalTint, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		}
		++VentIndex;
	}
	// Rear "stargazing deck #2": a second seating cluster + scenery so the back third of the roof is a place to
	// be, not an empty slab. Reuses the same imported props (grounded), with procedural fallbacks.
	const float BackDeckX = TubeMinX + 0.72f * RoofSpan;
	SpawnRuntimeMeshProp(FVector(BackDeckX - 120.0f, -210.0f, RoofTopZ), FRotator(0.0f, -90.0f, 0.0f), StoolPath, FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.55f, 0.55f, 0.45f), SeatTint, EBHBlockMaterial::Tiles, true);
	SpawnRuntimeMeshProp(FVector(BackDeckX + 170.0f, -235.0f, RoofTopZ), FRotator(0.0f, -90.0f, 0.0f), StoolPath, FString(), FVector(0.05f, 0.05f, 0.05f), true, FVector(0.55f, 0.55f, 0.45f), SeatTint, EBHBlockMaterial::Tiles, true);
	SpawnRuntimeMeshProp(FVector(BackDeckX - 380.0f, -250.0f, RoofTopZ), FRotator(0.0f, 200.0f, 0.0f), TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofRocks.SM_BH_RoofRocks"), FString(), FVector(0.03f, 0.03f, 0.03f), true, FVector(0.7f, 0.7f, 0.5f), DarkMetal, EBHBlockMaterial::Concrete, true);
	SpawnRuntimeMeshProp(FVector(BackDeckX + 430.0f, -245.0f, RoofTopZ), FRotator(0.0f, 60.0f, 0.0f), TEXT("/Game/BlackoutHunt/Art/Roof/SM_BH_RoofMeteor.SM_BH_RoofMeteor"), FString(), FVector(0.027f, 0.027f, 0.027f), true, FVector(0.7f, 0.7f, 0.6f), DarkMetal, EBHBlockMaterial::Concrete, true);

	// --- "Tower of Doom" obby: a neon spiral of small platforms climbing high above the roof, finishing in a
	// glowing gate at the very top. Gaps/rises are kept forgiving (small hops, ~55cm rise, ~130cm reach) so it's
	// completable, but the top is unreachable without the climb so the finish can't be cheesed. (Jump feel may
	// want a playtest pass.)
	const FVector TowerCenter(TubeMinX + 0.5f * RoofSpan, 0.0f, RoofTopZ);   // climb from the middle of the roof
	const FLinearColor ObbyColors[] = {
		FLinearColor(1.00f, 0.25f, 0.30f, 1.0f), FLinearColor(1.00f, 0.65f, 0.15f, 1.0f),
		FLinearColor(0.30f, 0.85f, 1.00f, 1.0f), FLinearColor(0.45f, 1.00f, 0.40f, 1.0f),
		FLinearColor(0.80f, 0.45f, 1.00f, 1.0f), FLinearColor(1.00f, 0.92f, 0.30f, 1.0f)
	};
	const int32 ObbyPlatforms = 18;
	const float ObbyRadius = 130.0f;
	const float ObbyRise = 55.0f;
	const float ObbyStepDeg = 60.0f;
	// A slim central mast for the tower silhouette (non-colliding so it never blocks a jump).
	const float MastHeight = (ObbyPlatforms * ObbyRise + 160.0f) / 100.0f;
	SpawnBlock(FVector(TowerCenter.X, TowerCenter.Y, CenterZForBlockBottom(RoofTopZ, MastHeight)), FVector(0.16f, 0.16f, MastHeight), DarkMetal, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	float ObbyTopZ = RoofTopZ;
	float FinalX = TowerCenter.X;
	float FinalY = TowerCenter.Y;
	for (int32 Step = 0; Step < ObbyPlatforms; ++Step)
	{
		const float AngleRad = FMath::DegreesToRadians(Step * ObbyStepDeg);
		const float Px = TowerCenter.X + ObbyRadius * FMath::Cos(AngleRad);
		const float Py = TowerCenter.Y + ObbyRadius * FMath::Sin(AngleRad);
		const float TopZ = RoofTopZ + 45.0f + Step * ObbyRise;
		ObbyTopZ = TopZ;
		FinalX = Px;
		FinalY = Py;
		// Every sixth platform is a larger "rest" pad to catch your breath each lap.
		const float PadScale = (Step % 6 == 5) ? 1.6f : 1.0f;
		SpawnBlock(FVector(Px, Py, CenterZForBlockTop(TopZ, 0.18f)), FVector(PadScale, PadScale, 0.18f), ObbyColors[Step % UE_ARRAY_COUNT(ObbyColors)], FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
	}
	// Finish gate on the top platform (its base sits on that platform's top).
	GetWorld()->SpawnActor<ABHTrainRoofParkourGate>(FVector(FinalX, FinalY, ObbyTopZ + 70.0f), FRotator::ZeroRotator, RoofSpawnParams);
}

void ABHGameMode::AddFoggroundsMoodPass()
{
	switch (RuntimeFogPreset)
	{
	case EBHFogPreset::Light:
		AddMoodPass(FLinearColor(0.155f, 0.205f, 0.220f, 1.0f), 0.210f, 0.30f, 0.035f);
		break;
	case EBHFogPreset::Extreme:
		AddMoodPass(FLinearColor(0.245f, 0.335f, 0.355f, 1.0f), 0.620f, 0.38f, 0.055f);
		break;
	case EBHFogPreset::Heavy:
	default:
		AddMoodPass(FLinearColor(0.255f, 0.355f, 0.380f, 1.0f), 0.480f, 0.40f, 0.060f);
		break;
	}

	AddFoggroundsModeledFog();
}

void ABHGameMode::AddFoggroundsLightingPass()
{
	const bool bExtremeFog = RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = RuntimeFogPreset == EBHFogPreset::Heavy;
	const float SkyBrightness = bExtremeFog ? 1.22f : (bHeavyFog ? 1.10f : 0.88f);
	const FLinearColor MoonTint(0.68f, 0.80f, 0.98f, 1.0f);
	const FLinearColor SkyTint(0.38f, 0.50f, 0.70f, 1.0f);

	if (ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		SkyAtmosphere->SetReplicates(true);
		SkyAtmosphere->SetReplicateMovement(false);
		SkyAtmosphere->bAlwaysRelevant = true;
		if (USkyAtmosphereComponent* SkyComponent = SkyAtmosphere->GetComponent())
		{
			SkyComponent->SetSkyLuminanceFactor(FLinearColor(SkyBrightness * 0.78f, SkyBrightness * 0.92f, SkyBrightness * 1.12f, 1.0f));
			SkyComponent->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(SkyBrightness * 0.72f, SkyBrightness * 0.86f, SkyBrightness, 1.0f));
			SkyComponent->SetGroundAlbedo(FColor(44, 54, 64));
			SkyComponent->SetMultiScatteringFactor(0.78f);
			SkyComponent->SetHeightFogContribution(0.18f);
			SkyComponent->SetAerialPespectiveViewDistanceScale(0.34f);
		}
	}

	if (ADirectionalLight* MoonLight = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-38.0f, 132.0f, 0.0f)))
	{
		MoonLight->SetReplicates(true);
		MoonLight->SetReplicateMovement(false);
		MoonLight->bAlwaysRelevant = true;
		MoonLight->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* LightComponent = Cast<UDirectionalLightComponent>(MoonLight->GetLightComponent()))
		{
			LightComponent->SetIntensity(bExtremeFog ? 1.60f : (bHeavyFog ? 1.42f : 1.02f));
			LightComponent->SetLightColor(MoonTint);
			LightComponent->SetIndirectLightingIntensity(bExtremeFog ? 1.12f : (bHeavyFog ? 0.98f : 0.62f));
			// Eerie moonlight: strong volumetric scattering makes the moonbeam shaft visibly through the
			// thick volumetric fog (shadows stay off to protect low-end/classroom perf; the fog's
			// directional inscattering + a brighter moon disk give the shaft/visible-moon read).
			LightComponent->SetVolumetricScatteringIntensity(0.85f);
			LightComponent->SetCastShadows(false);
			LightComponent->SetAtmosphereSunLight(true);
			LightComponent->SetAtmosphereSunLightIndex(0);
			LightComponent->SetAtmosphereSunDiskColorScale(FLinearColor(0.72f, 0.84f, 1.08f, 1.0f));
		}
	}

	if (ASkyLight* SkyLight = GetWorld()->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		SkyLight->SetReplicates(true);
		SkyLight->SetReplicateMovement(false);
		SkyLight->bAlwaysRelevant = true;
		if (USkyLightComponent* SkyLightComponent = SkyLight->GetLightComponent())
		{
			SkyLightComponent->SetMobility(EComponentMobility::Movable);
			SkyLightComponent->SourceType = SLS_CapturedScene;
			SkyLightComponent->SetIntensity(bExtremeFog ? 1.12f : (bHeavyFog ? 0.98f : 0.68f));
			SkyLightComponent->SetLightColor(SkyTint);
			SkyLightComponent->SetLowerHemisphereColor(FLinearColor(0.120f, 0.150f, 0.170f, 1.0f));
			SkyLightComponent->SetVolumetricScatteringIntensity(0.25f);
			SkyLightComponent->SetRealTimeCaptureEnabled(true);
			SkyLightComponent->SetCaptureIsDirty();
		}
	}
}

void ABHGameMode::AddFoggroundsModeledFog()
{
	// Intentionally empty. Foggrounds fog is now produced solely by the uniform global exponential
	// height fog configured in AddMoodPass: it is horizontally constant across the whole map and,
	// with volumetric fog forced on for every graphics tier (see ABHPlayerController::
	// ApplyAdaptiveGraphicsState), renders identically on all hardware. The previous per-spot
	// ALocalFogVolume patches were removed because they made fog density uneven across the map and
	// only rendered when r.VolumetricFog was enabled (off on Low/Medium), so they vanished there.
	// Kept as a hook in case hand-placed fog accents are reintroduced later.
}

void ABHGameMode::BuildSubstationLevel()
{
	// Clean spawn muster: a tidy 3x4 grid in the foyer just WEST of the platform, clear of the station furniture
	// (pillars/benches sit further east on the platform), but still right by the locked exit. Replaces the old
	// layout where survivors spawned scattered on top of the platform itself.
	SurvivorSpawns = {
		FVector(4150.0f, -1300.0f, 120.0f), FVector(4400.0f, -1300.0f, 120.0f), FVector(4650.0f, -1300.0f, 120.0f),
		FVector(4150.0f, -450.0f, 120.0f), FVector(4400.0f, -450.0f, 120.0f), FVector(4650.0f, -450.0f, 120.0f),
		FVector(4150.0f, 450.0f, 120.0f), FVector(4400.0f, 450.0f, 120.0f), FVector(4650.0f, 450.0f, 120.0f),
		FVector(4150.0f, 1300.0f, 120.0f), FVector(4400.0f, 1300.0f, 120.0f), FVector(4650.0f, 1300.0f, 120.0f)
	};
	HunterSpawn = FVector(-5650.0f, -1250.0f, 120.0f);

	const FLinearColor Concrete(0.11f, 0.12f, 0.125f, 1.0f);
	const FLinearColor Ceiling(0.045f, 0.050f, 0.055f, 1.0f);
	const FLinearColor Wall(0.21f, 0.24f, 0.25f, 1.0f);
	const FLinearColor Steel(0.20f, 0.22f, 0.23f, 1.0f);
	const FLinearColor Rust(0.34f, 0.16f, 0.08f, 1.0f);
	const FLinearColor Cable(0.02f, 0.025f, 0.028f, 1.0f);
	const FLinearColor Warning(0.85f, 0.58f, 0.08f, 1.0f);
	const FLinearColor ControlBlue(0.08f, 0.22f, 0.30f, 1.0f);
	const FLinearColor SickGreen(0.08f, 0.26f, 0.14f, 1.0f);

	// World-aligned (triplanar) concrete/plaster so the big floor/ceiling slabs and the tall interior walls read
	// as real textured concrete instead of stretched smears. ConcreteWACool keeps the substation's cold palette.
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(122.0f, 92.0f, 0.25f), Concrete, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(350.0f, 0.12f)), FVector(122.0f, 92.0f, 0.12f), Ceiling, FRotator::ZeroRotator, true, EBHBlockMaterial::PlasterWA);

	SpawnBlock(FVector(0.0f, -4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
	SpawnBlock(FVector(0.0f, 4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
	SpawnBlock(FVector(-6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
	SpawnBlock(FVector(6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
	AddMapContainment(6100.0f, 4600.0f);

	// ---- FLOW REDESIGN (Tools/MapAnalysis/layout_substation.py, verified by bh_mapcheck -> substation_after.png):
	// 5 vertical ribs + 2 horizontal band walls, with an OPEN Y=0 spine running the whole map width so the 'Power'
	// compass bearing matches a real run. Every room has >=2 wide arches (loops, no dead-ends); the west cable
	// gallery (X<-4500) stays open top-to-bottom. Breakers are PRE-SORTED near->far from spawn so the first power
	// beat is a fair, close objective (NOT the far hunter-den corner). 0 unreachable objectives (was: beat #0 in
	// the hunter's spawn room across the map). Crawls are hall<->band flanks offset from the centre doors. ----
	const TArray<TPair<FVector, FVector>> Walls = {
		{FVector(-4500.0f, -4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)}, {FVector(-4500.0f, -2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-4500.0f, -865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-4500.0f, 865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-4500.0f, 2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-4500.0f, 4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)},
		{FVector(-2500.0f, -4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)}, {FVector(-2500.0f, -2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-2500.0f, -865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-2500.0f, 865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-2500.0f, 2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-2500.0f, 4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)},
		{FVector(-500.0f, -4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)}, {FVector(-500.0f, -2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-500.0f, -865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-500.0f, 865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(-500.0f, 2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(-500.0f, 4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)},
		{FVector(1800.0f, -4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)}, {FVector(1800.0f, -2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(1800.0f, -865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(1800.0f, 865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(1800.0f, 2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(1800.0f, 4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)},
		{FVector(3900.0f, -4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)}, {FVector(3900.0f, -2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(3900.0f, -865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(3900.0f, 865.0f, 175.0f), FVector(0.3f, 12.1f, 3.25f)}, {FVector(3900.0f, 2550.0f, 175.0f), FVector(0.3f, 12.4f, 3.25f)}, {FVector(3900.0f, 4065.0f, 175.0f), FVector(0.3f, 8.7f, 3.25f)},
		{FVector(-4115.0f, -2500.0f, 175.0f), FVector(7.7f, 0.3f, 3.25f)}, {FVector(-2745.0f, -2500.0f, 175.0f), FVector(10.5f, 0.3f, 3.25f)}, {FVector(-1855.0f, -2500.0f, 175.0f), FVector(2.5f, 0.3f, 3.25f)}, {FVector(-425.0f, -2500.0f, 175.0f), FVector(16.9f, 0.3f, 3.25f)}, {FVector(1530.0f, -2500.0f, 175.0f), FVector(13.0f, 0.3f, 3.25f)}, {FVector(2520.0f, -2500.0f, 175.0f), FVector(2.0f, 0.3f, 3.25f)}, {FVector(3810.0f, -2500.0f, 175.0f), FVector(14.6f, 0.3f, 3.25f)}, {FVector(5580.0f, -2500.0f, 175.0f), FVector(10.4f, 0.3f, 3.25f)},
		{FVector(-4115.0f, 2500.0f, 175.0f), FVector(7.7f, 0.3f, 3.25f)}, {FVector(-2745.0f, 2500.0f, 175.0f), FVector(10.5f, 0.3f, 3.25f)}, {FVector(-1855.0f, 2500.0f, 175.0f), FVector(2.5f, 0.3f, 3.25f)}, {FVector(-425.0f, 2500.0f, 175.0f), FVector(16.9f, 0.3f, 3.25f)}, {FVector(1030.0f, 2500.0f, 175.0f), FVector(3.0f, 0.3f, 3.25f)}, {FVector(1800.0f, 2500.0f, 175.0f), FVector(7.6f, 0.3f, 3.25f)}, {FVector(2520.0f, 2500.0f, 175.0f), FVector(2.0f, 0.3f, 3.25f)}, {FVector(3810.0f, 2500.0f, 175.0f), FVector(14.6f, 0.3f, 3.25f)}, {FVector(5580.0f, 2500.0f, 175.0f), FVector(10.4f, 0.3f, 3.25f)}
	};
	// Dress every interior wall so it doesn't read as one flat grey slab: a tiled wainscot (lower band, breaks the
	// concrete with a different texture), a painted accent stripe that alternates amber/teal for colour variety,
	// and a metal cornice line near the top. All cosmetic (proud of the wall, non-colliding) on both faces.
	const FLinearColor WainscotTint(0.16f, 0.185f, 0.20f, 1.0f);
	const FLinearColor TrimTint(0.26f, 0.28f, 0.29f, 1.0f);
	auto DressWall = [&](float cx, float cy, const FVector& Scale, int32 Index)
	{
		const bool bAlongX = Scale.X >= Scale.Y;
		const FLinearColor Accent = (Index % 2 == 0) ? Warning : ControlBlue;
		for (float Face : {-1.0f, 1.0f})
		{
			const float Ox = bAlongX ? 0.0f : Face * 18.0f;
			const float Oy = bAlongX ? Face * 18.0f : 0.0f;
			const FVector WsScale = bAlongX ? FVector(Scale.X, 0.04f, 1.30f) : FVector(0.04f, Scale.Y, 1.30f);
			const FVector StScale = bAlongX ? FVector(Scale.X, 0.03f, 0.10f) : FVector(0.03f, Scale.Y, 0.10f);
			const FVector CnScale = bAlongX ? FVector(Scale.X, 0.05f, 0.10f) : FVector(0.05f, Scale.Y, 0.10f);
			SpawnBlock(FVector(cx + Ox, cy + Oy, CenterZForBlockBottom(0.0f, 1.30f)), WsScale, WainscotTint, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
			SpawnBlock(FVector(cx + Ox * 1.2f, cy + Oy * 1.2f, CenterZForBlockBottom(150.0f, 0.10f)), StScale, Accent, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(cx + Ox * 1.2f, cy + Oy * 1.2f, CenterZForBlockBottom(298.0f, 0.10f)), CnScale, TrimTint, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
		}
	};
	int32 WallIdx = 0;
	for (const TPair<FVector, FVector>& WallSpec : Walls)
	{
		SpawnBlock(FVector(WallSpec.Key.X, WallSpec.Key.Y, CenterZForBlockBottom(0.0f, WallSpec.Value.Z)), WallSpec.Value, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
		DressWall(WallSpec.Key.X, WallSpec.Key.Y, WallSpec.Value, WallIdx++);
	}

	// Freestanding half-height cover so the opened-up flanking rooms and hall aren't a bare field (anti-chasey):
	// short crate/cabinet stacks that block a sightline but not a route. Non-grid, placed clear of doorways.
	const TArray<TPair<FVector, FVector>> CoverBlocks = {
		{FVector(-3500.0f, -1900.0f, 0.0f), FVector(2.2f, 0.9f, 1.55f)}, {FVector(-3500.0f, 1900.0f, 0.0f), FVector(2.2f, 0.9f, 1.55f)},
		{FVector(2850.0f, -1900.0f, 0.0f), FVector(2.2f, 0.9f, 1.55f)}, {FVector(2850.0f, 1900.0f, 0.0f), FVector(2.2f, 0.9f, 1.55f)},
		{FVector(-1500.0f, -700.0f, 0.0f), FVector(0.9f, 2.4f, 1.55f)}, {FVector(1100.0f, 700.0f, 0.0f), FVector(0.9f, 2.4f, 1.55f)}
	};
	for (const TPair<FVector, FVector>& CoverSpec : CoverBlocks)
	{
		SpawnBlock(FVector(CoverSpec.Key.X, CoverSpec.Key.Y, CenterZForBlockBottom(0.0f, CoverSpec.Value.Z)), CoverSpec.Value, Steel, FRotator(0.0f, 8.0f * WallIdx++, 0.0f), true, EBHBlockMaterial::DiamondPlate);
	}

	// Doors at the narrow openings (the 3 wider shutter passages are spawned separately below). Vertical-wall
	// doorways use ZeroRotator; horizontal-wall doorways face 90 deg -- matching positions verified in the model.
	const TArray<TPair<FVector, FRotator>> Doors = {
		{FVector(-3500.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-1500.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(650.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(2850.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(4800.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-3500.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-1500.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(650.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(2850.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(4800.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Door : Doors)
	{
		if (ABHDoor* DoorActor = GetWorld()->SpawnActor<ABHDoor>(Door.Key, Door.Value))
		{
			DoorActors.Add(DoorActor);
		}
	}

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(2850.0f, 1900.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(2850.0f, -1900.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-350.0f, 1900.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-350.0f, -1900.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-1500.0f, 3400.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-1500.0f, -3400.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-5650.0f, 3400.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}, {FVector(-5650.0f, -3400.0f, 80.0f), FRotator(0.0f, 0.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-5650.0f, 0.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-5650.0f, -1700.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-5650.0f, 1700.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-3500.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-3500.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3500.0f, -3500.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-3500.0f, 3500.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-1200.0f, 0.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(1100.0f, 0.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-350.0f, -1900.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-350.0f, 1900.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(2850.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(2850.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(750.0f, -3500.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-1500.0f, 3500.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(4250.0f, 0.0f, 95.0f), EBHObjectiveStationType::Evidence}
	};
	for (const TPair<FVector, EBHObjectiveStationType>& Spec : ObjectiveStationSpecs)
	{
		if (ABHObjectiveStation* Station = GetWorld()->SpawnActor<ABHObjectiveStation>(Spec.Key, FRotator::ZeroRotator))
		{
			Station->Configure(Spec.Value);
			ObjectiveStations.Add(Station);
		}
	}

	const FLinearColor RevisionCyan(0.12f, 0.68f, 0.86f, 1.0f);
	const FLinearColor RevisionAmber(0.95f, 0.68f, 0.12f, 1.0f);
	const FLinearColor RevisionViolet(0.58f, 0.32f, 0.86f, 1.0f);
	const FVector RevisionLaneCenters[] = {
		FVector(-1200.0f, 0.0f, CenterZForBlockBottom(0.65f, 0.035f)),
		FVector(1100.0f, 0.0f, CenterZForBlockBottom(0.70f, 0.035f)),
		FVector(-350.0f, -1900.0f, CenterZForBlockBottom(0.75f, 0.035f)),
		FVector(-350.0f, 1900.0f, CenterZForBlockBottom(0.80f, 0.035f))
	};
	for (int32 LaneIndex = 0; LaneIndex < UE_ARRAY_COUNT(RevisionLaneCenters); ++LaneIndex)
	{
		const FVector Lane = RevisionLaneCenters[LaneIndex];
		const FLinearColor LaneTint = (LaneIndex % 3 == 0) ? RevisionCyan : ((LaneIndex % 3 == 1) ? RevisionAmber : RevisionViolet);
		SpawnBlock(Lane, FVector(4.8f, 2.7f, 0.035f), LaneTint, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(Lane + FVector(0.0f, -185.0f, 1.0f), FVector(4.4f, 0.08f, 0.045f), RevisionAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(Lane + FVector(0.0f, 185.0f, 1.0f), FVector(4.4f, 0.08f, 0.045f), RevisionAmber, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
	}
	if (ABHObjectiveStation* HiddenSwitch = GetWorld()->SpawnActor<ABHObjectiveStation>(FVector(-5750.0f, 3825.0f, 88.0f), FRotator(0.0f, -35.0f, 0.0f)))
	{
		HiddenSwitch->ConfigureTeacherMirrorTrapNode();
	}

	// Central transformer hall: 8 banks in two rows flanking a clear central cross (a long E-W sightline stays
	// open at |Y|<900, with N-S gaps between banks) -- replaces the old 17-box checkerboard that maze-walled the
	// middle and made the map feel small. The banks are the hall's primary cover.
	const float TransformerRowY[] = { -1450.0f, 1450.0f };
	const float TransformerColX[] = { -2000.0f, -900.0f, 300.0f, 1400.0f };
	for (float TY : TransformerRowY)
	{
		for (int32 Ci = 0; Ci < UE_ARRAY_COUNT(TransformerColX); ++Ci)
		{
			const float TX = TransformerColX[Ci];
			SpawnBlock(FVector(TX, TY, CenterZForBlockBottom(0.0f, 1.9f)), FVector(1.4f, 3.0f, 1.9f), Steel, FRotator(0.0f, 6.0f * Ci, 0.0f), true, EBHBlockMaterial::DiamondPlate);
			SpawnBlock(FVector(TX + 200.0f, TY, 205.0f), FVector(0.30f, 3.2f, 0.18f), Rust, FRotator(0.0f, 6.0f * Ci, 0.0f), false, EBHBlockMaterial::RustedMetal);
		}
	}

	for (int32 I = 0; I < 13; ++I)
	{
		const float X = -5700.0f + I * 950.0f;
		SpawnBlock(FVector(X, -4300.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(4.8f, 0.08f, 0.08f), Warning, FRotator::ZeroRotator, false);
		SpawnBlock(FVector(X + 260.0f, -4300.0f, CenterZForBlockBottom(0.5f, 0.09f)), FVector(1.8f, 0.09f, 0.09f), Cable, FRotator(0.0f, 35.0f, 0.0f), false);
		SpawnBlock(FVector(X, 4300.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(4.8f, 0.08f, 0.08f), Warning, FRotator::ZeroRotator, false);
		SpawnBlock(FVector(X + 260.0f, 4300.0f, CenterZForBlockBottom(0.5f, 0.09f)), FVector(1.8f, 0.09f, 0.09f), Cable, FRotator(0.0f, -35.0f, 0.0f), false);
	}

	// 10 set-dressing props (down from 16), spread through the outer bands + hall, clear of objectives/breakers
	// /transformers/doorways so they read as flavour, not obstacles.
	const TArray<FVector> ClutterCenters = {
		FVector(-4400.0f, -3550.0f, 40.0f), FVector(-2400.0f, -3550.0f, 40.0f), FVector(300.0f, -3550.0f, 40.0f), FVector(2100.0f, -3550.0f, 40.0f),
		FVector(-4400.0f, 3550.0f, 40.0f), FVector(-2400.0f, 3550.0f, 40.0f), FVector(300.0f, 3550.0f, 40.0f), FVector(2100.0f, 3550.0f, 40.0f),
		FVector(-1500.0f, 1900.0f, 40.0f), FVector(1100.0f, -1900.0f, 40.0f)
	};
	AddIndustrialClutter(ClutterCenters, Rust);

	const TArray<FVector> Lockers = {
		FVector(-5850.0f, -2400.0f, 100.0f), FVector(-5850.0f, -1000.0f, 100.0f), FVector(-5850.0f, 1000.0f, 100.0f), FVector(-5850.0f, 2400.0f, 100.0f),
		FVector(-2200.0f, 0.0f, 100.0f), FVector(1600.0f, 0.0f, 100.0f),
		FVector(-3500.0f, -2300.0f, 100.0f), FVector(-3500.0f, 2300.0f, 100.0f), FVector(2850.0f, -2300.0f, 100.0f), FVector(2850.0f, 2300.0f, 100.0f),
		FVector(-700.0f, -3550.0f, 100.0f), FVector(-1000.0f, 3550.0f, 100.0f), FVector(1300.0f, 3550.0f, 100.0f), FVector(-700.0f, 3550.0f, 100.0f),
		FVector(4100.0f, -1500.0f, 100.0f), FVector(4100.0f, 1500.0f, 100.0f)
	};
	for (const FVector& Location : Lockers)
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	// ---- Hiding pass (mid map = hide-and-move): prone-only crawl tunnels that punch THROUGH interior dividers to
	// connect two rooms -- a survivor-only shortcut (the Teacher is ejected and a sheltering prone survivor is
	// uncapturable), so the hunter must loop to a door while the hider slips across or jukes between the two
	// mouths. Plus squeeze pockets (turn-sideways auto-squeeze) and glowing blue control panels as accents. ----
	const FLinearColor DuctTint(0.16f, 0.18f, 0.19f, 1.0f);
	const FLinearColor PanelBlue(0.20f, 0.55f, 1.0f, 1.0f);

	auto SpawnCrawlDuct = [&](float cx, float cy, float yaw, float lenScale)
	{
		const float Rad = FMath::DegreesToRadians(yaw);
		const float Cs = FMath::Cos(Rad);
		const float Sn = FMath::Sin(Rad);
		auto LP = [&](float lx, float ly, float z) { return FVector(cx + lx * Cs - ly * Sn, cy + lx * Sn + ly * Cs, z); };
		const FRotator Rot(0.0f, yaw, 0.0f);
		const float SideH = 1.55f;
		SpawnBlock(LP(0.0f, -100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), DuctTint, Rot, true, EBHBlockMaterial::ConcreteWACool);
		SpawnBlock(LP(0.0f, 100.0f, CenterZForBlockBottom(0.0f, SideH)), FVector(lenScale, 0.12f, SideH), DuctTint, Rot, true, EBHBlockMaterial::ConcreteWACool);
		SpawnBlock(LP(0.0f, 0.0f, CenterZForBlockBottom(150.0f, 0.12f)), FVector(lenScale, 2.1f, 0.12f), DuctTint, Rot, true, EBHBlockMaterial::ConcreteWACool);
		if (ABHCrawlSpaceVolume* Crawl = GetWorld()->SpawnActor<ABHCrawlSpaceVolume>(LP(0.0f, 0.0f, 80.0f), Rot))
		{
			// Y half-extent 105 (>= the side walls at +/-100) so the whole between-walls passage is inside the
			// shelter volume -- a prone survivor anywhere in the mouth is uncapturable, no edge margin.
			Crawl->Configure(FVector(lenScale * 50.0f + 40.0f, 105.0f, 110.0f));
		}
	};
	// Each crawl TUNNELS THROUGH an interior divider via a lintel-capped gap (the 4 split wall segments are up in
	// the Walls array): a prone-only mouth joining two rooms, with ~325u of low duct each side so a fleeing
	// survivor can slide straight in. The lintel (bottom Z150 -> wall top) blocks standing; the crawl volume ejects
	// the Teacher and grants capture immunity -- a real escape route the hunter cannot follow.
	auto SpawnCrawlGate = [&](float cx, float cy, float tunnelYaw, bool bWallAlongX, float lenScale)
	{
		const float GapW = 2.4f;      // lintel width, overlaps the ~220u wall gap
		const float LintelH = 1.75f;  // bottom Z150 -> wall top Z325 (so the wall reads continuous above the mouth)
		const FVector LintelScale = bWallAlongX ? FVector(GapW, 0.35f, LintelH) : FVector(0.35f, GapW, LintelH);
		SpawnBlock(FVector(cx, cy, CenterZForBlockBottom(150.0f, LintelH)), LintelScale, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::ConcreteWACool);
		const FVector StripeScale = bWallAlongX ? FVector(GapW, 0.05f, 0.10f) : FVector(0.05f, GapW, 0.10f);
		SpawnBlock(FVector(cx, cy, CenterZForBlockBottom(138.0f, 0.10f)), StripeScale, Warning, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign); // maintenance-hatch signpost
		SpawnCrawlDuct(cx, cy, tunnelYaw, lenScale);
	};
	SpawnCrawlGate(-2100.0f, 2500.0f, 90.0f, true, 6.5f);  // hall<->N band, off the -1500 door
	SpawnCrawlGate(1300.0f, 2500.0f, 90.0f, true, 6.5f);  // hall<->N band, off the 650 door
	SpawnCrawlGate(2300.0f, 2500.0f, 90.0f, true, 6.5f);  // hall<->N band, off the 2850 door
	SpawnCrawlGate(-2100.0f, -2500.0f, 90.0f, true, 6.5f);  // hall<->S band, off the -1500 door
	SpawnCrawlGate(2300.0f, -2500.0f, 90.0f, true, 6.5f);  // hall<->S band, off the 2850 door

	// A snug ~2.6m hidey-pocket whose only entrance is a ~40u front slot: too narrow for the round standing
	// capsule, so the survivor must turn sideways (auto-squeeze) to slip in. Three solid walls + a slotted front.
	auto SpawnSqueezePocket = [&](float cx, float cy, float yaw)
	{
		const float Rad = FMath::DegreesToRadians(yaw);
		const float Cs = FMath::Cos(Rad);
		const float Sn = FMath::Sin(Rad);
		auto LP = [&](float lx, float ly, float z) { return FVector(cx + lx * Cs - ly * Sn, cy + lx * Sn + ly * Cs, z); };
		const FRotator Rot(0.0f, yaw, 0.0f);
		const float H = 2.6f;
		SpawnBlock(LP(150.0f, 0.0f, CenterZForBlockBottom(0.0f, H)), FVector(0.4f, 3.4f, H), Wall, Rot, true, EBHBlockMaterial::ConcreteWACool);   // back
		SpawnBlock(LP(0.0f, -150.0f, CenterZForBlockBottom(0.0f, H)), FVector(3.0f, 0.4f, H), Wall, Rot, true, EBHBlockMaterial::ConcreteWACool);  // side
		SpawnBlock(LP(0.0f, 150.0f, CenterZForBlockBottom(0.0f, H)), FVector(3.0f, 0.4f, H), Wall, Rot, true, EBHBlockMaterial::ConcreteWACool);   // side
		SpawnBlock(LP(-150.0f, -75.0f, CenterZForBlockBottom(0.0f, H)), FVector(0.4f, 1.1f, H), Wall, Rot, true, EBHBlockMaterial::ConcreteWACool); // front stub (slot @ ~40u)
		SpawnBlock(LP(-150.0f, 75.0f, CenterZForBlockBottom(0.0f, H)), FVector(0.4f, 1.1f, H), Wall, Rot, true, EBHBlockMaterial::ConcreteWACool);
	};
	SpawnSqueezePocket(-1900.0f, -3350.0f, 0.0f);
	SpawnSqueezePocket(-1900.0f, 3350.0f, 0.0f);

	// Glowing blue control panels mounted on the perimeter walls + a soft blue light each (cold accents / faint
	// orientation landmarks). Falls back to a solid blue tint if the screen material is missing.
	struct FSubstationPanelSpec { FVector Location; FVector Scale; };
	const FSubstationPanelSpec SubstationPanels[] = {
		{ FVector(-6070.0f, -2000.0f, 150.0f), FVector(0.06f, 1.4f, 1.0f) }, { FVector(-6070.0f, 2000.0f, 150.0f), FVector(0.06f, 1.4f, 1.0f) },
		{ FVector(6070.0f, -2000.0f, 150.0f), FVector(0.06f, 1.4f, 1.0f) }, { FVector(6070.0f, 2000.0f, 150.0f), FVector(0.06f, 1.4f, 1.0f) },
		{ FVector(-2000.0f, -4570.0f, 150.0f), FVector(1.4f, 0.06f, 1.0f) }, { FVector(2000.0f, 4570.0f, 150.0f), FVector(1.4f, 0.06f, 1.0f) }
	};
	for (const FSubstationPanelSpec& Panel : SubstationPanels)
	{
		SpawnBlock(Panel.Location, Panel.Scale, PanelBlue, FRotator::ZeroRotator, false, EBHBlockMaterial::BluePanel);
		const FVector LightOffset(FMath::IsNearlyZero(Panel.Scale.X - 0.06f) ? (Panel.Location.X < 0.0f ? 180.0f : -180.0f) : 0.0f,
			FMath::IsNearlyZero(Panel.Scale.Y - 0.06f) ? (Panel.Location.Y < 0.0f ? 180.0f : -180.0f) : 0.0f, 25.0f);
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Panel.Location + LightOffset, FRotator::ZeroRotator))
		{
			Light->Configure(0, FLinearColor(0.30f, 0.55f, 1.0f, 1.0f), 230.0f, 430.0f);
			FlickerLights.Add(Light);
		}
	}

	const FVector Batteries[] = {
		FVector(-5650.0f, -700.0f, 70.0f), FVector(-5650.0f, 700.0f, 70.0f), FVector(-3500.0f, 0.0f, 70.0f), FVector(2850.0f, 0.0f, 70.0f),
		FVector(-1900.0f, -1900.0f, 70.0f), FVector(1100.0f, 1900.0f, 70.0f), FVector(-350.0f, 0.0f, 70.0f), FVector(5300.0f, 0.0f, 70.0f)
	};
	for (const FVector& Location : Batteries)
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator(0.0f, 90.0f, 0.0f));
	}

	SpawnMapCollectables(TEXT("substation"), {
		FVector(-5650.0f, -700.0f, 70.0f), FVector(-5650.0f, 700.0f, 70.0f), FVector(-3500.0f, 0.0f, 70.0f),
		FVector(2850.0f, 0.0f, 70.0f), FVector(-1900.0f, -1900.0f, 70.0f)
	});

	const FVector Alarms[] = {
		FVector(-5650.0f, -2400.0f, 105.0f), FVector(-5650.0f, 2400.0f, 105.0f),
		FVector(-3500.0f, -600.0f, 105.0f), FVector(2850.0f, -600.0f, 105.0f),
		FVector(-1500.0f, 1250.0f, 105.0f), FVector(600.0f, -1250.0f, 105.0f),
		FVector(4000.0f, 1900.0f, 105.0f), FVector(4000.0f, -1900.0f, 105.0f)
	};
	for (const FVector& Location : Alarms)
	{
		GetWorld()->SpawnActor<ABHPanicAlarm>(Location, FRotator::ZeroRotator);
	}

	const FVector SubstationExitGateLocation(5600.0f, 0.0f, 120.0f);
	if (ABHExitGate* ExitGate = GetWorld()->SpawnActor<ABHExitGate>(SubstationExitGateLocation, FRotator(0.0f, 90.0f, 0.0f)))
	{
		ExitGates.Add(ExitGate);
	}
	BuildMapSubwayExitStation(SubstationExitGateLocation, 1.0f, TEXT("SUBSTATION PLATFORM"), GetNextMapAfterStage(RuntimeStageIndex));
	SpawnBlock(FVector(-5850.0f, -2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(6.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(-5850.0f, 2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(6.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(4200.0f, -2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(7.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(4200.0f, 2500.0f, CenterZForBlockBottom(0.5f, 0.04f)), FVector(7.5f, 1.4f, 0.04f), Steel, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);

	for (int32 Circuit = 1; Circuit <= 8; ++Circuit)
	{
		const float X = -5200.0f + Circuit * 1250.0f;
		if (ABHFlickerLight* LightA = GetWorld()->SpawnActor<ABHFlickerLight>(FVector(X, -3150.0f, 285.0f), FRotator::ZeroRotator))
		{
			LightA->Configure(Circuit, Circuit % 2 == 0 ? FLinearColor(0.55f, 0.90f, 1.0f, 1.0f) : FLinearColor(1.0f, 0.72f, 0.38f, 1.0f), 900.0f, 1150.0f);
			FlickerLights.Add(LightA);
		}
		if (ABHFlickerLight* LightB = GetWorld()->SpawnActor<ABHFlickerLight>(FVector(X, 3150.0f, 285.0f), FRotator::ZeroRotator))
		{
			LightB->Configure(Circuit, Circuit % 3 == 0 ? FLinearColor(0.80f, 1.0f, 0.62f, 1.0f) : FLinearColor(0.70f, 0.82f, 1.0f, 1.0f), 820.0f, 1150.0f);
			FlickerLights.Add(LightB);
		}
		if (ABHPowerSwitch* Switch = GetWorld()->SpawnActor<ABHPowerSwitch>(FVector(-5900.0f + Circuit * 1300.0f, -4450.0f, 120.0f), FRotator::ZeroRotator))
		{
			Switch->Configure(Circuit, FText::FromString(FString::Printf(TEXT("Toggle Substation Circuit %d"), Circuit)));
		}
	}

	// Circuit-gated shutters on three through-passages (vertical-wall doorways) -- counterplay: open the matching
	// circuit to pass and to blind the cameras on it. Each gated room still has other entrances if a shutter shuts.
	for (const FVector& Location : {FVector(-2500.0f, -1700.0f, 120.0f), FVector(1800.0f, 1700.0f, 120.0f), FVector(3900.0f, -1700.0f, 120.0f)})  // open rib hall-arches (Y=+/-1700 gaps)
	{
		if (ABHSecurityShutter* Shutter = GetWorld()->SpawnActor<ABHSecurityShutter>(Location, FRotator::ZeroRotator))
		{
			Shutter->Configure(200);
		}
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-2300.0f, -1700.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Toggle Substation Shutters")));
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(4100.0f, -1700.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Toggle Substation Shutters")));
	}
	GetWorld()->SpawnActor<ABHSecurityMonitor>(FVector(-5450.0f, -850.0f, 115.0f), FRotator(0.0f, 90.0f, 0.0f));
	const TArray<TPair<FVector, FRotator>> SubstationCameras = {
		{FVector(-1800.0f, -2350.0f, 355.0f), FRotator(0.0f, 35.0f, 0.0f)},   // overlooks the central transformer hall
		{FVector(1800.0f, 2350.0f, 355.0f), FRotator(0.0f, -145.0f, 0.0f)}
	};
	for (int32 CameraIndex = 0; CameraIndex < SubstationCameras.Num(); ++CameraIndex)
	{
		const TPair<FVector, FRotator>& CameraSpec = SubstationCameras[CameraIndex];
		if (ABHSecurityCamera* Camera = GetWorld()->SpawnActor<ABHSecurityCamera>(CameraSpec.Key, CameraSpec.Value))
		{
			Camera->ConfigureCamera(5000.0f, 50.0f, 200);
			SpawnCCTVZoneForCamera(Camera, 200, TEXT("substation CCTV zone"), true);
		}
	}

	AddSurfaceDetailGrid(6100.0f, 4600.0f, FLinearColor(0.020f, 0.022f, 0.024f, 1.0f));

	SpawnAmbient(FVector(-5200.0f, 0.0f, 160.0f), 34.0f, 0.20f, 0.05f, 0.12f);
	SpawnAmbient(FVector(-1200.0f, -3200.0f, 160.0f), 56.0f, 0.15f, 0.035f, 0.20f);
	SpawnAmbient(FVector(1200.0f, 3200.0f, 160.0f), 72.0f, 0.14f, 0.025f, 0.33f);
	SpawnAmbient(FVector(4300.0f, -2800.0f, 160.0f), 42.0f, 0.16f, 0.04f, 0.16f);
	SpawnAmbient(FVector(5300.0f, 1200.0f, 160.0f), 92.0f, 0.11f, 0.025f, 0.45f);

	SpawnBlock(FVector(-5600.0f, 0.0f, CenterZForBlockBottom(0.5f, 0.06f)), FVector(3.5f, 4.0f, 0.06f), ControlBlue, FRotator::ZeroRotator, false);
	SpawnBlock(FVector(5650.0f, 0.0f, CenterZForBlockBottom(0.5f, 0.06f)), FVector(4.0f, 6.0f, 0.06f), SickGreen, FRotator::ZeroRotator, false);

	ScarePoints.Append({
		FVector(-5650.0f, -3400.0f, 110.0f),
		FVector(-5650.0f, 3400.0f, 110.0f),
		FVector(-1500.0f, -3700.0f, 110.0f),
		FVector(2850.0f, -3700.0f, 110.0f),
		FVector(750.0f, 3700.0f, 110.0f),
		FVector(2850.0f, 3700.0f, 110.0f),
		FVector(5300.0f, -1700.0f, 110.0f),
		FVector(5300.0f, 1700.0f, 110.0f),
		FVector(-1200.0f, 0.0f, 110.0f),
		FVector(1100.0f, 0.0f, 110.0f),
		FVector(4250.0f, 0.0f, 110.0f)
	});

	AddMoodPass(FLinearColor(0.018f, 0.045f, 0.052f, 1.0f), 0.010f, 0.50f, 0.10f);
	// AddClassroomHorrorPass(); // disabled: crude blockout classroom (blackboard + red mark + cube desks) looked bad (playtest)
	{
		FRandomStream VariationStream(BHVariationSeedForLevel(RuntimeLevelName, RuntimeStageIndex, RoundSeed));
		AddHorrorVariationPass(VariationStream);
	}
	BuildRuntimeNavigation();
}

void ABHGameMode::BuildRuntimeNavigation()
{
	if (!GetWorld())
	{
		return;
	}

#if WITH_EDITOR
	// During the authored-map export bake, do not spawn/serialize a runtime nav volume. The authored .umap
	// keeps marker.bRebuildRuntimeNavigation = true, so the runtime rebuilds nav fresh on load; baking one
	// here would ship a stale, Movable-brush NavMeshBoundsVolume + RecastNavMesh that the runtime then
	// duplicates. The export produces geometry + gameplay actors only.
	if (bAuthoringExport)
	{
		return;
	}
#endif

	bRuntimeNavigationReady = false;
	FinalizeStaticBlockField();

	const bool bSubstation = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase);
	const bool bFoggrounds = RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const bool bTutorial = RuntimeLevelName.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase);
	const FVector BoundsSize = bFoggrounds ? FVector(17000.0f, 13600.0f, 1000.0f)
		: (bSubstation ? FVector(13200.0f, 10000.0f, 800.0f)
		: (bTutorial ? FVector(13200.0f, 4400.0f, 400.0f) : FVector(12000.0f, 9600.0f, 800.0f)));
	// The tutorial greybox is offset east (spans X[-600,11400]), so its nav volume must follow or the chase
	// arena + exit fall outside the mesh. It is ALSO a fully enclosed box (floor at Z=0, ceiling at Z=340):
	// a Z band of [0,800] makes Recast generate the navmesh on the CEILING ROOF (the floor sits on the very
	// bottom voxel and is skipped), so the Teacher's path projects up to the roof and it can never move.
	// Centre the band at Z=100 with half-height 200 -> Z[-100,300]: it dips below the floor (so the floor IS
	// captured) and stops below the ceiling (so the roof is NOT), giving one correct ground-level navmesh.
	const FVector BoundsCenter = bTutorial
		? FVector(5400.0f, 0.0f, 100.0f)
		: FVector(0.0f, 0.0f, BoundsSize.Z * 0.5f);

	if (!RuntimeNavBounds)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RuntimeNavBounds = GetWorld()->SpawnActor<ANavMeshBoundsVolume>(BoundsCenter, FRotator::ZeroRotator, SpawnParams);
	}

	if (RuntimeNavBounds)
	{
		if (UBrushComponent* BrushComponent = RuntimeNavBounds->GetBrushComponent())
		{
			BrushComponent->SetMobility(EComponentMobility::Movable);
			BrushComponent->SetCanEverAffectNavigation(true);
		}
		RuntimeNavBounds->SetActorLocation(BoundsCenter);
		RuntimeNavBounds->SetActorScale3D(FVector::OneVector);

		UBoxComponent* BoundsBox = RuntimeNavBounds->FindComponentByClass<UBoxComponent>();
		if (!BoundsBox)
		{
			BoundsBox = NewObject<UBoxComponent>(RuntimeNavBounds, TEXT("RuntimeNavigationBounds"));
			BoundsBox->SetupAttachment(RuntimeNavBounds->GetRootComponent());
			RuntimeNavBounds->AddInstanceComponent(BoundsBox);
			RuntimeNavBounds->AddOwnedComponent(BoundsBox);
			BoundsBox->RegisterComponentWithWorld(GetWorld());
		}
		if (BoundsBox)
		{
			BoundsBox->SetMobility(EComponentMobility::Movable);
			BoundsBox->SetRelativeLocation(FVector::ZeroVector);
			BoundsBox->SetBoxExtent(BoundsSize * 0.5f, true);
			BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BoundsBox->SetHiddenInGame(true);
			BoundsBox->SetCanEverAffectNavigation(false);
			BoundsBox->UpdateBounds();
		}

		const FBox RuntimeBounds = RuntimeNavBounds->GetComponentsBoundingBox(true);
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt runtime nav bounds: %s valid=%s"), *RuntimeBounds.ToString(), RuntimeBounds.IsValid ? TEXT("true") : TEXT("false"));

		if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			NavSystem->OnNavigationBoundsUpdated(RuntimeNavBounds);
			FTimerHandle NavBuildHandle;
			GetWorldTimerManager().SetTimer(NavBuildHandle, this, &ABHGameMode::CompleteRuntimeNavigationBuild, 0.25f, false);
		}
	}
}

void ABHGameMode::CompleteRuntimeNavigationBuild()
{
	if (!GetWorld() || !RuntimeNavBounds)
	{
		return;
	}

	const FBox RuntimeBounds = RuntimeNavBounds->GetComponentsBoundingBox(true);
	int32 UpdatedActors = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == RuntimeNavBounds)
		{
			continue;
		}

		const FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
		if (ActorBounds.IsValid && RuntimeBounds.Intersect(ActorBounds))
		{
			UNavigationSystemV1::UpdateActorAndComponentsInNavOctree(*Actor, false);
			++UpdatedActors;
		}
	}

	if (UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		NavSystem->AddDirtyArea(RuntimeBounds, ENavigationDirtyFlag::All | ENavigationDirtyFlag::NavigationBounds, TEXT("BlackoutHunt runtime nav build"));
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt runtime nav build: updated %d actors in %s"), UpdatedActors, *RuntimeBounds.ToString());
		NavSystem->Build();
		bRuntimeNavigationReady = true;
	}
}

void ABHGameMode::AddFacilityDetailPass()
{
	AddSurfaceDetailGrid(5500.0f, 4400.0f, FLinearColor(0.020f, 0.022f, 0.024f, 1.0f));

	const FLinearColor Trim(0.04f, 0.05f, 0.055f, 1.0f);
	const FLinearColor Rust(0.32f, 0.13f, 0.07f, 1.0f);
	const FLinearColor Hazard(0.78f, 0.55f, 0.08f, 1.0f);
	const FLinearColor Pipe(0.12f, 0.15f, 0.16f, 1.0f);
	const FLinearColor Grime(0.015f, 0.018f, 0.016f, 1.0f);

	SpawnBlock(FVector(3600.0f, -2940.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(13.0f, 11.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-3600.0f, 2780.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(13.0f, 10.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::Tiles);
	SpawnBlock(FVector(-3500.0f, -2950.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(10.0f, 8.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(650.0f, -2950.0f, CenterZForBlockBottom(0.5f, 0.035f)), FVector(10.0f, 8.0f, 0.035f), FLinearColor::White, FRotator::ZeroRotator, false, EBHBlockMaterial::DiamondPlate);

	for (float Y = -3100.0f; Y <= 3100.0f; Y += 620.0f)
	{
		SpawnBlock(FVector(-4455.0f, Y, 210.0f), FVector(0.08f, 2.2f, 0.16f), Pipe, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(4455.0f, Y, 210.0f), FVector(0.08f, 2.2f, 0.16f), Pipe, FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	}
	for (float X = -4000.0f; X <= 4000.0f; X += 800.0f)
	{
		SpawnBlock(FVector(X, -3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Hazard, FRotator(0.0f, 28.0f, 0.0f), false);
		SpawnBlock(FVector(X + 260.0f, -3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Trim, FRotator(0.0f, -28.0f, 0.0f), false);
		SpawnBlock(FVector(X, 3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Hazard, FRotator(0.0f, -28.0f, 0.0f), false);
		SpawnBlock(FVector(X + 260.0f, 3540.0f, CenterZForBlockBottom(0.5f, 0.08f)), FVector(2.2f, 0.08f, 0.08f), Trim, FRotator(0.0f, 28.0f, 0.0f), false);
	}

	const TArray<FVector> ExtraClutter = {
		FVector(-3800.0f, -2100.0f, 40.0f), FVector(-2450.0f, -1800.0f, 40.0f), FVector(-950.0f, -2850.0f, 40.0f),
		FVector(950.0f, -2850.0f, 40.0f), FVector(2350.0f, -1950.0f, 40.0f), FVector(3750.0f, -2100.0f, 40.0f),
		FVector(-3900.0f, 2100.0f, 40.0f), FVector(-2500.0f, 1850.0f, 40.0f), FVector(-950.0f, 2850.0f, 40.0f),
		FVector(950.0f, 2850.0f, 40.0f), FVector(2300.0f, 1950.0f, 40.0f), FVector(3750.0f, 2100.0f, 40.0f)
	};
	AddIndustrialClutter(ExtraClutter, Rust);

	for (const FVector& Location : {FVector(-3300.0f, 300.0f, 100.0f), FVector(-2500.0f, 300.0f, 100.0f), FVector(-900.0f, -900.0f, 100.0f), FVector(900.0f, -900.0f, 100.0f), FVector(2600.0f, 3150.0f, 100.0f), FVector(3200.0f, 3150.0f, 100.0f), FVector(4300.0f, 1350.0f, 100.0f), FVector(-4300.0f, -1350.0f, 100.0f)})
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	const TArray<TPair<FVector, FRotator>> ExtraBreakers = {
		{FVector(-350.0f, 3180.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(2450.0f, -3180.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : ExtraBreakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	for (const FVector& Smear : {FVector(-1250.0f, -400.0f, 4.0f), FVector(1450.0f, 450.0f, 4.0f), FVector(3200.0f, -650.0f, 4.0f), FVector(-3350.0f, 650.0f, 4.0f), FVector(200.0f, 2100.0f, 4.0f), FVector(200.0f, -2100.0f, 4.0f)})
	{
		SpawnBlock(Smear, FVector(3.5f, 1.7f, 0.035f), Grime, FRotator(0.0f, FMath::FRandRange(-24.0f, 24.0f), 0.0f), false, EBHBlockMaterial::RustedMetal);
	}
}

// DISABLED at the call sites: the blockout "classroom" scenes this builds (a dark blackboard rectangle with
// a red chalk-mark square, plus cube desks/chairs) read as crude placeholder art rather than horror dressing,
// so the calls in the level builds are commented out. Kept here for reference / future real-mesh classrooms.
void ABHGameMode::AddClassroomHorrorPass()
{
	const FLinearColor Blackboard(0.015f, 0.075f, 0.055f, 1.0f);
	const FLinearColor Chalk(0.82f, 0.86f, 0.78f, 1.0f);
	const FLinearColor Desk(0.22f, 0.17f, 0.11f, 1.0f);
	const FLinearColor Chair(0.09f, 0.10f, 0.11f, 1.0f);
	const FLinearColor RedMark(0.72f, 0.035f, 0.025f, 1.0f);

	auto SpawnClassroom = [this, Blackboard, Chalk, Desk, Chair, RedMark](const FVector& Origin, float YawDegrees)
	{
		const FRotator Rotation(0.0f, YawDegrees, 0.0f);
		const auto Local = [&Origin, &Rotation](float X, float Y, float Z)
		{
			return Origin + Rotation.RotateVector(FVector(X, Y, Z));
		};

		SpawnBlock(Local(0.0f, -300.0f, 145.0f), FVector(3.4f, 0.05f, 0.78f), Blackboard, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Local(-145.0f, -303.0f, 178.0f), FVector(0.85f, 0.035f, 0.035f), Chalk, FRotator(0.0f, YawDegrees + 8.0f, 0.0f), false);
		SpawnBlock(Local(80.0f, -304.0f, 120.0f), FVector(0.55f, 0.035f, 0.035f), RedMark, FRotator(0.0f, YawDegrees - 12.0f, 0.0f), false);
		SpawnBlock(Local(0.0f, -95.0f, 42.0f), FVector(2.3f, 0.78f, 0.46f), Desk, Rotation, false, EBHBlockMaterial::PaintedMetal);

		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				const float X = -260.0f + Col * 260.0f;
				const float Y = 150.0f + Row * 210.0f;
				const float Skew = (Row + Col) % 2 == 0 ? 6.0f : -5.0f;
				SpawnBlock(Local(X, Y, 32.0f), FVector(0.95f, 0.62f, 0.32f), Desk, FRotator(0.0f, YawDegrees + Skew, 0.0f), false, EBHBlockMaterial::PaintedMetal);
				SpawnBlock(Local(X, Y + 72.0f, 58.0f), FVector(0.72f, 0.10f, 0.72f), Chair, FRotator(0.0f, YawDegrees + Skew, 0.0f), false, EBHBlockMaterial::RustedMetal);
			}
		}
	};

	SpawnClassroom(FVector(-3600.0f, -450.0f, 0.0f), 90.0f);
	SpawnClassroom(FVector(3620.0f, 1600.0f, 0.0f), -90.0f);
	SpawnClassroom(FVector(-900.0f, 2850.0f, 0.0f), 180.0f);
	SpawnClassroom(FVector(1200.0f, -3000.0f, 0.0f), 0.0f);
}

void ABHGameMode::AddHorrorVariationPass(FRandomStream& Stream)
{
	if (!GetWorld())
	{
		return;
	}

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const FBHHorrorVariationSettings Variation = Settings ? Settings->HorrorVariation : FBHHorrorVariationSettings();
	const float SurfaceChance = FMath::Clamp(Variation.SurfacePatchChance * Variation.IntensityScale, 0.0f, 1.0f);
	const int32 GlassLimit = FMath::Max(0, FMath::RoundToInt(static_cast<float>(Variation.GlassPaneCount) * FMath::Max(0.25f, Variation.IntensityScale)));
	const int32 DecoyLimit = FMath::Max(0, FMath::RoundToInt(static_cast<float>(Variation.DecoyCueCount) * FMath::Max(0.25f, Variation.IntensityScale)));

	struct FBHSurfacePatchSpec
	{
		FVector Location;
		FVector Extent;
		EBHFootstepSurface Surface;
	};

	struct FBHGlassPaneSpec
	{
		FVector Location;
		FVector Scale;
		FRotator Rotation;
		FText Label;
	};

	TArray<FBHSurfacePatchSpec> SurfacePatches;
	TArray<FBHGlassPaneSpec> GlassPanes;
	TArray<FVector> SafeClutter;

	if (RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
	{
		SurfacePatches = {
			{FVector(-5850.0f, -2500.0f, 26.0f), FVector(650.0f, 140.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(-5850.0f, 2500.0f, 26.0f), FVector(650.0f, 140.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(4200.0f, -2500.0f, 26.0f), FVector(750.0f, 140.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(4200.0f, 2500.0f, 26.0f), FVector(750.0f, 140.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(-5600.0f, 0.0f, 26.0f), FVector(360.0f, 420.0f, 70.0f), EBHFootstepSurface::Wet},
			{FVector(5650.0f, 0.0f, 26.0f), FVector(420.0f, 620.0f, 70.0f), EBHFootstepSurface::Wet}
		};
		GlassPanes = {
			{FVector(-5600.0f, -390.0f, 145.0f), FVector(1.75f, 0.06f, 1.05f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Control Glass"))},
			{FVector(-5600.0f, 390.0f, 145.0f), FVector(1.75f, 0.06f, 1.05f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Control Glass"))},
			{FVector(5650.0f, -590.0f, 145.0f), FVector(2.25f, 0.06f, 1.05f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Control Glass"))},
			{FVector(5650.0f, 590.0f, 145.0f), FVector(2.25f, 0.06f, 1.05f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Control Glass"))}
		};
		SafeClutter = {FVector(-4700.0f, -3350.0f, 38.0f), FVector(-1800.0f, 3550.0f, 38.0f), FVector(3750.0f, -3450.0f, 38.0f)};
	}
	else if (RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		SurfacePatches = {
			{FVector(-7050.0f, -4900.0f, 26.0f), FVector(620.0f, 480.0f, 70.0f), EBHFootstepSurface::Gravel},
			{FVector(-3200.0f, -300.0f, 26.0f), FVector(620.0f, 420.0f, 70.0f), EBHFootstepSurface::Wet},
			{FVector(3200.0f, -300.0f, 26.0f), FVector(620.0f, 420.0f, 70.0f), EBHFootstepSurface::Wet},
			{FVector(7050.0f, 4200.0f, 26.0f), FVector(540.0f, 480.0f, 70.0f), EBHFootstepSurface::Gravel},
			{FVector(3300.0f, -760.0f, 32.0f), FVector(1700.0f, 520.0f, 70.0f), EBHFootstepSurface::Metal}
		};
		GlassPanes = {
			{FVector(-4050.0f, -4550.0f, 148.0f), FVector(1.55f, 0.06f, 0.95f), FRotator(0.0f, 8.0f, 0.0f), FText::FromString(TEXT("Break Cabin Glass"))},
			{FVector(3700.0f, 4550.0f, 148.0f), FVector(1.55f, 0.06f, 0.95f), FRotator(0.0f, -8.0f, 0.0f), FText::FromString(TEXT("Break Cabin Glass"))},
			{FVector(3300.0f, -430.0f, 136.0f), FVector(2.30f, 0.06f, 0.95f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Station Glass"))}
		};
		SafeClutter = {FVector(-6200.0f, -3700.0f, 38.0f), FVector(-2500.0f, 380.0f, 38.0f), FVector(2500.0f, 360.0f, 38.0f), FVector(6200.0f, 3650.0f, 38.0f)};
	}
	else if (RuntimeLevelName.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase))
	{
		SurfacePatches = {
			{FVector(-2200.0f, 0.0f, 26.0f), FVector(980.0f, 380.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(0.0f, 0.0f, 26.0f), FVector(980.0f, 380.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(2200.0f, 0.0f, 26.0f), FVector(980.0f, 380.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(3300.0f, -760.0f, 30.0f), FVector(1550.0f, 480.0f, 70.0f), EBHFootstepSurface::Concrete}
		};
		// No breakable train windows: the carriage now has sealed, collidable tinted glass set into the
		// side walls (see BuildTrainIntermissionLevel), so breakable panes here would be redundant and
		// would float as loose panels in front of the real window wall. Clutter crates move inside the
		// sealed tube against clear wall spots (the old y=+/-880 points are now behind the tunnel
		// backdrop and unreachable).
		SafeClutter = {FVector(-2950.0f, 250.0f, 38.0f), FVector(-650.0f, 250.0f, 38.0f), FVector(650.0f, -250.0f, 38.0f)};
	}
	else
	{
		SurfacePatches = {
			{FVector(3600.0f, -2940.0f, 26.0f), FVector(1280.0f, 1080.0f, 70.0f), EBHFootstepSurface::Tile},
			{FVector(-3600.0f, 2780.0f, 26.0f), FVector(1280.0f, 980.0f, 70.0f), EBHFootstepSurface::Tile},
			{FVector(-3500.0f, -2950.0f, 26.0f), FVector(980.0f, 780.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(650.0f, -2950.0f, 26.0f), FVector(980.0f, 780.0f, 70.0f), EBHFootstepSurface::Metal},
			{FVector(0.0f, -1200.0f, 26.0f), FVector(580.0f, 480.0f, 70.0f), EBHFootstepSurface::Wet},
			{FVector(-4200.0f, 0.0f, 26.0f), FVector(420.0f, 620.0f, 70.0f), EBHFootstepSurface::Soft}
		};
		GlassPanes = {
			{FVector(3500.0f, -2400.0f, 150.0f), FVector(1.55f, 0.06f, 1.0f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Office Glass"))},
			{FVector(-3000.0f, 2400.0f, 150.0f), FVector(1.55f, 0.06f, 1.0f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Office Glass"))},
			{FVector(650.0f, -2400.0f, 150.0f), FVector(1.55f, 0.06f, 1.0f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Lab Glass"))},
			{FVector(4455.0f, 1350.0f, 155.0f), FVector(0.06f, 1.35f, 1.0f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Hall Glass"))},
			{FVector(-4455.0f, -1350.0f, 155.0f), FVector(0.06f, 1.35f, 1.0f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Hall Glass"))}
		};
		SafeClutter = {FVector(-2800.0f, -3350.0f, 38.0f), FVector(1700.0f, 3350.0f, 38.0f), FVector(3150.0f, -1400.0f, 38.0f), FVector(-3150.0f, 1400.0f, 38.0f)};
	}

	for (const FBHSurfacePatchSpec& Patch : SurfacePatches)
	{
		if (Stream.FRand() <= SurfaceChance)
		{
			SpawnSurfacePatch(Patch.Location, Patch.Extent, Patch.Surface);
		}
	}

	for (int32 Index = 0; Index < GlassPanes.Num(); ++Index)
	{
		const int32 SwapIndex = Stream.RandRange(Index, GlassPanes.Num() - 1);
		GlassPanes.Swap(Index, SwapIndex);
	}
	for (int32 Index = 0; Index < FMath::Min(GlassLimit, GlassPanes.Num()); ++Index)
	{
		const FBHGlassPaneSpec& Glass = GlassPanes[Index];
		SpawnBreakableGlassPane(Glass.Location, Glass.Scale, Glass.Rotation, Glass.Label);
	}

	const FLinearColor VariationClutterTint(0.12f, 0.10f, 0.085f, 1.0f);
	for (const FVector& Center : SafeClutter)
	{
		const float Angle = Stream.FRandRange(-35.0f, 35.0f);
		SpawnBlock(Center, FVector(0.62f, 0.45f, 0.42f), VariationClutterTint, FRotator(0.0f, Angle, 0.0f), true, EBHBlockMaterial::RustedMetal);
	}

	for (int32 Index = 0; Index < FMath::Min(DecoyLimit, ScarePoints.Num()); ++Index)
	{
		const FVector DecoyLocation = ScarePoints[Stream.RandRange(0, ScarePoints.Num() - 1)] + FVector(0.0f, 0.0f, 72.0f);
		SpawnAmbient(DecoyLocation, Stream.FRandRange(85.0f, 210.0f), 0.11f, 0.06f, 1.4f, Stream.FRandRange(4.0f, 7.5f));
	}
}

void ABHGameMode::AddSurfaceDetailGrid(float HalfX, float HalfY, const FLinearColor& LineTint)
{
	for (float X = -HalfX + 600.0f; X < HalfX; X += 600.0f)
	{
		SpawnBlock(FVector(X, 0.0f, 2.0f), FVector(0.025f, HalfY / 100.0f, 0.025f), LineTint, FRotator::ZeroRotator, false);
	}
	for (float Y = -HalfY + 600.0f; Y < HalfY; Y += 600.0f)
	{
		SpawnBlock(FVector(0.0f, Y, 2.5f), FVector(HalfX / 100.0f, 0.025f, 0.025f), LineTint, FRotator::ZeroRotator, false);
	}
}

void ABHGameMode::AddIndustrialClutter(const TArray<FVector>& Centers, const FLinearColor& Tint)
{
	// Real-prop set dressing. Each curated (already collision-safe) clutter centre gets a SmartBasicInterfaces
	// industrial prop instead of a bare cube stack: a large floor prop (cabinet / shelf / generator) plus a
	// couple of small accents (gas can / battery). Meshes + scales are reused from AddFacilityVerticalSlicePass
	// so they are known-good sizes, and SpawnRuntimeMeshProp bakes a real AStaticMeshActor during authored
	// export while falling back to a tinted cube if a mesh is ever missing -- so this can never break the cook
	// or leave a hole. SmartBasicInterfaces is whole-AlwaysCook (DefaultGame.ini), so no new cook dependency.
	// ONE real prop per clutter centre (down from a 3-4 cube stack) for a far less cramped, more readable
	// space, cycling a varied set: SmartBasicInterfaces industrial pieces (known-good scales reused from the
	// vertical-slice pass) plus a few ResidentHorrorV1 props for flavour. SpawnRuntimeMeshProp bakes a real
	// AStaticMeshActor and falls back to a tinted cube if a mesh is missing. SmartBasicInterfaces is
	// whole-AlwaysCook; the ResidentHorrorV1 meshes cook transitively via the baked map's references.
	// NOTE: ResidentHorrorV1 props are placed at scale 1.0 (their native size is unverified) - flag any that
	// look wrong-sized on a walk-through and the scale here is a one-number fix.
	struct FClutterProp { const TCHAR* Mesh; const TCHAR* Material; float Scale; };
	static const FClutterProp Props[] = {
		{ TEXT("/Game/SmartBasicInterfaces/Meshes/SM_cabinet.SM_cabinet"),                   TEXT("/Game/SmartBasicInterfaces/Materials/MI_Cabinet.MI_Cabinet"),                   0.62f },
		{ TEXT("/Game/ResidentHorrorV1/Demo/Meshes/SM_Computer.SM_Computer"),                TEXT(""),                                                                             1.00f },
		{ TEXT("/Game/SmartBasicInterfaces/Demo/SCContent/Props/SM_Shelf.SM_Shelf"),         TEXT(""),                                                                             0.62f },
		{ TEXT("/Game/ResidentHorrorV1/Demo/Meshes/SM_trashbin.SM_trashbin"),                TEXT(""),                                                                             1.00f },
		{ TEXT("/Game/SmartBasicInterfaces/Meshes/SM_generatorSection.SM_generatorSection"), TEXT("/Game/SmartBasicInterfaces/Materials/MI_GeneratorSection.MI_GeneratorSection"), 0.58f },
		{ TEXT("/Game/ResidentHorrorV1/Demo/Meshes/SM_ammoBox.SM_ammoBox"),                  TEXT(""),                                                                             1.00f },
	};
	const int32 NumProps = static_cast<int32>(UE_ARRAY_COUNT(Props));
	for (int32 Index = 0; Index < Centers.Num(); ++Index)
	{
		const FClutterProp& Prop = Props[Index % NumProps];
		const float Yaw = static_cast<float>((Index * 53) % 360);
		SpawnRuntimeMeshProp(Centers[Index], FRotator(0.0f, Yaw, 0.0f), FString(Prop.Mesh), FString(Prop.Material), FVector(Prop.Scale), true, FVector(0.85f, 0.52f, 1.20f), Tint, EBHBlockMaterial::RustedMetal);
	}
}

void ABHGameMode::AddFacilityVerticalSlicePass()
{
	if (!GetWorld())
	{
		return;
	}

	const FLinearColor RouteExitGreen(0.12f, 0.92f, 0.48f, 1.0f);
	const FLinearColor RouteStorageAmber(0.95f, 0.58f, 0.18f, 1.0f);
	const FLinearColor RouteLabCyan(0.13f, 0.78f, 0.96f, 1.0f);
	const FLinearColor RouteWardViolet(0.70f, 0.36f, 0.94f, 1.0f);
	const FLinearColor RouteUtilityWhite(0.86f, 0.92f, 0.82f, 1.0f);
	const FLinearColor SignBack(0.030f, 0.035f, 0.038f, 1.0f);
	const FLinearColor DarkMetal(0.055f, 0.060f, 0.062f, 1.0f);

	const FString CabinetMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_cabinet.SM_cabinet"));
	const FString CabinetMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_Cabinet.MI_Cabinet"));
	const FString PanelMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_panel.SM_panel"));
	const FString PanelMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_Panel.MI_Panel"));
	const FString GeneratorFrontMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_generatorFront.SM_generatorFront"));
	const FString GeneratorFrontMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_GeneratorFront.MI_GeneratorFront"));
	const FString GeneratorSectionMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_generatorSection.SM_generatorSection"));
	const FString GeneratorSectionMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_GeneratorSection.MI_GeneratorSection"));
	const FString LightMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_light.SM_light"));
	const FString LightMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_EmissiveLight.MI_EmissiveLight"));
	const FString DoubleDoorFrameMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_doubledoorframe.SM_doubledoorframe"));
	const FString DoubleDoorFrameMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_DoubleDoorFrame.MI_DoubleDoorFrame"));
	const FString CardReaderMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_cardreader.SM_cardreader"));
	const FString CardReaderMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_CardReader.MI_CardReader"));
	const FString AlarmMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_alarm.SM_alarm"));
	const FString AlarmMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_EmissiveRed.MI_EmissiveRed"));
	const FString PowerButtonMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_powerbtn.SM_powerbtn"));
	const FString PowerButtonMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_PowerBTN.MI_PowerBTN"));
	const FString GasCanMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_gascan.SM_gascan"));
	const FString GasCanMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_GasCan.MI_GasCan"));
	const FString DoubleDoorLeftMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_doubledoor01.SM_doubledoor01"));
	const FString DoubleDoorRightMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_doubledoor02.SM_doubledoor02"));
	const FString DoubleDoorMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_DoubleDoor.MI_DoubleDoor"));
	const FString CabinetDrawerMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_cabinetdrawer.SM_cabinetdrawer"));
	const FString CabinetDrawerMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_CabinetDrawer.MI_CabinetDrawer"));
	const FString CardMesh(TEXT("/Game/SmartBasicInterfaces/Meshes/SM_card.SM_card"));
	const FString CardMaterial(TEXT("/Game/SmartBasicInterfaces/Materials/MI_Card01.MI_Card01"));
	const FString ScreenPanel1Material(TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel1.MI_ScreenPanel1"));
	const FString ScreenPanel2Material(TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel2.MI_ScreenPanel2"));
	const FString ScreenPanel3Material(TEXT("/Game/SmartBasicInterfaces/Materials/MI_ScreenPanel3.MI_ScreenPanel3"));

	auto SpawnFloorChevrons = [this](const FVector& Origin, const FVector& Direction, const FLinearColor& Tint, int32 Count, float Spacing = 280.0f)
	{
		FVector Forward(Direction.X, Direction.Y, 0.0f);
		Forward = Forward.GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return;
		}

		const float Yaw = Forward.Rotation().Yaw;
		const float Z = CenterZForBlockBottom(0.95f, 0.035f);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Center = FVector(Origin.X, Origin.Y, Z) + Forward * (Spacing * Index);
			SpawnBlock(Center, FVector(1.28f, 0.055f, 0.035f), Tint, FRotator(0.0f, Yaw + 33.0f, 0.0f), false, EBHBlockMaterial::WarningSign);
			SpawnBlock(Center, FVector(1.28f, 0.055f, 0.035f), Tint, FRotator(0.0f, Yaw - 33.0f, 0.0f), false, EBHBlockMaterial::WarningSign);
		}
	};

	const auto GlyphRowMask = [](TCHAR InGlyph, int32 Row) -> int32
	{
		switch (FChar::ToUpper(InGlyph))
		{
		case 'A':
		{
			static const int32 Rows[7] = {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
			return Rows[Row];
		}
		case 'E':
		{
			static const int32 Rows[7] = {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
			return Rows[Row];
		}
		case 'L':
		{
			static const int32 Rows[7] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
			return Rows[Row];
		}
		case 'S':
		{
			static const int32 Rows[7] = {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
			return Rows[Row];
		}
		case 'T':
		{
			static const int32 Rows[7] = {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
			return Rows[Row];
		}
		case 'U':
		{
			static const int32 Rows[7] = {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F};
			return Rows[Row];
		}
		case 'V':
		{
			static const int32 Rows[7] = {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04};
			return Rows[Row];
		}
		case 'W':
		{
			static const int32 Rows[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
			return Rows[Row];
		}
		default:
			return 0;
		}
	};

	auto SpawnGlyphPixels = [this, GlyphRowMask](const FVector& Location, const FRotator& Rotation, const FLinearColor& Accent, TCHAR Glyph, const FVector& TopLeftOffset, float CellX, float CellZ, const FVector& PixelScale)
	{
		if (Glyph == 0)
		{
			return;
		}

		const auto LocalPoint = [&Location, &Rotation](const FVector& Offset)
		{
			return Location + Rotation.RotateVector(Offset);
		};
		for (int32 Row = 0; Row < 7; ++Row)
		{
			const int32 Mask = GlyphRowMask(Glyph, Row);
			for (int32 Col = 0; Col < 5; ++Col)
			{
				if ((Mask & (1 << (4 - Col))) != 0)
				{
					SpawnBlock(LocalPoint(TopLeftOffset + FVector(Col * CellX, 0.0f, -Row * CellZ)), PixelScale, Accent, Rotation, false, EBHBlockMaterial::WarningSign);
				}
			}
		}
	};

	auto SpawnOverheadSign = [this, SignBack, SpawnGlyphPixels](const FVector& Location, const FRotator& Rotation, const FLinearColor& Accent, int32 Pattern, TCHAR Glyph)
	{
		const auto LocalPoint = [&Location, &Rotation](const FVector& Offset)
		{
			return Location + Rotation.RotateVector(Offset);
		};

		SpawnBlock(Location, FVector(3.2f, 0.075f, 0.56f), SignBack, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(FVector(0.0f, -7.0f, 31.0f)), FVector(2.75f, 0.040f, 0.060f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(FVector(0.0f, -8.0f, -29.0f)), FVector(2.75f, 0.040f, 0.050f), Accent * 0.72f, Rotation, false, EBHBlockMaterial::WarningSign);
		for (int32 Tick = 0; Tick < 3; ++Tick)
		{
			const float LocalX = -92.0f + Tick * 92.0f;
			const float TickHeight = Pattern == 0 ? 0.22f : (Pattern == 1 ? (0.16f + Tick * 0.05f) : (0.28f - Tick * 0.04f));
			SpawnBlock(LocalPoint(FVector(LocalX, -9.0f, 0.0f)), FVector(0.16f, 0.035f, TickHeight), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
		}
		SpawnGlyphPixels(Location, Rotation, Accent, Glyph, FVector(-44.0f, -10.5f, 21.5f), 22.0f, 7.2f, FVector(0.095f, 0.030f, 0.030f));
	};

	auto SpawnLandmarkBeacon = [this, DarkMetal, SpawnOverheadSign](const FVector& Location, const FRotator& Rotation, const FLinearColor& Accent, int32 Pattern, TCHAR Glyph)
	{
		SpawnBlock(Location + FVector(0.0f, 0.0f, 120.0f), FVector(0.20f, 0.20f, 2.35f), DarkMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Location + FVector(0.0f, 0.0f, 278.0f), FVector(0.32f, 0.32f, 0.16f), Accent, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		SpawnOverheadSign(Location + FVector(0.0f, 0.0f, 238.0f), Rotation, Accent, Pattern, Glyph);
	};

	auto SpawnRouteGateway = [this, DarkMetal, SpawnOverheadSign, LightMesh, LightMaterial](const FVector& FloorCenter, const FRotator& Rotation, const FLinearColor& Accent, TCHAR Glyph, bool bPlatformFrame)
	{
		const auto LocalPoint = [&FloorCenter, &Rotation](const FVector& Offset)
		{
			return FloorCenter + Rotation.RotateVector(Offset);
		};

		SpawnBlock(LocalPoint(FVector(0.0f, 0.0f, CenterZForBlockBottom(0.98f, 0.040f))), FVector(4.15f, 0.10f, 0.040f), Accent * 0.50f, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(FVector(-188.0f, 0.0f, 118.0f)), FVector(0.12f, 0.10f, 2.20f), DarkMetal, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(FVector(188.0f, 0.0f, 118.0f)), FVector(0.12f, 0.10f, 2.20f), DarkMetal, Rotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(LocalPoint(FVector(-188.0f, -8.0f, 224.0f)), FVector(0.16f, 0.045f, 0.24f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(LocalPoint(FVector(188.0f, -8.0f, 224.0f)), FVector(0.16f, 0.045f, 0.24f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnOverheadSign(LocalPoint(FVector(0.0f, 0.0f, 248.0f)), Rotation, Accent, bPlatformFrame ? 0 : 1, Glyph);
		SpawnRuntimeMeshProp(LocalPoint(FVector(-232.0f, -12.0f, 204.0f)), Rotation, LightMesh, LightMaterial, FVector(0.32f), false, FVector(0.32f, 0.08f, 0.10f), Accent, EBHBlockMaterial::WarningSign);
		SpawnRuntimeMeshProp(LocalPoint(FVector(232.0f, -12.0f, 204.0f)), Rotation, LightMesh, LightMaterial, FVector(0.32f), false, FVector(0.32f, 0.08f, 0.10f), Accent, EBHBlockMaterial::WarningSign);
		if (bPlatformFrame)
		{
			SpawnBlock(LocalPoint(FVector(0.0f, -82.0f, CenterZForBlockBottom(0.98f, 0.035f))), FVector(3.30f, 0.055f, 0.035f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
			SpawnBlock(LocalPoint(FVector(0.0f, 82.0f, CenterZForBlockBottom(0.98f, 0.035f))), FVector(3.30f, 0.055f, 0.035f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
		}
	};

	auto SpawnWallRouteBand = [this](const FVector& Location, const FRotator& Rotation, const FLinearColor& Accent, float LengthScale)
	{
		SpawnBlock(Location, FVector(LengthScale, 0.035f, 0.055f), Accent * 0.62f, Rotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(Location + Rotation.RotateVector(FVector(0.0f, -5.0f, 18.0f)), FVector(LengthScale * 0.72f, 0.030f, 0.035f), Accent, Rotation, false, EBHBlockMaterial::WarningSign);
	};

	auto SpawnRunwayDots = [this](const FVector& Origin, const FVector& Direction, const FLinearColor& Accent, int32 Count, float Spacing)
	{
		FVector Forward(Direction.X, Direction.Y, 0.0f);
		Forward = Forward.GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			return;
		}
		const FVector Right(-Forward.Y, Forward.X, 0.0f);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector Center = Origin + Forward * (Spacing * Index);
			const float Z = CenterZForBlockBottom(1.00f, 0.035f);
			SpawnBlock(FVector(Center.X, Center.Y, Z) + Right * 135.0f, FVector(0.24f, 0.24f, 0.035f), Accent, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
			SpawnBlock(FVector(Center.X, Center.Y, Z) - Right * 135.0f, FVector(0.24f, 0.24f, 0.035f), Accent * 0.70f, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);
		}
	};

	auto SpawnLoopChoiceCue = [this](const FVector& Location, float YawDegrees, const FLinearColor& Accent)
	{
		const FRotator BaseRotation(0.0f, YawDegrees, 0.0f);
		const FVector FloorLocation(Location.X, Location.Y, CenterZForBlockBottom(1.08f, 0.035f));
		SpawnBlock(FloorLocation, FVector(1.05f, 0.050f, 0.035f), Accent, BaseRotation + FRotator(0.0f, 34.0f, 0.0f), false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FloorLocation, FVector(1.05f, 0.050f, 0.035f), Accent, BaseRotation + FRotator(0.0f, -34.0f, 0.0f), false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FloorLocation + BaseRotation.RotateVector(FVector(0.0f, 138.0f, 0.5f)), FVector(0.34f, 0.10f, 0.035f), Accent * 0.72f, BaseRotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(FloorLocation + BaseRotation.RotateVector(FVector(0.0f, -138.0f, 0.5f)), FVector(0.34f, 0.10f, 0.035f), Accent * 0.72f, BaseRotation, false, EBHBlockMaterial::WarningSign);
	};

	SpawnFloorChevrons(FVector(1180.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), RouteExitGreen, 5, 310.0f);
	SpawnFloorChevrons(FVector(-1180.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), RouteExitGreen, 5, 310.0f);
	SpawnFloorChevrons(FVector(-2150.0f, -840.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), RouteStorageAmber, 4);
	SpawnFloorChevrons(FVector(2450.0f, -840.0f, 0.0f), FVector(0.0f, -1.0f, 0.0f), RouteLabCyan, 4);
	SpawnFloorChevrons(FVector(-2550.0f, 840.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), RouteWardViolet, 4);
	SpawnFloorChevrons(FVector(250.0f, 1480.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), RouteUtilityWhite, 5);
	SpawnFloorChevrons(FVector(2860.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), RouteExitGreen, 5, 330.0f);
	SpawnFloorChevrons(FVector(-2860.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), RouteExitGreen, 5, 330.0f);
	SpawnRunwayDots(FVector(2750.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), RouteExitGreen, 6, 360.0f);
	SpawnRunwayDots(FVector(-2750.0f, 0.0f, 0.0f), FVector(-1.0f, 0.0f, 0.0f), RouteExitGreen, 6, 360.0f);

	SpawnOverheadSign(FVector(-4050.0f, 0.0f, 245.0f), FRotator(0.0f, 90.0f, 0.0f), RouteExitGreen, 0, 'E');
	SpawnOverheadSign(FVector(4050.0f, 0.0f, 245.0f), FRotator(0.0f, 90.0f, 0.0f), RouteExitGreen, 0, 'E');
	SpawnLandmarkBeacon(FVector(-2180.0f, -1255.0f, 0.0f), FRotator::ZeroRotator, RouteStorageAmber, 1, 'S');
	SpawnLandmarkBeacon(FVector(2450.0f, -1255.0f, 0.0f), FRotator::ZeroRotator, RouteLabCyan, 2, 'L');
	SpawnLandmarkBeacon(FVector(-2550.0f, 1255.0f, 0.0f), FRotator::ZeroRotator, RouteWardViolet, 0, 'W');
	SpawnLandmarkBeacon(FVector(250.0f, 1255.0f, 0.0f), FRotator::ZeroRotator, RouteUtilityWhite, 1, 'U');
	SpawnLandmarkBeacon(FVector(250.0f, 2440.0f, 0.0f), FRotator::ZeroRotator, RouteUtilityWhite, 2, 'U');
	SpawnRouteGateway(FVector(-2150.0f, -1185.0f, 0.0f), FRotator::ZeroRotator, RouteStorageAmber, 'S', false);
	SpawnRouteGateway(FVector(2450.0f, -1185.0f, 0.0f), FRotator::ZeroRotator, RouteLabCyan, 'L', false);
	SpawnRouteGateway(FVector(-2550.0f, 1185.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), RouteWardViolet, 'W', false);
	SpawnRouteGateway(FVector(250.0f, 1185.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f), RouteUtilityWhite, 'U', false);
	SpawnRouteGateway(FVector(-4680.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), RouteExitGreen, 'E', true);
	SpawnRouteGateway(FVector(4680.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f), RouteExitGreen, 'E', true);

	SpawnWallRouteBand(FVector(-2200.0f, -1191.0f, 188.0f), FRotator::ZeroRotator, RouteStorageAmber, 3.8f);
	SpawnWallRouteBand(FVector(2450.0f, -1191.0f, 188.0f), FRotator::ZeroRotator, RouteLabCyan, 3.8f);
	SpawnWallRouteBand(FVector(-2550.0f, 1191.0f, 188.0f), FRotator(0.0f, 180.0f, 0.0f), RouteWardViolet, 3.8f);
	SpawnWallRouteBand(FVector(250.0f, 1191.0f, 188.0f), FRotator(0.0f, 180.0f, 0.0f), RouteUtilityWhite, 4.2f);
	SpawnWallRouteBand(FVector(-4050.0f, -310.0f, 188.0f), FRotator(0.0f, 90.0f, 0.0f), RouteExitGreen, 3.6f);
	SpawnWallRouteBand(FVector(4050.0f, 310.0f, 188.0f), FRotator(0.0f, -90.0f, 0.0f), RouteExitGreen, 3.6f);

	auto SpawnObjectiveSilhouette = [this, SignBack, DarkMetal, SpawnGlyphPixels](const FVector& StationLocation, EBHObjectiveStationType StationType)
	{
		const FLinearColor Accent = [StationType]()
		{
			switch (StationType)
			{
			case EBHObjectiveStationType::Valve:
				return FLinearColor(0.88f, 0.18f, 0.10f, 1.0f);
			case EBHObjectiveStationType::Terminal:
				return FLinearColor(0.08f, 0.78f, 0.98f, 1.0f);
			case EBHObjectiveStationType::Antenna:
				return FLinearColor(0.86f, 0.82f, 0.34f, 1.0f);
			case EBHObjectiveStationType::Evidence:
				return FLinearColor(0.94f, 0.34f, 0.08f, 1.0f);
			default:
				return FLinearColor(0.55f, 0.65f, 0.70f, 1.0f);
			}
		}();
		const TCHAR StationGlyph = [StationType]() -> TCHAR
		{
			switch (StationType)
			{
			case EBHObjectiveStationType::Valve:
				return 'V';
			case EBHObjectiveStationType::Terminal:
				return 'T';
			case EBHObjectiveStationType::Antenna:
				return 'A';
			case EBHObjectiveStationType::Evidence:
				return 'E';
			default:
				return TCHAR(0);
			}
		}();

		FVector ToHub(-StationLocation.X, -StationLocation.Y, 0.0f);
		ToHub = ToHub.GetSafeNormal();
		if (ToHub.IsNearlyZero())
		{
			ToHub = FVector::ForwardVector;
		}

		const FRotator PanelRotation(0.0f, ToHub.Rotation().Yaw + 90.0f, 0.0f);
		const FVector FloorLocation(StationLocation.X, StationLocation.Y, CenterZForBlockBottom(1.05f, 0.035f));
		const FVector PanelLocation = StationLocation + ToHub * 105.0f + FVector(0.0f, 0.0f, 128.0f);
		const auto PanelPoint = [&PanelLocation, &PanelRotation](const FVector& Offset)
		{
			return PanelLocation + PanelRotation.RotateVector(Offset);
		};
		SpawnBlock(FloorLocation, FVector(1.65f, 1.10f, 0.035f), Accent * 0.50f, FRotator(0.0f, PanelRotation.Yaw, 0.0f), false, EBHBlockMaterial::Tiles);
		SpawnBlock(PanelLocation, FVector(1.28f, 0.045f, 0.72f), SignBack, PanelRotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, 42.0f), FVector(1.06f, 0.036f, 0.055f), Accent, PanelRotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(PanelPoint(FVector(-72.0f, -2.5f, 0.0f)), FVector(0.045f, 0.032f, 0.62f), Accent * 0.48f, PanelRotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(PanelPoint(FVector(72.0f, -2.5f, 0.0f)), FVector(0.045f, 0.032f, 0.62f), Accent * 0.48f, PanelRotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(StationLocation + ToHub * 36.0f + FVector(0.0f, 0.0f, 164.0f), FVector(0.055f, 0.055f, 1.74f), DarkMetal, PanelRotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(StationLocation + ToHub * 36.0f + FVector(0.0f, 0.0f, 254.0f), FVector(0.30f, 0.035f, 0.060f), Accent, PanelRotation, false, EBHBlockMaterial::WarningSign);
		SpawnBlock(StationLocation + ToHub * 36.0f + FVector(0.0f, 0.0f, 274.0f), FVector(0.18f, 0.030f, 0.045f), Accent * 0.70f, PanelRotation, false, EBHBlockMaterial::WarningSign);
		const FVector GlyphPlaqueLocation = StationLocation + ToHub * 66.0f + FVector(0.0f, 0.0f, 318.0f);
		SpawnBlock(GlyphPlaqueLocation, FVector(0.72f, 0.035f, 0.38f), SignBack, PanelRotation, false, EBHBlockMaterial::PaintedMetal);
		SpawnGlyphPixels(GlyphPlaqueLocation, PanelRotation, Accent, StationGlyph, FVector(-38.0f, -3.0f, 18.0f), 19.0f, 6.3f, FVector(0.070f, 0.024f, 0.026f));

		switch (StationType)
		{
		case EBHObjectiveStationType::Valve:
			SpawnBlock(PanelLocation, FVector(0.48f, 0.034f, 0.055f), Accent, PanelRotation + FRotator(0.0f, 0.0f, 45.0f), false, EBHBlockMaterial::WarningSign);
			SpawnBlock(PanelLocation, FVector(0.48f, 0.034f, 0.055f), Accent, PanelRotation + FRotator(0.0f, 0.0f, -45.0f), false, EBHBlockMaterial::WarningSign);
			break;
		case EBHObjectiveStationType::Terminal:
			SpawnBlock(PanelLocation, FVector(0.66f, 0.034f, 0.30f), Accent * 0.82f, PanelRotation, false, EBHBlockMaterial::WarningSign);
			SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, -35.0f), FVector(0.42f, 0.034f, 0.050f), Accent, PanelRotation, false, EBHBlockMaterial::WarningSign);
			break;
		case EBHObjectiveStationType::Antenna:
			SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, -10.0f), FVector(0.06f, 0.034f, 0.48f), Accent, PanelRotation, false, EBHBlockMaterial::WarningSign);
			SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, 16.0f), FVector(0.76f, 0.034f, 0.050f), Accent, PanelRotation, false, EBHBlockMaterial::WarningSign);
			break;
		case EBHObjectiveStationType::Evidence:
			SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, 4.0f), FVector(0.70f, 0.034f, 0.22f), Accent * 0.85f, PanelRotation + FRotator(0.0f, 0.0f, -8.0f), false, EBHBlockMaterial::WarningSign);
			SpawnBlock(PanelLocation + FVector(0.0f, 0.0f, -24.0f), FVector(0.52f, 0.034f, 0.045f), Accent, PanelRotation + FRotator(0.0f, 0.0f, 8.0f), false, EBHBlockMaterial::WarningSign);
			break;
		default:
			break;
		}
	};

	for (const TObjectPtr<ABHObjectiveStation>& StationPtr : ObjectiveStations)
	{
		if (const ABHObjectiveStation* Station = StationPtr.Get())
		{
			SpawnObjectiveSilhouette(Station->GetActorLocation(), Station->GetStationType());
		}
	}

	const TArray<TPair<FVector, FRotator>> LoopLockerLocations = {
		{FVector(-2225.0f, -1800.0f, 100.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(2225.0f, -1800.0f, 100.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-2225.0f, 1800.0f, 100.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(2225.0f, 1800.0f, 100.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(-550.0f, -1810.0f, 100.0f), FRotator(0.0f, 180.0f, 0.0f)},
		{FVector(550.0f, 1810.0f, 100.0f), FRotator::ZeroRotator}
	};
	for (const TPair<FVector, FRotator>& LockerSpec : LoopLockerLocations)
	{
		GetWorld()->SpawnActor<ABHLocker>(LockerSpec.Key, LockerSpec.Value);
	}
	SpawnLoopChoiceCue(FVector(-2225.0f, -1800.0f, 0.0f), 90.0f, RouteStorageAmber);
	SpawnLoopChoiceCue(FVector(2225.0f, -1800.0f, 0.0f), -90.0f, RouteLabCyan);
	SpawnLoopChoiceCue(FVector(-2225.0f, 1800.0f, 0.0f), 90.0f, RouteWardViolet);
	SpawnLoopChoiceCue(FVector(2225.0f, 1800.0f, 0.0f), -90.0f, RouteUtilityWhite);
	SpawnLoopChoiceCue(FVector(-550.0f, -1810.0f, 0.0f), 180.0f, RouteStorageAmber);
	SpawnLoopChoiceCue(FVector(550.0f, 1810.0f, 0.0f), 0.0f, RouteUtilityWhite);

	SpawnRuntimeMeshProp(FVector(-4440.0f, -2820.0f, 72.0f), FRotator(0.0f, 90.0f, 0.0f), CabinetMesh, CabinetMaterial, FVector(0.70f), true, FVector(0.90f, 0.52f, 1.35f), RouteStorageAmber * 0.45f);
	SpawnRuntimeMeshProp(FVector(-3520.0f, -2620.0f, 72.0f), FRotator(0.0f, -90.0f, 0.0f), CabinetMesh, CabinetMaterial, FVector(0.62f), true, FVector(0.82f, 0.48f, 1.25f), RouteStorageAmber * 0.40f);
	SpawnRuntimeMeshProp(FVector(3560.0f, -3270.0f, 72.0f), FRotator(0.0f, -90.0f, 0.0f), GeneratorFrontMesh, GeneratorFrontMaterial, FVector(0.62f), true, FVector(1.10f, 0.50f, 1.15f), RouteLabCyan * 0.35f);
	SpawnRuntimeMeshProp(FVector(4100.0f, -2720.0f, 70.0f), FRotator(0.0f, 180.0f, 0.0f), GeneratorSectionMesh, GeneratorSectionMaterial, FVector(0.58f), true, FVector(0.92f, 0.62f, 1.20f), RouteLabCyan * 0.30f);
	SpawnRuntimeMeshProp(FVector(-4275.0f, 2530.0f, 72.0f), FRotator(0.0f, 90.0f, 0.0f), CabinetMesh, CabinetMaterial, FVector(0.58f), true, FVector(0.82f, 0.46f, 1.20f), RouteWardViolet * 0.38f);
	SpawnRuntimeMeshProp(FVector(-3260.0f, 3240.0f, 70.0f), FRotator(0.0f, 180.0f, 0.0f), PanelMesh, PanelMaterial, FVector(0.70f), true, FVector(0.78f, 0.28f, 1.05f), RouteWardViolet * 0.42f);
	SpawnRuntimeMeshProp(FVector(360.0f, 3200.0f, 72.0f), FRotator(0.0f, 180.0f, 0.0f), PanelMesh, PanelMaterial, FVector(0.72f), true, FVector(0.82f, 0.30f, 1.05f), RouteUtilityWhite * 0.45f);
	SpawnRuntimeMeshProp(FVector(1220.0f, 3165.0f, 74.0f), FRotator(0.0f, 180.0f, 0.0f), GeneratorSectionMesh, GeneratorSectionMaterial, FVector(0.54f), true, FVector(0.88f, 0.58f, 1.05f), RouteUtilityWhite * 0.42f);
	SpawnRuntimeMeshProp(FVector(-4880.0f, 420.0f, 155.0f), FRotator(0.0f, 90.0f, 0.0f), PanelMesh, PanelMaterial, FVector(0.58f), false, FVector(0.68f, 0.08f, 0.74f), RouteExitGreen * 0.55f, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(4880.0f, -420.0f, 155.0f), FRotator(0.0f, -90.0f, 0.0f), PanelMesh, PanelMaterial, FVector(0.58f), false, FVector(0.68f, 0.08f, 0.74f), RouteExitGreen * 0.55f, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-1600.0f, -1120.0f, 292.0f), FRotator(0.0f, 90.0f, 0.0f), LightMesh, LightMaterial, FVector(0.46f), false, FVector(0.44f, 0.10f, 0.14f), RouteStorageAmber, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(1600.0f, -1120.0f, 292.0f), FRotator(0.0f, 90.0f, 0.0f), LightMesh, LightMaterial, FVector(0.46f), false, FVector(0.44f, 0.10f, 0.14f), RouteLabCyan, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-1600.0f, 1120.0f, 292.0f), FRotator(0.0f, 90.0f, 0.0f), LightMesh, LightMaterial, FVector(0.46f), false, FVector(0.44f, 0.10f, 0.14f), RouteWardViolet, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(1600.0f, 1120.0f, 292.0f), FRotator(0.0f, 90.0f, 0.0f), LightMesh, LightMaterial, FVector(0.46f), false, FVector(0.44f, 0.10f, 0.14f), RouteUtilityWhite, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-5015.0f, 0.0f, 150.0f), FRotator(0.0f, 90.0f, 0.0f), DoubleDoorFrameMesh, DoubleDoorFrameMaterial, FVector(0.84f), false, FVector(0.22f, 2.25f, 2.50f), RouteExitGreen * 0.45f);
	SpawnRuntimeMeshProp(FVector(5015.0f, 0.0f, 150.0f), FRotator(0.0f, -90.0f, 0.0f), DoubleDoorFrameMesh, DoubleDoorFrameMaterial, FVector(0.84f), false, FVector(0.22f, 2.25f, 2.50f), RouteExitGreen * 0.45f);
	SpawnRuntimeMeshProp(FVector(-5012.0f, -118.0f, 150.0f), FRotator(0.0f, 90.0f, 0.0f), DoubleDoorLeftMesh, DoubleDoorMaterial, FVector(0.80f), false, FVector(0.18f, 1.05f, 2.12f), RouteExitGreen * 0.34f);
	SpawnRuntimeMeshProp(FVector(-5012.0f, 118.0f, 150.0f), FRotator(0.0f, 90.0f, 0.0f), DoubleDoorRightMesh, DoubleDoorMaterial, FVector(0.80f), false, FVector(0.18f, 1.05f, 2.12f), RouteExitGreen * 0.34f);
	SpawnRuntimeMeshProp(FVector(5012.0f, -118.0f, 150.0f), FRotator(0.0f, -90.0f, 0.0f), DoubleDoorRightMesh, DoubleDoorMaterial, FVector(0.80f), false, FVector(0.18f, 1.05f, 2.12f), RouteExitGreen * 0.34f);
	SpawnRuntimeMeshProp(FVector(5012.0f, 118.0f, 150.0f), FRotator(0.0f, -90.0f, 0.0f), DoubleDoorLeftMesh, DoubleDoorMaterial, FVector(0.80f), false, FVector(0.18f, 1.05f, 2.12f), RouteExitGreen * 0.34f);
	SpawnRuntimeMeshProp(FVector(-4920.0f, -260.0f, 132.0f), FRotator(0.0f, 90.0f, 0.0f), CardReaderMesh, CardReaderMaterial, FVector(0.46f), false, FVector(0.18f, 0.08f, 0.36f), RouteExitGreen, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(4920.0f, 260.0f, 132.0f), FRotator(0.0f, -90.0f, 0.0f), CardReaderMesh, CardReaderMaterial, FVector(0.46f), false, FVector(0.18f, 0.08f, 0.36f), RouteExitGreen, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-4590.0f, -3020.0f, 55.0f), FRotator(0.0f, 45.0f, 0.0f), GasCanMesh, GasCanMaterial, FVector(0.62f), true, FVector(0.34f, 0.34f, 0.74f), RouteStorageAmber * 0.42f);
	SpawnRuntimeMeshProp(FVector(-4210.0f, -2835.0f, 78.0f), FRotator(0.0f, 88.0f, 0.0f), CabinetDrawerMesh, CabinetDrawerMaterial, FVector(0.52f), false, FVector(0.72f, 0.22f, 0.30f), RouteStorageAmber * 0.36f);
	SpawnRuntimeMeshProp(FVector(-3705.0f, -2555.0f, 78.0f), FRotator(0.0f, -92.0f, 0.0f), CabinetDrawerMesh, CabinetDrawerMaterial, FVector(0.48f), false, FVector(0.66f, 0.22f, 0.28f), RouteStorageAmber * 0.34f);
	SpawnRuntimeMeshProp(FVector(3330.0f, -2360.0f, 118.0f), FRotator(0.0f, 180.0f, 0.0f), PowerButtonMesh, PowerButtonMaterial, FVector(0.42f), false, FVector(0.24f, 0.10f, 0.34f), RouteLabCyan, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(2830.0f, -1190.0f, 118.0f), FRotator(0.0f, 0.0f, 0.0f), CardMesh, CardMaterial, FVector(0.38f), false, FVector(0.28f, 0.018f, 0.16f), RouteLabCyan, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-4020.0f, 2380.0f, 170.0f), FRotator(0.0f, 90.0f, 0.0f), AlarmMesh, AlarmMaterial, FVector(0.44f), false, FVector(0.30f, 0.08f, 0.30f), RouteWardViolet, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-2890.0f, 1175.0f, 118.0f), FRotator(0.0f, 180.0f, 0.0f), CardMesh, CardMaterial, FVector(0.36f), false, FVector(0.26f, 0.018f, 0.15f), RouteWardViolet, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(1180.0f, 2455.0f, 132.0f), FRotator(0.0f, 180.0f, 0.0f), PowerButtonMesh, PowerButtonMaterial, FVector(0.44f), false, FVector(0.24f, 0.10f, 0.34f), RouteUtilityWhite, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-1120.0f, -540.0f, 162.0f), FRotator::ZeroRotator, PanelMesh, ScreenPanel1Material, FVector(0.50f), false, FVector(0.66f, 0.08f, 0.52f), RouteStorageAmber * 0.48f, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(1120.0f, -540.0f, 162.0f), FRotator::ZeroRotator, PanelMesh, ScreenPanel2Material, FVector(0.50f), false, FVector(0.66f, 0.08f, 0.52f), RouteLabCyan * 0.48f, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(-1120.0f, 540.0f, 162.0f), FRotator(0.0f, 180.0f, 0.0f), PanelMesh, ScreenPanel3Material, FVector(0.50f), false, FVector(0.66f, 0.08f, 0.52f), RouteWardViolet * 0.48f, EBHBlockMaterial::WarningSign);
	SpawnRuntimeMeshProp(FVector(1120.0f, 540.0f, 162.0f), FRotator(0.0f, 180.0f, 0.0f), PanelMesh, ScreenPanel1Material, FVector(0.50f), false, FVector(0.66f, 0.08f, 0.52f), RouteUtilityWhite * 0.48f, EBHBlockMaterial::WarningSign);

	const struct FRouteLightSpec { FVector Location; FLinearColor Color; float Intensity; float Radius; } RouteLights[] = {
		{FVector(-2150.0f, -1350.0f, 282.0f), RouteStorageAmber, 620.0f, 780.0f},
		{FVector(2450.0f, -1350.0f, 282.0f), RouteLabCyan, 660.0f, 820.0f},
		{FVector(-2550.0f, 1350.0f, 282.0f), RouteWardViolet, 620.0f, 780.0f},
		{FVector(250.0f, 1350.0f, 282.0f), RouteUtilityWhite, 560.0f, 780.0f},
		{FVector(-3820.0f, 0.0f, 282.0f), RouteExitGreen, 720.0f, 900.0f},
		{FVector(3820.0f, 0.0f, 282.0f), RouteExitGreen, 720.0f, 900.0f},
		{FVector(-4680.0f, 0.0f, 288.0f), RouteExitGreen, 620.0f, 760.0f},
		{FVector(4680.0f, 0.0f, 288.0f), RouteExitGreen, 620.0f, 760.0f},
		{FVector(-2150.0f, -1185.0f, 288.0f), RouteStorageAmber, 420.0f, 620.0f},
		{FVector(2450.0f, -1185.0f, 288.0f), RouteLabCyan, 440.0f, 620.0f},
		{FVector(-2550.0f, 1185.0f, 288.0f), RouteWardViolet, 420.0f, 620.0f},
		{FVector(250.0f, 1185.0f, 288.0f), RouteUtilityWhite, 400.0f, 620.0f}
	};
	for (const FRouteLightSpec& Spec : RouteLights)
	{
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(Spec.Location, FRotator::ZeroRotator))
		{
			Light->Configure(0, Spec.Color, Spec.Intensity, Spec.Radius);
			FlickerLights.Add(Light);
		}
	}
}

void ABHGameMode::AddMoodPass(const FLinearColor& FogColor, float FogDensity, float VignetteIntensity, float FilmGrainIntensity)
{
	const bool bFoggrounds = RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const bool bExtremeFog = bFoggrounds && RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = bFoggrounds && RuntimeFogPreset == EBHFogPreset::Heavy;
	if (APostProcessVolume* PostProcess = GetWorld()->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		PostProcess->bUnbound = true;
		PostProcess->Settings.bOverride_VignetteIntensity = true;
		PostProcess->Settings.VignetteIntensity = VignetteIntensity;
		PostProcess->Settings.bOverride_FilmGrainIntensity = true;
		PostProcess->Settings.FilmGrainIntensity = FilmGrainIntensity;
		PostProcess->Settings.bOverride_ColorSaturation = true;
		PostProcess->Settings.ColorSaturation = bFoggrounds
			? (bExtremeFog ? FVector4(1.02f, 1.04f, 1.04f, 1.0f) : (bHeavyFog ? FVector4(1.00f, 1.03f, 1.03f, 1.0f) : FVector4(0.96f, 0.99f, 1.00f, 1.0f)))
			: FVector4(0.78f, 0.84f, 0.90f, 1.0f);
		PostProcess->Settings.bOverride_ColorContrast = true;
		PostProcess->Settings.ColorContrast = bFoggrounds
			? (bExtremeFog ? FVector4(0.94f, 0.95f, 0.95f, 1.0f) : (bHeavyFog ? FVector4(0.96f, 0.97f, 0.97f, 1.0f) : FVector4(1.00f, 1.00f, 0.98f, 1.0f)))
			: FVector4(1.12f, 1.10f, 1.06f, 1.0f);
		const float FoggroundsFixedExposure = bExtremeFog ? 1.28f : (bHeavyFog ? 1.18f : 1.08f);
		PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
		PostProcess->Settings.AutoExposureMinBrightness = bFoggrounds ? FoggroundsFixedExposure : 0.030f;
		PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
		// Indoor maps keep the low exposure floor (0.030) above so dark corners stay readable, but the eye may
		// only adapt up to a tight ceiling. Previously this was 0.95, so aiming the flashlight at a near wall
		// filled the frame with bright pixels, auto-exposure adapted toward 0.95, and the whole scene crushed to
		// near-black -- then crept back at the engine's slow default SpeedDown when the player turned away. The
		// tight ceiling caps that darkening (the floor sits below normal-play luminance, so ordinary dark-room
		// play is unchanged) and the brisk symmetric speeds below make any residual change snap back.
		PostProcess->Settings.AutoExposureMaxBrightness = bFoggrounds ? FoggroundsFixedExposure : BHIndoorAutoExposureMaxBrightness;
		if (!bFoggrounds)
		{
			PostProcess->Settings.bOverride_AutoExposureSpeedUp = true;
			PostProcess->Settings.AutoExposureSpeedUp = BHAutoExposureAdaptSpeed;
			PostProcess->Settings.bOverride_AutoExposureSpeedDown = true;
			PostProcess->Settings.AutoExposureSpeedDown = BHAutoExposureAdaptSpeed;
		}
		if (bFoggrounds)
		{
			PostProcess->Settings.bOverride_AutoExposureBias = true;
			PostProcess->Settings.AutoExposureBias = bExtremeFog ? 0.42f : (bHeavyFog ? 0.34f : 0.18f);
			PostProcess->Settings.bOverride_SceneColorTint = true;
			PostProcess->Settings.SceneColorTint = bExtremeFog ? FLinearColor(1.13f, 1.18f, 1.22f, 1.0f) : FLinearColor(1.08f, 1.14f, 1.18f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingColor = true;
			PostProcess->Settings.IndirectLightingColor = FLinearColor(0.48f, 0.62f, 0.78f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingIntensity = true;
			PostProcess->Settings.IndirectLightingIntensity = bExtremeFog ? 1.12f : (bHeavyFog ? 0.98f : 0.62f);
		}
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			// Substation gets a low, ground-hugging fog (high height-falloff) so the haze pools near the floor as a
			// light mist for atmosphere instead of filling the room; other indoor maps keep the thin, even height fog.
			const bool bSubstationMist = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase);
			// Facility (the first, tight map) gets the same ground-hugging treatment as Substation but lighter, so
			// its faint red tint pools as a very slight mist on the floor instead of filling the room.
			const bool bFacilityMist = RuntimeLevelName.Equals(TEXT("Facility"), ESearchCase::IgnoreCase);
			const bool bGroundMist = bSubstationMist || bFacilityMist;
			FogComponent->SetFogDensity(bSubstationMist ? 0.018f : FogDensity);
			FogComponent->SetFogHeightFalloff(bFoggrounds ? (bExtremeFog ? 0.034f : 0.036f) : (bGroundMist ? 0.34f : 0.075f));
			FogComponent->SetFogInscatteringColor(FogColor);
			FogComponent->SetFogMaxOpacity(bFoggrounds ? (bExtremeFog ? 0.96f : (bHeavyFog ? 0.92f : 0.80f)) : 1.0f);
			FogComponent->SetStartDistance(bFoggrounds ? (bExtremeFog ? 80.0f : 110.0f) : 0.0f);
			FogComponent->SetEndDistance(bFoggrounds ? 7400.0f : 0.0f);
			FogComponent->SetFogCutoffDistance(0.0f);
			FogComponent->SetDirectionalInscatteringExponent(8.0f);
			FogComponent->SetDirectionalInscatteringStartDistance(0.0f);
			const float DirectionalInscatterScale = bFoggrounds ? (bExtremeFog ? 2.20f : 2.00f) : (bExtremeFog ? 1.75f : 1.45f);
			FogComponent->SetDirectionalInscatteringColor(FLinearColor(FogColor.R * DirectionalInscatterScale, FogColor.G * DirectionalInscatterScale, FogColor.B * DirectionalInscatterScale, 1.0f));
			FogComponent->SetVolumetricFog(true);
			FogComponent->SetVolumetricFogScatteringDistribution(bFoggrounds ? (bExtremeFog ? 0.18f : 0.22f) : (bExtremeFog ? 0.38f : 0.45f));
			FogComponent->SetVolumetricFogAlbedo(FColor(
				FMath::Clamp(FMath::RoundToInt(FogColor.R * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.G * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.B * 255.0f), 0, 255)));
			const float FogEmissiveScale = bFoggrounds ? (bExtremeFog ? 0.135f : 0.115f) : (bExtremeFog ? 0.030f : 0.018f);
			FogComponent->SetVolumetricFogEmissive(FLinearColor(FogColor.R * FogEmissiveScale, FogColor.G * FogEmissiveScale, FogColor.B * FogEmissiveScale, 1.0f));
			const float FogExtinctionMax = bFoggrounds ? (bExtremeFog ? 4.8f : (bHeavyFog ? 3.7f : 2.3f)) : (bExtremeFog ? 2.65f : 3.25f);
			FogComponent->SetVolumetricFogExtinctionScale(FMath::Clamp(FogDensity * (bFoggrounds ? 6.5f : 18.0f), 0.32f, FogExtinctionMax));
			FogComponent->SetVolumetricFogDistance(bFoggrounds ? (bExtremeFog ? 7200.0f : 7800.0f) : (bExtremeFog ? 4200.0f : 5200.0f));
			FogComponent->SetVolumetricFogStartDistance(bFoggrounds ? 100.0f : 0.0f);
			FogComponent->SetVolumetricFogNearFadeInDistance(bFoggrounds ? 180.0f : (bExtremeFog ? 75.0f : 55.0f));

			// Substation (the middle map): a denser, strongly ground-concentrated second fog layer pools a
			// visible-but-light mist on the floor that fades out by standing height. Volumetric fog picks it up
			// so it reads as drifting ground mist rather than a flat screen tint. No extra actor needed.
			if (bSubstationMist)
			{
				FogComponent->SetSecondFogData(FExponentialHeightFogData());
				FogComponent->SetSecondFogDensity(0.22f);
				FogComponent->SetSecondFogHeightFalloff(1.0f);
				FogComponent->SetSecondFogHeightOffset(0.0f);
			}
			else if (bFacilityMist)
			{
				// Same floor-pooling second layer as Substation but much lighter, so the red reads as a barely-there
				// haze hugging the ground rather than a visible mist bank.
				FogComponent->SetSecondFogData(FExponentialHeightFogData());
				FogComponent->SetSecondFogDensity(0.08f);
				FogComponent->SetSecondFogHeightFalloff(1.0f);
				FogComponent->SetSecondFogHeightOffset(0.0f);
			}
		}
	}
}

ABHStaticBlockField* ABHGameMode::EnsureStaticBlockField()
{
	// Per-shard spec cap. Under Large World Coordinates FBHStaticBlockSpec serializes to ~90 bytes (double
	// FVector/FRotator), so 300 specs keeps a shard's replicated array near ~27KB — a comfortable 2x margin
	// below the engine's single-bunch limit (~64KB), past which UChannel::SendBunch drops the bunch and
	// clients never receive the geometry. A dense level simply spans several shards.
	static constexpr int32 MaxSpecsPerShard = 300;

	if (IsValid(StaticBlockField) && StaticBlockField->GetStaticBlockCount() < MaxSpecsPerShard)
	{
		return StaticBlockField;
	}

	if (!GetWorld())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	StaticBlockField = GetWorld()->SpawnActor<ABHStaticBlockField>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (StaticBlockField)
	{
		StaticBlockFields.Add(StaticBlockField);
	}
	return StaticBlockField;
}

void ABHGameMode::ResetStaticBlockField()
{
	// Destroy every shard so the next build starts from an empty field. EnsureStaticBlockField re-spawns the
	// first shard on the next AddStaticBlock call.
	for (ABHStaticBlockField* Field : StaticBlockFields)
	{
		if (IsValid(Field))
		{
			Field->Destroy();
		}
	}
	StaticBlockFields.Reset();
	StaticBlockField = nullptr;
}

void ABHGameMode::FinalizeStaticBlockField()
{
	for (ABHStaticBlockField* Field : StaticBlockFields)
	{
		if (IsValid(Field))
		{
			Field->FinalizeBuild();
		}
	}
}

void ABHGameMode::AddStaticBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material, bool bStartHidden)
{
#if WITH_EDITOR
	// Authored export: emit a real, editable, hard-referenced AStaticMeshActor instead of a runtime
	// block-field spec (which is invisible in-editor until BeginPlay rebuilds the instanced components).
	if (bAuthoringExport)
	{
		SpawnAuthoredBlockActor(Location, Scale, Tint, Rotation, bCollides, Material, bStartHidden);
		return;
	}
#endif

	ABHStaticBlockField* Field = EnsureStaticBlockField();
	if (!Field)
	{
		return;
	}

	FBHStaticBlockSpec Spec;
	Spec.Location = Location;
	Spec.Scale = Scale;
	Spec.Rotation = Rotation;
	Spec.Tint = Tint;
	Spec.Material = Material;
	Spec.bCollides = bCollides;
	Spec.bHidden = bStartHidden;
	Field->AddBlockSpec(Spec);
}

ABHBlockActor* ABHGameMode::SpawnDynamicBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	if (ABHBlockActor* Block = GetWorld()->SpawnActor<ABHBlockActor>(Location, Rotation))
	{
		Block->SetActorScale3D(Scale);
		Block->SetVisualTint(Tint);
		Block->SetBlockMaterial(Material);
		Block->SetBlockCollisionEnabled(bCollides);
		return Block;
	}

	return nullptr;
}

ABHBlockActor* ABHGameMode::SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material)
{
	AddStaticBlock(Location, Scale, Tint, Rotation, bCollides, Material, false);
	return nullptr;
}

ABHBreakableGlassPane* ABHGameMode::SpawnBreakableGlassPane(const FVector& Location, const FVector& Scale, const FRotator& Rotation, const FText& Label)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	ABHBreakableGlassPane* GlassPane = GetWorld()->SpawnActor<ABHBreakableGlassPane>(Location, Rotation);
	if (GlassPane)
	{
		GlassPane->SetActorScale3D(Scale);
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		GlassPane->ConfigurePane(Label.IsEmpty() ? FText::FromString(TEXT("Break Glass")) : Label, Settings ? Settings->GlassBreakNoiseStrength : 1.15f);
	}
	return GlassPane;
}

ABHFootstepSurfaceVolume* ABHGameMode::SpawnSurfacePatch(const FVector& Location, const FVector& Extent, EBHFootstepSurface Surface)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	ABHFootstepSurfaceVolume* Volume = GetWorld()->SpawnActor<ABHFootstepSurfaceVolume>(Location, FRotator::ZeroRotator);
	if (Volume)
	{
		Volume->ConfigureSurfaceVolume(Surface, Extent);
	}

	const FLinearColor Tint = [Surface]()
	{
		switch (Surface)
		{
		case EBHFootstepSurface::Metal:
			return FLinearColor(0.18f, 0.19f, 0.18f, 1.0f);
		case EBHFootstepSurface::Tile:
			return FLinearColor(0.55f, 0.58f, 0.54f, 1.0f);
		case EBHFootstepSurface::Wet:
			return FLinearColor(0.035f, 0.070f, 0.085f, 1.0f);
		case EBHFootstepSurface::Gravel:
			return FLinearColor(0.12f, 0.12f, 0.105f, 1.0f);
		case EBHFootstepSurface::Soft:
			return FLinearColor(0.045f, 0.075f, 0.048f, 1.0f);
		case EBHFootstepSurface::Glass:
			return FLinearColor(0.18f, 0.44f, 0.56f, 1.0f);
		default:
			return FLinearColor(0.09f, 0.10f, 0.10f, 1.0f);
		}
	}();
	const EBHBlockMaterial Material = Surface == EBHFootstepSurface::Metal
		? EBHBlockMaterial::DiamondPlate
		: (Surface == EBHFootstepSurface::Tile ? EBHBlockMaterial::Tiles : EBHBlockMaterial::RustedMetal);
	SpawnBlock(Location - FVector(0.0f, 0.0f, 23.0f), FVector(FMath::Max(0.1f, Extent.X / 100.0f), FMath::Max(0.1f, Extent.Y / 100.0f), 0.025f), Tint, FRotator::ZeroRotator, false, Material);
	return Volume;
}

void ABHGameMode::SpawnHiddenBlocker(const FVector& Location, const FVector& Scale)
{
	AddStaticBlock(Location, Scale, FLinearColor::Black, FRotator::ZeroRotator, true, EBHBlockMaterial::Tinted, true);
}

void ABHGameMode::RegisterStationSignalBlock(ABHBlockActor* Block)
{
	if (Block)
	{
		StationSignalBlocks.AddUnique(Block);
	}
}

void ABHGameMode::RegisterStationSignalLight(ABHFlickerLight* Light)
{
	if (Light)
	{
		StationSignalLights.AddUnique(Light);
	}
}

void ABHGameMode::UpdateStationSignalState(bool bExitOpen)
{
	const FLinearColor LockedRed(0.58f, 0.035f, 0.025f, 1.0f);
	const FLinearColor OpenGreen(0.14f, 0.96f, 0.44f, 1.0f);
	const FLinearColor OpenWhite(0.88f, 0.96f, 0.90f, 1.0f);
	for (ABHBlockActor* Block : StationSignalBlocks)
	{
		if (Block)
		{
			Block->SetVisualTint(bExitOpen ? OpenGreen : LockedRed);
		}
	}
	for (int32 Index = 0; Index < StationSignalLights.Num(); ++Index)
	{
		ABHFlickerLight* Light = StationSignalLights[Index];
		if (!Light)
		{
			continue;
		}
		Light->Configure(0, bExitOpen ? (Index % 2 == 0 ? OpenWhite : OpenGreen) : LockedRed, bExitOpen ? 1250.0f : 280.0f, bExitOpen ? 1150.0f : 720.0f);
		Light->SetPowered(true);
	}
}

void ABHGameMode::AddMapContainment(float HalfX, float HalfY)
{
	constexpr float BlockerThickness = 0.90f;
	constexpr float CornerPlugScale = 1.85f;
	constexpr float BlockerHeight = 4.20f;
	const float BlockerZ = CenterZForBlockBottom(-55.0f, BlockerHeight);
	const float ScaleX = (HalfX * 2.0f) / 100.0f;
	const float ScaleY = (HalfY * 2.0f) / 100.0f;

	SpawnHiddenBlocker(FVector(0.0f, -HalfY, BlockerZ), FVector(ScaleX, BlockerThickness, BlockerHeight));
	SpawnHiddenBlocker(FVector(0.0f, HalfY, BlockerZ), FVector(ScaleX, BlockerThickness, BlockerHeight));
	SpawnHiddenBlocker(FVector(-HalfX, 0.0f, BlockerZ), FVector(BlockerThickness, ScaleY, BlockerHeight));
	SpawnHiddenBlocker(FVector(HalfX, 0.0f, BlockerZ), FVector(BlockerThickness, ScaleY, BlockerHeight));

	SpawnHiddenBlocker(FVector(-HalfX, -HalfY, BlockerZ), FVector(CornerPlugScale, CornerPlugScale, BlockerHeight));
	SpawnHiddenBlocker(FVector(-HalfX, HalfY, BlockerZ), FVector(CornerPlugScale, CornerPlugScale, BlockerHeight));
	SpawnHiddenBlocker(FVector(HalfX, -HalfY, BlockerZ), FVector(CornerPlugScale, CornerPlugScale, BlockerHeight));
	SpawnHiddenBlocker(FVector(HalfX, HalfY, BlockerZ), FVector(CornerPlugScale, CornerPlugScale, BlockerHeight));
}

void ABHGameMode::RecoverPlayersFromVoid()
{
	const AGameStateBase* BaseGameState = GameState;
	if (!BaseGameState)
	{
		return;
	}

	constexpr float VoidZThreshold = -650.0f;
	for (APlayerState* RawPS : BaseGameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		ABHCharacter* Character = FindCharacterForPlayerState(BHPS);
		if (!Character || Character->GetActorLocation().Z >= VoidZThreshold)
		{
			continue;
		}

		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}

		const FVector RecoveryLocation = GetVoidRecoveryLocationFor(Character);
		Character->SetActorLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(Character->GetController()))
		{
			PC->ClientShowStatusMessage(TEXT("Recovered from map edge."), 2.0f);
		}
	}
}

FVector ABHGameMode::GetVoidRecoveryLocationFor(const ABHCharacter* Character) const
{
	TArray<FVector> Candidates = SurvivorSpawns;
	if (!HunterSpawn.IsNearlyZero())
	{
		Candidates.Add(HunterSpawn);
	}
	if (Candidates.IsEmpty())
	{
		return FVector(0.0f, 0.0f, 160.0f);
	}

	const FVector CurrentLocation = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	FVector BestLocation = Candidates[0];
	float BestDistanceSquared = FVector::DistSquared2D(CurrentLocation, BestLocation);
	for (int32 Index = 1; Index < Candidates.Num(); ++Index)
	{
		const float CandidateDistanceSquared = FVector::DistSquared2D(CurrentLocation, Candidates[Index]);
		if (CandidateDistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = CandidateDistanceSquared;
			BestLocation = Candidates[Index];
		}
	}

	BestLocation.Z = FMath::Max(BestLocation.Z, 145.0f);
	return BestLocation;
}

void ABHGameMode::SpawnAmbient(const FVector& Location, float Frequency, float Volume, float Noise, float Pulse, float LifeSpan)
{
	// Single chokepoint for every dread audio drone. Suppressed during calm tutorial teaching beats; allowed during
	// the scripted chase climax (bTutorialHorrorAllowed) and never gated outside tutorial mode.
	if (IsTutorialHorrorSuppressed())
	{
		return;
	}
	if (ABHAmbientEmitter* Emitter = GetWorld()->SpawnActor<ABHAmbientEmitter>(Location, FRotator::ZeroRotator))
	{
		Emitter->Configure(Frequency, Volume, Noise, Pulse);
		if (LifeSpan > 0.0f)
		{
			Emitter->SetLifeSpan(LifeSpan);
		}
	}
}

// QA / replay reproduction: RoundSeed is logged into telemetry but sourced from FMath::Rand(), so a recorded
// round can't be re-run. Set bh.RoundSeedOverride to a logged value (>= 0) to force that seed; -1 (default)
// keeps the per-round randomness. Downstream derivation is already deterministic per-seed.
static TAutoConsoleVariable<int32> CVarBHRoundSeedOverride(
	TEXT("bh.RoundSeedOverride"),
	-1,
	TEXT("If >= 0, forces the per-round RoundSeed to this value so a recorded round can be reproduced (replay/QA). -1 = random."),
	ECVF_Default);

// Cosmetic-only easter eggs (locker graffiti, physicist-name nods, a rare calm whisper, a menu code). These
// never affect scoring, mastery, reports, fairness, or stability. Default on; a teacher can disable them all
// with `bh.EasterEggs 0`. See Docs/EASTER_EGGS.md.
static TAutoConsoleVariable<int32> CVarBHEasterEggs(
	TEXT("bh.EasterEggs"),
	1,
	TEXT("1 (default) = harmless cosmetic easter eggs on; 0 = off (for a strictly plain classroom session)."),
	ECVF_Default);

bool BHAreEasterEggsEnabled()
{
	return CVarBHEasterEggs.GetValueOnGameThread() != 0;
}

void ABHGameMode::PrepareRoundDirector()
{
	const int32 RoundSeedOverride = CVarBHRoundSeedOverride.GetValueOnGameThread();
	RoundSeed = (RoundSeedOverride >= 0) ? RoundSeedOverride : FMath::Rand();
	FRandomStream Stream(RoundSeed);
	const EBHRoundModifier ChosenModifier = bPracticeMode ? PracticeRoundModifier : ChooseRoundModifier(Stream);
	ApplyRoundCCTVVisibility(ChosenModifier);

	BreakerActors.RemoveAll([](const TObjectPtr<ABHBreaker>& Breaker) { return !IsValid(Breaker); });
	DoorActors.RemoveAll([](const TObjectPtr<ABHDoor>& Door) { return !IsValid(Door); });
	ExitGates.RemoveAll([](const TObjectPtr<ABHExitGate>& ExitGate) { return !IsValid(ExitGate); });
	FlickerLights.RemoveAll([](const TObjectPtr<ABHFlickerLight>& Light) { return !IsValid(Light); });
	ObjectiveStations.RemoveAll([](const TObjectPtr<ABHObjectiveStation>& Station) { return !IsValid(Station); });

	NoiseRadiusMultiplier = ChosenModifier == EBHRoundModifier::LoudFooting ? 1.55f : 1.0f;
	if (bPartyPace)
	{
		NoiseRadiusMultiplier *= 1.25f;
	}

	TArray<int32> BreakerOrder;
	for (int32 Index = 0; Index < BreakerActors.Num(); ++Index)
	{
		BreakerOrder.Add(Index);
	}
	for (int32 Index = BreakerOrder.Num() - 1; Index > 0; --Index)
	{
		BreakerOrder.Swap(Index, Stream.RandRange(0, Index));
	}

	ActiveSideObjectiveCount = FMath::Clamp(ObjectiveIntensity, 0, ObjectiveStations.Num());
	if (bTestMode)
	{
		ActiveSideObjectiveCount = ObjectiveStations.Num();
	}
	else if (bRevisionMode)
	{
		int32 StudentCount = CountActiveRevisionStudents(GameState, true);
		if (StudentCount <= 0)
		{
			StudentCount = CountActiveRevisionStudents(GameState, false);
		}
		ActiveSideObjectiveCount = ResolveRevisionNodeTargetFor(StudentCount, RuntimeStageIndex, ObjectiveStations.Num());
	}
	else if (bPartyPace && ObjectiveIntensity > 0)
	{
		ActiveSideObjectiveCount = FMath::Max(1, ObjectiveIntensity - 1);
	}
	// Never require more active breakers than physically exist. The inner Clamp floors the requirement at 2,
	// but FMath::Clamp(X, 2, Max(1, N)) has min>max when N<=1 and then returns the min (2) — so a map with a
	// single breaker (e.g. an authored map, or a partial spawn failure) would require 2 completions from 1
	// breaker and the exit could never unlock (unwinnable). Wrapping in Min(N, ...) is a no-op for the shipped
	// 7-8 breaker maps (the clamp result is already <= N there) and makes the degenerate cases satisfiable.
	ActiveBreakerCount = bTestMode ? BreakerActors.Num() : (bRevisionMode ? 0 : FMath::Min(BreakerActors.Num(), FMath::Clamp(RequiredBreakers - FMath::Min(2, ActiveSideObjectiveCount), 2, FMath::Max(1, BreakerActors.Num()))));
	TSet<int32> ActiveBreakerIndexes;
	for (int32 Index = 0; Index < ActiveBreakerCount && BreakerOrder.IsValidIndex(Index); ++Index)
	{
		ActiveBreakerIndexes.Add(BreakerOrder[Index]);
	}

	for (int32 Index = 0; Index < BreakerActors.Num(); ++Index)
	{
		if (ABHBreaker* Breaker = BreakerActors[Index])
		{
			Breaker->SetDirectorActive(ActiveBreakerIndexes.Contains(Index));
		}
	}

	TSet<int32> ActiveStationIndexes;
	if (bRevisionMode)
	{
		const EBHObjectiveStationType TopicStationTypes[4] = {
			EBHObjectiveStationType::Valve,
			EBHObjectiveStationType::Terminal,
			EBHObjectiveStationType::Antenna,
			EBHObjectiveStationType::Evidence
		};
		for (int32 TopicSlot = 0; TopicSlot < 4; ++TopicSlot)
		{
			const EBHPhysicsTopic Topic = FBHRevisionQuestionBank::TopicForStationType(TopicStationTypes[TopicSlot]);
			if ((RevisionTopicMask & FBHRevisionQuestionBank::TopicMaskBit(Topic)) == 0)
			{
				continue;
			}

			TArray<int32> Candidates;
			for (int32 StationIndex = 0; StationIndex < ObjectiveStations.Num(); ++StationIndex)
			{
				ABHObjectiveStation* Station = ObjectiveStations[StationIndex];
				if (Station && Station->GetStationType() == TopicStationTypes[TopicSlot])
				{
					Candidates.Add(StationIndex);
				}
			}

			if (Candidates.IsValidIndex(0))
			{
				ActiveStationIndexes.Add(Candidates[Stream.RandRange(0, Candidates.Num() - 1)]);
			}
		}

		auto FillRevisionStations = [&](bool bSelectedTopicsOnly)
		{
			TArray<int32> StationOrder;
			for (int32 StationIndex = 0; StationIndex < ObjectiveStations.Num(); ++StationIndex)
			{
				ABHObjectiveStation* Station = ObjectiveStations[StationIndex];
				if (!Station || ActiveStationIndexes.Contains(StationIndex))
				{
					continue;
				}
				if (bSelectedTopicsOnly)
				{
					const EBHPhysicsTopic StationTopic = FBHRevisionQuestionBank::TopicForStationType(Station->GetStationType());
					if ((RevisionTopicMask & FBHRevisionQuestionBank::TopicMaskBit(StationTopic)) == 0)
					{
						continue;
					}
				}
				StationOrder.Add(StationIndex);
			}
			for (int32 Index = StationOrder.Num() - 1; Index > 0; --Index)
			{
				StationOrder.Swap(Index, Stream.RandRange(0, Index));
			}
			for (const int32 StationIndex : StationOrder)
			{
				ActiveStationIndexes.Add(StationIndex);
				if (ActiveStationIndexes.Num() >= ActiveSideObjectiveCount)
				{
					break;
				}
			}
		};

		if (ActiveStationIndexes.Num() < ActiveSideObjectiveCount)
		{
			FillRevisionStations(true);
		}
		if (ActiveStationIndexes.Num() < ActiveSideObjectiveCount)
		{
			FillRevisionStations(false);
		}
		// Every node is available in revision now: there is no random active subset. Students roam (in
		// groups if they like) and each answers their own questions, so light up every objective station.
		for (int32 AllIndex = 0; AllIndex < ObjectiveStations.Num(); ++AllIndex)
		{
			if (ObjectiveStations[AllIndex])
			{
				ActiveStationIndexes.Add(AllIndex);
			}
		}
		ActiveSideObjectiveCount = ActiveStationIndexes.Num();
	}
	else
	{
		TArray<int32> StationOrder;
		for (int32 Index = 0; Index < ObjectiveStations.Num(); ++Index)
		{
			StationOrder.Add(Index);
		}
		for (int32 Index = StationOrder.Num() - 1; Index > 0; --Index)
		{
			StationOrder.Swap(Index, Stream.RandRange(0, Index));
		}
		for (int32 Index = 0; Index < ActiveSideObjectiveCount && StationOrder.IsValidIndex(Index); ++Index)
		{
			ActiveStationIndexes.Add(StationOrder[Index]);
		}
	}
	for (int32 Index = 0; Index < ObjectiveStations.Num(); ++Index)
	{
		if (ABHObjectiveStation* Station = ObjectiveStations[Index])
		{
			Station->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::None);
			Station->SetDirectorActive(ActiveStationIndexes.Contains(Index));
		}
	}
	if (bRevisionMode)
	{
		TArray<int32> CounterCandidates = ActiveStationIndexes.Array();
		CounterCandidates.Sort([](int32 Left, int32 Right)
		{
			return Left > Right;
		});
		if (CounterCandidates.IsValidIndex(0))
		{
			if (ABHObjectiveStation* PeerReview = ObjectiveStations[CounterCandidates[0]])
			{
				PeerReview->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::PeerReview);
			}
		}
		if (CounterCandidates.IsValidIndex(1))
		{
			if (ABHObjectiveStation* DemonstrationTrap = ObjectiveStations[CounterCandidates[1]])
			{
				DemonstrationTrap->ConfigureRevisionCounterNode(EBHRevisionCounterNodeType::DemonstrationTrap);
			}
		}
	}

	const int32 ActiveExitIndex = ExitGates.Num() > 0 ? Stream.RandRange(0, ExitGates.Num() - 1) : INDEX_NONE;
	for (int32 Index = 0; Index < ExitGates.Num(); ++Index)
	{
		if (ABHExitGate* ExitGate = ExitGates[Index])
		{
			ExitGate->SetDirectorActive(!IsFinalStage() && (bTestMode || Index == ActiveExitIndex));
		}
	}

	for (int32 Index = 0; Index < DoorActors.Num(); ++Index)
	{
		if (ABHDoor* Door = DoorActors[Index])
		{
			const float OpenChance = ChosenModifier == EBHRoundModifier::JammedDoors ? 0.08f : 0.28f;
			Door->SetOpen(Stream.FRand() < OpenChance);
		}
	}

	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0)
		{
			const float OutageChance = ChosenModifier == EBHRoundModifier::LightsOut ? 0.52f : 0.18f;
			Light->SetPowered(Stream.FRand() >= OutageChance);
		}
	}

	LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -999.0f;
	LastMonsterChargeTime = LastDirectorScareTime - 999.0f;
	LastColdCallTime = LastDirectorScareTime - 999.0f;
	// Wait out one full hand-out cooldown into the Hunt before the first decoy drop, so the opening minute
	// stays clean and the first hand-out feels like a mid-hunt event.
	LastDecoyHandoutTime = LastDirectorScareTime;
	LastPresenceWhisperTime = LastDirectorScareTime - 999.0f;
	LastWhisperJumpscareTime = LastDirectorScareTime - 999.0f;
	LastPresenceSpikeTime = LastDirectorScareTime - 999.0f;
	LastTeacherCounterScareTime = LastDirectorScareTime - 999.0f;
	LastTeacherProperScareTime = LastDirectorScareTime - 999.0f;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, ChosenModifier);
		BHGS->SetPresenceState(12.0f, TEXT("The building is listening."), 0);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		// Freeze the hall-monitor contribution gate target for the whole round at round start, so the
		// enforced threshold never shifts when students join/leave mid-round.
		RefreshRevisionContributionGateTarget();
	}
	if (bRevisionMode)
	{
		// Mastery is PERMANENT now: it is never wiped between rounds, stages, or games -- it only grows and is
		// tracked per student (persisted to their account). Each round clears only the per-round contribution
		// gate and per-round report bookkeeping, never the mastery fields or the spaced-review queue.
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			if (ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS))
			{
				BHPS->ResetRevisionRoundContribution();
			}
		}
		if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
		{
			BHGI->ClearQuestionAttemptHistory();
		}
		RevisionReviewTimeRemaining = 0;
		bRevisionReportExported = false;
		UpdateRevisionSummary(TEXT("Physics Classroom started: solve each zone, correct mistakes, and keep every student above threshold."));
		UpdateDirectorGameState(GetRevisionObjectiveText());
	}
	else
	{
		const FString ModifierText = GetRoundModifierText(ChosenModifier);
		const FString ModifierHint = ABHGameState::GetRoundModifierHintFor(ChosenModifier);
		UpdateDirectorGameState(FString::Printf(TEXT("Repair %d breakers, answer %d class questions, then finish each task before the Teacher finds you. Modifier: %s - %s"),
			ActiveBreakerCount,
			ActiveSideObjectiveCount,
			*ModifierText,
			*ModifierHint));
	}
}

void ABHGameMode::StartDirectorTimer()
{
	GetWorldTimerManager().ClearTimer(DirectorTimerHandle);
	float DirectorInterval = 7.0f;
	float FirstDirectorTickDelay = 5.0f;
	if (bRevisionMode)
	{
		DirectorInterval = RevisionScareIntensity >= 3 ? 4.0f : (RevisionScareIntensity >= 2 ? 5.0f : 6.0f);
		FirstDirectorTickDelay = RevisionScareIntensity >= 2 ? 3.0f : 5.0f;
	}
	GetWorldTimerManager().SetTimer(DirectorTimerHandle, this, &ABHGameMode::TickDirector, DirectorInterval, true, FirstDirectorTickDelay);
}

void ABHGameMode::TickDirector()
{
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	UpdatePresenceDirector();

	// Tutorials stage their OWN scripted scares (the Teacher reveal cutscene + chase, run by ABHTutorialDirector).
	// Suppress the random director scares, cold calls, and "scare a random Hunter" jolts here so a teaching beat
	// (press F, answer a question, learn the axe) is never interrupted by an unscripted jumpscare - and so the
	// jolt can't fire on the human player while they're learning to BE the Teacher. Presence still updates above,
	// so the dread vignette reads normally.
	if (bTutorialMode)
	{
		return;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bPanicRound = BHGS->RoundModifier == EBHRoundModifier::PanicSurge;
	const int32 ClampedRevisionScareIntensity = FMath::Clamp(RevisionScareIntensity, 0, 3);
	const float PresenceAlpha = FMath::Clamp(BHGS->PresenceLevel / 100.0f, 0.0f, 1.0f);
	const float TimePressureAlpha = BHGS->RemainingTime > 0 && HuntSeconds > 0
		? 1.0f - FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / static_cast<float>(HuntSeconds), 0.0f, 1.0f)
		: 0.0f;
	const float ObjectiveTotal = FMath::Max(1.0f, static_cast<float>(BHGS->BreakersRequired + BHGS->SideObjectivesRequired));
	const float ObjectiveAlpha = FMath::Clamp(static_cast<float>(BHGS->BreakersCompleted + BHGS->SideObjectivesCompleted) / ObjectiveTotal, 0.0f, 1.0f);
	const float DirectorPressure = FMath::Clamp(PresenceAlpha * 0.62f + TimePressureAlpha * 0.24f + ObjectiveAlpha * 0.28f, 0.0f, 1.0f);
	float ScareCooldown = bPartyPace || bPanicRound ? 11.0f : 24.0f;
	float ScareChance = (bPartyPace || bPanicRound ? 0.58f : 0.24f) * FMath::Lerp(0.52f, 1.24f, DirectorPressure);
	if (bRevisionMode)
	{
		static const float RevisionScareCooldowns[] = { 9999.0f, 20.0f, 14.0f, 9.0f };
		static const float RevisionScareBaseChances[] = { 0.0f, 0.24f, 0.46f, 0.64f };
		ScareCooldown = RevisionScareCooldowns[ClampedRevisionScareIntensity];
		ScareChance = RevisionScareBaseChances[ClampedRevisionScareIntensity] * FMath::Lerp(0.58f, 1.26f, DirectorPressure);
		if (bPartyPace || bPanicRound)
		{
			ScareCooldown *= 0.72f;
			ScareChance += 0.16f;
		}
	}
	ScareChance = FMath::Clamp(ScareChance, 0.0f, 0.95f);
	const bool bScareWindow = Now - LastPresenceSpikeTime >= 3.5f || DirectorPressure >= 0.72f;
	if (bScareWindow && Now - LastDirectorScareTime >= ScareCooldown && FMath::FRand() < ScareChance)
	{
		TriggerScareEvent();
	}

	const bool bColdCallWindow = DirectorPressure >= 0.38f || ObjectiveAlpha >= 0.34f || TimePressureAlpha >= 0.52f;
	float ColdCallCooldown = bPartyPace || bPanicRound ? 16.0f : 30.0f;
	float ColdCallChance = bColdCallWindow
		? ((bPartyPace || bPanicRound ? 0.18f : 0.055f) + PresenceAlpha * 0.24f + ObjectiveAlpha * 0.10f)
		: 0.0f;
	if (bRevisionMode)
	{
		static const float RevisionColdCallCooldowns[] = { 9999.0f, 28.0f, 20.0f, 14.0f };
		static const float RevisionColdCallBaseChances[] = { 0.0f, 0.08f, 0.14f, 0.21f };
		ColdCallCooldown = RevisionColdCallCooldowns[ClampedRevisionScareIntensity];
		ColdCallChance = bColdCallWindow
			? RevisionColdCallBaseChances[ClampedRevisionScareIntensity] + PresenceAlpha * 0.32f + ObjectiveAlpha * 0.14f + TimePressureAlpha * 0.10f
			: 0.0f;
		if (bPartyPace || bPanicRound)
		{
			ColdCallCooldown *= 0.72f;
			ColdCallChance += 0.12f;
		}
	}
	ColdCallChance = FMath::Clamp(ColdCallChance, 0.0f, 0.90f);
	if (Now - LastColdCallTime >= ColdCallCooldown && FMath::FRand() < ColdCallChance)
	{
		TriggerColdCallEvent();
	}

	// Keep the Teacher in the horror too: every now and then spring a proper scare (turn-around / super chain /
	// SCP-096) on a random alive hunter. Generous cooldown so it's an occasional jolt, never a nuisance.
	const float TeacherScareCooldown = bPartyPace || bPanicRound ? 80.0f : 135.0f;
	const float TeacherScareChance = bPartyPace || bPanicRound ? 0.22f : 0.13f;
	if (Now - LastTeacherProperScareTime >= TeacherScareCooldown && FMath::FRand() < TeacherScareChance)
	{
		TArray<ABHCharacter*> AliveHunters;
		for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
		{
			ABHCharacter* Character = *It;
			const ABHPlayerState* HunterPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
			if (Character && HunterPS && HunterPS->IsAliveHunter())
			{
				AliveHunters.Add(Character);
			}
		}
		if (AliveHunters.Num() > 0)
		{
			TriggerProperScareOnTeacher(AliveHunters[FMath::RandRange(0, AliveHunters.Num() - 1)]);
			LastTeacherProperScareTime = Now;
		}
	}

	MaybeHandOutDecoys();
}

void ABHGameMode::MaybeHandOutDecoys()
{
	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!GetWorld() || !BHGS || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	// Minimum spacing between drops, plus a per-window coin flip so the cadence is irregular rather than a
	// predictable metronome. With a 7s director tick this averages a drop somewhere around once a minute.
	const float HandoutCooldown = 38.0f;
	if (Now - LastDecoyHandoutTime < HandoutCooldown)
	{
		return;
	}
	if (FMath::FRand() > 0.5f)
	{
		return;
	}

	// Eligible = alive human survivors who aren't already holding a decoy (so we keep handing fresh ones to
	// players who have none, never stacking). Bots are excluded -- their baiting is paced by the bot AI.
	TArray<ABHPlayerState*> Eligible;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!PC || !BHPS || BHPS->IsABot())
		{
			continue;
		}
		if (!BHPS->IsAliveSurvivor() || BHPS->PlayerRole == EBHPlayerRole::Tester)
		{
			continue;
		}
		if (BHPS->GetPowerupCharges(EBHPowerupType::DecoySound) > 0)
		{
			continue;
		}
		Eligible.Add(BHPS);
	}
	if (Eligible.Num() == 0)
	{
		return;
	}

	LastDecoyHandoutTime = Now;

	// Hand to a small random subset -- "a few people", never the whole class at once. Roughly a third of the
	// eligible players, clamped to 1-2 so big lobbies don't suddenly arm five decoys in one beat.
	const int32 GrantCount = FMath::Clamp(FMath::CeilToInt(Eligible.Num() * 0.34f), 1, 2);
	for (int32 Granted = 0; Granted < GrantCount && Eligible.Num() > 0; ++Granted)
	{
		const int32 Pick = FMath::RandRange(0, Eligible.Num() - 1);
		ABHPlayerState* BHPS = Eligible[Pick];
		Eligible.RemoveAtSwap(Pick);
		// Cap the held charge at 1: a handed-out decoy is strictly single use.
		if (BHPS->AddPowerupCharge(EBHPowerupType::DecoySound, 1))
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(BHPS->GetOwningController()))
			{
				PC->ClientShowStatusMessage(TEXT("You found a decoy noisemaker. Throw it with your Decoy key to fake footsteps and pull the Teacher away. Single use."), 4.5f);
			}
		}
	}
}

void ABHGameMode::UpdatePresenceDirector()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	// Tutorial teaching: hold the presence director fully calm - no threat-driven climb, no dread text, no whisper,
	// no light-kill - so a lesson step never reads as scary (the dread meter stays at the practice baseline). The
	// scripted chase climax (bTutorialHorrorAllowed) runs the full director below for real tension. Inert in live
	// matches (the predicate is false there).
	if (IsTutorialHorrorSuppressed())
	{
		return;
	}

	int32 SurvivorCount = 0;
	int32 HiddenCount = 0;
	int32 HunterCount = 0;
	float BestThreatAlpha = 0.0f;
	ABHCharacter* BestTarget = nullptr;
	float BestTargetDistance = TNumericLimits<float>::Max();
	bool bBestTargetSeen = false;

	TArray<ABHCharacter*> Hunters;
	TArray<ABHCharacter*> Survivors;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Character || !BHPS)
		{
			continue;
		}

		if (BHPS->IsAliveHunter())
		{
			Hunters.Add(Character);
			++HunterCount;
		}
		else if (BHPS->IsAliveSurvivor())
		{
			Survivors.Add(Character);
			++SurvivorCount;
			if (Character->IsHiddenInLocker())
			{
				++HiddenCount;
			}
		}
	}

	for (ABHCharacter* Survivor : Survivors)
	{
		if (!Survivor)
		{
			continue;
		}

		float NearestHunterDistance = TNumericLimits<float>::Max();
		bool bHunterCanSee = false;
		for (ABHCharacter* Hunter : Hunters)
		{
			if (!Hunter)
			{
				continue;
			}

			const float Distance = FVector::Dist2D(Hunter->GetActorLocation(), Survivor->GetActorLocation());
			if (Distance < NearestHunterDistance)
			{
				NearestHunterDistance = Distance;
				bHunterCanSee = Hunter->GetController() ? Hunter->GetController()->LineOfSightTo(Survivor) : false;
			}
		}

		const float DistanceAlpha = NearestHunterDistance < TNumericLimits<float>::Max()
			? 1.0f - FMath::Clamp(NearestHunterDistance / 4200.0f, 0.0f, 1.0f)
			: 0.0f;
		const float HiddenBonus = Survivor->IsHiddenInLocker() ? 0.16f : 0.0f;
		const float SeenBonus = bHunterCanSee && !Survivor->IsHiddenInLocker() ? 0.25f : 0.0f;
		const float PanicBonus = FMath::Clamp(FMath::Max(Survivor->GetFear(), Survivor->GetDread()) / 100.0f, 0.0f, 1.0f) * 0.24f;
		const float ThreatAlpha = FMath::Clamp(DistanceAlpha + HiddenBonus + SeenBonus + PanicBonus, 0.0f, 1.0f);
		if (ThreatAlpha > BestThreatAlpha)
		{
			BestThreatAlpha = ThreatAlpha;
			BestTarget = Survivor;
			BestTargetDistance = NearestHunterDistance;
			bBestTargetSeen = bHunterCanSee;
		}
	}

	const float ObjectiveTotal = FMath::Max(1.0f, static_cast<float>(BHGS->BreakersRequired + BHGS->SideObjectivesRequired));
	const float ObjectiveDone = static_cast<float>(BHGS->BreakersCompleted + BHGS->SideObjectivesCompleted);
	const float ObjectiveAlpha = FMath::Clamp(ObjectiveDone / ObjectiveTotal, 0.0f, 1.0f);
	const float HiddenAlpha = SurvivorCount > 0 ? static_cast<float>(HiddenCount) / static_cast<float>(SurvivorCount) : 0.0f;
	const float TimeAlpha = BHGS->RemainingTime > 0 ? 1.0f - FMath::Clamp(static_cast<float>(BHGS->RemainingTime) / static_cast<float>(HuntSeconds), 0.0f, 1.0f) : 0.0f;
	float DesiredPresence = FMath::Max(BestThreatAlpha * 86.0f, ObjectiveAlpha * 38.0f + HiddenAlpha * 18.0f + TimeAlpha * 28.0f);

	if (BHGS->RoundModifier == EBHRoundModifier::PanicSurge)
	{
		DesiredPresence += 12.0f;
	}
	else if (BHGS->RoundModifier == EBHRoundModifier::LightsOut)
	{
		DesiredPresence += 8.0f;
	}
	if (HunterCount == 0)
	{
		DesiredPresence *= 0.45f;
	}

	const float CurrentPresence = BHGS->PresenceLevel;
	// Time-based decay: the director runs on a mode-dependent interval (~7s normal, ~4s in revision
	// intensity-3), so a flat per-call drop bled presence off at different real-world rates. Use a per-second
	// rate calibrated to the old ~10-per-tick at the 7s normal interval, scaled by the actual elapsed time
	// (clamped so a long suppressed / non-Hunt gap can't over-decay in one step; still floored by DesiredPresence).
	const float PresenceNow = GetWorld()->GetTimeSeconds();
	const float PresenceElapsed = (LastPresenceDirectorTime >= 0.0f)
		? FMath::Clamp(PresenceNow - LastPresenceDirectorTime, 0.0f, 30.0f)
		: 7.0f;
	LastPresenceDirectorTime = PresenceNow;
	const float DecayedPresence = FMath::Max(0.0f, CurrentPresence - (10.0f / 7.0f) * PresenceElapsed);
	const float NewPresence = FMath::Clamp(FMath::Max(DesiredPresence, DecayedPresence), 0.0f, 100.0f);

	FString PresenceText = TEXT("The building is listening.");
	const int32 PresenceTextSalt = RoundSeed + BHGS->PresencePulse * 11 + FMath::FloorToInt(NewPresence * 3.0f) + BHGS->RemainingTime;
	if (NewPresence >= 82.0f && BestTarget && BestTarget->IsHiddenInLocker())
	{
		static const TCHAR* HiddenCriticalLines[] = {
			TEXT("It is waiting outside a hiding place."),
			TEXT("The locker trace is warmer than it should be."),
			TEXT("The hiding place is no longer quiet."),
			TEXT("A handle moved on the heat trace.")
		};
		PresenceText = SelectPromptLine(HiddenCriticalLines, UE_ARRAY_COUNT(HiddenCriticalLines), PresenceTextSalt);
	}
	else if (NewPresence >= 78.0f && (bBestTargetSeen || BestTargetDistance <= 2600.0f))
	{
		static const TCHAR* CloseThreatLines[] = {
			TEXT("The Teacher is close enough to hear breathing."),
			TEXT("A warm path is closing behind you."),
			TEXT("The route has a second pulse on it."),
			TEXT("The corridor trace bends toward you.")
		};
		PresenceText = SelectPromptLine(CloseThreatLines, UE_ARRAY_COUNT(CloseThreatLines), PresenceTextSalt);
	}
	else if (NewPresence >= 62.0f && ObjectiveAlpha > 0.35f)
	{
		static const TCHAR* ObjectivePressureLines[] = {
			TEXT("Every completed task makes the route louder."),
			TEXT("The objective trail is glowing too clearly."),
			TEXT("The heat trace remembers each repaired station."),
			TEXT("The route is compromised.")
		};
		PresenceText = SelectPromptLine(ObjectivePressureLines, UE_ARRAY_COUNT(ObjectivePressureLines), PresenceTextSalt);
	}
	else if (NewPresence >= 48.0f && HiddenCount > 0)
	{
		static const TCHAR* HiddenPressureLines[] = {
			TEXT("The lockers do not feel empty."),
			TEXT("The map shows warmth where nobody is moving."),
			TEXT("A hiding place keeps returning a signal."),
			TEXT("The quiet spots are being counted.")
		};
		PresenceText = SelectPromptLine(HiddenPressureLines, UE_ARRAY_COUNT(HiddenPressureLines), PresenceTextSalt);
	}
	else if (NewPresence >= 30.0f)
	{
		static const TCHAR* SuspicionLines[] = {
			TEXT("Something is mapping your route."),
			TEXT("The heat trace corrects itself after you move."),
			TEXT("A path appears before anyone walks it."),
			TEXT("The sensor sees a line through the dark.")
		};
		PresenceText = SelectPromptLine(SuspicionLines, UE_ARRAY_COUNT(SuspicionLines), PresenceTextSalt);
	}

	// Easter egg ("a whisper in the static"): only at CALM presence (so it can never replace a real threat
	// warning set above) and rarely, swap the idle line for a hidden one. Cosmetic; gated by bh.EasterEggs.
	if (NewPresence < 30.0f && BHAreEasterEggsEnabled() && FMath::FRand() < 0.025f)
	{
		static const TCHAR* WhisperLines[] = {
			TEXT("the building hums at 50 Hz, like the lights."),
			TEXT("something in the walls is solving for x."),
			TEXT("the heat trace spells a word, then forgets it."),
			TEXT("a draft moves through a room with no doors."),
			TEXT("the sensors blink in threes, then stop."),
			TEXT("for a moment, the map shows a room that isn't there."),
			TEXT("the clock ticks sixty-one seconds to the minute, just once."),
			TEXT("a locker door breathes in, then holds it."),
			TEXT("the chalkboard solves itself and erases the proof."),
			TEXT("somewhere a bell rings for a period that isn't on the timetable.")
		};
		PresenceText = SelectPromptLine(WhisperLines, UE_ARRAY_COUNT(WhisperLines), PresenceTextSalt + 7);
		// Secret: this hidden line is replicated to everyone's presence readout, so award "Did You See That?" to
		// the human players present to witness it (idempotent client-side, so re-firing the rare line is harmless).
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ABHPlayerController* WitnessPC = Cast<ABHPlayerController>(It->Get()))
			{
				if (ABHCharacter* WitnessChar = Cast<ABHCharacter>(WitnessPC->GetPawn()))
				{
					WitnessChar->ClientGrantAchievement(FName(TEXT("did_you_see_that")), TEXT("...did you see that?"));
				}
			}
		}
	}

	const bool bPulse = NewPresence >= 72.0f || FMath::Abs(NewPresence - CurrentPresence) >= 18.0f;
	BHGS->SetPresenceState(NewPresence, PresenceText, bPulse ? BHGS->PresencePulse + 1 : BHGS->PresencePulse);

	const float Now = GetWorld()->GetTimeSeconds();
	const float WhisperCooldown = bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge ? 8.5f : 14.0f;
	const bool bWhisperMoment = BestTarget
		&& (BestTargetDistance <= 4200.0f || BestTarget->IsHiddenInLocker() || BestTarget->GetDread() >= 52.0f || BestTarget->IsDetentionMarked());
	// (Tutorial teaching never reaches here - UpdatePresenceDirector early-returns above when horror is suppressed,
	// so the whisper cue only fires in live matches and during the scripted chase climax.)
	if (bWhisperMoment && NewPresence >= 54.0f && Now - LastPresenceWhisperTime >= WhisperCooldown)
	{
		const FVector BehindTarget = BestTarget->GetActorLocation() - BestTarget->GetActorForwardVector() * FMath::FRandRange(180.0f, 420.0f) + FVector(0.0f, 0.0f, 88.0f);
		const float WhisperJumpscareCooldown = bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge ? 36.0f : 68.0f;
		const bool bHighPressureWhisper = NewPresence >= 82.0f
			|| (BestTarget->IsHiddenInLocker() && NewPresence >= 72.0f)
			|| BestTarget->GetDread() >= 70.0f
			|| BestTarget->IsDetentionMarked();
		FBHJumpscareVariant PendingWhisperVariant;
		// Never escalate to the monster-charge jumpscare in ANY tutorial state (including the chase): the scripted
		// reveal is the only intended scare. The softer whisper/locker-knock cue above still plays during the chase.
		const bool bTriggerWhisperJumpscare = bHighPressureWhisper
			&& !bTutorialMode
			&& GetEffectiveScareIntensity() > 0
			&& Now - LastWhisperJumpscareTime >= WhisperJumpscareCooldown
			&& Now - LastMonsterChargeTime >= 12.0f
			&& ChooseWhisperJumpscareVariant(PendingWhisperVariant);
		FString BudgetReason;
		const EBHScareEventType ReservedCueType = bTriggerWhisperJumpscare
			? EBHScareEventType::MonsterCharge
			: (BestTarget->IsHiddenInLocker() ? EBHScareEventType::LockerKnock : EBHScareEventType::Whisper);
		if (AtmosphereDirector && !AtmosphereDirector->ReserveDirectorCue(BestTarget, ReservedCueType, BehindTarget, bTriggerWhisperJumpscare ? 0.88f : 0.46f, TEXT("presence whisper"), BudgetReason))
		{
			LastPresenceWhisperTime = Now;
			return;
		}
		LastPresenceWhisperTime = Now;
		SpawnAmbient(BehindTarget, FMath::FRandRange(190.0f, 340.0f), BestTarget->IsHiddenInLocker() ? 0.22f : 0.18f, 0.18f, 5.6f, 3.75f);
		BestTarget->AddFear(BestTarget->IsHiddenInLocker() ? 12.0f : 8.0f);
		BestTarget->AddDread(BestTarget->IsHiddenInLocker() ? 16.0f : 9.0f);

		if (!bTriggerWhisperJumpscare)
		{
			if (ABHPlayerController* PC = Cast<ABHPlayerController>(BestTarget->GetController()))
			{
				const TCHAR* Message = nullptr;
				if (BestTarget->IsHiddenInLocker())
				{
					static const TCHAR* HiddenMessages[] = {
						TEXT("The handle moves once, then stops."),
						TEXT("Something listens inches from your face."),
						TEXT("The air inside the locker goes cold."),
						TEXT("A fingertip taps the other side."),
						TEXT("The locker breathes after you do."),
						TEXT("Something writes on the outside panel."),
						TEXT("A shoe stops directly outside."),
						TEXT("The silence leans against the door.")
					};
					Message = SelectPromptLine(HiddenMessages, UE_ARRAY_COUNT(HiddenMessages), FMath::Rand());
				}
				else if (bBestTargetSeen || BestTargetDistance <= 2200.0f)
				{
					static const TCHAR* CloseMessages[] = {
						TEXT("You hear a second set of footsteps match yours."),
						TEXT("A breath cuts off when you turn."),
						TEXT("Something scraped the wall beside you."),
						TEXT("Footsteps stop exactly when you stop."),
						TEXT("The wall next to you clicks twice."),
						TEXT("Your next turn is already occupied."),
						TEXT("A shoulder brushes past where no one is standing."),
						TEXT("The dark beside you inhales.")
					};
					Message = SelectPromptLine(CloseMessages, UE_ARRAY_COUNT(CloseMessages), FMath::Rand());
				}
				else
				{
					static const TCHAR* OpenMessages[] = {
						TEXT("You hear a second set of footsteps match yours."),
						TEXT("The dark ahead feels occupied."),
						TEXT("A breath cuts off when you turn."),
						TEXT("Your shadow moves half a second late."),
						TEXT("A route draws itself through a room you have not entered."),
						TEXT("The floor settles under an extra step."),
						TEXT("A light blinks in the shape of your path."),
						TEXT("Something waits at the end of the heat trace.")
					};
					Message = SelectPromptLine(OpenMessages, UE_ARRAY_COUNT(OpenMessages), FMath::Rand());
				}
				PC->ClientShowStatusMessage(Message, bBestTargetSeen || BestTargetDistance <= 2200.0f ? 2.85f : 3.0f);
			}
		}

		if (!FlickerLights.IsEmpty() && FMath::FRand() < 0.48f)
		{
			ABHFlickerLight* NearestLight = nullptr;
			float NearestLightDistSq = TNumericLimits<float>::Max();
			for (ABHFlickerLight* Light : FlickerLights)
			{
				if (!Light || Light->GetCircuitId() <= 0 || !Light->IsPowered())
				{
					continue;
				}

				const float DistSq = FVector::DistSquared2D(Light->GetActorLocation(), BestTarget->GetActorLocation());
				if (DistSq < NearestLightDistSq)
				{
					NearestLightDistSq = DistSq;
					NearestLight = Light;
				}
			}

			if (NearestLight && NearestLightDistSq <= FMath::Square(2600.0f))
			{
				NearestLight->SetPowered(false);
			}
		}

		if (bTriggerWhisperJumpscare)
		{
			LastWhisperJumpscareTime = Now;
			TriggerMonsterChargeJumpscareWithVariant(BestTarget, PendingWhisperVariant, TEXT("The Whisper found your route."), 30.0f, 34.0f);
		}
	}
}

void ABHGameMode::ApplyPresenceSpike(const FVector& SourceLocation, float SpikeLevel, const FString& PresenceText)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	// Tutorial teaching: no presence spike at all - a taught action (answering a graph, repairing the breaker) must
	// not raise the dread meter/vignette or fire its light-flicker burst. The chase climax (bTutorialHorrorAllowed)
	// and live matches still spike normally.
	if (IsTutorialHorrorSuppressed())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float NewPresence = FMath::Clamp(FMath::Max(BHGS->PresenceLevel, SpikeLevel), 0.0f, 100.0f);
	BHGS->SetPresenceState(NewPresence, PresenceText, BHGS->PresencePulse + 1);
	LastPresenceSpikeTime = Now;

	if (Now - LastPresenceWhisperTime > 5.0f)
	{
		SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 86.0f), FMath::FRandRange(125.0f, 260.0f), 0.20f, 0.16f, 4.7f, 3.25f);
	}

	// Danger reaction: briefly tint the nearest fixtures toward a sickly amber-green and quicken their
	// flicker so the lighting itself signals the Shape is closing in. Reuses the self-reverting burst
	// system (timer-based restore, replication- and reduced-flash-comfort safe), so this is purely
	// event-driven with no per-frame cost. Capped at a few nearest lights so it reads as a cue rather
	// than a room-wide strobe, and scales hotter with the spike level. (Tutorial teaching never reaches here -
	// ApplyPresenceSpike early-returns above when horror is suppressed.)
	if (!FlickerLights.IsEmpty())
	{
		const float PresenceAlpha = FMath::Clamp(SpikeLevel / 100.0f, 0.0f, 1.0f);
		const FLinearColor DreadTint = FMath::Lerp(FLinearColor(0.92f, 0.70f, 0.26f, 1.0f), FLinearColor(0.62f, 0.80f, 0.30f, 1.0f), PresenceAlpha);
		const float BurstMul = FMath::Lerp(1.25f, 1.7f, PresenceAlpha);
		const float BurstSpeed = FMath::Lerp(11.0f, 16.0f, PresenceAlpha);
		const float BurstDuration = FMath::Lerp(1.1f, 1.7f, PresenceAlpha);
		const float ReactRadiusSq = FMath::Square(2400.0f);
		const int32 MaxReacting = PresenceAlpha > 0.66f ? 4 : (PresenceAlpha > 0.33f ? 3 : 2);
		int32 Reacted = 0;
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (Reacted >= MaxReacting)
			{
				break;
			}
			if (Light && Light->IsPowered() && FVector::DistSquared(Light->GetActorLocation(), SourceLocation) <= ReactRadiusSq)
			{
				Light->TriggerFlickerBurst(BurstDuration, BurstMul, DreadTint, BurstSpeed);
				++Reacted;
			}
		}
	}
}

void ABHGameMode::PublishObjectiveBeats()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	// The guided tutorial deliberately shows a single marker for the one next object the current lesson step
	// needs (locker, then crawl duct, then station, then exit). ABHTutorialDirector publishes that beat, so
	// the generic many-beat planner must not run here or it would clutter the screen with every objective.
	if (bTutorialMode)
	{
		return;
	}

	TArray<FBHObjectiveBeat> Beats;
	const auto AddBeat = [&Beats](FName BeatId, const FString& Label, const FVector& Location, bool bPrimary, bool bDanger = false, float Radius = 1400.0f, float ExpireServerTime = 0.0f)
	{
		FBHObjectiveBeat Beat;
		Beat.BeatId = BeatId;
		Beat.Label = Label;
		Beat.Location = Location;
		Beat.Radius = Radius;
		Beat.ExpireServerTime = ExpireServerTime;
		Beat.bPrimary = bPrimary;
		Beat.bDanger = bDanger;
		Beats.Add(Beat);
	};

	bool bAddedPrimary = false;
	if (!BHGS->bExitUnlocked)
	{
		for (const ABHBreaker* Breaker : BreakerActors)
		{
			if (Breaker && Breaker->IsDirectorActive() && !Breaker->IsRepaired())
			{
				AddBeat(TEXT("Power"), TEXT("Power"), Breaker->GetActorLocation(), true, false, 5200.0f);
				bAddedPrimary = true;
				break;
			}
		}
	}

	if (!bAddedPrimary)
	{
		for (const ABHExitGate* ExitGate : ExitGates)
		{
			if (ExitGate)
			{
				AddBeat(TEXT("Exit"), BHGS->bExitUnlocked ? TEXT("Exit") : TEXT("Gate"), ExitGate->GetActorLocation(), true, false, 7200.0f);
				bAddedPrimary = true;
				break;
			}
		}
	}

	int32 AddedStations = 0;
	for (const ABHObjectiveStation* Station : ObjectiveStations)
	{
		if (Station && !Station->IsQuestionSolved() && AddedStations < 2)
		{
			AddBeat(FName(*FString::Printf(TEXT("Task%d"), AddedStations)), TEXT("Task"), Station->GetActorLocation(), false, false, 3600.0f);
			++AddedStations;
		}
	}

	if (RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
	{
		AddBeat(TEXT("FinalStation"), TEXT("Station"), FVector(3300.0f, -760.0f, 120.0f), !bAddedPrimary, false, 7800.0f);
	}
	else if (RuntimeLevelName.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase))
	{
		AddBeat(TEXT("BoardTrain"), TEXT("Train"), FVector(0.0f, 0.0f, 120.0f), !bAddedPrimary, false, 7800.0f);
	}

	BHGS->SetObjectiveBeats(Beats);
}

void ABHGameMode::PublishDangerObjectiveBeat(const FVector& Location, const FString& Label)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
	{
		return;
	}

	// Keep the tutorial's single next-object marker uncluttered: no transient red danger pings.
	if (bTutorialMode)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastObjectiveDangerBeatTime < 4.5f)
	{
		return;
	}
	LastObjectiveDangerBeatTime = Now;

	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float Lifetime = Settings ? FMath::Max(2.0f, Settings->ObjectiveBeatLifetimeSeconds) : 18.0f;
	const float ServerNow = BHGS->GetServerWorldTimeSeconds();
	TArray<FBHObjectiveBeat> Beats = BHGS->ObjectiveBeats;
	Beats.RemoveAll([ServerNow](const FBHObjectiveBeat& Beat)
	{
		return Beat.bDanger || (Beat.ExpireServerTime > 0.0f && ServerNow > Beat.ExpireServerTime);
	});

	FBHObjectiveBeat DangerBeat;
	DangerBeat.BeatId = TEXT("RecentDanger");
	DangerBeat.Label = Label.IsEmpty() ? TEXT("Noise") : Label.Left(18);
	DangerBeat.Location = Location;
	DangerBeat.Radius = 5800.0f;
	DangerBeat.ExpireServerTime = ServerNow + Lifetime;
	DangerBeat.bPrimary = false;
	DangerBeat.bDanger = true;
	Beats.Add(DangerBeat);
	BHGS->SetObjectiveBeats(Beats);
}

bool ABHGameMode::TriggerFakeOutTensionCue(ABHCharacter* Target)
{
	UWorld* World = GetWorld();
	if (!World || !Target)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastFakeOutTime < 25.0f)
	{
		return false;
	}
	LastFakeOutTime = Now;

	// "It saw you" pure-tension tease: sometimes just whisper a line onto the screen with nothing actually
	// happening — the psychological poke the user asked for. No sound, flicker, lock, or monster; only dread.
	if (FMath::FRand() < 0.45f)
	{
		if (ABHPlayerController* TextPC = Cast<ABHPlayerController>(Target->GetController()))
		{
			static const TCHAR* TeaseLines[] = {
				TEXT("It saw you."),
				TEXT("It knows where you are."),
				TEXT("You are not alone in here."),
				TEXT("Something is watching."),
				TEXT("Don't look behind you."),
				TEXT("It heard that.")
			};
			const FString Line = TeaseLines[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(TeaseLines)) - 1)];
			TextPC->ClientShowStatusMessage(Line, 2.6f);
		}
		Target->AddDread(5.0f);
		RecordPlaytestTelemetryMarker(TEXT("scare_cue"), Target->GetActorLocation(), TEXT("fake_out_text"), nullptr, Target->GetPlayerState<ABHPlayerState>());
		return true;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	FVector Forward = Target->GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();

	// Place the phantom sound behind or beside the player, out of view, so it reads as "something there".
	const FVector SideDir = (FMath::FRand() < 0.5f) ? Right : -Right;
	const FVector Behind = -Forward * FMath::FRandRange(420.0f, 760.0f) + SideDir * FMath::FRandRange(-220.0f, 220.0f);
	const FVector CueLocation = TargetLocation + Behind + FVector(0.0f, 0.0f, 70.0f);

	// Faint whisper/footstep emitter + a brief light flicker that resolves to nothing.
	SpawnAmbient(CueLocation, FMath::FRandRange(300.0f, 360.0f), 0.30f, 0.42f, 2.6f, 1.4f);
	CutLightsForJumpscare(TargetLocation, CueLocation, 760.0f, 0.45f);

	// A very subtle physical tension on the target — low shake, no input lock, no flash.
	if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController()))
	{
		const float SensoryScale = GetScareSensoryScale();
		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::Whisper;
		Cue.FocusLocation = CueLocation;
		Cue.DurationSeconds = 0.9f;
		Cue.LockSeconds = 0.0f;
		Cue.ShakeIntensity = FMath::Clamp(0.10f * SensoryScale, 0.0f, 1.0f);
		Cue.CameraJitterDuration = 0.35f * SensoryScale;
		Cue.CameraJitterFrequency = 24.0f;
		Cue.FlashIntensity = 0.0f;
		Cue.bSnapToFocus = false;
		Cue.bLockInput = false;
		Cue.bCloseRangeFocus = false;
		TargetPC->ClientPlayHorrorCue(Cue);
	}

	Target->AddDread(6.0f);
	RecordPlaytestTelemetryMarker(TEXT("scare_cue"), TargetLocation, TEXT("fake_out_tension"), nullptr, Target->GetPlayerState<ABHPlayerState>());
	return true;
}

void ABHGameMode::TriggerScareEvent()
{
	if (bRevisionMode && RevisionScareIntensity <= 0)
	{
		return;
	}

	TArray<ABHCharacter*> Candidates;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHCharacter* Character = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Character && BHPS && BHPS->IsAliveSurvivor() && !Character->IsHiddenInLocker())
		{
			Candidates.Add(Character);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ABHCharacter* Target = Candidates[0];
	float BestCandidateScore = -1.0f;
	for (ABHCharacter* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		float NearestHunterDistance = TNumericLimits<float>::Max();
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			const ABHCharacter* Hunter = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
			const ABHPlayerState* HunterPS = Hunter ? Hunter->GetPlayerState<ABHPlayerState>() : nullptr;
			if (Hunter && HunterPS && HunterPS->IsAliveHunter())
			{
				NearestHunterDistance = FMath::Min(NearestHunterDistance, FVector::Dist2D(Hunter->GetActorLocation(), Candidate->GetActorLocation()));
			}
		}

		const float DreadAlpha = FMath::Clamp(Candidate->GetDread() / 100.0f, 0.0f, 1.0f);
		const float FearAlpha = FMath::Clamp(Candidate->GetFear() / 100.0f, 0.0f, 1.0f);
		const float NearMissAlpha = NearestHunterDistance < TNumericLimits<float>::Max()
			? FMath::Clamp((NearestHunterDistance - 1200.0f) / 3600.0f, 0.0f, 1.0f)
			: 0.25f;
		const float BudgetWeight = AtmosphereDirector ? AtmosphereDirector->GetDirectorCueWeight(Candidate, EBHScareEventType::AudioStinger, 0.66f) : 1.0f;
		const float CandidateScore = FMath::FRandRange(0.0f, 0.32f)
			+ DreadAlpha * 0.34f
			+ FearAlpha * 0.20f
			+ NearMissAlpha * 0.18f
			+ (Candidate->IsDetentionMarked() ? 0.22f : 0.0f)
			+ FMath::Clamp(BudgetWeight - 0.55f, -0.45f, 0.38f);
		if (CandidateScore > BestCandidateScore)
		{
			BestCandidateScore = CandidateScore;
			Target = Candidate;
		}
	}

	FVector ScareLocation = Target->GetActorLocation() - Target->GetActorForwardVector() * 320.0f + FVector(0.0f, 0.0f, 80.0f);
	if (!ScarePoints.IsEmpty())
	{
		TArray<FVector> NearbyScares;
		for (const FVector& Point : ScarePoints)
		{
			if (FVector::DistSquared2D(Point, Target->GetActorLocation()) <= FMath::Square(1900.0f))
			{
				NearbyScares.Add(Point);
			}
		}
		if (!NearbyScares.IsEmpty())
		{
			ScareLocation = NearbyScares[FMath::RandRange(0, NearbyScares.Num() - 1)];
		}
	}

	// Occasionally build dread with a fake-out that resolves to nothing instead of a real scare.
	// Its own 25s cooldown keeps it rare, and it doesn't consume the monster cooldown.
	if (GetEffectiveScareIntensity() > 0 && FMath::FRand() < 0.22f && TriggerFakeOutTensionCue(Target))
	{
		LastDirectorScareTime = GetWorld()->GetTimeSeconds();
		return;
	}

	float MonsterCooldown = bPartyPace ? 24.0f : 38.0f;
	float MonsterChance = bPartyPace ? 0.45f : 0.30f;
	if (bRevisionMode)
	{
		const int32 ClampedRevisionScareIntensity = FMath::Clamp(RevisionScareIntensity, 0, 3);
		static const float RevisionMonsterCooldowns[] = { 9999.0f, 34.0f, 22.0f, 14.0f };
		static const float RevisionMonsterChances[] = { 0.0f, 0.36f, 0.62f, 0.78f };
		MonsterCooldown = RevisionMonsterCooldowns[ClampedRevisionScareIntensity];
		MonsterChance = RevisionMonsterChances[ClampedRevisionScareIntensity];
		if (bPartyPace)
		{
			MonsterCooldown *= 0.75f;
			MonsterChance = FMath::Min(0.90f, MonsterChance + 0.10f);
		}
	}
	if (GetWorld() && GetWorld()->GetTimeSeconds() - LastMonsterChargeTime >= MonsterCooldown && FMath::FRand() < MonsterChance
		&& !IsTeacherNearby(ScareLocation))
	{
		FString BudgetReason;
		const bool bBudgetAllowsMonster = !AtmosphereDirector
			|| AtmosphereDirector->ReserveDirectorCue(Target, EBHScareEventType::MonsterCharge, ScareLocation, 0.92f, TEXT("timed director monster beat"), BudgetReason);
		if (bBudgetAllowsMonster)
		{
			RecordPlaytestTelemetryMarker(TEXT("scare_cue"), Target->GetActorLocation(), TEXT("director_monster_charge"), nullptr, Target->GetPlayerState<ABHPlayerState>());
			TriggerMonsterChargeJumpscare(Target);
			LastDirectorScareTime = GetWorld()->GetTimeSeconds();
			return;
		}
	}

	if (bRevisionMode)
	{
		if (TriggerRevisionThemedAmbientScare(Target))
		{
			LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastDirectorScareTime;
		}
		return;
	}

	static const TCHAR* Messages[] = {
		TEXT("Something scraped the wall beside you."),
		TEXT("A light snaps behind you."),
		TEXT("You hear a breath where nobody should be."),
		TEXT("Metal shifts in the dark."),
		TEXT("A door settles shut in a room you never opened."),
		TEXT("The floor clicks once under an extra footstep."),
		TEXT("Your flashlight catches movement, then forgets it."),
		TEXT("Something drags along the wall, keeping pace."),
		TEXT("The ceiling tiles creak one by one above you."),
		TEXT("A shape crosses the heat trace and disappears."),
		TEXT("The air behind your neck goes warm."),
		TEXT("A locker knocks from the inside as you pass.")
	};
	const float TargetPressure = FMath::Clamp(FMath::Max(Target->GetFear(), Target->GetDread()) / 100.0f, 0.0f, 1.0f);
	FBHScareEventSpec Spec;
	Spec.EventType = AtmosphereDirector
		? AtmosphereDirector->ChoosePressureCueType(Target, ScareLocation, FMath::Max(TargetPressure, BestCandidateScore), false)
		: EBHScareEventType::AudioStinger;
	Spec.Target = Target;
	Spec.Origin = ScareLocation;
	Spec.Intensity = FMath::Clamp(0.44f + FMath::Max(TargetPressure, BestCandidateScore) * 0.32f, 0.32f, 0.82f);
	Spec.LightRadius = 1700.0f;
	Spec.LockSeconds = Spec.EventType == EBHScareEventType::LightCut ? 4.2f : 0.0f;
	Spec.Message = Messages[FMath::RandRange(0, UE_ARRAY_COUNT(Messages) - 1)];
	const bool bCueTriggered = TriggerAtmosphereCue(Spec);
	if (bCueTriggered)
	{
		const UEnum* ScareEnum = StaticEnum<EBHScareEventType>();
		const FString ScareType = ScareEnum ? ScareEnum->GetNameStringByValue(static_cast<int64>(Spec.EventType)) : TEXT("AtmosphereCue");
		RecordPlaytestTelemetryMarker(TEXT("scare_cue"), ScareLocation, FString::Printf(TEXT("director_%s"), *ScareType), nullptr, Target->GetPlayerState<ABHPlayerState>());
		LastDirectorScareTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastDirectorScareTime;
	}

	if (bCueTriggered && !FlickerLights.IsEmpty() && FMath::FRand() < 0.45f)
	{
		ABHFlickerLight* Light = FlickerLights[FMath::RandRange(0, FlickerLights.Num() - 1)];
		if (Light && Light->GetCircuitId() > 0)
		{
			Light->SetPowered(false);
		}
	}
}

bool ABHGameMode::TriggerRevisionThemedAmbientScare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return false;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	const FVector BehindTarget = TargetLocation - Target->GetActorForwardVector() * 260.0f + FVector(0.0f, 0.0f, 92.0f);
	const int32 Variant = FMath::RandRange(0, 11);
	FString Message;
	float Frequency = 260.0f;
	float Pulse = 5.6f;
	switch (Variant)
	{
	case 0:
		Message = TEXT("Blackboard slam: the examiner's method appears too close.");
		Frequency = 185.0f;
		Pulse = 6.4f;
		break;
	case 1:
		Message = TEXT("Exam bell spike: TIME flashes red in the corner of your eye.");
		Frequency = 520.0f;
		Pulse = 7.0f;
		break;
	case 2:
		Message = TEXT("Circuit breaker pop: every light nearby clicks once.");
		Frequency = 360.0f;
		Pulse = 5.8f;
		break;
	case 3:
		Message = TEXT("Graph line lunge: a plotted line races across your vision.");
		Frequency = 310.0f;
		Pulse = 6.1f;
		break;
	case 4:
		Message = TEXT("Roll call: the register whispers your name.");
		Frequency = 140.0f;
		Pulse = 4.9f;
		break;
	case 5:
		Message = TEXT("Method whisper: justify it before it screams.");
		Frequency = 230.0f;
		Pulse = 5.2f;
		break;
	case 6:
		Message = TEXT("Chalk dust falls upward and spells your last mistake.");
		Frequency = 205.0f;
		Pulse = 5.9f;
		break;
	case 7:
		Message = TEXT("A desk leg drags beside the wall, keeping pace.");
		Frequency = 155.0f;
		Pulse = 6.0f;
		break;
	case 8:
		Message = TEXT("Calculator keys click your heartbeat back at you.");
		Frequency = 490.0f;
		Pulse = 6.8f;
		break;
	case 9:
		Message = TEXT("A ruler snaps in the dark and points at your feet.");
		Frequency = 575.0f;
		Pulse = 7.1f;
		break;
	case 10:
		Message = TEXT("A chair turns toward you without touching the floor.");
		Frequency = 245.0f;
		Pulse = 5.5f;
		break;
	default:
		Message = TEXT("Reflection scare: something appears at the equal angle.");
		Frequency = 430.0f;
		Pulse = 6.7f;
		break;
	}

	EBHScareEventType CueType = EBHScareEventType::Ambient;
	const bool bPrefersLightCut = Variant == 1 || Variant == 2 || Variant == 8 || Variant == 9;
	if (bPrefersLightCut)
	{
		CueType = EBHScareEventType::LightCut;
	}
	else if (Variant == 4 || Variant == 5 || Variant == 6)
	{
		CueType = EBHScareEventType::Whisper;
	}
	else if (Variant == 7 || Variant == 10)
	{
		CueType = EBHScareEventType::FootstepEcho;
	}
	else if (Variant == 0 || Variant == 3)
	{
		CueType = EBHScareEventType::AudioStinger;
	}

	if (AtmosphereDirector)
	{
		const float PressureAlpha = FMath::Clamp((Target->GetFear() + Target->GetDread()) / 180.0f, 0.0f, 1.0f);
		CueType = AtmosphereDirector->ChoosePressureCueType(Target, BehindTarget, PressureAlpha, false);
	}

	FBHScareEventSpec Spec;
	Spec.EventType = CueType;
	Spec.Target = Target;
	Spec.Origin = BehindTarget;
	Spec.Intensity = FMath::Clamp(0.32f + (Frequency / 620.0f) * 0.24f + (Pulse / 8.0f) * 0.12f, 0.32f, 0.74f);
	Spec.Message = Message;
	Spec.LightRadius = bPrefersLightCut ? FMath::Max(1700.0f, FVector::Dist2D(TargetLocation, BehindTarget) + 1200.0f) : 1400.0f;
	Spec.LockSeconds = CueType == EBHScareEventType::LightCut ? 4.8f : 0.0f;
	const bool bTriggered = TriggerAtmosphereCue(Spec);
	if (bTriggered)
	{
		const UEnum* ScareEnum = StaticEnum<EBHScareEventType>();
		const FString ScareType = ScareEnum ? ScareEnum->GetNameStringByValue(static_cast<int64>(Spec.EventType)) : TEXT("AtmosphereCue");
		RecordPlaytestTelemetryMarker(TEXT("scare_cue"), BehindTarget, FString::Printf(TEXT("revision_%s variant=%d"), *ScareType, Variant), nullptr, Target->GetPlayerState<ABHPlayerState>());
	}
	return bTriggered;
}

void ABHGameMode::TriggerTeacherFlatScare(ABHCharacter* Target, const FVector& FocusLocation, const FString& Message, float LockSeconds)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	SpawnAmbient(FocusLocation + FVector(0.0f, 0.0f, 85.0f), 620.0f, 0.32f, 0.24f, 7.4f, 4.2f);
	CutLightsForJumpscare(Target->GetActorLocation(), FocusLocation, 2200.0f, 5.5f);
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		PC->ClientSnapViewToFlatFocus(FocusLocation + FVector(0.0f, 0.0f, 120.0f));
		PC->ClientShowStatusMessage(Message, 3.2f);
	}
	FreezeTargetForJumpscare(Target, LockSeconds);
	Target->AddFear(18.0f);
	Target->AddDread(24.0f);
}

void ABHGameMode::TriggerTeacherCounterJumpscare(ABHObjectiveStation* Station, EBHRevisionCounterNodeType CounterType)
{
	if (!bRevisionMode || !Station || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastTeacherCounterScareTime < 2.0f)
	{
		return;
	}
	LastTeacherCounterScareTime = Now;

	FString Message = CounterType == EBHRevisionCounterNodeType::PeerReview
		? TEXT("Peer review counter: the class corrected your marking.")
		: TEXT("Demonstration trap: the practical fired back.");
	if (CounterType == EBHRevisionCounterNodeType::DemonstrationTrap)
	{
		switch (Station->GetStationType())
		{
		case EBHObjectiveStationType::Terminal:
			Message = TEXT("Circuit breaker pop: the students overloaded your scare.");
			break;
		case EBHObjectiveStationType::Valve:
			Message = TEXT("Force demo: a desk slams across your path.");
			break;
		case EBHObjectiveStationType::Antenna:
			Message = TEXT("Wave demo: the Doppler scream comes back at you.");
			break;
		case EBHObjectiveStationType::Evidence:
			Message = TEXT("Energy demo: the Sankey arrow snaps shut.");
			break;
		}
	}

	int32 HitTeachers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHCharacter* Teacher = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = Teacher ? Teacher->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Teacher || !BHPS || !BHPS->IsAliveHunter())
		{
			continue;
		}

		++HitTeachers;
		if (CounterType == EBHRevisionCounterNodeType::DemonstrationTrap && FMath::FRand() < 0.35f)
		{
			TriggerMonsterChargeJumpscare(Teacher);
			PC->ClientShowStatusMessage(Message, 3.2f);
		}
		else
		{
			TriggerTeacherFlatScare(Teacher, Station->GetActorLocation(), Message, CounterType == EBHRevisionCounterNodeType::PeerReview ? 1.65f : 2.0f);
		}
	}

	if (HitTeachers > 0)
	{
		BroadcastStatus(CounterType == EBHRevisionCounterNodeType::PeerReview
			? TEXT("Peer Review complete: the Teacher was stunned by a correction chain.")
			: TEXT("Demonstration Trap complete: the practical fired back at the Teacher."), 4.0f);
		ApplyPresenceSpike(Station->GetActorLocation(), 72.0f, TEXT("The class fought back with physics."));
	}
}

void ABHGameMode::ActivateStudentScareRelay(ABHCharacter* Activator, ABHObjectiveStation* SourceNode)
{
	if (!Activator || !GetWorld())
	{
		return;
	}

	int32 ScaredTeachers = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHCharacter* TeacherTarget = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* BHPS = TeacherTarget ? TeacherTarget->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!TeacherTarget || TeacherTarget == Activator || !BHPS || !BHPS->IsAliveHunter())
		{
			continue;
		}

		++ScaredTeachers;
		TriggerProperScareOnTeacher(TeacherTarget);
		PC->ClientShowStatusMessage(TEXT("Something sees you."), 2.75f);
	}

	if (ABHPlayerController* ActivatorPC = Cast<ABHPlayerController>(Activator->GetController()))
	{
		ActivatorPC->ClientShowStatusMessage(ScaredTeachers > 0 ? TEXT("Scare relay armed.") : TEXT("Scare relay armed, but no Teacher signal was found."), 2.75f);
	}
}

bool ABHGameMode::TriggerStudentScareSwitch(ABHCharacter* Activator, const FVector& SourceLocation, const FString& ScareTitle, int32 Severity)
{
	if (!Activator || !GetWorld())
	{
		return false;
	}

	ABHPlayerController* ActivatorPC = Cast<ABHPlayerController>(Activator->GetController());
	const ABHPlayerState* ActivatorPS = Activator->GetBHPlayerState();
	const bool bStudentRole = ActivatorPS
		&& ActivatorPS->LifeState == EBHPlayerLifeState::Alive
		&& (ActivatorPS->PlayerRole == EBHPlayerRole::Survivor
			|| ActivatorPS->PlayerRole == EBHPlayerRole::FakeHunter
			|| ActivatorPS->PlayerRole == EBHPlayerRole::Tester);
	if (!bStudentRole)
	{
		if (ActivatorPC)
		{
			ActivatorPC->ClientShowStatusMessage(TEXT("Student scare toggles are student-only."), 2.4f);
		}
		return false;
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const bool bScarePhase = bPracticeMode
		|| bTestMode
		|| (BHGS && (BHGS->bTestMode || BHGS->RoundPhase == EBHRoundPhase::Hunt || BHGS->RoundPhase == EBHRoundPhase::FinalEscape));
	if (!bScarePhase)
	{
		if (ActivatorPC)
		{
			ActivatorPC->ClientShowStatusMessage(TEXT("Scare toggles wake up when Hunt starts."), 2.6f);
		}
		return false;
	}

	const int32 ClampedSeverity = FMath::Clamp(Severity, 1, 4);
	static const TCHAR* DefaultTitles[] = {
		TEXT("Shadow Flicker"),
		TEXT("Static Face"),
		TEXT("Hallway Lunge"),
		TEXT("Blackout Charge")
	};
	const FString Title = ScareTitle.IsEmpty() ? FString(DefaultTitles[ClampedSeverity - 1]) : ScareTitle.Left(24);

	ABHCharacter* TargetTeacher = nullptr;
	ABHPlayerController* TargetPC = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHCharacter* Teacher = PC ? Cast<ABHCharacter>(PC->GetPawn()) : nullptr;
		const ABHPlayerState* TeacherPS = Teacher ? Teacher->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Teacher || Teacher == Activator || !TeacherPS || !TeacherPS->IsAliveHunter())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared2D(SourceLocation, Teacher->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			TargetTeacher = Teacher;
			TargetPC = PC;
		}
	}

	if (!TargetTeacher)
	{
		if (ActivatorPC)
		{
			ActivatorPC->ClientShowStatusMessage(TEXT("No Teacher signal found for the scare toggle."), 2.6f);
		}
		return false;
	}

	static const TCHAR* TeacherMessages[] = {
		TEXT("a face flashes at the edge of your vision."),
		TEXT("static tears open and fills the camera."),
		TEXT("something lunges out of the corridor."),
		TEXT("something charges from the blackout.")
	};
	const FString TeacherMessage = FString::Printf(TEXT("%s (%d/4): %s"), *Title, ClampedSeverity, TeacherMessages[ClampedSeverity - 1]);
	const FVector TeacherLocation = TargetTeacher->GetActorLocation();
	FVector FocusDirection = TargetPC ? TargetPC->GetControlRotation().Vector() : TargetTeacher->GetActorForwardVector();
	FocusDirection.Z = 0.0f;
	FocusDirection = FocusDirection.GetSafeNormal();
	if (FocusDirection.IsNearlyZero())
	{
		FocusDirection = FVector::ForwardVector;
	}
	const FVector FocusLocation = TeacherLocation + FocusDirection * 230.0f + FVector(0.0f, 0.0f, 132.0f);
	const FBHJumpscareVariant CueVariant = ChooseJumpscareVariant(ClampedSeverity >= 2 ? EBHScareEventType::FaceFlash : EBHScareEventType::AudioStinger);

	// Students scaring the Teacher ALWAYS spring one of the three "proper" scares (turn-around / super chain /
	// SCP-096 variation) — never the lighter face/flash/ambient cues. Severity now only flavours the dread hit
	// and the broadcast. (The old graduated branch is kept compiled-but-disabled below so its locals stay used.)
	if (TargetPC)
	{
		TargetPC->ClientShowStatusMessage(TeacherMessage, 3.0f);
	}
	TriggerProperScareOnTeacher(TargetTeacher);
	TargetTeacher->AddFear(14.0f + ClampedSeverity * 6.0f);
	TargetTeacher->AddDread(12.0f + ClampedSeverity * 7.0f);
	// Legacy graduated path retained (compiled, never taken) so its locals stay referenced; ClampedSeverity is
	// clamped to [1,4] so this is always false without being a literal-constant condition.
	if (ClampedSeverity < 0)
	{
		const float SeverityAlpha = static_cast<float>(ClampedSeverity - 1) / 3.0f;
		const float LockSeconds = ClampedSeverity == 1 ? 0.0f : (ClampedSeverity == 2 ? 0.45f : 1.15f);
		SpawnAmbient(FocusLocation, 320.0f + ClampedSeverity * 140.0f, 0.26f + SeverityAlpha * 0.20f, 0.18f + SeverityAlpha * 0.18f, 5.2f + ClampedSeverity * 1.0f, 3.6f + SeverityAlpha * 1.8f);
		if (ClampedSeverity >= 1)
		{
			CutLightsForJumpscare(TeacherLocation, FocusLocation, 950.0f + ClampedSeverity * 420.0f, 3.4f + SeverityAlpha * 2.4f);
		}
		if (TargetPC)
		{
			if (ClampedSeverity >= 2)
			{
				TargetPC->ClientSnapViewToFlatFocus(FocusLocation);
			}

			FBHClientHorrorCue Cue;
			Cue.EventType = ClampedSeverity >= 2 ? EBHScareEventType::FaceFlash : EBHScareEventType::AudioStinger;
			Cue.FocusLocation = FocusLocation;
			Cue.Message = TeacherMessage;
			Cue.DurationSeconds = 2.0f + ClampedSeverity * 0.42f;
			Cue.LockSeconds = LockSeconds;
			Cue.ShakeIntensity = FMath::Clamp(0.30f + ClampedSeverity * 0.18f, 0.0f, 0.90f);
			Cue.CameraJitterDuration = 0.36f + ClampedSeverity * 0.27f;
			Cue.FlashIntensity = FMath::Clamp(0.25f + ClampedSeverity * 0.14f, 0.0f, 0.78f);
			Cue.FlashColor = CueVariant.LightColor;
			Cue.AudioAsset = CueVariant.LaunchSound;
			Cue.AudioVolume = FMath::Clamp(0.62f + ClampedSeverity * 0.11f, 0.0f, 1.0f);
			Cue.VisualActorClass = CueVariant.VisualActorClass;
			Cue.CloseVisualOffset = CueVariant.CloseVisualOffset;
			Cue.CloseVisualRotation = CueVariant.CloseVisualRotation;
			Cue.CloseVisualScale = CueVariant.CloseVisualScale;
			Cue.VariantId = CueVariant.VariantId;
			Cue.bSnapToFocus = ClampedSeverity >= 2;
			Cue.bLockInput = LockSeconds > 0.0f;
			Cue.bCloseRangeFocus = ClampedSeverity >= 2;
			// Full body on every close-range scare (legs included) instead of cropping to the upper torso.
			Cue.bUpperBodyCloseVisual = false;
			TargetPC->ClientPlayHorrorCue(Cue);
			TargetPC->ClientShowStatusMessage(TeacherMessage, 2.6f);
		}
		if (LockSeconds > 0.0f)
		{
			FreezeTargetForJumpscare(TargetTeacher, LockSeconds);
		}
		TargetTeacher->AddFear(10.0f + ClampedSeverity * 7.0f);
		TargetTeacher->AddDread(8.0f + ClampedSeverity * 8.0f);
	}

	ApplyPresenceSpike(SourceLocation, 34.0f + ClampedSeverity * 10.0f, FString::Printf(TEXT("A student triggered %s."), *Title));
	if (ActivatorPC)
	{
		ActivatorPC->ClientShowStatusMessage(FString::Printf(TEXT("%s sent to the Teacher. Severity %d/4."), *Title, ClampedSeverity), 2.5f);
	}
	if (ClampedSeverity >= 3)
	{
		BroadcastStatus(FString::Printf(TEXT("Student scare fired: %s %d/4."), *Title, ClampedSeverity), 2.6f);
	}
	return true;
}

void ABHGameMode::TriggerMonsterChargeJumpscare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	// The ambient monster charge IS the SCP-096 beat, but with a genuine imported creature instead of the old
	// low-poly proxy: it reveals at distance with a faint red glow, holds, then sprints in screaming and cuts
	// the lights afterwards. (bForceRevealSequence keeps that choreography even though the variant isn't the
	// legacy SCP096 id.)
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge, /*bPreferRealMesh=*/true);
	TriggerMonsterChargeJumpscareWithVariant(Target, Variant, TEXT("Something sees you."), 38.0f, 42.0f, /*bForceRevealSequence=*/true);
}

void ABHGameMode::TriggerScp096Scare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	// The classic SCP-096 beat, now wearing a real creature: it appears far off with a faint red glow, holds
	// (the dread pause), then sprints in screaming and the lights cut out for a few seconds afterwards. Force a
	// genuine imported monster so it is never the low-poly proxy; the reveal choreography is driven by the flag.
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge, /*bPreferRealMesh=*/true);
	TriggerMonsterChargeJumpscareWithVariant(Target, Variant, TEXT("It sees you."), 40.0f, 44.0f, /*bForceRevealSequence=*/true);
}

void ABHGameMode::TriggerRealScp096Scare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	// Same beloved SCP-096 flow (distant reveal -> faint red -> pause -> charge+scream -> lights cut), but
	// driven by the genuine imported SCP-096 creature instead of a pack monster. This is a NEW scare layered on
	// top of the existing flow function — it does not touch the original SCP-096 scare or any other.
	const FBHJumpscareVariant Variant = MakeRealScp096JumpscareVariant();
	TriggerMonsterChargeJumpscareWithVariant(Target, Variant, TEXT("SCP-096 has seen your face."), 42.0f, 46.0f, /*bForceRevealSequence=*/true);
}

bool ABHGameMode::IsTeacherNearby(const FVector& Location, float Radius) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float RadiusSq = FMath::Square(FMath::Max(Radius, 0.0f));
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		const ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		// The "teacher" is the Hunter. (FakeHunters are still students, so they do not suppress scares.)
		if (!BHPS || BHPS->PlayerRole != EBHPlayerRole::Hunter)
		{
			continue;
		}
		const APawn* Pawn = PC->GetPawn();
		if (Pawn && FVector::DistSquared(Pawn->GetActorLocation(), Location) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

void ABHGameMode::TriggerTeacherChosenScare(ABHCharacter* Target, ABHPlayerController* TargetPC)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	// Roll one of the showcased scares so "the Teacher chose you" never feels the same twice: the super
	// jumpscare chain, the turn-around payoff, a corner peek, the in-your-face slam, or the SCP-096 reveal.
	enum class EBHTeacherScarePick : uint8 { SuperChain, BehindYou, CornerPeek, FaceSlam, Scp096, Count };
	const int32 Roll = FMath::RandRange(0, static_cast<int32>(EBHTeacherScarePick::Count) - 1);
	switch (static_cast<EBHTeacherScarePick>(Roll))
	{
	case EBHTeacherScarePick::SuperChain:
		// The super chain plays on the victim's own controller (it scares that controller's pawn).
		if (TargetPC)
		{
			TriggerTesterSuperJumpscare(TargetPC);
		}
		else
		{
			TriggerScp096Scare(Target);
		}
		break;
	case EBHTeacherScarePick::BehindYou:
		TriggerBehindYouScare(Target);
		break;
	case EBHTeacherScarePick::CornerPeek:
		TriggerCornerPeek(Target);
		break;
	case EBHTeacherScarePick::FaceSlam:
		TriggerFaceImageJumpscare(Target);
		break;
	case EBHTeacherScarePick::Scp096:
	default:
		TriggerScp096Scare(Target);
		break;
	}
}

void ABHGameMode::TriggerProperScareOnTeacher(ABHCharacter* Teacher)
{
	if (!Teacher || !GetWorld())
	{
		return;
	}

	ABHPlayerController* TeacherPC = Cast<ABHPlayerController>(Teacher->GetController());
	// Only the three "proper" scares may hit the teacher: turn-around, super-jumpscare chain, or an SCP-096
	// variation (real model or the pack-monster reveal). Never the lighter face/peek/ambient cues.
	enum class EBHProperTeacherScare : uint8 { BehindYou, SuperChain, Scp096, Count };
	const int32 Roll = FMath::RandRange(0, static_cast<int32>(EBHProperTeacherScare::Count) - 1);
	switch (static_cast<EBHProperTeacherScare>(Roll))
	{
	case EBHProperTeacherScare::BehindYou:
		TriggerBehindYouScare(Teacher);
		break;
	case EBHProperTeacherScare::SuperChain:
		if (TeacherPC)
		{
			TriggerTesterSuperJumpscare(TeacherPC);
		}
		else
		{
			TriggerScp096Scare(Teacher);
		}
		break;
	case EBHProperTeacherScare::Scp096:
	default:
		// Alternate the two SCP-096 variations: the genuine imported model and the pack-monster reveal.
		if (FMath::RandBool())
		{
			TriggerRealScp096Scare(Teacher);
		}
		else
		{
			TriggerScp096Scare(Teacher);
		}
		break;
	}
}

void ABHGameMode::TriggerMonsterChargeJumpscareWithVariant(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float FearAmount, float DreadAmount, bool bForceRevealSequence)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	// Remember the variant so the next pick can avoid an immediate repeat.
	if (!Variant.VariantId.IsNone())
	{
		LastJumpscareVariantId = Variant.VariantId;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const int32 EffectiveScareIntensity = GetEffectiveScareIntensity();
	const bool bLegacyScp096 = BHIsLegacyScp096JumpscareVariant(Variant);
	// The classic SCP-096 choreography (distant reveal -> pause -> charge -> blackout) runs for the legacy
	// proxy AND any modern variant the caller flags, so a real creature can carry the same scare.
	const bool bRevealSequence = bLegacyScp096 || bForceRevealSequence;
	const float IntensityAlpha = static_cast<float>(EffectiveScareIntensity) / 3.0f;
	const float Speed = bRevealSequence
		? (bPartyPace ? 9800.0f : FMath::Lerp(8800.0f, 10000.0f, IntensityAlpha))
		: (bPartyPace ? 10800.0f : FMath::Lerp(9000.0f, 10300.0f, IntensityAlpha));
	const float HoldDuration = bRevealSequence
		? (bPartyPace ? 2.75f : FMath::Lerp(3.05f, 3.55f, IntensityAlpha))
		: (bPartyPace ? 2.15f : FMath::Lerp(2.65f, 3.25f, IntensityAlpha));

	TArray<FVector> Candidates;
	Candidates.Reserve(ScarePoints.Num() + 24);
	for (const FVector& Point : ScarePoints)
	{
		Candidates.Add(Point);
	}

	const float MaxX = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? 5850.0f : 5350.0f;
	const float MaxY = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase) ? 4250.0f : 4250.0f;
	FVector ViewLocation = FVector::ZeroVector;
	FVector Forward = Target->GetActorForwardVector().GetSafeNormal();
	BHResolveJumpscareView(Target, TargetPC, ViewLocation, Forward);
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	auto AddDirectionalCandidates = [&](const TArray<FVector>& Dirs, const TArray<float>& Dists)
	{
		for (const FVector& Dir : Dirs)
		{
			if (Dir.IsNearlyZero())
			{
				continue;
			}
			for (float Distance : Dists)
			{
				FVector Candidate = TargetLocation + Dir * Distance;
				Candidate.X = FMath::Clamp(Candidate.X, -MaxX, MaxX);
				Candidate.Y = FMath::Clamp(Candidate.Y, -MaxY, MaxY);
				Candidates.Add(Candidate);
			}
		}
	};

	// Legacy SCP-096 keeps its scripted head-on reveal; everything else rolls an even-mix approach.
	const EBHJumpscareApproach Approach = bRevealSequence ? EBHJumpscareApproach::HeadOn : ChooseMonsterChargeApproach();

	FBHResolvedJumpscareSpawn ResolvedSpawn;
	bool bResolved = false;
	bool bDescend = false;
	float UseSpeed = Speed;
	float UseHold = HoldDuration;

	switch (Approach)
	{
	case EBHJumpscareApproach::AlreadyThere:
		// Already in view, close, lunges almost immediately.
		Candidates.Reset();
		AddDirectionalCandidates({ Forward, Right, -Right }, { 1100.0f, 1500.0f, 1900.0f, 2300.0f });
		bResolved = ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 600.0f, 2600.0f, 70.0f, ResolvedSpawn);
		UseSpeed = Speed * 1.3f;
		UseHold = 0.0f;
		break;

	case EBHJumpscareApproach::Behind:
		// Out of view behind the player; reveal lands on contact.
		Candidates.Reset();
		AddDirectionalCandidates({ -Forward, Right, -Right }, { 1400.0f, 1900.0f, 2500.0f, 3200.0f });
		bResolved = ResolveDirectionalJumpscareSpawn(Target, Candidates, FocusHeight, 900.0f, 3600.0f, 70.0f, 0.0f, /*bPreferClose=*/true, ResolvedSpawn);
		UseHold = FMath::Min(HoldDuration, 0.5f);
		break;

	case EBHJumpscareApproach::CeilingDrop:
		// Roughly overhead, descends onto the target.
		Candidates.Reset();
		AddDirectionalCandidates({ Forward, -Forward, Right, -Right }, { 120.0f, 280.0f, 440.0f });
		bResolved = ResolveDirectionalJumpscareSpawn(Target, Candidates, FocusHeight, 0.0f, 1400.0f, 90.0f, 560.0f, /*bPreferClose=*/true, ResolvedSpawn);
		bDescend = true;
		UseSpeed = Speed * 0.9f;
		UseHold = 0.0f;
		break;

	case EBHJumpscareApproach::HeadOn:
	default:
		// Parity with the other approaches: discard the pre-seeded ScarePoints so HeadOn relies on
		// its own freshly built directional candidates rather than stale level scare anchors.
		Candidates.Reset();
		AddDirectionalCandidates({ Forward, Right, -Right, -Forward }, { 4600.0f, 3800.0f, 3000.0f, 2200.0f, 1500.0f });
		bResolved = ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 900.0f, 6400.0f, 70.0f, ResolvedSpawn);
		break;
	}

	// Fall back to a head-on visible spawn (then the overlay) if the chosen approach found no clear spot.
	if (!bResolved && Approach != EBHJumpscareApproach::HeadOn)
	{
		Candidates.Reset();
		for (const FVector& Point : ScarePoints)
		{
			Candidates.Add(Point);
		}
		AddDirectionalCandidates({ Forward, Right, -Right, -Forward }, { 4600.0f, 3800.0f, 3000.0f, 2200.0f, 1500.0f });
		bResolved = ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 900.0f, 6400.0f, 70.0f, ResolvedSpawn);
		bDescend = false;
		UseSpeed = Speed;
		UseHold = HoldDuration;
	}

	if (!bResolved)
	{
		TriggerCloseOverlayJumpscare(Target, Variant, Message, HoldDuration, FMath::Min(FearAmount, 22.0f), FMath::Min(DreadAmount, 24.0f));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(ResolvedSpawn.SpawnLocation, ResolvedSpawn.SpawnRotation, SpawnParams))
	{
		Monster->Configure(Target, UseSpeed, bPartyPace ? 7.4f : 8.6f, UseHold);
		Monster->ConfigureVariant(Variant);
		Monster->SetChargeDescend(bDescend);
	}
	else
	{
		TriggerCloseOverlayJumpscare(Target, Variant, Message, HoldDuration, FMath::Min(FearAmount, 22.0f), FMath::Min(DreadAmount, 24.0f));
		return;
	}

	// The lights sink to ~15% the moment the creature appears (held through the reveal pause); the hard
	// blackout below/at-impact then takes over. A reveal sequence lingers longer so the dread of the pause lands.
	const float DimSeconds = FMath::Clamp(HoldDuration + (bRevealSequence ? 1.0f : 0.5f), 2.5f, 5.0f);
	DimLightsForJumpscare(TargetLocation, ResolvedSpawn.SpawnLocation, 2800.0f, 0.15f, DimSeconds);
	// Also sink the player's whole view (environmental brightness), so even areas with no flicker-lights go
	// dark around the creature — the lit monster stays the brightest, still-visible thing on screen.
	if (TargetPC)
	{
		TargetPC->ClientDimEnvironment(0.45f, DimSeconds);
	}

	const UEnum* ApproachEnum = StaticEnum<EBHJumpscareApproach>();
	const FString ApproachName = ApproachEnum ? ApproachEnum->GetNameStringByValue(static_cast<int64>(Approach)) : FString::FromInt(static_cast<int32>(Approach));
	RecordPlaytestTelemetryMarker(TEXT("jumpscare"), TargetLocation, FString::Printf(TEXT("monster_charge variant=%s approach=%s reveal=%d"), *Variant.VariantId.ToString(), *ApproachName, bRevealSequence ? 1 : 0), nullptr, Target->GetPlayerState<ABHPlayerState>());
	if (bRevealSequence)
	{
		if (TargetPC)
		{
			TargetPC->ClientSnapViewToFlatFocus(ResolvedSpawn.FocusLocation);

			const float SensoryScale = GetScareSensoryScale();
			FBHClientHorrorCue RevealCue;
			RevealCue.EventType = EBHScareEventType::MonsterCharge;
			RevealCue.FocusLocation = ResolvedSpawn.FocusLocation;
			RevealCue.Message = Message;
			RevealCue.DurationSeconds = HoldDuration + 0.85f;
			RevealCue.LockSeconds = HoldDuration;
			RevealCue.ShakeIntensity = FMath::Clamp(0.18f * SensoryScale, 0.0f, 1.0f);
			RevealCue.CameraJitterDuration = 0.25f * SensoryScale;
			RevealCue.FlashIntensity = 0.0f;
			RevealCue.FlashColor = Variant.LightColor;
			RevealCue.CloseVisualOffset = Variant.CloseVisualOffset;
			RevealCue.CloseVisualRotation = Variant.CloseVisualRotation;
			RevealCue.CloseVisualScale = Variant.CloseVisualScale;
			RevealCue.VariantId = Variant.VariantId;
			RevealCue.bSnapToFocus = true;
			RevealCue.bLockInput = true;
			TargetPC->ClientPlayHorrorCue(RevealCue);
		}

		FTimerDelegate ChargeCueDelegate;
		TWeakObjectPtr<ABHGameMode> WeakGameMode(this);
		TWeakObjectPtr<ABHPlayerController> WeakTargetPC(TargetPC);
		const FBHJumpscareVariant ChargeVariant = Variant;
		const FVector ChargeFocusLocation = ResolvedSpawn.FocusLocation;
		const FVector ChargeMonsterLocation = ResolvedSpawn.SpawnLocation;
		const FString ChargeMessage;
		const float ChargeRestoreDelay = bPartyPace ? 9.0f : 10.25f;
		const float ChargeSensoryScale = GetScareSensoryScale();
		const float AudioSensoryScale = ChargeSensoryScale <= 0.0f ? 0.0f : FMath::Lerp(0.35f, 1.0f, ChargeSensoryScale);
		ChargeCueDelegate.BindLambda([WeakGameMode, WeakTargetPC, ChargeVariant, ChargeFocusLocation, ChargeMonsterLocation, TargetLocation, ChargeMessage, ChargeRestoreDelay, ChargeSensoryScale, AudioSensoryScale]()
		{
			if (ABHPlayerController* SprintPC = WeakTargetPC.Get())
			{
				FBHClientHorrorCue ChargeCue;
				ChargeCue.EventType = EBHScareEventType::MonsterCharge;
				ChargeCue.FocusLocation = ChargeFocusLocation;
				ChargeCue.Message = ChargeMessage;
				ChargeCue.DurationSeconds = 3.95f;
				ChargeCue.LockSeconds = 0.55f;
				ChargeCue.ShakeIntensity = FMath::Clamp(FMath::Max(ChargeVariant.CameraShakeIntensity, 0.96f) * ChargeSensoryScale, 0.0f, 1.0f);
				ChargeCue.CameraJitterDuration = FMath::Max(ChargeVariant.CameraJitterDuration, 1.25f) * ChargeSensoryScale;
				ChargeCue.CameraJitterFrequency = 58.0f;
				ChargeCue.FlashIntensity = FMath::Clamp(FMath::Max(ChargeVariant.FlashIntensity, 0.92f) * ChargeSensoryScale, 0.0f, 1.0f);
				ChargeCue.FlashColor = ChargeVariant.LightColor;
				ChargeCue.AudioAsset = ChargeVariant.LaunchSound;
				ChargeCue.AudioVolume = FMath::Clamp(1.15f * AudioSensoryScale, 0.0f, 2.0f);
				ChargeCue.CloseVisualOffset = ChargeVariant.CloseVisualOffset;
				ChargeCue.CloseVisualRotation = ChargeVariant.CloseVisualRotation;
				ChargeCue.CloseVisualScale = ChargeVariant.CloseVisualScale;
				ChargeCue.VariantId = ChargeVariant.VariantId;
				ChargeCue.bSnapToFocus = true;
				ChargeCue.bLockInput = true;
				SprintPC->ClientPlayHorrorCue(ChargeCue);
			}
			if (ABHGameMode* GameMode = WeakGameMode.Get())
			{
				GameMode->CutLightsForJumpscare(TargetLocation, ChargeMonsterLocation, 0.0f, ChargeRestoreDelay);
			}
		});

		FTimerHandle ChargeCueHandle;
		GetWorldTimerManager().SetTimer(ChargeCueHandle, ChargeCueDelegate, HoldDuration, false);
	}
	else
	{
		SendJumpscareChargeCue(Target, Variant, ResolvedSpawn.FocusLocation, Message, HoldDuration, 1.0f, false);
		CutLightsForJumpscare(TargetLocation, ResolvedSpawn.SpawnLocation, 0.0f, bPartyPace ? 9.0f : 10.25f);
	}
	FreezeTargetForJumpscare(Target, HoldDuration);

	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(FearAmount);
	Target->AddDread(DreadAmount);

	if (!bRevealSequence)
	{
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (Light && Light->GetCircuitId() > 0 && FVector::DistSquared2D(Light->GetActorLocation(), TargetLocation) <= FMath::Square(2400.0f) && FMath::FRand() < 0.34f)
			{
				Light->SetPowered(false);
			}
		}
	}
}

void ABHGameMode::TriggerFaceImageJumpscare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	if (!TargetPC)
	{
		return;
	}

	// A real creature slammed into the camera — the same close-up payoff mechanic the "behind you" scare
	// uses (which reads as a genuine monster in your face), fired immediately instead of after a turn. Force a
	// genuine imported monster (never the low-poly proxy) for this showcased scare.
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge, /*bPreferRealMesh=*/true);
	if (!Variant.VariantId.IsNone())
	{
		LastJumpscareVariantId = Variant.VariantId;
	}
	const float SensoryScale = GetScareSensoryScale();
	const float AudioSensoryScale = SensoryScale <= 0.0f ? 0.0f : FMath::Lerp(0.35f, 1.0f, SensoryScale);

	FBHClientHorrorCue Cue;
	Cue.EventType = EBHScareEventType::MonsterCharge;
	Cue.FocusLocation = Target->GetActorLocation();
	Cue.DurationSeconds = 1.6f;
	Cue.LockSeconds = 0.85f;
	Cue.bLockInput = true;
	Cue.bCloseRangeFocus = true;  // spawn + lunge the close-up creature at the camera
	Cue.bUpperBodyCloseVisual = false;  // show the FULL towering creature (legs included) so it never reads as a stumpy torso
	Cue.bSnapToFocus = false;     // appears in front of wherever the player is already looking
	Cue.VariantId = Variant.VariantId;
	Cue.CloseVisualOffset = FVector(82.0f, 0.0f, -52.0f);
	Cue.CloseVisualRotation = Variant.CloseVisualRotation;
	Cue.CloseVisualScale = Variant.CloseVisualScale;
	Cue.ShakeIntensity = FMath::Clamp(FMath::Max(Variant.CameraShakeIntensity, 0.92f) * SensoryScale, 0.0f, 1.0f);
	Cue.CameraJitterDuration = FMath::Max(Variant.CameraJitterDuration, 1.15f) * SensoryScale;
	Cue.CameraJitterFrequency = 56.0f;
	Cue.FlashIntensity = FMath::Clamp(FMath::Max(Variant.FlashIntensity, 0.85f) * SensoryScale, 0.0f, 1.0f);
	Cue.FlashColor = Variant.LightColor;
	Cue.AudioAsset = Variant.LaunchSound;
	Cue.AudioVolume = FMath::Clamp(1.25f * AudioSensoryScale, 0.0f, 2.0f);
	Cue.ImpactStinger = Variant.ImpactStinger;
	Cue.bLayeredImpactAudio = true;
	Cue.FOVPunch = FMath::Clamp(FMath::Max(Variant.ImpactFOVPunch, 12.0f) * SensoryScale, 0.0f, 30.0f);
	Cue.HitStopSeconds = FMath::Clamp(FMath::Max(Variant.ImpactHitStopSeconds, 0.06f) * SensoryScale, 0.0f, 0.25f);
	Cue.RumbleIntensity = FMath::Clamp(FMath::Max(Variant.ImpactRumbleIntensity, 0.8f) * SensoryScale, 0.0f, 1.0f);
	TargetPC->ClientPlayHorrorCue(Cue);

	RecordPlaytestTelemetryMarker(TEXT("jumpscare"), Target->GetActorLocation(), FString::Printf(TEXT("face_monster variant=%s"), *Variant.VariantId.ToString()), nullptr, Target->GetPlayerState<ABHPlayerState>());
	FreezeTargetForJumpscare(Target, 0.85f);
	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(28.0f);
	Target->AddDread(26.0f);
}

void ABHGameMode::TriggerBehindYouScare(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	if (!TargetPC)
	{
		return;
	}

	// Force a genuine imported creature for the payoff slam, never the low-poly proxy.
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge, /*bPreferRealMesh=*/true);
	if (!Variant.VariantId.IsNone())
	{
		LastJumpscareVariantId = Variant.VariantId;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector Forward = Target->GetActorForwardVector().GetSafeNormal();
	BHResolveJumpscareView(Target, TargetPC, ViewLocation, Forward);
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	// The presence is directly behind the player at roughly eye height — the focus they must turn to face.
	const FVector FocusLocation = ViewLocation - Forward * 200.0f;

	static const TCHAR* Directives[] = {
		TEXT("SOMETHING IS BEHIND YOU"),
		TEXT("DON'T TURN AROUND"),
		TEXT("YOU FEEL HOT BREATH ON YOUR NECK"),
		TEXT("IT'S RIGHT BEHIND YOU"),
		TEXT("DO NOT LOOK BACK")
	};
	const FString Directive = Directives[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(Directives)) - 1)];

	const float SensoryScale = GetScareSensoryScale();
	const float AudioSensoryScale = SensoryScale <= 0.0f ? 0.0f : FMath::Lerp(0.35f, 1.0f, SensoryScale);
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float MaxHold = Settings ? FMath::Clamp(Settings->BehindYouMaxHoldSeconds, 1.5f, 8.0f) : 6.0f;

	FBHClientHorrorCue Cue;
	Cue.EventType = EBHScareEventType::BehindYou;
	Cue.FocusLocation = FocusLocation;
	Cue.Message = Directive;
	Cue.bDirectivePrompt = true;
	Cue.bMoveOnlyLock = true;
	Cue.bLockInput = true;
	Cue.LockSeconds = MaxHold;
	Cue.DurationSeconds = MaxHold;
	Cue.TurnReleaseHalfAngleDegrees = 42.0f;
	// Payoff framing carried on the cue (the client copies it into the close-range payoff slam).
	Cue.VariantId = Variant.VariantId;
	Cue.ShakeIntensity = FMath::Clamp(FMath::Max(Variant.CameraShakeIntensity, 0.92f) * SensoryScale, 0.0f, 1.0f);
	Cue.CameraJitterDuration = FMath::Max(Variant.CameraJitterDuration, 1.15f) * SensoryScale;
	Cue.CameraJitterFrequency = 56.0f;
	Cue.FlashIntensity = FMath::Clamp(FMath::Max(Variant.FlashIntensity, 0.85f) * SensoryScale, 0.0f, 1.0f);
	Cue.FlashColor = Variant.LightColor;
	Cue.AudioAsset = Variant.LaunchSound;
	Cue.AudioVolume = FMath::Clamp(1.25f * AudioSensoryScale, 0.0f, 2.0f);
	Cue.ImpactStinger = Variant.ImpactStinger;
	Cue.CloseVisualOffset = Variant.CloseVisualOffset;
	Cue.CloseVisualRotation = Variant.CloseVisualRotation;
	Cue.CloseVisualScale = Variant.CloseVisualScale;
	Cue.FOVPunch = FMath::Clamp(Variant.ImpactFOVPunch * SensoryScale, 0.0f, 30.0f);
	Cue.HitStopSeconds = FMath::Clamp(Variant.ImpactHitStopSeconds * SensoryScale, 0.0f, 0.25f);
	Cue.RumbleIntensity = FMath::Clamp(Variant.ImpactRumbleIntensity * SensoryScale, 0.0f, 1.0f);
	TargetPC->ClientPlayHorrorCue(Cue);

	RecordPlaytestTelemetryMarker(TEXT("jumpscare"), Target->GetActorLocation(), FString::Printf(TEXT("behind_you variant=%s"), *Variant.VariantId.ToString()), nullptr, Target->GetPlayerState<ABHPlayerState>());
	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	// The dread of the directive lands up front; the payoff scare is its own jolt, sprung client-side.
	Target->AddFear(18.0f);
	Target->AddDread(34.0f);
}

void ABHGameMode::TriggerCornerPeek(ABHCharacter* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge, /*bPreferRealMesh=*/true);
	const FVector TargetLocation = Target->GetActorLocation();
	FVector ViewLocation = FVector::ZeroVector;
	FVector Forward = Target->GetActorForwardVector().GetSafeNormal();
	BHResolveJumpscareView(Target, TargetPC, ViewLocation, Forward);
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	UWorld* World = GetWorld();
	const FVector EyeLocation = ViewLocation;
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	const float Linger = Settings ? FMath::Clamp(Settings->PeekLingerSeconds, 0.6f, 6.0f) : 2.6f;

	FCollisionQueryParams PeekParams(SCENE_QUERY_STAT(BHCornerPeek), false);
	PeekParams.AddIgnoredActor(Target);

	// Sweep horizontal rays across the front of the player to find a wall edge to peek around: a spot where
	// one ray hits a near wall and the neighbour opens past its vertical edge (a corner or doorway). The
	// figure is then placed straddling that edge so the wall occludes its inner half while the head/shoulder
	// lean into view — an actual corner peek rather than a figure standing in the open.
	constexpr int32 NumSteps = 28;
	constexpr float MaxAngle = 110.0f;
	constexpr float TraceLen = 1000.0f;
	constexpr float NearWallMin = 160.0f;
	constexpr float NearWallMax = 850.0f;

	struct FPeekRay
	{
		FVector Dir = FVector::ForwardVector;
		bool bNearWall = false;
		FVector Hit = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
	};
	TArray<FPeekRay> Rays;
	Rays.Reserve(NumSteps + 1);
	for (int32 i = 0; i <= NumSteps; ++i)
	{
		const float Angle = FMath::Lerp(-MaxAngle, MaxAngle, static_cast<float>(i) / static_cast<float>(NumSteps));
		const FVector Dir = Forward.RotateAngleAxis(Angle, FVector::UpVector);
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, EyeLocation, EyeLocation + Dir * TraceLen, ECC_Visibility, PeekParams);
		FPeekRay Ray;
		Ray.Dir = Dir;
		Ray.bNearWall = bHit && Hit.Distance >= NearWallMin && Hit.Distance <= NearWallMax;
		Ray.Hit = bHit ? Hit.ImpactPoint : (EyeLocation + Dir * TraceLen);
		Ray.Normal = bHit ? Hit.ImpactNormal.GetSafeNormal2D() : FVector::ZeroVector;
		Rays.Add(Ray);
	}

	auto DropToFloor = [&](const FVector& InPoint) -> FVector
	{
		const FVector Start = InPoint + FVector(0.0f, 0.0f, 120.0f);
		FHitResult FloorHit;
		if (World->LineTraceSingleByChannel(FloorHit, Start, Start - FVector(0.0f, 0.0f, 600.0f), ECC_Visibility, PeekParams))
		{
			return FloorHit.ImpactPoint;
		}
		return InPoint;
	};

	auto HeadVisible = [&](const FVector& FootPoint) -> bool
	{
		// The head/shoulder must be seen for it to read as "peeking out" rather than fully hidden.
		const FVector Head = FootPoint + FVector(0.0f, 0.0f, 150.0f);
		FHitResult Block;
		const bool bBlocked = World->LineTraceSingleByChannel(Block, EyeLocation, Head, ECC_Visibility, PeekParams)
			&& Block.Distance < (Head - EyeLocation).Size() - 40.0f;
		return !bBlocked;
	};

	// A figure-sized capsule used to reject any candidate spot whose body would be buried in the wall it is
	// trying to peek around — the bug that previously spawned the peeker inside geometry. Lifted clear of the
	// floor so the ground itself never counts as a blocker.
	const FCollisionShape PeekBodyShape = FCollisionShape::MakeCapsule(34.0f, 80.0f);
	auto BodyFitsClear = [&](const FVector& FootPoint) -> bool
	{
		const FVector CapsuleCenter = FootPoint + FVector(0.0f, 0.0f, 95.0f);
		return !World->OverlapBlockingTestByChannel(CapsuleCenter, FQuat::Identity, ECC_Visibility, PeekBodyShape, PeekParams);
	};

	FVector PeekFoot = FVector::ZeroVector;
	bool bFoundEdge = false;
	for (int32 i = 0; i < Rays.Num() - 1 && !bFoundEdge; ++i)
	{
		if (Rays[i].bNearWall == Rays[i + 1].bNearWall)
		{
			continue;
		}

		// One side is a near wall, the other opens up — the boundary is a vertical edge to peek around.
		const FPeekRay& Wall = Rays[i].bNearWall ? Rays[i] : Rays[i + 1];
		const FVector OpenDir = (Rays[i].bNearWall ? Rays[i + 1] : Rays[i]).Dir.GetSafeNormal2D();

		// Outward wall normal: which way is "out of the wall, into the room". Prefer the measured surface
		// normal; fall back to the direction from the wall back toward the player. Force it to face the
		// player's side so the figure is pushed off the wall plane rather than into it.
		FVector OutNormal = Wall.Normal;
		const FVector ToPlayer = (EyeLocation - Wall.Hit).GetSafeNormal2D();
		if (OutNormal.IsNearlyZero())
		{
			OutNormal = ToPlayer;
		}
		if (FVector::DotProduct(OutNormal, ToPlayer) < 0.0f)
		{
			OutNormal *= -1.0f;
		}

		// Try hugging the corner first (small step past the edge, small push off the wall) and only move
		// further into the open if the body would otherwise clip the wall. Each candidate must both clear the
		// geometry and leave the head visible, so it reads as leaning out of the corner — never embedded.
		for (const float OpenNudge : { 30.0f, 50.0f, 75.0f, 105.0f })
		{
			bool bPlaced = false;
			for (const float OutNudge : { 46.0f, 72.0f, 104.0f })
			{
				const FVector Candidate = Wall.Hit + OpenDir * OpenNudge + OutNormal * OutNudge;
				const FVector Foot = DropToFloor(Candidate);
				if (BodyFitsClear(Foot) && HeadVisible(Foot))
				{
					PeekFoot = Foot;
					bFoundEdge = true;
					bPlaced = true;
					break;
				}
			}
			if (bPlaced)
			{
				break;
			}
		}
	}

	if (!bFoundEdge)
	{
		// No corner in view to lean around (or every spot was blocked) — skip quietly; peeks are passive.
		return;
	}

	const FRotator FaceRotation = (FVector(TargetLocation.X, TargetLocation.Y, PeekFoot.Z) - PeekFoot).Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABHJumpscareMonster* Peeker = World->SpawnActor<ABHJumpscareMonster>(PeekFoot, FaceRotation, SpawnParams))
	{
		Peeker->ConfigurePeek(Target, Variant, Linger);
		RecordPlaytestTelemetryMarker(TEXT("scare"), TargetLocation, FString::Printf(TEXT("corner_peek variant=%s"), *Variant.VariantId.ToString()), nullptr, Target->GetPlayerState<ABHPlayerState>());
		// A whisper of dread — peeks are about tension, not a big hit.
		Target->AddDread(10.0f);
	}
}

bool ABHGameMode::ChooseWhisperJumpscareVariant(FBHJumpscareVariant& OutVariant) const
{
	const int32 EffectiveScareIntensity = GetEffectiveScareIntensity();
	TArray<FBHJumpscareVariant> Candidates;
	for (const FBHJumpscareVariant& Variant : GetResolvedWhisperJumpscareVariants())
	{
		if (!Variant.VariantId.IsNone()
			&& Variant.Weight > 0.0f
			&& Variant.MinimumScareIntensity <= EffectiveScareIntensity
			&& BHJumpscareVariantHasExistingVisual(Variant))
		{
			Candidates.Add(Variant);
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	float TotalWeight = 0.0f;
	for (const FBHJumpscareVariant& Candidate : Candidates)
	{
		TotalWeight += FMath::Max(0.01f, Candidate.Weight);
	}

	float Pick = FMath::FRandRange(0.0f, TotalWeight);
	for (const FBHJumpscareVariant& Candidate : Candidates)
	{
		const float CandidateWeight = FMath::Max(0.01f, Candidate.Weight);
		if (Pick <= CandidateWeight)
		{
			OutVariant = Candidate;
			return true;
		}
		Pick -= CandidateWeight;
	}

	OutVariant = Candidates.Last();
	return true;
}

void ABHGameMode::FreezeTargetForJumpscare(ABHCharacter* Target, float DurationSeconds)
{
	if (!Target || !GetWorld() || DurationSeconds <= 0.0f)
	{
		return;
	}

	if (UCharacterMovementComponent* Movement = Target->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController());
	if (PC)
	{
		PC->ClientSetJumpscareInputLocked(true);
	}

	TWeakObjectPtr<ABHCharacter> WeakTarget = Target;
	TWeakObjectPtr<ABHPlayerController> WeakPC = PC;
	FTimerDelegate RestoreDelegate;
	// Owned by the game mode so a round transition can cancel this pending un-freeze (see CutLights).
	RestoreDelegate.BindWeakLambda(this, [WeakTarget, WeakPC]()
	{
		if (WeakPC.IsValid())
		{
			WeakPC->ClientSetJumpscareInputLocked(false);
		}

		if (!WeakTarget.IsValid())
		{
			return;
		}

		ABHCharacter* FrozenTarget = WeakTarget.Get();
		const ABHPlayerState* BHPS = FrozenTarget->GetPlayerState<ABHPlayerState>();
		const bool bAlive = !BHPS || BHPS->LifeState == EBHPlayerLifeState::Alive;
		if (!FrozenTarget->IsHiddenInLocker() && bAlive)
		{
			if (UCharacterMovementComponent* Movement = FrozenTarget->GetCharacterMovement())
			{
				Movement->SetMovementMode(MOVE_Walking);
			}
		}
	});

	FTimerHandle RestoreHandle;
	GetWorldTimerManager().SetTimer(RestoreHandle, RestoreDelegate, DurationSeconds, false);
}

void ABHGameMode::CutLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float RestoreDelaySeconds)
{
	TArray<TWeakObjectPtr<ABHFlickerLight>> AffectedLights;
	TArray<bool> OriginalPoweredStates;

	if (Radius <= 0.0f)
	{
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (Light)
			{
				AffectedLights.Add(Light);
				OriginalPoweredStates.Add(Light->IsPowered());
				Light->SetPowered(false);
			}
		}
	}
	else
	{
		const float RadiusSq = FMath::Square(Radius);
		for (ABHFlickerLight* Light : FlickerLights)
		{
			if (!Light)
			{
				continue;
			}

			const FVector LightLocation = Light->GetActorLocation();
			const bool bNearTarget = FVector::DistSquared2D(LightLocation, TargetLocation) <= RadiusSq;
			const bool bNearMonster = FVector::DistSquared2D(LightLocation, MonsterLocation) <= RadiusSq;
			if (bNearTarget || bNearMonster)
			{
				AffectedLights.Add(Light);
				OriginalPoweredStates.Add(Light->IsPowered());
				Light->SetPowered(false);
			}
		}
	}

	if (AffectedLights.IsEmpty() || RestoreDelaySeconds <= 0.0f || !GetWorld())
	{
		return;
	}

	FTimerDelegate RestoreDelegate;
	// Bind to the game mode so a round transition's ClearAllTimersForObject(this) can cancel a pending
	// restore. A bare BindLambda timer is unowned and survives the phase change, re-powering lights
	// after the round it belonged to has already ended.
	RestoreDelegate.BindWeakLambda(this, [AffectedLights = MoveTemp(AffectedLights), OriginalPoweredStates = MoveTemp(OriginalPoweredStates)]() mutable
	{
		const int32 RestoreCount = FMath::Min(AffectedLights.Num(), OriginalPoweredStates.Num());
		for (int32 Index = 0; Index < RestoreCount; ++Index)
		{
			if (AffectedLights[Index].IsValid())
			{
				AffectedLights[Index]->SetPowered(OriginalPoweredStates[Index]);
			}
		}
	});

	FTimerHandle RestoreHandle;
	GetWorldTimerManager().SetTimer(RestoreHandle, RestoreDelegate, RestoreDelaySeconds, false);
}

void ABHGameMode::DimLightsForJumpscare(const FVector& TargetLocation, const FVector& MonsterLocation, float Radius, float DimScale, float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		return;
	}

	const float ClampedDim = FMath::Clamp(DimScale, 0.05f, 1.0f);
	const float RadiusSq = Radius > 0.0f ? FMath::Square(Radius) : -1.0f;
	// A low, slightly warm hue so it reads as the lights sinking rather than recolouring; the slow burst speed
	// keeps it a steady low glow rather than a strobe. The flicker-burst auto-restores after DurationSeconds.
	const FLinearColor DimColor(0.95f, 0.82f, 0.72f, 1.0f);
	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (!Light || !Light->IsPowered())
		{
			continue; // only dim lights that are actually lit; leave already-dark ones dark
		}
		if (RadiusSq > 0.0f)
		{
			const FVector LightLocation = Light->GetActorLocation();
			if (FVector::DistSquared2D(LightLocation, TargetLocation) > RadiusSq
				&& FVector::DistSquared2D(LightLocation, MonsterLocation) > RadiusSq)
			{
				continue;
			}
		}
		Light->TriggerFlickerBurst(DurationSeconds, ClampedDim, DimColor, 3.0f, /*bForcePowered=*/false);
	}
}

void ABHGameMode::TriggerColdCallEvent()
{
	TArray<ABHCharacter*> Candidates;
	for (TActorIterator<ABHCharacter> It(GetWorld()); It; ++It)
	{
		ABHCharacter* Character = *It;
		const ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
		if (Character && BHPS && BHPS->IsAliveSurvivor())
		{
			Candidates.Add(Character);
		}
	}

	if (Candidates.IsEmpty())
	{
		return;
	}

	ABHCharacter* Target = Candidates[0];
	float BestCandidateScore = -1.0f;
	for (ABHCharacter* Candidate : Candidates)
	{
		if (!Candidate)
		{
			continue;
		}

		const float DreadAlpha = FMath::Clamp(Candidate->GetDread() / 100.0f, 0.0f, 1.0f);
		const float FearAlpha = FMath::Clamp(Candidate->GetFear() / 100.0f, 0.0f, 1.0f);
		const EBHScareEventType CandidateCueType = Candidate->IsHiddenInLocker() ? EBHScareEventType::LockerKnock : EBHScareEventType::Whisper;
		const float BudgetWeight = AtmosphereDirector ? AtmosphereDirector->GetDirectorCueWeight(Candidate, CandidateCueType, 0.54f) : 1.0f;
		const float CandidateScore = FMath::FRandRange(0.0f, 0.34f)
			+ DreadAlpha * 0.36f
			+ FearAlpha * 0.22f
			+ (Candidate->IsHiddenInLocker() ? 0.24f : 0.0f)
			+ (Candidate->IsDetentionMarked() ? 0.18f : 0.0f)
			+ FMath::Clamp(BudgetWeight - 0.50f, -0.34f, 0.28f);
		if (CandidateScore > BestCandidateScore)
		{
			BestCandidateScore = CandidateScore;
			Target = Candidate;
		}
	}

	const bool bTargetHidden = Target->IsHiddenInLocker();
	const FVector SourceLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 84.0f);
	FString BudgetReason;
	const EBHScareEventType ColdCallCueType = bTargetHidden ? EBHScareEventType::LockerKnock : EBHScareEventType::Whisper;
	if (AtmosphereDirector && !AtmosphereDirector->ReserveDirectorCue(Target, ColdCallCueType, SourceLocation, bTargetHidden ? 0.62f : 0.50f, TEXT("cold call"), BudgetReason))
	{
		LastColdCallTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastColdCallTime;
		return;
	}
	SpawnAmbient(SourceLocation - Target->GetActorForwardVector() * 180.0f, FMath::FRandRange(210.0f, 380.0f), bTargetHidden ? 0.24f : 0.18f, 0.18f, 6.2f, 4.0f);
	Target->AddFear(bTargetHidden ? 20.0f : 12.0f);
	Target->AddDread(bTargetHidden ? 26.0f : 16.0f);
	ApplyPresenceSpike(SourceLocation, bTargetHidden ? 78.0f : 66.0f, TEXT("The Teacher called on someone."));
	LastColdCallTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastColdCallTime;

	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Target->GetController()))
	{
		static const TCHAR* OpenMessages[] = {
			TEXT("The intercom says your name."),
			TEXT("A desk scrapes behind you, but there is no classroom."),
			TEXT("You hear chalk write the answer you got wrong."),
			TEXT("The Teacher asks you to explain your working."),
			TEXT("A chair is pulled out for you in the dark."),
			TEXT("The register marks you present twice."),
			TEXT("Someone turns a page directly behind your head."),
			TEXT("The whiteboard squeals your route number."),
			TEXT("A detention slip slides under a door ahead."),
			TEXT("The bell rings once, but only in your corridor."),
			TEXT("A desk lid opens and shuts in time with your steps."),
			TEXT("Your name is whispered from the wrong side of the wall.")
		};
		static const TCHAR* HiddenMessages[] = {
			TEXT("The Teacher calls your name from outside the locker."),
			TEXT("A register is taken inches from your face."),
			TEXT("You hear your chair scrape across the floor."),
			TEXT("The locker walls tighten like a detention desk."),
			TEXT("A clipboard taps against the locker door."),
			TEXT("The roll call stops one name before yours."),
			TEXT("Someone writes detention on the other side."),
			TEXT("A chair leg scrapes under your feet."),
			TEXT("The locker vents breathe chalk dust."),
			TEXT("The Teacher waits for you to answer in a whisper."),
			TEXT("A desk is dragged into place outside the door."),
			TEXT("The register knows where you are hiding.")
		};
		const TCHAR** MessageSet = bTargetHidden ? HiddenMessages : OpenMessages;
		const int32 MessageCount = bTargetHidden ? UE_ARRAY_COUNT(HiddenMessages) : UE_ARRAY_COUNT(OpenMessages);
		PC->ClientShowStatusMessage(MessageSet[FMath::RandRange(0, MessageCount - 1)], 3.25f);
	}

	if (bTargetHidden && FMath::FRand() < 0.34f)
	{
		NotifyLoudNoise(Target->GetActorLocation(), TEXT("stifled answer"));
	}
}

void ABHGameMode::BroadcastStatus(const FString& Message, float DurationSeconds) const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(Message, DurationSeconds);
		}
	}
}

ABHCharacter* ABHGameMode::FindCharacterForPlayerState(APlayerState* TargetPlayerState) const
{
	if (!TargetPlayerState || !GetWorld())
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState == TargetPlayerState)
		{
			return Cast<ABHCharacter>(PC->GetPawn());
		}
	}

	// AI bots are driven by ABHBotController (not a PlayerController), so the loop above never
	// matches them and void recovery used to skip bots entirely. Walk every controller as a
	// fallback so bot pawns are recoverable too.
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		if (Controller && Controller->PlayerState == TargetPlayerState)
		{
			return Cast<ABHCharacter>(Controller->GetPawn());
		}
	}

	return nullptr;
}

void ABHGameMode::UpdateDirectorGameState(const FString& ObjectiveText)
{
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetDirectorState(RoundSeed, ObjectiveText, NextRuntimeLevelName);
		BHGS->SetFogOptions(NextFogPreset, bFogPresetOverride);
		BHGS->SetActiveFogPreset(RuntimeFogPreset);
		BHGS->SetActiveLevelName(RuntimeLevelName);
	}
}

void ABHGameMode::UpdateExitUnlockState(bool bBroadcastBlockReason)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->bExitUnlocked)
	{
		return;
	}

	if (BHGS->BreakersCompleted >= BHGS->BreakersRequired && BHGS->SideObjectivesCompleted >= BHGS->SideObjectivesRequired)
	{
		if (bRevisionMode)
		{
			UpdateRevisionSummary();
			FString RevisionBlockReason;
			if (!CanUnlockRevisionExit(RevisionBlockReason))
			{
				// Silent on the per-tick re-check path so the block reason is not re-broadcast every
				// second; completion events still surface it. The class can re-open the exit at any
				// time once the gate passes (e.g. a blocking straggler is captured, leaves, or earns mastery).
				if (bBroadcastBlockReason)
				{
					BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 54.0f), TEXT("The Teacher is holding a correction conference."), BHGS->PresencePulse + 1);
					UpdateDirectorGameState(GetRevisionObjectiveText());
					BroadcastStatus(RevisionBlockReason, 5.0f);
				}
				return;
			}
		}
		BHGS->SetExitUnlocked(true);
		UpdateStationSignalState(true);
		PublishObjectiveBeats();
		if (IsFinalStage())
		{
			BHGS->SetPresenceState(100.0f, TEXT("Evacuation train authorized."), BHGS->PresencePulse + 1);
			BroadcastStatus(TEXT("Physics mastery met. Final train unlocking."), 4.0f);
			TriggerFinalEscapeIfNeeded();
			return;
		}
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 76.0f), bRevisionMode ? TEXT("The class passed. The Teacher is furious.") : TEXT("The exit woke up. So did everything else."), BHGS->PresencePulse + 1);
		BroadcastStatus(bRevisionMode ? TEXT("Physics mastery met. Reach the subway gate.") : TEXT("Exit power restored. Reach the active exit gate."), 4.0f);
	}
}

void ABHGameMode::RefreshNextLevelFromVotes()
{
	if (!GameState)
	{
		return;
	}

	int32 FacilityVotes = 0;
	int32 SubstationVotes = 0;
	int32 FoggroundsVotes = 0;
	int32 LightFogVotes = 0;
	int32 HeavyFogVotes = 0;
	int32 ExtremeFogVotes = 0;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS)
		{
			continue;
		}

		if (!BHPS->MapVote.IsEmpty())
		{
			const FString Vote = NormalizeBHLevelName(BHPS->MapVote);
			if (Vote.Equals(TEXT("Substation"), ESearchCase::IgnoreCase))
			{
				++SubstationVotes;
			}
			else if (Vote.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase))
			{
				++FoggroundsVotes;
			}
			else
			{
				++FacilityVotes;
			}
		}

		if (BHPS->bHasFogPresetVote)
		{
			switch (BHPS->FogPresetVote)
			{
			case EBHFogPreset::Light:
				++LightFogVotes;
				break;
			case EBHFogPreset::Extreme:
				++ExtremeFogVotes;
				break;
			case EBHFogPreset::Heavy:
			default:
				++HeavyFogVotes;
				break;
			}
		}
	}

	if (FacilityVotes > SubstationVotes && FacilityVotes > FoggroundsVotes)
	{
		NextRuntimeLevelName = TEXT("Facility");
	}
	else if (SubstationVotes > FacilityVotes && SubstationVotes > FoggroundsVotes)
	{
		NextRuntimeLevelName = TEXT("Substation");
	}
	else if (FoggroundsVotes > FacilityVotes && FoggroundsVotes > SubstationVotes)
	{
		NextRuntimeLevelName = TEXT("Foggrounds");
	}

	if (!bFogPresetOverride)
	{
		if (LightFogVotes > HeavyFogVotes && LightFogVotes > ExtremeFogVotes)
		{
			NextFogPreset = EBHFogPreset::Light;
		}
		else if (ExtremeFogVotes > LightFogVotes && ExtremeFogVotes > HeavyFogVotes)
		{
			NextFogPreset = EBHFogPreset::Extreme;
		}
		else if (HeavyFogVotes > LightFogVotes && HeavyFogVotes > ExtremeFogVotes)
		{
			NextFogPreset = EBHFogPreset::Heavy;
		}
	}

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	UpdateDirectorGameState(BHGS ? BHGS->ObjectiveText : FString(TEXT("Map votes updated.")));
	const FString FogMode = bFogPresetOverride ? TEXT("host override") : TEXT("votes");
	BroadcastStatus(FString::Printf(TEXT("Map votes: Facility %d, Substation %d, Foggrounds %d. Next: %s. Fog: %s (%s)."),
		FacilityVotes,
		SubstationVotes,
		FoggroundsVotes,
		*NextRuntimeLevelName,
		*FogPresetToString(NextFogPreset),
		*FogMode), 3.5f);
}

void ABHGameMode::StartPracticeMode(ABHPlayerController* RequestingController)
{
	bPracticeMode = true;
	bTestMode = false;
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS)
		{
			continue;
		}

		if (BHPS->PlayerRole == EBHPlayerRole::Unassigned || BHPS->PlayerRole == EBHPlayerRole::Spectator)
		{
			BHPS->SetRole(EBHPlayerRole::Survivor);
			BHPS->SetDesiredRole(EBHPlayerRole::Survivor);
		}
		BHPS->SetReady(true);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
	}

	RefreshPracticeDirector(TEXT("Practice Lab started. Use Escape to switch roles, modifiers, and objective pressure."));
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Practice Lab: no ready-up, no match timer, no forced round end."), 5.0f);
	}
}

void ABHGameMode::RefreshPracticeDirector(const FString& Reason)
{
	if (!bPracticeMode)
	{
		return;
	}

	PrepareRoundDirector();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetPracticeMode(true);
		BHGS->SetTutorialMode(bTutorialMode);
		BHGS->SetTestMode(false);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 18.0f), TEXT("Practice Lab is holding the round open."), BHGS->PresencePulse + 1);
	}

	UpdateDirectorGameState(FString::Printf(TEXT("Practice Lab: no ready-up and no match timer. Repair %d breakers, answer %d questions, test roles and modifier: %s."),
		ActiveBreakerCount,
		ActiveSideObjectiveCount,
		*GetRoundModifierText(PracticeRoundModifier)));

	StartDirectorTimer();
	BroadcastStatus(Reason, 4.0f);
}

void ABHGameMode::StartTestMode(ABHPlayerController* RequestingController)
{
	bTestMode = true;
	bPracticeMode = false;
	bBotMode = false;
	bRevisionMode = false;
	RevisionMode = EBHRevisionMode::None;
	TargetBotCount = 0;
	GetWorldTimerManager().ClearTimer(RoundTimerHandle);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!BHPS)
		{
			continue;
		}

		BHPS->SetRole(EBHPlayerRole::Tester);
		BHPS->SetDesiredRole(EBHPlayerRole::Tester);
		BHPS->SetReady(true);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetFakeHunterEligible(false);
	}

	RefreshTestDirector(TEXT("Test Round started. Tester role can use survivor objectives and Teacher tools."));
	if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Test Round: no minimum players, no match timer, infinite flashlight, round end blocked."), 5.0f);
	}
}

void ABHGameMode::RefreshTestDirector(const FString& Reason)
{
	if (!bTestMode)
	{
		return;
	}

	PrepareRoundDirector();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetPracticeMode(false);
		BHGS->SetTestMode(true);
		BHGS->SetBotOptions(false, 0, BotDifficulty);
		BHGS->SetRevisionOptions(EBHRevisionMode::None, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(true);
		BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 12.0f), TEXT("Test Round is holding the simulation open."), BHGS->PresencePulse + 1);
	}

	UpdateDirectorGameState(FString::Printf(TEXT("Test Round: Tester role has survivor objectives, Teacher capture/scan/blackout, lockers, doors, lights, scares, and exit access. No ready-up, no match timer, no forced win/loss. Active: %d breakers, %d stations."),
		ActiveBreakerCount,
		ActiveSideObjectiveCount));

	StartDirectorTimer();
	BroadcastStatus(Reason, 4.0f);
}

void ABHGameMode::ResetRoleWarmupForLiveRoundStart()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABHNoiseDecoy> It(World); It; ++It)
	{
		if (ABHNoiseDecoy* Decoy = *It)
		{
			Decoy->Destroy();
		}
	}
	for (TActorIterator<ABHAlarmTrap> It(World); It; ++It)
	{
		if (ABHAlarmTrap* Trap = *It)
		{
			Trap->Destroy();
		}
	}

	for (FConstControllerIterator It = World->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Controller || !BHPS || BHPS->PlayerRole == EBHPlayerRole::Spectator)
		{
			continue;
		}

		if (ABHCharacter* Character = Cast<ABHCharacter>(Controller->GetPawn()))
		{
			Character->ResetRoleWarmupStateForRoundStart();
		}
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->ResetWarmupChecklist();
		RestartPlayer(Controller);
	}
}

EBHRoundModifier ABHGameMode::ChooseRoundModifier(FRandomStream& Stream) const
{
	if (bRevisionMode && RevisionScareIntensity <= 0 && !bPartyPace)
	{
		return EBHRoundModifier::None;
	}

	const int32 Roll = Stream.RandRange(0, 99);
	if (bPartyPace)
	{
		if (Roll < 24)
		{
			return EBHRoundModifier::LightsOut;
		}
		if (Roll < 46)
		{
			return EBHRoundModifier::LoudFooting;
		}
		if (Roll < 66)
		{
			return EBHRoundModifier::JammedDoors;
		}
		if (Roll < 84)
		{
			return EBHRoundModifier::DeadCCTV;
		}
		return EBHRoundModifier::PanicSurge;
	}

	if (ObjectiveIntensity <= 0)
	{
		if (Roll < 45)
		{
			return EBHRoundModifier::DeadCCTV;
		}
		if (Roll < 72)
		{
			return EBHRoundModifier::JammedDoors;
		}
		return EBHRoundModifier::LoudFooting;
	}

	if (Roll < 22)
	{
		return EBHRoundModifier::LightsOut;
	}
	if (Roll < 46)
	{
		return EBHRoundModifier::LoudFooting;
	}
	if (Roll < 68)
	{
		return EBHRoundModifier::JammedDoors;
	}
	if (Roll < 90)
	{
		return EBHRoundModifier::DeadCCTV;
	}
	return EBHRoundModifier::PanicSurge;
}

FString ABHGameMode::GetRoundModifierText(EBHRoundModifier Modifier) const
{
	return ABHGameState::GetRoundModifierTextFor(Modifier);
}

void ABHGameMode::StartPrepPhase()
{
	AssignRoles();
	PrepareRoundDirector();
	BotWorldStimuli.Reset();
	BotObjectiveClaims.Reset();
	BotTargetCooldowns.Reset();
	LoggedBotTacticalWarnings.Reset();
	TelemetryUsedLockerKeys.Reset();
	TelemetryStartedObjectiveKeys.Reset();
	TelemetryCompletedObjectiveKeys.Reset();
	TelemetrySnapshotKeys.Reset();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Prep);
		// Auto-enable a longer warmup on the install's first-ever class (or when the host opts in),
		// so a brand-new student has time to try everything. ForceStart still ends Prep instantly.
		const bool bUseExtendedWarmup = bExtendedFirstWarmup || !bHasHostedClassBefore;
		BHGS->SetRemainingTime(bUseExtendedWarmup ? FMath::Max(PrepSeconds, ExtendedPrepSeconds) : PrepSeconds);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		UpdateDirectorGameState(TEXT("Role warmup: try flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools. The live Hunt resets everyone."));
	}

	// Prop Hunt: publish the props-left/total readout during warmup so the HUD reflects the mode before the Hunt.
	if (bPropHuntMode)
	{
		RefreshPropHuntGameState();
	}

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::TickRoundTimer, 1.0f, true);

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Role warmup started. Try your controls safely; host can press F10 or the menu button to start Hunt now."), 5.0f);
		}
	}
}

void ABHGameMode::StartHuntPhaseImmediately()
{
	AssignRoles();
	PrepareRoundDirector();
	BotWorldStimuli.Reset();
	BotObjectiveClaims.Reset();
	BotTargetCooldowns.Reset();
	LoggedBotTacticalWarnings.Reset();
	TelemetryUsedLockerKeys.Reset();
	TelemetryStartedObjectiveKeys.Reset();
	TelemetryCompletedObjectiveKeys.Reset();
	TelemetrySnapshotKeys.Reset();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(HuntSeconds);
	}
	RecordPlaytestTelemetryMarker(TEXT("round_start"), HunterSpawn, bTestMode ? TEXT("test_hunt_immediate") : TEXT("hunt_immediate"));

	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::TickRoundTimer, 1.0f, true);
	StartDirectorTimer();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Test hunt started. Answer questions, finish tasks, use lockers sparingly, and keep moving."), 4.0f);
		}
	}
}

void ABHGameMode::StartHuntPhase()
{
	ResetRoleWarmupForLiveRoundStart();
	PrepareRoundDirector();
	SweepExpiredBotTacticalState();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(HuntSeconds);
	}
	TelemetryUsedLockerKeys.Reset();
	TelemetryStartedObjectiveKeys.Reset();
	TelemetryCompletedObjectiveKeys.Reset();
	TelemetrySnapshotKeys.Reset();
	RecordPlaytestTelemetryMarker(TEXT("round_start"), HunterSpawn, TEXT("hunt"));
	// Prop Hunt: seed the taunt clock + send the seeker/prop intro now that the live Hunt is underway.
	if (bPropHuntMode)
	{
		BeginPropHuntHunt();
	}
	// Arm the round ticker explicitly (idempotent — SetTimer replaces the handle), like
	// StartHuntPhaseImmediately, rather than relying on the looping timer armed back in StartPrepPhase. If any
	// Prep-phase path cleared RoundTimerHandle, the live Hunt would otherwise have no ticker and the
	// win/loss/time conditions in TickRoundTimer would never evaluate (round hangs).
	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::TickRoundTimer, 1.0f, true);
	StartDirectorTimer();

	// First live Hunt of the install: remember it so the extended first-play warmup auto-enables
	// only for a brand-new class, then defaults off for experienced hosts on later sessions.
	if (!bHasHostedClassBefore)
	{
		bHasHostedClassBefore = true;
		if (UBHGameSettings* MutableSettings = GetMutableDefault<UBHGameSettings>())
		{
			MutableSettings->bHasHostedClassBefore = true;
			MutableSettings->SaveConfig();
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		if (!PC)
		{
			continue;
		}
		// Role-tailored "now live" handoff. ResetRoleWarmupForLiveRoundStart() already ran above,
		// so "capture is on now" is truthfully aligned with the actual safe->live switch.
		const ABHPlayerState* BHPS = PC->GetPlayerState<ABHPlayerState>();
		const EBHPlayerRole HandoffRole = BHPS ? BHPS->PlayerRole : EBHPlayerRole::Survivor;
		FString HandoffMessage;
		switch (HandoffRole)
		{
		case EBHPlayerRole::Hunter:
			HandoffMessage = TEXT("Hunt is live. Capture is on now - scan, listen, and cut off the exits.");
			break;
		case EBHPlayerRole::FakeHunter:
			HandoffMessage = TEXT("Hunt is live. Contribute at stations, then misdirect with hints and traps.");
			break;
		case EBHPlayerRole::Spectator:
			HandoffMessage = TEXT("Hunt is live. Cheer the class on and request a role for next round.");
			break;
		default:
			HandoffMessage = TEXT("Hunt is live! Answer questions, finish tasks, and hide from the Teacher.");
			break;
		}
		PC->ClientShowStatusMessage(HandoffMessage, 4.0f);
	}
}

void ABHGameMode::AssignRoles()
{
	if (!GameState)
	{
		return;
	}

	TArray<ABHPlayerState*> Players;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS)
		{
			Players.Add(BHPS);
		}
	}
	for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
	{
		ABHPlayerState* BotPS = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
		if (BotPS && !Players.Contains(BotPS))
		{
			Players.Add(BotPS);
			GameState->AddPlayerState(BotPS);
		}
	}

	TMap<ABHPlayerState*, EBHPlayerRole> PreviousRoles;
	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS)
		{
			PreviousRoles.Add(BHPS, BHPS->PlayerRole);
		}
	}

	if (Players.IsEmpty())
	{
		return;
	}

	const int32 DesiredHunterCount = FMath::Clamp(TargetHunterCount, 1, FMath::Max(1, Players.Num() - 1));
	TArray<ABHPlayerState*> ChosenHunters;
	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && BHPS->DesiredRole == EBHPlayerRole::Hunter && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && BHPS->DesiredRole != EBHPlayerRole::Survivor && BHPS->DesiredRole != EBHPlayerRole::FakeHunter && !ChosenHunters.Contains(BHPS) && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	for (ABHPlayerState* BHPS : Players)
	{
		if (BHPS && !ChosenHunters.Contains(BHPS) && ChosenHunters.Num() < DesiredHunterCount)
		{
			ChosenHunters.Add(BHPS);
		}
	}

	if (bBotMode)
	{
		const auto IsBotPlayerState = [this](const ABHPlayerState* Candidate)
		{
			if (!Candidate)
			{
				return false;
			}
			if (Candidate->IsABot())
			{
				return true;
			}
			for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
			{
				if (Bot && Bot->GetPlayerState<ABHPlayerState>() == Candidate)
				{
					return true;
				}
			}
			return false;
		};

		bool bChosenHumanExplicitlyRequestedHunter = false;
		for (const ABHPlayerState* Chosen : ChosenHunters)
		{
			if (Chosen && !IsBotPlayerState(Chosen) && Chosen->DesiredRole == EBHPlayerRole::Hunter)
			{
				bChosenHumanExplicitlyRequestedHunter = true;
				break;
			}
		}

		for (int32 HunterIndex = 0; HunterIndex < ChosenHunters.Num(); ++HunterIndex)
		{
			ABHPlayerState* Chosen = ChosenHunters[HunterIndex];
			if (!Chosen || IsBotPlayerState(Chosen) || bChosenHumanExplicitlyRequestedHunter)
			{
				continue;
			}

			ABHPlayerState* ReplacementBot = nullptr;
			for (const TObjectPtr<ABHBotController>& Bot : BotControllers)
			{
				ABHPlayerState* Candidate = Bot ? Bot->GetPlayerState<ABHPlayerState>() : nullptr;
				if (Candidate && !ChosenHunters.Contains(Candidate) && Candidate->DesiredRole != EBHPlayerRole::FakeHunter)
				{
					ReplacementBot = Candidate;
					break;
				}
			}

			if (ReplacementBot)
			{
				UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot mode role override: %s takes Teacher slot instead of %s"),
					*GetNameSafe(ReplacementBot),
					*GetNameSafe(Chosen));
				ChosenHunters[HunterIndex] = ReplacementBot;
			}
		}
	}

	int32 AssignedSurvivors = 0;
	for (ABHPlayerState* BHPS : Players)
	{
		if (!BHPS)
		{
			continue;
		}

		BHPS->SetReady(false);
		BHPS->SetLifeState(EBHPlayerLifeState::Alive);
		BHPS->SetHiddenInLocker(false);
		BHPS->ResetWarmupChecklist();
		if (ChosenHunters.Contains(BHPS))
		{
			BHPS->SetRole(EBHPlayerRole::Hunter);
			BHPS->SetFakeHunterEligible(false);
		}
		else if (BHPS->DesiredRole == EBHPlayerRole::FakeHunter || BHPS->bFakeHunterEligible)
		{
			BHPS->SetRole(EBHPlayerRole::FakeHunter);
			BHPS->SetFakeHunterEligible(false);
		}
		else
		{
			BHPS->SetRole(EBHPlayerRole::Survivor);
			++AssignedSurvivors;
		}
		BHPS->ClearSpectatorSupportState();
	}

	// Always guarantee at least one survivor when there are any participants. The old
	// `Players.Num() > 1` guard meant a single ready player became the lone Hunter with 0
	// survivors, so the round ended instantly as a confusing Teacher win. Falling back here
	// regardless of player count keeps a solo participant playable as the survivor.
	if (AssignedSurvivors == 0 && Players.Num() > 0)
	{
		for (int32 Index = ChosenHunters.Num() - 1; Index >= 0; --Index)
		{
			ABHPlayerState* BHPS = ChosenHunters[Index];
			if (BHPS)
			{
				BHPS->SetRole(EBHPlayerRole::Survivor);
				BHPS->SetFakeHunterEligible(false);
				++AssignedSurvivors;
				break;
			}
		}
	}

	int32 AssignedHunters = 0;
	int32 AssignedFakeHunters = 0;
	int32 AssignedBotHunters = 0;
	int32 AssignedBotSurvivors = 0;
	for (const ABHPlayerState* BHPS : Players)
	{
		if (!BHPS)
		{
			continue;
		}
		if (BHPS->PlayerRole == EBHPlayerRole::Hunter)
		{
			++AssignedHunters;
			if (BHPS->IsABot())
			{
				++AssignedBotHunters;
			}
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::Survivor)
		{
			if (BHPS->IsABot())
			{
				++AssignedBotSurvivors;
			}
		}
		else if (BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
		{
			++AssignedFakeHunters;
		}
	}
	// Symmetric hunter guarantee. With 2+ participants a round MUST have at least one Hunter, or the Hunt
	// timer runs to zero with nobody able to capture anyone — an unwinnable, confusing round. This can
	// happen when the survivor-guarantee fallback above flips the only chosen Hunter back to Survivor, or
	// when a host forces every player to Monitor/Survivor. Promote a Monitor first (keeps the survivor
	// count intact), otherwise a Survivor — but only while at least one survivor would remain.
	if (AssignedHunters == 0 && Players.Num() >= 2)
	{
		ABHPlayerState* Promote = nullptr;
		for (ABHPlayerState* BHPS : Players)
		{
			if (BHPS && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
			{
				Promote = BHPS;
				break;
			}
		}
		if (!Promote && AssignedSurvivors >= 2)
		{
			for (ABHPlayerState* BHPS : Players)
			{
				if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Survivor)
				{
					Promote = BHPS;
					break;
				}
			}
		}
		if (Promote)
		{
			const bool bWasSurvivor = Promote->PlayerRole == EBHPlayerRole::Survivor;
			Promote->SetRole(EBHPlayerRole::Hunter);
			Promote->SetFakeHunterEligible(false);
			++AssignedHunters;
			if (bWasSurvivor)
			{
				--AssignedSurvivors;
			}
			else
			{
				--AssignedFakeHunters;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BlackoutHunt roles assigned: players=%d hunters=%d survivors=%d monitors=%d botHunters=%d botSurvivors=%d"),
		Players.Num(),
		AssignedHunters,
		AssignedSurvivors,
		AssignedFakeHunters,
		AssignedBotHunters,
		AssignedBotSurvivors);

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
		if (!Controller || !Players.Contains(BHPS))
		{
			continue;
		}
		// Teaching: show each human their role intro card at assignment (warmup start), even if
		// their role is unchanged this round. Bots are ABHBotController, so the cast skips them.
		if (ABHPlayerController* IntroPC = Cast<ABHPlayerController>(Controller))
		{
			IntroPC->ClientShowRoleIntro(BHPS->PlayerRole, bRevisionMode);
		}
		const EBHPlayerRole* PreviousRole = PreviousRoles.Find(BHPS);
		const bool bRoleUnchanged = PreviousRole && *PreviousRole == BHPS->PlayerRole;
		const ABHCharacter* ExistingCharacter = Controller->GetPawn<ABHCharacter>();
		if (bRoleUnchanged
			&& ExistingCharacter
			&& ExistingCharacter->GetPlayerState<ABHPlayerState>() == BHPS
			&& BHPS->LifeState == EBHPlayerLifeState::Alive)
		{
			continue;
		}
		RestartPlayer(Controller);
	}
}

void ABHGameMode::TickRoundTimer()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
	{
		return;
	}

	if (bPracticeMode || bTestMode)
	{
		BHGS->SetRemainingTime(0);
		return;
	}

	BHGS->SetRemainingTime(BHGS->RemainingTime - 1);
	if (bRevisionMode && RevisionReviewTimeRemaining > 0)
	{
		RevisionReviewTimeRemaining = FMath::Max(0, RevisionReviewTimeRemaining - 1);
		UpdateRevisionSummary();
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Prep && BHGS->RemainingTime <= 0)
	{
		StartHuntPhase();
		return;
	}

	if (BHGS->RoundPhase == EBHRoundPhase::Hunt)
	{
		// Prop Hunt: drive the forced-taunt cadence + refresh the props-left HUD counters once per second.
		if (bPropHuntMode)
		{
			TickPropHunt();
		}

		// Re-evaluate the revision exit every tick so it opens the instant the class qualifies — e.g.
		// after a blocking straggler is captured/leaves/disconnects, or late mastery is earned. Previously
		// the exit was only re-checked on node/breaker completion, so a class that finished every node with
		// one student still short could never unlock the exit and could only lose on the Hunt timer. Silent
		// (bBroadcastBlockReason=false) to avoid spamming the "exit locked" status once per second.
		if (bRevisionMode && !BHGS->bExitUnlocked)
		{
			UpdateExitUnlockState(false);
		}

		if (CountAliveSurvivors() <= 0)
		{
			// Revision mode: hold the round open while hall monitors finish their contributions -- the Teacher
			// catching everyone must not skip the class's revision. Ticks the grace countdown once per second and
			// returns false once monitors finish, the window expires, or we're not in revision mode; then resolve
			// normally below (EndRound applies the proportional dock to any monitor still short).
			if (TickAllCaughtRevisionGrace())
			{
				// Holding for monitor revision -- do not resolve this tick.
			}
			// Evacuate-together: everyone still inside has either escaped or been caught. Survivors win only
			// if at least one student made it out to the exit; if every survivor was captured, the hunters
			// win. (The first survivor to escape no longer ends the round for the whole class.)
			else if (CountEscapedSurvivors() > 0)
			{
				EndRound(EBHRoundPhase::SurvivorsWin);
			}
			else
			{
				// Don't hand the Teacher an instant win while dropped survivor(s) are still inside their
				// reconnect grace window — a transient classroom Wi-Fi blip can knock out the last survivors
				// together. Logout already defers this, but TickRoundTimer was overriding that defer ~1s later
				// (it re-checked CountAliveSurvivors with no grace guard), defeating it. Mirror the Logout
				// guard here; the Hunt RemainingTime timeout is the no-hang backstop, so this only ever delays
				// a HunterWin (never prevents a legitimate one, and never overruns the round timer).
				const UBHGameSettings* GraceSettings = GetDefault<UBHGameSettings>();
				const float GraceSeconds = GraceSettings ? GraceSettings->ReconnectGraceSeconds : 0.0f;
				const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>();
				const int32 PendingSurvivors = (BHGI && GraceSeconds > 0.0f) ? BHGI->CountReconnectableAliveSurvivors(GraceSeconds) : 0;
				if (PendingSurvivors <= 0 || BHGS->RemainingTime <= 0)
				{
					EndRound(EBHRoundPhase::HunterWin);
				}
			}
		}
		else if (CountAliveHunters() <= 0)
		{
			// No hunter remains — every hunter disconnected, or a degenerate role assignment left none.
			// Survivors can never be caught, so resolve in their favor now instead of stalling on the Hunt
			// timer (which would otherwise wrongly credit the absent hunters on timeout). Mirrors Logout.
			EndRound(EBHRoundPhase::SurvivorsWin);
		}
		else if (BHGS->RemainingTime <= 0)
		{
			// Prop Hunt flips the timeout: surviving the clock is a PROPS win, not a Hunter win.
			EndRound(bPropHuntMode ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
		}
	}
	else if (BHGS->RoundPhase == EBHRoundPhase::FinalEscape)
	{
		// Backstop only. The escape-station manager's expiry timer (or the manager-less fallback timer) and the
		// per-survivor escape/capture events drive the normal FinalEscape resolution. But if every alive survivor
		// leaves the in-play set by a path that fires no escape/capture event (e.g. an admin role change),
		// nothing else here resolves the round until that expiry. Mirror the Hunt tie-break so it ends promptly.
		if (CountAliveSurvivors() <= 0)
		{
			EndRound(CountEscapedSurvivors() > 0 ? EBHRoundPhase::SurvivorsWin : EBHRoundPhase::HunterWin);
		}
	}
}

void ABHGameMode::EndRound(EBHRoundPhase ResultPhase)
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || BHGS->RoundPhase == EBHRoundPhase::HunterWin || BHGS->RoundPhase == EBHRoundPhase::SurvivorsWin)
	{
		return;
	}

	if (bPracticeMode || bTestMode)
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Hunt);
		BHGS->SetRemainingTime(0);
		BroadcastStatus(bTestMode ? TEXT("Test Round blocked the round end.") : TEXT("Practice Lab blocked the round end."), 2.5f);
		return;
	}

	// Revision mode: dock any hall monitor who did not finish their contributions by round-end. Centralized here so
	// EVERY end path (all-caught grace expiry, hunt timer, no-hunter resolve, etc.) docks exactly once -- EndRound is
	// idempotent (the guard above bails on a second call). Also clear the all-caught grace countdown.
	ApplyUnfinishedMonitorRevisionDock();
	BHGS->SetAllCaughtGraceRemaining(-1);

	BHGS->SetRoundPhase(ResultPhase);
	BHGS->SetRemainingTime(0);
	RecordPlaytestTelemetryMarker(TEXT("round_end"), HunterSpawn, StaticEnum<EBHRoundPhase>() ? StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)) : TEXT("Unknown"));
	if (bBotMode)
	{
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt bot round summary: result=%s %s"),
			*StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)),
			*GetBotStatusReport());
	}
	if (bRevisionMode)
	{
		RevisionReviewTimeRemaining = 60;
		UpdateRevisionSummary(TEXT("Final review: check weak topics, corrected mistakes, and exam-method gaps before the next round."));
		UE_LOG(LogTemp, Log, TEXT("BlackoutHunt revision round summary: result=%s %s"),
			*StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)),
			*GetRevisionStatusReport());
		FString ExportMessage;
		ExportRevisionReportToDisk(ResultPhase, true, ExportMessage);
		UE_LOG(LogTemp, Display, TEXT("%s"), *ExportMessage);
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ABHPlayerController* BHPC = Cast<ABHPlayerController>(It->Get());
			ABHPlayerState* BHPS = BHPC ? BHPC->GetPlayerState<ABHPlayerState>() : nullptr;
			if (BHPC && BHPS)
			{
				BHPC->ClientRecordRoundResult(BHPS->PlayerRole, BHPS->LifeState, ResultPhase);
			}
		}
	}

	GetWorldTimerManager().ClearTimer(RoundTimerHandle);
	GetWorldTimerManager().ClearTimer(DirectorTimerHandle);
	GetWorldTimerManager().ClearTimer(FinalEscapeFallbackTimerHandle);
	// A post-round travel is now committed (fires in 8s). Block discretionary travels until then so a
	// tester shortcut cannot redirect the session away from the intended post-round destination.
	bServerTravelInProgress = true;
	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::ResetRoundByTravel, 8.0f, false);
}

bool ABHGameMode::RequestServerTravel(const FString& URL, bool bAbsolute)
{
	if (bServerTravelInProgress || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt: ignored a ServerTravel to '%s' because a travel is already pending."), *URL);
		return false;
	}

	bServerTravelInProgress = true;
	GetWorld()->ServerTravel(URL, bAbsolute);
	return true;
}

void ABHGameMode::AdvanceTutorialPhase()
{
	if (!bTutorialMode || !GetWorld())
	{
		return;
	}

	// Chain Survivor -> Teacher -> Monitor (full course); after Monitor there is nothing left, so return to the
	// menu. A single selected tutorial (bTutorialChain false) never advances - it returns to the menu on its exit.
	const TCHAR* NextPhaseName = nullptr;
	if (bTutorialChain)
	{
		switch (RuntimeTutorialPhase)
		{
		case EBHTutorialPhase::Survivor: NextPhaseName = TEXT("Teacher"); break;
		case EBHTutorialPhase::Teacher:  NextPhaseName = TEXT("Monitor"); break;
		case EBHTutorialPhase::Monitor:
		default:                         NextPhaseName = nullptr; break;
		}
	}

	if (NextPhaseName)
	{
		// ServerTravel-reload the same baked Tutorial map into the next phase (mirrors the train-intermission
		// reload). The map, nav, and director re-initialise fresh; the director reads the new phase option.
		// Carry BHTutorialChain=1 so the reloaded map keeps chaining through the remaining tutorials. Pin
		// BHStageIndex=0 so IsFinalStage() is deterministically false in the tutorial regardless of the player's
		// persisted campaign stage - otherwise a player who last reached stage 2 would make the tutorial look like
		// the final stage and could arm the final-escape/train path.
		const FString TravelURL = FString::Printf(TEXT("%s?listen?BHLevel=Tutorial?BHTutorial=1?BHTutorialPhase=%s?BHTutorialChain=1?BHStageIndex=0"),
			*BHResolveLevelMapPackage(TEXT("Tutorial")), NextPhaseName);
		RequestServerTravel(TravelURL, true);
		return;
	}

	// All three tutorials finished: send the host back to the main menu.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ReturnToMainMenu();
			break;
		}
	}
}

void ABHGameMode::ResetRoundByTravel()
{
	if (GetWorld())
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		if (Settings && Settings->bUseTrainIntermissions && bRevisionMode && !bTrainIntermissionLevel && !bPracticeMode && !bTestMode)
		{
			const ABHGameState* BHGS = GetGameState<ABHGameState>();
			TravelToTrainIntermission(BHGS ? BHGS->RoundPhase : EBHRoundPhase::Lobby);
			return;
		}

		PersistPlayersForTravel();
		const FString TravelLevelName = NextRuntimeLevelName.IsEmpty() ? RuntimeLevelName : NextRuntimeLevelName;
		FString TravelURL = FString::Printf(TEXT("%s?listen?BHLevel=%s?BHFogPreset=%s"),
			*ResolveTravelMapForLevel(TravelLevelName),
			*TravelLevelName,
			*FogPresetToString(NextFogPreset));
		if (bFogPresetOverride)
		{
			TravelURL += TEXT("?BHFogOverride=1");
		}
		if (bBotMode)
		{
			TravelURL += FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHumanRole=Survivor"),
				FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
				*BotDifficultyToString(BotDifficulty));
		}
		if (bTestMode)
		{
			TravelURL += TEXT("?BHTestMode=1");
		}
		if (bRevisionMode)
		{
			TravelURL += FString::Printf(TEXT("?BHRevisionMode=1?BHRevisionTopics=%d?BHRevisionDifficultyMix=%s?BHRevisionClassThreshold=%.0f?BHRevisionIndividualThreshold=%.0f?BHHuntSeconds=%d?BHScareIntensity=%d"),
				RevisionTopicMask,
				*RevisionDifficultyMixToString(RevisionDifficultyMix),
				RevisionClassThreshold,
				RevisionIndividualThreshold,
				RevisionRoundDuration,
				RevisionScareIntensity);
			TravelURL += FString::Printf(TEXT("?BHStartDifficulty=%s?BHMinDifficulty=%s?BHMaxDifficulty=%s"),
				*FBHLessonPresetStore::QuestionDifficultyToString(RevisionStartingDifficulty),
				*FBHLessonPresetStore::QuestionDifficultyToString(RevisionMinDifficulty),
				*FBHLessonPresetStore::QuestionDifficultyToString(RevisionMaxDifficulty));
			if (!RevisionQuestionSetId.IsEmpty())
			{
				TravelURL += FString::Printf(TEXT("?BHQuestionSet=%s"), *RevisionQuestionSetId);
			}
		}
		if (RuntimeMapRoute.Num() > 0)
		{
			TravelURL += FString::Printf(TEXT("?BHMapRoute=%s"), *FBHLessonPresetStore::MapRouteToString(RuntimeMapRoute));
		}
		if (LayoutSeed > 0)
		{
			TravelURL += FString::Printf(TEXT("?BHLayoutSeed=%d"), LayoutSeed);
		}
		if (LayoutBreakerCount > 0)
		{
			TravelURL += FString::Printf(TEXT("?BHBreakerCount=%d"), LayoutBreakerCount);
		}
		if (LayoutDensity > 0)
		{
			TravelURL += FString::Printf(TEXT("?BHLayoutDensity=%d"), LayoutDensity);
		}
		if (bForceProceduralLayout)
		{
			TravelURL += TEXT("?BHForceProcedural=1");
		}
		GetWorld()->ServerTravel(TravelURL);
	}
}

int32 ABHGameMode::GetConfiguredStageIndex() const
{
	if (const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		return FMath::Clamp(BHGI->GetPersistentStageIndex(), 0, GetMaxStageIndex());
	}
	return 0;
}

void ABHGameMode::ResolveAllowedQuestionIds()
{
	RevisionAllowedQuestionIds.Reset();
	if (RevisionQuestionSetId.IsEmpty())
	{
		return;
	}
	FString Message;
	if (FBHRevisionQuestionBank::LoadQuestionSetIds(RevisionQuestionSetId, RevisionAllowedQuestionIds, Message))
	{
		UE_LOG(LogTemp, Display, TEXT("[BlackoutHunt] %s"), *Message);
	}
	else
	{
		// Missing/empty set => fall back to the full bank (no restriction) rather than starving selection.
		UE_LOG(LogTemp, Warning, TEXT("[BlackoutHunt] Question set '%s' not applied: %s"), *RevisionQuestionSetId, *Message);
		RevisionAllowedQuestionIds.Reset();
	}
}

FString ABHGameMode::GetDefaultMapForStage(int32 StageIndex) const
{
	// Host-authored route takes precedence; clamp into range so a too-high stage reuses the last leg.
	if (RuntimeMapRoute.Num() > 0)
	{
		return RuntimeMapRoute[FMath::Clamp(StageIndex, 0, RuntimeMapRoute.Num() - 1)];
	}
	if (StageIndex <= 0)
	{
		return TEXT("Facility");
	}
	if (StageIndex == 1)
	{
		return TEXT("Substation");
	}
	return TEXT("Foggrounds");
}

FString ABHGameMode::GetNextMapAfterStage(int32 StageIndex) const
{
	// "Final" is the sentinel that ends the run (no further map). With a host route, the final stage is
	// the last entry; otherwise the default sequence ends after Foggrounds (stage 2).
	if (RuntimeMapRoute.Num() > 0)
	{
		const int32 NextStage = StageIndex + 1;
		return RuntimeMapRoute.IsValidIndex(NextStage) ? RuntimeMapRoute[NextStage] : TEXT("Final");
	}
	if (StageIndex <= 0)
	{
		return TEXT("Substation");
	}
	if (StageIndex == 1)
	{
		return TEXT("Foggrounds");
	}
	return TEXT("Final");
}

FString BHResolveLevelMapPackage(const FString& LevelName)
{
	// The non-combat train intermission is never authored as a /Game map; keep it on the runtime base map.
	if (LevelName.TrimStartAndEnd().Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase))
	{
		return TEXT("/Engine/Maps/Entry");
	}
	// The dedicated tutorial level is always authored (there is no procedural Tutorial generator), so
	// resolve it to its baked .umap whenever that asset exists, independent of the global
	// bUseAuthoredLevels flag. This keeps Facility/Substation/Foggrounds on their existing shipping
	// behaviour (procedural until that flag is flipped) while the tutorial map ships on its own.
	if (LevelName.TrimStartAndEnd().Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		const FString TutorialPath = TEXT("/Game/BlackoutHunt/Maps/Tutorial");
		return FPackageName::DoesPackageExist(TutorialPath) ? TutorialPath : TEXT("/Engine/Maps/Entry");
	}
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (Settings && Settings->bUseAuthoredLevels)
	{
		// NormalizeBHLevelName always yields one of Facility/Substation/Foggrounds, so the path is canonical.
		const FString AuthoredPath = FString::Printf(TEXT("/Game/BlackoutHunt/Maps/%s"), *NormalizeBHLevelName(LevelName));
		if (FPackageName::DoesPackageExist(AuthoredPath))
		{
			return AuthoredPath;
		}
	}
	return TEXT("/Engine/Maps/Entry");
}

FString ABHGameMode::ResolveTravelMapForLevel(const FString& NormalizedLevel) const
{
	// Host "custom layout" forces the procedural generator even when authored .umaps are the shipping
	// default -- the baked maps are fixed geometry, so the seed/density knobs only matter on the runtime
	// base map. The intermission/tutorial always resolve normally.
	if (bForceProceduralLayout
		&& !NormalizedLevel.Equals(TEXT("TrainIntermission"), ESearchCase::IgnoreCase)
		&& !NormalizedLevel.Equals(TEXT("Tutorial"), ESearchCase::IgnoreCase))
	{
		return TEXT("/Engine/Maps/Entry");
	}
	return BHResolveLevelMapPackage(NormalizedLevel);
}

FString ABHGameMode::BuildTravelOptionsForLevel(const FString& LevelName, bool bIntermission, int32 StageIndex, EBHRoundPhase ResultPhase) const
{
	const FString NormalizedLevel = bIntermission ? TEXT("TrainIntermission") : NormalizeBHLevelName(LevelName);
	const FString BaseMap = ResolveTravelMapForLevel(NormalizedLevel);
	FString TravelURL = FString::Printf(TEXT("%s?listen?BHFogPreset=%s?BHStageIndex=%d"),
		*BaseMap,
		*FogPresetToString(NextFogPreset),
		FMath::Clamp(StageIndex, 0, GetMaxStageIndex()));

	if (bIntermission)
	{
		TravelURL += FString::Printf(TEXT("?BHLevel=TrainIntermission?BHTrainIntermission=1?BHIntermissionResult=%s"), *RoundPhaseToUrlValue(ResultPhase));
	}
	else
	{
		TravelURL += FString::Printf(TEXT("?BHLevel=%s"), *NormalizedLevel);
		if (StageIndex > 0 && bRevisionMode)
		{
			TravelURL += TEXT("?BHForceHunt=1");
		}
	}

	if (bFogPresetOverride)
	{
		TravelURL += TEXT("?BHFogOverride=1");
	}

	if (bRevisionMode)
	{
		const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
		const int32 StageSeconds = StageIndex <= 0
			? (Settings ? Settings->StageOneSeconds : 300)
			: (StageIndex == 1 ? (Settings ? Settings->StageTwoSeconds : 420) : (Settings ? Settings->StageThreeSeconds : 600));
		TravelURL += FString::Printf(TEXT("?BHRevisionMode=1?BHRevisionTopics=%d?BHRevisionDifficultyMix=%s?BHRevisionClassThreshold=%.0f?BHRevisionIndividualThreshold=%.0f?BHHuntSeconds=%d?BHScareIntensity=%d"),
			RevisionTopicMask,
			*RevisionDifficultyMixToString(RevisionDifficultyMix),
			RevisionClassThreshold,
			RevisionIndividualThreshold,
			FMath::Clamp(RevisionRoundDuration > 0 ? RevisionRoundDuration : StageSeconds, 60, 3600),
			RevisionScareIntensity);
		if (bBotMode)
		{
			TravelURL += FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s"),
				FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
				*BotDifficultyToString(BotDifficulty));
		}
	}
	else if (bBotMode)
	{
		TravelURL += FString::Printf(TEXT("?BHBotMode=1?BHBotCount=%d?BHBotDifficulty=%s?BHHumanRole=Survivor"),
			FMath::Clamp(TargetBotCount, 0, FMath::Max(0, MaxPlayers - 1)),
			*BotDifficultyToString(BotDifficulty));
	}
	if (bTestMode && !bIntermission)
	{
		TravelURL += TEXT("?BHTestMode=1");
	}

	// Host lobby customization carried across EVERY hop (incl. the intermission) so it does not silently
	// revert mid-route -- inter-stage options are reconstructed from member state, not the original URL.
	if (bRevisionMode)
	{
		TravelURL += FString::Printf(TEXT("?BHStartDifficulty=%s?BHMinDifficulty=%s?BHMaxDifficulty=%s"),
			*FBHLessonPresetStore::QuestionDifficultyToString(RevisionStartingDifficulty),
			*FBHLessonPresetStore::QuestionDifficultyToString(RevisionMinDifficulty),
			*FBHLessonPresetStore::QuestionDifficultyToString(RevisionMaxDifficulty));
		if (!RevisionQuestionSetId.IsEmpty())
		{
			TravelURL += FString::Printf(TEXT("?BHQuestionSet=%s"), *RevisionQuestionSetId);
		}
	}
	if (RuntimeMapRoute.Num() > 0)
	{
		TravelURL += FString::Printf(TEXT("?BHMapRoute=%s"), *FBHLessonPresetStore::MapRouteToString(RuntimeMapRoute));
	}
	if (LayoutSeed > 0)
	{
		TravelURL += FString::Printf(TEXT("?BHLayoutSeed=%d"), LayoutSeed);
	}
	if (LayoutBreakerCount > 0)
	{
		TravelURL += FString::Printf(TEXT("?BHBreakerCount=%d"), LayoutBreakerCount);
	}
	if (LayoutDensity > 0)
	{
		TravelURL += FString::Printf(TEXT("?BHLayoutDensity=%d"), LayoutDensity);
	}
	if (bForceProceduralLayout)
	{
		TravelURL += TEXT("?BHForceProcedural=1");
	}

	// Non-seamless ServerTravel rebuilds AGameSession, whose MaxPlayers resets to the engine default (16)
	// unless the travel URL carries ?MaxPlayers=. Without this a class larger than 16 is silently bounced
	// at login on every level hop and intermission. Mirrors the initial listen-host options.
	UBHGameSettings::AppendMaxPlayersOption(TravelURL);

	return TravelURL;
}

bool ABHGameMode::AreAllReady() const
{
	if (!GameState || GameState->PlayerArray.Num() < MinPlayers)
	{
		return false;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (!BHPS || !BHPS->bReady)
		{
			return false;
		}
	}

	return true;
}

int32 ABHGameMode::CountAliveSurvivors() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->IsAliveSurvivor())
		{
			++Count;
		}
	}

	return Count;
}

int32 ABHGameMode::CountAliveHunters() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->IsAliveHunter())
		{
			++Count;
		}
	}

	return Count;
}

int32 ABHGameMode::CountEscapedSurvivors() const
{
	int32 Count = 0;
	if (!GameState)
	{
		return Count;
	}

	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		const ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (BHPS && BHPS->PlayerRole == EBHPlayerRole::Survivor && BHPS->LifeState == EBHPlayerLifeState::Escaped)
		{
			++Count;
		}
	}

	return Count;
}

FTransform ABHGameMode::GetSpawnTransformFor(AController* Controller) const
{
	const ABHPlayerState* BHPS = Controller ? Controller->GetPlayerState<ABHPlayerState>() : nullptr;
	int32 PlayerIndex = 0;
	if (GameState && BHPS)
	{
		PlayerIndex = GameState->PlayerArray.IndexOfByKey(BHPS);
	}
	// IndexOfByKey returns INDEX_NONE (-1) in a brief login/travel window before the player is in the
	// array; a negative index yields a wrong spawn offset (and a negative modulo below), so floor to 0.
	PlayerIndex = FMath::Max(0, PlayerIndex);

	if (BHPS && (BHPS->PlayerRole == EBHPlayerRole::Hunter || BHPS->PlayerRole == EBHPlayerRole::FakeHunter))
	{
		const float OffsetSign = PlayerIndex % 2 == 0 ? 1.0f : -1.0f;
		const FVector SpawnOffset = BHPS->PlayerRole == EBHPlayerRole::FakeHunter
			? FVector(180.0f, OffsetSign * (160.0f + 70.0f * PlayerIndex), 0.0f)
			: FVector(0.0f, OffsetSign * 80.0f * PlayerIndex, 0.0f);
		return FTransform(FRotator(0.0f, 0.0f, 0.0f), BHResolveSpawnLocation(GetWorld(), HunterSpawn + SpawnOffset, PlayerIndex));
	}

	const FVector SpawnLocation = SurvivorSpawns.IsValidIndex(PlayerIndex % FMath::Max(1, SurvivorSpawns.Num()))
		? SurvivorSpawns[PlayerIndex % SurvivorSpawns.Num()]
		: FVector(1000.0f, 0.0f, 120.0f);

	return FTransform(FRotator(0.0f, 180.0f, 0.0f), BHResolveSpawnLocation(GetWorld(), SpawnLocation, PlayerIndex));
}

void ABHGameMode::RecordTrainCabinCenters(float TubeMinX, float TubeMaxX, float FloorZ)
{
	// Cars sit at a fixed 1500cm pitch, centred in the tube (car i centre = TubeMinX + 750 + i*1500), matching
	// BuildTrainLobbyLevel / BuildTrainIntermissionLevel. Deriving the centres from the bounds covers EVERY car in
	// either build -- including the intermission's two end lounge cars, which are dressed outside the main car loop
	// -- without duplicating those per-car dressing loops or their geometry constants.
	TrainCabinCenters.Reset();
	const int32 NumCars = FMath::Max(1, FMath::RoundToInt((TubeMaxX - TubeMinX) / 1500.0f));
	for (int32 CarIndex = 0; CarIndex < NumCars; ++CarIndex)
	{
		TrainCabinCenters.Add(FVector(TubeMinX + 750.0f + static_cast<float>(CarIndex) * 1500.0f, 0.0f, FloorZ));
	}
}

int32 ABHGameMode::NearestCabinCenterIndex(const TArray<FVector>& Centers, float X)
{
	int32 BestIndex = INDEX_NONE;
	float BestDistX = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Centers.Num(); ++Index)
	{
		const float DistX = FMath::Abs(Centers[Index].X - X);
		if (DistX < BestDistX)
		{
			BestDistX = DistX;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

bool ABHGameMode::ResetPlayerToTrainInterior(AController* Controller)
{
	if (!HasAuthority())
	{
		return false;
	}
	// Only on the train (lobby / intermission). Never on a live hunt level -- teleporting there would be an
	// escape exploit.
	if (!bLobbyLevel && !bTrainIntermissionLevel)
	{
		return false;
	}
	ABHCharacter* Character = Controller ? Cast<ABHCharacter>(Controller->GetPawn()) : nullptr;
	if (!IsValid(Character))
	{
		return false;
	}

	// Drop the player into the centre of the carriage they're CURRENTLY in. Cars run along the train's length
	// (X) at a fixed pitch, so the nearest recorded cabin centre by X is the car they're standing in (or standing
	// above, on the roof). This is the O-key unstick for players wedged in interior geometry or out on the roof.
	const int32 CabinIndex = NearestCabinCenterIndex(TrainCabinCenters, Character->GetActorLocation().X);
	FVector InteriorLocation;
	if (TrainCabinCenters.IsValidIndex(CabinIndex))
	{
		// Resolve out of anything occupying the dead centre (a game car's feature table, a wall, another player)
		// while staying in this car: BHResolveSpawnLocation probes outward on the Pawn channel, which blocking
		// train furniture also occupies, so it lands a clear capsule near the cabin centre. Ignore this pawn so
		// its own current capsule isn't counted as a blocker (else an already-centred press would be nudged off).
		const ABHPlayerState* BHPS = Controller->GetPlayerState<ABHPlayerState>();
		const int32 PlayerIndex = (GameState && BHPS) ? FMath::Max(0, GameState->PlayerArray.IndexOfByKey(BHPS)) : 0;
		InteriorLocation = BHResolveSpawnLocation(GetWorld(), TrainCabinCenters[CabinIndex], PlayerIndex, Character);
	}
	else
	{
		// No cabin centres recorded (defensive): fall back to the role-aware spawn so O still does something.
		InteriorLocation = GetSpawnTransformFor(Controller).GetLocation();
	}

	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	Character->SetActorLocation(InteriorLocation, false, nullptr, ETeleportType::TeleportPhysics);
	if (ABHPlayerController* PC = Cast<ABHPlayerController>(Controller))
	{
		PC->ClientShowStatusMessage(TEXT("Reset to the centre of the carriage."), 2.5f);
	}
	return true;
}
