#pragma once

#include <cstdint>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::playerreward {

/**
 * A player's reward in the PvP instances (dredgions, Kamar, Engulfed Ophidan Bridge, Iron Wall Warfront, Idgel Dome): race, captured zones,
 * the AP/GP and the item rewards the score writer shows. The reward arrays are null until set (Java int[] fields).
 *
 * @author Estrayl
 */
class PvpInstancePlayerReward : public InstancePlayerReward {
	AION_MAKE_REF_FRIEND
private:
	const Race race;
	runtime::AtomicInteger capturedZones{AION_LOCK_CLASS(PvpInstancePlayerReward::capturedZones)};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> reward1{};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> reward2{};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> reward3{};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> reward4{};
	runtime::Field<runtime::Ref<runtime::Array<int32_t>>> bonusReward{};
	runtime::Field<int32_t> baseAp{};
	runtime::Field<int32_t> bonusAp{};
	runtime::Field<int32_t> baseGp{};
	runtime::Field<int32_t> bonusGp{};

	/** Java: array == null ? 0 : array[index] */
	static int32_t valueOf(const runtime::Field<runtime::Ref<runtime::Array<int32_t>>>& array, int32_t index);

protected:
	PvpInstancePlayerReward(int32_t objectId, Race race);
	~PvpInstancePlayerReward() override;

public:
	static runtime::Ref<PvpInstancePlayerReward> create(int32_t objectId, Race race);

	int32_t getReward1ItemId() const { return valueOf(reward1, 0); }

	int32_t getReward1Count() const { return valueOf(reward1, 1); }

	int32_t getReward1BonusCount() const { return valueOf(reward1, 2); }

	void setReward1(int32_t itemId, int32_t itemCount, int32_t bonusCount);

	int32_t getReward2ItemId() const { return valueOf(reward2, 0); }

	int32_t getReward2Count() const { return valueOf(reward2, 1); }

	int32_t getReward2BonusCount() const { return valueOf(reward2, 2); }

	void setReward2(int32_t itemId, int32_t itemCount, int32_t bonusCount);

	int32_t getReward3ItemId() const { return valueOf(reward3, 0); }

	int32_t getReward3Count() const { return valueOf(reward3, 1); }

	void setReward3(int32_t itemId, int32_t itemCount);

	int32_t getReward4ItemId() const { return valueOf(reward4, 0); }

	int32_t getReward4Count() const { return valueOf(reward4, 1); }

	void setReward4(int32_t itemId, int32_t itemCount);

	int32_t getBonusRewardItemId() const { return valueOf(bonusReward, 0); }

	int32_t getBonusRewardCount() const { return valueOf(bonusReward, 1); }

	void setBonusReward(int32_t itemId, int32_t itemCount);

	int32_t getBaseAp() const { return baseAp.get(); }

	void setBaseAp(int32_t value) { baseAp.set(value); }

	int32_t getBonusAp() const { return bonusAp.get(); }

	void setBonusAp(int32_t value) { bonusAp.set(value); }

	int32_t getBaseGp() const { return baseGp.get(); }

	void setBaseGp(int32_t value) { baseGp.set(value); }

	int32_t getBonusGp() const { return bonusGp.get(); }

	void setBonusGp(int32_t value) { bonusGp.set(value); }

	void incrementCapturedZones() { capturedZones.incrementAndGet(); }

	int32_t getCapturedZones() const { return capturedZones.get(); }

	Race getRace() const { return race; }

	int32_t getMythicKunaxEquipment(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::instance::playerreward
