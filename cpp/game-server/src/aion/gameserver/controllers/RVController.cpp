#include "aion/gameserver/controllers/RVController.h"

#include <array>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/RiftService.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/rift/RiftInformer.h"
#include "aion/gameserver/services/rift/RiftManager.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::controllers {

using model::gameobjects::Creature;
using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::gameobjects::player::RequestResponseHandler;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using runtime::Ptr;
using runtime::Ref;
using utils::PacketSendUtility;

namespace {

/** Java constructor: deSpawnedTime = ((int) (System.currentTimeMillis() / 1000)) + (isVortex ? vortex duration : rift duration) * 3600 */
int32_t despawnTime(bool isVortex) {
	int32_t nowSeconds = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);
	int32_t durationHours = isVortex ? services::VortexService::getInstance().getDuration() : services::RiftService::getInstance().getDuration();
	return detail::add(nowSeconds, detail::mul(durationHours, 3600));
}

} // namespace

/** Java: anonymous RequestResponseHandler<Npc> of onRequest for vortex rifts (RVController$1, capture this$0 = the controller) */
class RVController_RequestResponseHandler final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const Ref<RVController> rVController;

protected:
	RVController_RequestResponseHandler(Npc& requester, RVController& controller) : RequestResponseHandler(requester), rVController(controller) {}
	~RVController_RequestResponseHandler() override = default;

public:
	static Ref<RVController_RequestResponseHandler> create(Npc& requester, RVController& controller) {
		return runtime::makeRef<RVController_RequestResponseHandler>(requester, controller);
	}

	void acceptRequest(Ptr<Creature> requesterValue, Player& responder) override {
		RVController& controller = *rVController;
		if (controller.onAccept(responder)) {
			if (responder.isInTeam()) {
				if (runtime::as<model::team::group::PlayerGroup>(responder.getCurrentTeam())) {
					standins::playerGroupServiceRemovePlayer(responder);
				} else {
					standins::playerAllianceServiceRemovePlayer(responder);
				}
			}

			Ptr<model::vortex::VortexLocation> loc = services::VortexService::getInstance().getLocationByRift(runtime::cast<Npc>(requesterValue)->getNpcId());
			services::teleport::TeleportService::teleportTo(responder, *loc->getStartPoint());

			PacketSendUtility::sendPacket(responder, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_INVADE_DIRECT_PORTAL_OPEN_NOTICE());

			// Update passed players count
			controller.passedPlayers.put(responder.getObjectId(), Ref<Player>(responder));
			controller.syncPassed(true);
		}
	}
};

/** Java: anonymous RequestResponseHandler<Npc> of onRequest for rifts (RVController$2, capture this$0 = the controller) */
class RVController_RequestResponseHandler_2 final : public RequestResponseHandler {
	AION_MAKE_REF_FRIEND
public:
	const Ref<RVController> rVController;

protected:
	RVController_RequestResponseHandler_2(Npc& requester, RVController& controller) : RequestResponseHandler(requester), rVController(controller) {}
	~RVController_RequestResponseHandler_2() override = default;

public:
	static Ref<RVController_RequestResponseHandler_2> create(Npc& requester, RVController& controller) {
		return runtime::makeRef<RVController_RequestResponseHandler_2>(requester, controller);
	}

	void acceptRequest(Ptr<Creature> requesterValue, Player& responder) override {
		RVController& controller = *rVController;
		if (controller.onAccept(responder)) {
			int32_t worldId = controller.slaveSpawnTemplate->getWorldId();
			float x = controller.slaveSpawnTemplate->getX();
			float y = controller.slaveSpawnTemplate->getY();
			float z = controller.slaveSpawnTemplate->getZ();

			services::teleport::TeleportService::teleportTo(responder, worldId, x, y, z);
			// Update passed players count
			controller.syncPassed(false);
		}
	}
};

