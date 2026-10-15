#include "aion/gameserver/model/vortex/VortexLocation.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/RVController.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/vortex/HomePoint.h"
#include "aion/gameserver/model/templates/vortex/ResurrectionPoint.h"
#include "aion/gameserver/model/templates/vortex/StartPoint.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/zone/InvasionZoneInstance.h"

namespace aion::gameserver::model::vortex {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

VortexLocation::VortexLocation(const templates::vortex::VortexTemplate* value)
	: template_(value) {
}

runtime::Ref<VortexLocation> VortexLocation::create(const templates::vortex::VortexTemplate* value) {
	return runtime::makeRef<VortexLocation>(value);
}

// Java VortexLocation.java:46-49
void VortexLocation::setActiveVortex(runtime::Ptr<services::vortex::DimensionalVortex> vortex) {
	isActive_.set(static_cast<bool>(vortex));
	this->activeVortex.set(vortex);
}

void VortexLocation::setVortexController(runtime::Ptr<controllers::RVController> controller) {
	this->vortexController.set(controller);
}

// Java VortexLocation.java:67-69
runtime::Ref<world::WorldPosition> VortexLocation::getHomePoint() {
	if (template_->getHomePoint() == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getHomePoint() is null");
	return template_->getHomePoint()->getHomePoint();
}

// Java VortexLocation.java:71-73
runtime::Ref<world::WorldPosition> VortexLocation::getResurrectionPoint() {
	if (template_->getResurrectionPoint() == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getResurrectionPoint() is null");
	return template_->getResurrectionPoint()->getResurrectionPoint();
}

// Java VortexLocation.java:75-77
runtime::Ref<world::WorldPosition> VortexLocation::getStartPoint() {
	if (template_->getStartPoint() == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getStartPoint() is null");
	return template_->getStartPoint()->getStartPoint();
}

int32_t VortexLocation::getId() {
	return template_->getId();
}

Race VortexLocation::getDefendersRace() {
	// Java returns the template's nullable race; both vortex_location entries of dimensional_vortex.xml have defends_race
	const std::optional<Race> race = template_->getDefendersRace();
	if (!race) // Java: null
		throw runtime::NullPointerException("VortexTemplate.getDefendersRace() is null for location " + std::to_string(template_->getId()));
	return *race;
}

Race VortexLocation::getInvadersRace() {
	// Java returns the template's nullable race; both vortex_location entries of dimensional_vortex.xml have offence_race
	const std::optional<Race> race = template_->getInvadersRace();
	if (!race) // Java: null
		throw runtime::NullPointerException("VortexTemplate.getInvadersRace() is null for location " + std::to_string(template_->getId()));
	return *race;
}

bool VortexLocation::isInvadersRace(Race race) {
	// Java: race.equals(getInvadersRace()) - false, not a NullPointerException, when the template has no offence_race
	const std::optional<Race> invaders = template_->getInvadersRace();
	return invaders && *invaders == race;
}

int32_t VortexLocation::getHomeWorldId() {
	const auto* home = template_->getHomePoint();
	if (home == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getHomePoint() is null");
	return home->getWorldId();
}

int32_t VortexLocation::getInvasionWorldId() {
	const auto* start = template_->getStartPoint();
	if (start == nullptr) // Java: NullPointerException
		throw runtime::NullPointerException("VortexTemplate.getStartPoint() is null");
	return start->getWorldId();
}

// Java VortexLocation.java:111-113
bool VortexLocation::isInvaderInside(int32_t objId) {
	return isActive() && getVortexController()->getPassedPlayers().containsKey(objId);
}

bool VortexLocation::isInsideActiveVotrex(gameobjects::player::Player& player) {
	return isActive() && isInsideLocation(player);
}

void VortexLocation::addZone(world::zone::InvasionZoneInstance& zone) {
	zones.add(runtime::Ref<world::zone::InvasionZoneInstance>(zone));
	zone.addHandler(*this);
}

bool VortexLocation::isInsideLocation(gameobjects::Creature& creature) {
	if (!zones.isEmpty()) {
		for (runtime::Ptr<world::zone::InvasionZoneInstance> zone : zones) {
			if (zone->isInsideCreature(creature))
				return true;
		}
	}
	return false;
}

void VortexLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	if (auto* kisk = dynamic_cast<gameobjects::Kisk*>(&creature)) {
		if (isInvadersRace(creature.getRace())) {
			kisks.put(creature.getObjectId(), runtime::Ref<gameobjects::Kisk>(*kisk));
		}
	} else if (auto* player = dynamic_cast<gameobjects::player::Player*>(&creature)) {
		// java-race: Java's containsKey/put guard is not atomic (VortexLocation.java:145); a lost race leaves the previous Ref in the map
		// until onLeaveZone removes it by object id, exactly as in Java
		if (!players.containsKey(player->getObjectId())) {
			players.put(player->getObjectId(), runtime::Ref<gameobjects::player::Player>(*player));

			if (isActive()) {
				if (isInvadersRace(player->getRace())) {
					if (getVortexController()->getPassedPlayers().containsKey(player->getObjectId()) &&
						!getActiveVortex()->getInvaders().contains(player->getObjectId())) {
						getActiveVortex()->addPlayer(*player, true);
					}
				} else {
					getActiveVortex()->updateDefenders(*player);
				}
			}
		}
	}
}

// lambda at VortexLocation.java:181 (fieldmap key vortex.VortexLocation@L181:49)
// lambda at VortexLocation.java:189 (fieldmap key vortex.VortexLocation@L189:48)
void VortexLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	if (!isInsideLocation(creature)) {
		if (dynamic_cast<gameobjects::Kisk*>(&creature) != nullptr) {
			kisks.remove(creature.getObjectId());
		} else if (auto* playerPointer = dynamic_cast<gameobjects::player::Player*>(&creature)) {
			gameobjects::player::Player& player = *playerPointer;

			players.remove(player.getObjectId());

			if (isActive()) {
				if (isInvadersRace(player.getRace())) {
					if (getVortexController()->getPassedPlayers().containsKey(player.getObjectId())) {
						// You have left the battlefield.
						utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE(904305, std::vector<std::string>{}));

						// start kick timer
						utils::ThreadPoolManager::getInstance().schedule({this, &player}, [this, &player] {
							if (player.isOnline() && !isInsideActiveVotrex(player)) {
								getActiveVortex()->kickPlayer(player, true);
							}
						}, 10 * 1000);
					}
				} else {
					// start kick timer
					utils::ThreadPoolManager::getInstance().schedule({this, &player}, [this, &player] {
						if (player.isOnline() && !isInsideActiveVotrex(player)) {
							getActiveVortex()->kickPlayer(player, false);
						}
					}, 10 * 1000);
				}
			}
		}
	}
}

VortexLocation::~VortexLocation() = default;

} // namespace aion::gameserver::model::vortex
