#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::rentusBase {

/**
 * The AI of the collapsed Reian buildings in Rentus Base ("collapsed_reian_building"): it casts 20088 on its spawn.
 * <p>
 * Java: data/handlers/ai/instance/rentusBase/CollapsedReianBuildingAI.java, @AIName("collapsed_reian_building") (the marker is in the .cpp).
 *
 * @author xTz
 */
class CollapsedReianBuildingAI : public NpcAI {
public:
	explicit CollapsedReianBuildingAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::rentusBase
