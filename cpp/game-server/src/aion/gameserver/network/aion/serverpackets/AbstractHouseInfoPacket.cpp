#include "aion/gameserver/network/aion/serverpackets/AbstractHouseInfoPacket.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionEmblem.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/housing/Building.h"
#include "aion/gameserver/model/templates/housing/BuildingTypeInfo.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/PartType.h"
#include "aion/gameserver/model/templates/housing/PartTypeInfo.h"
#include "aion/gameserver/network/aion/serverpackets/AbstractPlayerInfoPacket.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketSupport.h"

namespace aion::gameserver::network::aion::serverpackets {

AbstractHouseInfoPacket::AbstractHouseInfoPacket(int32_t opCode, model::house::House& houseValue) : AionServerPacket(opCode), house(houseValue) {
}

AbstractHouseInfoPacket::~AbstractHouseInfoPacket() = default;

void AbstractHouseInfoPacket::writeCommonInfo() {
	using model::templates::housing::PartType;
	runtime::Ptr<model::team::legion::LegionMember> member =
		house->isInactive() || house->getOwnerId() == 0 ? nullptr : detail::getLegionMember(house->getOwnerId());
	writeD(0);
	writeD(house->getAddress()->getId());
	writeD(house->getOwnerId());
	writeD(model::templates::housing::getId(detail::unbox(house->getBuilding()->getType(), "Building.getType()")));
	writeC(1); // unk
	writeD(house->getBuilding()->getId());
	writeC(house->getHouseOwnerStates());
	writeC(detail::houseDoorStateId(house->getDoorState()));
	writeS(house->getOwnerName().value_or(std::string()), AbstractPlayerInfoPacket::CHARNAME_MAX_LENGTH); // Java null: zero-padded like ""
	writeD(member == nullptr ? 0 : member->getLegion()->getLegionId());
	writeC(house->isShowOwnerName() ? 1 : 0);
	writeS(house->getSignNotice(), SIGN_NOTICE_MAX_LENGTH); // client can display much longer strings but then decor won't show
	for (size_t ordinal = 0; ordinal < xml::EnumTraits<PartType>::names.size(); ordinal++) { // Java PartType.values()
		const auto partType = static_cast<PartType>(ordinal);
		for (int32_t roomNo = 0; roomNo < model::templates::housing::getRooms(partType); roomNo++) {
			std::optional<int32_t> decorId = house->getRegistry()->getUsedDecorId(partType, roomNo);
			writeD(decorId.value_or(0));
		}
	}
	writeD(0);
	writeD(0);
	writeC(0); // show legion flags near house door: 0 = none, 1 = left, 2 = right (1+2 = both)
	// Emblem and color
	runtime::Ptr<model::team::legion::LegionEmblem> emblem = member == nullptr ? nullptr : member->getLegion()->getLegionEmblem();
	if (emblem == nullptr) {
		writeB(std::vector<uint8_t>(6));
	} else {
		writeC(emblem->getEmblemId());
		writeC(detail::legionEmblemTypeValue(emblem->getEmblemType()));
		writeC(emblem->getColor_a());
		writeC(emblem->getColor_r());
		writeC(emblem->getColor_g());
		writeC(emblem->getColor_b());
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
