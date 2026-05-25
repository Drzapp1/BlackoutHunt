#include "BHAccountSubsystem.h"

#include "BHAccountSettings.h"
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
			Url = FString(TEXT("http://")) + Url;
		}

		return TrimTrailingSlash(Url);
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
		return FFileHelper::SaveStringToFile(JsonObjectToString(Root), *Path);
	}

	bool LoadEncryptedCredentialFile(const FString& Path, FBHLocalCredentialRecord& OutRecord)
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
		if (ExpectedMac.IsEmpty() || ExpectedMac != MakePayloadMac(Iv, Ciphertext))
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

	const bool bHadCredential = IFileManager::Get().FileExists(*GetCredentialPath());
	if (bHadCredential)
	{
		IFileManager::Get().Delete(*GetCredentialPath(), false, true, true);
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
	FileManager.Delete(*GetProfilePath(), false, true, true);
	FileManager.Delete(*GetProgressPath(), false, true, true);
	FileManager.Delete(*GetCredentialPath(), false, true, true);

	Profile = FBHAccountProfile();
	Progress = FBHAccountProgress();

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

	++Progress.RoundsPlayed;
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

	Progress.XP += EarnedXP;
	Progress.LastUpdatedUtc = UtcNowString();
	SaveProgress();

	FString Message;
	SyncProgress(Message);
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
	PendingProvider.Reset();
}

void UBHAccountSubsystem::SetLastAccountMessage(const FString& Message)
{
	LastAccountMessage = Message;
	UE_LOG(LogTemp, Display, TEXT("%s"), *LastAccountMessage);
}

void UBHAccountSubsystem::LoadProfile()
{
	FString Input;
	if (!FFileHelper::LoadFileToString(Input, *GetProfilePath()))
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (StringToJsonObject(Input, JsonObject))
	{
		ApplyProfileJson(JsonObject);
	}
}

void UBHAccountSubsystem::LoadProgress()
{
	FString Input;
	if (!FFileHelper::LoadFileToString(Input, *GetProgressPath()))
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObject;
	if (StringToJsonObject(Input, JsonObject))
	{
		ApplyProgressJson(JsonObject);
	}
}

void UBHAccountSubsystem::SaveProfile() const
{
	IFileManager::Get().MakeDirectory(*GetAccountDirectory(), true);
	FFileHelper::SaveStringToFile(JsonObjectToString(ProfileToJson()), *GetProfilePath());
}

void UBHAccountSubsystem::SaveProgress() const
{
	IFileManager::Get().MakeDirectory(*GetAccountDirectory(), true);
	FFileHelper::SaveStringToFile(JsonObjectToString(ProgressToJson()), *GetProgressPath());
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
	JsonObject->SetNumberField(TEXT("xp"), Progress.XP);
	JsonObject->SetStringField(TEXT("selected_avatar_url"), Progress.SelectedAvatarUrl);
	JsonObject->SetStringField(TEXT("last_updated_utc"), Progress.LastUpdatedUtc);
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
	Progress.ProfileVersion = JsonInt(JsonObject, TEXT("profile_version"), 1);
	Progress.RoundsPlayed = JsonInt(JsonObject, TEXT("rounds_played"));
	Progress.HunterWins = JsonInt(JsonObject, TEXT("hunter_wins"));
	Progress.SurvivorWins = JsonInt(JsonObject, TEXT("survivor_wins"));
	Progress.Escapes = JsonInt(JsonObject, TEXT("escapes"));
	Progress.XP = JsonInt(JsonObject, TEXT("xp"));
	Progress.SelectedAvatarUrl = JsonString(JsonObject, TEXT("selected_avatar_url"));
	Progress.LastUpdatedUtc = JsonString(JsonObject, TEXT("last_updated_utc"));
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
