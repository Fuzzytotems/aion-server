#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/instance/instancescore/fwd.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::custom::instance {

/**
 * A reusable instance score (C++-dev only, not in Java; docs/deviations/C2.md, "C++-dev only: instance scoring"): a countdown on the
 * client's timer bar, points per kill weighted by the npc's kind, and at the end - the final boss killed or the time out - a rank of 1 to 8
 * in Dark Poeta's shape, a result line in chat and a reward per rank.
 * <p>
 * The instance handler owns it and wires it: start at the first entry, onKill from onDie, finish(timeout) from its own timer task. The
 * score holds no instance reference (the handler passes it), so it adds no cycle.
 * <p>
 * Thread-safety: the points are an AtomicInteger, the end is taken once (AtomicBoolean), the rest are Fields.
 */
class InstanceScoring : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	/** milliseconds since the epoch */
	using Clock = std::function<int64_t()>;

	/** one reward: item id and count */
	using Reward = std::pair<int32_t, int64_t>;

	struct Settings {
		/** the countdown in seconds (SM_QUEST_ACTION's time) */
		int32_t timeLimitSeconds = 1200;
		/** points of a boss (a HERO or LEGENDARY rating, or one of bossNpcIds), a named or elite (ELITE) and a normal npc */
		int32_t bossPoints = 200;
		int32_t elitePoints = 30;
		int32_t normalPoints = 10;
		/** npcs that count as bosses whatever their rating (an instance's named bosses) */
		std::vector<int32_t> bossNpcIds;
		/** npcs whose death ends the run */
		std::vector<int32_t> finalBossNpcIds;
		/**
		 * Dark Poeta's ranks 1-4 (DarkPoetaInstance.java:95-115): rank r needs more than timeFractions[r-1] of the time left and at least
		 * pointThresholds[r-1] points; the final boss in time is at least rank 5; the time out is rank 8
		 */
		std::array<double, 4> timeFractions{0.5, 0.375, 0.25, 0.125};
		std::array<int32_t, 4> pointThresholds{4000, 3200, 2400, 1600};
		/** rewards of ranks 1 to 8 (index rank - 1) */
		std::array<std::vector<Reward>, 8> rewards{};
		/** the map id SM_INSTANCE_SCORE carries when the experimental panel is on (Dark Poeta's, whose layout the writer is) */
		int32_t panelMapId = 300040000;
	};

	static runtime::Ref<InstanceScoring> create(Settings settings, Clock clock);

	/** starts the countdown once and shows it to the instance's players: @return true the first time */
	bool start(world::WorldMapInstance& instance);

	/** a killed npc's points, announced with STR_MSG_GET_SCORE; the final boss ends the run. @return the points (0 when not running) */
	int32_t onKill(world::WorldMapInstance& instance, model::gameobjects::Npc& npc);

	/** ends the run once: the rank, the timer cleared, the result line and the rewards to every player inside. @return the rank, 0 if ended */
	int32_t finish(world::WorldMapInstance& instance, bool finalBossKilled);

	/** the points of an npc by its kind */
	int32_t pointsOf(model::gameobjects::Npc& npc) const;

	/** Dark Poeta's rank shape over this score's settings */
	int32_t rankOf(int64_t timeLeftMillis, int32_t points, bool finalBossKilled) const;

	/** "[Fire Temple] Rank 3 - 2450 points, 12:34 left" */
	std::string resultLine(int32_t rank, int32_t points, int64_t timeLeftMillis) const;

	int64_t getTimeLeftMillis() const;
	int32_t getPoints() const { return points.get(); }
	int32_t getRank() const { return rank.get(); }
	bool isStarted() const { return startMillis.get() != 0; }
	bool isFinished() const { return finished.get(); }
	const Settings& getSettings() const { return settings; }

	/** the name the result line starts with */
	void setTitle(std::string value);

	/** gives one reward (ItemService::addItem with inventory overflow allowed) */
	using RewardGiver = void (*)(model::gameobjects::player::Player& player, int32_t itemId, int64_t count);

	/** tests: replaces the reward giver (nullptr restores ItemService::addItem) */
	static void setRewardGiverForTests(RewardGiver giver);

protected:
	InstanceScoring(Settings settings, Clock clock);
	~InstanceScoring() override;

private:
	void sendPanel(world::WorldMapInstance& instance);

	static void addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count);

	static inline runtime::Field<RewardGiver> rewardGiver{&InstanceScoring::addItem};

	const Settings settings;
	const Clock clock;
	runtime::Field<int64_t> startMillis{};
	runtime::AtomicInteger points{AION_LOCK_CLASS(InstanceScoring::points)};
	runtime::AtomicBoolean finished{AION_LOCK_CLASS(InstanceScoring::finished)};
	runtime::Field<int32_t> rank{};
	runtime::Field<std::string> title{};
	runtime::Field<int64_t> endMillis{};
	/** the experimental panel's counters (DarkPoetaScoreWriter's input) */
	const runtime::Ref<model::instance::instancescore::DarkPoetaScore> panelScore;
};

} // namespace aion::gameserver::custom::instance
