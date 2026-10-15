#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/sched/Future.h"

namespace aion::gameserver::handlers::instance::pvp {

/**
 * The base of the newer PvP instances (Idgel Dome, Engulfed Ophidan Bridge, Kamar Battlefield, Iron Wall Warfront): the faction score,
 * the reinforcement, preparation and progress phases, the kill points and the rewards.
 * <p>
 * Java: data/handlers/instance/pvp/BasicPvpInstance.java (no @InstanceID: not registered). C++: the scheduled lambdas are pinned to the
 * handler; Java's `String npcL10n` (null for none) is a std::optional<std::string_view>.
 *
 * @author Estrayl
 */
class BasicPvpInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	runtime::ArrayList<runtime::FutureRef> tasks{AION_LOCK_CLASS(BasicPvpInstance::tasks)}; // Java: = new ArrayList<>()
	const int32_t raceStartPosition;                                                       // Java: = Rnd.nextInt(2)
	runtime::Field<runtime::Ref<PvpInstanceScore>> instanceScore{};
	runtime::Field<int64_t> startTime{};

	explicit BasicPvpInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<BasicPvpInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

protected:
	virtual void spawnFactionRelatedNpcs();

private:
	void startPreparation();

protected:
	virtual void onStart();

	virtual void onStop(bool isBossKilled);

	virtual void setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled);

	void distributeRewards(Player& player, PvpInstancePlayerReward& reward);

public:
	void leaveInstance(Player& player) override;

	void onLeaveInstance(Player& player) override;

	using GeneralInstanceHandler::onDie;

	bool onDie(Player& victim, Creature& lastAttacker) override;

	bool onReviveEvent(Player& player) override;

	void onEnterInstance(Player& player) override;

	void onInstanceDestroy() override;

protected:
	virtual int32_t getReinforceMemberPhaseDelay();

	virtual int32_t getTime();

	virtual void updateProgress(InstanceProgressionType progressionType);

	virtual void updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points);

	virtual void updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points, bool isVictim,
	                          bool shouldRewardPvpKill);

	virtual void revivePlayerOnEnd(Player& player);

	/** Java sendPacket(AionServerPacket): broadcastToMap(instance, packet). C++: a template, as the packets are temporaries */
	template <class P>
	void sendPacket(P&& packet) {
		PacketSendUtility::broadcastToMap(*instance, std::forward<P>(packet));
	}

	void cancelTasks();

	/** C++: the writer of a SM_INSTANCE_SCORE to everyone on the map (Java: sendPacket(new SM_INSTANCE_SCORE(mapId, writer, getTime()))) */
	void sendScore(runtime::Ref<PvpInstanceScoreWriter> writer);

	/** C++: Java's instance.getPlayersInside() as the writer's participant list */
	std::vector<runtime::Ref<Player>> playersInside();

public:
	runtime::Ptr<InstanceScore> getInstanceScore() override;
};

} // namespace aion::gameserver::handlers::instance::pvp
