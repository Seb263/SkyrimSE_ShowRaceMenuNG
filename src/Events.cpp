#include "Events.h"

namespace Events
{
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!SettingsIni::bEnabled) return continueEvent;
		if (event->menuName.empty() || event->menuName != "RaceSex Menu") return continueEvent;

		if (event->opening) SRMCore::Main::BackupProcess();
		else SRMCore::Main::RestoreProcess();

		return continueEvent;
	}
	
	RE::BSEventNotifyControl ModEventSink::ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*)
	{
		if (!SettingsIni::bEnabled || SettingsIni::iVampireRaceSyncMode < 2) return continueEvent;
		if (!event->subject || !event->subject->IsPlayerRef()) return continueEvent;

		SRMCore::Main::SwitchRaceProcess();

		return continueEvent;
	}
}
