// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHAccountSubsystem.h"

#include "BHAccountSettings.h"
#include "BHPlayerController.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/AES.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 BHMinLocalUsernameChars = 3;
	constexpr int32 BHMaxLocalUsernameChars = 32;
	constexpr int32 BHMinDisplayNameChars = 2;
	constexpr int32 BHMaxDisplayNameChars = 24;
	constexpr int32 BHMinLocalPasswordChars = 8;
	constexpr int32 BHMaxLocalPasswordChars = 128;

	FString TrimTrailingSlash(FString Url)
	{
		Url.TrimStartAndEndInline();
		Url.TrimQuotesInline();
		while (Url.EndsWith(TEXT("/")))
		{
			Url.LeftChopInline(1);
		}
		return Url;
	}

	FString NormalizeBackendBaseUrl(FString Url)
	{
		Url = TrimTrailingSlash(Url);
		if (Url.IsEmpty() || Url.Equals(TEXT("http:"), ESearchCase::IgnoreCase) || Url.Equals(TEXT("https:"), ESearchCase::IgnoreCase))
		{
			return FString();
		}

		if (!Url.Contains(TEXT("://")))
		{
			// Default to HTTPS: this base URL carries the bearer session token and player identity,
			// so a scheme-less operator-entered host must not silently downgrade to cleartext.
			Url = FString(TEXT("https://")) + Url;
		}

		return TrimTrailingSlash(Url);
	}

	// Highest progress schema version this build understands. If a save declares a higher version we
	// refuse to parse it with the old schema and refuse to overwrite it (avoids silently wiping a
	// newer build's data when an older build runs against the same profile).
	// v2 added unlocked_achievements (cosmetic achievements + the hidden prestige tints they unlock).
	constexpr int32 BHCurrentProgressVersion = 2;

	// Cosmetic achievement registry. Earning one awards XP (so achievements feed the same economy as play) and
	// unlocks any tint gated on its id (see BHCosmeticUnlocks.cpp). Purely cosmetic -- never affects gameplay.
	struct FBHAchievementDef
	{
		const TCHAR* Id;
		const TCHAR* Title;
		const TCHAR* Description;
		int32 XPReward;
		int32 Difficulty;          // 1..5 -- the badge's star meter + tier colour in the Achievements tab
		const TCHAR* RewardLabel;  // cosmetic reward shown on the badge ("" if the reward is XP only)
		bool bHidden;              // a secret achievement: the tab shows "???" until it is earned
	};
	const FBHAchievementDef GBHAchievements[] = {
		{ TEXT("honorary_faculty"), TEXT("Honorary Faculty"), TEXT("Played under a famous physicist's name."), 40, 1, TEXT("Chalk tint"),                  true  },
		{ TEXT("codebreaker"),      TEXT("Codebreaker"),       TEXT("Found the code that asks for nothing."),  30, 2, TEXT("Arcade tint"),                 true  },
		{ TEXT("escape_artist"),    TEXT("Escape Artist"),     TEXT("Reached the exit and made it out."),      60, 2, TEXT("Exit Sign tint"),              false },
		{ TEXT("survivor"),         TEXT("Last One Standing"), TEXT("Won a Hunt as a survivor."),              50, 3, TEXT("Suit outfit"),                 false },
		{ TEXT("spelunker"),        TEXT("Spelunker"),         TEXT("Hid in a locker. The dark is patient."),  20, 1, TEXT(""),                            false },
		{ TEXT("perfect_chain"),    TEXT("Perfect Chain"),     TEXT("Nailed a frame-perfect momentum chain."), 80, 5, TEXT("Afterimage tint + Spacesuit"), false },
		// Newer achievements (rewards: Veteran / Faculty tints + the Top Hat headwear). See Docs/EASTER_EGGS.md.
		{ TEXT("veteran"),          TEXT("Veteran"),           TEXT("Played 25 rounds. A familiar face."),     60, 2, TEXT("Veteran tint"),                false },
		{ TEXT("top_of_the_class"), TEXT("Top of the Class"),  TEXT("Won a Hunt as the Teacher."),             50, 3, TEXT("Faculty tint"),                false },
		{ TEXT("roof_rider"),       TEXT("Roof Rider"),        TEXT("Found a way onto the train roof."),       70, 3, TEXT("Top Hat"),                     true  },
		// Skill / Teacher / Exploration / Dedication achievements. Rewards: tints + Crown / Halo / Graduation Cap.
		{ TEXT("flow_master"),      TEXT("Flow Master"),       TEXT("Chained a full three-link flow chain."),  90, 5, TEXT("Slipstream tint"),             false },
		{ TEXT("first_blood"),      TEXT("First Blood"),       TEXT("Captured your first survivor as Teacher."), 40, 2, TEXT("Detention tint"),            false },
		{ TEXT("flawless_hunt"),    TEXT("Flawless Hunt"),     TEXT("Caught every survivor in one round."),    75, 4, TEXT("Apex tint"),                   false },
		{ TEXT("tourist"),          TEXT("Tourist"),           TEXT("Tried all four train activities."),       45, 2, TEXT("Commuter tint"),               false },
		{ TEXT("completionist"),    TEXT("Completionist"),     TEXT("Found every hidden easter egg."),         120, 5, TEXT("Halo"),                       true  },
		{ TEXT("centurion"),        TEXT("Centurion"),         TEXT("Played 100 rounds."),                     100, 4, TEXT("Graduation Cap"),             false },
		{ TEXT("on_a_roll"),        TEXT("On a Roll"),         TEXT("Won three rounds in a row."),             70, 3, TEXT("Crown"),                       false }
	};
	const FBHAchievementDef* BHFindAchievement(FName Id)
	{
		for (const FBHAchievementDef& Def : GBHAchievements)
		{
			if (Id == FName(Def.Id))
			{
				return &Def;
			}
		}
		return nullptr;
	}

	FString JsonObjectToString(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Output;
	}

	bool StringToJsonObject(const FString& Input, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Input);
		return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
	}

	// Crash-safe write: serialize to "<path>.tmp", rotate the current file to "<path>.bak", then move
	// the temp into place. A power loss / crash mid-write can only corrupt the throwaway .tmp; the
	// live file and its backup are never both truncated, so progress/account data is never lost.
	bool AtomicSaveStringToFile(const FString& Contents, const FString& Path)
	{
		IFileManager& FileManager = IFileManager::Get();
		const FString TempPath = Path + TEXT(".tmp");
		const FString BackupPath = Path + TEXT(".bak");

		if (!FFileHelper::SaveStringToFile(Contents, *TempPath))
		{
			return false;
		}

		if (FileManager.FileExists(*Path))
		{
			// Keep one rollback copy; ignore failure (target may simply not exist yet).
			FileManager.Move(*BackupPath, *Path, /*bReplace=*/true, /*bEvenIfReadOnly=*/true);
		}

		if (!FileManager.Move(*Path, *TempPath, /*bReplace=*/true, /*bEvenIfReadOnly=*/true))
		{
			// Could not commit the temp into place; leave the prior file/backup intact.
			FileManager.Delete(*TempPath, false, true, true);
			return false;
		}

		return true;
	}

	// Read a file, falling back to its ".bak" rollback if the primary is missing or fails to parse.
	// Returns the parsed object via OutJsonObject; false only if neither copy yields valid JSON.
	bool LoadJsonWithBackup(const FString& Path, TSharedPtr<FJsonObject>& OutJsonObject)
	{
		FString Input;
		if (FFileHelper::LoadFileToString(Input, *Path) && StringToJsonObject(Input, OutJsonObject))
		{
			return true;
		}

		const FString BackupPath = Path + TEXT(".bak");
		if (FFileHelper::LoadFileToString(Input, *BackupPath) && StringToJsonObject(Input, OutJsonObject))
		{
			UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt account data: primary %s was unreadable; recovered from backup."), *Path);
			return true;
		}

		return false;
	}

	FString JsonString(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName)
	{
		if (!JsonObject.IsValid())
		{
			return FString();
		}

		FString Value;
		JsonObject->TryGetStringField(FieldName, Value);
		return Value;
	}

	int32 JsonInt(const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const int32 DefaultValue = 0)
	{
		if (!JsonObject.IsValid())
		{
			return DefaultValue;
		}

		int32 Value = DefaultValue;
		JsonObject->TryGetNumberField(FieldName, Value);
		return Value;
	}

	FString UtcNowString()
	{
		return FDateTime::UtcNow().ToIso8601();
	}

	struct FBHLocalCredentialRecord
	{
		FString Username;
		FString PlayerId;
		FString PasswordSalt;
		FString PasswordHash;
		FString CreatedUtc;
		FString LastLoginUtc;
	};

	TArray<uint8> StringToUtf8Bytes(const FString& Input)
	{
		FTCHARToUTF8 Converter(*Input);
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
		return Bytes;
	}

	FString Utf8BytesToString(const TArray<uint8>& Bytes)
	{
		if (Bytes.IsEmpty())
		{
			return FString();
		}

		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString(Converter.Length(), Converter.Get());
	}

	void HashString(const FString& Input, uint8 OutHash[FSHA1::DigestSize])
	{
		const TArray<uint8> Bytes = StringToUtf8Bytes(Input);
		FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), OutHash);
	}

	TArray<uint8> MakeDerivedBytes(const FString& Seed, int32 NumBytes)
	{
		TArray<uint8> Output;
		Output.Reserve(NumBytes);

		for (int32 Counter = 0; Output.Num() < NumBytes; ++Counter)
		{
			uint8 Hash[FSHA1::DigestSize];
			HashString(FString::Printf(TEXT("%s|%d"), *Seed, Counter), Hash);
			Output.Append(Hash, FMath::Min<int32>(FSHA1::DigestSize, NumBytes - Output.Num()));
		}

		return Output;
	}

	FString CredentialMachineSeed()
	{
		FString LoginId = FPlatformMisc::GetLoginId();
		if (LoginId.IsEmpty())
		{
			LoginId = FPlatformProcess::UserName(false);
		}

		return FString::Printf(
			TEXT("BlackoutHunt.LocalCredentials.v1|%s|%s"),
			*LoginId,
			*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	}

	FAES::FAESKey MakeCredentialAesKey()
	{
		const TArray<uint8> KeyBytes = MakeDerivedBytes(CredentialMachineSeed() + TEXT("|aes"), FAES::FAESKey::KeySize);
		FAES::FAESKey Key;
		FMemory::Memcpy(Key.Key, KeyBytes.GetData(), FAES::FAESKey::KeySize);
		return Key;
	}

	TArray<uint8> MakeCredentialMacKey()
	{
		return MakeDerivedBytes(CredentialMachineSeed() + TEXT("|mac"), 32);
	}

	TArray<uint8> MakeRandomBytes(int32 NumBytes)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve(NumBytes);

		while (Bytes.Num() < NumBytes)
		{
			const FGuid Guid = FGuid::NewGuid();
			const uint32 Parts[] = { Guid.A, Guid.B, Guid.C, Guid.D };
			const int32 BytesToCopy = FMath::Min<int32>(sizeof(Parts), NumBytes - Bytes.Num());
			Bytes.Append(reinterpret_cast<const uint8*>(Parts), BytesToCopy);
		}

		return Bytes;
	}

	FString SanitizeLocalUsername(FString Username)
	{
		Username.TrimStartAndEndInline();

		FString Sanitized;
		for (const TCHAR Character : Username)
		{
			if (FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.'))
			{
				Sanitized.AppendChar(Character);
			}
			else if (FChar::IsWhitespace(Character))
			{
				Sanitized.AppendChar(TEXT('_'));
			}
		}

		return Sanitized.Left(BHMaxLocalUsernameChars);
	}

	FString SanitizeDisplayName(FString DisplayName)
	{
		DisplayName.TrimStartAndEndInline();

		FString Sanitized;
		bool bLastWasSpace = false;
		for (const TCHAR Character : DisplayName)
		{
			if (FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.'))
			{
				Sanitized.AppendChar(Character);
				bLastWasSpace = false;
			}
			else if (FChar::IsWhitespace(Character) && !bLastWasSpace && !Sanitized.IsEmpty())
			{
				Sanitized.AppendChar(TEXT(' '));
				bLastWasSpace = true;
			}

			if (Sanitized.Len() >= BHMaxDisplayNameChars)
			{
				break;
			}
		}

		Sanitized.TrimStartAndEndInline();
		return Sanitized;
	}

	FString MakePasswordSalt()
	{
		const TArray<uint8> SaltBytes = MakeRandomBytes(16);
		return FBase64::Encode(SaltBytes);
	}

	FString MakePasswordHash(const FString& Username, const FString& Password, const FString& Salt)
	{
		TArray<uint8> State = StringToUtf8Bytes(FString::Printf(TEXT("%s|%s|%s"), *Username.ToLower(), *Password, *Salt));
		const TArray<uint8> SaltBytes = StringToUtf8Bytes(Salt);
		uint8 Digest[FSHA1::DigestSize];

		for (int32 Iteration = 0; Iteration < 4096; ++Iteration)
		{
			FSHA1 Hasher;
			Hasher.Update(SaltBytes.GetData(), SaltBytes.Num());
			Hasher.Update(State.GetData(), State.Num());
			Hasher.Update(Iteration);
			Hasher.Final();
			Hasher.GetHash(Digest);
			State.Reset();
			State.Append(Digest, FSHA1::DigestSize);
		}

		return BytesToHex(Digest, FSHA1::DigestSize).ToLower();
	}

	FString MakeLocalPlayerId(const FString& Username)
	{
		uint8 Hash[FSHA1::DigestSize];
		HashString(Username.ToLower() + TEXT("|BlackoutHuntLocalPlayer"), Hash);
		return FString::Printf(TEXT("local_%s_%s"), *Username.ToLower(), *BytesToHex(Hash, 4).ToLower());
	}

	TSharedRef<FJsonObject> LocalCredentialToJson(const FBHLocalCredentialRecord& Record)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		JsonObject->SetNumberField(TEXT("version"), 1);
		JsonObject->SetStringField(TEXT("username"), Record.Username);
		JsonObject->SetStringField(TEXT("player_id"), Record.PlayerId);
		JsonObject->SetStringField(TEXT("password_salt"), Record.PasswordSalt);
		JsonObject->SetStringField(TEXT("password_hash"), Record.PasswordHash);
		JsonObject->SetStringField(TEXT("created_utc"), Record.CreatedUtc);
		JsonObject->SetStringField(TEXT("last_login_utc"), Record.LastLoginUtc);
		return JsonObject;
	}

	bool ApplyLocalCredentialJson(const TSharedPtr<FJsonObject>& JsonObject, FBHLocalCredentialRecord& OutRecord)
	{
		if (!JsonObject.IsValid())
		{
			return false;
		}

		OutRecord.Username = JsonString(JsonObject, TEXT("username"));
		OutRecord.PlayerId = JsonString(JsonObject, TEXT("player_id"));
		OutRecord.PasswordSalt = JsonString(JsonObject, TEXT("password_salt"));
		OutRecord.PasswordHash = JsonString(JsonObject, TEXT("password_hash"));
		OutRecord.CreatedUtc = JsonString(JsonObject, TEXT("created_utc"));
		OutRecord.LastLoginUtc = JsonString(JsonObject, TEXT("last_login_utc"));
		return !OutRecord.Username.IsEmpty() && !OutRecord.PlayerId.IsEmpty() && !OutRecord.PasswordSalt.IsEmpty() && !OutRecord.PasswordHash.IsEmpty();
	}

	FString MakePayloadMac(const TArray<uint8>& Iv, const TArray<uint8>& Ciphertext)
	{
		TArray<uint8> MacInput;
		MacInput.Reserve(Iv.Num() + Ciphertext.Num());
		MacInput.Append(Iv);
		MacInput.Append(Ciphertext);

		const TArray<uint8> MacKey = MakeCredentialMacKey();
		uint8 Mac[FSHA1::DigestSize];
		FSHA1::HMACBuffer(MacKey.GetData(), MacKey.Num(), MacInput.GetData(), MacInput.Num(), Mac);
		return BytesToHex(Mac, FSHA1::DigestSize).ToLower();
	}

	bool EncryptCredentialBytes(const TArray<uint8>& Plaintext, TArray<uint8>& OutIv, TArray<uint8>& OutCiphertext)
	{
		OutIv = MakeRandomBytes(FAES::AESBlockSize);
		if (OutIv.Num() != FAES::AESBlockSize)
		{
			return false;
		}

		OutCiphertext = Plaintext;
		int32 PaddingBytes = FAES::AESBlockSize - (OutCiphertext.Num() % FAES::AESBlockSize);
		if (PaddingBytes <= 0)
		{
			PaddingBytes = FAES::AESBlockSize;
		}
		for (int32 Index = 0; Index < PaddingBytes; ++Index)
		{
			OutCiphertext.Add(static_cast<uint8>(PaddingBytes));
		}

		const FAES::FAESKey Key = MakeCredentialAesKey();
		uint8 PreviousBlock[FAES::AESBlockSize];
		FMemory::Memcpy(PreviousBlock, OutIv.GetData(), FAES::AESBlockSize);

		for (int32 Offset = 0; Offset < OutCiphertext.Num(); Offset += FAES::AESBlockSize)
		{
			uint8* Block = OutCiphertext.GetData() + Offset;
			for (int32 ByteIndex = 0; ByteIndex < FAES::AESBlockSize; ++ByteIndex)
			{
				Block[ByteIndex] ^= PreviousBlock[ByteIndex];
			}

			FAES::EncryptData(Block, FAES::AESBlockSize, Key);
			FMemory::Memcpy(PreviousBlock, Block, FAES::AESBlockSize);
		}

		return true;
	}

	bool DecryptCredentialBytes(const TArray<uint8>& Iv, const TArray<uint8>& Ciphertext, TArray<uint8>& OutPlaintext)
	{
		if (Iv.Num() != FAES::AESBlockSize || Ciphertext.IsEmpty() || Ciphertext.Num() % FAES::AESBlockSize != 0)
		{
			return false;
		}

		OutPlaintext = Ciphertext;
		const FAES::FAESKey Key = MakeCredentialAesKey();
		uint8 PreviousBlock[FAES::AESBlockSize];
		FMemory::Memcpy(PreviousBlock, Iv.GetData(), FAES::AESBlockSize);

		for (int32 Offset = 0; Offset < OutPlaintext.Num(); Offset += FAES::AESBlockSize)
		{
			uint8 CurrentCipherBlock[FAES::AESBlockSize];
			FMemory::Memcpy(CurrentCipherBlock, Ciphertext.GetData() + Offset, FAES::AESBlockSize);

			uint8* Block = OutPlaintext.GetData() + Offset;
			FAES::DecryptData(Block, FAES::AESBlockSize, Key);
			for (int32 ByteIndex = 0; ByteIndex < FAES::AESBlockSize; ++ByteIndex)
			{
				Block[ByteIndex] ^= PreviousBlock[ByteIndex];
			}

			FMemory::Memcpy(PreviousBlock, CurrentCipherBlock, FAES::AESBlockSize);
		}

		const uint8 PaddingBytes = OutPlaintext.Last();
		if (PaddingBytes == 0 || PaddingBytes > FAES::AESBlockSize || PaddingBytes > OutPlaintext.Num())
		{
			return false;
		}

		for (int32 Index = OutPlaintext.Num() - PaddingBytes; Index < OutPlaintext.Num(); ++Index)
		{
			if (OutPlaintext[Index] != PaddingBytes)
			{
				return false;
			}
		}

		OutPlaintext.SetNum(OutPlaintext.Num() - PaddingBytes);
		return true;
	}

	bool SaveEncryptedCredentialFile(const FString& Path, const FBHLocalCredentialRecord& Record)
	{
		TArray<uint8> Iv;
		TArray<uint8> Ciphertext;
		if (!EncryptCredentialBytes(StringToUtf8Bytes(JsonObjectToString(LocalCredentialToJson(Record))), Iv, Ciphertext))
		{
			return false;
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("version"), 1);
		Root->SetStringField(TEXT("algorithm"), TEXT("AES-256-CBC-HMAC-SHA1"));
		Root->SetStringField(TEXT("iv"), FBase64::Encode(Iv));
		Root->SetStringField(TEXT("ciphertext"), FBase64::Encode(Ciphertext));
		Root->SetStringField(TEXT("mac"), MakePayloadMac(Iv, Ciphertext));

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return AtomicSaveStringToFile(JsonObjectToString(Root), Path);
	}

	// Constant-time compare of two equal-length ASCII strings (used for the credential MAC). FString's
	// operator!= short-circuits on the first differing byte, which leaks a byte-by-byte timing oracle;
	// the loop below always visits every byte. Marginal here (the MAC key is machine-bound and any
	// attacker is already local), but cheap to do right.
	bool BHConstantTimeStringEquals(const FString& A, const FString& B)
	{
		if (A.Len() != B.Len())
		{
			return false;
		}
		uint32 Accumulator = 0;
		for (int32 Index = 0; Index < A.Len(); ++Index)
		{
			Accumulator |= static_cast<uint32>(A[Index] ^ B[Index]);
		}
		return Accumulator == 0;
	}

	// Parse a single encrypted credential file: decode, verify the MAC, decrypt, and apply the record.
	// Returns false if the file is missing/unreadable or fails any integrity/decrypt step.
	bool TryLoadEncryptedCredentialFile(const FString& Path, FBHLocalCredentialRecord& OutRecord)
	{
		FString Input;
		if (!FFileHelper::LoadFileToString(Input, *Path))
		{
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		if (!StringToJsonObject(Input, Root))
		{
			return false;
		}

		TArray<uint8> Iv;
		TArray<uint8> Ciphertext;
		if (!FBase64::Decode(JsonString(Root, TEXT("iv")), Iv) || !FBase64::Decode(JsonString(Root, TEXT("ciphertext")), Ciphertext))
		{
			return false;
		}

		const FString ExpectedMac = JsonString(Root, TEXT("mac")).ToLower();
		if (ExpectedMac.IsEmpty() || !BHConstantTimeStringEquals(ExpectedMac, MakePayloadMac(Iv, Ciphertext)))
		{
			return false;
		}

		TArray<uint8> Plaintext;
		if (!DecryptCredentialBytes(Iv, Ciphertext, Plaintext))
		{
			return false;
		}

		TSharedPtr<FJsonObject> CredentialJson;
		return StringToJsonObject(Utf8BytesToString(Plaintext), CredentialJson) && ApplyLocalCredentialJson(CredentialJson, OutRecord);
	}

	// Read the encrypted credential file, falling back to its ".bak" rollback if the primary is
	// missing or fails to load/verify. Mirrors the profile/progress backup-aware LoadJsonWithBackup,
	// since SaveEncryptedCredentialFile also rotates a ".bak" via AtomicSaveStringToFile.
	bool LoadEncryptedCredentialFile(const FString& Path, FBHLocalCredentialRecord& OutRecord)
	{
		if (TryLoadEncryptedCredentialFile(Path, OutRecord))
		{
			return true;
		}

		const FString BackupPath = Path + TEXT(".bak");
		if (TryLoadEncryptedCredentialFile(BackupPath, OutRecord))
		{
			UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt local credentials: primary %s was unreadable; recovered from backup."), *Path);
			return true;
		}

		return false;
	}
}

void UBHAccountSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadProfile();
	LoadProgress();
	EnsureDeviceId();

	if (Profile.PlayerId.IsEmpty())
	{
		FString Message;
		ContinueAsGuest(Message);
	}
}

