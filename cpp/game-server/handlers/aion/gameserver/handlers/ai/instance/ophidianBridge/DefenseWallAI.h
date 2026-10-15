#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::ophidianBridge {

/**
 * The AI of Ophidan Bridge's defense walls ("defense_wall"): it never thinks; no decay, no AP, XP, DP or loot.
 * <p>
 * Java: data/handlers/ai/instance/ophidianBridge/DefenseWallAI.java, @AIName("defense_wall") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class DefenseWallAI : public GeneralNpcAI {
public:
	explicit DefenseWallAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::ophidianBridge
