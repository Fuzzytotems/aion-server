#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/account/fwd.h"

namespace aion::gameserver::model::account {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Account.playerPassports`), created with create().
 *
 * @author ViAl, SVDNESS
 */
class PassportsList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::ArrayList<runtime::Ref<Passport>> passports{AION_LOCK_CLASS(PassportsList::passports)};

protected:
	PassportsList();
	~PassportsList() override;

public:
	/** Java: new PassportsList() */
	static runtime::Ref<PassportsList> create();

	void addPassport(Passport& passport);

	void removePassport(Passport& passport);

	/** @return the passport, null if there is none with that id and arrive time */
	runtime::Ptr<Passport> getPassport(int32_t passportId, int32_t timestamp);

	bool isPassportPresent(int32_t passportId);

	runtime::ArrayList<runtime::Ref<Passport>>& getAllPassports() { return passports; }

	bool hasPassportForDay(int32_t passportId, commons::database::Date attendDay);
};

} // namespace aion::gameserver::model::account
