// Copyright (c) 2026 Adam Rosta. All Rights Reserved.
// This source code is proprietary and confidential.
// Unauthorized copying or distribution is strictly prohibited.

#include "BHNetworkSupport.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Containers/StringConv.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

#if PLATFORM_WINDOWS
THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include <bcrypt.h>
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace
{
	constexpr int32 BHDefaultGamePort = 7777;
	constexpr int32 BHMaxJoinAddressChars = 512;
	constexpr int32 BHMaxInviteCodeChars = 1024;
	constexpr int32 BHMaxInviteDecodedBytes = 512;

	const TCHAR* GameHotspotPrefix()
	{
		return TEXT("BlackoutHunt-");
	}

	const TCHAR* PlayitDownloadUrl()
	{
		return TEXT("https://playit.gg/download");
	}

	const TCHAR* PlayitTunnelSetupUrl()
	{
		return TEXT("https://playit.gg/account/setup/new-tunnel");
	}

	const TCHAR* JoinInvitePrefix()
	{
		return TEXT("BH1:");
	}

	const TCHAR* JoinInviteLinkPrefix()
	{
		return TEXT("blackouthunt://join/");
	}

	const TCHAR* ExpectedPlayitSha256()
	{
		return TEXT("88000d40af7a8e5a0548d27d71c0cad7d5f4b91fd85f6e9297237ac8b57fbdc9");
	}

	FString FilterForNetshToken(const FString& Input, const int32 MaxLength)
	{
		FString Output;
		Output.Reserve(FMath::Min(Input.Len(), MaxLength));

		for (int32 Index = 0; Index < Input.Len() && Output.Len() < MaxLength; ++Index)
		{
			const TCHAR Ch = Input[Index];
			if (FChar::IsAlnum(Ch) || Ch == TEXT('-') || Ch == TEXT('_'))
			{
				Output.AppendChar(Ch);
			}
		}

		return Output;
	}

	FString NormalizeSsid(const FString& PreferredSsid)
	{
		FString Ssid = FilterForNetshToken(PreferredSsid.TrimStartAndEnd(), 32);
		if (Ssid.IsEmpty())
		{
			return FBHNetworkSupport::MakeDefaultGameSsid();
		}

		if (!FBHNetworkSupport::IsGameHotspotSsid(Ssid))
		{
			Ssid = FString(GameHotspotPrefix()) + Ssid;
		}

		return Ssid.Left(32);
	}

	FString NormalizePassphrase(const FString& PreferredPassphrase)
	{
		FString Passphrase = FilterForNetshToken(PreferredPassphrase.TrimStartAndEnd(), 63);
		if (Passphrase.Len() < 8)
		{
			return FBHNetworkSupport::MakeDefaultPassphrase();
		}

		return Passphrase;
	}

	FString CompactProcessOutput(FString Output)
	{
		Output.ReplaceInline(TEXT("\r"), TEXT(" "));
		Output.ReplaceInline(TEXT("\n"), TEXT(" "));
		Output.TrimStartAndEndInline();

		constexpr int32 MaxOutputLength = 280;
		if (Output.Len() > MaxOutputLength)
		{
			Output = Output.Left(MaxOutputLength) + TEXT("...");
		}

		return Output;
	}

#if PLATFORM_WINDOWS
	bool RunNetsh(const FString& Args, FString& OutCombinedOutput)
	{
		int32 ReturnCode = -1;
		FString StdOut;
		FString StdErr;
		const bool bLaunched = FPlatformProcess::ExecProcess(TEXT("netsh"), *Args, &ReturnCode, &StdOut, &StdErr);

		OutCombinedOutput = CompactProcessOutput(StdOut + TEXT(" ") + StdErr);
		return bLaunched && ReturnCode == 0;
	}
#endif

	int32 NormalizePort(int32 LocalPort)
	{
		return FMath::Clamp(LocalPort <= 0 ? BHDefaultGamePort : LocalPort, 1, 65535);
	}

	bool TryParsePort(const FString& PortText, int32 DefaultPort, int32& OutPort)
	{
		FString TrimmedPort = PortText.TrimStartAndEnd();
		if (TrimmedPort.IsEmpty())
		{
			OutPort = NormalizePort(DefaultPort);
			return true;
		}

		for (const TCHAR Ch : TrimmedPort)
		{
			if (!FChar::IsDigit(Ch))
			{
				OutPort = NormalizePort(DefaultPort);
				return false;
			}
		}

		const int32 ParsedPort = FCString::Atoi(*TrimmedPort);
		if (ParsedPort < 1 || ParsedPort > 65535)
		{
			OutPort = NormalizePort(DefaultPort);
			return false;
		}

		OutPort = ParsedPort;
		return true;
	}

	bool IsInviteTokenTerminator(const TCHAR Ch)
	{
		return Ch <= TEXT(' ')
			|| Ch == TEXT(',')
			|| Ch == TEXT(';')
			|| Ch == TEXT('/')
			|| Ch == TEXT('?')
			|| Ch == TEXT('#')
			|| Ch == TEXT(')')
			|| Ch == TEXT(']')
			|| Ch == TEXT('}')
			|| Ch == TEXT('>')
			|| Ch == TEXT('"')
			|| Ch == TEXT('\'');
	}

	FString TrimInviteToken(FString Token)
	{
		Token.TrimStartAndEndInline();

		int32 EndIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Token.Len(); ++Index)
		{
			if (IsInviteTokenTerminator(Token[Index]))
			{
				EndIndex = Index;
				break;
			}
		}

		if (EndIndex != INDEX_NONE)
		{
			Token = Token.Left(EndIndex);
		}

		return Token;
	}

	bool IsInvitePayloadChar(const TCHAR Ch)
	{
		return FChar::IsAlnum(Ch) || Ch == TEXT('-') || Ch == TEXT('_') || Ch == TEXT('=');
	}

	FString ExtractInvitePayload(FString Input)
	{
		Input.TrimStartAndEndInline();
		if (Input.IsEmpty() || Input.Len() > BHMaxInviteCodeChars)
		{
			return FString();
		}

		FString Payload;
		if (Input.StartsWith(JoinInviteLinkPrefix(), ESearchCase::IgnoreCase))
		{
			Payload = Input.Mid(FCString::Strlen(JoinInviteLinkPrefix()));
			Payload = TrimInviteToken(Payload);
			Payload.RemoveFromStart(JoinInvitePrefix(), ESearchCase::IgnoreCase);
		}
		else
		{
			const int32 PrefixIndex = Input.Find(JoinInvitePrefix(), ESearchCase::IgnoreCase);
			if (PrefixIndex == INDEX_NONE)
			{
				return FString();
			}

			Payload = TrimInviteToken(Input.Mid(PrefixIndex + FCString::Strlen(JoinInvitePrefix())));
		}

		if (Payload.IsEmpty() || Payload.Len() > BHMaxInviteCodeChars)
		{
			return FString();
		}

		for (const TCHAR Ch : Payload)
		{
			if (!IsInvitePayloadChar(Ch))
			{
				return FString();
			}
		}

		return Payload;
	}

	bool TryDecodeInviteAddress(const FString& Input, FString& OutAddress)
	{
		FString Payload = ExtractInvitePayload(Input);
		if (Payload.IsEmpty())
		{
			return false;
		}

		Payload.ReplaceInline(TEXT("-"), TEXT("+"));
		Payload.ReplaceInline(TEXT("_"), TEXT("/"));
		while (Payload.Len() % 4 != 0)
		{
			Payload.AppendChar(TEXT('='));
		}

		TArray<uint8> DecodedBytes;
		if (!FBase64::Decode(Payload, DecodedBytes)
			|| DecodedBytes.IsEmpty()
			|| DecodedBytes.Num() > BHMaxInviteDecodedBytes)
		{
			return false;
		}

		const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(DecodedBytes.GetData()), DecodedBytes.Num());
		if (Converted.Length() <= 0 || Converted.Length() > BHMaxJoinAddressChars)
		{
			return false;
		}

		OutAddress = FString(Converted.Length(), Converted.Get());
		return true;
	}

	bool LooksLikeInviteAddress(const FString& Input)
	{
		const FString TrimmedInput = Input.TrimStartAndEnd();
		return TrimmedInput.Find(JoinInvitePrefix(), ESearchCase::IgnoreCase) != INDEX_NONE
			|| TrimmedInput.StartsWith(JoinInviteLinkPrefix(), ESearchCase::IgnoreCase);
	}

	bool IsSafeJoinHost(const FString& Host)
	{
		if (Host.IsEmpty() || Host.Len() > 253)
		{
			return false;
		}

		for (const TCHAR Ch : Host)
		{
			if (!FChar::IsAlnum(Ch)
				&& Ch != TEXT('.')
				&& Ch != TEXT('-')
				&& Ch != TEXT('_')
				&& Ch != TEXT(':'))
			{
				return false;
			}
		}

		return true;
	}

	FString CleanJoinAddress(FString Address)
	{
		Address.TrimStartAndEndInline();
		if (Address.IsEmpty() || Address.Len() > BHMaxJoinAddressChars)
		{
			return FString();
		}

		Address.ReplaceInline(TEXT(" "), TEXT(""));
		if (Address.StartsWith(TEXT("blackouthunt://"), ESearchCase::IgnoreCase))
		{
			return FString();
		}

		Address.RemoveFromStart(TEXT("http://"), ESearchCase::IgnoreCase);
		Address.RemoveFromStart(TEXT("https://"), ESearchCase::IgnoreCase);
		// playit.gg / tunnel UIs sometimes present the endpoint as udp://host:port. Strip the scheme so a
		// pasted tunnel address normalizes to host:port instead of being rejected by the "://" guard below.
		Address.RemoveFromStart(TEXT("udp://"), ESearchCase::IgnoreCase);

		if (Address.Contains(TEXT("://")))
		{
			return FString();
		}

		int32 StripIndex = INDEX_NONE;
		const TCHAR Delimiters[] = { TCHAR('/'), TCHAR('?'), TCHAR('#') };
		for (const TCHAR Delimiter : Delimiters)
		{
			int32 CandidateIndex = INDEX_NONE;
			if (Address.FindChar(Delimiter, CandidateIndex)
				&& (StripIndex == INDEX_NONE || CandidateIndex < StripIndex))
			{
				StripIndex = CandidateIndex;
			}
		}

		if (StripIndex != INDEX_NONE)
		{
			return FString();
		}

		return Address;
	}

	bool HasSingleColon(const FString& Address, int32& OutColonIndex)
	{
		OutColonIndex = INDEX_NONE;
		if (!Address.FindChar(TEXT(':'), OutColonIndex))
		{
			return false;
		}

		int32 LastColonIndex = INDEX_NONE;
		Address.FindLastChar(TEXT(':'), LastColonIndex);
		return LastColonIndex == OutColonIndex;
	}

	FString BuildJoinAddress(const FString& Host, int32 Port)
	{
		if (Host.Contains(TEXT(":")) && !Host.StartsWith(TEXT("[")))
		{
			return FString::Printf(TEXT("[%s]:%d"), *Host, Port);
		}

		return FString::Printf(TEXT("%s:%d"), *Host, Port);
	}

	void OpenExternalUrl(const TCHAR* Url)
	{
		FPlatformProcess::LaunchURL(Url, nullptr, nullptr);
	}

	FString QuoteCommandLineArg(FString Arg)
	{
		Arg.ReplaceInline(TEXT("\""), TEXT(""));
		return FString::Printf(TEXT("\"%s\""), *Arg);
	}

