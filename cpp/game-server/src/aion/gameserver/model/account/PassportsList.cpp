#include "aion/gameserver/model/account/PassportsList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/account/Passport.h"

namespace aion::gameserver::model::account {

PassportsList::PassportsList() = default;

PassportsList::~PassportsList() = default;

runtime::Ref<PassportsList> PassportsList::create() {
	return runtime::makeRef<PassportsList>();
}

void PassportsList::addPassport(Passport& passport) {
	AION_UNPORTED();
}

void PassportsList::removePassport(Passport& passport) {
	AION_UNPORTED();
}

runtime::Ptr<Passport> PassportsList::getPassport(int32_t passportId, int32_t timestamp) {
	AION_UNPORTED();
}

bool PassportsList::isPassportPresent(int32_t passportId) {
	AION_UNPORTED();
}

bool PassportsList::hasPassportForDay(int32_t passportId, commons::database::Date attendDay) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::account
