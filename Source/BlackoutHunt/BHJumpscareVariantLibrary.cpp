// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHJumpscareVariantLibrary.h"
#include "BHGameSettings.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundBase.h"

namespace
{
constexpr TCHAR BHWhisperJumpscareRoot[] = TEXT("/Game/BlackoutHunt/Art/Jumpscares/Whisper");
constexpr TCHAR BHFallbackJumpscareAudioPath[] = TEXT("/Game/BlackoutHunt/Audio/SW_TerrifiedScreamFaint.SW_TerrifiedScreamFaint");

struct FBHDiscoveredWhisperAsset
{
	FAssetData AssetData;
	FSoftObjectPath ObjectPath;
	FString PackagePath;
	FString AssetName;
	FString NormalizedName;
	FString Digits;
	bool bSkeletalMesh = false;
	bool bStaticMesh = false;
};

struct FBHWhisperDiscoveryBuckets
{
	TArray<FBHDiscoveredWhisperAsset> SkeletalMeshes;
	TArray<FBHDiscoveredWhisperAsset> StaticMeshes;
	TArray<FBHDiscoveredWhisperAsset> Materials;
	TArray<FBHDiscoveredWhisperAsset> Animations;
	TArray<FBHDiscoveredWhisperAsset> Sounds;
};

bool BHAssetClassIs(const FAssetData& AssetData, const TCHAR* ClassName)
{
	return AssetData.AssetClassPath.GetAssetName() == FName(ClassName);
}

bool BHAssetClassIsMaterial(const FAssetData& AssetData)
{
	const FName ClassName = AssetData.AssetClassPath.GetAssetName();
	return ClassName == FName(TEXT("Material"))
		|| ClassName == FName(TEXT("MaterialInstance"))
		|| ClassName == FName(TEXT("MaterialInstanceConstant"));
}

bool BHAssetClassIsSound(const FAssetData& AssetData)
{
	const FName ClassName = AssetData.AssetClassPath.GetAssetName();
	return ClassName == FName(TEXT("SoundWave"))
		|| ClassName == FName(TEXT("SoundCue"))
		|| ClassName == FName(TEXT("MetaSoundSource"));
}

FString BHNormalizeWhisperAssetName(const FString& RawName)
{
	FString Result = RawName.ToLower();
	const TCHAR* Prefixes[] = {
		TEXT("skm_"),
		TEXT("sk_"),
		TEXT("sm_"),
		TEXT("mi_"),
		TEXT("m_"),
		TEXT("mat_"),
		TEXT("as_"),
		TEXT("a_"),
		TEXT("anim_"),
		TEXT("sw_"),
		TEXT("wav_"),
		TEXT("cue_"),
		TEXT("s_")
	};

	bool bRemovedPrefix = true;
	while (bRemovedPrefix)
	{
		bRemovedPrefix = false;
		for (const TCHAR* Prefix : Prefixes)
		{
			if (Result.StartsWith(Prefix))
			{
				Result.RightChopInline(FCString::Strlen(Prefix), EAllowShrinking::No);
				bRemovedPrefix = true;
				break;
			}
		}
	}

	Result.ReplaceInline(TEXT("_"), TEXT(""));
	Result.ReplaceInline(TEXT("-"), TEXT(""));
	Result.ReplaceInline(TEXT(" "), TEXT(""));
	Result.ReplaceInline(TEXT("whisper"), TEXT(""));
	return Result;
}

FString BHDigitsInName(const FString& Name)
{
	FString Result;
	for (const TCHAR Character : Name)
	{
		if (FChar::IsDigit(Character))
		{
			Result.AppendChar(Character);
		}
	}
	return Result;
}

FString BHMakeWhisperDisplayLabel(const FString& AssetName, int32 OneBasedIndex)
{
	FString Label = AssetName;
	const TCHAR* Prefixes[] = {
		TEXT("SKM_"),
		TEXT("SK_"),
		TEXT("SM_"),
		TEXT("BP_"),
		TEXT("Whisper_"),
		TEXT("Whisper")
	};

	bool bRemovedPrefix = true;
	while (bRemovedPrefix)
	{
		bRemovedPrefix = false;
		for (const TCHAR* Prefix : Prefixes)
		{
			if (Label.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				Label.RightChopInline(FCString::Strlen(Prefix), EAllowShrinking::No);
				bRemovedPrefix = true;
				break;
			}
		}
	}

	Label.ReplaceInline(TEXT("_"), TEXT(" "));
	Label.ReplaceInline(TEXT("-"), TEXT(" "));
	Label.TrimStartAndEndInline();
	if (Label.IsEmpty())
	{
		Label = TEXT("Entity");
	}
	return FString::Printf(TEXT("Whisper %02d - %s"), OneBasedIndex, *Label);
}

int32 BHCommonPrefixLength(const FString& A, const FString& B)
{
	const int32 Limit = FMath::Min(A.Len(), B.Len());
	int32 Count = 0;
	for (; Count < Limit; ++Count)
	{
		if (A[Count] != B[Count])
		{
			break;
		}
	}
	return Count;
}

bool BHStaticMeshLooksLikeWhisperEntity(const FBHDiscoveredWhisperAsset& Asset, bool bHasSkeletalMeshes)
{
	if (!Asset.bStaticMesh)
	{
		return false;
	}
	if (!bHasSkeletalMeshes)
	{
		return true;
	}

	const FString SearchText = (Asset.AssetName + TEXT("/") + Asset.PackagePath).ToLower();
	return SearchText.Contains(TEXT("whisper"))
		|| SearchText.Contains(TEXT("monster"))
		|| SearchText.Contains(TEXT("entity"))
		|| SearchText.Contains(TEXT("creature"))
		|| SearchText.Contains(TEXT("ghost"))
		|| SearchText.Contains(TEXT("scare"))
		|| SearchText.Contains(TEXT("jumpscare"));
}

FBHDiscoveredWhisperAsset BHMakeDiscoveredAsset(const FAssetData& AssetData)
{
	FBHDiscoveredWhisperAsset Asset;
	Asset.AssetData = AssetData;
	Asset.ObjectPath = AssetData.GetSoftObjectPath();
	Asset.PackagePath = AssetData.PackagePath.ToString();
	Asset.AssetName = AssetData.AssetName.ToString();
	Asset.NormalizedName = BHNormalizeWhisperAssetName(Asset.AssetName);
	Asset.Digits = BHDigitsInName(Asset.AssetName);
	Asset.bSkeletalMesh = BHAssetClassIs(AssetData, TEXT("SkeletalMesh"));
	Asset.bStaticMesh = BHAssetClassIs(AssetData, TEXT("StaticMesh"));
	return Asset;
}

void BHSortDiscoveredAssets(TArray<FBHDiscoveredWhisperAsset>& Assets)
{
	Assets.Sort([](const FBHDiscoveredWhisperAsset& Left, const FBHDiscoveredWhisperAsset& Right)
	{
		return Left.ObjectPath.ToString() < Right.ObjectPath.ToString();
	});
}

int32 BHWhisperPairScore(const FBHDiscoveredWhisperAsset& Mesh, const FBHDiscoveredWhisperAsset& Candidate)
{
	int32 Score = 0;
	if (Candidate.PackagePath == Mesh.PackagePath)
	{
		Score += 10000;
	}
	else if (Candidate.PackagePath.StartsWith(Mesh.PackagePath) || Mesh.PackagePath.StartsWith(Candidate.PackagePath))
	{
		Score += 3000;
	}

	if (!Mesh.NormalizedName.IsEmpty() && Mesh.NormalizedName == Candidate.NormalizedName)
	{
		Score += 8000;
	}
	else if (!Mesh.NormalizedName.IsEmpty() && !Candidate.NormalizedName.IsEmpty()
		&& (Mesh.NormalizedName.Contains(Candidate.NormalizedName) || Candidate.NormalizedName.Contains(Mesh.NormalizedName)))
	{
		Score += 4000;
	}

	if (!Mesh.Digits.IsEmpty() && Mesh.Digits == Candidate.Digits)
	{
		Score += 2500;
	}

	Score += BHCommonPrefixLength(Mesh.NormalizedName, Candidate.NormalizedName) * 25;
	return Score;
}

const FBHDiscoveredWhisperAsset* BHFindBestPairedAsset(const FBHDiscoveredWhisperAsset& Mesh, const TArray<FBHDiscoveredWhisperAsset>& Candidates)
{
	const FBHDiscoveredWhisperAsset* BestAsset = nullptr;
	int32 BestScore = TNumericLimits<int32>::Lowest();
	for (const FBHDiscoveredWhisperAsset& Candidate : Candidates)
	{
		const int32 Score = BHWhisperPairScore(Mesh, Candidate);
		if (!BestAsset || Score > BestScore || (Score == BestScore && Candidate.ObjectPath.ToString() < BestAsset->ObjectPath.ToString()))
		{
			BestAsset = &Candidate;
			BestScore = Score;
		}
	}
	return BestAsset;
}

FBHWhisperDiscoveryBuckets BHDiscoverWhisperAssets()
{
	FBHWhisperDiscoveryBuckets Buckets;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

#if WITH_EDITOR
	TArray<FString> ScanPaths;
	ScanPaths.Add(BHWhisperJumpscareRoot);
	AssetRegistry.ScanPathsSynchronous(ScanPaths, false);
#endif

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(BHWhisperJumpscareRoot));
	Filter.bRecursivePaths = true;
	// On-disk registry state only: in-memory enumeration walks every live UObject (GetAssetRegistryTags on
	// each) and adds nothing here — imported Whisper art is always in the cooked registry, and the editor
	// path rescans the root above. It also ran at boot (menu construction resolves variants during the
	// first LoadMap), where it was the first victim of the 2026-06 Development heap-corruption crash; see
	// the regression note in Docs/CLASSROOM_DEPLOYMENT.md.
	Filter.bIncludeOnlyOnDiskAssets = true;

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	for (const FAssetData& AssetData : Assets)
	{
		if (!AssetData.IsValid() || BHAssetClassIs(AssetData, TEXT("ObjectRedirector")))
		{
			continue;
		}

		FBHDiscoveredWhisperAsset Asset = BHMakeDiscoveredAsset(AssetData);
		if (Asset.bSkeletalMesh)
		{
			Buckets.SkeletalMeshes.Add(Asset);
		}
		else if (Asset.bStaticMesh)
		{
			Buckets.StaticMeshes.Add(Asset);
		}
		else if (BHAssetClassIsMaterial(AssetData))
		{
			Buckets.Materials.Add(Asset);
		}
		else if (BHAssetClassIs(AssetData, TEXT("AnimSequence")))
		{
			Buckets.Animations.Add(Asset);
		}
		else if (BHAssetClassIsSound(AssetData))
		{
			Buckets.Sounds.Add(Asset);
		}
	}

