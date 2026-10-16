#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold {

/**
 * The AI of Tiamat's eyes in Tiamat Stronghold ("tiamateye"): meant to speak a general's message 2 s after its spawn; unless dead it is deleted
 * 5 s after the spawn.
 * <p>
 * Java: data/handlers/ai/instance/tiamatStrongHold/TiamatEyeAI.java, @AIName("tiamateye") (the marker is in the .cpp). **Java bug, ported
 * (proposed correction J-I6-6):** the switch reads the 4.0 ids 283913-283916, but TiamatStrongHoldInstance spawns the eyes as 283177-283180
 * ("ex 283913" ...), so no message comes; proposed: switch on 283177-283180.
 *
 * @author Cheatkiller
 */
class TiamatEyeAI : public NpcAI {
public:
	explicit TiamatEyeAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void despawn();
};

} // namespace aion::gameserver::handlers::ai::instance::tiamatStrongHold
