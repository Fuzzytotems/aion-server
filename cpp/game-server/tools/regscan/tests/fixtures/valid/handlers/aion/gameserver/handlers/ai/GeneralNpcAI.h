#pragma once

#include <string>

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/** A handler base class that is registered itself: header plus .cpp with the marker. */
class GeneralNpcAI : public gameserver::ai::AITemplate<Npc> {
public:
	explicit GeneralNpcAI(Npc& owner) : AITemplate(owner) {}
	std::string describe() const override;
};

} // namespace aion::gameserver::handlers::ai