	BHSortDiscoveredAssets(Buckets.SkeletalMeshes);
	BHSortDiscoveredAssets(Buckets.StaticMeshes);
	BHSortDiscoveredAssets(Buckets.Materials);
	BHSortDiscoveredAssets(Buckets.Animations);
	BHSortDiscoveredAssets(Buckets.Sounds);
	return Buckets;
}

void BHAppendConfiguredVariants(TArray<FBHJumpscareVariant>& OutVariants)
{
	const UBHGameSettings* Settings = GetDefault<UBHGameSettings>();
	if (!Settings)
	{
		return;
	}

	OutVariants.Append(Settings->JumpscareVariants);
}
}

bool IsWhisperJumpscareVariant(const FBHJumpscareVariant& Variant)
{
	return Variant.VariantId.ToString().StartsWith(TEXT("Whisper"), ESearchCase::IgnoreCase);
}

FBHJumpscareVariant MakeLegacyScp096ProxyJumpscareVariant()
{
	FBHJumpscareVariant Variant;
	Variant.VariantId = TEXT("SCP096");
	Variant.DisplayName = TEXT("SCP-096 Legacy Proxy");
	Variant.Weight = 0.0f;
	Variant.MinimumScareIntensity = 0;
	Variant.LaunchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(BHFallbackJumpscareAudioPath));
	Variant.VisualOffset = FVector(-20.0f, 0.0f, -88.0f);
	Variant.VisualRotation = FRotator(0.0f, -90.0f, 0.0f);
	Variant.VisualScale = FVector(1.0f);
	Variant.CloseVisualOffset = FVector(90.0f, 0.0f, -52.0f);
	Variant.CloseVisualRotation = FRotator::ZeroRotator;
	Variant.CloseVisualScale = FVector(1.55f);
	Variant.LightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);
	Variant.FocusHeight = 145.0f;
	Variant.CameraShakeIntensity = 0.96f;
	Variant.FlashIntensity = 0.70f;
	Variant.CameraJitterDuration = 1.10f;
	return Variant;
}

