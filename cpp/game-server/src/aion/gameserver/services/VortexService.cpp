#include "aion/gameserver/services/VortexService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"

namespace aion::gameserver::services {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.VortexService@L38:39
//   com.aionemu.gameserver.services.VortexService@L39:39
//   com.aionemu.gameserver.services.VortexService@L57:44

void VortexService::initVortexLocations() {
	AION_UNPORTED();
}

void VortexService::startInvasion(int32_t id) { // lint: L7 unported stub; the port adds the Java synchronized block
	AION_UNPORTED();
}

void VortexService::stopInvasion(int32_t id) {
	AION_UNPORTED();
}

void VortexService::spawn(model::vortex::VortexLocation& loc, model::vortex::VortexStateType state) {
	AION_UNPORTED();
}

void VortexService::despawn(model::vortex::VortexLocation& loc) {
	AION_UNPORTED();
}

bool VortexService::isInvasionInProgress(int32_t id) {
	AION_UNPORTED();
}

int32_t VortexService::getDuration() {
	AION_UNPORTED();
}

void VortexService::removeDefenderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void VortexService::removeInvaderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool VortexService::isInvaderPlayer(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool VortexService::isInsideVortexZone(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByRift(int32_t npcId) {
	AION_UNPORTED();
}

runtime::Ptr<model::vortex::VortexLocation> VortexService::getLocationByWorld(int32_t worldId) {
	AION_UNPORTED();
}

VortexService& VortexService::getInstance() {
	static VortexService instance; // Java VortexServiceHolder
	return instance;
}

} // namespace aion::gameserver::services
