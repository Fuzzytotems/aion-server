#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::drakenspire {

/**
 * The AI of Orissan's door destroyer ("orissan_door_destroyer"): spawned, it shouts 7 s later and 14 s later blows up the gates (731580)
 * within 15 m (20840).
 * <p>
 * Java: data/handlers/ai/instance/drakenspire/OrissanDoorDestroyerAI.java, @AIName("orissan_door_destroyer") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class OrissanDoorDestroyerAI : public GeneralNpcAI {
public:
	explicit OrissanDoorDestroyerAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

private:
	void scheduleGateDestruction();
};

} // namespace aion::gameserver::handlers::ai::instance::drakenspire
