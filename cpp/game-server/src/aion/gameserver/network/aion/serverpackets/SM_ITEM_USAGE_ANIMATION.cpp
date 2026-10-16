#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"

#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ITEM_USAGE_ANIMATION::SM_ITEM_USAGE_ANIMATION(int32_t playerObjIdValue, int32_t itemObjIdValue, int32_t itemIdValue)
	: AionServerPacket(opcodeOf<SM_ITEM_USAGE_ANIMATION>), playerObjId(playerObjIdValue), targetObjId(playerObjIdValue), itemObjId(itemObjIdValue),
	  itemId(itemIdValue), time(0), end(1), unk3(1) {
}

SM_ITEM_USAGE_ANIMATION::SM_ITEM_USAGE_ANIMATION(int32_t playerObjIdValue, int32_t itemObjIdValue, int32_t itemIdValue, int32_t timeValue,
	int32_t endValue)
	: AionServerPacket(opcodeOf<SM_ITEM_USAGE_ANIMATION>), playerObjId(playerObjIdValue), targetObjId(playerObjIdValue), itemObjId(itemObjIdValue),
	  itemId(itemIdValue), time(timeValue), end(endValue) {
}

SM_ITEM_USAGE_ANIMATION::SM_ITEM_USAGE_ANIMATION(int32_t playerObjIdValue, int32_t itemObjIdValue, int32_t itemIdValue, int32_t timeValue,
	int32_t endValue, int32_t unkValue)
	: AionServerPacket(opcodeOf<SM_ITEM_USAGE_ANIMATION>), playerObjId(playerObjIdValue), targetObjId(playerObjIdValue), itemObjId(itemObjIdValue),
	  itemId(itemIdValue), time(timeValue), end(endValue), unk3(unkValue) {
}

SM_ITEM_USAGE_ANIMATION::SM_ITEM_USAGE_ANIMATION(int32_t playerObjIdValue, int32_t targetObjIdValue, int32_t itemObjIdValue, int32_t itemIdValue,
	int32_t timeValue, int32_t endValue, int32_t unkValue)
	: AionServerPacket(opcodeOf<SM_ITEM_USAGE_ANIMATION>), playerObjId(playerObjIdValue), targetObjId(targetObjIdValue), itemObjId(itemObjIdValue),
	  itemId(itemIdValue), time(timeValue), end(endValue), unk3(unkValue) {
}

SM_ITEM_USAGE_ANIMATION::SM_ITEM_USAGE_ANIMATION(int32_t playerObjIdValue, int32_t targetObjIdValue, int32_t itemObjIdValue, int32_t itemIdValue,
	int32_t timeValue, int32_t endValue, int32_t unkValue, int32_t unk1Value, int32_t unk2Value, int32_t unk3Value)
	: AionServerPacket(opcodeOf<SM_ITEM_USAGE_ANIMATION>), playerObjId(playerObjIdValue), targetObjId(targetObjIdValue), itemObjId(itemObjIdValue),
	  itemId(itemIdValue), time(timeValue), end(endValue), unk(unkValue), unk1(unk1Value), unk2(unk2Value), unk3(unk3Value) {
}

void SM_ITEM_USAGE_ANIMATION::writeImpl(AionConnection* con) {
	writeD(playerObjId); // player obj id
	writeD(targetObjId); // target obj id
	writeD(itemObjId); // itemObjId
	writeD(itemId); // item id
	writeD(time); // unk
	writeC(end); // unk
	writeC(unk); // unk
	writeC(unk1);
	writeC(unk2); // unk
	writeD(unk3); // mb cd?
}

} // namespace aion::gameserver::network::aion::serverpackets
