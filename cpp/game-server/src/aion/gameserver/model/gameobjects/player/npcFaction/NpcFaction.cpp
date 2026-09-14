#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

NpcFaction::NpcFaction(int32_t idValue, int32_t timeValue, bool activeValue, ENpcFactionQuestState stateValue, int32_t questIdValue)
	: id(idValue), time(timeValue), active(activeValue), mentor(false), state(stateValue), questId(questIdValue),
	  persistentState(PersistentState::NEW) {
	// Java: this.mentor = DataManager.NPC_FACTIONS_DATA.getNpcFactionById(id).isMentor()
	AION_UNPORTED();
}

NpcFaction::~NpcFaction() = default;

runtime::Ref<NpcFaction> NpcFaction::create(int32_t idValue, int32_t timeValue, bool activeValue, ENpcFactionQuestState stateValue,
	int32_t questIdValue) {
	return runtime::makeRef<NpcFaction>(idValue, timeValue, activeValue, stateValue, questIdValue);
}

void NpcFaction::setTime(int32_t value) {
	AION_UNPORTED();
}

void NpcFaction::setActive(bool value) {
	AION_UNPORTED();
}

void NpcFaction::setState(ENpcFactionQuestState value) {
	AION_UNPORTED();
}

void NpcFaction::setQuestId(int32_t value) {
	AION_UNPORTED();
}

void NpcFaction::setPersistentState(PersistentState value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
