#include "aion/gameserver/handlers/ai/SpeakerAI.h"

#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::handlers::ai {

AION_AI(SpeakerAI, "speaker");

// Java SpeakerAI.java:20-61
bool SpeakerAI::onPatternShout(ShoutEventType event, std::string_view pattern, int32_t /*skillNumber*/) {
	if (getOwner().getWorldId() != 210050000 && getOwner().getWorldId() != 220070000)
		return false;
	if (event != ShoutEventType::IDLE || pattern.empty())
		return false;
	SiegeService& srv = SiegeService::getInstance();
	Race npcRace = getOwner().getRace();
	if (npcRace == Race::ASMODIANS) {
		if (pattern == "1") {
			// TODO: find if Dredgion Ship is spawned
		} else if (pattern == "2") {
			return inProgressFor(srv, 1142, SiegeRace::ASMODIANS) || inProgressFor(srv, 1143, SiegeRace::ASMODIANS)
				|| inProgressFor(srv, 1144, SiegeRace::ASMODIANS) || inProgressFor(srv, 1145, SiegeRace::ASMODIANS);
		} else if (pattern == "3") {
			return inProgressFor(srv, 1132, SiegeRace::ASMODIANS) || inProgressFor(srv, 1251, SiegeRace::ASMODIANS);
		} else if (pattern == "4") {
			return inProgressFor(srv, 1221, SiegeRace::BALAUR);
		} else if (pattern == "5") {
			return srv.isSiegeInProgress(1131);
		} else if (pattern == "6") {
			return heldBy(srv, 3011, SiegeRace::ELYOS) && heldBy(srv, 3021, SiegeRace::ELYOS);
		}
	} else if (npcRace == Race::ELYOS) {
		if (pattern == "1") {
			// TODO: find if Dredgion Ship is spawned
		} else if (pattern == "2") {
			return inProgressFor(srv, 1142, SiegeRace::ELYOS) || inProgressFor(srv, 1143, SiegeRace::ELYOS)
				|| inProgressFor(srv, 1144, SiegeRace::ELYOS) || inProgressFor(srv, 1145, SiegeRace::ELYOS);
		} else if (pattern == "3") {
			return inProgressFor(srv, 1141, SiegeRace::ELYOS) || inProgressFor(srv, 1211, SiegeRace::ELYOS);
		} else if (pattern == "4") {
			return inProgressFor(srv, 1241, SiegeRace::BALAUR);
		} else if (pattern == "5") {
			return srv.isSiegeInProgress(1141);
		} else if (pattern == "6") {
			return heldBy(srv, 2011, SiegeRace::ASMODIANS) && heldBy(srv, 2021, SiegeRace::ASMODIANS);
		}
	}
	return false;
}

bool SpeakerAI::inProgressFor(SiegeService& srv, int32_t id, SiegeRace race) {
	return srv.isSiegeInProgress(id) && srv.getSiegeLocation(id)->getRace() == race; // a siege in progress has its location
}

bool SpeakerAI::heldBy(SiegeService& srv, int32_t id, SiegeRace race) {
	return srv.getSiegeLocation(id) != nullptr && srv.getSiegeLocation(id)->getRace() == race;
}

} // namespace aion::gameserver::handlers::ai
