#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

namespace ModData
{
	class DataHandler
	{
	public:
		static DataHandler* GetSingleton()
		{
			static DataHandler singleton;
			return &singleton;
		}

		void LoadData()
		{
			static bool loadingStarted = false;

			if (loadingStarted) return;
			loadingStarted = true;

			TESdataHandler = RE::TESDataHandler::GetSingleton();

			LoadPluginsForms();
		}

	private:
		static inline void LoadPluginsForms()
		{
			logger::info("Loading Plugins Froms Data...");

			for (const auto& formInfo : pluginForms) {
				*formInfo.formPtr = TESdataHandler->LookupForm(formInfo.formID, formInfo.pluginName.data());
				if (!*formInfo.formPtr && !formInfo.optional) {
					REPORT_AND_FAIL("ERROR: Form \"{}\" not found in \"{}\".", formInfo.pluginName, formInfo.name, formInfo.pluginName);
				}
			}

			logger::info("Loading Plugins Froms Data: DONE");
		}
	};
}
