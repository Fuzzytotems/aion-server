#include "aion/gameserver/services/RecallService.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECALLED_BY_OTHER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/templates/zone/ZoneClassName.h"
#include "aion/gameserver/model/templates/zone/ZoneTemplate.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/zone/ZoneAttributes.h"
#include "aion/gameserver/world/zone/ZoneAttributesInfo.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.RecallService@L61:62

RecallService::Request::Request(int32_t value, int32_t worldIdValue, int32_t instanceIdValue, float xValue, float yValue, float zValue,
	int8_t headingValue)
	: casterObjectId(value), worldId(worldIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue), heading(headingValue) {
}

runtime::Ref<RecallService::Request> RecallService::Request::create(int32_t value, int32_t worldIdValue, int32_t instanceIdValue, float xValue,
	float yValue, float zValue, int8_t headingValue) {
	return runtime::makeRef<RecallService::Request>(value, worldIdValue, instanceIdValue, xValue, yValue, zValue, headingValue);
}

RecallService::Request::~Request() = default;

RecallService& RecallService::getInstance() {
	static RecallService instance; // Java SingletonHolder
	return instance;
}

RecallService::RecallService() = default;

bool RecallService::hasPendingRequest(model::gameobjects::player::Player& summoned) {
	return requests.containsKey(summoned.getObjectId());
}

void RecallService::requestSummon(model::gameobjects::player::Player& caster, model::gameobjects::player::Player& summoned, int32_t skillId) {
	runtime::Ref<Request> request = Request::create(caster.getObjectId(), caster.getWorldId(), caster.getInstanceId(), caster.getX(), caster.getY(),
		caster.getZ(), caster.getHeading());
	if (requests.putIfAbsent(summoned.getObjectId(), request))
		return;
	// lambda at RecallService.java:61-64 (cycles.toml RecallService@L61:62#request, an accepted one-shot): the summoned player and the request
	// travel as Refs in an unpinned task
	request->timeout.set(utils::ThreadPoolManager::getInstance().schedule(
		runtime::bindTask(
			[](model::gameobjects::player::Player& summonedPlayer, const runtime::Ref<Request>& timedOut) {
				// never time out a request which replaced this one
				if (getInstance().requests.get(summonedPlayer.getObjectId()).rawPointer() == timedOut.get())
					getInstance().cancel(summonedPlayer, CancelReason::TIMEOUT);
			},
			runtime::Ref<model::gameobjects::player::Player>(summoned), request),
		CONFIRMATION_SECONDS * 1000));
	utils::PacketSendUtility::sendPacket(summoned, network::aion::serverpackets::SM_RECALLED_BY_OTHER(caster.getName(), skillId, CONFIRMATION_SECONDS));
}

void RecallService::accept(model::gameobjects::player::Player& summoned) {
	runtime::Ptr<Request> request = remove(summoned);
	if (request)
		teleport::TeleportService::teleportTo(summoned, request->worldId, request->instanceId, request->x, request->y, request->z, request->heading);
}

void RecallService::cancel(model::gameobjects::player::Player& summoned, RecallService::CancelReason reason) {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	runtime::Ptr<Request> request = remove(summoned);
	if (!request)
		return;
	if (reason == CancelReason::TIMEOUT || reason == CancelReason::CANCELLED)
		PacketSendUtility::sendPacket(summoned, network::aion::serverpackets::SM_RECALLED_BY_OTHER()); // the client closes the window itself only when it answered

	runtime::Ptr<model::gameobjects::player::Player> caster = world::World::getInstance().getPlayer(request->casterObjectId);
	if (!caster)
		return;
	switch (reason) {
		case CancelReason::TIMEOUT:
			PacketSendUtility::sendPacket(*caster, SM_SYSTEM_MESSAGE::STR_MSG_Recall_DONOT_ACCEPT_EFFECT(summoned.getName()));
			break;
		case CancelReason::DECLINED:
			PacketSendUtility::sendPacket(*caster, SM_SYSTEM_MESSAGE::STR_MSG_Recall_Rejected_EFFECT(summoned.getName()));
			PacketSendUtility::sendPacket(summoned, SM_SYSTEM_MESSAGE::STR_MSG_Recall_Reject_EFFECT(caster->getName()));
			break;
		case CancelReason::CANCELLED:
			PacketSendUtility::sendPacket(*caster, SM_SYSTEM_MESSAGE::STR_MSG_Recall_CANCEL_EFFECT(summoned.getName()));
			PacketSendUtility::sendPacket(summoned, SM_SYSTEM_MESSAGE::STR_MSG_Recall_CANCEL_EFFECT(caster->getName()));
			break;
	}
}

