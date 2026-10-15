#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of Bubblegut ("bubblegut"): a GeneralNpcAI that casts skill 16447 once it spawned.
 * <p>
 * Java: data/handlers/ai/BubblegutAI.java, @AIName("bubblegut").
 */
class BubblegutAI : public GeneralNpcAI {
public:
	explicit BubblegutAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;
};

} // namespace aion::gameserver::handlers::ai
