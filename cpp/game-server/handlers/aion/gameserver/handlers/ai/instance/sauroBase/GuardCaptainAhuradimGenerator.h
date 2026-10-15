#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/NoActionAI.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of Guard Captain Ahuradim's shield generators ("guard_captain_ahuradim_generator"): the end of their charge (21200) shields
 * Ahuradim (21191) when they know him.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/GuardCaptainAhuradimGenerator.java, @AIName("guard_captain_ahuradim_generator")
 * (the marker is in the .cpp).
 */
class GuardCaptainAhuradimGenerator : public NoActionAI {
public:
	explicit GuardCaptainAhuradimGenerator(Npc& owner) : NoActionAI(owner) {}

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
