#pragma once

#include "aion/gameserver/handlers/instance/pvp/BasicPvpInstance.h"

#include <vector>

namespace aion::gameserver::handlers::instance::pvp {

/**
 * Kamar Battlefield (301120000): two races of 12; the timeline of the teleporters, supplies, siege weapons, the dreadgion, the drakan and
 * their generals; the end at a lead of 20,000 points, at the death of 232853 or after 30 minutes. Morale Boost is applied to re-spawning
 * players which are a part of the losing side (ID: 10). REINFORCE_MEMBER for 2 minutes, PREPARING for 1 minute, START_PROGRESS for 30
 * minutes.
 * <p>
 * Java: data/handlers/instance/pvp/KamarBattlefieldInstance.java, @InstanceID(301120000).
 *
 * @author xTz
 */
class KamarBattlefieldInstance : public BasicPvpInstance {
	AION_MAKE_REF_FRIEND
private:
	/** Java: private static final List<WorldPosition> generalsPos (filled once in the static block; immortal) */
	static const std::vector<runtime::Ref<WorldPosition>>& generalsPos();
	/** Java: private static final List<WorldPosition> garnonPos (filled once in the static block; immortal) */
	static const std::vector<runtime::Ref<WorldPosition>>& garnonPos();
	static constexpr int32_t MAX_PLAYERS_PER_FACTION = 12;
	runtime::Field<int8_t> timeInMin{-1};

protected:
	explicit KamarBattlefieldInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<KamarBattlefieldInstance> create(WorldMapInstance& instance);

protected:
	void onStart() override;

private:
	void endPreparingAndStart();

	void onTimeProgressed();

protected:
	void setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) override;

	using BasicPvpInstance::updatePoints;

	void updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points) override;

public:
	using BasicPvpInstance::onDie;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void openFirstDoors();

protected:
	int32_t getReinforceMemberPhaseDelay() override;

public:
	void onInstanceCreate() override;

	void portToStartPosition(Player& player) override;

	void onEnterZone(Player& player, ZoneInstance& zone) override;
};

} // namespace aion::gameserver::handlers::instance::pvp
