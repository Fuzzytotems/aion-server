#include "aion/gameserver/services/vortex/Invasion.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/TeamType.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/model/vortex/VortexStateType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::vortex {

using model::gameobjects::player::Player;
using model::team::alliance::PlayerAlliance;
using model::team::alliance::PlayerAllianceService;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

namespace {
/** Java: the anonymous RequestResponseHandler<Player> of updateDefenders (Invasion$1, capture this$0 = the invasion) */
class Invasion_RequestResponseHandler final : public model::gameobjects::player::RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const runtime::Ref<Invasion> invasion;

protected:
	Invasion_RequestResponseHandler(Player& requester, Invasion& value) : RequestResponseHandler(requester), invasion(value) {}
	~Invasion_RequestResponseHandler() override = default;

public:
	static runtime::Ref<Invasion_RequestResponseHandler> create(Player& requester, Invasion& value) {
		return runtime::makeRef<Invasion_RequestResponseHandler>(requester, value);
	}

	// Java Invasion.java:130-141
	void acceptRequest(runtime::Ptr<model::gameobjects::Creature> requester, Player& responder) override {
		static_cast<void>(requester);
		if (responder.isInGroup()) {
			model::team::group::PlayerGroupService::removePlayer(responder);
		} else if (responder.isInAlliance()) {
			PlayerAllianceService::removePlayer(responder);
		}

		runtime::Ptr<PlayerAlliance> defAlliance = invasion->defAllianceForHandler();
		if (!defAlliance || !defAlliance->isFull()) {
			invasion->addPlayer(responder, false);
		}
	}
};

std::unordered_map<int32_t, runtime::Ptr<Player>> copyOf(runtime::ConcurrentHashMap<int32_t, runtime::Ref<Player>>& participants) {
	std::unordered_map<int32_t, runtime::Ptr<Player>> copy;
	for (const runtime::Ptr<Player>& player : participants.values())
		copy.emplace(player->getObjectId(), player);
	return copy;
}
} // namespace

// Java Invasion.java:33-35
Invasion::Invasion(model::vortex::VortexLocation& vortex) : DimensionalVortex(vortex) {
}

runtime::Ref<Invasion> Invasion::create(model::vortex::VortexLocation& vortex) {
	return runtime::makeRef<Invasion>(vortex);
}

// Java Invasion.java:38-44
void Invasion::startInvasion() {
	getVortexLocation()->setActiveVortex(runtime::Ptr<DimensionalVortex>(static_cast<DimensionalVortex&>(*this)));
	despawn();
	spawn(model::vortex::VortexStateType::INVASION);
	initRiftGenerator();
	updateAlliance();
}

// Java Invasion.java:47-59
void Invasion::stopInvasion() {
	getVortexLocation()->setActiveVortex(nullptr);
	for (const runtime::Ptr<model::gameobjects::Kisk>& kisk : getVortexLocation()->getInvadersKisks().values()) { // Java: a new ArrayList copy
		kisk->getController().die();
	}
	for (const runtime::Ptr<Player>& invader : invaders.values()) {
		if (invader->isOnline()) {
			kickPlayer(*invader, true);
		}
	}
	despawn();
	spawn(model::vortex::VortexStateType::PEACE);
}

// Java Invasion.java:62-88
void Invasion::addPlayer(Player& player, bool isInvader) {
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<Player>>& participants = isInvader ? invaders : defenders;
	runtime::Ptr<PlayerAlliance> alliance = isInvader ? invAlliance.get() : defAlliance.get();

	if (alliance && !alliance->isDisbanded()) {
		PlayerAllianceService::addPlayer(*alliance, player);
	} else if (participants.size() == 1) { // create alliance once two players participate in this invasion
		runtime::Ptr<Player> otherPlayer = *participants.values().begin(); // Java: values().iterator().next()
		for (Player* p : {&player, otherPlayer.get()}) {
			if (p->isInGroup()) {
				model::team::group::PlayerGroupService::removePlayer(*p);
			} else if (p->isInAlliance()) {
				PlayerAllianceService::removePlayer(*p);
			}
		}

		if (isInvader)
			invAlliance.set(PlayerAllianceService::createAlliance(*otherPlayer, player, model::team::TeamType::ALLIANCE_OFFENCE));
		else
			defAlliance.set(PlayerAllianceService::createAlliance(*otherPlayer, player, model::team::TeamType::ALLIANCE_DEFENCE));
	} else if (participants.size() > 1) { // should never happen
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.vortex.Invasion")
			.warn("Couldn't add " + player.toString() + " to " + (isInvader ? "invaders" : "defenders") +
				" (alliance not initialized). Current participants: " + std::to_string(participants.size()));
		return;
	}
	participants.put(player.getObjectId(), runtime::Ref<Player>(player));
}

// Java Invasion.java:91-118
void Invasion::kickPlayer(Player& player, bool isInvader) {
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<Player>>& participants = isInvader ? invaders : defenders;
	runtime::Ptr<PlayerAlliance> alliance = isInvader ? invAlliance.get() : defAlliance.get();

	participants.remove(player.getObjectId());

	if (alliance && alliance->hasMember(player.getObjectId())) {
		if (player.isOnline()) {
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(isInvader ? 1401452 : 1401476, std::vector<std::string>{}));
		}
		PlayerAllianceService::removePlayer(player);
		if (alliance->isDisbanded()) {
			if (isInvader) {
				invAlliance.set(nullptr);
			} else {
				defAlliance.set(nullptr);
			}
		}
	}

	if (isInvader && player.isOnline() && player.getWorldId() == getVortexLocation()->getInvasionWorldId()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INVADE_DIRECT_PORTAL_OUT_COMPULSION());
		services::teleport::TeleportService::teleportTo(player, *getVortexLocation()->getHomePoint());
	}

	getVortexLocation()->getVortexController()->getPassedPlayers().remove(player.getObjectId());
	getVortexLocation()->getVortexController()->syncPassed(true);
}

// Java Invasion.java:121-149
void Invasion::updateDefenders(Player& defender) {
	if (defenders.containsKey(defender.getObjectId())) {
		return;
	}

	runtime::Ptr<PlayerAlliance> alliance = defAlliance.get();
	if (!alliance || !alliance->isFull()) {
		runtime::Ref<Invasion_RequestResponseHandler> responseHandler = Invasion_RequestResponseHandler::create(defender, *this);

		const bool requested = defender.getResponseRequester().putRequest(904306, responseHandler);
		if (requested) {
			utils::PacketSendUtility::sendPacket(defender, network::aion::serverpackets::SM_QUESTION_WINDOW(904306, 0, 0));
		}
	}
}

// Java Invasion.java:152-158
void Invasion::updateInvaders(Player& invader) {
	if (invaders.containsKey(invader.getObjectId())) {
		return;
	}

	addPlayer(invader, true);
}

// Java Invasion.java:160-166
void Invasion::updateAlliance() {
	for (const runtime::Ptr<Player>& player : getVortexLocation()->getPlayers().values()) {
		if (player->getRace() == getVortexLocation()->getDefendersRace()) {
			updateDefenders(*player);
		}
	}
}

// Java Invasion.java:169-171 (Java returns the live map; C++ a copy of its entries)
std::unordered_map<int32_t, runtime::Ptr<Player>> Invasion::getInvaders() {
	return copyOf(invaders);
}

// Java Invasion.java:174-176
std::unordered_map<int32_t, runtime::Ptr<Player>> Invasion::getDefenders() {
	return copyOf(defenders);
}

Invasion::~Invasion() = default;

} // namespace aion::gameserver::services::vortex
