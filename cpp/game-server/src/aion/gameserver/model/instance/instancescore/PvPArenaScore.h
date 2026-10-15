#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/instanceposition/fwd.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/world/WorldMapType.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::instance::instancescore {

/**
 * The PvP arenas' score (Java InstanceScore&lt;PvPArenaPlayerReward&gt;): the start positions, the zones, the rounds and the score caps.
 * <p>
 * C++: the generic base is erased (InstanceScore.h), so getPlayerReward and getPlayerRewards are redeclared here with the reward type
 * (Java's T); a reward of another type in the map is Java's ClassCastException. The synchronized methods lock the object's Monitor.
 *
 * @author xTz
 */
class PvPArenaScore : public InstanceScore {
	AION_MAKE_REF_FRIEND
private:
	struct Setup {
		int32_t bonusTime;
		int32_t positionSize;
		runtime::Ref<instanceposition::GeneralInstancePosition> instancePosition;
	};

	runtime::HashMap<int32_t, bool> positions{AION_LOCK_CLASS(PvPArenaScore::positions)};
	runtime::ArrayList<int32_t> zones{AION_LOCK_CLASS(PvPArenaScore::zones)};
	runtime::Field<int32_t> round{1};
	runtime::Field<std::optional<int32_t>> zone{};
	const int32_t bonusTime;
	runtime::Field<int32_t> difficultyId{};
	runtime::Field<int32_t> lowerScoreCap{};
	runtime::Field<int32_t> upperScoreCap{};
	runtime::Field<int32_t> maxScoreGap{};
	runtime::Field<int64_t> instanceTime{};
	const int8_t buffId;

protected:
	runtime::Field<runtime::Ref<world::WorldMapInstance>> instance{};

private:
	const runtime::Ref<instanceposition::GeneralInstancePosition> instancePosition;

	PvPArenaScore(world::WorldMapInstance& instance, const Setup& setup);

	/** Java: the constructor's switch over the map type (bonus time, position count, position table) */
	static Setup setupOf(std::optional<world::WorldMapType> wmt);

protected:
	explicit PvPArenaScore(world::WorldMapInstance& instance);
	~PvPArenaScore() override;

public:
	static runtime::Ref<PvPArenaScore> create(world::WorldMapInstance& instance);

	/** Java: InstanceScore&lt;PvPArenaPlayerReward&gt;.getPlayerReward */
	runtime::Ptr<playerreward::PvPArenaPlayerReward> getPlayerReward(int32_t objectId);

	/** Java: InstanceScore&lt;PvPArenaPlayerReward&gt;.getPlayerRewards */
	std::vector<runtime::Ptr<playerreward::PvPArenaPlayerReward>> getPlayerRewards();

	/** Java: final */
	bool isSoloArena(std::optional<world::WorldMapType> wmt) const;

	/** Java: final */
	void setRndZone();

private:
	std::vector<int32_t> getFreePositions();

public:
	void setRndPosition(int32_t objectId); // synchronized

	void clearPosition(int32_t position, bool result); // synchronized

	int32_t getRound() const { return round.get(); }

	void incrementRound();

	bool regPlayerReward(gameobjects::player::Player& player);

	void portToPosition(gameobjects::player::Player& player);

	virtual int32_t getRank(playerreward::PvPArenaPlayerReward& reward);

	bool reachedScoreGap();

	virtual int32_t getTotalPoints();

	bool canReward();

	int32_t getNpcBonusSkill(int32_t npcId) const;

	int32_t getTime();

	int32_t getDifficultyId() const { return difficultyId.get(); }

	void setDifficultyId(int32_t value) { difficultyId.set(value); }

	int32_t getLowerScoreCap() const { return lowerScoreCap.get(); }

	void setLowerScoreCap(int32_t value) { lowerScoreCap.set(value); }

	int32_t getUpperScoreCap() const { return upperScoreCap.get(); }

	void setUpperScoreCap(int32_t value) { upperScoreCap.set(value); }

	void setMaxScoreGap(int32_t value) { maxScoreGap.set(value); }

	void setInstanceStartTime();

	int8_t getBuffId() const { return buffId; }

	void clear() override;
};

} // namespace aion::gameserver::model::instance::instancescore
