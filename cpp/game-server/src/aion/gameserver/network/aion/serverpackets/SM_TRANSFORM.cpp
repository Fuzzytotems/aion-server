#include "aion/gameserver/network/aion/serverpackets/SM_TRANSFORM.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/TransformModel.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_TRANSFORM::SM_TRANSFORM(model::gameobjects::Creature& creatureValue)
	: AionServerPacket(opcodeOf<SM_TRANSFORM>), creature(creatureValue) {
}

SM_TRANSFORM::~SM_TRANSFORM() = default;

void SM_TRANSFORM::writeImpl(AionConnection* con) {
	writeD(creature->getObjectId());
	writeD(creature->getTransformModel().getModelId());
	writeH(creature->getState());
	writeF(0.25f);
	writeF(2.0f);
	writeC(creature->getTransformModel().cantUseSkills() ? 1 : 0);
	writeD(detail::transformTypeId(creature->getTransformModel().getType()));
	writeC(creature->getTransformModel().cantFly() ? 1 : 0);
	writeC(creature->getTransformModel().cantUseItems() ? 1 : 0);
	writeC(creature->getTransformModel().cantAttack() ? 1 : 0);
	writeC(creature->getTransformModel().cantJump() ? 1 : 0);
	writeC(creature->getTransformModel().cantRecall() ? 1 : 0);
	writeC(creature->getTransformModel().cantMove() ? 1 : 0);
	writeD(creature->getTransformModel().getPanelId()); // display panel
}

} // namespace aion::gameserver::network::aion::serverpackets
