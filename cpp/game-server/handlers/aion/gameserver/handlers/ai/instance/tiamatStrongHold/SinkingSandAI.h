#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Shabokan's sinking sand in Tiamat Stronghold ("sinkingsand"): 3 s after its spawn it casts 20723 and 1 s later it is deleted.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/SinkingSandAI.java, @AIName("sinkingsand") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class SinkingSandAI : public NpcAI {
public:
	explicit SinkingSandAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void useskill();
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
