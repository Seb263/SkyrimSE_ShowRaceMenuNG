#pragma once

#include "DataHandler.hpp"

class ModUtils
{
public:

	static void EquipObject(RE::Actor* target, RE::TESBoundObject* object)
	{
		if (!target || !object) return;

		auto equipType = object->As<RE::BGSEquipType>();
		RE::BGSEquipSlot* slot = equipType ? equipType->GetEquipSlot() : nullptr;

		const auto& itemManager = RE::ActorEquipManager::GetSingleton();
		if (!itemManager) REPORT_AND_FAIL("Item Manager could not be initialized.");
		itemManager->EquipObject(target, object, nullptr, 1, slot, false, false, false, false);
	}

	static void UnequipObject(RE::Actor* target, RE::TESBoundObject* object)
	{
		if (!target || !object) return;

		const auto& inv = target->GetInventory();
		auto it = inv.find(object);
		if (it == inv.end()) return;

		auto& [count, entryData] = it->second;

		if (entryData && entryData->IsWorn()) {
			auto* slot = object->As<RE::BGSEquipType>() ? object->As<RE::BGSEquipType>()->GetEquipSlot() : nullptr;
			if (const auto& itemManager = RE::ActorEquipManager::GetSingleton()) {
				itemManager->UnequipObject(target, object, nullptr, 1, slot, false, false, false);
			}
		}
	}

	template<typename T>
	static T GetGameSetting(const std::string& settingName, const T& defaultValue = T{})
	{
		auto* gsc = RE::GameSettingCollection::GetSingleton();
		if (!gsc) return defaultValue;

		auto* setting = gsc->GetSetting(settingName.c_str());
		if (!setting) {
			logger::warn("GetGameSetting: setting \"{}\" not found", settingName);
			return defaultValue;
		}

		using SettingType = RE::Setting::Type;
		switch (setting->GetType()) {
			case SettingType::kBool: if constexpr (std::is_same_v<T, bool>) return setting->data.b; break;
			case SettingType::kFloat: if constexpr (std::is_same_v<T, float>) return setting->data.f; break;
			case SettingType::kInteger: if constexpr (std::is_same_v<T, int32_t>) return setting->data.i; break;
			case SettingType::kUnsignedInteger: if constexpr (std::is_same_v<T, uint32_t>) return setting->data.u; break;
			case SettingType::kString:
				if constexpr (std::is_same_v<T, std::string>) {
					return (setting->data.s && !IsBadReadPtr(setting->data.s, 1)) ? std::string(setting->data.s) : defaultValue;
				}
				break;
			default: break;
		}

		return defaultValue;
	}
};
