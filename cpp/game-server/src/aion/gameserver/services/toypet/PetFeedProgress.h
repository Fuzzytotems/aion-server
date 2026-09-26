#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/toypet/PetHungryLevel.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Rolandas
 */
class PetFeedProgress final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> totalPoints{0};
	runtime::Field<int16_t> regularConsumed{0};
	runtime::Field<int16_t> lovedConsumed{0};
	runtime::Field<PetHungryLevel> hungryLevel{PetHungryLevel::HUNGRY};
	const int16_t lovedFoodMax;
	runtime::Field<bool> lovedFeeded{false};

protected:
	explicit PetFeedProgress(int16_t lovedFoodLimit);

public:
	static runtime::Ref<PetFeedProgress> create(int16_t lovedFoodLimit);

	int32_t getTotalPoints() const { return this->totalPoints.get(); }

	void setTotalPoints(int32_t points);

	PetHungryLevel getHungryLevel() const { return this->hungryLevel.get(); }

	void setHungryLevel(PetHungryLevel level) { this->hungryLevel.set(level); }

	int32_t getRegularCount();

	void setRegularCount(int16_t value) { this->regularConsumed.set(value); }

	int32_t getLovedFoodRemaining();

	bool isLovedFeeded() const { return this->lovedFeeded.get(); }

	void setIsLovedFeeded();

	void incrementCount(bool lovedFood);

	void reset();

	int32_t getDataForPacket();

	void setData(int32_t savedData);

protected:
	~PetFeedProgress() override;
};

} // namespace aion::gameserver::services::toypet
