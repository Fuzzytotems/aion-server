#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <vector>

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Tiamat's incarnations ("tiamats_incarnation"): 20 s after the activation, then every 30 s while two living players are
 * within 30 m, its summon (random of its ids) on two random of them; gone, it takes its summons; no decay or rewards.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/TiamatsIncarnationAI.java, @AIName("tiamats_incarnation") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class TiamatsIncarnationAI : public AggressiveNpcAI {
public:
	explicit TiamatsIncarnationAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleActivate() override;

private:
	void scheduleSummons(int32_t delay);

	std::vector<runtime::Ptr<Player>> getNearbyPlayers();

protected:
	void handleDespawned() override;

private:
	std::vector<int32_t> getSummonNpcIds();

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
