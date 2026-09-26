#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder. The
 * constructor is ported (it only logs).
 *
 * @author Simple, Sphinx, xTz
 */
class DuelService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, int32_t> duels{AION_LOCK_CLASS(DuelService::duels#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::FutureRef> drawTasks{AION_LOCK_CLASS(DuelService::drawTasks#stripe)}; // Java: = new ConcurrentHashMap<>()
public:
	static DuelService& getInstance(); // Java singleton
private:
	DuelService();
	~DuelService();
public:
	/** Send the duel request to the target */
	void onDuelRequest(model::gameobjects::player::Player& requester, runtime::Ptr<model::gameobjects::player::Player> targetPlayer);
	/** Asks confirmation for the duel request */
	void confirmDuelWith(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& targetPlayer);
private:
	/** Rejects the duel request */
	void rejectDuelRequest(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder);
	void cancelDuelRequest(model::gameobjects::player::Player& canceller, model::gameobjects::player::Player& target);
	/** Starts the duel */
	void startDuel(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder);
public:
	/**
	 * send SM_DELETE a second time to fix client not fading out the char (only happens when dueling with a team member of a group or alliance)
	 */
	void fixTeamVisibility(model::gameobjects::player::Player& hiddenDuelist);
	/** Lets the given player lose the duel, ending it */
	void loseDuel(model::gameobjects::player::Player& loser);
private:
	void endDebuffsByOpponent(model::gameobjects::player::Player& player, int32_t opponentId);
	void cancelSummonedObjectAttacks(model::gameobjects::player::Player& target, int32_t summonerId);
	void createTask(model::gameobjects::player::Player& requester, model::gameobjects::player::Player& responder);
	void onDuelEnd(model::DuelResult duelResult, model::gameobjects::player::Player& player, int32_t opponentId);
public:
	std::optional<int32_t> getOpponentId(model::gameobjects::player::Player& player);
	bool isDueling(model::gameobjects::player::Player& player);
	bool isDueling(model::gameobjects::player::Player& player, model::gameobjects::player::Player& opponent);
private:
	void registerDuel(int32_t requesterObjId, int32_t responderObjId);
	void removeDuel(model::gameobjects::player::Player& player);
	void removeAndEndTask(int32_t playerId);
};

} // namespace aion::gameserver::services
