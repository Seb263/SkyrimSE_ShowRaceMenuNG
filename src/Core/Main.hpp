#pragma once

#include "DataHandler.hpp"
#include "SettingsIni.hpp"

#include "Utils/ModUtils.hpp"
#include "Utils/PapyrusUtils.hpp"

namespace SRMCore
{
	class ShowTextEntryFieldHook : public RE::GFxFunctionHandler
	{
	public:
		RE::GFxValue originalFunc;

		void Call(Params& params) override
		{
			RE::GFxValue result;
			RE::GFxValue thisVal = *params.thisPtr;
			originalFunc.Invoke("call", &result, &thisVal, 1);

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			const auto defaultName = ModUtils::GetGameSetting<std::string>("sPrisoner");
			const auto currentName = player->GetName();
			if (defaultName == currentName) return;

			RE::GFxValue gfxName(currentName);
			params.movie->SetVariable("_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance._TextEntryField.TextInputInstance.text", gfxName);
		}
	};

	class Main
	{
	public:

		static void BackupProcess()
		{
			using namespace ModData;

			if (SettingsIni::bOverrideLimitedRaceMenu && OverrideRaceMenu()) return;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			auto* race = player->GetRace();
			if (!race) return;

			std::lock_guard<std::mutex> lock(syncMutex);

			skillSet.clear();
			actorValueSnapshot.clear();
			extraSet = {};
			extraSet.previousRace = race;
			extraSet.isVampire = (VampireKeyword && race->HasKeyword(VampireKeyword));

			BackupStats(player);

			if (SettingsIni::bHideHelmet) {
				if (auto* wornHelmet = player->GetWornArmor(RE::BIPED_MODEL::BipedObjectSlot::kHair)) {
					ModUtils::UnequipObject(player, wornHelmet->As<RE::TESBoundObject>());
					extraSet.backupHelmet = wornHelmet;
				}
			}

			if (SettingsIni::bFillPlayerName) FillPlayerName();

			if (restoreIsReady && debugVerboseMode > 1) TraceSnapshot(false);
		}

		static bool OverrideRaceMenu()
		{
			bool isLimitedRaceMenu = false;

			if (const auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
				RE::BSSpinLockGuard lock(ui->processMessagesLock);
				if (const auto menu = ui->GetMenu(RE::RaceSexMenu::MENU_NAME); menu) {
					if (!menu || !menu->uiMovie || !menu->uiMovie->GetVisible()) return false;

					RE::GFxValue bLimitedMenu;
					menu->uiMovie->GetVariable(&bLimitedMenu, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance.bLimitedMenu");
					isLimitedRaceMenu = bLimitedMenu.GetBool();
				}
			}

			if (isLimitedRaceMenu) {
				RE::UIMessageQueue::GetSingleton()->AddMessage(RE::RaceSexMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
				SKSE::GetTaskInterface()->AddTask([]() {
					RE::UIMessageQueue::GetSingleton()->AddMessage(RE::RaceSexMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
				});
			}

			return isLimitedRaceMenu;
		}

		static void FillPlayerName()
		{
			if (const auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME)) {
				RE::BSSpinLockGuard lock(ui->processMessagesLock);
				if (const auto menu = ui->GetMenu(RE::RaceSexMenu::MENU_NAME); menu) {
					if (!menu || !menu->uiMovie || !menu->uiMovie->GetVisible()) return;

					RE::GFxValue panel;
					if (!menu->uiMovie->GetVariable(&panel, "_root.RaceSexMenuBaseInstance.RaceSexPanelsInstance")) return;

					auto* handler = new ShowTextEntryFieldHook();
					panel.GetMember("ShowTextEntryField", &handler->originalFunc);

					RE::GFxValue funcVal;
					menu->uiMovie->CreateFunction(&funcVal, handler);
					panel.SetMember("ShowTextEntryField", funcVal);
				}
			}
		}

		static void RestoreProcess()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			std::lock_guard<std::mutex> lock(syncMutex);

			if (restoreIsReady) {
				if (extraSet.isVampire && SettingsIni::iVampireRaceSyncMode >= 1) SwitchToVampire();

				RestoreStats(player);

				if (SettingsIni::bUpdatePapyrusScripts) SetScriptValues();

				if (SettingsIni::bHideHelmet && extraSet.backupHelmet) {
					ModUtils::EquipObject(player, extraSet.backupHelmet->As<RE::TESBoundObject>());
				}

				if (debugVerboseMode > 1) TraceSnapshot(true);
			}

			skillSet.clear();
			actorValueSnapshot.clear();
			extraSet = {};
		}

		static void SwitchRaceProcess()
		{
			std::lock_guard<std::mutex> lock(syncMutex);

			auto* ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen("RaceSex Menu")) return;

			if (extraSet.isVampire) SwitchToVampire();
		}

