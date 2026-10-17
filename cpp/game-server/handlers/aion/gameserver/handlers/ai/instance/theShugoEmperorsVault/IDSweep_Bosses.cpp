#include "aion/gameserver/handlers/ai/instance/theShugoEmperorsVault/IDSweep_Bosses.h"

namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault {

AION_AI(IDSweep_Bosses, "IDSweep_Boss");

// Java IDSweep_Bosses.java:17-23
bool IDSweep_Bosses::ask(AIQuestion question) {
	switch (question) {
		case AIQuestion::IS_IMMUNE_TO_ABNORMAL_STATES:
			return true;
		default:
			return IDSweep_Shugos::ask(question);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::theShugoEmperorsVault