#if PLATFORM_WINDOWS
	FProcHandle PlayitAgentProcess;
	FString PlayitAgentPath;

	FString GetPlayitLogPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Logs"),
			TEXT("BlackoutHuntPlayit.log")));
	}

	FString BytesToLowerHex(const uint8* Bytes, const int32 NumBytes)
	{
		static constexpr TCHAR Hex[] = TEXT("0123456789abcdef");
		FString Result;
		Result.Reserve(NumBytes * 2);
		for (int32 Index = 0; Index < NumBytes; ++Index)
		{
			const uint8 Byte = Bytes[Index];
			Result.AppendChar(Hex[(Byte >> 4) & 0x0f]);
			Result.AppendChar(Hex[Byte & 0x0f]);
		}
		return Result;
	}

	bool ComputeSha256Hex(TArray<uint8>& Bytes, FString& OutHash)
	{
		BCRYPT_ALG_HANDLE AlgorithmHandle = nullptr;
		BCRYPT_HASH_HANDLE HashHandle = nullptr;

		auto CloseHandles = [&]()
		{
			if (HashHandle)
			{
				BCryptDestroyHash(HashHandle);
				HashHandle = nullptr;
			}
			if (AlgorithmHandle)
			{
				BCryptCloseAlgorithmProvider(AlgorithmHandle, 0);
				AlgorithmHandle = nullptr;
			}
		};

		NTSTATUS Status = BCryptOpenAlgorithmProvider(&AlgorithmHandle, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
		if (Status < 0)
		{
			return false;
		}

		DWORD BytesWritten = 0;
		DWORD HashObjectSize = 0;
		Status = BCryptGetProperty(AlgorithmHandle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&HashObjectSize), sizeof(HashObjectSize), &BytesWritten, 0);
		if (Status < 0 || HashObjectSize == 0)
		{
			CloseHandles();
			return false;
		}

		DWORD HashLength = 0;
		Status = BCryptGetProperty(AlgorithmHandle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&HashLength), sizeof(HashLength), &BytesWritten, 0);
		if (Status < 0 || HashLength == 0)
		{
			CloseHandles();
			return false;
		}

		TArray<uint8> HashObject;
		HashObject.SetNumUninitialized(static_cast<int32>(HashObjectSize));
		Status = BCryptCreateHash(AlgorithmHandle, &HashHandle, HashObject.GetData(), HashObjectSize, nullptr, 0, 0);
		if (Status < 0)
		{
			CloseHandles();
			return false;
		}

		Status = BCryptHashData(HashHandle, Bytes.GetData(), static_cast<ULONG>(Bytes.Num()), 0);
		if (Status < 0)
		{
			CloseHandles();
			return false;
		}

		TArray<uint8> Digest;
		Digest.SetNumUninitialized(static_cast<int32>(HashLength));
		Status = BCryptFinishHash(HashHandle, Digest.GetData(), HashLength, 0);
		if (Status < 0)
		{
			CloseHandles();
			return false;
		}

		OutHash = BytesToLowerHex(Digest.GetData(), Digest.Num());
		CloseHandles();
		return true;
	}

	bool VerifyPlayitExecutable(const FString& Path, FString& OutMessage)
	{
		const int64 FileSize = IFileManager::Get().FileSize(*Path);
		constexpr int64 MaxAgentBytes = 64ll * 1024ll * 1024ll;
		if (FileSize <= 0 || FileSize > MaxAgentBytes)
		{
			OutMessage = FString::Printf(TEXT("Bundled tunnel agent has an unexpected size at %s."), *Path);
			return false;
		}

		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *Path))
		{
			OutMessage = FString::Printf(TEXT("Could not read bundled tunnel agent at %s."), *Path);
			return false;
		}

		FString ActualHash;
		if (!ComputeSha256Hex(FileBytes, ActualHash))
		{
			OutMessage = TEXT("Could not verify bundled tunnel agent signature on this platform.");
			return false;
		}

		if (!ActualHash.Equals(ExpectedPlayitSha256(), ESearchCase::IgnoreCase))
		{
			OutMessage = FString::Printf(
				TEXT("Bundled tunnel agent hash mismatch. Expected %s but found %s. Not launching this executable."),
				ExpectedPlayitSha256(),
				*ActualHash);
			return false;
		}

		return true;
	}

	bool TryFindVerifiedPlayitExecutable(FString& OutPath, FString& OutFailureMessage)
	{
		const TArray<FString> CandidatePaths = {
			FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("playit.exe")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty"), TEXT("Playit"), TEXT("playit.exe")),
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), TEXT("Win64"), TEXT("playit.exe"))
		};

		bool bFoundCandidate = false;
		for (const FString& CandidatePath : CandidatePaths)
		{
			if (FPaths::FileExists(CandidatePath))
			{
				bFoundCandidate = true;
				const FString FullPath = FPaths::ConvertRelativePathToFull(CandidatePath);
				if (VerifyPlayitExecutable(FullPath, OutFailureMessage))
				{
					OutPath = FullPath;
					return true;
				}
			}
		}

		if (!bFoundCandidate)
		{
			OutFailureMessage = TEXT("No bundled playit agent was found.");
		}

		return false;
	}

	void ClearFinishedPlayitProcess()
	{
		if (PlayitAgentProcess.IsValid() && !FPlatformProcess::IsProcRunning(PlayitAgentProcess))
		{
			FPlatformProcess::CloseProc(PlayitAgentProcess);
			PlayitAgentProcess.Reset();
			PlayitAgentPath.Reset();
		}
	}

	bool IsLocalTunnelAddress(const FString& Address)
	{
		return Address.StartsWith(TEXT("127."), ESearchCase::IgnoreCase)
			|| Address.StartsWith(TEXT("localhost"), ESearchCase::IgnoreCase)
			|| Address.StartsWith(TEXT("0.0.0.0"), ESearchCase::IgnoreCase)
			|| Address.StartsWith(TEXT("[::1]"), ESearchCase::IgnoreCase);
	}

	FString StripLogTokenDelimiters(FString Token)
	{
		Token.TrimStartAndEndInline();
		while (!Token.IsEmpty())
		{
			const TCHAR Last = Token[Token.Len() - 1];
			if (Last != TEXT('"') && Last != TEXT('\'') && Last != TEXT(',') && Last != TEXT(')') && Last != TEXT(']') && Last != TEXT('}'))
			{
				break;
			}
			Token.LeftChopInline(1);
		}
		while (!Token.IsEmpty())
		{
			const TCHAR First = Token[0];
			if (First != TEXT('"') && First != TEXT('\'') && First != TEXT('(') && First != TEXT('[') && First != TEXT('{'))
			{
				break;
			}
			Token.RightChopInline(1);
		}
		return Token;
	}

	bool IsAgentControlPlaneLogLine(const FString& Line)
	{
		// playitd 1.x logs control-plane diagnostics that contain host:port tokens which are NOT the
		// public tunnel allocation: the tunnel server address ("got initial pong ... tunnel_addr:
		// 69.9.185.1:5525") and our own public address ("client_addr: x.x.x.x:port" inside Pong {...}).
		// Harvesting a join address from those lines would hand students the playit control server or
		// the host's raw WAN address. Skip them entirely.
		return Line.Contains(TEXT("pong"), ESearchCase::IgnoreCase)
			|| Line.Contains(TEXT("address_selector"), ESearchCase::IgnoreCase)
			|| Line.Contains(TEXT("client_addr"), ESearchCase::IgnoreCase)
			|| Line.Contains(TEXT("tunnel_addr"), ESearchCase::IgnoreCase);
	}

	bool TryExtractTunnelAddressFromLog(const FString& TunnelLogPath, int32 LocalPort, FString& OutAddress)
	{
		FString LogText;
		if (TunnelLogPath.IsEmpty() || !FFileHelper::LoadFileToString(LogText, *TunnelLogPath))
		{
			return false;
		}

		TArray<FString> LogLines;
		LogText.ParseIntoArrayLines(LogLines);

		// Keep the LAST valid allocation in the log, not the first. The agent appends to (or rotates) this
		// log, so after a tunnel restart the file still contains the OLD allocation earlier in the file;
		// returning the first match would hand out a dead address. The newest line is the live one.
		bool bFound = false;
		for (const FString& Line : LogLines)
		{
			if (IsAgentControlPlaneLogLine(Line))
			{
				continue;
			}

			TArray<FString> Tokens;
			Line.ParseIntoArrayWS(Tokens);
			for (FString Token : Tokens)
			{
				Token = StripLogTokenDelimiters(Token);

				if (!Token.Contains(TEXT(":")) || Token.Contains(TEXT("://")))
				{
					continue;
				}

				const FString Candidate = FBHNetworkSupport::NormalizeJoinAddress(Token, LocalPort);
				if (!Candidate.IsEmpty() && !IsLocalTunnelAddress(Candidate))
				{
					OutAddress = Candidate;
					bFound = true;
				}
			}
		}

		return bFound;
	}

	// playitd 1.x (the daemon shipped as playit.exe since v1.0) never logs the public allocation
	// address, so TryExtractTunnelAddressFromLog can never succeed against it. The reliable in-log
	// signal that the tunnel is being serviced is a fresh agent session: while connected the agent
	// re-authenticates its UDP channel and logs "udp session details received" every ~10 seconds.
	// Healthy = that marker exists in the RECENT TAIL of the log AND the agent wrote to the log within
	// the last 90 seconds. When healthy, callers should treat the tunnel as ready and use the
	// CONFIGURED classroom endpoint (ClassroomJoinEndpoints) as the join address.
	bool LogShowsHealthyAgentSession(const FString& TunnelLogPath)
	{
		if (TunnelLogPath.IsEmpty())
		{
			return false;
		}

		const FDateTime LastWriteUtc = IFileManager::Get().GetTimeStamp(*TunnelLogPath);
		if (LastWriteUtc == FDateTime::MinValue())
		{
			return false;
		}

		const FTimespan SinceLastWrite = FDateTime::UtcNow() - LastWriteUtc;
		if (SinceLastWrite > FTimespan::FromSeconds(90.0) || SinceLastWrite < FTimespan::FromSeconds(-90.0))
		{
			return false;
		}

		FString LogText;
		if (!FFileHelper::LoadFileToString(LogText, *TunnelLogPath))
		{
			return false;
		}

		// The mtime window above only proves SOMETHING wrote recently — a dead session spamming
		// reconnect errors satisfies it indefinitely. The marker scan therefore has to be tail-only;
		// see LogTextShowsRecentAgentSession for the policy.
		return FBHNetworkSupport::LogTextShowsRecentAgentSession(LogText);
	}

