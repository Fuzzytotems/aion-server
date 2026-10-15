#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/event/fwd.h"

namespace aion::gameserver::model::event {

/**
 * Created on 28.05.2016
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Estrayl
 * @since AION 4.8
 */
class ArcadeProgress : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t playerObjId;
	runtime::Field<int32_t> frenzyPoints{};
	runtime::Field<int32_t> currentLevel{};
	runtime::Field<int64_t> frenzyEndTimeMillis{};
	runtime::Field<int32_t> resumeLevel{};
	runtime::Field<int64_t> nextTryTimeMillis{};

protected:
	explicit ArcadeProgress(int32_t playerObjId);

public:
	static runtime::Ref<ArcadeProgress> create(int32_t value);

	int32_t getPlayerObjId() const { return this->playerObjId; }

	int32_t getFrenzyPoints() const { return this->frenzyPoints.get(); }

	void setFrenzyPoints(int32_t value) { this->frenzyPoints.set(value); }

	int32_t getCurrentLevel() const { return this->currentLevel.get(); }

	void setCurrentLevel(int32_t value) { this->currentLevel.set(value); }

	int64_t getFrenzyEndTimeMillis() const { return this->frenzyEndTimeMillis.get(); }

	void setFrenzyEndTimeMillis(int64_t value) { this->frenzyEndTimeMillis.set(value); }

	int32_t getResumeLevel() const { return this->resumeLevel.get(); }

	void setResumeLevel(int32_t value) { this->resumeLevel.set(value); }

	int64_t getNextTryTimeMillis() const { return this->nextTryTimeMillis.get(); }

	void setTimeNextTry(int64_t value) { this->nextTryTimeMillis.set(value); }

protected:
	~ArcadeProgress() override;
};

} // namespace aion::gameserver::model::event
