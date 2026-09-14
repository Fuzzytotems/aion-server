#include "aion/gameserver/network/aion/serverpackets/SM_MACRO_LIST.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

// callback key com.aionemu.gameserver.network.aion.serverpackets.SM_MACRO_LIST@L18:90 (captureless)
const runtime::PinnedCallback<int32_t(model::gameobjects::player::Macros::Macro&)> SM_MACRO_LIST::DYNAMIC_BODY_PART_SIZE_CALCULATOR(
	[](model::gameobjects::player::Macros::Macro& macro) -> int32_t {
		AION_UNPORTED(); // 1 + macro.xml().length() * 2 + 2 (Java String.length: UTF-16 units)
	});

SM_MACRO_LIST::SM_MACRO_LIST(int32_t playerObjectIdValue, const std::vector<runtime::Ptr<model::gameobjects::player::Macros::Macro>>& macrosValue,
	bool clearListValue)
	: AionServerPacket(opcodeOf<SM_MACRO_LIST>), playerObjectId(playerObjectIdValue), macros(macrosValue.begin(), macrosValue.end()),
	  clearList(clearListValue) {
}

SM_MACRO_LIST::~SM_MACRO_LIST() = default;

void SM_MACRO_LIST::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
