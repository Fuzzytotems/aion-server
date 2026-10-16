#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/instance/playerreward/fwd.h"

namespace aion::gameserver::model::instance::playerreward {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5), written for the member-type guards waiting for it at the spine freeze.
 *
 * @author xTz
 */
class InstancePlayerReward : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> points{};
	runtime::Field<int32_t> playerPvPKills{};
	runtime::Field<int32_t> playerMonsterKills{};
	const int32_t objectId;

protected:
	explicit InstancePlayerReward(int32_t objectId);

public:
	static runtime::Ref<InstancePlayerReward> create(int32_t value);

	int32_t getOwnerId() const { return this->objectId; }

	int32_t getPoints() const { return this->points.get(); }

	int32_t getPvPKills() const { return this->playerPvPKills.get(); }

	int32_t getMonsterKills() const { return this->playerMonsterKills.get(); }

	void addPoints(int32_t points);

	void setPoints(int32_t value) { this->points.set(value); }

	void addPvPKill();

	void addMonsterKillToPlayer();

protected:
	~InstancePlayerReward() override;
};

} // namespace aion::gameserver::model::instance::playerreward
