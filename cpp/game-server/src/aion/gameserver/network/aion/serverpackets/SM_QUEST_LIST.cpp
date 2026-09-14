#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/questEngine/model/QuestState.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_QUEST_LIST::SM_QUEST_LIST(const std::vector<runtime::Ptr<questEngine::model::QuestState>>& questState)
	: AionServerPacket(opcodeOf<SM_QUEST_LIST>), questStates(questState.begin(), questState.end()) {
}

SM_QUEST_LIST::~SM_QUEST_LIST() = default;

void SM_QUEST_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