void UBHAccountSubsystem::Deinitialize()
{
	StopLoginPolling();
	Super::Deinitialize();
}

void UBHAccountSubsystem::AccountGuest()
{
	FString Message;
	ContinueAsGuest(Message);
}

void UBHAccountSubsystem::LoginGoogle()
{
	FString Message;
	BeginProviderLogin(TEXT("google"), Message);
}

void UBHAccountSubsystem::LoginMicrosoft()
{
	FString Message;
	BeginProviderLogin(TEXT("microsoft"), Message);
}

void UBHAccountSubsystem::AccountPollLogin()
{
	// Stop an abandoned sign-in from polling indefinitely (the backend can keep replying "pending").
	if (LoginPollDeadlineSeconds > 0.0 && FPlatformTime::Seconds() > LoginPollDeadlineSeconds)
	{
		StopLoginPolling();
		SetLastAccountMessage(TEXT("Sign-in timed out. Re-open the login to try again."));
		return;
	}

	FString Message;
	PollProviderLogin(Message);
}

void UBHAccountSubsystem::AccountSync()
{
	FString Message;
	SyncProgress(Message);
}

void UBHAccountSubsystem::AccountSignOut()
{
	FString Message;
	SignOut(Message);
}

void UBHAccountSubsystem::AccountCreateLocal(const FString& Username, const FString& Password)
{
	FString Message;
	CreateOrUpdateLocalCredential(Username, Password, Message);
}