#endif
}

FBHHotspotLaunchResult FBHNetworkSupport::StartGameHotspot(const FString& PreferredSsid, const FString& PreferredPassphrase)
{
	FBHHotspotLaunchResult Result;
	Result.Ssid = NormalizeSsid(PreferredSsid);
	Result.Passphrase = NormalizePassphrase(PreferredPassphrase);

#if PLATFORM_WINDOWS
	FString ConfigureOutput;
	const FString ConfigureArgs = FString::Printf(
		TEXT("wlan set hostednetwork mode=allow ssid=\"%s\" key=\"%s\" keyUsage=persistent"),
		*Result.Ssid,
		*Result.Passphrase);

	if (!RunNetsh(ConfigureArgs, ConfigureOutput))
	{
		Result.Message = FString::Printf(
			TEXT("Could not configure the game hotspot. Run the game as administrator and make sure the Wi-Fi adapter supports Windows Hosted Network. Details: %s"),
			*ConfigureOutput);
		return Result;
	}

	FString StartOutput;
	if (!RunNetsh(TEXT("wlan start hostednetwork"), StartOutput))
	{
		Result.Message = FString::Printf(
			TEXT("Configured hotspot %s, but Windows could not start it. The adapter may not support Hosted Network, Mobile Hotspot may already own the radio, or administrator permission is required. Details: %s"),
			*Result.Ssid,
			*StartOutput);
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(
		TEXT("Game hotspot started. SSID: %s  Password: %s. Host a match, then players connect to this Wi-Fi and join the host address on port 7777."),
		*Result.Ssid,
		*Result.Passphrase);
#else
	Result.Message = TEXT("Game hotspot creation is currently implemented for Windows builds only.");
#endif

	return Result;
}

FBHHotspotLaunchResult FBHNetworkSupport::StopGameHotspot(const FString& KnownSsid)
{
	FBHHotspotLaunchResult Result;
	Result.Ssid = KnownSsid;

	if (!IsGameHotspotSsid(KnownSsid))
	{
		Result.Message = TEXT("No game-created hotspot is active in this session.");
		return Result;
	}

#if PLATFORM_WINDOWS
	FString StopOutput;
	if (!RunNetsh(TEXT("wlan stop hostednetwork"), StopOutput))
	{
		Result.Message = FString::Printf(TEXT("Could not stop the game hotspot. Details: %s"), *StopOutput);
		return Result;
	}

	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Game hotspot stopped: %s."), *KnownSsid);
#else
	Result.Message = TEXT("Game hotspot control is currently implemented for Windows builds only.");
#endif

	return Result;
}

FString FBHNetworkSupport::MakeDefaultGameSsid()
{
	const FString GuidDigits = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(6).ToUpper();
	return FString::Printf(TEXT("%s%s"), GameHotspotPrefix(), *GuidDigits);
}

FString FBHNetworkSupport::MakeDefaultPassphrase()
{
	return FString::Printf(TEXT("BH%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(14).ToUpper());
}

bool FBHNetworkSupport::IsGameHotspotSsid(const FString& Ssid)
{
	return Ssid.StartsWith(GameHotspotPrefix(), ESearchCase::IgnoreCase);
}

FString FBHNetworkSupport::NormalizeJoinAddress(const FString& Address, int32 DefaultPort)
{
	FString DecodedAddress;
	const bool bDecodedInvite = TryDecodeInviteAddress(Address, DecodedAddress);
	if (!bDecodedInvite && LooksLikeInviteAddress(Address))
	{
		return FString();
	}

	const FString& AddressToNormalize = bDecodedInvite ? DecodedAddress : Address;

	FString CleanAddress = CleanJoinAddress(AddressToNormalize);
	if (CleanAddress.IsEmpty())
	{
		return FString();
	}

	FString Host = CleanAddress;
	int32 Port = NormalizePort(DefaultPort);

	if (CleanAddress.StartsWith(TEXT("[")))
	{
		int32 BracketIndex = INDEX_NONE;
		if (!CleanAddress.FindChar(TEXT(']'), BracketIndex))
		{
			return FString();
		}

		Host = CleanAddress.Mid(1, BracketIndex - 1);
		if (CleanAddress.IsValidIndex(BracketIndex + 1))
		{
			if (CleanAddress[BracketIndex + 1] != TEXT(':'))
			{
				return FString();
			}

			if (!TryParsePort(CleanAddress.Mid(BracketIndex + 2), DefaultPort, Port))
			{
				return FString();
			}
		}
	}
	else
	{
		int32 ColonIndex = INDEX_NONE;
		if (HasSingleColon(CleanAddress, ColonIndex))
		{
			Host = CleanAddress.Left(ColonIndex);
			if (!TryParsePort(CleanAddress.Mid(ColonIndex + 1), DefaultPort, Port))
			{
				return FString();
			}
		}
	}

	Host.TrimStartAndEndInline();
	if (!IsSafeJoinHost(Host))
	{
		return FString();
	}

	return BuildJoinAddress(Host, Port);
}

FString FBHNetworkSupport::NormalizePreferredJoinEndpoint(const TArray<FString>& Endpoints, int32 DefaultPort)
{
	for (const FString& Endpoint : Endpoints)
	{
		const FString NormalizedEndpoint = NormalizeJoinAddress(Endpoint, DefaultPort);
		if (!NormalizedEndpoint.IsEmpty())
		{
			return NormalizedEndpoint;
		}
	}

	return FString();
}

FString FBHNetworkSupport::MakeJoinInviteCode(const FString& Address, int32 DefaultPort)
{
	const FString NormalizedAddress = NormalizeJoinAddress(Address, DefaultPort);
	if (NormalizedAddress.IsEmpty())
	{
		return FString();
	}

	const FTCHARToUTF8 Converted(*NormalizedAddress);
	TArray<uint8> AddressBytes;
	AddressBytes.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());

	FString Payload = FBase64::Encode(AddressBytes);
	Payload.ReplaceInline(TEXT("+"), TEXT("-"));
	Payload.ReplaceInline(TEXT("/"), TEXT("_"));
	Payload.ReplaceInline(TEXT("="), TEXT(""));

	return FString(JoinInvitePrefix()) + Payload;
}

FString FBHNetworkSupport::ResolveLocalJoinAddress(int32 LocalPort)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return BuildJoinAddress(TEXT("127.0.0.1"), NormalizePort(LocalPort));
	}

	bool bCanBindAll = false;
	const TSharedRef<FInternetAddr> LocalAddress = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
	if (!LocalAddress->IsValid())
	{
		return BuildJoinAddress(TEXT("127.0.0.1"), NormalizePort(LocalPort));
	}

	return BuildJoinAddress(LocalAddress->ToString(false), NormalizePort(LocalPort));
}

