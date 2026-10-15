#pragma once

#include "aion/gameserver/handlers/instance/pvp/BasicPvpInstance.h"

namespace aion::gameserver::handlers::instance::pvp {

/**
 * Idgel Dome (301310000): two races of six, the turrets, levers and quest npcs of each side, Kunax after ten minutes, the end at his death
 * or after twenty minutes.
 * <p>
 * Java: data/handlers/instance/pvp/IdgelDomeInstance.java, @InstanceID(301310000). Java never fills chestPositions before the first
 * spawnChest, so the chests only come back from positions a used chest added; ported as written.
 *
 * @author Ritsu, Estrayl
 */
class IdgelDomeInstance : public BasicPvpInstance {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t MAX_PLAYERS_PER_FACTION = 6;
	runtime::ArrayList<runtime::Ref<WorldPosition>> chestPositions{AION_LOCK_CLASS(IdgelDomeInstance::chestPositions)}; // Java: = new ArrayList<>()

protected:
	explicit IdgelDomeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<IdgelDomeInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

protected:
	void spawnFactionRelatedNpcs() override;

	void onStart() override;

	void setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) override;

public:
	void onSpawn(VisibleObject& object) override;

private:
	void setFlameVentNoInteraction(Player& player, VisibleObject& flameVent);

public:
	using BasicPvpInstance::onDie;

	void onDie(Npc& npc) override;

private:
	void spawnChest();

	void scheduleChestRespawn();

public:
	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

	void onEnterInstance(Player& player) override;

	void portToStartPosition(Player& player) override;

	bool isBoss(Npc& npc) override;
};

} // namespace aion::gameserver::handlers::instance::pvp
