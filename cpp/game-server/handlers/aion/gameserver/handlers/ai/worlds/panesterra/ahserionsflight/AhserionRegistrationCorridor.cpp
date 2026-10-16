#include "aion/gameserver/handlers/ai/worlds/panesterra/ahserionsflight/AhserionRegistrationCorridor.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/ai/AIRequest.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/panesterra/ahserion/AhserionRaid.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraFaction.h"
#include "aion/gameserver/services/panesterra/ahserion/PanesterraTeam.h"

namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight {

AION_AI(AhserionRegistrationCorridor, "ahserion_registration_corridor");

/**
 * Java: the anonymous AIRequest of handleDialogStart (AhserionRegistrationCorridor.java:49-64, fieldmap
 * ai.worlds.panesterra.ahserionsflight.AhserionRegistrationCorridor$1); it captures the AI (for its faction field) and the clicking player.
 */
struct AhserionRegistrationCorridor_AIRequest final : AIRequest {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<AhserionRegistrationCorridor> corridor; // the enclosing instance (its field faction)
	const runtime::Ref<Player> player;                          // captured param Player player (line 43)

	static runtime::Ref<AhserionRegistrationCorridor_AIRequest> create(AhserionRegistrationCorridor& corridor, Player& player) {
		return runtime::makeRef<AhserionRegistrationCorridor_AIRequest>(corridor, player);
	}

	// Java AhserionRegistrationCorridor.java:51-63 (getTeamMemberCount(null) and getTeam(faction).addTeamMemberIfAbsent on no team are Java's
	// NullPointerExceptions)
	void acceptRequest(runtime::Ptr<Creature> requester, Player& responder, int32_t requestId) override {
		static_cast<void>(requester);
		static_cast<void>(responder);
		static_cast<void>(requestId);
		if (!AhserionRaid::getInstance().isStarted()) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_READY_PANGAEA());
			return;
		}
		std::optional<services::panesterra::ahserion::PanesterraFaction> faction = corridor->getFaction();
		if (!faction)
			throw runtime::NullPointerException("AhserionRegistrationCorridor: no faction");
		if (PanesterraService::getInstance().getTeamMemberCount(*faction) >= SiegeConfig::AHSERION_MAX_PLAYERS_PER_TEAM.load()) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_SVS_DIRECT_PORTAL_USE_COUNT_LIMIT());
			return;
		}
		runtime::Ptr<services::panesterra::ahserion::PanesterraTeam> team = PanesterraService::getInstance().getTeam(*faction);
		if (team == nullptr)
			throw runtime::NullPointerException("AhserionRegistrationCorridor: no Panesterra team");
		team->addTeamMemberIfAbsent(player->getObjectId());
		team->movePlayerToStartPosition(*player);
		player->setPanesterraFaction(team->getFaction());
	}

protected:
	AhserionRegistrationCorridor_AIRequest(AhserionRegistrationCorridor& corridorValue, Player& playerValue)
		: corridor(runtime::Ref<AhserionRegistrationCorridor>(corridorValue)), player(runtime::Ref<Player>(playerValue)) {}
	~AhserionRegistrationCorridor_AIRequest() override = default;
};

// Java AhserionRegistrationCorridor.java:28-40
void AhserionRegistrationCorridor::handleSpawned() {
	GeneralNpcAI::handleSpawned();
	PacketSendUtility::broadcastToMap(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_SVS_INVADE_DIRECT_PORTAL_OPEN());
	switch (getNpcId()) {
		case 802219:
			faction = services::panesterra::ahserion::PanesterraFaction::BELUS;
			break;
		case 802221:
			faction = services::panesterra::ahserion::PanesterraFaction::ASPIDA;
			break;
		case 802223:
			faction = services::panesterra::ahserion::PanesterraFaction::ATANATOS;
			break;
		case 802225:
			faction = services::panesterra::ahserion::PanesterraFaction::DISILLON;
			break;
		default:
			break;
	}
	getOwner().getController().addTask(model::TaskId::DESPAWN, ThreadPoolManager::getInstance().schedule({this}, [this] {
		getOwner().getController().deleteIfAliveOrCancelRespawn();
	}, 10, runtime::TimeUnit::MINUTES));
}

// Java AhserionRegistrationCorridor.java:42-65
void AhserionRegistrationCorridor::handleDialogStart(Player& player) {
	if (player.getLevel() < 65) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_SVS_DIRECT_PORTAL_LEVEL_LIMIT());
		return;
	}
	runtime::Ref<AhserionRegistrationCorridor_AIRequest> request = AhserionRegistrationCorridor_AIRequest::create(*this, player);
	AIActions::addRequest(*this, player, SM_QUESTION_WINDOW::STR_ASK_PASS_BY_SVS_DIRECT_PORTAL, *request);
}

} // namespace aion::gameserver::handlers::ai::worlds::panesterra::ahserionsflight