FBHInternetTunnelResult FBHNetworkSupport::StartInternetTunnel(int32 LocalPort)
{
	FBHInternetTunnelResult Result;
	Result.LocalPort = NormalizePort(LocalPort);

#if PLATFORM_WINDOWS
	Result.LogPath = GetPlayitLogPath();
	ClearFinishedPlayitProcess();

	if (PlayitAgentProcess.IsValid())
	{
		Result.bSuccess = true;
		Result.AgentPath = PlayitAgentPath;
		if (TryExtractTunnelAddressFromLog(Result.LogPath, Result.LocalPort, Result.TunnelAddress))
		{
			Result.bTunnelReady = true;
			Result.Message = FString::Printf(
				TEXT("Tunnel ready: %s. Agent path: %s. Agent log: %s"),
				*Result.TunnelAddress,
				Result.AgentPath.IsEmpty() ? TEXT("already running") : *Result.AgentPath,
				*Result.LogPath);
			return Result;
		}

		if (LogShowsHealthyAgentSession(Result.LogPath))
		{
			Result.bTunnelReady = true;
			Result.Message = FString::Printf(
				TEXT("Tunnel agent session is healthy (control + UDP channel up). Students join via the configured classroom endpoint. Local game port: 127.0.0.1:%d. Agent log: %s"),
				Result.LocalPort,
				*Result.LogPath);
			return Result;
		}

		Result.Message = FString::Printf(
			TEXT("Internet tunnel agent is already running, but no usable allocation was found yet. Use OPEN TUNNEL SETUP only if you need to change the playit tunnel. It must be Custom UDP to local 127.0.0.1:%d. Agent path: %s. Agent log: %s"),
			Result.LocalPort,
			Result.AgentPath.IsEmpty() ? TEXT("already running") : *Result.AgentPath,
			*Result.LogPath);
		return Result;
	}

	// Single-owner rule: one playit secret backs ONE live agent process. If a playit.exe is already
	// running outside our process handle (the playit desktop app / playitd service, an orphan from a
	// crashed game instance, or a second copy of the game), spawning another agent on the same secret
	// makes the two instances fight over the UDP session. Adopt the existing agent instead.
	if (FPlatformProcess::IsApplicationRunning(TEXT("playit.exe")))
	{
		Result.bSuccess = true;
		if (TryExtractTunnelAddressFromLog(Result.LogPath, Result.LocalPort, Result.TunnelAddress))
		{
			Result.bTunnelReady = true;
			Result.Message = FString::Printf(
				TEXT("Adopted an external playit agent. Tunnel ready: %s. Agent log: %s"),
				*Result.TunnelAddress,
				*Result.LogPath);
			return Result;
		}

		if (LogShowsHealthyAgentSession(Result.LogPath))
		{
			Result.bTunnelReady = true;
			Result.Message = FString::Printf(
				TEXT("Adopted an external playit agent with a healthy session. Students join via the configured classroom endpoint. Local game port: 127.0.0.1:%d. Agent log: %s"),
				Result.LocalPort,
				*Result.LogPath);
			return Result;
		}

		// An EXTERNAL agent writes its own log elsewhere, so the game-log health probes above can never
		// confirm it — this branch realistically only resolves once the user closes the outside copy.
		// Say so: a bare "waiting" with no actionable step left hosts stuck at class start.
		Result.Message = FString::Printf(
			TEXT("A playit agent is already running outside the game (desktop app or service); not starting a second one (one secret backs one live agent). Close the playit app/service and retry hosting so the game can run its own agent. Tunnel must be Custom UDP to local 127.0.0.1:%d. Agent log: %s"),
			Result.LocalPort,
			*Result.LogPath);
		return Result;
	}

	FString AgentLookupMessage;
	if (!TryFindVerifiedPlayitExecutable(Result.AgentPath, AgentLookupMessage))
	{
		Result.Message = FString::Printf(
			TEXT("%s No browser was opened automatically. Use OPEN TUNNEL SETUP only if this host needs Playit; otherwise share the LAN address. Playit tunnel should be Custom UDP to local 127.0.0.1:%d."),
			*AgentLookupMessage,
			Result.LocalPort);
		return Result;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Result.LogPath), true);
	const FString AgentParams = FString::Printf(TEXT("--log-path %s"), *QuoteCommandLineArg(Result.LogPath));
	PlayitAgentProcess = FPlatformProcess::CreateProc(
		*Result.AgentPath,
		*AgentParams,
		false,
		true,
		true,
		nullptr,
		0,
		nullptr,
		nullptr);

	if (!PlayitAgentProcess.IsValid())
	{
		Result.Message = FString::Printf(
			TEXT("Could not start the playit tunnel agent at %s. No browser was opened automatically. Use OPEN TUNNEL SETUP only if this host needs Playit; tunnel should be Custom UDP to local 127.0.0.1:%d. Agent log: %s"),
			*Result.AgentPath,
			Result.LocalPort,
			*Result.LogPath);
		return Result;
	}

	PlayitAgentPath = Result.AgentPath;
	Result.bSuccess = true;
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt tunnel agent path: %s"), *Result.AgentPath);
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt tunnel agent log path: %s"), *Result.LogPath);
	UE_LOG(LogTemp, Display, TEXT("BlackoutHunt tunnel startup result: started"));
	Result.Message = FString::Printf(
		TEXT("Internet tunnel agent started. Waiting for a Custom UDP allocation to local 127.0.0.1:%d. If setup is needed, press OPEN TUNNEL SETUP manually. Agent path: %s. Agent log: %s"),
		Result.LocalPort,
		*Result.AgentPath,
		*Result.LogPath);
