#include "BHGameMode.h"
#include "BHAlarmTrap.h"
#include "BHAtmosphereDirector.h"
#include "BHAmbientEmitter.h"
#include "BHBatteryPickup.h"
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
#include "BHJumpscareVariantLibrary.h"
#include "BHLessonPreset.h"
#include "BHLocker.h"
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
#include "BHTrainIntermissionManager.h"
#include "BHTrainTunnelMotionActor.h"
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
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/Navigation/NavigationDirtyArea.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
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
		FLinearColor(0.76f, 0.76f, 0.80f, 1.0f)
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
	return Variant.VariantId.ToString().Equals(TEXT("SCP096"), ESearchCase::IgnoreCase);
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

constexpr float BHJumpscareMinViewDot = 0.50f;
constexpr float BHJumpscareSpawnCapsuleRadius = 62.0f;
constexpr float BHJumpscareSpawnCapsuleHalfHeight = 142.0f;

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

FVector BHJumpscareCloseFaceFocusLocation(const FVector& ViewLocation, const FVector& ViewForward, const FBHJumpscareVariant& Variant)
{
	FVector Forward = ViewForward;
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const FVector CloseOffset = Variant.CloseVisualOffset;
	const float HeightFactor = CloseOffset.Z < -80.0f ? 0.98f : 0.50f;
	const float FaceLift = FMath::Clamp(CloseOffset.Z + FocusHeight * HeightFactor, 22.0f, 54.0f);
	const float FaceDistance = FMath::Max(88.0f, CloseOffset.X + FMath::Clamp(FocusHeight * 0.08f, 8.0f, 18.0f));
	return ViewLocation + Forward * FaceDistance + FVector::UpVector * FaceLift;
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

FVector BHResolveSpawnLocation(const UWorld* World, const FVector& DesiredLocation, int32 PlayerIndex)
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
	for (int32 Attempt = 0; Attempt < UE_ARRAY_COUNT(Rings); ++Attempt)
	{
		const FVector2D Offset2D = Rings[(StartOffset + Attempt) % UE_ARRAY_COUNT(Rings)];
		const FVector Candidate = DesiredLocation + FVector(Offset2D.X, Offset2D.Y, 0.0f);
		if (!World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule))
		{
			return Candidate;
		}
	}

	return DesiredLocation + FVector(static_cast<float>((PlayerIndex % 5) - 2) * 180.0f, static_cast<float>((PlayerIndex / 5) % 5) * 180.0f, 0.0f);
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

EBHQuestionDifficulty AdaptiveDifficultyForStats(const FBHPlayerRevisionStats& Stats, bool bLastAnswerCorrect)
{
	if (Stats.Attempts <= 0)
	{
		return EBHQuestionDifficulty::Easy;
	}
	if (!bLastAnswerCorrect && Stats.MasteryPercent < 55.0f)
	{
		return EBHQuestionDifficulty::Easy;
	}
	if (Stats.MasteryPercent >= 78.0f && bLastAnswerCorrect)
	{
		return EBHQuestionDifficulty::Hard;
	}
	if (Stats.MasteryPercent >= 45.0f)
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

	return Lines[FMath::Abs(Salt) % LineCount];
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
	MaxPlayers = FMath::Max(MinPlayers, Settings->MaxPlayers);
	PrepSeconds = FMath::Max(0, Settings->PrepSeconds);
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
	RuntimeStageIndex = 0;
	PendingIntermissionResult = EBHRoundPhase::Lobby;
	RuntimeNavBounds = nullptr;
	TrainIntermissionManager = nullptr;
	StaticBlockField = nullptr;
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
		BHGS->SetBotOptions(bBotMode, TargetBotCount, BotDifficulty);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
		GetWorldTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ABHGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const ABHGameState* BHGS = GetGameState<ABHGameState>();
	const bool bCanJoinRound = bPracticeMode || bTestMode || !BHGS || BHGS->RoundPhase == EBHRoundPhase::Lobby;

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
		BHPS->SetReady(bPracticeMode || bTestMode);
		BHPS->SetDesiredRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode || bLateJoinSpectator ? EBHPlayerRole::Survivor : RequestedRole));
		BHPS->SetRole(bTestMode ? EBHPlayerRole::Tester : (bPracticeMode ? EBHPlayerRole::Survivor : (bCanJoinRound ? EBHPlayerRole::Unassigned : EBHPlayerRole::Spectator)));
		BHPS->SetLifeState(bCanJoinRound ? EBHPlayerLifeState::Alive : EBHPlayerLifeState::Captured);
		BHPS->SetHiddenInLocker(false);
		BHPS->SetAvatarIndex(PlayerIndex);
		BHPS->SetAvatarColor(AvatarColorForIndex(PlayerIndex));
		BHPS->SetMapVote(TEXT(""));
		BHPS->ClearFogPresetVote();
		BHPS->ClearSpectatorSupportState();
		if (bRevisionMode && !bTrainIntermissionLevel)
		{
			BHPS->ResetRevisionStats();
		}
		RestorePlayersAfterTravel(NewPlayer);
	}

	if (bBotMode)
	{
		TrimBotRosterToCapacity();
	}

	if (GameState && GameState->PlayerArray.Num() > MaxPlayers)
	{
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

	if (!bCanJoinRound)
	{
		if (ABHPlayerController* BHPC = Cast<ABHPlayerController>(NewPlayer))
		{
			BHPC->ClientShowStatusMessage(TEXT("Round already started. Spectator support is available: H encourages the team; T/Y/U request next-round roles."), 8.0f);
		}
	}
}

void ABHGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		if (!bPracticeMode && !bTestMode && BHGS->RoundPhase == EBHRoundPhase::Hunt && CountAliveSurvivors() <= 0)
		{
			EndRound(EBHRoundPhase::HunterWin);
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
		StartPrepPhase();
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
	if (bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge)
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
		RecordPlaytestTelemetryMarker(TEXT("capture"), CaptureLocation, bTestMode ? TEXT("test") : TEXT("practice"), CapturingHunterPS, SurvivorPS);
		Survivor->MarkCaptured();
		ApplyPresenceSpike(CaptureLocation, 70.0f, bTestMode ? TEXT("Test capture registered.") : TEXT("Practice capture registered."));
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
		BroadcastStatus(bTestMode ? TEXT("Test capture registered. The round stays open.") : TEXT("Practice capture registered. The round stays open."), 3.5f);
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
			EndRound(EBHRoundPhase::HunterWin);
		}
		return;
	}

	const bool bCanReturnAsFakeHunter = BHGS && BHGS->RoundPhase == EBHRoundPhase::Hunt && SurvivorPS && SurvivorPS->IsAliveSurvivor() && SurvivorController;
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
		CaptureImpactCue.DurationSeconds = 0.48f;
		CaptureImpactCue.ShakeIntensity = 0.24f;
		CaptureImpactCue.CameraJitterDuration = 0.28f;
		CaptureImpactCue.CameraJitterFrequency = 42.0f;
		CaptureImpactCue.FlashIntensity = 0.24f;
		CaptureImpactCue.FlashColor = FLinearColor(1.0f, 0.70f, 0.30f, 1.0f);
		SurvivorPC->ClientPlayHorrorCue(CaptureImpactCue);
	}
	Survivor->MarkCaptured();
	ReportBotStimulus(EBHBotStimulusType::Capture, CaptureLocation, Survivor, Survivor, TEXT("survivor captured"), 1.6f);
	ApplyPresenceSpike(CaptureLocation, 92.0f, TEXT("The Teacher found someone."));
	if (SurvivorPS && bCanReturnAsFakeHunter)
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
		EndRound(EBHRoundPhase::HunterWin);
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

	ReportBotStimulus(EBHBotStimulusType::Escape, Survivor->GetActorLocation(), Survivor, Survivor, TEXT("survivor escaped"), 1.6f);
	RecordPlaytestTelemetryMarker(TEXT("escape"), Survivor->GetActorLocation(), bFinalEscapeInProgress ? TEXT("final_escape") : TEXT("standard"), Survivor->GetPlayerState<ABHPlayerState>());
	Survivor->MarkEscaped();
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		if (BHGS->RoundPhase == EBHRoundPhase::FinalEscape || BHGS->FinalEscapeState == EBHFinalEscapeState::EscapeActive)
		{
			BroadcastStatus(TEXT("A student boarded the evacuation train."), 3.0f);
			if (CountAliveSurvivors() <= 0)
			{
				BHGS->SetFinalEscapeState(EBHFinalEscapeState::Departed, 0.0f, 0.0f, 0.0f);
				EndRound(EBHRoundPhase::SurvivorsWin);
			}
			return;
		}
	}
	EndRound(EBHRoundPhase::SurvivorsWin);
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
	EBHBlockMaterial FallbackMaterial)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	UStaticMesh* MeshAsset = MeshAssetPath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshAssetPath);
	if (!MeshAsset)
	{
		SpawnBlock(Location, FallbackScale, FallbackTint, Rotation, bCollides, FallbackMaterial);
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABHRuntimeMeshPropActor* Prop = GetWorld()->SpawnActor<ABHRuntimeMeshPropActor>(Location, Rotation, SpawnParams);
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
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get());
		ABHPlayerState* BHPS = PC ? PC->GetPlayerState<ABHPlayerState>() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !BHPS || !Pawn || BHPS->LifeState != EBHPlayerLifeState::Alive)
		{
			continue;
		}

		const FVector Delta = Location - Pawn->GetActorLocation();
		const float DistanceMeters = Delta.Size2D() / 100.0f;
		if (BHPS->IsAliveHunter())
		{
			PC->ClientShowStatusMessage(FString::Printf(TEXT("%s marker: unverified corridor %s, %.0fm away."), *SenderLabel, *CompassFromDelta(Delta), DistanceMeters), 4.0f);
			++TeachersNotified;
		}
		else if (BHPS->IsAliveSurvivor() && DistanceMeters <= 22.0f)
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
	if (AtmosphereDirector)
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

