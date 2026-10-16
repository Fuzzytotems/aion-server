#pragma once

#include <cstdint>

#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::playerreward {

/**
 * A player's reward state in the Empyrean Crucible instances.
 *
 * @author xTz
 */
class CruciblePlayerReward : public InstancePlayerReward {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<int32_t> spawnPosition{};
	runtime::Field<bool> isRewarded_{false};
	runtime::Field<int32_t> insignia{};
	runtime::Field<bool> isPlayerLeave_{false};
	runtime::Field<bool> isPlayerDefeated_{false};

protected:
	explicit CruciblePlayerReward(int32_t objectId);
	~CruciblePlayerReward() override;

public:
	static runtime::Ref<CruciblePlayerReward> create(int32_t objectId);

	bool isRewarded() const { return isRewarded_.get(); }

	void setRewarded() { isRewarded_.set(true); }

	void setInsignia(int32_t value) { insignia.set(value); }

	int32_t getInsignia() const { return insignia.get(); }

	void setSpawnPosition(int32_t value) { spawnPosition.set(value); }

	int32_t getSpawnPosition() const { return spawnPosition.get(); }

	bool isPlayerLeave() const { return isPlayerLeave_.get(); }

	void setPlayerLeave() { isPlayerLeave_.set(true); }

	void setPlayerDefeated(bool value) { isPlayerDefeated_.set(value); }

	bool isPlayerDefeated() const { return isPlayerDefeated_.get(); }
};

} // namespace aion::gameserver::model::instance::playerreward
