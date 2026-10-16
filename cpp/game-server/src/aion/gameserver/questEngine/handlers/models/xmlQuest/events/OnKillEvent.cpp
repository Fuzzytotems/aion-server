#include "aion/gameserver/questEngine/handlers/models/xmlQuest/events/OnKillEvent.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/handlers/models/Monster.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events {

using network::aion::serverpackets::SM_QUEST_ACTION;

// C++: Java's `monster == null` test is dead once the quest is registered (XmlQuest.register calls getMonsters(), which stores an empty list
// into a null field), so the bound vector - empty without <monster> children - stands for that list and there is no early return for it.
bool OnKillEvent::operate(model::QuestEnv& env) const {
	runtime::Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(env.getVisibleObject());
	if (!npc)
		return false;
	runtime::Ptr<gameserver::model::gameobjects::player::Player> player = env.getPlayer();
	runtime::Ptr<model::QuestState> qs = player->getQuestStateList()->getQuestState(env.getQuestId());
	if (!qs)
		return false;
	for (const Monster& m : monster) {
		if (!m.getNpcIds()) // Java: NullPointerException on the null npc_ids list
			throw runtime::NullPointerException("<monster> without npc_ids");
		if (std::ranges::find(*m.getNpcIds(), npc->getNpcId()) != m.getNpcIds()->end()) {
			int32_t var = qs->getQuestVarById(m.getVar());
			if (var >= m.getStartVar() && var < m.getEndVar()) {
				qs->setQuestVarById(m.getVar(), var + 1);
				utils::PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
			}
		}
	}
	if (complite) {
		for (const Monster& m : monster) {
			if (qs->getQuestVarById(m.getVar()) != m.getEndVar())
				return false;
		}
		complite->operate(env);
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::events