FBHJumpscareVariant MakeRealScp096JumpscareVariant()
{
	// The genuine imported SCP-096 creature (its own skeleton/anims, NOT the shared Monster skeleton). Built in
	// code and kept out of the shared JumpscareVariants pool so it only ever drives its own dedicated scare and
	// never alters the existing face / behind-you / peek / super / ambient picks.
	FBHJumpscareVariant Variant;
	Variant.VariantId = TEXT("Scp096Real");
	Variant.DisplayName = TEXT("SCP-096");
	Variant.Weight = 0.0f; // never auto-selected by the weighted pool; only triggered explicitly
	Variant.MinimumScareIntensity = 0;
	Variant.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096.SK_SCP096")));
	// Charge clip drives the sprint; the swipe clip is the close-up kill pose (its own skeleton, so it cannot
	// borrow the Monster_03 lunge). Both are bound to SK_SCP096_Skeleton.
	Variant.RunAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096C_096_AChrge_F.SK_SCP096C_096_AChrge_F")));
	Variant.CloseUpAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/SCP096/Skeletal/SK_SCP096C_096_ASwp1_F.SK_SCP096C_096_ASwp1_F")));
	Variant.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/SCP096/M_SCP096.M_SCP096")));
	Variant.LaunchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/BlackoutHunt/Art/Jumpscares/FreeCustomizableJumpscares/Sounds/SW_Jumpscare_01.SW_Jumpscare_01")));
	Variant.VisualOffset = FVector(0.0f, 0.0f, -92.0f);
	Variant.VisualRotation = FRotator(0.0f, -90.0f, 0.0f);
	// The source FBX is authored in metres, so it imports ~100x too small (~2.83 units tall). Pre-scale it
	// back to a real ~2.8 m creature; the height-fitter then normalises it to the looming target. Without this
	// the upscale cap leaves it a few centimetres tall and effectively invisible.
	Variant.VisualScale = FVector(100.0f);
	Variant.CloseVisualOffset = FVector(82.0f, 0.0f, -52.0f);
	Variant.CloseVisualRotation = FRotator::ZeroRotator;
	Variant.CloseVisualScale = FVector(1.32f);
	Variant.LightColor = FLinearColor(1.0f, 0.02f, 0.0f, 1.0f);
	Variant.FocusHeight = 172.0f;
	Variant.CameraShakeIntensity = 1.0f;
	Variant.FlashIntensity = 0.92f;
	Variant.CameraJitterDuration = 1.30f;
	Variant.ImpactFOVPunch = 16.0f;
	Variant.ImpactHitStopSeconds = 0.08f;
	Variant.ImpactRumbleIntensity = 0.92f;
	return Variant;
}

