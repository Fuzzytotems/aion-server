#pragma once

#include "aion/gameserver/handlers/instance/pvp/BasicPvpInstance.h"

namespace aion::gameserver::handlers::instance::pvp {

/**
 * Iron Wall Warfront (301220000): two races of 24, the points for the generators, the bosses and the used objects, the end at a lead of
 * 30,000 points or after 40 minutes.
 * <p>
 * Java: data/handlers/instance/pvp/IronWallWarfrontInstance.java, @InstanceID(301220000).
 * <p>
 * Correction (owner's decision 2026-10-15, both branches; docs/deviations/P6-instances.md): Java's portToPosition(Player, WorldMapInstance)
 * overrides nothing, so onStart and onReviveEvent reach GeneralInstanceHandler.portToStartPosition, which throws
 * UnsupportedOperationException, and a run with players inside never starts. Here it is portToStartPosition(Player), on the handler's
 * instance.
 *
 * @author Tibald
 */
class IronWallWarfrontInstance : public BasicPvpInstance {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t MAX_PLAYERS_PER_FACTION = 24;

protected:
	explicit IronWallWarfrontInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<IronWallWarfrontInstance> create(WorldMapInstance& instance);

protected:
	void onStart() override;

private:
	void endPreparingAndStart();

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
};

} // namespace aion::gameserver::handlers::instance::pvp
