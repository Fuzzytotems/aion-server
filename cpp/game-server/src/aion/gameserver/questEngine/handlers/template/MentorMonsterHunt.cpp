#include "aion/gameserver/questEngine/handlers/template/MentorMonsterHunt.h"

#include <string>
#include <utility>

#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/QuestTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestMentorType.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::questEngine::handlers::template_ {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::templates::quest::QuestMentorType;
using model::QuestState;
using model::QuestStatus;

namespace {

/** Java `for (Player member : player.getPlayerGroup().getMembers())`: NullPointerException without a group; the members are Players */
std::vector<runtime::Ptr<Player>> groupMembersOf(Player& player) {
	runtime::Ptr<gameserver::model::team::group::PlayerGroup> group = player.getPlayerGroup();
	if (!group)
		throw runtime::NullPointerException("player.getPlayerGroup()");
	std::vector<runtime::Ptr<Player>> members;
	for (const runtime::Ptr<gameserver::model::gameobjects::AionObject>& member : group->getMembers()) {
		runtime::Ptr<Player> memberPlayer = runtime::as<Player>(member);
		if (!memberPlayer) // Java: the erased List<Player> element cast
			throw runtime::ClassCastException("a group member cannot be cast to class com.aionemu.gameserver.model.gameobjects.player.Player");
		members.push_back(memberPlayer);
	}
	return members;
}

} // namespace

MentorMonsterHunt::MentorMonsterHunt(int32_t questIdValue, const std::optional<std::vector<int32_t>>& startNpcIdsValue,
	const std::optional<std::vector<int32_t>>& endNpcIdsValue, std::vector<models::Monster> monstersValue, int32_t menteMinLevelValue,
	int32_t menteMaxLevelValue, bool rewardValue, bool rewardNextStepValue)
	: MonsterHunt(questIdValue, startNpcIdsValue, endNpcIdsValue, std::move(monstersValue), 0, 0, std::nullopt, 0, "", 0, rewardValue,
		  rewardNextStepValue),
	  menteMinLevel(menteMinLevelValue), menteMaxLevel(menteMaxLevelValue) {
}

bool MentorMonsterHunt::onKillEvent(model::QuestEnv& env) {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
	if (qs && qs->getStatus() == QuestStatus::START) {
		switch (questTemplateOf(questId).getMentorType()) {
			case QuestMentorType::MENTOR:
				if (player->isMentor()) {
					for (const runtime::Ptr<Player>& member : groupMembersOf(*player)) {
						if (member->getLevel() >= menteMinLevel && member->getLevel() <= menteMaxLevel &&
							utils::PositionUtil::isInRange(*player, *member, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load()))) {
							return MonsterHunt::onKillEvent(env);
						}
					}
				}
				break;
			case QuestMentorType::MENTE:
				if (player->isInGroup()) {
					for (const runtime::Ptr<Player>& member : groupMembersOf(*player)) {
						if (member->isMentor() &&
							utils::PositionUtil::isInRange(*player, *member, static_cast<float>(configs::main::GroupConfig::GROUP_MAX_DISTANCE.load())))
							return MonsterHunt::onKillEvent(env);
					}
				}
				break;
			default: // NONE: Java's switch has no case for it
				break;
		}
	}
	return false;
}

} // namespace aion::gameserver::questEngine::handlers::template_
