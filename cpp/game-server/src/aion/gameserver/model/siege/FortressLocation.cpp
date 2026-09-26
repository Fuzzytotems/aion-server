#include "aion/gameserver/model/siege/FortressLocation.h"

#include <cstdlib>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/ShieldObserver.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ShieldService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/SiegeZoneInstance.h"

namespace aion::gameserver::model::siege {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

FortressLocation::FortressLocation(const templates::siegelocation::SiegeLocationTemplate* value)
	: SiegeLocation(value) {
}

runtime::Ref<FortressLocation> FortressLocation::create(const templates::siegelocation::SiegeLocationTemplate* value) {
	return runtime::makeRef<FortressLocation>(value);
}

std::vector<const templates::siegelocation::SiegeLegionReward*> FortressLocation::getLegionRewards() {
	AION_UNPORTED();
}

std::vector<const templates::siegelocation::SiegeMercenaryZone*> FortressLocation::getSiegeMercenaryZones() {
	AION_UNPORTED();
}

bool FortressLocation::isEnemy(gameobjects::Creature& creature) {
	return model::getRaceId(creature.getRace()) != siege::getRaceId(getRace());
}

void FortressLocation::onEnterZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	SiegeLocation::onEnterZone(creature, zone);
	creature.setInsideZoneType(templates::zone::ZoneType::SIEGE);
	checkForBalanceBuff(creature, SiegeBuffAction::ADD);
	if (isUnderShield() && getRace() != siege::getByRace(creature.getRace())) {
		runtime::Ref<controllers::observer::ShieldObserver> observer = services::ShieldService::getInstance().createShieldObserver(*this, creature);
		if (observer) {
			creature.getObserveController()->addObserver(*observer);
			shieldObservers.put(creature.getObjectId(), observer);
		}
	}
}

void FortressLocation::onLeaveZone(gameobjects::Creature& creature, world::zone::ZoneInstance& zone) {
	SiegeLocation::onLeaveZone(creature, zone);
	creature.unsetInsideZoneType(templates::zone::ZoneType::SIEGE);
	checkForBalanceBuff(creature, SiegeBuffAction::LEAVE_ZONE_REMOVE);
	runtime::Ptr<controllers::observer::ShieldObserver> observer = shieldObservers.remove(creature.getObjectId());
	if (observer)
		creature.getObserveController()->removeObserver(*observer);
}

void FortressLocation::checkForBalanceBuff(gameobjects::Creature& creature, FortressLocation::SiegeBuffAction siegeBuffAction) {
	auto* player = dynamic_cast<gameobjects::player::Player*>(&creature);
	if (player != nullptr && isVulnerable() && getFactionBalance() != 0) {
		switch (siegeBuffAction) {
			case SiegeBuffAction::LEAVE_ZONE_REMOVE:
			case SiegeBuffAction::SIEGE_END_REMOVE:
				for (int32_t i = 8867; i <= 8884; i++) {
					if (creature.getEffectController()->hasAbnormalEffect(i)) {
						creature.getEffectController()->removeEffect(i);
						if (creature.getRace() == Race::ELYOS) {
							utils::PacketSendUtility::sendPacket(*player, siegeBuffAction == SiegeBuffAction::LEAVE_ZONE_REMOVE
									? SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_LIGHT_GET_OUT_AREA()
									: SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_LIGHT_MIST_OFF());
						} else {
							utils::PacketSendUtility::sendPacket(*player, siegeBuffAction == SiegeBuffAction::LEAVE_ZONE_REMOVE
									? SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_DARK_GET_OUT_AREA()
									: SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_DARK_MIST_OFF());
						}
						break;
					}
				}
				break;
			case SiegeBuffAction::ADD: {
				int32_t balance = getFactionBalance();
				if (creature.getRace() == Race::ELYOS) {
					if (balance < 0) {
						skillengine::SkillEngine::getInstance().applyEffectDirectly(8866 + std::abs(balance), creature, creature);
						utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_LIGHT_GAIN());
					} else {
						utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_DARK_WARNING());
					}
				} else if (creature.getRace() == Race::ASMODIANS) {
					if (balance > 0) {
						skillengine::SkillEngine::getInstance().applyEffectDirectly(8875 + balance, creature, creature);
						utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_DARK_GAIN());
					} else {
						utils::PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_WEAK_RACE_BUFF_LIGHT_WARNING());
					}
				}
				break;
			}
			default:
				break;
		}
	}
}

void FortressLocation::clearLocation() {
	AION_UNPORTED();
}

FortressLocation::~FortressLocation() = default;

} // namespace aion::gameserver::model::siege
