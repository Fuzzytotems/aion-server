#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Result of an attacker critical status check, or the remaining count of an AttackerCriticalStatusObserver.
 * <p>
 * RefCounted (fieldmap K4: the member of AttackerCriticalStatusObserver and the return value of ObserveController::checkAttackerCriticalStatus),
 * created with create(). The member `isPercent` carries a trailing underscore (the class has a method of that name).
 *
 * @author kecimis
 */
class AttackerCriticalStatus : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> result{false};
	runtime::Field<int32_t> count{};
	const int32_t value;
	const bool isPercent_;

protected:
	explicit AttackerCriticalStatus(bool result);
	AttackerCriticalStatus(int32_t count, int32_t value, bool isPercent);
	~AttackerCriticalStatus() override;

public:
	/** Java: new AttackerCriticalStatus(result) */
	static runtime::Ref<AttackerCriticalStatus> create(bool result);

	/** Java: new AttackerCriticalStatus(count, value, isPercent) */
	static runtime::Ref<AttackerCriticalStatus> create(int32_t count, int32_t value, bool isPercent);

	/**
	 * @return the count
	 */
	int32_t getCount() const { return count.get(); }

	/**
	 * @param count the count to set
	 */
	void setCount(int32_t newCount) { count.set(newCount); }

	/**
	 * @return the value
	 */
	int32_t getValue() const { return value; }

	/**
	 * @return the isPercent
	 */
	bool isPercent() const { return isPercent_; }

	/**
	 * @return the result
	 */
	bool isResult() const { return result.get(); }

	/**
	 * @param result the result to set
	 */
	void setResult(bool newResult) { result.set(newResult); }
};

} // namespace aion::gameserver::controllers::observer
