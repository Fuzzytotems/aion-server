#include "aion/gameserver/model/gameobjects/StaticDoor.h"

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorState.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::model::gameobjects {

namespace {

using templates::staticdoor::StaticDoorState;

} // namespace

// StaticDoor(CreateKey, std::unique_ptr<controllers::StaticObjectController>, SpawnTemplate&, const StaticDoorTemplate*, int32_t) is defined with
// StaticObject's constructor (controllers/StaticObjectController.h, P4-11b). Java body after super(...):
//   states = EnumSet.noneOf(StaticDoorState.class); StaticDoorState.setStates(getObjectTemplate().getState(), states);
//   if (objectTemplate.getKeyId() < 2) isLocked = false;

StaticDoor::~StaticDoor() = default;

bool StaticDoor::isOpen() {
	return states.contains(StaticDoorState::OPENED);
}

std::set<StaticDoorState> StaticDoor::getStates() {
	std::set<StaticDoorState> result;
	for (StaticDoorState state : states)
		result.insert(state);
	return result;
}

void StaticDoor::setOpen(bool open) {
	EmotionType emotion;
	int32_t packetState; // not important IMO, similar to internal state
	if (open) {
		emotion = EmotionType::OPEN_DOOR;
		states.remove(StaticDoorState::CLICKABLE);
		states.add(StaticDoorState::OPENED); // 1001
		packetState = 0x9;
		world::geo::GeoService::getInstance().setDoorState(getWorldId(), getInstanceId(), getSpawn()->getStaticId(), true);
	} else {
		emotion = EmotionType::CLOSE_DOOR;
		if ((getObjectTemplate()->getState() & detail::flagOf(StaticDoorState::CLICKABLE)) == detail::flagOf(StaticDoorState::CLICKABLE))
			states.add(StaticDoorState::CLICKABLE);
		states.remove(StaticDoorState::OPENED); // 1010
		packetState = 0xA;
		world::geo::GeoService::getInstance().setDoorState(getWorldId(), getInstanceId(), getSpawn()->getStaticId(), false);
	}
	// int stateFlags = StaticDoorState.getFlags(states);
	utils::PacketSendUtility::broadcastPacket(*this, network::aion::serverpackets::SM_EMOTION(getSpawn()->getStaticId(), emotion, packetState));
}

void StaticDoor::changeState(bool open, int32_t state) {
	state = state & 0xF;
	detail::setStates(state, states);
	EmotionType emotion = open ? EmotionType::OPEN_DOOR : EmotionType::CLOSE_DOOR;
	utils::PacketSendUtility::broadcastPacket(*this, network::aion::serverpackets::SM_EMOTION(getSpawn()->getStaticId(), emotion, state));
}

const templates::staticdoor::StaticDoorTemplate* StaticDoor::getObjectTemplate() const {
	return static_cast<const templates::staticdoor::StaticDoorTemplate*>(StaticObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
