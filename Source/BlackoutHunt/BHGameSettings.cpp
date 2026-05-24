#include "BHGameSettings.h"

UBHGameSettings::UBHGameSettings()
{
	MinPlayers = 2;
	MaxPlayers = 12;
	PrepSeconds = 45;
	HuntSeconds = 900;
	RequiredBreakers = 6;
	bAllowHostForceStart = false;
	bClassroomMode = true;
	bAllowStudentTeacherAdminControls = false;
	bAllowTunnelHelper = true;
	bAllowHotspotHelper = true;

	InteractDistance = 550.0f;
	CaptureDistance = 220.0f;

	FlashlightDrainPerSecond = 0.17f;
	ScanCooldownSeconds = 25.0f;
	DecoyCooldownSeconds = 10.0f;
	BatteryRefillAmount = 45.0f;

	DefaultMasterVolume = 1.0f;
	DefaultMusicVolume = 0.85f;
	DefaultUiVolume = 0.9f;

	DefaultBotCount = 5;
	DefaultBotDifficulty = EBHBotDifficulty::Normal;
	BotThinkInterval = 0.25f;
	BotSightRange = 2800.0f;
	BotHearingMemorySeconds = 12.0f;
	BotStuckSeconds = 6.0f;
	RevisionRoundSeconds = 600;
	RevisionClassThreshold = 70.0f;
	RevisionIndividualThreshold = 50.0f;
	RevisionScareIntensity = 2;
}
