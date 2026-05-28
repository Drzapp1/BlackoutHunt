// BHLowSpecLauncher.cpp
//
// Tiny non-console (GUI-subsystem) launcher for locked-down school PCs that block .cmd /
// console execution. It starts BlackoutHunt.exe with low-spec, DX11, windowed arguments so a
// weak machine can reach the menu even when the engine would otherwise fail to pick a usable
// renderer at default settings.
//
// Design constraints (see Docs/LIVE_CLASSROOM_TEST_2026-05-29.md, task LT2):
//   * No console window  -> build with /SUBSYSTEM:WINDOWS (WinMain, not main).
//   * No absolute paths   -> resolve BlackoutHunt.exe relative to THIS launcher's own folder,
//                            so it survives zip/extract to any arbitrary student-machine folder
//                            (the rejected .lnk approach baked in absolute paths).
//   * No admin rights     -> plain CreateProcess of a sibling exe; nothing elevated.
//
// Deployment rule: this launcher MUST sit in the same folder as BlackoutHunt.exe and be started
// by double-click. It is an IT/developer fallback that must be BUILT and VALIDATED on the real
// school Windows image before it is added to a classroom package. Do not auto-stage an unbuilt
// or unvalidated binary into a release. See BHLowSpecLauncher.README.md.

#include <windows.h>
#include <string>

namespace
{
	void ShowError(const wchar_t* Message)
	{
		MessageBoxW(nullptr, Message, L"Blackout Hunt - Low Spec Launcher", MB_ICONERROR | MB_OK);
	}
}

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	wchar_t ModulePath[MAX_PATH] = { 0 };
	const DWORD Length = GetModuleFileNameW(nullptr, ModulePath, MAX_PATH);
	if (Length == 0 || Length >= MAX_PATH)
	{
		ShowError(L"Could not determine the launcher location.");
		return 1;
	}

	std::wstring Directory(ModulePath, Length);
	const size_t Slash = Directory.find_last_of(L"\\/");
	if (Slash == std::wstring::npos)
	{
		ShowError(L"Could not determine the launcher folder.");
		return 1;
	}
	Directory.resize(Slash + 1);

	const std::wstring GameExe = Directory + L"BlackoutHunt.exe";
	if (GetFileAttributesW(GameExe.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		ShowError(L"BlackoutHunt.exe was not found next to this launcher.\n"
			L"Keep this launcher in the same folder as BlackoutHunt.exe.");
		return 1;
	}

	// Low-spec / DX11 / windowed startup. CreateProcessW needs a writable command-line buffer.
	std::wstring CommandLine = L"\"" + GameExe + L"\" -d3d11 -BHVirtualBoxSafe -ResX=1280 -ResY=720 -WINDOWED";

	STARTUPINFOW StartupInfo = { sizeof(StartupInfo) };
	PROCESS_INFORMATION ProcessInfo = { 0 };
	if (!CreateProcessW(
			GameExe.c_str(),
			&CommandLine[0],
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			Directory.c_str(),
			&StartupInfo,
			&ProcessInfo))
	{
		ShowError(L"Failed to start BlackoutHunt.exe.");
		return 1;
	}

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	return 0;
}
