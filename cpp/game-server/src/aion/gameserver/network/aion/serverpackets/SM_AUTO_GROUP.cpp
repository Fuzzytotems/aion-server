#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"

#include <array>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/**
 * Java: AutoGroupType.getAGTByMaskId(maskId).getTemplate(): the template of the first AutoGroupType constant whose template has the mask id, null
 * if there is none. Each constant's template is DataManager.AUTO_GROUP.getTemplateByInstanceMaskId(instanceMaskId) of its constructor argument;
 * like Java's loop, a constant before the match without a template throws NullPointerException. C++: the instance mask ids of the 33 constants
 * in ordinal order stand in for the AutoGroupType companion (P5-10), whose getL10nId() is the template's.
 */
const model::autogroup::AutoGroup* autoGroupTemplateByMaskId(int32_t maskId) {
	static constexpr std::array<int32_t, 33> INSTANCE_MASK_IDS{
		1, 2, 3, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 38, 39, 40, 41, 42, 43, 44, 45, 101, 102, 103, 107, 108, 109, 111};
	const dataholders::AutoGroupData& autoGroupData = *dataholders::DataManager::AUTO_GROUP;
	for (int32_t instanceMaskId : INSTANCE_MASK_IDS) {
		const model::autogroup::AutoGroup* groupTemplate = autoGroupData.getTemplateByInstanceMaskId(instanceMaskId);
		if (groupTemplate == nullptr)
			throw runtime::NullPointerException("AutoGroupType template of instanceMaskId " + std::to_string(instanceMaskId) + " is null");
		if (groupTemplate->getMaskId() == maskId)
			return groupTemplate;
	}
	return nullptr;
}

} // namespace

SM_AUTO_GROUP::SM_AUTO_GROUP(int32_t maskIdValue) : AionServerPacket(opcodeOf<SM_AUTO_GROUP>) {
	const model::autogroup::AutoGroup* agt = autoGroupTemplateByMaskId(maskIdValue);
	if (agt == nullptr) {
		throw commons::utils::IllegalArgumentException("AutoGroupType not found for maskId: " + std::to_string(maskIdValue));
	}
	this->maskId = maskIdValue;
	this->messageId = agt->getL10nId();
	this->titleId = agt->getTitleId();
	this->mapId = agt->getInstanceMapId();
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
	writeD(maskId);
	writeC(windowId);
	writeD(mapId);
	switch (windowId) {
		case 0:
		case 7: // 0 = request entry, 7 = failed window
			writeD(messageId);
			writeD(titleId);
			writeD(0);
			break;
		case 1:
		case 3:
		case 8: // 1 = waiting window, 3 = pass window, 8 = on login
			writeD(0); // progression type: 0 = Group Formation in Progress, 1 = Opponent Group formation in progress
			writeD(0);
			writeD(requestTypeId);
			break;
		case 2:
		case 4:
		case 5: // 2 = cancel looking, 4 = enter window, 5 = after clicking enter
			writeD(0);
			writeD(0);
			writeD(0);
			break;
		case WND_ENTRY_ICON: // entry icon
			writeD(messageId);
			writeD(titleId);
			writeD(close ? 0 : 1);
			break;
	}
	writeC(0);
	writeS(name);
}

} // namespace aion::gameserver::network::aion::serverpackets
