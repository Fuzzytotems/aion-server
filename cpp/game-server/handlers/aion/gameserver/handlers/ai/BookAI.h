#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a readable book ("book": the books of Sanctum and Pandaemonium): the dialog opens page 1011.
 * <p>
 * Java: data/handlers/ai/BookAI.java, @AIName("book").
 */
class BookAI : public NpcAI {
public:
	explicit BookAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai
