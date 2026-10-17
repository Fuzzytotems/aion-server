#include "aion/gameserver/handlers/ai/siege/GateRepairAI.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/gameobjects/player/RequestResponseHandler.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.h"
#include "aion/gameserver/model/templates/siegelocation/DoorRepairStone.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/SiegeException.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::siege {

AION_AI(GateRepairAI, "siege_gaterepair");

const commons::logging::Logger GateRepairAI::log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

/**
 * Java: the two anonymous RequestResponseHandler<Npc> of handleDialogStart (GateRepairAI.java:51-68, fieldmap
 * ai.siege.GateRepairAI$1 and $1$1); each acceptRequest body is the lambda, written where Java writes it.
 */
struct GateRepairAI_Handler final : RequestResponseHandler {
	AION_MAKE_REF_FRIEND

	const std::function<void(runtime::Ptr<Creature>, Player&)> accept;

	static runtime::Ref<GateRepairAI_Handler> create(runtime::Ptr<Creature> requesterValue, std::function<void(runtime::Ptr<Creature>, Player&)> accept) {
		return runtime::makeRef<GateRepairAI_Handler>(requesterValue, std::move(accept));
	}

	void acceptRequest(runtime::Ptr<Creature> requesterCreature, Player& responder) override { accept(requesterCreature, responder); }

protected:
	GateRepairAI_Handler(runtime::Ptr<Creature> requesterValue, std::function<void(runtime::Ptr<Creature>, Player&)> acceptValue)
		: RequestResponseHandler(requesterValue), accept(std::move(acceptValue)) {}
	~GateRepairAI_Handler() override = default;
};

// Java GateRepairAI.java:42-45
runtime::Ptr<SiegeSpawnTemplate> GateRepairAI::getSpawnTemplate() {
	return runtime::cast<SiegeSpawnTemplate>(NpcAI::getSpawnTemplate());
}

// Java GateRepairAI.java:47-71 (repairData is a static template: the handlers keep its address)
void GateRepairAI::handleDialogStart(Player& player) {
	const model::templates::siegelocation::DoorRepairData* repairData = SiegeService::getInstance().getDoorRepairData(getSpawnTemplate()->getSiegeId());
	if (repairData == nullptr)
		return;
	runtime::Ref<GateRepairAI> self(*this);
	runtime::Ref<Player> requestingPlayer(player);
	runtime::Ref<GateRepairAI_Handler> gaterepair =
		GateRepairAI_Handler::create(runtime::Ptr<Creature>(getOwner()), [self, requestingPlayer, repairData](runtime::Ptr<Creature> requester, Player& responder) {
			static_cast<void>(responder);
			runtime::Ref<GateRepairAI_Handler> repairstone =
				GateRepairAI_Handler::create(requester, [self, repairData](runtime::Ptr<Creature> stoneRequester, Player& stoneResponder) {
					static_cast<void>(stoneRequester);
					if (self->canActivate(stoneResponder, *repairData)) {
						self->onActivate(stoneResponder, *repairData);
					}
				});
			if (requestingPlayer->getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_ASK_DOOR_REPAIR_DO_YOU_ACCEPT_REPAIR, repairstone)) {
				PacketSendUtility::sendPacket(*requestingPlayer,
					SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_DOOR_REPAIR_DO_YOU_ACCEPT_REPAIR, self->getObjectId(), 5,
						dataholders::DataManager::ITEM_DATA->getItemTemplate(repairData->getItemId())->getL10n(), repairData->getCount()));
			}
		});
	if (player.getResponseRequester().putRequest(SM_QUESTION_WINDOW::STR_ASK_DOOR_REPAIR_POPUPDIALOG, gaterepair))
		PacketSendUtility::sendPacket(player, SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_ASK_DOOR_REPAIR_POPUPDIALOG,
			runtime::cast<SiegeNpc>(getOwner())->getSiegeId(), getCooldown()));
}

// Java GateRepairAI.java:73-75
void GateRepairAI::handleDialogFinish(Player& player) {
	static_cast<void>(player);
}

// Java GateRepairAI.java:77-96
void GateRepairAI::onActivate(Player& player, const model::templates::siegelocation::DoorRepairData& repairData) {
	const model::templates::siegelocation::DoorRepairStone* repairStone =
		getSpawnTemplate() == nullptr ? nullptr : repairData.getRepairStone(getSpawnTemplate()->getStaticId());
	if (repairStone == nullptr)
		return;
	if (runtime::Ptr<Creature> door = runtime::as<Creature>(getPosition()->getWorldMapInstance()->getObjectByStaticId(repairStone->getDoorId()))) {
		int32_t healValue = static_cast<int32_t>(std::llround(door->getLifeStats()->getMaxHp() * SiegeConfig::DOOR_REPAIR_HEAL_PERCENT.load()));
		if (door->getLifeStats()->getCurrentHp() + healValue > door->getLifeStats()->getMaxHp()) {
			healValue = door->getLifeStats()->getMaxHp() - door->getLifeStats()->getCurrentHp();
		}
		if (LoggingConfig::LOG_SIEGE.load())
			log.info("Gate Repair Stone with staticId: " + std::to_string(getSpawnTemplate()->getStaticId()) + " siege: " +
				std::to_string(getSpawnTemplate()->getSiegeId()) + " activated by " + player.toString() + " (race: " +
				std::string(xml::enumName(player.getRace())) + ") to heal door with staticId: " + std::to_string(door->getSpawn()->getStaticId()) +
				" by " + std::to_string(healValue));
		nextActivationTime.set(commons::utils::currentTimeMillis() + repairData.getCd());
		PacketSendUtility::broadcastPacket(getOwner(), SM_SYSTEM_MESSAGE::STR_MSG_REPAIR_ABYSS_DOOR(player.getName(), std::to_string(healValue)));
		PacketSendUtility::broadcastPacket(getOwner(), SM_ACTION_ANIMATION(getObjectId(), ActionAnimation::REPAIR_GATE, door->getObjectId()));
		door->getLifeStats()->increaseHp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::DOOR_REPAIR, healValue);
	} else {
		throw services::siege::SiegeException("Could not find a door to repair for siege " + std::to_string(getSpawnTemplate()->getSiegeId()) +
			" for npc_id = " + std::to_string(getNpcId()) + " with static_id = " + std::to_string(getSpawnTemplate()->getStaticId()));
	}
}

// Java GateRepairAI.java:98-100
int32_t GateRepairAI::getCooldown() {
	return nextActivationTime.get() <= 0 ? 0
	                                     : static_cast<int32_t>(std::max<int64_t>(0, (nextActivationTime.get() - commons::utils::currentTimeMillis()) / 1000));
}

// Java GateRepairAI.java:102-119 (getFortress(...).getRace() on null is Java's NullPointerException)
bool GateRepairAI::canActivate(Player& player, const model::templates::siegelocation::DoorRepairData& repairData) {
	runtime::Ptr<model::siege::FortressLocation> loc = SiegeService::getInstance().getFortress(getSpawnTemplate()->getSiegeId());
	if (getCooldown() > 0) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_DOOR_REPAIR_OUT_OF_COOLTIME());
		return false;
	}
	if (loc == nullptr)
		throw runtime::NullPointerException("FortressLocation " + std::to_string(getSpawnTemplate()->getSiegeId()));
	if (model::siege::getRaceId(loc->getRace()) != model::getRaceId(player.getRace())) {
		return false;
	}
	if (loc->getLegionId() > 0 && (!player.isLegionMember() || player.getLegion()->getLegionId() != loc->getLegionId()
		|| !player.getLegionMember()->hasRights(LegionPermissionsMask::ARTIFACT))) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_DOOR_REPAIR_HAVE_NO_AUTHORITY());
	} else if (player.getInventory().getItemCountByItemId(repairData.getItemId()) >= repairData.getCount()
		&& player.getInventory().decreaseByItemId(repairData.getItemId(), repairData.getCount())) {
		return true;
	} else {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_DOOR_REPAIR_NOT_ENOUGH_FEE(
			dataholders::DataManager::ITEM_DATA->getItemTemplate(repairData.getItemId())->getL10n(), std::to_string(repairData.getCount())));
	}
	return false;
}

} // namespace aion::gameserver::handlers::ai::siege
