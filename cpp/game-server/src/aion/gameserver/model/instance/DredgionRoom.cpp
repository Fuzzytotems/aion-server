#include "aion/gameserver/model/instance/DredgionRoom.h"

#include "aion/gameserver/model/Race.h"

namespace aion::gameserver::model::instance {

// Java DredgionRoom.java:14-16
DredgionRoom::DredgionRoom(int32_t roomId) : roomId(roomId) {
}

DredgionRoom::~DredgionRoom() = default;

runtime::Ref<DredgionRoom> DredgionRoom::create(int32_t roomId) {
	return runtime::makeRef<DredgionRoom>(roomId);
}

int32_t DredgionRoom::getRoomId() const {
	return roomId;
}

void DredgionRoom::captureRoom(Race race) {
	state = race == Race::ASMODIANS ? 0x01 : 0x00;
}

int32_t DredgionRoom::getState() const {
	return state;
}

} // namespace aion::gameserver::model::instance
