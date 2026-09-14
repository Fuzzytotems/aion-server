#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.aion.serverpackets.SM_MESSAGE");

SM_MESSAGE::SM_MESSAGE(model::gameobjects::player::Player& sender, std::string_view messageValue, model::ChatType chatTypeValue)
	: AionServerPacket(opcodeOf<SM_MESSAGE>) {
	AION_UNPORTED();
}

SM_MESSAGE::SM_MESSAGE(model::gameobjects::Npc& sender, std::string_view messageValue, model::ChatType chatTypeValue)
	: AionServerPacket(opcodeOf<SM_MESSAGE>) {
	AION_UNPORTED();
}

SM_MESSAGE::SM_MESSAGE(int32_t senderObjectIdValue, std::string_view senderNameValue, std::string_view messageValue, model::ChatType chatTypeValue)
	: SM_MESSAGE(nullptr, senderObjectIdValue, senderNameValue, messageValue, chatTypeValue) {
}

SM_MESSAGE::SM_MESSAGE(runtime::Ptr<model::gameobjects::Creature> sender, int32_t senderObjectIdValue, std::string_view senderNameValue,
	std::string_view messageValue, model::ChatType chatTypeValue)
	: AionServerPacket(opcodeOf<SM_MESSAGE>) {
	AION_UNPORTED();
}

void SM_MESSAGE::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
