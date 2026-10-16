#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::raksang {

/**
 * The AI of the gargoyles in Raksang Ruins ("raksang_gargoyle"): 2 s after its spawn a living gargoyle casts 19126; immune to abnormal
 * states; dead it goes.
 * <p>
 * Java: data/handlers/ai/instance/raksang/RaksangGargoyleAI.java, @AIName("raksang_gargoyle") (the marker is in the .cpp).
 *
 * @author xTz
 */
class RaksangGargoyleAI : public AggressiveNpcAI {
public:
	explicit RaksangGargoyleAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	bool ask(AIQuestion question) override;

protected:
	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::raksang
