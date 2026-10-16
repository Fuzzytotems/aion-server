#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/commons/logging/Logger.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of an obelisk ("resurrect", the 129 bind-point npcs): a click asks the player whether to bind his resurrection point here for the
 * template's price; on yes (within 5 m, same world, enough kinah) the point is stored at the player's own position.
 * <p>
 * Java: data/handlers/ai/ResurrectAI.java, @AIName("resurrect") (the marker is in the .cpp). Java's anonymous AIRequest is the callback struct
 * ResurrectAI_AIRequest of the .cpp (fieldmap ai.ResurrectAI$1).
 *
 * @author ATracer
 */
class ResurrectAI : public NpcAI {
private:
	static const commons::logging::Logger log;

public:
	explicit ResurrectAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

private:
	void bindHere(Player& player, const BindPointTemplate* bindPointTemplate);
};

} // namespace aion::gameserver::handlers::ai