#else
	Result.Message = TEXT("Internet tunnel helper is currently wired for Windows builds. No browser was opened automatically.");
#endif

	return Result;
}

FBHInternetTunnelResult FBHNetworkSupport::StopInternetTunnel()
{
	FBHInternetTunnelResult Result;
	Result.LocalPort = BHDefaultGamePort;

#if PLATFORM_WINDOWS
	ClearFinishedPlayitProcess();

	if (!PlayitAgentProcess.IsValid())
	{
		Result.Message = TEXT("No game-launched internet tunnel agent is running.");
		return Result;
	}

	FPlatformProcess::TerminateProc(PlayitAgentProcess, true);
	FPlatformProcess::CloseProc(PlayitAgentProcess);
	PlayitAgentProcess.Reset();
	PlayitAgentPath.Reset();

	// Delete the agent log so a later GetInternetTunnelStatus can't parse the now-dead allocation address
	// out of it and wrongly report "Tunnel ready", handing the host a BH1 join code for a tunnel that no
	// longer exists (students would then hit connection-refused with no host-side warning).
	const FString StoppedTunnelLogPath = GetPlayitLogPath();
	if (!StoppedTunnelLogPath.IsEmpty())
	{
		IFileManager::Get().Delete(*StoppedTunnelLogPath, false, true, true);
	}

	Result.bSuccess = true;
	Result.Message = TEXT("Game-launched internet tunnel agent stopped.");
#else
	Result.Message = TEXT("Internet tunnel helper is currently wired for Windows builds only.");
#endif

	return Result;
}

