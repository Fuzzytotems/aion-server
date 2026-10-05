#include "aion/gameserver/network/aion/clientpackets/CM_QUEST_SHARE.h"

#include <string>
#include <vector>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestTarget.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/collections/Predicates.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_QUEST_SHARE::CM_QUEST_SHARE(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

using model::gameobjects::player::Player;
using serverpackets::SM_QUEST_ACTION;
using serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;
using utils::collections::Predicates;

// Java CM_QUEST_SHARE.java:39-41
void CM_QUEST_SHARE::readImpl() {
	this->questId = readD();
}

// Java CM_QUEST_SHARE.java:44-82
void CM_QUEST_SHARE::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	const model::templates::QuestTemplate* questTemplate = dataholders::DataManager::QUEST_DATA->getQuestById(questId);
	if (questTemplate == nullptr || questTemplate->isCannotShare()) {
		PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE(1100001, std::vector<std::string>())); // This quest cannot be shared.
		return;
	}
	runtime::Ptr<questEngine::model::QuestState> questState = player->getQuestStateList()->getQuestState(questId);
	if (!questState || questState->getStatus() == questEngine::model::QuestStatus::COMPLETE)
		return;
	std::vector<runtime::Ptr<Player>> membersToShareWith;
	runtime::Ptr<model::team::TemporaryPlayerTeam> currentGroup = player->getCurrentGroup();
	if (currentGroup) {
		// Java: allExcept(player).and(ONLINE).and(member -> PositionUtil.isInRange(member, player, GroupConfig.GROUP_MAX_DISTANCE))
		const auto allExcept = Predicates::Players::allExcept(*player);
		Player& sharer = *player;
		for (const runtime::Ptr<model::gameobjects::AionObject>& object : currentGroup->filterMembers([&allExcept, &sharer](model::gameobjects::AionObject& o) {
				 Player& member = *runtime::cast<Player>(o);
				 return allExcept(member) && Predicates::Players::ONLINE(member) &&
					 utils::PositionUtil::isInRange(member, sharer, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load()));
			 }))
			membersToShareWith.push_back(runtime::cast<Player>(object));
	}
	if (membersToShareWith.empty()) {
		if (questTemplate->getTarget() == model::templates::quest::QuestTarget::ALLIANCE) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE(1100005, std::vector<std::string>())); // There are no Alliance members to share the quest with.
		} else {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE(1100000, std::vector<std::string>())); // There are no group members to share the quest with.
		}
		return;
	}
	for (const runtime::Ptr<Player>& member : membersToShareWith) {
		if (!services::QuestService::checkStartConditions(*member, questId, false)) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE(1100003, member->getName())); // You failed to share the quest with %0.
		} else {
			PacketSendUtility::sendPacket(*member, SM_QUEST_ACTION(questId, player->getObjectId(), member->isInAlliance()));
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE(1100002, member->getName())); // You shared the quest with %0.
		}
	}
}

AION_CLIENT_PACKET(CM_QUEST_SHARE);

} // namespace aion::gameserver::network::aion::clientpackets