	private:
		using Skill = RE::PlayerCharacter::PlayerSkills::Data::Skills::Skill;

		static RE::PlayerCharacter::PlayerSkills::Data* GetSkillsData(RE::PlayerCharacter* player)
		{
			if (REL::Module::IsVR()) {
				auto* runtime = player->GetVRInfoRuntimeData();
				return (runtime && runtime->skills) ? runtime->skills->data : nullptr;
			} else {
				auto& runtime = player->GetPlayerRuntimeData();
				return (runtime.skills) ? runtime.skills->data : nullptr;
			}
		}

		static void BackupStats(RE::PlayerCharacter* player)
		{
			if (!player) return;

			auto* actorAV = player->AsActorValueOwner();
			if (!actorAV) return;

			auto* data = GetSkillsData(player);
			if (!data) return;

			auto* playerBase = player->GetActorBase();
			if (!playerBase) return;

			if (SettingsIni::bRestoreAttributes) {
				const float baseHealth = extraSet.previousRace->data.startingHealth + playerBase->actorData.healthOffset;
				const float baseStamina = extraSet.previousRace->data.startingStamina + playerBase->actorData.staminaOffset;
				const float baseMagicka = extraSet.previousRace->data.startingMagicka + playerBase->actorData.magickaOffset;

				actorValueSnapshot[RE::ActorValue::kHealth] = actorAV->GetBaseActorValue(RE::ActorValue::kHealth) - baseHealth;
				actorValueSnapshot[RE::ActorValue::kStamina] = actorAV->GetBaseActorValue(RE::ActorValue::kStamina) - baseStamina;
				actorValueSnapshot[RE::ActorValue::kMagicka] = actorAV->GetBaseActorValue(RE::ActorValue::kMagicka) - baseMagicka;
			}

			float skillLevelThresholds = 0.0f;
			if (SettingsIni::bRestoreSkills) {
				for (std::uint32_t i = 0; i < Skill::kTotal; ++i) {
					auto skill = static_cast<Skill>(i);
					skillSet[skill] = {
						actorAV->GetBaseActorValue(SkillToActorValue(skill)),
						data->skills[skill].xp,
						data->skills[skill].levelThreshold,
						(SettingsIni::bRestoreLegendaryLevels ? data->legendaryLevels[skill] : 0)
					};
					skillLevelThresholds += data->skills[skill].levelThreshold;
				}
			}

			if (SettingsIni::bFixPlayerLevel) {
				playerLevelSnapshot = { data->xp, data->levelThreshold };
			}

			if (data->levelThreshold <= 0.0f || skillLevelThresholds <= 0.0f) {
				logger::warn("BackupProcess: Player skills or AV are empty. Aborting.");
				restoreIsReady = false;
			} else restoreIsReady = true;
		}

