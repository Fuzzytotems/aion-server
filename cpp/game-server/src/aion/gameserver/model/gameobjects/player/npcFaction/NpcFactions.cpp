#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"

namespace aion::gameserver::model::gameobjects::player::npcFaction {

NpcFactions::NpcFactions(Player& ownerValue)
	: OwnedPart(ownerValue), owner(ownerValue), activeNpcFaction(runtime::Array<runtime::Ref<NpcFaction>>::make(2)),
	  timeLimit(runtime::Array<int32_t>::of({0, 0})) {
}

NpcFactions::~NpcFactions() = default;

void NpcFactions::addNpcFaction(NpcFaction& faction) {
	AION_UNPORTED();
}

runtime::Ptr<NpcFaction> NpcFactions::getFactionById(int32_t id) {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<NpcFaction>> NpcFactions::getNpcFactions() {
	AION_UNPORTED();
}

runtime::Ptr<NpcFaction> NpcFactions::getActiveNpcFaction(bool mentor) {
	AION_UNPORTED();
}

runtime::Ptr<NpcFaction> NpcFactions::setActive(int32_t npcFactionId) {
	AION_UNPORTED();
}

void NpcFactions::leaveNpcFaction(Npc& npc) {
	AION_UNPORTED();
}

void NpcFactions::leaveNpcFaction(NpcFaction& npcFaction) {
	AION_UNPORTED();
}

void NpcFactions::enterGuild(Npc& npc) {
	AION_UNPORTED();
}

// Callback struct (fieldmap.py --class): NpcFactions$1 (RequestResponseHandler)
void NpcFactions::askLeaveNpcFaction(Npc& npc) {
	AION_UNPORTED();
}

void NpcFactions::startQuest(const templates::QuestTemplate* questTemplate) {
	AION_UNPORTED();
}

void NpcFactions::abortQuest(const templates::QuestTemplate* questTemplate) {
	AION_UNPORTED();
}

void NpcFactions::completeQuest(const templates::QuestTemplate* questTemplate) {
	AION_UNPORTED();
}

void NpcFactions::sendDailyQuest() {
	AION_UNPORTED();
}

void NpcFactions::onLevelUp() {
	AION_UNPORTED();
}

int32_t NpcFactions::getNextTime() {
	AION_UNPORTED();
}

bool NpcFactions::canStartQuest(const templates::QuestTemplate* template_) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::npcFaction
