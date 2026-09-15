#include "aion/gameserver/network/aion/serverpackets/SM_ATREIAN_PASSPORT.h"

#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/Passport_RewardStatusInfo.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_ATREIAN_PASSPORT::SM_ATREIAN_PASSPORT(model::account::PassportsList& passportsValue, int32_t stampsValue,
	commons::database::Date accountCreationDateValue)
	: AionServerPacket(opcodeOf<SM_ATREIAN_PASSPORT>), accountCreationDate(accountCreationDateValue), passports(passportsValue), stamps(stampsValue) {
}

SM_ATREIAN_PASSPORT::~SM_ATREIAN_PASSPORT() = default;

void SM_ATREIAN_PASSPORT::writeImpl(AionConnection* con) {
	writeH(static_cast<int32_t>(accountCreationDate.year()));
	writeH(static_cast<int32_t>(static_cast<unsigned>(accountCreationDate.month())));
	writeH(static_cast<int32_t>(static_cast<unsigned>(accountCreationDate.day())));
	std::vector<runtime::Ptr<model::account::Passport>> allPassports = passports->getAllPassports().snapshot();
	writeH(static_cast<int32_t>(allPassports.size()));
	for (runtime::Ptr<model::account::Passport> pp : allPassports) {
		writeD(pp->getId());
		writeD(stamps); // wrong, this is the stamp count when each passport was received (current month sends current count for upcoming rewards)
		// 0 = not yet arrived (upcoming this months rewards), 1 = arrived and not taken, 2 = arrived and taken, 3 = not arrived (last months rewards)
		writeD(model::account::getId(pp->getRewardStatus()));
		std::optional<commons::database::Timestamp> arriveDate = pp->getArriveDate();
		if (!arriveDate)
			throw runtime::NullPointerException("Passport arriveDate is null");
		writeD(static_cast<int32_t>(arriveDate->time_since_epoch().count() / 1000)); // for upcoming rewards it's the first login time each day
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