TArray<FBHJumpscareVariant> GetResolvedWhisperJumpscareVariants()
{
	FBHWhisperDiscoveryBuckets Discovery = BHDiscoverWhisperAssets();
	const bool bHasSkeletalMeshes = !Discovery.SkeletalMeshes.IsEmpty();
	Discovery.StaticMeshes.RemoveAll([bHasSkeletalMeshes](const FBHDiscoveredWhisperAsset& Asset)
	{
		return !BHStaticMeshLooksLikeWhisperEntity(Asset, bHasSkeletalMeshes);
	});

	TArray<FBHDiscoveredWhisperAsset> Meshes;
	Meshes.Append(Discovery.SkeletalMeshes);
	Meshes.Append(Discovery.StaticMeshes);
	BHSortDiscoveredAssets(Meshes);

	TArray<FBHJumpscareVariant> Variants;
	Variants.Reserve(Meshes.Num());
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		const FBHDiscoveredWhisperAsset& Mesh = Meshes[Index];
		FBHJumpscareVariant Variant;
		Variant.VariantId = FName(*FString::Printf(TEXT("Whisper%02d"), Index + 1));
		Variant.DisplayName = BHMakeWhisperDisplayLabel(Mesh.AssetName, Index + 1);
		Variant.Weight = 1.05f;
		Variant.MinimumScareIntensity = 1;
		if (Mesh.bSkeletalMesh)
		{
			Variant.SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(Mesh.ObjectPath);
		}
		else
		{
			Variant.StaticMesh = TSoftObjectPtr<UStaticMesh>(Mesh.ObjectPath);
		}

		if (const FBHDiscoveredWhisperAsset* Material = BHFindBestPairedAsset(Mesh, Discovery.Materials))
		{
			Variant.Material = TSoftObjectPtr<UMaterialInterface>(Material->ObjectPath);
		}
		if (Mesh.bSkeletalMesh)
		{
			if (const FBHDiscoveredWhisperAsset* Animation = BHFindBestPairedAsset(Mesh, Discovery.Animations))
			{
				Variant.RunAnimation = TSoftObjectPtr<UAnimSequence>(Animation->ObjectPath);
			}
		}
		if (const FBHDiscoveredWhisperAsset* Sound = BHFindBestPairedAsset(Mesh, Discovery.Sounds))
		{
			Variant.LaunchSound = TSoftObjectPtr<USoundBase>(Sound->ObjectPath);
		}
		else
		{
			Variant.LaunchSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(BHFallbackJumpscareAudioPath));
		}

		Variant.VisualOffset = FVector(0.0f, 0.0f, -92.0f);
		Variant.VisualRotation = FRotator(0.0f, -90.0f, 0.0f);
		Variant.VisualScale = FVector(1.08f);
		Variant.CloseVisualOffset = FVector(82.0f, 0.0f, -52.0f);
		Variant.CloseVisualRotation = FRotator::ZeroRotator;
		Variant.CloseVisualScale = FVector(1.46f);
		Variant.LightColor = FLinearColor(0.64f, 0.78f, 1.0f, 1.0f);
		Variant.FocusHeight = 158.0f;
		Variant.CameraShakeIntensity = 0.86f;
		Variant.FlashIntensity = 0.52f;
		Variant.CameraJitterDuration = 1.05f;
		Variants.Add(Variant);
	}

	if (Variants.IsEmpty())
	{
		// Announce the "works in editor, absent in package" trap: whisper art that exists in Content but
		// was stripped from the cook (a DirectoriesToNeverCook entry, or an asset-registry miss) resolves
		// to zero variants here with no other symptom — the whisper scare pool just silently never grows.
		// Warn once per process so a cooked session log states WHY the whisper scares are missing.
		static bool bWarnedNoWhisperVariants = false;
		if (!bWarnedNoWhisperVariants)
		{
			bWarnedNoWhisperVariants = true;
			UE_LOG(LogTemp, Warning,
				TEXT("BlackoutHunt Jumpscare: whisper discovery resolved ZERO variants under %s. If whisper art has been imported, verify the folder is actually cooked (not in DirectoriesToNeverCook) and present in the on-disk asset registry."),
				BHWhisperJumpscareRoot);
		}
	}

	return Variants;
}

