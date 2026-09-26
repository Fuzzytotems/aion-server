#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/account/fwd.h"

namespace aion::gameserver::model::account {

/**
 * Class for storing account's online and rest time
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Account.accountTime`), created with create().
 *
 * @author EvilSpirit
 */
class AccountTime : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	/** Accumulated online time in millis */
	runtime::Field<int64_t> accumulatedOnlineTime{};
	/** Accumulated rest(offline) time in millis */
	runtime::Field<int64_t> accumulatedRestTime{};

protected:
	AccountTime();
	~AccountTime() override;

public:
	/** Java: new AccountTime() */
	static runtime::Ref<AccountTime> create();

	/** get daily accumulated online time in millis */
	int64_t getAccumulatedOnlineTime() const { return accumulatedOnlineTime.get(); }

	/** get daily accumulated online time in millis */
	void setAccumulatedOnlineTime(int64_t value) { accumulatedOnlineTime.set(value); }

	/** get daily accumulated rest (offline) time since first login */
	int64_t getAccumulatedRestTime() const { return accumulatedRestTime.get(); }

	/** get daily accumulated rest (offline) time since first login */
	void setAccumulatedRestTime(int64_t value) { accumulatedRestTime.set(value); }

	/** Returns hour part rounded down. For instance if time is 1 hr 32 min - it will return 1 hr */
	int32_t getAccumulatedOnlineHours();

	/** Returns minutes part. For instance: if time is 1 hr 32 min - it will return 32 min */
	int32_t getAccumulatedOnlineMinutes();

	/** Returns hour part rounded down. For instance if time is 1 hr 32 min - it will return 1 hr */
	int32_t getAccumulatedRestHours();

	/** Returns minutes part. For instance: if time is 1 hr 32 min - it will return 32 min */
	int32_t getAccumulatedRestMinutes();

private:
	/** Converts milliseconds to hours. For instance if millis = 1 hr 32 min, 1 hour will be returned */
	static int32_t toHours(int64_t millis);

	/** Converts milliseconds to minutes. For instance if millis = 1 hr 32 min, 32 min will be returned */
	static int32_t toMinutes(int64_t millis);
};

} // namespace aion::gameserver::model::account
