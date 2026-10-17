#include "aion/gameserver/handlers/ai/instance/beshmundirTemple/Door1AI.h"

#include <vector>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

AION_AI(Door1AI, "door1");

// Java Door1AI.java:26-37
void Door1AI::handleDialogStart(Player& player) {
	int32_t questId = player.getRace() == Race::ELYOS ? 30208 : 30308;
	// Only one player in group has to have this quest
	std::vector<runtime::Ptr<Player>> members; // Java: player.isInGroup() ? player.getPlayerGroup().getOnlineMembers() : Arrays.asList(player)
	if (player.isInGroup()) {
		for (const runtime::Ptr<Player>& member : player.getPlayerGroup()->getOnlineMembers())
			members.push_back(member);
	} else {
		members.push_back(runtime::Ptr<Player>(player));
	}
	for (const runtime::Ptr<Player>& member : members) {
		if (member->getQuestStateList()->hasQuest(questId)) {
			ActionItemNpcAI::handleDialogStart(player);
			return;
		}
	}
	PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
}

// Java Door1AI.java:39-42
void Door1AI::handleUseItemFinish(Player& player) {
	AIActions::deleteOwner(*this);
}

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
