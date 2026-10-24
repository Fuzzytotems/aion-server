#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai::instance::stonespearReach {

/**
 * The AI of Stonespear Reach's aetheric field blaststones and of its clown 856305 ("atheric_field_blaststone"): the clown walks its path; dead,
 * the npc is deleted at once; no respawn, reward, loot or decay.
 * <p>
 * Java: data/handlers/ai/instance/stonespearReach/AethericFieldBlaststoneAI.java, @AIName("atheric_field_blaststone").
 *
 * @author Yeats
 */
class AethericFieldBlaststoneAI : public NpcAI {
public:
	explicit AethericFieldBlaststoneAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDied() override;

	void handleMoveArrived() override;

public:
	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::stonespearReach
