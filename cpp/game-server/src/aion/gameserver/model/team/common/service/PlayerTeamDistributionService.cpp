#include "aion/gameserver/model/team/common/service/PlayerTeamDistributionService.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/DropConfig.h"
#include "aion/gameserver/configs/main/GroupConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/attack/DamageInfo.h"
#include "aion/gameserver/controllers/attack/TeamDamageList.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/detail/PlayerMath.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/services/abyss/AbyssPointsService.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::team::common::service {

using gameobjects::Npc;
using gameobjects::player::Player;

/**
 * Java: the private static class implementing Consumer<Player> (PlayerTeamDistributionService.java:107-138). K5: a local of doReward.
 */
class PlayerTeamDistributionService::PlayerTeamRewardStats {
public:
	std::vector<runtime::Ptr<Player>> players;
	/** C++ correction (owner's decision 2026-10-05): the members QuestEngine.onKill is called for after the team lock is released */
	std::vector<runtime::Ref<Player>> questKillers;
	const bool disableRangeChecks;
	int32_t partyLvlSum = 0;
	int32_t highestLevel = 0;
	int32_t mentorCount = 0;
	bool hasLivingPlayer = false;
	runtime::Ptr<Npc> owner;

	PlayerTeamRewardStats(Npc& ownerValue, bool disableRangeChecksValue) : disableRangeChecks(disableRangeChecksValue), owner(ownerValue) {}

	void accept(Player& member) {
		if (member.isOnline() &&
			utils::PositionUtil::isInRange(member, *owner,
				static_cast<float>(disableRangeChecks ? 9999 : configs::main::GroupConfig::GROUP_MAX_DISTANCE.load()))) {
			// Java: QuestEngine.getInstance().onKill(new QuestEnv(owner, member, 0)) here, under the team lock (PlayerTeamDistributionService.java:122).
			// Corrected (owner's decision 2026-10-05, both branches; docs/deviations/P5-10a.md): the member is collected and doReward calls onKill
			// once the forEach released the lock - quest handlers take other locks and may reach the team again
			questKillers.emplace_back(member);

			if (member.isMentor()) {
				mentorCount++;
			} else {
				if (!hasLivingPlayer && !member.isDead())
					hasLivingPlayer = true;

				players.push_back(runtime::Ptr<Player>(member));
				partyLvlSum += member.getLevel();
				if (member.getLevel() > highestLevel)
					highestLevel = member.getLevel();
			}
		}
	}
};

void PlayerTeamDistributionService::doReward(TemporaryPlayerTeam& team, float damagePercent, Npc& owner, gameobjects::AionObject& winner,
	controllers::attack::TeamDamageList& teamDamageList) {
	// Find team's members and determine highest level
	bool disableRangeChecks = configs::main::DropConfig::DISABLE_RANGE_CHECK_MAPS.get()->contains(owner.getPosition()->getMapId());
	PlayerTeamRewardStats filteredStats(owner, disableRangeChecks);
	auto accept = [&filteredStats](gameobjects::AionObject& object) { filteredStats.accept(*runtime::cast<Player>(object)); };
	runtime::Ptr<alliance::PlayerAlliance> alli = runtime::as<alliance::PlayerAlliance>(team);
	if (alli && alli->isInLeague()) {
		for (const runtime::Ptr<gameobjects::AionObject>& a : alli->getLeague()->getMembers())
			runtime::cast<alliance::PlayerAlliance>(a)->forEach(accept);
	} else {
		team.forEach(accept);
	}
	// C++ correction (owner's decision 2026-10-05): the quest kills of the members accept collected, outside the team lock, in the same order
	for (const runtime::Ref<Player>& member : filteredStats.questKillers) {
		runtime::Ref<questEngine::model::QuestEnv> env = questEngine::model::QuestEnv::create(owner, *member, 0);
		questEngine::QuestEngine::getInstance().onKill(*env);
	}

	// All non-mentors are not nearby or dead
	if (filteredStats.players.empty() || !filteredStats.hasLivingPlayer) {
		return;
	}

	int64_t expReward = utils::stats::StatFunctions::calculateExperienceReward(filteredStats.highestLevel, owner);

	float instanceApMultiplier = owner.getPosition()->getWorldMapInstance()->getInstanceHandler()->getApMultiplier();
	for (const runtime::Ptr<Player>& member : filteredStats.players) {
		// dead players shouldn't receive AP/EP/DP
		if (member->isDead())
			continue;

		// Reward init: Math.round(long * byte / (float) int) - the long product is converted to float, Math.round(float) answers an int
		int64_t rewardXp = gameobjects::player::detail::javaRound(static_cast<float>(expReward * member->getLevel()) /
			static_cast<float>(filteredStats.partyLvlSum));
		int32_t rewardDp = utils::stats::StatFunctions::calculateDPReward(*member, owner);
		float rewardAp = 1;

		// Players 10 levels below highest member get 0 reward.
		if (filteredStats.highestLevel - member->getLevel() >= 10) {
			rewardXp = 0;
			rewardDp = 0;
		}

		// Dmg percent correction (Java compound assignments: the long and the int are converted to float and narrowed back)
		rewardXp = controllers::detail::toLong(static_cast<float>(rewardXp) * damagePercent);
		rewardDp = controllers::detail::toInt(static_cast<float>(rewardDp) * damagePercent);
		rewardAp *= damagePercent;
		rewardAp *= instanceApMultiplier;

		member->getCommonData()->addExp(rewardXp, gameobjects::player::Rates::XP_GROUP_HUNTING, owner.getObjectTemplate()->getL10n());
		member->getCommonData()->addDp(rewardDp);
		if (owner.getAi().ask(ai::poll::AIQuestion::REWARD_AP) && !(filteredStats.mentorCount > 0 && configs::main::CustomConfig::MENTOR_GROUP_AP.load())) {
			rewardAp *= static_cast<float>(utils::stats::StatFunctions::calculatePvEApGained(*member, owner));
			int32_t ap = controllers::detail::toInt(rewardAp) / static_cast<int32_t>(filteredStats.players.size());
			if (ap >= 1) {
				services::abyss::AbyssPointsService::addAp(*member, owner, ap);
			}
		}
	}
	if (owner.getAi().ask(ai::poll::AIQuestion::REWARD_LOOT)) {
		// Give Drop
		std::optional<controllers::attack::DamageInfo> mostDamageMember = teamDamageList.getMostDamageByTeam(team);
		if (!mostDamageMember) {
			return;
		}
		runtime::Ptr<Player> mostDamagePlayer = runtime::cast<Player>(mostDamageMember->getAttacker());
		if (mostDamagePlayer->isMentor()) {
			for (const runtime::Ptr<gameobjects::AionObject>& object : team.getMembers()) {
				runtime::Ptr<Player> member = runtime::cast<Player>(object);
				if (member->getLevel() == filteredStats.highestLevel)
					mostDamagePlayer = member;
			}
		}
		if (winner.equals(team) && (filteredStats.mentorCount == 0 || owner.getAi().getName() != "chest")) {
			services::drop::DropRegistrationService::getInstance().registerDrop(owner, *mostDamagePlayer, filteredStats.highestLevel,
				filteredStats.players);
		}
	}
}

} // namespace aion::gameserver::model::team::common::service