FBHJumpscareVariant ABHGameMode::ChooseJumpscareVariant(EBHScareEventType EventType) const
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

	if (Candidates.IsEmpty() || TotalWeight <= 0.0f)
	{
		FBHJumpscareVariant Fallback;
		Fallback.VariantId = TEXT("SCP096");
		Fallback.DisplayName = TEXT("SCP-096 Prototype");
		Fallback.Weight = 1.0f;
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
	Report += TEXT("Configured jumpscare variants:\n");

	if (Variants.IsEmpty())
	{
		Report += TEXT("No configured variants. Runtime scares will use the SCP096 fallback proxy.");
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
		Cue.bUpperBodyCloseVisual = bCloseRangeFocus;
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
	const FVector FocusLocation = BHJumpscareCloseFaceFocusLocation(ViewLocation, ViewForward, Variant);
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
	const FString Label = FString::Printf(TEXT("%d/%d %s"),
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
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(62.0f, 142.0f);
		return !World->OverlapBlockingTestByChannel(Location + FVector(0.0f, 0.0f, 142.0f), FQuat::Identity, ECC_WorldStatic, Shape, TraceParams);
	};

	const int32 FirstSide = FMath::RandBool() ? 1 : -1;
	const int32 SideOptions[] = { FirstSide, -FirstSide };
	const float ForwardDistances[] = { 1550.0f, 1850.0f, 2200.0f, 2550.0f };
	const float SideScales[] = { 1.0f, 0.72f, 0.52f };
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
				if (!HasVisibleFocus(PeekEnd) || !HasVisibleFocus(CrossStart) || !HasVisibleFocus(CrossEnd) || !HasVisibleFocus(ChargeStart))
				{
					continue;
				}
				if (!HasClearSweep(PeekStart, PeekEnd, 46.0f) || !HasClearSweep(CrossStart, CrossEnd, 54.0f) || !HasClearSweep(ChargeBendStart, ChargeStart, 62.0f) || !HasClearSweep(ChargeStart, ChargeEnd, 70.0f))
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
	const float TotalLockSeconds = 4.95f;
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

	SendStageCue(RequestingController, PeekEnd + FVector(0.0f, 0.0f, FocusHeight), TEXT("Something sees you."), Variant, StageColor, 1.35f, TotalLockSeconds, 0.42f, 0.18f, 0.55f, 0.0f);
	BHSpawnScriptedJumpscareStage(GetWorld(), RequestingController, Variant, TArray<FVector>{ PeekStart, PeekEnd }, 420.0f, 1.25f, Target, true, false);

	FTimerDelegate CrossDelegate;
	CrossDelegate.BindLambda([WeakController, Variant, StageColor, CrossStart, CrossEnd, FocusHeight, TotalLockSeconds]()
	{
		ABHPlayerController* TargetPC = WeakController.Get();
		if (!TargetPC)
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = (CrossStart + CrossEnd) * 0.5f + FVector(0.0f, 0.0f, FocusHeight);
		Cue.Message = TEXT("");
		Cue.DurationSeconds = 0.95f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - 1.05f);
		Cue.ShakeIntensity = 0.68f;
		Cue.CameraJitterDuration = 0.95f;
		Cue.CameraJitterFrequency = 42.0f;
		Cue.FlashIntensity = 0.28f;
		Cue.FlashColor = StageColor;
		Cue.AudioAsset = Variant.LaunchSound;
		Cue.AudioVolume = 1.2f;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		TargetPC->ClientSnapViewToFlatFocus(Cue.FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);

		BHSpawnScriptedJumpscareStage(TargetPC->GetWorld(), TargetPC, Variant, TArray<FVector>{ CrossStart, CrossEnd }, 5600.0f, 1.05f);
	});
	FTimerHandle CrossTimerHandle;
	GetWorldTimerManager().SetTimer(CrossTimerHandle, CrossDelegate, 1.05f, false);

	FTimerDelegate ChargeDelegate;
	FTimerDelegate BendDelegate;
	BendDelegate.BindLambda([WeakController, Variant, StageColor, ChargeBendStart, ChargeStart, FocusHeight, TotalLockSeconds]()
	{
		ABHPlayerController* TargetPC = WeakController.Get();
		if (!TargetPC)
		{
			return;
		}

		FBHClientHorrorCue Cue;
		Cue.EventType = EBHScareEventType::MonsterCharge;
		Cue.FocusLocation = ChargeStart + FVector(0.0f, 0.0f, FocusHeight);
		Cue.Message = TEXT("It moved.");
		Cue.DurationSeconds = 0.80f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - 2.05f);
		Cue.ShakeIntensity = 0.58f;
		Cue.CameraJitterDuration = 0.70f;
		Cue.CameraJitterFrequency = 44.0f;
		Cue.FlashIntensity = 0.20f;
		Cue.FlashColor = StageColor;
		Cue.VariantId = Variant.VariantId;
		Cue.bSnapToFocus = true;
		Cue.bLockInput = true;
		TargetPC->ClientSnapViewToFlatFocus(Cue.FocusLocation);
		TargetPC->ClientPlayHorrorCue(Cue);
		TargetPC->ClientShowStatusMessage(Cue.Message, 1.35f);

		BHSpawnScriptedJumpscareStage(TargetPC->GetWorld(), TargetPC, Variant, TArray<FVector>{ ChargeBendStart, ChargeStart }, 3300.0f, 0.78f, nullptr, false, false);
	});
	FTimerHandle BendTimerHandle;
	GetWorldTimerManager().SetTimer(BendTimerHandle, BendDelegate, 2.05f, false);

	ChargeDelegate.BindLambda([WeakGameMode, WeakController, WeakTarget, Variant, StageColor, ChargeStart, FocusHeight, TotalLockSeconds]()
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
		Cue.FocusLocation = ChargeStart + FVector(0.0f, 0.0f, FocusHeight);
		Cue.Message = TEXT("It is coming.");
		Cue.DurationSeconds = 2.45f;
		Cue.LockSeconds = FMath::Max(0.0f, TotalLockSeconds - 2.62f);
		Cue.ShakeIntensity = 0.88f;
		Cue.CameraJitterDuration = 1.85f;
		Cue.CameraJitterFrequency = 48.0f;
		Cue.FlashIntensity = 0.42f;
		Cue.FlashColor = StageColor;
		Cue.AudioAsset = Variant.LaunchSound;
		Cue.AudioVolume = 1.25f;
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
			Monster->Configure(TargetCharacter, 9800.0f, 6.6f, 0.10f);
			Monster->ConfigureVariant(Variant);
		}
	});
	FTimerHandle ChargeTimerHandle;
	GetWorldTimerManager().SetTimer(ChargeTimerHandle, ChargeDelegate, 2.62f, false);

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
	if (StudentCount >= 10)
	{
		return StageIndex <= 0 ? 2 : 3;
	}
	if (StudentCount >= 6)
	{
		return 3;
	}
	return 2;
}

int32 ABHGameMode::ResolveRevisionNodeTargetFor(int32 StudentCount, int32 StageIndex, int32 StationCount)
{
	const int32 StageNodeFloor = StageIndex <= 0 ? 8 : (StageIndex == 1 ? 10 : 12);
	const int32 StudentNodeTarget = StudentCount >= 10 ? 18 : (StudentCount >= 6 ? 12 : 8);
	const int32 RevisionNodeTarget = FMath::Max(StageNodeFloor, StudentNodeTarget);
	return FMath::Clamp(RevisionNodeTarget, 0, FMath::Max(0, StationCount));
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
	if (StudentCount >= 10)
	{
		return 5;
	}
	return 4;
}

int32 ABHGameMode::GetRevisionMinimumContributionTarget() const
{
	return FMath::Clamp(GetRevisionQuestionTargetPerNode() - 1 + FMath::Clamp(RuntimeStageIndex, 0, 2), 1, 4);
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
		OutDifficulty = EBHQuestionDifficulty::Easy;
		OutReason = TEXT("No student profile yet; using the current class weak topic.");
		return;
	}

	const FBHPlayerRevisionStats& Stats = PlayerState->RevisionStats;
	OutTopic = WeakestTopicForStats(Stats, RevisionTopicMask);
	OutDifficulty = AdaptiveDifficultyForStats(Stats, bLastAnswerCorrect);
	OutReason = FString::Printf(TEXT("Mastery %.0f%% after %d attempt(s); next question targets %s at %s difficulty."),
		Stats.MasteryPercent,
		Stats.Attempts,
		*FBHRevisionQuestionBank::TopicToString(OutTopic),
		*FBHRevisionQuestionBank::DifficultyToString(OutDifficulty));
}

