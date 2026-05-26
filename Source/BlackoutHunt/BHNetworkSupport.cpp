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

		FSHA256Signature Signature;
		if (!FPlatformMisc::GetSHA256Signature(FileBytes.GetData(), static_cast<uint32>(FileBytes.Num()), Signature))
		{
			OutMessage = TEXT("Could not verify bundled tunnel agent signature on this platform.");
			return false;
		}

		const FString ActualHash = Signature.ToString();
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

	bool TryExtractTunnelAddressFromLog(const FString& TunnelLogPath, int32 LocalPort, FString& OutAddress)
	{
		FString LogText;
		if (TunnelLogPath.IsEmpty() || !FFileHelper::LoadFileToString(LogText, *TunnelLogPath))
		{
			return false;
		}

		LogText.ReplaceInline(TEXT("\r"), TEXT(" "));
		LogText.ReplaceInline(TEXT("\n"), TEXT(" "));

		TArray<FString> Tokens;
		LogText.ParseIntoArrayWS(Tokens);
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
				return true;
			}
		}

		return false;
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

		Result.Message = FString::Printf(
			TEXT("Internet tunnel agent is already running, but no usable allocation was found yet. Use OPEN TUNNEL SETUP only if you need to change the playit tunnel. It must be Custom UDP to local 127.0.0.1:%d. Agent path: %s. Agent log: %s"),
			Result.LocalPort,
			Result.AgentPath.IsEmpty() ? TEXT("already running") : *Result.AgentPath,
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
