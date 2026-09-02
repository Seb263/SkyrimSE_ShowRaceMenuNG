#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Core/Main.hpp"

namespace Events
{
	class ModEventSink :
		public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
		public RE::BSTEventSink<RE::TESSwitchRaceCompleteEvent>
	{
		ModEventSink() = default;
		ModEventSink(const ModEventSink&) = delete;
		ModEventSink(ModEventSink&&) = delete;
		ModEventSink& operator=(const ModEventSink&) = delete;
		ModEventSink& operator=(ModEventSink&&) = delete;

	public:
		#define continueEvent RE::BSEventNotifyControl::kContinue

		static ModEventSink* GetSingleton()
		{
			static ModEventSink singleton;
			return &singleton;
		}

		static void LoadEvents()
		{
			auto* eventSink = GetSingleton();
			auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
			eventSourceHolder->AddEventSink<RE::TESSwitchRaceCompleteEvent>(eventSink);

			if (auto* menu = RE::UI::GetSingleton()) {
				menu->AddEventSink<RE::MenuOpenCloseEvent>(eventSink);
			}
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*);
		RE::BSEventNotifyControl ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*);
	};
};
