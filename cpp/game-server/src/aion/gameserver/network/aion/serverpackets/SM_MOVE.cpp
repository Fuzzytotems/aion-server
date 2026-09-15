#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"

#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/controllers/movement/GlideFlag.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/PlayableMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_MOVE::SM_MOVE(model::gameobjects::Creature& creatureValue)
	: SM_MOVE(creatureValue, creatureValue.getMoveController()->getMovementMask()) {
}

SM_MOVE::SM_MOVE(model::gameobjects::Creature& creatureValue, int8_t movementMaskValue)
	: AionServerPacket(opcodeOf<SM_MOVE>), creature(creatureValue), movementMask(movementMaskValue) {
}

SM_MOVE::~SM_MOVE() = default;

void SM_MOVE::writeImpl(AionConnection* client) {
	using controllers::movement::GlideFlag;
	using controllers::movement::MovementMask;
	runtime::Ptr<controllers::movement::CreatureMoveController> mc = creature->getMoveController();
	runtime::Ptr<controllers::movement::PlayableMoveController> pmc = runtime::as<controllers::movement::PlayableMoveController>(mc);
	writeD(creature->getObjectId());
	writeF(creature->getX());
	writeF(creature->getY());
	writeF(creature->getZ());
	writeC(creature->getHeading());
	writeC(movementMask);
	if ((movementMask & MovementMask::POSITION) == MovementMask::POSITION && (movementMask & MovementMask::MANUAL) == MovementMask::MANUAL) {
		if (pmc != nullptr && (movementMask & MovementMask::ABSOLUTE) == 0) {
			writeF(pmc->vectorX.get());
			writeF(pmc->vectorY.get());
			writeF(pmc->vectorZ.get());
		} else {
			writeF(mc->getTargetX2());
			writeF(mc->getTargetY2());
			writeF(mc->getTargetZ2());
		}
	}
	if ((movementMask & MovementMask::GLIDE) == MovementMask::GLIDE) {
		int8_t glideFlag = pmc == nullptr ? int8_t{0} : pmc->glideFlag.get();
		writeC(glideFlag);
		if (glideFlag == GlideFlag::GEYSER)
			writeC(pmc->geyserLocationId.get());
	}
	if (pmc != nullptr && (movementMask & MovementMask::VEHICLE) == MovementMask::VEHICLE) {
		writeD(pmc->unk1.get());
		writeD(pmc->unk2.get());
		writeF(pmc->vectorX.get());
		writeF(pmc->vectorY.get());
		writeF(pmc->vectorZ.get());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