runtime::Ptr<RecallService::Request> RecallService::remove(model::gameobjects::player::Player& summoned) {
	runtime::Ptr<Request> request = requests.remove(summoned.getObjectId());
	if (request) {
		if (runtime::FutureRef timeout = request->timeout.get())
			timeout->cancel(false);
	}
	return request;
}

bool RecallService::validateCast(model::gameobjects::player::Player& caster, runtime::Ptr<model::gameobjects::VisibleObject> target) {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	using utils::PacketSendUtility;
	if (caster.isFlying()) {
		PacketSendUtility::sendPacket(caster, SM_SYSTEM_MESSAGE::STR_SKILL_RESTRICTION_NO_FLY());
		return false;
	}
	if (!canRecallAt(caster)) {
		PacketSendUtility::sendPacket(caster, SM_SYSTEM_MESSAGE::STR_SKILL_CANT_CAST_IN_CURRENT_POSTION());
		return false;
	}
	runtime::Ptr<model::gameobjects::player::Player> targetPlayer = runtime::as<model::gameobjects::player::Player>(target);
	if (!targetPlayer) {
		PacketSendUtility::sendPacket(caster, SM_SYSTEM_MESSAGE::STR_SKILL_TARGET_IS_NOT_VALID());
		return false;
	}
	if (getInstance().hasPendingRequest(*targetPlayer)) {
		PacketSendUtility::sendPacket(caster, SM_SYSTEM_MESSAGE::STR_MSG_Recall_DUPLICATE_EFFECT(targetPlayer->getName()));
		return false;
	}
	if (!canBeSummoned(caster, *targetPlayer)) {
		PacketSendUtility::sendPacket(caster, SM_SYSTEM_MESSAGE::STR_MSG_Recall_CANNOT_ACCEPT_EFFECT(targetPlayer->getName()));
		return false;
	}
	return true;
}

bool RecallService::canBeSummoned(model::gameobjects::Creature& caster, model::gameobjects::Creature& summoned) {
	runtime::Ptr<model::gameobjects::player::Player> summonedPlayer = runtime::as<model::gameobjects::player::Player>(summoned);
	if (!summonedPlayer || &caster == &summoned)
		return false;
	if (caster.getWorldId() != summoned.getWorldId() || caster.getInstanceId() != summoned.getInstanceId())
		return false;
	if (caster.isEnemy(summoned) || summonedPlayer->isDead())
		return false;
	if (summonedPlayer->getController().isInCombat() || summonedPlayer->isUsingFlightTransporterOrWindstream())
		return false;
	if (summonedPlayer->isInState(model::gameobjects::state::CreatureState::PRIVATE_SHOP))
		return false;
	if (summonedPlayer->getInteractionTask()) // gathering or crafting
		return false;
	return !summonedPlayer->getTransformModel().cantRecall();
}

bool RecallService::canRecallAt(model::gameobjects::player::Player& caster) {
	const model::templates::zone::ZoneTemplate* decisive = nullptr;
	for (const runtime::Ptr<world::zone::ZoneInstance>& zone : caster.findZones()) {
		const model::templates::zone::ZoneTemplate* zoneTemplate = zone->getZoneTemplate();
		if (zoneTemplate->getZoneType() != model::templates::zone::ZoneClassName::LIMIT || zoneTemplate->getFlags() == -1) // no flags at all means no information
			continue;
		if (decisive == nullptr || zoneTemplate->getPriority() < decisive->getPriority())
			decisive = zoneTemplate;
	}
	if (decisive != nullptr && (decisive->getFlags() & world::zone::getId(world::zone::ZoneAttributes::RECALL)) == 0)
		return false;
	return world::World::getInstance().getWorldMap(caster.getWorldId())->canRecall();
}

} // namespace aion::gameserver::services
