#include "aion/gameserver/handlers/ai/instance/aturamSkyFortress/DredgionGeneratorAI.h"

namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress {

AION_AI(DredgionGeneratorAI, "dredgion_generator");

// Java DredgionGeneratorAI.java:19-22
bool DredgionGeneratorAI::canThink() {
	return false;
}

// Java DredgionGeneratorAI.java:24-30
bool DredgionGeneratorAI::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return GeneralNpcAI::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::aturamSkyFortress
