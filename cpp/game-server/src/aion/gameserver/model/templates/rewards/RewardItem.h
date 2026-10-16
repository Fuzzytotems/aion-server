#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"

namespace aion::gameserver::model::templates::rewards {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author Neon
 */
class RewardItem : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id;
	const int64_t count;

protected:
	RewardItem(int32_t id, int64_t count);

public:
	static runtime::Ref<RewardItem> create(int32_t value, int64_t countValue);

	int32_t getId() const { return this->id; }

	int64_t getCount() const { return this->count; }

	std::string toString();

protected:
	~RewardItem() override;
};

} // namespace aion::gameserver::model::templates::rewards