RVController::RVController(runtime::Ptr<model::gameobjects::Npc> value, services::rift::RiftEnum riftTemplateValue)
	: isMaster_(static_cast<bool>(value)), isVortex_(standins::riftEnumData(riftTemplateValue).vortex), isVolatile_(false),
	  isInvasion_(standins::riftEnumData(riftTemplateValue).isInvasionRift), slaveSpawnTemplate(value ? value->getSpawn() : nullptr), slave(value),
	  maxEntries(standins::riftEnumData(riftTemplateValue).entries), minLevel(standins::riftEnumData(riftTemplateValue).minLevel),
	  maxLevel(standins::riftEnumData(riftTemplateValue).maxLevel), isAccepting(static_cast<bool>(value)), riftTemplate(riftTemplateValue),
	  deSpawnedTime(despawnTime(isVortex_)) {
	// Java: if (slave != null) { this.slave = slave; this.slaveSpawnTemplate = slave.getSpawn(); isMaster = true; isAccepting = true; } (the
	// members are const: the initializer list applies the condition)
}

RVController::RVController(runtime::Ptr<model::gameobjects::Npc> value, services::rift::RiftEnum riftTemplateValue, bool isWithGuards)
	: isMaster_(static_cast<bool>(value)), isVortex_(standins::riftEnumData(riftTemplateValue).vortex),
	  isVolatile_(value && standins::riftEnumData(riftTemplateValue).canBeVolatile && isWithGuards),
	  isInvasion_(standins::riftEnumData(riftTemplateValue).isInvasionRift), slaveSpawnTemplate(value ? value->getSpawn() : nullptr), slave(value),
	  maxEntries(standins::riftEnumData(riftTemplateValue).entries), minLevel(standins::riftEnumData(riftTemplateValue).minLevel),
	  maxLevel(standins::riftEnumData(riftTemplateValue).maxLevel), isAccepting(static_cast<bool>(value)), riftTemplate(riftTemplateValue),
	  deSpawnedTime(despawnTime(isVortex_)) {
	// Java: if (slave != null) { ...; isVolatile = riftTemplate.canBeVolatile() && isWithGuards; }
}

void RVController::onDialogRequest(model::gameobjects::player::Player& player) {
	if (isMaster_ || isAccepting) {
		if (isInvasion_ && player.getOppositeRace() != standins::riftEnumData(riftTemplate).destination)
			return;
		onRequest(player);
	}
}

void RVController::onRequest(model::gameobjects::player::Player& player) {
	if (isVortex_) {
		Ref<RVController_RequestResponseHandler> responseHandler = RVController_RequestResponseHandler::create(getOwner(), *this);

		bool requested = player.getResponseRequester().putRequest(904304, responseHandler);
		if (requested) {
			PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(904304, getOwner().getObjectId(), 5));
		}
	} else {
		Ref<RVController_RequestResponseHandler_2> responseHandler = RVController_RequestResponseHandler_2::create(getOwner(), *this);

		bool requested = player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_ASK_PASS_BY_DIRECT_PORTAL, responseHandler);
		if (requested) {
			PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_PASS_BY_DIRECT_PORTAL, 0, 0));
		}
	}
}

bool RVController::onAccept(model::gameobjects::player::Player& player) {
	if (!isAccepting) {
		return false;
	}

	if (!getOwner().isSpawned()) {
		return false;
	}

	// Java: unboxing of the Integer getters
	if (player.getLevel() > detail::unbox(getMaxLevel(), "maxLevel") || player.getLevel() < detail::unbox(getMinLevel(), "minLevel")) {
		utils::audit::AuditLogger::log(player, "tried to use rift outside level restriction");
		return false;
	}

	if (isVortex_ && getUsedEntries() >= detail::unbox(getMaxEntries(), "maxEntries")) {
		return false;
	}

	return true;
}

void RVController::onDespawn() {
	services::rift::RiftInformer::sendRiftDespawn(getOwner().getWorldId(), getOwner().getObjectId());
	services::rift::RiftManager::removeSpawnedRift(getOwner());
	NpcController::onDespawn();
}

int32_t RVController::getRemainTime() {
	return detail::sub(deSpawnedTime, static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
}

void RVController::syncPassed(bool invasion) {
	usedEntries = invasion ? passedPlayers.size() : usedEntries.get() + 1; // java-race: ++usedEntries without a lock, as in Java
	std::vector<int32_t> worlds = getWorldsList(*this);
	services::rift::RiftInformer::sendRiftInfo(worlds);
}

std::vector<int32_t> RVController::getWorldsList(RVController& controller) {
	int32_t first = controller.getOwner().getWorldId();
	if (controller.isMaster()) {
		return {first, controller.slaveSpawnTemplate->getWorldId()};
	}
	return {first};
}

} // namespace aion::gameserver::controllers