FBHInternetTunnelResult FBHNetworkSupport::GetInternetTunnelStatus(int32 LocalPort)
{
	FBHInternetTunnelResult Result;
	Result.LocalPort = NormalizePort(LocalPort);

#if PLATFORM_WINDOWS
	Result.LogPath = GetPlayitLogPath();
	ClearFinishedPlayitProcess();
	Result.bSuccess = PlayitAgentProcess.IsValid();
	Result.AgentPath = PlayitAgentPath;
	if (TryExtractTunnelAddressFromLog(Result.LogPath, Result.LocalPort, Result.TunnelAddress))
	{
		Result.bTunnelReady = true;
		Result.bSuccess = true;
		Result.Message = FString::Printf(
			TEXT("Tunnel ready: %s. Agent path: %s. Agent log: %s"),
			*Result.TunnelAddress,
			Result.AgentPath.IsEmpty() ? TEXT("unknown") : *Result.AgentPath,
			*Result.LogPath);
		return Result;
	}

	// playitd 1.x never logs the allocation address; a fresh, authenticated session in the log is the
	// readiness signal. The log freshness window (90s) also guarantees SOME agent process is alive,
	// covering agents the game did not spawn itself (external/adopted ones).
	if (LogShowsHealthyAgentSession(Result.LogPath))
	{
		Result.bSuccess = true;
		Result.bTunnelReady = true;
		Result.Message = FString::Printf(
			TEXT("Tunnel agent session is healthy (control + UDP channel up). Students join via the configured classroom endpoint. Local game port: 127.0.0.1:%d. Agent log: %s"),
			Result.LocalPort,
			*Result.LogPath);
		return Result;
	}

	Result.Message = Result.bSuccess
		? FString::Printf(TEXT("Tunnel agent running, but no usable allocation address was found yet. Create/select a Custom UDP tunnel to local 127.0.0.1:%d. Agent path: %s. Agent log: %s"),
			Result.LocalPort,
			Result.AgentPath.IsEmpty() ? TEXT("unknown") : *Result.AgentPath,
			*Result.LogPath)
		: FString::Printf(TEXT("Tunnel agent is not running. Agent log path: %s"), *Result.LogPath);
#else
	Result.Message = TEXT("Internet tunnel status is currently wired for Windows builds only.");
#endif

	return Result;
}

FBHInternetTunnelResult FBHNetworkSupport::OpenInternetTunnelSetup(int32 LocalPort)
{
	FBHInternetTunnelResult Result;
	Result.bSuccess = true;
	Result.LocalPort = NormalizePort(LocalPort);
	OpenExternalUrl(PlayitTunnelSetupUrl());
	Result.Message = FString::Printf(
		TEXT("Opened tunnel setup. Create a Custom UDP tunnel to local 127.0.0.1:%d, put its allocation host/port in the menu, then copy a join code."),
		Result.LocalPort);
	return Result;
}

bool FBHNetworkSupport::LogTextShowsRecentAgentSession(const FString& LogText, int32 TailCharsToScan)
{
	// 64KB of tail is hours of idle keep-alive lines but only minutes of reconnect spam — plenty for a
	// marker the live agent rewrites every ~10 seconds, and small enough that a marker stranded in the
	// old head of a long-running log (from a previous, now dead session) falls out of the window.
	const int32 TailLen = FMath::Clamp(TailCharsToScan, 0, LogText.Len());
	const FString Tail = LogText.Right(TailLen);
	return Tail.Contains(TEXT("udp session details received"))
		|| Tail.Contains(TEXT("agent registered"));
}
