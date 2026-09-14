#include "aion/gameserver/services/RecallService.h"

#include "aion/gameserver/runtime/base/Unported.h"

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
	AION_UNPORTED();
}

void RecallService::requestSummon(model::gameobjects::player::Player& caster, model::gameobjects::player::Player& summoned, int32_t skillId) {
	AION_UNPORTED();
}

void RecallService::accept(model::gameobjects::player::Player& summoned) {
	AION_UNPORTED();
}

void RecallService::cancel(model::gameobjects::player::Player& summoned, RecallService::CancelReason reason) {
	AION_UNPORTED();
}

runtime::Ptr<RecallService::Request> RecallService::remove(model::gameobjects::player::Player& summoned) {
	AION_UNPORTED();
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
