#include "BHCreateDiagramMaterialCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BHCreateDiagramMaterialCommandlet)

UBHCreateDiagramMaterialCommandlet::UBHCreateDiagramMaterialCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Creates the M_BH_DiagramRT unlit emissive material (texture param 'RT') for the bonus terminal.");
	HelpUsage = TEXT("BlackoutHuntEditor-Cmd.exe BlackoutHunt.uproject -run=BHCreateDiagramMaterial");
}

int32 UBHCreateDiagramMaterialCommandlet::Main(const FString& Params)
{
#if !WITH_EDITOR
	UE_LOG(LogTemp, Error, TEXT("BHCreateDiagramMaterial requires an editor target."));
	return 1;
#else
	const FString PackagePath = TEXT("/Game/BlackoutHunt/Art/Diagrams/M_BH_DiagramRT");

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHCreateDiagramMaterial] Could not create package %s."), *PackagePath);
		return 1;
	}
	Package->FullyLoad();

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	if (!Factory)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHCreateDiagramMaterial] Failed to create the material factory."));
		return 1;
	}
	UMaterial* Material = Cast<UMaterial>(Factory->FactoryCreateNew(UMaterial::StaticClass(), Package, TEXT("M_BH_DiagramRT"), RF_Standalone | RF_Public, nullptr, GWarn));
	if (!Material)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHCreateDiagramMaterial] Failed to create the material object."));
		return 1;
	}

	// Unlit "screen" material: the render target drives Emissive Color directly so the terminal glows
	// regardless of scene lighting.
	Material->MaterialDomain = MD_Surface;
	Material->SetShadingModel(MSM_Unlit);

	UMaterialExpressionTextureSampleParameter2D* TexParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -350, 0));
	if (!TexParam)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHCreateDiagramMaterial] Failed to create the texture-sample parameter."));
		return 1;
	}
	TexParam->ParameterName = TEXT("RT");
	TexParam->SamplerType = SAMPLERTYPE_LinearColor;
	// A default texture keeps the material valid before the dynamic instance binds the render target.
	TexParam->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture"));

	UMaterialEditingLibrary::ConnectMaterialProperty(TexParam, FString(), MP_EmissiveColor);
	UMaterialEditingLibrary::RecompileMaterial(Material);

	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone | RF_Public;
	SaveArgs.SaveFlags = SAVE_NoError;
	const bool bSaved = UPackage::SavePackage(Package, Material, *FileName, SaveArgs);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Error, TEXT("[BHCreateDiagramMaterial] Failed to save %s."), *FileName);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("[BHCreateDiagramMaterial] Saved %s (unlit emissive, texture param 'RT')."), *PackagePath);
	return 0;
#endif
}
