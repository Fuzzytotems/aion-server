#include "aion/gameserver/spawnengine/WalkerGroup.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/templates/zone/Point2D.h"
#include "aion/gameserver/spawnengine/ClusteredNpc.h"
#include "aion/gameserver/spawnengine/WalkerGroupType.h"

namespace aion::gameserver::spawnengine {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.spawnengine.WalkerGroup");

WalkerGroup::WalkerGroup(const std::vector<runtime::Ptr<ClusteredNpc>>& membersValue)
	// Java: members sorted by walker index (nulls last, reversed); the first member's position and walk template give walkerXpos, walkerYpos,
	// type and versionId (neutral values until the constructor is ported)
	: type(WalkerGroupType::POINT), walkerXpos(0), walkerYpos(0),
	  memberSteps(runtime::Array<int32_t>::make(static_cast<int32_t>(membersValue.size()))) {
	AION_UNPORTED();
}

WalkerGroup::~WalkerGroup() = default;

runtime::Ref<WalkerGroup> WalkerGroup::create(const std::vector<runtime::Ptr<ClusteredNpc>>& membersValue) {
	return runtime::makeRef<WalkerGroup>(membersValue);
}

void WalkerGroup::form() {
	AION_UNPORTED();
}

float WalkerGroup::getSidesExtra(std::span<const int32_t> rows, int32_t startIndex, int32_t endIndex) {
	AION_UNPORTED();
}

model::templates::zone::Point2D WalkerGroup::getLinePoint(const model::templates::zone::Point2D& origin,
	const model::templates::zone::Point2D& destination, WalkerGroupShift& shift) {
	AION_UNPORTED();
}

runtime::Ref<WalkerGroupShift> WalkerGroup::getShiftSigns(const model::templates::zone::Point2D& origin,
	const model::templates::zone::Point2D& destination) {
	AION_UNPORTED();
}

void WalkerGroup::setStep(model::gameobjects::Npc& member, int32_t step) {
	AION_UNPORTED();
}

void WalkerGroup::targetReached(ai::NpcAI& npcAI) {
	AION_UNPORTED();
}

void WalkerGroup::spawn() {
	AION_UNPORTED();
}

void WalkerGroup::respawn(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

void WalkerGroup::despawn() {
	AION_UNPORTED();
}

runtime::Ptr<ClusteredNpc> WalkerGroup::getClusterData(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

float WalkerGroup::getHeight(float x, float y, model::templates::spawns::SpawnTemplate& template_) {
	AION_UNPORTED();
}

int32_t WalkerGroup::getPool() {
	AION_UNPORTED();
}

bool WalkerGroup::isLinearlyPositioned(model::gameobjects::Npc& npc) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::spawnengine