		static void RestoreStats(RE::PlayerCharacter* player)
		{
			if (!player) return;

			auto* actorAV = player->AsActorValueOwner();
			if (!actorAV) return;

			auto* data = GetSkillsData(player);
			if (!data) return;

			auto* playerRace = player->GetRace();
			auto* playerBase = player->GetActorBase();
			if (!playerRace || !playerBase) return;

			if (SettingsIni::bRestoreAttributes) {
				const float baseHealth = playerRace->data.startingHealth + playerBase->actorData.healthOffset;
				const float baseStamina = playerRace->data.startingStamina + playerBase->actorData.staminaOffset;
				const float baseMagicka = playerRace->data.startingMagicka + playerBase->actorData.magickaOffset;

				actorAV->SetBaseActorValue(RE::ActorValue::kHealth, actorValueSnapshot[RE::ActorValue::kHealth] + baseHealth);
				actorAV->SetBaseActorValue(RE::ActorValue::kStamina, actorValueSnapshot[RE::ActorValue::kStamina] + baseStamina);
				actorAV->SetBaseActorValue(RE::ActorValue::kMagicka, actorValueSnapshot[RE::ActorValue::kMagicka] + baseMagicka);
			}

			if (SettingsIni::bRestoreSkills) {
				if (SettingsIni::bApplyRaceSkillBoosts) ComputeSkillBoosts(extraSet.previousRace, playerRace);

				for (auto& [skill, snap] : skillSet) {
					actorAV->SetBaseActorValue(SkillToActorValue(skill), snap.level);
					data->skills[skill].level  = snap.level;
					data->skills[skill].xp = snap.xp;
					data->skills[skill].levelThreshold = snap.levelThreshold;

					if (SettingsIni::bRestoreLegendaryLevels) {
						data->legendaryLevels[skill] = snap.legendaryLevels;
					}
				}
			}

			if (SettingsIni::bFixPlayerLevel) {
				data->xp = playerLevelSnapshot.first;
				data->levelThreshold = playerLevelSnapshot.second;
			}
		}

		static RE::ActorValue SkillToActorValue(Skill a_skill)
		{
			for (auto& [skill, av] : kSkillAVTable)
				if (skill == a_skill) return av;
			return RE::ActorValue::kNone;
		}

		static Skill ActorValueToSkill(RE::ActorValue a_av)
		{
			for (auto& [skill, av] : kSkillAVTable)
				if (av == a_av) return skill;
			return Skill::kTotal;
		}

		static void ComputeSkillBoosts(RE::TESRace* previousRace, RE::TESRace* newRace)
		{
			if (!previousRace || !newRace) return;

			for (std::uint32_t i = 0; i < std::size(previousRace->data.skillBoosts); ++i) {
				auto& boost = previousRace->data.skillBoosts[i];
				auto av = boost.skill.get();
				if (av == RE::ActorValue::kNone) continue;

				auto skill = ActorValueToSkill(av);
				if (skill == Skill::kTotal) continue;

				if (auto it = skillSet.find(skill); it != skillSet.end()) {
					it->second.level -= static_cast<float>(boost.bonus);
				}
			}

			for (std::uint32_t i = 0; i < std::size(newRace->data.skillBoosts); ++i) {
				auto& boost = newRace->data.skillBoosts[i];
				auto av = boost.skill.get();
				if (av == RE::ActorValue::kNone) continue;

				auto skill = ActorValueToSkill(av);
				if (skill == Skill::kTotal) continue;

				if (auto it = skillSet.find(skill); it != skillSet.end()) {
					it->second.level += static_cast<float>(boost.bonus);
				}
			}
		}

		static void SetScriptValues()
		{
			using namespace ModData;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			auto* newRace = player->GetRace();
			if (!newRace) return;

			if (auto* werewolfQuest = TESdataHandler->LookupForm(0x4B2D9, "Skyrim.esm")) {
				if (auto scriptObj = PapyrusUtils::GetObject(werewolfQuest, "companionshousekeepingscript")) {
					PapyrusUtils::SetProperty(scriptObj, "PlayerOriginalRace", newRace);
				}
			}

			if (auto* vampireQuest = TESdataHandler->LookupForm(0x71D2, "Dawnguard.esm")) {
				if (auto scriptObj = PapyrusUtils::GetObject(vampireQuest, "DLC1VampireTrackingQuest")) {
					PapyrusUtils::SetProperty(scriptObj, "playerRace", newRace);
				}
			}
		}

		static void SwitchToVampire()
		{
			using namespace ModData;

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) return;

			auto* newRace = player->GetRace();
			if (!newRace || newRace->HasKeyword(VampireKeyword)) return;