void ABHGameMode::RecordRevisionAnswer(ABHCharacter* Character, const FBHRevisionQuestion& Question, bool bCorrect, bool bCorrection, const FString& SelectedAnswer, const FString& TeamSummary)
{
	ABHPlayerState* BHPS = Character ? Character->GetPlayerState<ABHPlayerState>() : nullptr;
	if (!bRevisionMode || !IsRevisionParticipantState(BHPS))
	{
		return;
	}

	FBHPlayerRevisionStats Stats = BHPS->RevisionStats;
	const int32 PreviousContributionCount = Stats.ContributionCount;
	Stats.Attempts = FMath::Max(0, Stats.Attempts + 1);
	int32 PointsEarned = 0;
	if (bCorrect)
	{
		Stats.CorrectAnswers = FMath::Max(0, Stats.CorrectAnswers + 1);
		Stats.ContributionCount = FMath::Max(0, Stats.ContributionCount + 1);
		PointsEarned = FBHPowerupLibrary::QuestionPointValue(Question.Difficulty, false);
		BHPS->AddQuestionPoints(PointsEarned);
		if (bCorrection)
		{
			Stats.CorrectionsCompleted = FMath::Max(0, Stats.CorrectionsCompleted + 1);
		}

		const float TopicGain = 24.0f * FMath::Max(0.75f, Question.MasteryWeight);
		switch (Question.Topic)
		{
		case EBHPhysicsTopic::ForcesAndMotion:
			Stats.ForcesMastery = FMath::Clamp(Stats.ForcesMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Electricity:
			Stats.ElectricityMastery = FMath::Clamp(Stats.ElectricityMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Waves:
			Stats.WavesMastery = FMath::Clamp(Stats.WavesMastery + TopicGain, 0.0f, 100.0f);
			break;
		case EBHPhysicsTopic::Energy:
			Stats.EnergyMastery = FMath::Clamp(Stats.EnergyMastery + TopicGain, 0.0f, 100.0f);
			break;
		}
	}
	else
	{
		Stats.HintCount = FMath::Max(0, Stats.HintCount + 1);
	}

	const int32 MasteredAnswers = Stats.CorrectAnswers + Stats.CorrectionsCompleted;
	Stats.MasteryPercent = Stats.Attempts > 0 ? FMath::Clamp(100.0f * static_cast<float>(MasteredAnswers) / static_cast<float>(Stats.Attempts), 0.0f, 100.0f) : 0.0f;
	BHPS->RevisionStats = Stats;

	if (bCorrect && BHPS->PlayerRole == EBHPlayerRole::FakeHunter)
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
}

bool ABHGameMode::BuildRevisionAnswerTeam(ABHObjectiveStation* Station, ABHCharacter* RequestingCharacter, TSet<int32>& OutPlayerIds, FString& OutSummary) const
{
	OutPlayerIds.Reset();
	OutSummary = TEXT("");
	if (!bRevisionMode || !GameState)
	{
		return false;
	}

	const EBHPhysicsTopic Topic = Station ? FBHRevisionQuestionBank::TopicForStationType(Station->GetStationType()) : EBHPhysicsTopic::ForcesAndMotion;
	const auto TopicMastery = [Topic](const ABHPlayerState* PS)
	{
		if (!PS)
		{
			return 0.0f;
		}
		switch (Topic)
		{
		case EBHPhysicsTopic::ForcesAndMotion:
			return PS->RevisionStats.ForcesMastery;
		case EBHPhysicsTopic::Electricity:
			return PS->RevisionStats.ElectricityMastery;
		case EBHPhysicsTopic::Waves:
			return PS->RevisionStats.WavesMastery;
		case EBHPhysicsTopic::Energy:
			return PS->RevisionStats.EnergyMastery;
		default:
			return 0.0f;
		}
	};

	TArray<ABHPlayerState*> Students;
	for (APlayerState* RawPS : GameState->PlayerArray)
	{
		ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
		if (IsRevisionParticipantState(BHPS) && !BHPS->IsABot())
		{
			Students.Add(BHPS);
		}
	}
	if (Students.IsEmpty())
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			ABHPlayerState* BHPS = Cast<ABHPlayerState>(RawPS);
			if (IsRevisionParticipantState(BHPS))
			{
				Students.Add(BHPS);
			}
		}
	}
	if (Students.IsEmpty())
	{
		return false;
	}

	Students.Sort([&TopicMastery](const ABHPlayerState& A, const ABHPlayerState& B)
	{
		if (A.RevisionStats.ContributionCount != B.RevisionStats.ContributionCount)
		{
			return A.RevisionStats.ContributionCount < B.RevisionStats.ContributionCount;
		}
		if (!FMath::IsNearlyEqual(TopicMastery(&A), TopicMastery(&B)))
		{
			return TopicMastery(&A) < TopicMastery(&B);
		}
		return A.GetPlayerId() < B.GetPlayerId();
	});

	const ABHPlayerState* RequestingPS = RequestingCharacter ? RequestingCharacter->GetPlayerState<ABHPlayerState>() : nullptr;
	const int32 TargetTeamSize = FMath::Clamp(GetRevisionAnswerTeamTargetSize(), 1, Students.Num());
	if (RequestingPS)
	{
		OutPlayerIds.Add(RequestingPS->GetPlayerId());
	}
	for (ABHPlayerState* Student : Students)
	{
		if (Student && OutPlayerIds.Num() < TargetTeamSize)
		{
			OutPlayerIds.Add(Student->GetPlayerId());
		}
	}

	TArray<FString> Names;
	for (ABHPlayerState* Student : Students)
	{
		if (Student && OutPlayerIds.Contains(Student->GetPlayerId()))
		{
			Names.Add(Student->GetPlayerName());
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
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
	BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
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
	const bool bTesterContext = bTestMode
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
	GetWorld()->ServerTravel(TravelURL, true);
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

	FString TravelURL = FString::Printf(TEXT("/Engine/Maps/Entry?listen?BHLevel=Foggrounds?BHFogPreset=%s?BHStageIndex=2?BHTestMode=1"),
		*FogPresetToString(NextFogPreset));
	if (bFogPresetOverride)
	{
		TravelURL += TEXT("?BHFogOverride=1");
	}
	GetWorld()->ServerTravel(TravelURL, true);
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
	GetWorld()->ServerTravel(TravelURL, true);
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
	};

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
		++StudentRow.TopicAttempts[TopicIndex];
		if (Attempt.bCorrect)
		{
			++StudentRow.Correct;
			++StudentRow.TopicCorrect[TopicIndex];
			StudentRow.CorrectQuestionIds.Add(Attempt.QuestionId.IsEmpty() ? Attempt.QuestionText.Left(48) : Attempt.QuestionId);
		}
		else
		{
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
	const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString LevelToken = SanitizeReportToken(RuntimeLevelName);
	const FString Prefix = FString::Printf(TEXT("BlackoutHunt_%s_stage%d_%s"), *LevelToken, RuntimeStageIndex, *Timestamp);
	const FString SummaryPath = FPaths::Combine(ReportDir, Prefix + TEXT("_summary.csv"));
	const FString StudentPath = FPaths::Combine(ReportDir, Prefix + TEXT("_students.csv"));
	const FString AttemptsPath = FPaths::Combine(ReportDir, Prefix + TEXT("_attempts.csv"));
	const FString QuestionsPath = FPaths::Combine(ReportDir, Prefix + TEXT("_questions.csv"));

	const FBHClassRevisionSummary ClassSummary = ComputeRevisionSummary();
	const FString ResultText = StaticEnum<EBHRoundPhase>() ? StaticEnum<EBHRoundPhase>()->GetNameStringByValue(static_cast<int64>(ResultPhase)) : TEXT("Unknown");
	TArray<FString> SummaryLines;
	SummaryLines.Add(MakeCsvRow({TEXT("Metric"), TEXT("Value")}));
	SummaryLines.Add(MakeCsvRow({TEXT("ExportedUtc"), FDateTime::UtcNow().ToIso8601()}));
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
		const EBHQuestionDifficulty RecommendedDifficulty = AdaptiveDifficultyForStats(EffectiveStats, Row->Attempts <= 0 || Row->Correct >= Row->Attempts);
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
		TEXT("MissedBy"), TEXT("Explanation")
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
			First.Explanation
		}));
	}

	const bool bSavedSummary = FFileHelper::SaveStringToFile(FString::Join(SummaryLines, LINE_TERMINATOR), *SummaryPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedStudents = FFileHelper::SaveStringToFile(FString::Join(StudentLines, LINE_TERMINATOR), *StudentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedAttempts = FFileHelper::SaveStringToFile(FString::Join(AttemptLines, LINE_TERMINATOR), *AttemptsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	const bool bSavedQuestions = FFileHelper::SaveStringToFile(FString::Join(QuestionLines, LINE_TERMINATOR), *QuestionsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSavedSummary || !bSavedStudents || !bSavedAttempts || !bSavedQuestions)
	{
		OutMessage = FString::Printf(TEXT("Classroom report export failed. Check write access to %s."), *ReportDir);
		return false;
	}

	bRevisionReportExported = true;
	OutMessage = FString::Printf(TEXT("Classroom report exported to %s"), *ReportDir);
	UE_LOG(LogTemp, Display, TEXT("%s (%s, %s, %s, %s)"), *OutMessage, *SummaryPath, *StudentPath, *AttemptsPath, *QuestionsPath);
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

	const FVector TargetLocation = Target->GetActorLocation();
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	FVector Forward = RequestingController ? RequestingController->GetControlRotation().Vector() : Target->GetActorForwardVector();
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
	const float Distances[] = { 4300.0f, 3700.0f, 3100.0f, 2500.0f, 1900.0f };
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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = RequestingController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge);
	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	FBHResolvedJumpscareSpawn ResolvedSpawn;
	if (!ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 900.0f, 4600.0f, 62.0f, ResolvedSpawn))
	{
		if (RequestingController)
		{
			RequestingController->ClientShowStatusMessage(TEXT("No camera-visible monster route found; using close visual."), 2.75f);
		}
		TriggerCloseOverlayJumpscare(Target, Variant, TEXT("Something sees you."), 3.0f, 42.0f, 42.0f);
		return;
	}

	if (ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(ResolvedSpawn.SpawnLocation, ResolvedSpawn.SpawnRotation, SpawnParams))
	{
		Monster->Configure(Target, 9300.0f, 8.6f, 3.05f);
		Monster->ConfigureVariant(Variant);
		if (TargetPC)
		{
			SendJumpscareChargeCue(Target, Variant, ResolvedSpawn.FocusLocation, TEXT("Something sees you."), 3.05f, 1.0f, false);
		}
		FreezeTargetForJumpscare(Target, 3.0f);
		CutLightsForJumpscare(TargetLocation, ResolvedSpawn.SpawnLocation, 0.0f, 10.25f);
		LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
		Target->AddFear(48.0f);
		Target->AddDread(46.0f);
	}
	else if (RequestingController)
	{
		RequestingController->ClientShowStatusMessage(TEXT("Could not spawn the visible monster model; using close visual."), 3.0f);
		TriggerCloseOverlayJumpscare(Target, Variant, TEXT("Something sees you."), 3.0f, 42.0f, 42.0f);
		return;
	}
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

	if (!TriggerManualScare(Target, EBHScareEventType::MonsterCharge))
	{
		TriggerMonsterChargeJumpscare(Target);
	}
	if (ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController()))
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
			}
		}
	}

	SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 80.0f), 90.0f + 10.0f * static_cast<float>(ClampedSurgeCharges), 0.32f, 0.22f, 5.0f, 5.5f);
	ApplyPresenceSpike(SourceLocation, 72.0f + 8.0f * static_cast<float>(ClampedSurgeCharges), TEXT("The dark moved with the Teacher."));
	BroadcastStatus(TEXT("The Teacher killed a bank of lights."), 3.5f);
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
	const FString StageIndexOption = GetWorld()->URL.GetOption(TEXT("BHStageIndex="), TEXT(""));
	RuntimeStageIndex = StageIndexOption.IsEmpty() ? GetConfiguredStageIndex() : FMath::Clamp(FCString::Atoi(*StageIndexOption), 0, 2);
	PendingIntermissionResult = RoundPhaseFromUrlValue(GetWorld()->URL.GetOption(TEXT("BHIntermissionResult="), TEXT("")));
	const FString TestOption = GetWorld()->URL.GetOption(TEXT("BHTestMode="), TEXT(""));
	bTestMode = IsTrueOption(TestOption);
	const FString PracticeOption = GetWorld()->URL.GetOption(TEXT("BHPractice="), TEXT(""));
	bPracticeMode = !bTestMode && (PracticeOption.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| PracticeOption.Equals(TEXT("Practice"), ESearchCase::IgnoreCase));
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
	if (bRevisionMode)
	{
		const FString RevisionTopicsOption = GetWorld()->URL.GetOption(TEXT("BHRevisionTopics="), TEXT("All"));
		const FString RevisionMixOption = GetWorld()->URL.GetOption(TEXT("BHRevisionDifficultyMix="), TEXT("Adaptive"));
		const FString ClassThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionClassThreshold="), *FString::SanitizeFloat(Settings->RevisionClassThreshold));
		const FString IndividualThresholdOption = GetWorld()->URL.GetOption(TEXT("BHRevisionIndividualThreshold="), *FString::SanitizeFloat(Settings->RevisionIndividualThreshold));
		const FString ScareIntensityOption = GetWorld()->URL.GetOption(TEXT("BHScareIntensity="), *FString::FromInt(Settings->RevisionScareIntensity));
		RevisionTopicMask = ParsePhysicsTopicMask(RevisionTopicsOption, 0x0F);
		RevisionDifficultyMix = ParseRevisionDifficultyMix(RevisionMixOption, EBHRevisionDifficultyMix::Adaptive);
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

	if (UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		BHGI->SetPersistentStageIndex(RuntimeStageIndex);
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

	AddMoodPass(FLinearColor(0.020f, 0.032f, 0.040f, 1.0f), 0.014f, 0.55f, 0.14f);
	AddFacilityVerticalSlicePass();
	AddFacilityDetailPass();
	AddClassroomHorrorPass();
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
		{FVector(3000.0f, -50.0f, CenterZForBlockBottom(0.0f, 2.60f)), FVector(0.38f, 8.8f, 2.60f), FRotator(0.0f, -18.0f, 0.0f), BrushScreen, EBHBlockMaterial::PaintedMetal}
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

	FRandomStream NatureStream(73421);
	for (int32 Index = 0; Index < 62; ++Index)
	{
		const float X = NatureStream.FRandRange(-7350.0f, 7350.0f);
		const float Y = NatureStream.FRandRange(-5650.0f, 5650.0f);
		if ((X > 5100.0f && FMath::Abs(Y) < 1600.0f) || FMath::Abs(Y + 3600.0f) < 460.0f || FMath::Abs(Y - 3950.0f) < 360.0f)
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
		FVector(-520.0f, -140.0f, 124.0f), FVector(-420.0f, -40.0f, 124.0f), FVector(-320.0f, 80.0f, 124.0f),
		FVector(-220.0f, 180.0f, 124.0f), FVector(-90.0f, -165.0f, 124.0f), FVector(20.0f, -55.0f, 124.0f),
		FVector(130.0f, 65.0f, 124.0f), FVector(240.0f, 165.0f, 124.0f), FVector(360.0f, -145.0f, 124.0f),
		FVector(470.0f, -45.0f, 124.0f), FVector(580.0f, 65.0f, 124.0f), FVector(690.0f, 165.0f, 124.0f)
	};
	HunterSpawn = FVector(-760.0f, 0.0f, 124.0f);

	const FLinearColor FloorTint(0.055f, 0.065f, 0.068f, 1.0f);
	const FLinearColor WallTint(0.10f, 0.12f, 0.13f, 1.0f);
	const FLinearColor TrainMetal(0.18f, 0.22f, 0.23f, 1.0f);
	const FLinearColor SeatTint(0.10f, 0.36f, 0.42f, 1.0f);
	const FLinearColor PoleTint(0.70f, 0.66f, 0.54f, 1.0f);
	const FLinearColor PlatformTint(0.17f, 0.16f, 0.14f, 1.0f);
	const FLinearColor WarningTint(0.92f, 0.64f, 0.18f, 1.0f);

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.20f)), FVector(80.0f, 24.0f, 0.20f), FloorTint, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
	SpawnBlock(FVector(0.0f, -1180.0f, CenterZForBlockBottom(0.0f, 3.1f)), FVector(80.0f, 0.22f, 3.1f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 1180.0f, CenterZForBlockBottom(0.0f, 3.1f)), FVector(80.0f, 0.22f, 3.1f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-4000.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.1f)), FVector(0.22f, 24.0f, 3.1f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(4000.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.1f)), FVector(0.22f, 24.0f, 3.1f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, -620.0f, CenterZForBlockTop(3.0f, 0.07f)), FVector(80.0f, 3.7f, 0.07f), FLinearColor(0.018f, 0.021f, 0.023f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(0.0f, 620.0f, CenterZForBlockTop(3.0f, 0.07f)), FVector(80.0f, 3.7f, 0.07f), FLinearColor(0.018f, 0.021f, 0.023f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(0.0f, -720.0f, 21.0f), FVector(78.0f, 0.055f, 0.12f), FLinearColor(0.68f, 0.64f, 0.46f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, -520.0f, 21.0f), FVector(78.0f, 0.055f, 0.12f), FLinearColor(0.68f, 0.64f, 0.46f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 520.0f, 21.0f), FVector(78.0f, 0.055f, 0.12f), FLinearColor(0.68f, 0.64f, 0.46f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 720.0f, 21.0f), FVector(78.0f, 0.055f, 0.12f), FLinearColor(0.68f, 0.64f, 0.46f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(-1150.0f, -392.0f, CenterZForBlockBottom(0.0f, 1.35f)), FVector(57.0f, 0.10f, 1.35f), FLinearColor(0.035f, 0.045f, 0.050f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, 392.0f, CenterZForBlockBottom(0.0f, 1.35f)), FVector(80.0f, 0.10f, 1.35f), FLinearColor(0.035f, 0.045f, 0.050f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
	SpawnBlock(FVector(0.0f, -520.0f, CenterZForBlockBottom(0.0f, 0.48f)), FVector(80.0f, 0.07f, 0.48f), FLinearColor(0.06f, 0.07f, 0.07f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::RustedMetal);
	SpawnBlock(FVector(0.0f, 520.0f, CenterZForBlockBottom(0.0f, 0.48f)), FVector(80.0f, 0.07f, 0.48f), FLinearColor(0.06f, 0.07f, 0.07f, 1.0f), FRotator::ZeroRotator, true, EBHBlockMaterial::RustedMetal);

	const FString Destination = PendingIntermissionResult == EBHRoundPhase::HunterWin
		? TEXT("Remediation Platform")
		: FString::Printf(TEXT("Next Stop: %s"), *GetNextMapAfterStage(RuntimeStageIndex));
	const bool bFinalRecap = RuntimeStageIndex >= 2 || GetNextMapAfterStage(RuntimeStageIndex).Equals(TEXT("Final"), ESearchCase::IgnoreCase);

	TrainIntermissionManager = GetWorld()->SpawnActor<ABHTrainIntermissionManager>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (TrainIntermissionManager)
	{
		TrainIntermissionManager->ConfigureIntermission(RuntimeStageIndex, GetNextMapAfterStage(RuntimeStageIndex), bFinalRecap ? TEXT("Final Station: Leaderboard") : Destination, bFinalRecap);
	}

	for (int32 CarIndex = 0; CarIndex < 5; ++CarIndex)
	{
		const float CenterX = -3000.0f + CarIndex * 1500.0f;
		const FString CarName = CarIndex == 0
			? TEXT("BOARDING")
			: (CarIndex == 1 ? TEXT("RECAP") : (CarIndex == 2 ? TEXT("SHOP") : (CarIndex == 3 ? TEXT("SOCIAL") : TEXT("STATION"))));
		SpawnBlock(FVector(CenterX, 0.0f, CenterZForBlockTop(0.0f, 0.18f)), FVector(13.0f, 4.8f, 0.18f), TrainMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::DiamondPlate);
		SpawnBlock(FVector(CenterX, -300.0f, 148.0f), FVector(13.0f, 0.14f, 2.1f), TrainMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(CenterX, 300.0f, 148.0f), FVector(13.0f, 0.14f, 2.1f), TrainMetal, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(FVector(CenterX, 0.0f, 304.0f), FVector(13.0f, 4.9f, 0.16f), WallTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		for (float BenchXOffset : {-430.0f, 430.0f})
		{
			SpawnBlock(FVector(CenterX + BenchXOffset, -238.0f, 70.0f), FVector(2.55f, 0.42f, 0.38f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
			SpawnBlock(FVector(CenterX + BenchXOffset, 238.0f, 70.0f), FVector(2.55f, 0.42f, 0.38f), SeatTint, FRotator::ZeroRotator, true, EBHBlockMaterial::Tiles);
		}
		SpawnBlock(FVector(CenterX, -265.0f, 166.0f), FVector(1.25f, 0.04f, 0.68f), FLinearColor(0.02f, 0.03f, 0.04f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
		SpawnBlock(FVector(CenterX, 265.0f, 166.0f), FVector(1.25f, 0.04f, 0.68f), FLinearColor(0.02f, 0.03f, 0.04f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
		SpawnBlock(FVector(CenterX, 0.0f, 260.0f), FVector(5.8f, 0.05f, 0.06f), WarningTint, FRotator::ZeroRotator, false, EBHBlockMaterial::WarningSign);

		for (int32 PoleIndex = 0; PoleIndex < 2; ++PoleIndex)
		{
			const float PoleX = CenterX - 420.0f + PoleIndex * 840.0f;
			SpawnBlock(FVector(PoleX, -72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
			SpawnBlock(FVector(PoleX, 72.0f, 150.0f), FVector(0.07f, 0.07f, 2.7f), PoleTint, FRotator::ZeroRotator, true, EBHBlockMaterial::PaintedMetal);
		}

		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(CenterX - 250.0f, -304.0f, 178.0f), FRotator(0.0f, 0.0f, 0.0f)))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(CarName, TEXT("Class performance review in progress."), FLinearColor(0.38f, 0.90f, 0.78f, 1.0f));
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterDisplay(Display);
			}
		}

		if (ABHTrainTunnelMotionActor* TunnelLeft = GetWorld()->SpawnActor<ABHTrainTunnelMotionActor>(FVector(CenterX, -430.0f, 166.0f), FRotator::ZeroRotator))
		{
			TunnelLeft->ConfigureMotion(1250.0f, 740.0f);
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterTunnelMotion(TunnelLeft);
			}
		}
		if (ABHTrainTunnelMotionActor* TunnelRight = GetWorld()->SpawnActor<ABHTrainTunnelMotionActor>(FVector(CenterX, 430.0f, 166.0f), FRotator::ZeroRotator))
		{
			TunnelRight->ConfigureMotion(1250.0f, 740.0f);
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterTunnelMotion(TunnelRight);
			}
		}
		if (TrainIntermissionManager)
		{
			ABHBlockActor* LeftShutter = SpawnDynamicBlock(FVector(CenterX, -430.0f, 166.0f), FVector(10.8f, 0.10f, 1.02f), FLinearColor(0.050f, 0.060f, 0.062f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
			ABHBlockActor* LeftBlocker = SpawnDynamicBlock(FVector(CenterX, -430.0f, 156.0f), FVector(11.2f, 0.18f, 2.24f), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
			ABHBlockActor* RightShutter = SpawnDynamicBlock(FVector(CenterX, 430.0f, 166.0f), FVector(10.8f, 0.10f, 1.02f), FLinearColor(0.050f, 0.060f, 0.062f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::PaintedMetal);
			ABHBlockActor* RightBlocker = SpawnDynamicBlock(FVector(CenterX, 430.0f, 156.0f), FVector(11.2f, 0.18f, 2.24f), FLinearColor(0.0f, 0.0f, 0.0f, 1.0f), FRotator::ZeroRotator, false, EBHBlockMaterial::Tinted);
			TrainIntermissionManager->RegisterMovingBarrier(LeftShutter, LeftBlocker);
			TrainIntermissionManager->RegisterMovingBarrier(RightShutter, RightBlocker);
		}
	}

	for (int32 CarIndex = 0; CarIndex < 5; ++CarIndex)
	{
		const float CenterX = -3000.0f + CarIndex * 1500.0f;
		const FString CarName = CarIndex == 0
			? TEXT("BOARDING")
			: (CarIndex == 1 ? TEXT("RECAP") : (CarIndex == 2 ? TEXT("SHOP") : (CarIndex == 3 ? TEXT("SOCIAL") : TEXT("STATION"))));
		if (ABHTrainDisplayActor* Display = GetWorld()->SpawnActor<ABHTrainDisplayActor>(FVector(CenterX + 250.0f, 304.0f, 178.0f), FRotator(0.0f, 180.0f, 0.0f)))
		{
			Display->SetDisplayProfile(EBHTrainDisplayProfile::TrainWallRecap);
			Display->ConfigureDisplay(CarName, TEXT("Class performance review in progress."), FLinearColor(0.38f, 0.90f, 0.78f, 1.0f));
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterDisplay(Display);
			}
		}
	}

	for (int32 LinkIndex = 0; LinkIndex < 4; ++LinkIndex)
	{
		const float DoorX = -2250.0f + LinkIndex * 1500.0f;
		if (ABHTrainDoor* Door = GetWorld()->SpawnActor<ABHTrainDoor>(FVector(DoorX, -304.0f, 92.0f), FRotator(0.0f, 0.0f, 0.0f)))
		{
			Door->ConfigureDoor(false, TEXT("Carriage Door"));
			Door->SetDoorOpen(true);
			if (TrainIntermissionManager)
			{
				TrainIntermissionManager->RegisterDoor(Door);
			}
		}
	}

	if (ABHTrainDoor* PlatformDoor = GetWorld()->SpawnActor<ABHTrainDoor>(FVector(3600.0f, -304.0f, 92.0f), FRotator(0.0f, 0.0f, 0.0f)))
	{
		PlatformDoor->ConfigureDoor(false, TEXT("Station Access"));
		if (TrainIntermissionManager)
		{
			TrainIntermissionManager->RegisterDoor(PlatformDoor);
		}
	}

	const EBHPowerupType ShopTypes[] = {
		EBHPowerupType::StaminaBoost,
		EBHPowerupType::SprintBurst,
		EBHPowerupType::FlashlightBoost,
		EBHPowerupType::QuestionHint,
		EBHPowerupType::DecoySound,
		EBHPowerupType::DoorRush
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShopTypes); ++Index)
	{
		if (ABHPowerupShopTerminal* Terminal = GetWorld()->SpawnActor<ABHPowerupShopTerminal>(FVector(-120.0f + (Index % 3) * 280.0f, 210.0f - (Index / 3) * 420.0f, 110.0f), FRotator(0.0f, 180.0f, 0.0f)))
		{
			Terminal->ConfigureShopItem(ShopTypes[Index]);
		}
	}

	const EBHPowerupType TeacherShopTypes[] = {
		EBHPowerupType::TeacherScanFocus,
		EBHPowerupType::TeacherBlackoutSurge,
		EBHPowerupType::TeacherPatrolIntel
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(TeacherShopTypes); ++Index)
	{
		if (ABHPowerupShopTerminal* Terminal = GetWorld()->SpawnActor<ABHPowerupShopTerminal>(FVector(1040.0f + Index * 270.0f, -210.0f, 110.0f), FRotator(0.0f, 0.0f, 0.0f)))
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
	const FTrainActivitySpec ActivitySpecs[] = {
		{EBHTrainActivityType::ReflexArcade, FVector(1320.0f, 96.0f, 110.0f), FRotator(0.0f, 180.0f, 0.0f), 17},
		{EBHTrainActivityType::SnackCart, FVector(1320.0f, -96.0f, 108.0f), FRotator(0.0f, 0.0f, 0.0f), 31},
		{EBHTrainActivityType::DrinkCooler, FVector(1680.0f, 96.0f, 108.0f), FRotator(0.0f, 180.0f, 0.0f), 47},
		{EBHTrainActivityType::MemoryTable, FVector(1680.0f, -96.0f, 104.0f), FRotator(0.0f, 0.0f, 0.0f), 59}
	};
	for (const FTrainActivitySpec& Spec : ActivitySpecs)
	{
		if (ABHTrainActivityStation* Activity = GetWorld()->SpawnActor<ABHTrainActivityStation>(Spec.Location, Spec.Rotation))
		{
			Activity->ConfigureActivity(Spec.Type, RoundSeed + RuntimeStageIndex * 101 + Spec.SeedOffset);
		}
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

	for (int32 LightIndex = 0; LightIndex < 8; ++LightIndex)
	{
		const FVector LightLocation(-3200.0f + LightIndex * 900.0f, 0.0f, 270.0f);
		if (ABHFlickerLight* Light = GetWorld()->SpawnActor<ABHFlickerLight>(LightLocation, FRotator::ZeroRotator))
		{
			Light->Configure(0, FLinearColor(0.42f, 0.84f, 0.78f, 1.0f), 680.0f, 820.0f);
			FlickerLights.Add(Light);
		}
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

void ABHGameMode::AddFoggroundsMoodPass()
{
	switch (RuntimeFogPreset)
	{
	case EBHFogPreset::Light:
		AddMoodPass(FLinearColor(0.060f, 0.090f, 0.095f, 1.0f), 0.075f, 0.56f, 0.14f);
		break;
	case EBHFogPreset::Extreme:
		AddMoodPass(FLinearColor(0.055f, 0.078f, 0.082f, 1.0f), 0.820f, 0.68f, 0.16f);
		break;
	case EBHFogPreset::Heavy:
	default:
		AddMoodPass(FLinearColor(0.012f, 0.022f, 0.024f, 1.0f), 1.150f, 0.94f, 0.36f);
		break;
	}

	AddFoggroundsModeledFog();
}

void ABHGameMode::AddFoggroundsLightingPass()
{
	const bool bExtremeFog = RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bHeavyFog = RuntimeFogPreset == EBHFogPreset::Heavy;
	const float SkyBrightness = bExtremeFog ? 0.43f : (bHeavyFog ? 0.34f : 0.29f);
	const FLinearColor MoonTint(0.36f, 0.48f, 0.72f, 1.0f);
	const FLinearColor SkyTint(0.10f, 0.16f, 0.27f, 1.0f);

	if (ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		SkyAtmosphere->SetReplicates(true);
		SkyAtmosphere->SetReplicateMovement(false);
		SkyAtmosphere->bAlwaysRelevant = true;
		if (USkyAtmosphereComponent* SkyComponent = SkyAtmosphere->GetComponent())
		{
			SkyComponent->SetSkyLuminanceFactor(FLinearColor(SkyBrightness * 0.78f, SkyBrightness * 0.92f, SkyBrightness * 1.12f, 1.0f));
			SkyComponent->SetSkyAndAerialPerspectiveLuminanceFactor(FLinearColor(SkyBrightness * 0.72f, SkyBrightness * 0.86f, SkyBrightness, 1.0f));
			SkyComponent->SetGroundAlbedo(FColor(22, 28, 34));
			SkyComponent->SetMultiScatteringFactor(0.35f);
			SkyComponent->SetHeightFogContribution(0.10f);
			SkyComponent->SetAerialPespectiveViewDistanceScale(0.25f);
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
			LightComponent->SetIntensity(bExtremeFog ? 0.68f : (bHeavyFog ? 0.56f : 0.43f));
			LightComponent->SetLightColor(MoonTint);
			LightComponent->SetIndirectLightingIntensity(bExtremeFog ? 0.50f : (bHeavyFog ? 0.40f : 0.32f));
			LightComponent->SetVolumetricScatteringIntensity(0.10f);
			LightComponent->SetCastShadows(false);
			LightComponent->SetAtmosphereSunLight(true);
			LightComponent->SetAtmosphereSunLightIndex(0);
			LightComponent->SetAtmosphereSunDiskColorScale(FLinearColor(0.12f, 0.18f, 0.30f, 1.0f));
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
			SkyLightComponent->SetIntensity(bExtremeFog ? 0.30f : (bHeavyFog ? 0.24f : 0.18f));
			SkyLightComponent->SetLightColor(SkyTint);
			SkyLightComponent->SetLowerHemisphereColor(FLinearColor(0.018f, 0.026f, 0.032f, 1.0f));
			SkyLightComponent->SetVolumetricScatteringIntensity(0.12f);
			SkyLightComponent->SetRealTimeCaptureEnabled(true);
			SkyLightComponent->SetCaptureIsDirty();
		}
	}
}

void ABHGameMode::AddFoggroundsModeledFog()
{
	int32 HorizontalBankCount = 40;
	int32 VerticalWispCount = 18;
	float BaseAlpha = 0.38f;
	float LocalVolumeDensity = 1.25f;
	FLinearColor FogTint(0.55f, 0.68f, 0.70f, BaseAlpha);

	switch (RuntimeFogPreset)
	{
	case EBHFogPreset::Light:
		HorizontalBankCount = 24;
		VerticalWispCount = 9;
		BaseAlpha = 0.26f;
		LocalVolumeDensity = 0.70f;
		FogTint = FLinearColor(0.60f, 0.72f, 0.73f, BaseAlpha);
		break;
	case EBHFogPreset::Extreme:
		HorizontalBankCount = 82;
		VerticalWispCount = 34;
		BaseAlpha = 0.42f;
		LocalVolumeDensity = 2.20f;
		FogTint = FLinearColor(0.42f, 0.56f, 0.58f, BaseAlpha);
		break;
	case EBHFogPreset::Heavy:
		HorizontalBankCount = 170;
		VerticalWispCount = 96;
		BaseAlpha = 0.78f;
		LocalVolumeDensity = 5.25f;
		FogTint = FLinearColor(0.24f, 0.32f, 0.34f, BaseAlpha);
		break;
	default:
		break;
	}

	const bool bExtremeFog = RuntimeFogPreset == EBHFogPreset::Extreme;
	const bool bDenseFog = RuntimeFogPreset == EBHFogPreset::Heavy || RuntimeFogPreset == EBHFogPreset::Extreme;
	const auto SpawnFogSheet = [](const FVector& Location, const FVector& Scale, const FRotator& Rotation, float AlphaScale)
	{
		// Fog cards read as visible rectangular planes in cooked builds. Local fog volumes below keep the modeled fog without geometry artifacts.
		(void)Location;
		(void)Scale;
		(void)Rotation;
		(void)AlphaScale;
	};

	struct FFogSheetSpec
	{
		FVector Location;
		FVector Scale;
		FRotator Rotation;
		float AlphaScale;
	};

	const FFogSheetSpec FixedSheets[] = {
		{FVector(5600.0f, -4920.0f, 42.0f), FVector(30.0f, 5.6f, 0.030f), FRotator(0.0f, 2.0f, 0.0f), 1.20f},
		{FVector(6500.0f, -3900.0f, 58.0f), FVector(18.0f, 4.0f, 0.030f), FRotator(0.0f, -15.0f, 0.0f), 1.15f},
		{FVector(0.0f, -4050.0f, 44.0f), FVector(48.0f, 4.4f, 0.030f), FRotator(0.0f, 0.0f, 0.0f), 1.05f},
		{FVector(-5800.0f, 4050.0f, 52.0f), FVector(24.0f, 5.2f, 0.030f), FRotator(0.0f, 8.0f, 0.0f), 1.25f},
		{FVector(-7100.0f, 4600.0f, 125.0f), FVector(12.0f, 0.050f, 2.4f), FRotator(0.0f, 90.0f, 0.0f), 1.30f},
		{FVector(-6900.0f, 1200.0f, 112.0f), FVector(10.0f, 0.050f, 2.1f), FRotator(0.0f, 104.0f, 0.0f), 1.10f},
		{FVector(-5200.0f, -5100.0f, 92.0f), FVector(11.0f, 0.045f, 1.7f), FRotator(0.0f, -8.0f, 0.0f), 0.92f},
		{FVector(7000.0f, -1150.0f, 132.0f), FVector(9.0f, 0.045f, 2.2f), FRotator(0.0f, 82.0f, 0.0f), 1.05f},
		{FVector(-980.0f, 5050.0f, 130.0f), FVector(13.5f, 0.055f, 2.6f), FRotator(0.0f, 38.0f, 0.0f), 1.30f},
		{FVector(-1200.0f, 4800.0f, 65.0f), FVector(14.0f, 4.2f, 0.035f), FRotator(0.0f, -20.0f, 0.0f), 1.18f}
	};
	for (const FFogSheetSpec& Sheet : FixedSheets)
	{
		SpawnFogSheet(Sheet.Location, Sheet.Scale, Sheet.Rotation, Sheet.AlphaScale);
	}

	if (bDenseFog)
	{
		const FFogSheetSpec ExtremeCurtains[] = {
			{FVector(5900.0f, -4725.0f, 155.0f), FVector(34.0f, 0.060f, 3.8f), FRotator(0.0f, 6.0f, 0.0f), 1.35f},
			{FVector(4200.0f, -3525.0f, 145.0f), FVector(26.0f, 0.060f, 3.4f), FRotator(0.0f, -12.0f, 0.0f), 1.20f},
			{FVector(0.0f, -4000.0f, 140.0f), FVector(42.0f, 0.060f, 3.3f), FRotator(0.0f, 2.0f, 0.0f), 1.25f},
			{FVector(-4300.0f, -4200.0f, 150.0f), FVector(24.0f, 0.060f, 3.5f), FRotator(0.0f, 15.0f, 0.0f), 1.28f},
			{FVector(-6500.0f, 700.0f, 155.0f), FVector(20.0f, 0.060f, 3.6f), FRotator(0.0f, 96.0f, 0.0f), 1.32f},
			{FVector(-5850.0f, 4050.0f, 145.0f), FVector(28.0f, 0.060f, 3.4f), FRotator(0.0f, -8.0f, 0.0f), 1.35f},
			{FVector(-800.0f, 4450.0f, 150.0f), FVector(30.0f, 0.060f, 3.5f), FRotator(0.0f, 12.0f, 0.0f), 1.25f},
			{FVector(4200.0f, 3300.0f, 145.0f), FVector(24.0f, 0.060f, 3.3f), FRotator(0.0f, -18.0f, 0.0f), 1.24f},
			{FVector(7000.0f, 900.0f, 155.0f), FVector(18.0f, 0.060f, 3.5f), FRotator(0.0f, 88.0f, 0.0f), 1.28f},
			{FVector(-980.0f, 5050.0f, 210.0f), FVector(22.0f, 0.065f, 4.2f), FRotator(0.0f, 40.0f, 0.0f), 1.42f}
		};
		for (const FFogSheetSpec& Curtain : ExtremeCurtains)
		{
			SpawnFogSheet(Curtain.Location, bExtremeFog ? Curtain.Scale * 0.82f : Curtain.Scale, Curtain.Rotation, bExtremeFog ? Curtain.AlphaScale * 0.50f : Curtain.AlphaScale);
		}
	}

	FRandomStream FogStream(91273 + static_cast<int32>(RuntimeFogPreset) * 811);
	for (int32 Index = 0; Index < HorizontalBankCount; ++Index)
	{
		const float X = FogStream.FRandRange(-7350.0f, 7350.0f);
		const float Y = FogStream.FRandRange(-5650.0f, 5650.0f);
		if (X > 5400.0f && Y < -3300.0f && RuntimeFogPreset == EBHFogPreset::Light)
		{
			continue;
		}

		const FVector Location(X, Y, FogStream.FRandRange(28.0f, 72.0f));
		const float SheetScaleBoost = bExtremeFog ? 0.88f : 1.0f;
		const FVector Scale(FogStream.FRandRange(7.5f, 19.0f) * SheetScaleBoost, FogStream.FRandRange(2.0f, 6.2f) * SheetScaleBoost, FogStream.FRandRange(0.020f, 0.045f) * (bExtremeFog ? 0.75f : 1.0f));
		const FRotator Rotation(0.0f, FogStream.FRandRange(-35.0f, 35.0f), 0.0f);
		SpawnFogSheet(Location, Scale, Rotation, bExtremeFog ? FogStream.FRandRange(0.34f, 0.74f) : FogStream.FRandRange(0.65f, 1.20f));
	}

	for (int32 Index = 0; Index < VerticalWispCount; ++Index)
	{
		const float X = FogStream.FRandRange(-7600.0f, 7350.0f);
		const float Y = FogStream.FRandRange(-5700.0f, 5700.0f);
		const FVector Location(X, Y, FogStream.FRandRange(92.0f, 170.0f));
		const float WispScaleBoost = bExtremeFog ? 0.82f : 1.0f;
		const FVector Scale(FogStream.FRandRange(5.0f, 13.5f) * WispScaleBoost, FogStream.FRandRange(0.030f, 0.070f) * (bExtremeFog ? 0.72f : 1.0f), FogStream.FRandRange(1.2f, 3.0f) * WispScaleBoost);
		const FRotator Rotation(0.0f, FogStream.FRandRange(-180.0f, 180.0f), 0.0f);
		SpawnFogSheet(Location, Scale, Rotation, bExtremeFog ? FogStream.FRandRange(0.38f, 0.82f) : FogStream.FRandRange(0.75f, 1.35f));
	}

	struct FFogVolumeSpec
	{
		FVector Location;
		FVector Scale;
		float DensityScale;
	};

	const FFogVolumeSpec VolumeSpecs[] = {
		{FVector(6100.0f, -4850.0f, 130.0f), FVector(7.8f, 3.0f, 1.15f), 1.20f},
		{FVector(0.0f, -4050.0f, 125.0f), FVector(10.5f, 2.2f, 1.05f), 0.95f},
		{FVector(-5850.0f, 4050.0f, 135.0f), FVector(6.5f, 3.1f, 1.20f), 1.25f},
		{FVector(-6950.0f, 4000.0f, 160.0f), FVector(4.5f, 4.0f, 1.55f), 1.35f},
		{FVector(-6600.0f, -900.0f, 150.0f), FVector(4.8f, 3.5f, 1.30f), 1.05f},
		{FVector(6900.0f, -900.0f, 145.0f), FVector(4.2f, 3.0f, 1.20f), 0.95f},
		{FVector(-980.0f, 5050.0f, 190.0f), FVector(5.5f, 4.4f, 1.80f), 1.40f},
		{FVector(2500.0f, 2400.0f, 135.0f), FVector(6.0f, 3.6f, 1.25f), 0.90f}
	};

	for (const FFogVolumeSpec& Spec : VolumeSpecs)
	{
		if (ALocalFogVolume* Volume = GetWorld()->SpawnActor<ALocalFogVolume>(Spec.Location, FRotator::ZeroRotator))
		{
			Volume->SetReplicates(true);
			Volume->SetReplicateMovement(false);
			Volume->bAlwaysRelevant = true;
			Volume->SetActorScale3D(Spec.Scale);
			if (ULocalFogVolumeComponent* FogComponent = Volume->GetComponent())
			{
				FogComponent->SetRadialFogExtinction(LocalVolumeDensity * Spec.DensityScale);
				FogComponent->SetHeightFogExtinction(LocalVolumeDensity * (bExtremeFog ? 0.38f : 0.55f) * Spec.DensityScale);
				FogComponent->SetHeightFogFalloff(bExtremeFog ? 2200.0f : (RuntimeFogPreset == EBHFogPreset::Heavy ? 3600.0f : 1800.0f));
				FogComponent->SetHeightFogOffset(bExtremeFog ? -0.38f : (RuntimeFogPreset == EBHFogPreset::Heavy ? -0.65f : -0.35f));
				FogComponent->SetFogAlbedo(FLinearColor(FogTint.R, FogTint.G, FogTint.B, 1.0f));
				FogComponent->SetFogEmissive(bExtremeFog ? FLinearColor(0.016f, 0.024f, 0.025f, 1.0f) : FLinearColor(0.008f, 0.014f, 0.016f, 1.0f));
				FogComponent->SetFogPhaseG(bExtremeFog ? 0.32f : 0.35f);
			}
		}
	}
}

void ABHGameMode::BuildSubstationLevel()
{
	SurvivorSpawns = {
		FVector(4860.0f, -1420.0f, 120.0f), FVector(5120.0f, -1040.0f, 120.0f), FVector(5380.0f, -660.0f, 120.0f),
		FVector(4860.0f, -240.0f, 120.0f), FVector(5120.0f, 180.0f, 120.0f), FVector(5380.0f, 580.0f, 120.0f),
		FVector(4860.0f, 980.0f, 120.0f), FVector(5120.0f, 1380.0f, 120.0f), FVector(4560.0f, -980.0f, 120.0f),
		FVector(4560.0f, 980.0f, 120.0f), FVector(5480.0f, -1260.0f, 120.0f), FVector(5480.0f, 1260.0f, 120.0f)
	};
	HunterSpawn = FVector(-5600.0f, -1200.0f, 120.0f);

	const FLinearColor Concrete(0.11f, 0.12f, 0.125f, 1.0f);
	const FLinearColor Ceiling(0.045f, 0.050f, 0.055f, 1.0f);
	const FLinearColor Wall(0.21f, 0.24f, 0.25f, 1.0f);
	const FLinearColor Steel(0.20f, 0.22f, 0.23f, 1.0f);
	const FLinearColor Rust(0.34f, 0.16f, 0.08f, 1.0f);
	const FLinearColor Cable(0.02f, 0.025f, 0.028f, 1.0f);
	const FLinearColor Warning(0.85f, 0.58f, 0.08f, 1.0f);
	const FLinearColor ControlBlue(0.08f, 0.22f, 0.30f, 1.0f);
	const FLinearColor SickGreen(0.08f, 0.26f, 0.14f, 1.0f);

	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockTop(0.0f, 0.25f)), FVector(122.0f, 92.0f, 0.25f), Concrete, FRotator::ZeroRotator, true, EBHBlockMaterial::Concrete);
	SpawnBlock(FVector(0.0f, 0.0f, CenterZForBlockBottom(350.0f, 0.12f)), FVector(122.0f, 92.0f, 0.12f), Ceiling, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);

	SpawnBlock(FVector(0.0f, -4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(0.0f, 4600.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(122.0f, 0.35f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(-6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	SpawnBlock(FVector(6100.0f, 0.0f, CenterZForBlockBottom(0.0f, 3.5f)), FVector(0.35f, 92.0f, 3.5f), Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	AddMapContainment(6100.0f, 4600.0f);

	const TArray<TPair<FVector, FVector>> Walls = {
		{FVector(-4400.0f, -2500.0f, 175.0f), FVector(34.0f, 0.30f, 3.25f)}, {FVector(-900.0f, -2500.0f, 175.0f), FVector(24.0f, 0.30f, 3.25f)}, {FVector(3200.0f, -2500.0f, 175.0f), FVector(40.0f, 0.30f, 3.25f)},
		{FVector(-4200.0f, 0.0f, 175.0f), FVector(36.0f, 0.30f, 3.25f)}, {FVector(100.0f, 0.0f, 175.0f), FVector(28.0f, 0.30f, 3.25f)}, {FVector(4300.0f, 0.0f, 175.0f), FVector(28.0f, 0.30f, 3.25f)},
		{FVector(-4100.0f, 2500.0f, 175.0f), FVector(38.0f, 0.30f, 3.25f)}, {FVector(250.0f, 2500.0f, 175.0f), FVector(34.0f, 0.30f, 3.25f)}, {FVector(4450.0f, 2500.0f, 175.0f), FVector(30.0f, 0.30f, 3.25f)},
		{FVector(-4500.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(-4500.0f, 1200.0f, 175.0f), FVector(0.30f, 34.0f, 3.25f)},
		{FVector(-2500.0f, -1200.0f, 175.0f), FVector(0.30f, 26.0f, 3.25f)}, {FVector(-2500.0f, 3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)},
		{FVector(-500.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(-500.0f, 1250.0f, 175.0f), FVector(0.30f, 33.0f, 3.25f)},
		{FVector(1800.0f, -1400.0f, 175.0f), FVector(0.30f, 32.0f, 3.25f)}, {FVector(1800.0f, 3600.0f, 175.0f), FVector(0.30f, 20.0f, 3.25f)},
		{FVector(3900.0f, -3400.0f, 175.0f), FVector(0.30f, 24.0f, 3.25f)}, {FVector(3900.0f, 1300.0f, 175.0f), FVector(0.30f, 32.0f, 3.25f)}
	};
	for (const TPair<FVector, FVector>& WallSpec : Walls)
	{
		SpawnBlock(FVector(WallSpec.Key.X, WallSpec.Key.Y, CenterZForBlockBottom(0.0f, WallSpec.Value.Z)), WallSpec.Value, Wall, FRotator::ZeroRotator, true, EBHBlockMaterial::Plaster);
	}

	const TArray<TPair<FVector, FRotator>> Doors = {
		{FVector(-2400.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(750.0f, -2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-1850.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(2200.0f, 0.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-1825.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(2450.0f, 2500.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-4500.0f, -1350.0f, 120.0f), FRotator::ZeroRotator}, {FVector(-2500.0f, 1150.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(-500.0f, -1300.0f, 120.0f), FRotator::ZeroRotator}, {FVector(1800.0f, 1400.0f, 120.0f), FRotator::ZeroRotator},
		{FVector(3900.0f, -1250.0f, 120.0f), FRotator::ZeroRotator}
	};
	for (const TPair<FVector, FRotator>& Door : Doors)
	{
		if (ABHDoor* DoorActor = GetWorld()->SpawnActor<ABHDoor>(Door.Key, Door.Value))
		{
			DoorActors.Add(DoorActor);
		}
	}

	const TArray<TPair<FVector, FRotator>> Breakers = {
		{FVector(-5550.0f, -3650.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)}, {FVector(-5400.0f, 3650.0f, 80.0f), FRotator(0.0f, 90.0f, 0.0f)},
		{FVector(-850.0f, -3800.0f, 80.0f), FRotator(0.0f, 180.0f, 0.0f)}, {FVector(850.0f, 3800.0f, 80.0f), FRotator::ZeroRotator},
		{FVector(3300.0f, -3750.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}, {FVector(5450.0f, 3300.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)},
		{FVector(5200.0f, 350.0f, 80.0f), FRotator(0.0f, -90.0f, 0.0f)}
	};
	for (const TPair<FVector, FRotator>& Breaker : Breakers)
	{
		if (ABHBreaker* BreakerActor = GetWorld()->SpawnActor<ABHBreaker>(Breaker.Key, Breaker.Value))
		{
			BreakerActors.Add(BreakerActor);
		}
	}

	const TArray<TPair<FVector, EBHObjectiveStationType>> ObjectiveStationSpecs = {
		{FVector(-5850.0f, -1650.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-5850.0f, 1650.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-1200.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1650.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(5350.0f, -1700.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(5300.0f, 2100.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-3600.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-3600.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-1200.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-1200.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(1450.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(1450.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3700.0f, -1250.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(3700.0f, 1250.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-5550.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(-5400.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-2500.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(-2500.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-500.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(-500.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(1800.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(1800.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(5350.0f, 350.0f, 95.0f), EBHObjectiveStationType::Evidence},
		{FVector(3900.0f, 3650.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(3900.0f, -3650.0f, 95.0f), EBHObjectiveStationType::Terminal},
		{FVector(5450.0f, -3300.0f, 95.0f), EBHObjectiveStationType::Antenna},
		{FVector(-5850.0f, 0.0f, 95.0f), EBHObjectiveStationType::Valve},
		{FVector(5350.0f, 3900.0f, 95.0f), EBHObjectiveStationType::Evidence}
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
		FVector(-3600.0f, 0.0f, CenterZForBlockBottom(0.65f, 0.035f)),
		FVector(-1200.0f, 0.0f, CenterZForBlockBottom(0.70f, 0.035f)),
		FVector(1450.0f, 0.0f, CenterZForBlockBottom(0.75f, 0.035f)),
		FVector(3700.0f, 0.0f, CenterZForBlockBottom(0.80f, 0.035f))
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

	for (int32 Row = -3; Row <= 3; ++Row)
	{
		for (int32 Col = -2; Col <= 2; ++Col)
		{
			if ((Row + Col) % 2 == 0)
			{
				SpawnBlock(FVector(Row * 950.0f, Col * 780.0f, 80.0f), FVector(1.1f, 3.5f, 1.7f), Steel, FRotator(0.0f, 12.0f * Row, 0.0f), true, EBHBlockMaterial::DiamondPlate);
				SpawnBlock(FVector(Row * 950.0f + 165.0f, Col * 780.0f, 245.0f), FVector(0.28f, 3.7f, 0.16f), Rust, FRotator(0.0f, 12.0f * Row, 0.0f), false, EBHBlockMaterial::RustedMetal);
			}
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

	const TArray<FVector> ClutterCenters = {
		FVector(-5200.0f, -3300.0f, 40.0f), FVector(-3600.0f, -3300.0f, 40.0f), FVector(-1200.0f, -3500.0f, 40.0f), FVector(1200.0f, -3500.0f, 40.0f),
		FVector(3450.0f, -3300.0f, 40.0f), FVector(5200.0f, -2700.0f, 40.0f), FVector(-5200.0f, 3200.0f, 40.0f), FVector(-3300.0f, 3400.0f, 40.0f),
		FVector(-900.0f, 3500.0f, 40.0f), FVector(1550.0f, 3500.0f, 40.0f), FVector(3900.0f, 3300.0f, 40.0f), FVector(5300.0f, 2300.0f, 40.0f)
	};
	AddIndustrialClutter(ClutterCenters, Rust);
	AddIndustrialClutter({
		FVector(-5700.0f, -4100.0f, 45.0f), FVector(-5700.0f, 4100.0f, 45.0f),
		FVector(5700.0f, -4100.0f, 45.0f), FVector(5700.0f, 4100.0f, 45.0f)
	}, Steel);

	const TArray<FVector> Lockers = {
		FVector(-5750.0f, -4050.0f, 100.0f), FVector(-5000.0f, -4050.0f, 100.0f), FVector(-3100.0f, -4050.0f, 100.0f), FVector(-1800.0f, -4050.0f, 100.0f),
		FVector(-250.0f, -4050.0f, 100.0f), FVector(1250.0f, -4050.0f, 100.0f), FVector(3000.0f, -4050.0f, 100.0f), FVector(5000.0f, -4050.0f, 100.0f),
		FVector(-5750.0f, 4050.0f, 100.0f), FVector(-5000.0f, 4050.0f, 100.0f), FVector(-3100.0f, 4050.0f, 100.0f), FVector(-1800.0f, 4050.0f, 100.0f),
		FVector(-250.0f, 4050.0f, 100.0f), FVector(1250.0f, 4050.0f, 100.0f), FVector(3000.0f, 4050.0f, 100.0f), FVector(5000.0f, 4050.0f, 100.0f),
		FVector(-5850.0f, -900.0f, 100.0f), FVector(-5850.0f, 900.0f, 100.0f), FVector(5850.0f, -900.0f, 100.0f), FVector(5850.0f, 900.0f, 100.0f),
		FVector(2200.0f, -400.0f, 100.0f), FVector(2200.0f, 720.0f, 100.0f), FVector(-3100.0f, -640.0f, 100.0f), FVector(-3100.0f, 760.0f, 100.0f)
	};
	for (const FVector& Location : Lockers)
	{
		GetWorld()->SpawnActor<ABHLocker>(Location, FRotator::ZeroRotator);
	}

	const FVector Batteries[] = {
		FVector(-5200.0f, -1650.0f, 70.0f), FVector(-5200.0f, 1650.0f, 70.0f), FVector(-1500.0f, -3650.0f, 70.0f), FVector(-1500.0f, 3650.0f, 70.0f),
		FVector(900.0f, -3650.0f, 70.0f), FVector(900.0f, 3650.0f, 70.0f), FVector(3300.0f, -1450.0f, 70.0f), FVector(3300.0f, 1450.0f, 70.0f),
		FVector(5400.0f, -600.0f, 70.0f), FVector(5400.0f, 1900.0f, 70.0f)
	};
	for (const FVector& Location : Batteries)
	{
		GetWorld()->SpawnActor<ABHBatteryPickup>(Location, FRotator(0.0f, 90.0f, 0.0f));
	}

	const FVector Alarms[] = {
		FVector(-5600.0f, -2400.0f, 105.0f), FVector(-5600.0f, 2400.0f, 105.0f),
		FVector(-850.0f, -2500.0f, 105.0f), FVector(850.0f, 2500.0f, 105.0f),
		FVector(3650.0f, -2500.0f, 105.0f), FVector(3650.0f, 2500.0f, 105.0f),
		FVector(5600.0f, -900.0f, 105.0f), FVector(5600.0f, 900.0f, 105.0f)
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

	for (const FVector& Location : {FVector(-900.0f, 0.0f, 120.0f), FVector(1800.0f, 0.0f, 120.0f), FVector(3900.0f, 0.0f, 120.0f), FVector(-2500.0f, 2500.0f, 120.0f)})
	{
		if (ABHSecurityShutter* Shutter = GetWorld()->SpawnActor<ABHSecurityShutter>(Location, FRotator(0.0f, 90.0f, 0.0f)))
		{
			Shutter->Configure(200);
		}
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(-5850.0f, 0.0f, 110.0f), FRotator(0.0f, 90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Toggle Substation Shutters")));
	}
	if (ABHSecurityTerminal* Terminal = GetWorld()->SpawnActor<ABHSecurityTerminal>(FVector(5800.0f, 2100.0f, 110.0f), FRotator(0.0f, -90.0f, 0.0f)))
	{
		Terminal->Configure(200, FText::FromString(TEXT("Toggle Substation Shutters")));
	}
	GetWorld()->SpawnActor<ABHSecurityMonitor>(FVector(-5450.0f, -850.0f, 115.0f), FRotator(0.0f, 90.0f, 0.0f));
	const TArray<TPair<FVector, FRotator>> SubstationCameras = {
		{FVector(-4550.0f, -2650.0f, 355.0f), FRotator(0.0f, 35.0f, 0.0f)},
		{FVector(4550.0f, 2650.0f, 355.0f), FRotator(0.0f, -145.0f, 0.0f)}
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
		FVector(-5550.0f, -3650.0f, 110.0f),
		FVector(-5400.0f, 3650.0f, 110.0f),
		FVector(-850.0f, -3800.0f, 110.0f),
		FVector(850.0f, 3800.0f, 110.0f),
		FVector(3300.0f, -3750.0f, 110.0f),
		FVector(5450.0f, 3300.0f, 110.0f),
		FVector(5200.0f, 350.0f, 110.0f),
		FVector(-5850.0f, -900.0f, 110.0f),
		FVector(5850.0f, 900.0f, 110.0f),
		FVector(1800.0f, 0.0f, 110.0f),
		FVector(-2500.0f, 2500.0f, 110.0f)
	});

	AddMoodPass(FLinearColor(0.018f, 0.045f, 0.052f, 1.0f), 0.010f, 0.50f, 0.10f);
	AddClassroomHorrorPass();
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

	bRuntimeNavigationReady = false;
	FinalizeStaticBlockField();

	const bool bSubstation = RuntimeLevelName.Equals(TEXT("Substation"), ESearchCase::IgnoreCase);
	const bool bFoggrounds = RuntimeLevelName.Equals(TEXT("Foggrounds"), ESearchCase::IgnoreCase);
	const FVector BoundsSize = bFoggrounds ? FVector(17000.0f, 13600.0f, 1000.0f) : (bSubstation ? FVector(13200.0f, 10000.0f, 800.0f) : FVector(12000.0f, 9600.0f, 800.0f));
	const FVector BoundsCenter(0.0f, 0.0f, BoundsSize.Z * 0.5f);

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
		for (float CenterX = -3000.0f; CenterX <= 3000.0f; CenterX += 1500.0f)
		{
			GlassPanes.Add({FVector(CenterX, -265.0f, 166.0f), FVector(1.25f, 0.05f, 0.68f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Train Glass"))});
			GlassPanes.Add({FVector(CenterX, 265.0f, 166.0f), FVector(1.25f, 0.05f, 0.68f), FRotator::ZeroRotator, FText::FromString(TEXT("Break Train Glass"))});
		}
		SafeClutter = {FVector(-3450.0f, -880.0f, 38.0f), FVector(0.0f, 880.0f, 38.0f), FVector(3450.0f, -880.0f, 38.0f)};
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
	const FLinearColor DarkMetal(0.08f, 0.09f, 0.095f, 1.0f);
	for (int32 Index = 0; Index < Centers.Num(); ++Index)
	{
		const FVector& Center = Centers[Index];
		const float Angle = (Index * 37) % 180;
		SpawnBlock(Center, FVector(0.85f, 0.85f, 0.85f), Tint, FRotator(0.0f, Angle, 0.0f), true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Center + FVector(115.0f, -85.0f, 25.0f), FVector(0.55f, 0.42f, 0.46f), DarkMetal, FRotator(0.0f, Angle + 18.0f, 0.0f), true, EBHBlockMaterial::PaintedMetal);
		SpawnBlock(Center + FVector(-95.0f, 115.0f, 75.0f), FVector(0.35f, 0.35f, 1.25f), DarkMetal, FRotator(0.0f, Angle - 25.0f, 0.0f), true, EBHBlockMaterial::RustedMetal);
		SpawnBlock(Center + FVector(20.0f, 0.0f, 165.0f), FVector(0.10f, 1.4f, 0.10f), DarkMetal, FRotator(0.0f, Angle, 0.0f), false, EBHBlockMaterial::PaintedMetal);
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
		PostProcess->Settings.ColorSaturation = bExtremeFog ? FVector4(0.88f, 0.94f, 0.96f, 1.0f) : FVector4(0.78f, 0.84f, 0.90f, 1.0f);
		PostProcess->Settings.bOverride_ColorContrast = true;
		PostProcess->Settings.ColorContrast = bExtremeFog ? FVector4(1.02f, 1.02f, 1.00f, 1.0f) : FVector4(1.12f, 1.10f, 1.06f, 1.0f);
		const float FoggroundsBrightExposure = bExtremeFog ? 0.025f : (bHeavyFog ? 0.024f : 0.030f);
		const float FoggroundsDarkExposure = bExtremeFog ? 0.85f : (bHeavyFog ? 0.78f : 0.95f);
		const float FoggroundsHybridExposure = (FoggroundsBrightExposure + FoggroundsDarkExposure) * 0.5f;
		PostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
		PostProcess->Settings.AutoExposureMinBrightness = bFoggrounds ? FoggroundsHybridExposure : 0.030f;
		PostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
		PostProcess->Settings.AutoExposureMaxBrightness = bFoggrounds ? FoggroundsHybridExposure : 0.95f;
		if (bFoggrounds)
		{
			PostProcess->Settings.bOverride_AutoExposureBias = true;
			PostProcess->Settings.AutoExposureBias = bExtremeFog ? -0.32f : (bHeavyFog ? -0.24f : -0.14f);
			PostProcess->Settings.bOverride_SceneColorTint = true;
			PostProcess->Settings.SceneColorTint = bExtremeFog ? FLinearColor(0.94f, 0.99f, 1.04f, 1.0f) : FLinearColor(0.92f, 0.96f, 1.02f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingColor = true;
			PostProcess->Settings.IndirectLightingColor = FLinearColor(0.12f, 0.18f, 0.28f, 1.0f);
			PostProcess->Settings.bOverride_IndirectLightingIntensity = true;
			PostProcess->Settings.IndirectLightingIntensity = bExtremeFog ? 0.48f : (bHeavyFog ? 0.38f : 0.30f);
		}
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetFogDensity(FogDensity);
			FogComponent->SetFogHeightFalloff(bExtremeFog ? 0.085f : 0.075f);
			FogComponent->SetFogInscatteringColor(FogColor);
			FogComponent->SetFogMaxOpacity(bExtremeFog ? 0.78f : 1.0f);
			FogComponent->SetStartDistance(0.0f);
			FogComponent->SetEndDistance(0.0f);
			FogComponent->SetFogCutoffDistance(0.0f);
			FogComponent->SetDirectionalInscatteringExponent(8.0f);
			FogComponent->SetDirectionalInscatteringStartDistance(0.0f);
			const float DirectionalInscatterScale = bExtremeFog ? 1.75f : 1.45f;
			FogComponent->SetDirectionalInscatteringColor(FLinearColor(FogColor.R * DirectionalInscatterScale, FogColor.G * DirectionalInscatterScale, FogColor.B * DirectionalInscatterScale, 1.0f));
			FogComponent->SetVolumetricFog(true);
			FogComponent->SetVolumetricFogScatteringDistribution(bExtremeFog ? 0.38f : 0.45f);
			FogComponent->SetVolumetricFogAlbedo(FColor(
				FMath::Clamp(FMath::RoundToInt(FogColor.R * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.G * 255.0f), 0, 255),
				FMath::Clamp(FMath::RoundToInt(FogColor.B * 255.0f), 0, 255)));
			FogComponent->SetVolumetricFogEmissive(FLinearColor(FogColor.R * (bExtremeFog ? 0.030f : 0.018f), FogColor.G * (bExtremeFog ? 0.030f : 0.018f), FogColor.B * (bExtremeFog ? 0.030f : 0.018f), 1.0f));
			FogComponent->SetVolumetricFogExtinctionScale(FMath::Clamp(FogDensity * 18.0f, 0.55f, bExtremeFog ? 2.65f : 3.25f));
			FogComponent->SetVolumetricFogDistance(bExtremeFog ? 4200.0f : 5200.0f);
			FogComponent->SetVolumetricFogStartDistance(0.0f);
			FogComponent->SetVolumetricFogNearFadeInDistance(bExtremeFog ? 75.0f : 55.0f);
		}
	}
}

ABHStaticBlockField* ABHGameMode::EnsureStaticBlockField()
{
	if (IsValid(StaticBlockField))
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
	return StaticBlockField;
}

void ABHGameMode::ResetStaticBlockField()
{
	if (ABHStaticBlockField* Field = EnsureStaticBlockField())
	{
		Field->ResetBlockSpecs();
	}
}

void ABHGameMode::FinalizeStaticBlockField()
{
	if (IsValid(StaticBlockField))
	{
		StaticBlockField->FinalizeBuild();
	}
}

void ABHGameMode::AddStaticBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Tint, const FRotator& Rotation, bool bCollides, EBHBlockMaterial Material, bool bStartHidden)
{
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
	if (ABHAmbientEmitter* Emitter = GetWorld()->SpawnActor<ABHAmbientEmitter>(Location, FRotator::ZeroRotator))
	{
		Emitter->Configure(Frequency, Volume, Noise, Pulse);
		if (LifeSpan > 0.0f)
		{
			Emitter->SetLifeSpan(LifeSpan);
		}
	}
}

void ABHGameMode::PrepareRoundDirector()
{
	RoundSeed = FMath::Rand();
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
	ActiveBreakerCount = bTestMode ? BreakerActors.Num() : (bRevisionMode ? 0 : FMath::Clamp(RequiredBreakers - FMath::Min(2, ActiveSideObjectiveCount), 2, FMath::Max(1, BreakerActors.Num())));
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
	LastPresenceWhisperTime = LastDirectorScareTime - 999.0f;
	LastWhisperJumpscareTime = LastDirectorScareTime - 999.0f;
	LastPresenceSpikeTime = LastDirectorScareTime - 999.0f;
	LastTeacherCounterScareTime = LastDirectorScareTime - 999.0f;
	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetRoundOptions(TargetHunterCount, ObjectiveIntensity, bInfectionMode, bPartyPace, ChosenModifier);
		BHGS->SetPresenceState(12.0f, TEXT("The building is listening."), 0);
		BHGS->SetRevisionOptions(RevisionMode, RevisionTopicMask, RevisionDifficultyMix, RevisionClassThreshold, RevisionIndividualThreshold, RevisionRoundDuration, RevisionScareIntensity);
		BHGS->SetRevisionContributionTarget(GetRevisionMinimumContributionTarget());
	}
	if (bRevisionMode)
	{
		ResetRevisionStats();
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
}

void ABHGameMode::UpdatePresenceDirector()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS || !GetWorld() || BHGS->RoundPhase != EBHRoundPhase::Hunt)
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
	const float DecayedPresence = FMath::Max(0.0f, CurrentPresence - 10.0f);
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

	const bool bPulse = NewPresence >= 72.0f || FMath::Abs(NewPresence - CurrentPresence) >= 18.0f;
	BHGS->SetPresenceState(NewPresence, PresenceText, bPulse ? BHGS->PresencePulse + 1 : BHGS->PresencePulse);

	const float Now = GetWorld()->GetTimeSeconds();
	const float WhisperCooldown = bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge ? 8.5f : 14.0f;
	const bool bWhisperMoment = BestTarget
		&& (BestTargetDistance <= 4200.0f || BestTarget->IsHiddenInLocker() || BestTarget->GetDread() >= 52.0f || BestTarget->IsDetentionMarked());
	if (bWhisperMoment && NewPresence >= 54.0f && Now - LastPresenceWhisperTime >= WhisperCooldown)
	{
		const FVector BehindTarget = BestTarget->GetActorLocation() - BestTarget->GetActorForwardVector() * FMath::FRandRange(180.0f, 420.0f) + FVector(0.0f, 0.0f, 88.0f);
		const float WhisperJumpscareCooldown = bPartyPace || BHGS->RoundModifier == EBHRoundModifier::PanicSurge ? 36.0f : 68.0f;
		const bool bHighPressureWhisper = NewPresence >= 82.0f
			|| (BestTarget->IsHiddenInLocker() && NewPresence >= 72.0f)
			|| BestTarget->GetDread() >= 70.0f
			|| BestTarget->IsDetentionMarked();
		FBHJumpscareVariant PendingWhisperVariant;
		const bool bTriggerWhisperJumpscare = bHighPressureWhisper
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

	const float Now = GetWorld()->GetTimeSeconds();
	const float NewPresence = FMath::Clamp(FMath::Max(BHGS->PresenceLevel, SpikeLevel), 0.0f, 100.0f);
	BHGS->SetPresenceState(NewPresence, PresenceText, BHGS->PresencePulse + 1);
	LastPresenceSpikeTime = Now;

	if (Now - LastPresenceWhisperTime > 5.0f)
	{
		SpawnAmbient(SourceLocation + FVector(0.0f, 0.0f, 86.0f), FMath::FRandRange(125.0f, 260.0f), 0.20f, 0.16f, 4.7f, 3.25f);
	}
}

void ABHGameMode::PublishObjectiveBeats()
{
	ABHGameState* BHGS = GetGameState<ABHGameState>();
	if (!BHGS)
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
	if (GetWorld() && GetWorld()->GetTimeSeconds() - LastMonsterChargeTime >= MonsterCooldown && FMath::FRand() < MonsterChance)
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
		TriggerMonsterChargeJumpscare(TeacherTarget);
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

	if (ClampedSeverity >= 4)
	{
		if (TargetPC)
		{
			TargetPC->ClientShowStatusMessage(TeacherMessage, 3.0f);
		}
		TriggerMonsterChargeJumpscare(TargetTeacher);
	}
	else
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
			Cue.bUpperBodyCloseVisual = Cue.bCloseRangeFocus;
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
	const FBHJumpscareVariant Variant = ChooseJumpscareVariant(EBHScareEventType::MonsterCharge);
	TriggerMonsterChargeJumpscareWithVariant(Target, Variant, TEXT("Something sees you."), 38.0f, 42.0f);
}

void ABHGameMode::TriggerMonsterChargeJumpscareWithVariant(ABHCharacter* Target, const FBHJumpscareVariant& Variant, const FString& Message, float FearAmount, float DreadAmount)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	const FVector TargetLocation = Target->GetActorLocation();
	ABHPlayerController* TargetPC = Cast<ABHPlayerController>(Target->GetController());
	const float FocusHeight = FMath::Clamp(Variant.FocusHeight, 80.0f, 320.0f);
	const int32 EffectiveScareIntensity = GetEffectiveScareIntensity();
	const float Speed = bPartyPace ? 10800.0f : FMath::Lerp(9000.0f, 10300.0f, static_cast<float>(EffectiveScareIntensity) / 3.0f);
	const float HoldDuration = bPartyPace ? 2.15f : FMath::Lerp(2.65f, 3.25f, static_cast<float>(EffectiveScareIntensity) / 3.0f);

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
	const FVector Directions[] = { Forward, Right, -Right, -Forward };
	const float Distances[] = { 4600.0f, 3800.0f, 3000.0f, 2200.0f, 1500.0f };
	for (const FVector& DirectionCandidate : Directions)
	{
		if (DirectionCandidate.IsNearlyZero())
		{
			continue;
		}

		for (float Distance : Distances)
		{
			FVector Candidate = TargetLocation + DirectionCandidate * Distance;
			Candidate.X = FMath::Clamp(Candidate.X, -MaxX, MaxX);
			Candidate.Y = FMath::Clamp(Candidate.Y, -MaxY, MaxY);
			Candidates.Add(Candidate);
		}
	}

	FBHResolvedJumpscareSpawn ResolvedSpawn;
	if (!ResolveVisibleJumpscareSpawn(Target, TargetPC, Candidates, FocusHeight, 900.0f, 6400.0f, 70.0f, ResolvedSpawn))
	{
		TriggerCloseOverlayJumpscare(Target, Variant, Message, HoldDuration, FMath::Min(FearAmount, 22.0f), FMath::Min(DreadAmount, 24.0f));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABHJumpscareMonster* Monster = GetWorld()->SpawnActor<ABHJumpscareMonster>(ResolvedSpawn.SpawnLocation, ResolvedSpawn.SpawnRotation, SpawnParams))
	{
		Monster->Configure(Target, Speed, bPartyPace ? 7.4f : 8.6f, HoldDuration);
		Monster->ConfigureVariant(Variant);
	}
	else
	{
		TriggerCloseOverlayJumpscare(Target, Variant, Message, HoldDuration, FMath::Min(FearAmount, 22.0f), FMath::Min(DreadAmount, 24.0f));
		return;
	}

	RecordPlaytestTelemetryMarker(TEXT("jumpscare"), TargetLocation, FString::Printf(TEXT("monster_charge variant=%s"), *Variant.VariantId.ToString()), nullptr, Target->GetPlayerState<ABHPlayerState>());
	SendJumpscareChargeCue(Target, Variant, ResolvedSpawn.FocusLocation, Message, HoldDuration, 1.0f, false);
	FreezeTargetForJumpscare(Target, HoldDuration);
	CutLightsForJumpscare(TargetLocation, ResolvedSpawn.SpawnLocation, 0.0f, bPartyPace ? 9.0f : 10.25f);

	LastMonsterChargeTime = GetWorld()->GetTimeSeconds();
	Target->AddFear(FearAmount);
	Target->AddDread(DreadAmount);

	for (ABHFlickerLight* Light : FlickerLights)
	{
		if (Light && Light->GetCircuitId() > 0 && FVector::DistSquared2D(Light->GetActorLocation(), TargetLocation) <= FMath::Square(2400.0f) && FMath::FRand() < 0.34f)
		{
			Light->SetPowered(false);
		}
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
	RestoreDelegate.BindLambda([WeakTarget, WeakPC]()
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
	RestoreDelegate.BindLambda([AffectedLights = MoveTemp(AffectedLights), OriginalPoweredStates = MoveTemp(OriginalPoweredStates)]() mutable
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

void ABHGameMode::UpdateExitUnlockState()
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
				BHGS->SetPresenceState(FMath::Max(BHGS->PresenceLevel, 54.0f), TEXT("The Teacher is holding a correction conference."), BHGS->PresencePulse + 1);
				UpdateDirectorGameState(GetRevisionObjectiveText());
				BroadcastStatus(RevisionBlockReason, 5.0f);
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
	BotApproachPointCache.Reset();
	LoggedBotTacticalWarnings.Reset();
	TelemetryUsedLockerKeys.Reset();
	TelemetryStartedObjectiveKeys.Reset();
	TelemetryCompletedObjectiveKeys.Reset();
	TelemetrySnapshotKeys.Reset();

	if (ABHGameState* BHGS = GetGameState<ABHGameState>())
	{
		BHGS->SetRoundPhase(EBHRoundPhase::Prep);
		BHGS->SetRemainingTime(PrepSeconds);
		BHGS->SetBreakerCounts(0, ActiveBreakerCount);
		BHGS->SetSideObjectiveCounts(0, ActiveSideObjectiveCount);
		BHGS->SetExitUnlocked(false);
		UpdateStationSignalState(false);
		UpdateDirectorGameState(TEXT("Role warmup: try flashlight, lockers, questions, decoys, scans, captures, and Hall Monitor tools. The live Hunt resets everyone."));
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
	BotApproachPointCache.Reset();
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
	StartDirectorTimer();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABHPlayerController* PC = Cast<ABHPlayerController>(It->Get()))
		{
			PC->ClientShowStatusMessage(TEXT("Hunt started. Answer questions, finish tasks, and hide before the Teacher learns your route."), 4.0f);
		}
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

	if (AssignedSurvivors == 0 && Players.Num() > 1)
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
		if (CountEscapedSurvivors() > 0)
		{
			EndRound(EBHRoundPhase::SurvivorsWin);
		}
		else if (CountAliveSurvivors() <= 0 || BHGS->RemainingTime <= 0)
		{
			EndRound(EBHRoundPhase::HunterWin);
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
	GetWorldTimerManager().SetTimer(RoundTimerHandle, this, &ABHGameMode::ResetRoundByTravel, 8.0f, false);
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
		FString TravelURL = FString::Printf(TEXT("/Engine/Maps/Entry?listen?BHLevel=%s?BHFogPreset=%s"),
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
		}
		GetWorld()->ServerTravel(TravelURL);
	}
}

int32 ABHGameMode::GetConfiguredStageIndex() const
{
	if (const UBHGameInstance* BHGI = GetGameInstance<UBHGameInstance>())
	{
		return FMath::Clamp(BHGI->GetPersistentStageIndex(), 0, 2);
	}
	return 0;
}

FString ABHGameMode::GetDefaultMapForStage(int32 StageIndex) const
{
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

FString ABHGameMode::BuildTravelOptionsForLevel(const FString& LevelName, bool bIntermission, int32 StageIndex, EBHRoundPhase ResultPhase) const
{
	const FString NormalizedLevel = bIntermission ? TEXT("TrainIntermission") : NormalizeBHLevelName(LevelName);
	FString TravelURL = FString::Printf(TEXT("/Engine/Maps/Entry?listen?BHFogPreset=%s?BHStageIndex=%d"),
		*FogPresetToString(NextFogPreset),
		FMath::Clamp(StageIndex, 0, 2));

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
