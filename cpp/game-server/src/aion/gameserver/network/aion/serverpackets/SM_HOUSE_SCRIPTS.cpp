#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/model/house/PlayerScript.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: the lambda of DYNAMIC_BODY_PART_SIZE_CALCULATOR (SM_HOUSE_SCRIPTS.java:19, key SM_HOUSE_SCRIPTS@L19:90) */
struct DynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::house::PlayerScript& script) const {
		AION_UNPORTED();
	}
};

} // namespace

const runtime::PinnedCallback<int32_t(model::house::PlayerScript&)> SM_HOUSE_SCRIPTS::DYNAMIC_BODY_PART_SIZE_CALCULATOR{
	DynamicBodyPartSizeCalculator{}};

SM_HOUSE_SCRIPTS::SM_HOUSE_SCRIPTS(int32_t houseAddressValue, runtime::Ptr<model::house::PlayerScript> script)
	: AionServerPacket(opcodeOf<SM_HOUSE_SCRIPTS>), houseAddress(houseAddressValue) {
	// Java: script == null ? Collections.emptyList() : Collections.singletonList(script)
	if (script)
		scripts.emplace_back(script);
}

SM_HOUSE_SCRIPTS::SM_HOUSE_SCRIPTS(int32_t houseAddressValue, const std::vector<runtime::Ptr<model::house::PlayerScript>>& scriptsValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_SCRIPTS>), houseAddress(houseAddressValue), scripts(scriptsValue.begin(), scriptsValue.end()) {
}

SM_HOUSE_SCRIPTS::~SM_HOUSE_SCRIPTS() = default;

void SM_HOUSE_SCRIPTS::writeImpl(AionConnection* con) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::network::aion::serverpackets
