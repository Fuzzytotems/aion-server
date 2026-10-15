#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a skill area ("skillarea": the Sorcerer's Ice Sheet, the Templar's Threatening Wave, ...): an NpcAI with no behaviour of its own;
 * SummonSkillAreaEffect's fixed-rate task makes it cast.
 * <p>
 * Java: data/handlers/ai/SkillAreaNpcAI.java, @AIName("skillarea") (the marker is in the .cpp).
 */
class SkillAreaNpcAI : public NpcAI {
public:
	explicit SkillAreaNpcAI(Npc& owner) : NpcAI(owner) {}
};

} // namespace aion::gameserver::handlers::ai
