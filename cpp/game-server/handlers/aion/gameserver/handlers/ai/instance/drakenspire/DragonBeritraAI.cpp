#include "aion/gameserver/handlers/ai/instance/drakenspire/DragonBeritraAI.h"

#include "aion/gameserver/controllers/attack/AggroInfo.h"
#include "aion/gameserver/controllers/attack/AggroList.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

AION_AI(DragonBeritraAI, "drakenspire_dragon_beritra");

// Java DragonBeritraAI.java:32-39
void DragonBeritraAI::wipe() {
	std::vector<runtime::Ptr<AggroInfo>> infos;
	for (const runtime::Ptr<AggroInfo>& ai : getAggroList().stream())
		if (runtime::as<Player>(ai->getAttacker()) != nullptr)
			infos.push_back(ai);
	std::ranges::stable_sort(infos, [](const runtime::Ptr<AggroInfo>& a, const runtime::Ptr<AggroInfo>& b) {
		return a->getDamage() > b->getDamage(); // Comparator.comparingInt(AggroInfo::getDamage).reversed()
	});
	for (const runtime::Ptr<AggroInfo>& ai : infos)
		spawn(855450, ai->getAttacker()->getX(), ai->getAttacker()->getY(), ai->getAttacker()->getZ(), int8_t{0});
	// STR_CHAT_IDSeal_Vritra_Human_Gossip_09
	PacketSendUtility::broadcastMessage(runtime::Ptr<Npc>(getOwner()), 1501276);
}

// Java DragonBeritraAI.java:41-47
bool DragonBeritraAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKED:
		case AIQuestion::CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_ATTACKING:
			return true;
		default:
			return AggressiveNoLootNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
