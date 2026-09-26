#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/model/templates/factions/NpcFactionTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

namespace {

/** Java: DataManager.NPC_FACTIONS_DATA.getNpcFactionById(id).isMentor() (NullPointerException for a faction without a template) */
bool isMentorFaction(int32_t id) {
	const templates::factions::NpcFactionTemplate* factionTemplate = dataholders::DataManager::NPC_FACTIONS_DATA->getNpcFactionById(id);
	if (factionTemplate == nullptr)
		throw runtime::NullPointerException("NpcFactionTemplate of faction " + std::to_string(id) + " is null");
	return factionTemplate->isMentor();
}

} // namespace

NpcFaction::NpcFaction(int32_t idValue, int32_t timeValue, bool activeValue, ENpcFactionQuestState stateValue, int32_t questIdValue)
	: id(idValue), time(timeValue), active(activeValue), mentor(isMentorFaction(idValue)), state(stateValue), questId(questIdValue),
	  persistentState(PersistentState::NEW) {
}

NpcFaction::~NpcFaction() = default;

runtime::Ref<NpcFaction> NpcFaction::create(int32_t idValue, int32_t timeValue, bool activeValue, ENpcFactionQuestState stateValue,
	int32_t questIdValue) {
	return runtime::makeRef<NpcFaction>(idValue, timeValue, activeValue, stateValue, questIdValue);
}

void NpcFaction::setTime(int32_t value) {
	time.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void NpcFaction::setActive(bool value) {
	active.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void NpcFaction::setState(ENpcFactionQuestState value) {
	setPersistentState(PersistentState::UPDATE_REQUIRED);
	state.set(value);
}

void NpcFaction::setQuestId(int32_t value) {
	questId.set(value);
	setPersistentState(PersistentState::UPDATE_REQUIRED);
}

void NpcFaction::setPersistentState(PersistentState value) {
	switch (value) {
		case PersistentState::DELETED:
			if (persistentState.get() == PersistentState::NEW)
				persistentState.set(PersistentState::NOACTION);
			else
				persistentState.set(PersistentState::DELETED);
			break;
		case PersistentState::UPDATE_REQUIRED:
			if (persistentState.get() != PersistentState::NEW)
				persistentState.set(PersistentState::UPDATE_REQUIRED);
			break;
		case PersistentState::NOACTION:
			break;
		default:
			persistentState.set(value);
	}
}

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
