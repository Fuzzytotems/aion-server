#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_AUTO_GROUP::SM_AUTO_GROUP(int32_t maskIdValue) : AionServerPacket(opcodeOf<SM_AUTO_GROUP>) {
	AION_UNPORTED();
}

SM_AUTO_GROUP::SM_AUTO_GROUP(int32_t maskIdValue, int32_t windowIdValue) : SM_AUTO_GROUP(maskIdValue) {
	this->windowId = windowIdValue;
}

SM_AUTO_GROUP::SM_AUTO_GROUP(int32_t maskIdValue, int32_t windowIdValue, bool closeValue) : SM_AUTO_GROUP(maskIdValue) {
	this->windowId = windowIdValue;
	this->close = closeValue;
}

SM_AUTO_GROUP::SM_AUTO_GROUP(int32_t maskIdValue, int32_t windowIdValue, int32_t requestTypeIdValue, std::string_view nameValue)
	: SM_AUTO_GROUP(maskIdValue) {
	this->windowId = windowIdValue;
	this->requestTypeId = requestTypeIdValue;
	this->name = nameValue;
}

void SM_AUTO_GROUP::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
