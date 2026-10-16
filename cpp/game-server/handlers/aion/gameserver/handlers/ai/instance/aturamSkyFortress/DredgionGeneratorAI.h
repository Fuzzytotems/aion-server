#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

/**
 * The AI of the dredgion generators ("dredgion_generator"): it never thinks; immune to abnormal states.
 * <p>
 * Java: data/handlers/ai/instance/aturamSkyFortress/DredgionGeneratorAI.java, @AIName("dredgion_generator") (the marker is in the .cpp).
 *
 * @author Tibald
 */
class DredgionGeneratorAI : public GeneralNpcAI {
public:
	explicit DredgionGeneratorAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