TArray<FBHJumpscareVariant> GetResolvedJumpscareVariants()
{
	TArray<FBHJumpscareVariant> Variants;
	BHAppendConfiguredVariants(Variants);

	TSet<FName> ExistingIds;
	for (const FBHJumpscareVariant& Variant : Variants)
	{
		if (!Variant.VariantId.IsNone())
		{
			ExistingIds.Add(Variant.VariantId);
		}
	}

	for (const FBHJumpscareVariant& WhisperVariant : GetResolvedWhisperJumpscareVariants())
	{
		if (!ExistingIds.Contains(WhisperVariant.VariantId))
		{
			Variants.Add(WhisperVariant);
			ExistingIds.Add(WhisperVariant.VariantId);
		}
	}

	return Variants;
}

bool FindResolvedJumpscareVariantById(FName VariantId, FBHJumpscareVariant& OutVariant)
{
	if (VariantId.IsNone())
	{
		return false;
	}

	if (VariantId == FName(TEXT("SCP096")))
	{
		OutVariant = MakeLegacyScp096ProxyJumpscareVariant();
		return true;
	}

	for (const FBHJumpscareVariant& Variant : GetResolvedJumpscareVariants())
	{
		if (Variant.VariantId == VariantId)
		{
			OutVariant = Variant;
			return true;
		}
	}

	return false;
}
