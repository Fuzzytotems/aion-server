#include "aion/gameserver/handlers/ai/worlds/panesterra/AdvanceCorridorAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra {

AION_AI(AdvanceCorridorAI, "panesterra_advance_corridor");

/**
 * Java: the anonymous AIRequest of handleDialogStart (AdvanceCorridorAI.java:46-57, fieldmap ai.worlds.panesterra.AdvanceCorridorAI$1); it
 * captures the faction and the clicking player.
 */
struct AdvanceCorridorAI_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const services::panesterra::ahserion::PanesterraFaction faction; // captured local PanesterraFaction faction (line 45)
	const runtime::Ref<Player> player;                               // captured param Player player (line 37)

	static runtime::Ref<AdvanceCorridorAI_AIRequest> create(services::panesterra::ahserion::PanesterraFaction faction, Player& player) {
		return runtime::makeRef<AdvanceCorridorAI_AIRequest>(faction, player);
	}

	// Java AdvanceCorridorAI.java:48-56 (getTeam(faction).addTeamMemberIfAbsent on no team is Java's NullPointerException)
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		static_cast<void>(requestId);
		if (PanesterraService::getInstance().getTeamMemberCount(faction) >= SiegeConfig::PANESTERRA_MAX_PLAYERS_PER_TEAM.load()) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_SVS_DIRECT_PORTAL_USE_COUNT_LIMIT());
			return;
		}
		runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(faction);
		if (team == nullptr)
			throw runtime::NullPointerException("AdvanceCorridorAI: no Panesterra team");
		team->addTeamMemberIfAbsent(player->getObjectId());
		team->movePlayerToStartPosition(*player);
	}

protected:
	AdvanceCorridorAI_AIRequest(services::panesterra::ahserion::PanesterraFaction factionValue, Player& playerValue)
		: faction(factionValue), player(runtime::Ref<Player>(playerValue)) {}
	~AdvanceCorridorAI_AIRequest() override = default;
};

// Java AdvanceCorridorAI.java:30-35
void AdvanceCorridorAI::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
		getOwner().getController().deleteIfAliveOrCancelRespawn();
	}, despawnInMin.get(), runtime::TimeUnit::MINUTES));
}

// Java AdvanceCorridorAI.java:37-58
void AdvanceCorridorAI::handleDialogStart(Player& player) {
	if (static_cast<int32_t>(player.getAbyssRank()->getRank()) < static_cast<int32_t>(AbyssRankEnum::STAR1_OFFICER)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_TELEPOTER_GAB1_USER07());
		return;
	}
	if (player.getLevel() < 65) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_SVS_DIRECT_PORTAL_LEVEL_LIMIT());
		return;
	}
	services::panesterra::ahserion::PanesterraFaction faction = getFactionToAssign(player);
	runtime::Ref<AdvanceCorridorAI_AIRequest> request = AdvanceCorridorAI_AIRequest::create(faction, player);
	AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_PASS_BY_SVS_DIRECT_PORTAL, *request);
}

// Java AdvanceCorridorAI.java:60-66
services::panesterra::ahserion::PanesterraFaction AdvanceCorridorAI::getFactionToAssign(Player& player) {
	return player.getRace() == Race::ELYOS ? services::panesterra::ahserion::PanesterraFaction::IVY_TEMPLE
	                                       : services::panesterra::ahserion::PanesterraFaction::ALPINE_TEMPLE;
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra
