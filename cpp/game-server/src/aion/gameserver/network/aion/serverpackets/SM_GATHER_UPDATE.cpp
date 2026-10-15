#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_UPDATE.h"

#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_GATHER_UPDATE::SM_GATHER_UPDATE(const model::templates::gather::GatherableTemplate* template_, const model::templates::gather::Material* material,
	int32_t successValue, int32_t failureValue, int32_t actionValue, int32_t executionSpeedValue, int32_t delayValue)
	: AionServerPacket(opcodeOf<SM_GATHER_UPDATE>), skillId(template_->getHarvestSkill()), action(actionValue), itemId(material->getItemId()),
	  success(successValue), failure(failureValue), l10n(material->getL10n()), executionSpeed(executionSpeedValue), delay(delayValue) {
}

void SM_GATHER_UPDATE::writeImpl(AionConnection* con) {
	writeH(skillId);
	writeC(action);
	writeD(itemId);
	writeD(success);
	writeD(failure);
	writeD(executionSpeed);
	writeD(delay);
	switch (action) {
		case 0: // init
			writeSystemMsgInfo(SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHER_START_1_BASIC("").getId()); // Java: (null)
			break;
		case 1: // For updates both for ground and aerial
			writeSystemMsgInfo(0);
			break;
		case 2: // Light blue bar = +10%
			writeSystemMsgInfo(0);
			break;
		case 3: // Purple bar = 100%
			writeSystemMsgInfo(0);
			break;
		case 5: // canceled
			writeSystemMsgInfo(SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHER_CANCEL_1_BASIC().getId());
			break;
		case 6: // success
			writeSystemMsgInfo(SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHER_SUCCESS_1_BASIC("").getId());
			break;
		case 7: // failure
			writeSystemMsgInfo(SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHER_FAIL_1_BASIC("").getId());
			break;
		case 8: // deselects target
			writeSystemMsgInfo(SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHER_OCCUPIED_BY_OTHER().getId());
			break;
	}
}

void SM_GATHER_UPDATE::writeSystemMsgInfo(int32_t msgId) {
	writeD(msgId); // msgId
	writeS(msgId == 0 ? std::string_view() : std::string_view(l10n)); // parameter (Java null: "")
}

} // namespace aion::gameserver::network::aion::serverpackets