			for (auto* race : RE::TESDataHandler::GetSingleton()->GetFormArray<RE::TESRace>()) {
				if (!race || race->morphRace != newRace || !race->HasKeyword(ModData::VampireKeyword)) continue;
				if (!race->morphRace->GetPlayable()) continue;

				player->SwitchRace(race, true);

				break;
			}
		}

		static void TraceSnapshot(bool restore)
		{
			const char* phase = restore ? "RESTORE" : "BACKUP";

			TRACE("=== {} SNAPSHOT BEGIN ===", phase);

			// --- Actor Values ---
			TRACE("[{}] Actor Values:", phase);
			for (auto& [av, value] : actorValueSnapshot)
			{
				TRACE("  {:>10} : {:.2f}", magic_enum::enum_name(av), value);
			}

			// --- Skills ---
			TRACE("[{}] Skills:", phase);
			for (auto& [skill, snap] : skillSet)
			{
				TRACE("  {:>12} | level: {:>6.2f} | xp: {:>8.2f} | threshold: {:>8.2f} | legendary: {}",
					magic_enum::enum_name(skill), snap.level, snap.xp, snap.levelThreshold, snap.legendaryLevels);
			}

			// --- Player Level ---
			TRACE("[{}] Player Level XP        : {:.2f}", phase, playerLevelSnapshot.first);
			TRACE("[{}] Player Level Threshold : {:.2f}", phase, playerLevelSnapshot.second);

			// --- Extra ---
			TRACE("[{}] Previous Race : {}", phase, extraSet.previousRace ? extraSet.previousRace->GetFormEditorID() : "null");
			TRACE("[{}] Is Vampire    : {}", phase, extraSet.isVampire);

			TRACE("=== {} SNAPSHOT END ===", phase);
		}

		struct SkillAVPair
		{
			Skill skill;
			RE::ActorValue av;
		};

		static constexpr std::array<SkillAVPair, 18> kSkillAVTable = {{
			{ Skill::kOneHanded,   RE::ActorValue::kOneHanded   },
			{ Skill::kTwoHanded,   RE::ActorValue::kTwoHanded   },
			{ Skill::kArchery,     RE::ActorValue::kArchery     },
			{ Skill::kBlock,       RE::ActorValue::kBlock       },
			{ Skill::kSmithing,    RE::ActorValue::kSmithing    },
			{ Skill::kHeavyArmor,  RE::ActorValue::kHeavyArmor  },
			{ Skill::kLightArmor,  RE::ActorValue::kLightArmor  },
			{ Skill::kPickpocket,  RE::ActorValue::kPickpocket  },
			{ Skill::kLockpicking, RE::ActorValue::kLockpicking },
			{ Skill::kSneak,       RE::ActorValue::kSneak       },
			{ Skill::kAlchemy,     RE::ActorValue::kAlchemy     },
			{ Skill::kSpeech,      RE::ActorValue::kSpeech      },
			{ Skill::kAlteration,  RE::ActorValue::kAlteration  },
			{ Skill::kConjuration, RE::ActorValue::kConjuration },
			{ Skill::kDestruction, RE::ActorValue::kDestruction },
			{ Skill::kIllusion,    RE::ActorValue::kIllusion    },
			{ Skill::kRestoration, RE::ActorValue::kRestoration },
			{ Skill::kEnchanting,  RE::ActorValue::kEnchanting  }
		}};

		struct SkillSnapshot
		{
			float level = 0.0f;
			float xp = 0.0f;
			float levelThreshold = 0.0f;
			std::uint32_t legendaryLevels = 0;
		};

		struct ExtraSnapshot
		{
			RE::TESRace* previousRace = nullptr;
			RE::TESObjectARMO* backupHelmet = nullptr;
			bool isVampire = false;
		};

		inline static std::mutex syncMutex;
		inline static std::unordered_map<Skill, SkillSnapshot> skillSet;
		inline static std::unordered_map<RE::ActorValue, float> actorValueSnapshot;
		inline static std::pair<float, float> playerLevelSnapshot;
		inline static ExtraSnapshot extraSet;
		inline static bool restoreIsReady = false;
	};
}
