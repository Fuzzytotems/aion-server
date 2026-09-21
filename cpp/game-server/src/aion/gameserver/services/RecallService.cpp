#include "aion/gameserver/services/RecallService.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_RECALLED_BY_OTHER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
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
	AION_UNPORTED();
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

bool RecallService::validateCast(model::gameobjects::player::Player& caster, model::gameobjects::VisibleObject& target) {
	AION_UNPORTED();
}

bool RecallService::canBeSummoned(model::gameobjects::Creature& caster, model::gameobjects::Creature& summoned) {
	AION_UNPORTED();
}

bool RecallService::canRecallAt(model::gameobjects::player::Player& caster) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