void UBHAccountSubsystem::AccountLoginLocal(const FString& Username, const FString& Password)
{
	FString Message;
	LoginLocalCredential(Username, Password, Message);
}

void UBHAccountSubsystem::AccountForgetLocal()
{
	FString Message;
	ForgetLocalCredential(Message);
}

void UBHAccountSubsystem::AccountResetLocalClassroomData()
{
	FString Message;
	ResetLocalClassroomData(Message);
}

bool UBHAccountSubsystem::ContinueAsGuest(FString& OutMessage)
{
	StopLoginPolling();
	EnsureDeviceId();

	if (Profile.PlayerId.IsEmpty() || !Profile.bGuest)
	{
		Profile.PlayerId = FString::Printf(TEXT("guest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Profile.DisplayName = TEXT("Guest");
		Profile.Provider = TEXT("guest");
		Profile.ProviderSubject.Reset();
		Profile.Email.Reset();
		Profile.AvatarUrl.Reset();
		Profile.SessionToken.Reset();
		Profile.bGuest = true;
		Profile.LastLoginUtc = UtcNowString();
		SaveProfile();
	}

	OutMessage = FString::Printf(TEXT("Using local guest profile: %s"), *Profile.PlayerId);
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::BeginProviderLogin(const FString& Provider, FString& OutMessage)
{
	if (bLoginStartRequestInFlight)
	{
		OutMessage = TEXT("Account login is already starting.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const UBHAccountSettings* Settings = GetDefault<UBHAccountSettings>();
	if (!Settings || !Settings->bEnableExternalAccountLogin)
	{
		OutMessage = TEXT("External account login is disabled in account settings.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString BackendBaseUrl = GetBackendBaseUrl();
	if (BackendBaseUrl.IsEmpty())
	{
		OutMessage = TEXT("No account backend URL is configured.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	EnsureDeviceId();
	PendingProvider = Provider.ToLower();

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BackendBaseUrl + TEXT("/health"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->OnProcessRequestComplete().BindUObject(this, &UBHAccountSubsystem::HandleProviderHealthResponse);
	bLoginStartRequestInFlight = true;
	Request->SetTimeout(10.0f); // Bound flaky-Wi-Fi hangs; engine default is ~300s.
	Request->ProcessRequest();

	OutMessage = FString::Printf(TEXT("Checking %s account service..."), *PendingProvider);
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::PollProviderLogin(FString& OutMessage)
{
	if (bLoginRequestInFlight)
	{
		OutMessage = TEXT("Account login check is already in progress.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString BackendBaseUrl = GetBackendBaseUrl();
	if (BackendBaseUrl.IsEmpty() || Profile.DeviceId.IsEmpty())
	{
		OutMessage = TEXT("No pending account login is available.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString PollUrl = FString::Printf(TEXT("%s/auth/device/%s"), *BackendBaseUrl, *Profile.DeviceId);
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(PollUrl);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->OnProcessRequestComplete().BindUObject(this, &UBHAccountSubsystem::HandleLoginPollResponse);
	bLoginRequestInFlight = true;
	Request->SetTimeout(10.0f); // Bound flaky-Wi-Fi hangs; engine default is ~300s.
	Request->ProcessRequest();

	OutMessage = TEXT("Checking account login status...");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::SyncProgress(FString& OutMessage)
{
	const UBHAccountSettings* Settings = GetDefault<UBHAccountSettings>();
	if (!Settings || !Settings->bEnableExternalAccountLogin)
	{
		SaveProgress();
		OutMessage = TEXT("Progress saved locally. External account sync is disabled for this classroom build.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	if (Profile.SessionToken.IsEmpty())
	{
		SaveProgress();
		OutMessage = TEXT("Progress saved locally. Sign in to sync it.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	if (bSyncRequestInFlight)
	{
		OutMessage = TEXT("Progress sync is already in progress.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString BackendBaseUrl = GetBackendBaseUrl();
	if (BackendBaseUrl.IsEmpty())
	{
		OutMessage = TEXT("Progress saved locally. No account backend URL is configured.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetObjectField(TEXT("profile"), ProfileToJson());
	Root->SetObjectField(TEXT("progress"), ProgressToJson());

	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BackendBaseUrl + TEXT("/player/save"));
	Request->SetVerb(TEXT("PUT"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Profile.SessionToken));
	Request->SetContentAsString(JsonObjectToString(Root));
	Request->OnProcessRequestComplete().BindUObject(this, &UBHAccountSubsystem::HandleSyncResponse);
	bSyncRequestInFlight = true;
	Request->SetTimeout(10.0f); // Bound flaky-Wi-Fi hangs; engine default is ~300s.
	Request->ProcessRequest();

	OutMessage = TEXT("Syncing progress...");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::SignOut(FString& OutMessage)
{
	StopLoginPolling();
	Profile.SessionToken.Reset();
	Profile.Provider = TEXT("guest");
	Profile.ProviderSubject.Reset();
	Profile.Email.Reset();
	Profile.AvatarUrl.Reset();
	Profile.bGuest = true;
	SaveProfile();

	OutMessage = TEXT("Signed out. Progress remains saved locally.");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::CreateOrUpdateLocalCredential(const FString& Username, const FString& Password, FString& OutMessage)
{
	StopLoginPolling();

	const FString CleanUsername = SanitizeLocalUsername(Username);
	if (CleanUsername.Len() < BHMinLocalUsernameChars)
	{
		OutMessage = FString::Printf(TEXT("Local username must be at least %d letters or numbers."), BHMinLocalUsernameChars);
		SetLastAccountMessage(OutMessage);
		return false;
	}

	if (Password.Len() < BHMinLocalPasswordChars || Password.Len() > BHMaxLocalPasswordChars)
	{
		OutMessage = FString::Printf(TEXT("Local password must be %d-%d characters."), BHMinLocalPasswordChars, BHMaxLocalPasswordChars);
		SetLastAccountMessage(OutMessage);
		return false;
	}

	FBHLocalCredentialRecord ExistingRecord;
	FBHLocalCredentialRecord Record;
	if (LoadEncryptedCredentialFile(GetCredentialPath(), ExistingRecord) && ExistingRecord.Username.Equals(CleanUsername, ESearchCase::IgnoreCase))
	{
		Record = ExistingRecord;
	}
	else
	{
		Record.Username = CleanUsername;
		Record.PlayerId = MakeLocalPlayerId(CleanUsername);
		Record.CreatedUtc = UtcNowString();
	}

	Record.PasswordSalt = MakePasswordSalt();
	Record.PasswordHash = MakePasswordHash(CleanUsername, Password, Record.PasswordSalt);
	Record.LastLoginUtc = UtcNowString();

	if (!SaveEncryptedCredentialFile(GetCredentialPath(), Record))
	{
		OutMessage = TEXT("Could not save encrypted local credentials.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	Profile.PlayerId = Record.PlayerId;
	Profile.DisplayName = Record.Username;
	Profile.Provider = TEXT("local");
	Profile.ProviderSubject = Record.Username.ToLower();
	Profile.Email.Reset();
	Profile.AvatarUrl.Reset();
	Profile.SessionToken.Reset();
	Profile.bGuest = false;
	Profile.LastLoginUtc = Record.LastLoginUtc;
	EnsureDeviceId();
	SaveProfile();

	OutMessage = TEXT("Local credentials saved.");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::LoginLocalCredential(const FString& Username, const FString& Password, FString& OutMessage)
{
	StopLoginPolling();

	FBHLocalCredentialRecord Record;
	if (!LoadEncryptedCredentialFile(GetCredentialPath(), Record))
	{
		OutMessage = TEXT("No readable encrypted local credential file was found.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString CleanUsername = SanitizeLocalUsername(Username);
	if (!Record.Username.Equals(CleanUsername, ESearchCase::IgnoreCase))
	{
		OutMessage = TEXT("Local username or password was incorrect.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	if (Password.Len() > BHMaxLocalPasswordChars)
	{
		OutMessage = TEXT("Local username or password was incorrect.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	const FString CandidateHash = MakePasswordHash(Record.Username, Password, Record.PasswordSalt);
	if (!CandidateHash.Equals(Record.PasswordHash, ESearchCase::IgnoreCase))
	{
		OutMessage = TEXT("Local username or password was incorrect.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	Record.LastLoginUtc = UtcNowString();
	SaveEncryptedCredentialFile(GetCredentialPath(), Record);

	Profile.PlayerId = Record.PlayerId;
	Profile.DisplayName = Record.Username;
	Profile.Provider = TEXT("local");
	Profile.ProviderSubject = Record.Username.ToLower();
	Profile.Email.Reset();
	Profile.AvatarUrl.Reset();
	Profile.SessionToken.Reset();
	Profile.bGuest = false;
	Profile.LastLoginUtc = Record.LastLoginUtc;
	EnsureDeviceId();
	SaveProfile();

	OutMessage = TEXT("Signed in locally.");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::ForgetLocalCredential(FString& OutMessage)
{
	StopLoginPolling();

	// Delete the base credential AND its atomic-write siblings (.bak/.tmp). LoadEncryptedCredentialFile
	// deliberately falls back to the .bak, so a base-only delete left a working copy behind: on a shared
	// school PC a "forgotten" account would still log in, and the salted password hash the user asked to
	// erase would remain on disk.
	IFileManager& FileManager = IFileManager::Get();
	const TCHAR* const CredentialSuffixes[] = { TEXT(""), TEXT(".bak"), TEXT(".tmp") };
	bool bHadCredential = false;
	for (const TCHAR* Suffix : CredentialSuffixes)
	{
		const FString CredentialFile = GetCredentialPath() + Suffix;
		if (FileManager.FileExists(*CredentialFile))
		{
			bHadCredential = true;
			FileManager.Delete(*CredentialFile, false, true, true);
		}
	}

	if (Profile.Provider.Equals(TEXT("local"), ESearchCase::IgnoreCase))
	{
		Profile.PlayerId.Reset();
		Profile.DisplayName.Reset();
		Profile.Provider.Reset();
		Profile.ProviderSubject.Reset();
		Profile.SessionToken.Reset();
		Profile.bGuest = true;

		FString GuestMessage;
		ContinueAsGuest(GuestMessage);
	}

	OutMessage = bHadCredential ? TEXT("Encrypted local credentials removed.") : TEXT("No local credentials were saved.");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::ResetLocalClassroomData(FString& OutMessage)
{
	StopLoginPolling();

	IFileManager& FileManager = IFileManager::Get();
	const TCHAR* const Suffixes[] = { TEXT(""), TEXT(".bak"), TEXT(".tmp") };
	for (const TCHAR* Suffix : Suffixes)
	{
		FileManager.Delete(*(GetProfilePath() + Suffix), false, true, true);
		FileManager.Delete(*(GetProgressPath() + Suffix), false, true, true);
		// Include the credential's atomic-write siblings (.bak/.tmp): a base-only delete left a .bak that
		// LoadEncryptedCredentialFile falls back to, so a "reset" PC could still authenticate the old account.
		FileManager.Delete(*(GetCredentialPath() + Suffix), false, true, true);
	}

	Profile = FBHAccountProfile();
	Progress = FBHAccountProgress();
	bProgressSaveLocked = false;

	FString GuestMessage;
	ContinueAsGuest(GuestMessage);

	OutMessage = TEXT("Local classroom account data reset on this machine.");
	SetLastAccountMessage(OutMessage);
	return true;
}

bool UBHAccountSubsystem::SetLocalDisplayName(const FString& DisplayName, FString& OutMessage)
{
	const FString CleanDisplayName = SanitizeDisplayName(DisplayName);
	if (CleanDisplayName.Len() < BHMinDisplayNameChars)
	{
		OutMessage = FString::Printf(TEXT("Name must be at least %d visible characters."), BHMinDisplayNameChars);
		SetLastAccountMessage(OutMessage);
		return false;
	}

	EnsureDeviceId();
	if (Profile.PlayerId.IsEmpty())
	{
		Profile.PlayerId = FString::Printf(TEXT("guest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		Profile.Provider = TEXT("guest");
		Profile.ProviderSubject.Reset();
		Profile.Email.Reset();
		Profile.AvatarUrl.Reset();
		Profile.SessionToken.Reset();
		Profile.bGuest = true;
		Profile.LastLoginUtc = UtcNowString();
	}

	Profile.DisplayName = CleanDisplayName;
	if (Profile.Provider.IsEmpty())
	{
		Profile.Provider = TEXT("guest");
		Profile.bGuest = true;
	}
	SaveProfile();

	OutMessage = FString::Printf(TEXT("Name set to %s."), *Profile.DisplayName);
	SetLastAccountMessage(OutMessage);
	return true;
}

void UBHAccountSubsystem::RecordRoundResult(EBHPlayerRole Role, EBHPlayerLifeState LifeState, EBHRoundPhase ResultPhase)
{
	if (Role != EBHPlayerRole::Hunter && Role != EBHPlayerRole::Survivor)
	{
		return;
	}

	// Saturating increments: overflow is unreachable (~7.8M rounds) but mirror the BHSaturatingAddPoints
	// posture used for in-round points so a wrap can never flip a lifetime stat negative.
	if (Progress.RoundsPlayed < MAX_int32)
	{
		++Progress.RoundsPlayed;
	}
	int32 EarnedXP = 25;

	if (Role == EBHPlayerRole::Hunter && ResultPhase == EBHRoundPhase::HunterWin)
	{
		++Progress.HunterWins;
		EarnedXP += 100;
	}
	else if (Role == EBHPlayerRole::Survivor && ResultPhase == EBHRoundPhase::SurvivorsWin)
	{
		++Progress.SurvivorWins;
		EarnedXP += 100;
	}

	if (Role == EBHPlayerRole::Survivor && LifeState == EBHPlayerLifeState::Escaped)
	{
		++Progress.Escapes;
		EarnedXP += 50;
	}

	// Consecutive-win streak (for On a Roll): the player's side winning extends it; anything else breaks it.
	const bool bWonRound = (Role == EBHPlayerRole::Hunter && ResultPhase == EBHRoundPhase::HunterWin)
		|| (Role == EBHPlayerRole::Survivor && ResultPhase == EBHRoundPhase::SurvivorsWin);
	if (bWonRound)
	{
		if (Progress.CurrentWinStreak < MAX_int32)
		{
			++Progress.CurrentWinStreak;
		}
	}
	else
	{
		Progress.CurrentWinStreak = 0;
	}

	Progress.XP = (EarnedXP > 0 && Progress.XP > MAX_int32 - EarnedXP) ? MAX_int32 : Progress.XP + EarnedXP;
	Progress.LastUpdatedUtc = UtcNowString();
	SanitizeProgressCosmetics();
	SaveProgress();

	FString Message;
	SyncProgress(Message);

	// Cosmetic achievements (idempotent; each first-time-only awards XP + unlocks its tint + toasts).
	if (Role == EBHPlayerRole::Survivor && LifeState == EBHPlayerLifeState::Escaped)
	{
		UnlockAchievement(FName(TEXT("escape_artist")));
	}
	if (Role == EBHPlayerRole::Survivor && ResultPhase == EBHRoundPhase::SurvivorsWin)
	{
		UnlockAchievement(FName(TEXT("survivor")));
	}
	// Teacher (Hunter) win -> the Faculty tint.
	if (Role == EBHPlayerRole::Hunter && ResultPhase == EBHRoundPhase::HunterWin)
	{
		UnlockAchievement(FName(TEXT("top_of_the_class")));
	}
	// Milestone: 25 rounds played -> the Veteran tint (idempotent, so it only toasts once at round 25).
	if (Progress.RoundsPlayed >= 25)
	{
		UnlockAchievement(FName(TEXT("veteran")));
	}
	// Milestone: 100 rounds -> the Graduation Cap.
	if (Progress.RoundsPlayed >= 100)
	{
		UnlockAchievement(FName(TEXT("centurion")));
	}
	// Three wins in a row -> the Crown.
	if (Progress.CurrentWinStreak >= 3)
	{
		UnlockAchievement(FName(TEXT("on_a_roll")));
	}
}

void UBHAccountSubsystem::UnlockAchievement(FName AchievementId)
{
	if (AchievementId.IsNone() || Progress.UnlockedAchievements.Contains(AchievementId))
	{
		return;
	}

	Progress.UnlockedAchievements.AddUnique(AchievementId);
	const FBHAchievementDef* Def = BHFindAchievement(AchievementId);
	if (Def && Def->XPReward > 0)
	{
		Progress.XP = (Progress.XP > MAX_int32 - Def->XPReward) ? MAX_int32 : Progress.XP + Def->XPReward;
	}
	Progress.LastUpdatedUtc = UtcNowString();
	SaveProgress();

	// Toast on the menu status line + the local player's HUD (this subsystem is per-client, so the "first"
	// controller here is the local player whose progress this is). Purely cosmetic feedback.
	const FString Title = Def ? FString(Def->Title) : AchievementId.ToString();
	const FString Toast = FString::Printf(TEXT("Achievement unlocked: %s"), *Title);
	SetLastAccountMessage(Toast);
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UWorld* World = GameInstance->GetWorld())
		{
			if (ABHPlayerController* LocalPC = Cast<ABHPlayerController>(World->GetFirstPlayerController()))
			{
				LocalPC->ShowLocalStatusMessage(Toast, 4.5f);
			}
		}
	}

	// Completionist: collecting every hidden egg achievement earns it. Guarded so unlocking completionist
	// itself can't re-enter (and UnlockAchievement is idempotent, so the recursion bottoms out immediately).
	if (AchievementId != FName(TEXT("completionist")))
	{
		static const FName EggAchievements[] = {
			FName(TEXT("spelunker")), FName(TEXT("honorary_faculty")),
			FName(TEXT("codebreaker")), FName(TEXT("roof_rider"))
		};
		bool bAllEggs = true;
		for (const FName& Egg : EggAchievements)
		{
			if (!Progress.UnlockedAchievements.Contains(Egg))
			{
				bAllEggs = false;
				break;
			}
		}
		if (bAllEggs)
		{
			UnlockAchievement(FName(TEXT("completionist")));
		}
	}
}

void UBHAccountSubsystem::RecordTrainActivityUse(int32 ActivityIndex)
{
	if (ActivityIndex < 0 || ActivityIndex > 3)
	{
		return;
	}
	const int32 Bit = 1 << ActivityIndex;
	if ((Progress.TrainActivityMask & Bit) != 0)
	{
		return; // already recorded this activity type
	}
	Progress.TrainActivityMask |= Bit;
	Progress.LastUpdatedUtc = UtcNowString();
	SaveProgress();
	// All four train activities tried -> Tourist.
	if ((Progress.TrainActivityMask & 0x0F) == 0x0F)
	{
		UnlockAchievement(FName(TEXT("tourist")));
	}
}

bool UBHAccountSubsystem::HasAchievement(FName AchievementId) const
{
	return Progress.UnlockedAchievements.Contains(AchievementId);
}

TArray<FBHAchievementDisplay> UBHAccountSubsystem::GetAchievementsForDisplay() const
{
	TArray<FBHAchievementDisplay> Result;
	Result.Reserve(UE_ARRAY_COUNT(GBHAchievements));
	for (const FBHAchievementDef& Def : GBHAchievements)
	{
		FBHAchievementDisplay Display;
		Display.Id = FName(Def.Id);
		Display.Title = Def.Title;
		Display.Description = Def.Description;
		Display.RewardLabel = Def.RewardLabel ? Def.RewardLabel : TEXT("");
		Display.Difficulty = Def.Difficulty;
		Display.bHidden = Def.bHidden;
		Display.bUnlocked = Progress.UnlockedAchievements.Contains(Display.Id);
		Result.Add(MoveTemp(Display));
	}
	return Result;
}

void UBHAccountSubsystem::GetAchievementCounts(int32& OutEarned, int32& OutTotal) const
{
	OutTotal = UE_ARRAY_COUNT(GBHAchievements);
	OutEarned = 0;
	for (const FBHAchievementDef& Def : GBHAchievements)
	{
		if (Progress.UnlockedAchievements.Contains(FName(Def.Id)))
		{
			++OutEarned;
		}
	}
}

const FBHAccountProfile& UBHAccountSubsystem::GetProfile() const
{
	return Profile;
}

const FBHAccountProgress& UBHAccountSubsystem::GetProgress() const
{
	return Progress;
}

const FString& UBHAccountSubsystem::GetLastAccountMessage() const
{
	return LastAccountMessage;
}

bool UBHAccountSubsystem::IsLoginPending() const
{
	return bLoginPending;
}

bool UBHAccountSubsystem::IsLoggedIn() const
{
	return !Profile.bGuest && !Profile.SessionToken.IsEmpty();
}

bool UBHAccountSubsystem::HasLocalCredential() const
{
	return IFileManager::Get().FileExists(*GetCredentialPath());
}

bool UBHAccountSubsystem::IsCosmeticUnlocked(EBHCosmeticCategory Category, int32 Index) const
{
	return BHCosmeticIsUnlocked(Category, Index, Progress.XP, &Progress.UnlockedAchievements);
}

int32 UBHAccountSubsystem::GetSelectedCosmeticIndex(EBHCosmeticCategory Category) const
{
	switch (Category)
	{
	case EBHCosmeticCategory::Outfit:
		return BHCosmeticClampUnlockedIndex(Category, Progress.SelectedAvatarIndex, Progress.XP, &Progress.UnlockedAchievements);
	case EBHCosmeticCategory::ShirtColor:
		return BHCosmeticClampUnlockedIndex(Category, Progress.SelectedAvatarColorIndex, Progress.XP, &Progress.UnlockedAchievements);
	case EBHCosmeticCategory::Headwear:
		return BHCosmeticClampUnlockedIndex(Category, Progress.SelectedAvatarHeadwearIndex, Progress.XP, &Progress.UnlockedAchievements);
	case EBHCosmeticCategory::Gear:
		return 0;
	default:
		return 0;
	}
}

bool UBHAccountSubsystem::SetSelectedCosmetic(EBHCosmeticCategory Category, int32 Index, FString& OutMessage)
{
	const int32 ClampedIndex = BHCosmeticClampIndex(Category, Index);
	const int32 RequiredXP = BHCosmeticRequiredXP(Category, ClampedIndex);
	if (!BHCosmeticIsUnlocked(Category, ClampedIndex, Progress.XP, &Progress.UnlockedAchievements))
	{
		const TCHAR* RequiredAchievement = BHCosmeticRequiredAchievement(Category, ClampedIndex);
		if (RequiredAchievement && RequiredAchievement[0] != TEXT('\0'))
		{
			OutMessage = FString::Printf(
				TEXT("%s %s is an achievement reward -- earn the matching achievement to unlock it."),
				BHCosmeticCategoryName(Category),
				BHCosmeticItemName(Category, ClampedIndex));
		}
		else
		{
			OutMessage = FString::Printf(
				TEXT("%s %s unlocks at %d XP. Current XP: %d."),
				BHCosmeticCategoryName(Category),
				BHCosmeticItemName(Category, ClampedIndex),
				RequiredXP,
				Progress.XP);
		}
		SetLastAccountMessage(OutMessage);
		return false;
	}

	switch (Category)
	{
	case EBHCosmeticCategory::Outfit:
		Progress.SelectedAvatarIndex = ClampedIndex;
		break;
	case EBHCosmeticCategory::ShirtColor:
		Progress.SelectedAvatarColorIndex = ClampedIndex;
		break;
	case EBHCosmeticCategory::Headwear:
		Progress.SelectedAvatarHeadwearIndex = ClampedIndex;
		break;
	case EBHCosmeticCategory::Gear:
		Progress.SelectedAvatarGearIndex = 0;
		break;
	default:
		OutMessage = TEXT("Unknown cosmetic category.");
		SetLastAccountMessage(OutMessage);
		return false;
	}

	Progress.LastUpdatedUtc = UtcNowString();
	SanitizeProgressCosmetics();
	SaveProgress();

	OutMessage = FString::Printf(
		TEXT("%s cosmetic saved: %s."),
		BHCosmeticCategoryName(Category),
		BHCosmeticItemName(Category, ClampedIndex));
	SetLastAccountMessage(OutMessage);
	return true;
}

FString UBHAccountSubsystem::GetCosmeticSummary() const
{
	return FString::Printf(
		TEXT("Outfit: %s | Shirt: %s | Headwear: %s"),
		BHCosmeticItemName(EBHCosmeticCategory::Outfit, GetSelectedCosmeticIndex(EBHCosmeticCategory::Outfit)),
		BHCosmeticItemName(EBHCosmeticCategory::ShirtColor, GetSelectedCosmeticIndex(EBHCosmeticCategory::ShirtColor)),
		BHCosmeticItemName(EBHCosmeticCategory::Headwear, GetSelectedCosmeticIndex(EBHCosmeticCategory::Headwear)));
}

FString UBHAccountSubsystem::GetAccountSummary() const
{
	const FString ProviderName = Profile.Provider.IsEmpty() ? TEXT("guest") : Profile.Provider;
	const FString DisplayName = Profile.DisplayName.IsEmpty() ? TEXT("Guest") : Profile.DisplayName;
	FString Summary = FString::Printf(
		TEXT("%s (%s)\nXP: %d | Rounds: %d | Hunter Wins: %d | Survivor Wins: %d | Escapes: %d"),
		*DisplayName,
		*ProviderName,
		Progress.XP,
		Progress.RoundsPlayed,
		Progress.HunterWins,
		Progress.SurvivorWins,
		Progress.Escapes);

	Summary += FString::Printf(TEXT("\nCosmetics: %s"), *GetCosmeticSummary());

	Summary += HasLocalCredential()
		? TEXT("\nEncrypted local credentials: saved")
		: TEXT("\nEncrypted local credentials: none");
	return Summary;
}

FString UBHAccountSubsystem::GetAccountDirectory() const
{
	return FPaths::ProjectSavedDir() / TEXT("Account");
}

FString UBHAccountSubsystem::GetProfilePath() const
{
	return GetAccountDirectory() / TEXT("profile.json");
}

FString UBHAccountSubsystem::GetProgressPath() const
{
	return GetAccountDirectory() / TEXT("progress.json");
}

FString UBHAccountSubsystem::GetCredentialPath() const
{
	return GetAccountDirectory() / TEXT("local_credentials.enc.json");
}

FString UBHAccountSubsystem::GetBackendBaseUrl() const
{
	const UBHAccountSettings* Settings = GetDefault<UBHAccountSettings>();
	return Settings ? NormalizeBackendBaseUrl(Settings->BackendBaseUrl) : FString();
}

FString UBHAccountSubsystem::MakeDeviceId()
{
	return FString::Printf(TEXT("bh_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

void UBHAccountSubsystem::EnsureDeviceId()
{
	if (Profile.DeviceId.IsEmpty())
	{
		Profile.DeviceId = MakeDeviceId();
		SaveProfile();
	}
}

void UBHAccountSubsystem::LaunchPendingProviderLogin()
{
	const FString BackendBaseUrl = GetBackendBaseUrl();
	if (BackendBaseUrl.IsEmpty() || PendingProvider.IsEmpty())
	{
		SetLastAccountMessage(TEXT("No account backend or provider is configured."));
		return;
	}

	EnsureDeviceId();
	const FString LoginUrl = FString::Printf(TEXT("%s/auth/%s/start?device_id=%s&client=blackouthunt"), *BackendBaseUrl, *PendingProvider, *Profile.DeviceId);
	FPlatformProcess::LaunchURL(*LoginUrl, nullptr, nullptr);

	bLoginPending = true;
	StartLoginPolling();
	SetLastAccountMessage(FString::Printf(TEXT("Opened %s login in your browser. Complete the sign-in, then return to the game."), *PendingProvider));
}

void UBHAccountSubsystem::StartLoginPolling()
{
	const UBHAccountSettings* Settings = GetDefault<UBHAccountSettings>();
	const float PollSeconds = Settings ? FMath::Max(1.0f, Settings->LoginPollSeconds) : 2.0f;
	// Give up after ~5 minutes of "pending" so an abandoned browser sign-in stops polling the backend
	// forever. Comfortably longer than any real OAuth completion; the user can re-launch login to retry.
	LoginPollDeadlineSeconds = FPlatformTime::Seconds() + 300.0;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().SetTimer(LoginPollTimerHandle, this, &UBHAccountSubsystem::AccountPollLogin, PollSeconds, true, PollSeconds);
	}
}

void UBHAccountSubsystem::StopLoginPolling()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World)
	{
		World->GetTimerManager().ClearTimer(LoginPollTimerHandle);
	}

	bLoginPending = false;
	bLoginStartRequestInFlight = false;
	bLoginRequestInFlight = false;
	LoginPollDeadlineSeconds = 0.0;
	PendingProvider.Reset();
}

void UBHAccountSubsystem::SetLastAccountMessage(const FString& Message)
{
	LastAccountMessage = Message;
	UE_LOG(LogTemp, Display, TEXT("%s"), *LastAccountMessage);
}

void UBHAccountSubsystem::LoadProfile()
{
	TSharedPtr<FJsonObject> JsonObject;
	if (LoadJsonWithBackup(GetProfilePath(), JsonObject))
	{
		ApplyProfileJson(JsonObject);
	}
}

void UBHAccountSubsystem::LoadProgress()
{
	TSharedPtr<FJsonObject> JsonObject;
	if (LoadJsonWithBackup(GetProgressPath(), JsonObject))
	{
		ApplyProgressJson(JsonObject);
	}
}

void UBHAccountSubsystem::SaveProfile() const
{
	IFileManager::Get().MakeDirectory(*GetAccountDirectory(), true);
	AtomicSaveStringToFile(JsonObjectToString(ProfileToJson()), GetProfilePath());
}

void UBHAccountSubsystem::SaveProgress() const
{
	if (bProgressSaveLocked)
	{
		// On-disk file is from a newer build; refuse to overwrite it with this build's schema.
		return;
	}
	IFileManager::Get().MakeDirectory(*GetAccountDirectory(), true);
	AtomicSaveStringToFile(JsonObjectToString(ProgressToJson()), GetProgressPath());
}

void UBHAccountSubsystem::SanitizeProgressCosmetics()
{
	Progress.XP = FMath::Max(0, Progress.XP);
	Progress.SelectedAvatarIndex = BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Outfit, Progress.SelectedAvatarIndex, Progress.XP, &Progress.UnlockedAchievements);
	Progress.SelectedAvatarColorIndex = BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::ShirtColor, Progress.SelectedAvatarColorIndex, Progress.XP, &Progress.UnlockedAchievements);
	Progress.SelectedAvatarHeadwearIndex = BHCosmeticClampUnlockedIndex(EBHCosmeticCategory::Headwear, Progress.SelectedAvatarHeadwearIndex, Progress.XP, &Progress.UnlockedAchievements);
	Progress.SelectedAvatarGearIndex = 0;
}

TSharedRef<FJsonObject> UBHAccountSubsystem::ProfileToJson() const
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("player_id"), Profile.PlayerId);
	JsonObject->SetStringField(TEXT("display_name"), Profile.DisplayName);
	JsonObject->SetStringField(TEXT("provider"), Profile.Provider);
	JsonObject->SetStringField(TEXT("provider_subject"), Profile.ProviderSubject);
	JsonObject->SetStringField(TEXT("email"), Profile.Email);
	JsonObject->SetStringField(TEXT("avatar_url"), Profile.AvatarUrl);
	JsonObject->SetStringField(TEXT("device_id"), Profile.DeviceId);
	JsonObject->SetStringField(TEXT("session_token"), Profile.SessionToken);
	JsonObject->SetStringField(TEXT("last_login_utc"), Profile.LastLoginUtc);
	JsonObject->SetBoolField(TEXT("guest"), Profile.bGuest);
	return JsonObject;
}

TSharedRef<FJsonObject> UBHAccountSubsystem::ProgressToJson() const
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetNumberField(TEXT("profile_version"), Progress.ProfileVersion);
	JsonObject->SetNumberField(TEXT("rounds_played"), Progress.RoundsPlayed);
	JsonObject->SetNumberField(TEXT("hunter_wins"), Progress.HunterWins);
	JsonObject->SetNumberField(TEXT("survivor_wins"), Progress.SurvivorWins);
	JsonObject->SetNumberField(TEXT("escapes"), Progress.Escapes);
	JsonObject->SetNumberField(TEXT("current_win_streak"), Progress.CurrentWinStreak);
	JsonObject->SetNumberField(TEXT("train_activity_mask"), Progress.TrainActivityMask);
	JsonObject->SetNumberField(TEXT("xp"), Progress.XP);
	JsonObject->SetStringField(TEXT("selected_avatar_url"), Progress.SelectedAvatarUrl);
	JsonObject->SetNumberField(TEXT("selected_avatar_index"), Progress.SelectedAvatarIndex);
	JsonObject->SetNumberField(TEXT("selected_avatar_color_index"), Progress.SelectedAvatarColorIndex);
	JsonObject->SetNumberField(TEXT("selected_avatar_headwear_index"), Progress.SelectedAvatarHeadwearIndex);
	JsonObject->SetNumberField(TEXT("selected_avatar_gear_index"), Progress.SelectedAvatarGearIndex);
	JsonObject->SetStringField(TEXT("last_updated_utc"), Progress.LastUpdatedUtc);
	TArray<TSharedPtr<FJsonValue>> AchievementsJson;
	for (const FName& Achievement : Progress.UnlockedAchievements)
	{
		AchievementsJson.Add(MakeShared<FJsonValueString>(Achievement.ToString()));
	}
	JsonObject->SetArrayField(TEXT("unlocked_achievements"), AchievementsJson);
	return JsonObject;
}

void UBHAccountSubsystem::ApplyProfileJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	Profile.PlayerId = JsonString(JsonObject, TEXT("player_id"));
	Profile.DisplayName = JsonString(JsonObject, TEXT("display_name"));
	Profile.Provider = JsonString(JsonObject, TEXT("provider"));
	Profile.ProviderSubject = JsonString(JsonObject, TEXT("provider_subject"));
	Profile.Email = JsonString(JsonObject, TEXT("email"));
	Profile.AvatarUrl = JsonString(JsonObject, TEXT("avatar_url"));
	Profile.DeviceId = JsonString(JsonObject, TEXT("device_id"));
	Profile.SessionToken = JsonString(JsonObject, TEXT("session_token"));
	Profile.LastLoginUtc = JsonString(JsonObject, TEXT("last_login_utc"));

	bool bGuest = true;
	if (JsonObject.IsValid() && JsonObject->TryGetBoolField(TEXT("guest"), bGuest))
	{
		Profile.bGuest = bGuest;
	}
}

void UBHAccountSubsystem::ApplyProgressJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	const int32 OnDiskVersion = JsonInt(JsonObject, TEXT("profile_version"), 1);
	if (OnDiskVersion > BHCurrentProgressVersion)
	{
		// A newer build wrote this file. Don't reinterpret it with the old schema, and lock saving so
		// we never overwrite the newer data with a downgraded copy. Keep current in-memory progress.
		bProgressSaveLocked = true;
		Progress.ProfileVersion = OnDiskVersion;
		UE_LOG(LogTemp, Warning, TEXT("BlackoutHunt progress file is version %d (build supports %d); loading read-only to avoid clobbering newer data."), OnDiskVersion, BHCurrentProgressVersion);
		return;
	}

	bProgressSaveLocked = false;
	Progress.ProfileVersion = OnDiskVersion;
	Progress.RoundsPlayed = JsonInt(JsonObject, TEXT("rounds_played"));
	Progress.HunterWins = JsonInt(JsonObject, TEXT("hunter_wins"));
	Progress.SurvivorWins = JsonInt(JsonObject, TEXT("survivor_wins"));
	Progress.Escapes = JsonInt(JsonObject, TEXT("escapes"));
	Progress.CurrentWinStreak = JsonInt(JsonObject, TEXT("current_win_streak"));
	Progress.TrainActivityMask = JsonInt(JsonObject, TEXT("train_activity_mask"));
	Progress.XP = JsonInt(JsonObject, TEXT("xp"));
	Progress.SelectedAvatarUrl = JsonString(JsonObject, TEXT("selected_avatar_url"));
	Progress.SelectedAvatarIndex = JsonInt(JsonObject, TEXT("selected_avatar_index"));
	Progress.SelectedAvatarColorIndex = JsonInt(JsonObject, TEXT("selected_avatar_color_index"));
	Progress.SelectedAvatarHeadwearIndex = JsonInt(JsonObject, TEXT("selected_avatar_headwear_index"));
	Progress.SelectedAvatarGearIndex = JsonInt(JsonObject, TEXT("selected_avatar_gear_index"));
	Progress.LastUpdatedUtc = JsonString(JsonObject, TEXT("last_updated_utc"));
	Progress.UnlockedAchievements.Reset();
	const TArray<TSharedPtr<FJsonValue>>* AchievementsJson = nullptr;
	if (JsonObject.IsValid() && JsonObject->TryGetArrayField(TEXT("unlocked_achievements"), AchievementsJson) && AchievementsJson)
	{
		for (const TSharedPtr<FJsonValue>& Value : *AchievementsJson)
		{
			FString AchievementId;
			if (Value.IsValid() && Value->TryGetString(AchievementId) && !AchievementId.IsEmpty())
			{
				Progress.UnlockedAchievements.AddUnique(FName(AchievementId));
			}
		}
	}
	SanitizeProgressCosmetics();
}

void UBHAccountSubsystem::ApplyBackendPlayerJson(const TSharedPtr<FJsonObject>& PlayerObject, const FString& SessionToken)
{
	if (!PlayerObject.IsValid())
	{
		return;
	}

	Profile.PlayerId = JsonString(PlayerObject, TEXT("player_id"));
	if (Profile.PlayerId.IsEmpty())
	{
		Profile.PlayerId = FString::Printf(TEXT("%s_%s"), *PendingProvider, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	Profile.DisplayName = JsonString(PlayerObject, TEXT("display_name"));
	if (Profile.DisplayName.IsEmpty())
	{
		Profile.DisplayName = TEXT("Player");
	}

	Profile.Provider = JsonString(PlayerObject, TEXT("provider"));
	if (Profile.Provider.IsEmpty())
	{
		Profile.Provider = PendingProvider;
	}

	Profile.ProviderSubject = JsonString(PlayerObject, TEXT("provider_subject"));
	Profile.Email = JsonString(PlayerObject, TEXT("email"));
	Profile.AvatarUrl = JsonString(PlayerObject, TEXT("avatar_url"));
	Profile.SessionToken = SessionToken;
	Profile.LastLoginUtc = UtcNowString();
	Profile.bGuest = false;
	EnsureDeviceId();
	SaveProfile();

	if (Progress.SelectedAvatarUrl.IsEmpty() && !Profile.AvatarUrl.IsEmpty())
	{
		Progress.SelectedAvatarUrl = Profile.AvatarUrl;
		SaveProgress();
	}
}

void UBHAccountSubsystem::HandleProviderHealthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bLoginStartRequestInFlight = false;

	if (PendingProvider.IsEmpty())
	{
		SetLastAccountMessage(TEXT("Account login was cancelled."));
		return;
	}

	const FString ProviderName = PendingProvider.Equals(TEXT("google"), ESearchCase::IgnoreCase) ? TEXT("Google") : TEXT("Microsoft");
	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		SetLastAccountMessage(FString::Printf(TEXT("%s account login is unavailable. Account backend is not reachable."), *ProviderName));
		PendingProvider.Reset();
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (!StringToJsonObject(Response->GetContentAsString(), JsonObject))
	{
		SetLastAccountMessage(TEXT("Account backend returned invalid service status."));
		PendingProvider.Reset();
		return;
	}

	const TSharedPtr<FJsonObject>* ProvidersObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("providers"), ProvidersObject) || !ProvidersObject || !ProvidersObject->IsValid())
	{
		SetLastAccountMessage(TEXT("Account backend does not report provider readiness."));
		PendingProvider.Reset();
		return;
	}

	bool bProviderReady = false;
	if (!(*ProvidersObject)->TryGetBoolField(PendingProvider, bProviderReady) || !bProviderReady)
	{
		SetLastAccountMessage(FString::Printf(TEXT("%s login is not enabled for this build yet. Continue as Guest or use a build connected to the production account backend."), *ProviderName));
		PendingProvider.Reset();
		return;
	}

	LaunchPendingProviderLogin();
}

void UBHAccountSubsystem::HandleLoginPollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bLoginRequestInFlight = false;

	if (!bWasSuccessful || !Response.IsValid())
	{
		SetLastAccountMessage(TEXT("Account login check failed. Is the account backend running?"));
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (!StringToJsonObject(Response->GetContentAsString(), JsonObject))
	{
		SetLastAccountMessage(TEXT("Account backend returned invalid login JSON."));
		return;
	}

	const FString Status = JsonString(JsonObject, TEXT("status")).ToLower();
	if (Status == TEXT("pending"))
	{
		SetLastAccountMessage(TEXT("Waiting for browser sign-in to finish..."));
		return;
	}

	if (Status != TEXT("authorized"))
	{
		StopLoginPolling();
		const FString Error = JsonString(JsonObject, TEXT("error"));
		SetLastAccountMessage(Error.IsEmpty() ? TEXT("Account login failed.") : Error);
		return;
	}

	const FString SessionToken = JsonString(JsonObject, TEXT("session_token"));
	const TSharedPtr<FJsonObject>* PlayerObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("player"), PlayerObject) || SessionToken.IsEmpty())
	{
		StopLoginPolling();
		SetLastAccountMessage(TEXT("Account login succeeded, but the backend response was missing player data."));
		return;
	}

	ApplyBackendPlayerJson(*PlayerObject, SessionToken);
	StopLoginPolling();
	SetLastAccountMessage(FString::Printf(TEXT("Signed in as %s."), *Profile.DisplayName));

	FString SyncMessage;
	SyncProgress(SyncMessage);
}

void UBHAccountSubsystem::HandleSyncResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	bSyncRequestInFlight = false;

	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		SetLastAccountMessage(TEXT("Progress sync failed. Local save is still intact."));
		return;
	}

	SetLastAccountMessage(TEXT("Progress synced."));
}
