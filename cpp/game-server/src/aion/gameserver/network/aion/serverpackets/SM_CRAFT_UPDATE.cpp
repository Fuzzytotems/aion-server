#include "aion/gameserver/network/aion/serverpackets/SM_CRAFT_UPDATE.h"

#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_CRAFT_UPDATE::SM_CRAFT_UPDATE(int32_t skillIdValue, const model::templates::item::ItemTemplate* item, int32_t successValue, int32_t failureValue,
	int32_t actionValue, int32_t executionSpeedValue, int32_t delayValue)
	: AionServerPacket(opcodeOf<SM_CRAFT_UPDATE>), skillId(skillIdValue), itemId(item->getTemplateId()), action(actionValue), success(successValue),
	  failure(failureValue), itemNameL10n(item->getL10n()), executionSpeed(executionSpeedValue), delay(skillIdValue == 40009 ? 1000 : delayValue) {
}

void SM_CRAFT_UPDATE::writeImpl(AionConnection* con) {
	writeH(skillId);
	writeC(action);
	writeD(itemId);
	writeD(success); // max
	writeD(failure); // max
	writeD(executionSpeed);
	writeD(delay); // delay
	switch (action) {
		case 0: // init
		case 3: // crit = proc
			writeD(1330048); // msgId
			writeS(itemNameL10n); // param
			break;
		case 1: // update (normal)
		case 2: // crit (blue) = +10%
			writeD(0);
			writeS(""); // Java: null
			break;
		case 4: // cancelled
			writeD(1330051);
			writeS(""); // Java: null
			break;
		case 5: // success (end)
			writeD(1330049);
			writeS(itemNameL10n); // param
			break;
		case 6: // failed (end)
		case 7: // failure (never used?)
			writeD(1330050);
			writeS(itemNameL10n); // param
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
