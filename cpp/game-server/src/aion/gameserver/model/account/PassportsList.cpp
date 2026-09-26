#include "aion/gameserver/model/account/PassportsList.h"

#include <chrono>

#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/model/account/Passport.h"

namespace aion::gameserver::model::account {

namespace {

/** Java ServerTime.atDate(date).toLocalDate(): the calendar date of the instant in GSConfig.TIME_ZONE_ID (P4-05 has no ServerTime yet) */
commons::database::Date serverDateOf(commons::database::Timestamp timestamp) {
	const std::chrono::time_zone* zone = configs::main::GSConfig::TIME_ZONE_ID.load();
	if (zone == nullptr)
		zone = std::chrono::current_zone();
	std::chrono::local_time<std::chrono::milliseconds> local = zone->to_local(timestamp);
	return commons::database::Date(std::chrono::floor<std::chrono::days>(local));
}

} // namespace

PassportsList::PassportsList() = default;

PassportsList::~PassportsList() = default;

runtime::Ref<PassportsList> PassportsList::create() {
	return runtime::makeRef<PassportsList>();
}

void PassportsList::addPassport(Passport& passport) {
	passports.add(runtime::Ref<Passport>(passport));
}

void PassportsList::removePassport(Passport& passport) {
	passports.remove(runtime::Ptr<Passport>(passport));
}

runtime::Ptr<Passport> PassportsList::getPassport(int32_t passportId, int32_t timestamp) {
	for (const runtime::Ptr<Passport>& passport : passports) {
		// Java: passport.getArriveDate().getTime() / 1000 == timestamp (NullPointerException for a null arrive date)
		if (passport->getId() == passportId && passport->getArriveDate().value().time_since_epoch().count() / 1000 == timestamp)
			return passport;
	}
	return nullptr;
}

bool PassportsList::isPassportPresent(int32_t passportId) {
	for (const runtime::Ptr<Passport>& pp : passports) {
		if (pp->getId() == passportId)
			return true;
	}
	return false;
}

bool PassportsList::hasPassportForDay(int32_t passportId, commons::database::Date attendDay) {
	for (const runtime::Ptr<Passport>& pp : passports) {
		if (pp->getId() == passportId && serverDateOf(pp->getArriveDate().value()) == attendDay)
			return true;
	}
	return false;
}

} // namespace aion::gameserver::model::account
