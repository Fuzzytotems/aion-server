#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/custom/instance/CustomInstanceRank.h"
#include "aion/gameserver/custom/instance/fwd.h"
#include "aion/gameserver/custom/instance/neuralnetwork/fwd.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::custom::instance {

/**
 * The Eternal Challenge (Roah chamber) custom instance: entry cooldown, ranks, the leaderboard and the recorded player model entries of the
 * neural network.
 * <p>
 * Declaration header (m5a-plan.md W-03): an Immortal singleton (hub-headers.md §11.2) that GameServer creates at startup; the constructor is
 * empty like Java's, every other body is AION_UNPORTED. C++ differences:
 * - `restrictedSkills` (an Arrays.asList of literals) is a static constexpr std::array (hub-headers.md §11.1).
 * - `LEADERBOARD_WINDOW_OBJECT_ID` is initialized from IDFactory in Java's static initializer, which would run before the id factory exists in
 *   C++: leaderboardWindowObjectId() allocates it on first use, and the constructor calls it, so the id is taken at the same point of the
 *   startup as in Java (the first getInstance()).
 *
 * @author Jo, Estrayl
 */
class CustomInstanceService : public runtime::Immortal {
private:
	// fieldmap: Arrays.asList of literals only read by contains(): a static constexpr std::array (hub-headers.md §11.1; decision requested)
	static constexpr std::array<int32_t, 39> restrictedSkills{0, 243, 244, 277, 282, 302, 912, 1178, 1327, 1346, 1347, 1757, 2106, 2167, 2400, 2425, 2565,
		2778, 3331, 3643, 3663, 3683, 3705, 3729, 3788, 3789, 3833, 3835, 3837, 3839, 3904, 3991, 4407, 8291, 10164, 11011, 13010, 13234, 13231};

public:
	static constexpr int32_t REWARD_COIN_ID = 186000409;

private:
	static constexpr int32_t CUSTOM_INSTANCE_WORLD_ID = 300070000; // roah chamber
	static constexpr int32_t RESET_HOUR = 9;

	// Neural network related
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>>> playerModelEntriesCache{
		AION_LOCK_CLASS(CustomInstanceService::playerModelEntriesCache#stripe)};

	CustomInstanceService();
	~CustomInstanceService();

	/** Java: private static final int LEADERBOARD_WINDOW_OBJECT_ID = IDFactory.getInstance().nextId() (see the class comment) */
	static int32_t leaderboardWindowObjectId();

public:
	bool canEnter(int32_t playerId);

	void onEnter(model::gameobjects::player::Player& player);

	CustomInstanceRank loadOrCreateRank(int32_t playerId);

	bool resetEntryCooldown(int32_t playerId);

	bool updateLastEntry(int32_t playerId, int64_t newEntryTime);

	bool changePlayerRank(int32_t playerId, int32_t newRank, int32_t achievedDps);

private:
	bool storeNewRankData(CustomInstanceRank& rankObj);

	void changeRank(CustomInstanceRank& rankObj, int32_t newRank);

public:
	/** target: nullable (Java passes effector.getTarget() and checks instanceof Creature) */
	void recordPlayerModelEntry(model::gameobjects::player::Player& player, skillengine::model::Skill& skill,
		runtime::Ptr<model::gameobjects::VisibleObject> target);

private:
	std::vector<runtime::Ref<neuralnetwork::PlayerModelEntry>> loadPlayerModelEntries(int32_t playerId);

public:
	void saveNewPlayerModelEntries(int32_t playerId);

	/** @return the live cached list (created if absent) */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<neuralnetwork::PlayerModelEntry>>> getPlayerModelEntries(int32_t playerId);

	void openLeaderboard(model::gameobjects::player::Player& player, model::Race race);

	static CustomInstanceService& getInstance();
};

} // namespace aion::gameserver::custom::instance
