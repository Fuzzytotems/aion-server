#pragma once

#include "aion/gameserver/handlers/ai/KiskAI.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of the four invisible kisks ("invisible_kisk", npcs 701768-701771): a kisk that hides itself with its skill when it spawns.
 * <p>
 * Java: data/handlers/ai/InvisiblekiskAI.java, @AIName("invisible_kisk") (the marker is in the .cpp).
 *
 * @author Cheatkiller, Bobobear
 */
class InvisiblekiskAI : public KiskAI {
public:
	explicit InvisiblekiskAI(Npc& owner) : KiskAI(owner) {}

protected:
	void handleSpawned() override;

private:
	int32_t getHideSkillId();
};

} // namespace aion::gameserver::handlers::ai
