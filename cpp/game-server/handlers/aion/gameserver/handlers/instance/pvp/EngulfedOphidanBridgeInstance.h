#pragma once

#include "aion/gameserver/handlers/instance/pvp/BasicPvpInstance.h"

#include <vector>

namespace aion::gameserver::handlers::instance::pvp {

/**
 * Engulfed Ophidan Bridge (301210000): two races of six, the bases whose generators the races capture and that give points every 30 s,
 * the supplies and guards on a timeline, the end at a lead of 30,000 points or after 20 minutes.
 * <p>
 * Java: data/handlers/instance/pvp/EngulfedOphidanBridgeInstance.java, @InstanceID(301210000).
 *
 * @author Tibald
 */
class EngulfedOphidanBridgeInstance : public BasicPvpInstance {
	AION_MAKE_REF_FRIEND
private:
	static constexpr int32_t MAX_PLAYERS_PER_FACTION = 6;
	runtime::Field<int32_t> timeInMin{};

protected:
	explicit EngulfedOphidanBridgeInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<EngulfedOphidanBridgeInstance> create(WorldMapInstance& instance);

protected:
	void spawnFactionRelatedNpcs() override;

	void onStart() override;

private:
	void startTimeCheck();

	void checkBases();

protected:
	void setAndDistributeRewards(Player& player, PvpInstancePlayerReward& reward, Race winningRace, bool isBossKilled) override;

	using BasicPvpInstance::updatePoints;

	void updatePoints(runtime::Ptr<Player> player, Race race, std::optional<std::string_view> npcL10n, int32_t points) override;

private:
	void updatePointsByGenerators(const std::vector<runtime::Ptr<Npc>>& generators, Race race, bool check);

public:
	using BasicPvpInstance::onDie;

	void onDie(Npc& npc) override;

	void handleUseItemFinish(runtime::Ptr<Player> player, Npc& npc) override;

private:
	void captureBasesBalaurs(Npc& npc);

	void captureBases(Player& player, Npc& npc);

	void openFirstDoors();

public:
	void onInstanceCreate() override;

	void portToStartPosition(Player& player) override;
};

} // namespace aion::gameserver::handlers::instance::pvp
