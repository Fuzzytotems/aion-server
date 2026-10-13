#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/fwd.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/model/instance/playerreward/InstancePlayerReward.h"
#include "aion/gameserver/model/templates/rewards/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::model::instance::playerreward {

/**
 * A player's state in the PvP arenas: points, time bonus, position, the boost morale buff and the reward items.
 * <p>
 * C++: playerClass and playerName are null in Java for the objectId-only constructor (HarmonyGroupReward's group entry); here
 * std::optional and an empty name (getPlayerClass() of such a reward is Java's NullPointerException at its readers).
 *
 * @author xTz
 */
class PvPArenaPlayerReward : public InstancePlayerReward {
	AION_MAKE_REF_FRIEND
private:
	const std::optional<PlayerClass> playerClass; // fieldmap: Java passes null from the objectId-only constructor (HarmonyGroupReward); a nullable enum (hub-headers.md §6)
	const std::string playerName;
	runtime::Field<int32_t> position{};
	runtime::Field<int32_t> timeBonus{};
	const float timeBonusModifier;
	// Default values for training arenas
	runtime::Field<runtime::Ref<templates::rewards::ArenaRewardItem>> ap;
	runtime::Field<runtime::Ref<templates::rewards::ArenaRewardItem>> gp;
	runtime::Field<runtime::Ref<templates::rewards::ArenaRewardItem>> crucibleInsignia;
	runtime::Field<runtime::Ref<templates::rewards::ArenaRewardItem>> courageInsignia;
	runtime::Field<runtime::Ref<templates::rewards::RewardItem>> rewardItem1{};
	runtime::Field<runtime::Ref<templates::rewards::RewardItem>> rewardItem2{};
	runtime::Field<int64_t> logoutTime{};
	runtime::Field<bool> isRewarded_{false};
	const runtime::Ref<InstanceBuff> boostMorale;

protected:
	PvPArenaPlayerReward(gameobjects::player::Player& player, int32_t timeBonus, int8_t buffId);
	PvPArenaPlayerReward(int32_t objectId, int32_t timeBonus, int8_t buffId);
	~PvPArenaPlayerReward() override;

private:
	PvPArenaPlayerReward(int32_t objectId, std::optional<PlayerClass> playerClass, std::string_view playerName, int32_t timeBonus, int8_t buffId);

public:
	static runtime::Ref<PvPArenaPlayerReward> create(gameobjects::player::Player& player, int32_t timeBonus, int8_t buffId);

	static runtime::Ref<PvPArenaPlayerReward> create(int32_t objectId, int32_t timeBonus, int8_t buffId);

	using InstancePlayerReward::addPoints;

	void addPoints(int32_t points, instancescore::PvPArenaScore& instanceScore);

	/** @throws NullPointerException for the objectId-only reward (Java returns null; its readers dereference it) */
	PlayerClass getPlayerClass() const;

	const std::string& getPlayerName() const { return playerName; }

	int32_t getPosition() const { return position.get(); }

	void setPosition(int32_t value) { position.set(value); }

	int32_t getTimeBonus() const;

	void updateLogoutTime();

	void updateBonusTime();

	bool isRewarded() const { return isRewarded_.get(); }

	void setRewarded() { isRewarded_.set(true); }

	float getParticipation() const;

	runtime::Ptr<templates::rewards::ArenaRewardItem> getAp() const { return ap.get(); }

	void setAp(templates::rewards::ArenaRewardItem& value);

	runtime::Ptr<templates::rewards::ArenaRewardItem> getGp() const { return gp.get(); }

	void setGp(templates::rewards::ArenaRewardItem& value);

	runtime::Ptr<templates::rewards::ArenaRewardItem> getCrucibleInsignia() const { return crucibleInsignia.get(); }

	void setCrucibleInsignia(templates::rewards::ArenaRewardItem& value);

	runtime::Ptr<templates::rewards::ArenaRewardItem> getCourageInsignia() const { return courageInsignia.get(); }

	void setCourageInsignia(templates::rewards::ArenaRewardItem& value);

	/** null if not set */
	runtime::Ptr<templates::rewards::RewardItem> getRewardItem1() const { return rewardItem1.get(); }

	void setRewardItem1(runtime::Ptr<templates::rewards::RewardItem> value);

	/** null if not set */
	runtime::Ptr<templates::rewards::RewardItem> getRewardItem2() const { return rewardItem2.get(); }

	void setRewardItem2(runtime::Ptr<templates::rewards::RewardItem> value);

	int32_t getScorePoints() const;

	int32_t getRemainingTime() const;

	bool hasBoostMorale() const;

	void applyBoostMoraleEffect(gameobjects::player::Player& player, int32_t duration);

	void endBoostMoraleEffect(gameobjects::player::Player& player);
};

} // namespace aion::gameserver::model::instance::playerreward
