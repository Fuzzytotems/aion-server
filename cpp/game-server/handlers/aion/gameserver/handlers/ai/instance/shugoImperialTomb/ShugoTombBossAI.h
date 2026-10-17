#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/shugoImperialTomb/ShugoTombAttackerAI.h"
#include "aion/gameserver/runtime/fields/Atomic.h"

namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb {

/**
 * The AI of the Shugo Imperial Tomb's bosses ("shugo_tomb_boss"): an attacker that, when attacked and free to act, casts its skills in order,
 * each once its HP fell to the skill's max HP (stopping its walk), one at a time; the end of 21266 calls four 219509 around it, and a boss
 * still walking its route walks on 0.5 s after a cast. Decays, gives loot.
 * <p>
 * Java: data/handlers/ai/instance/shugoImperialTomb/ShugoTombBossAI.java, @AIName("shugo_tomb_boss") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class ShugoTombBossAI : public ShugoTombAttackerAI {
private:
	runtime::AtomicInteger usedSkills{AION_LOCK_CLASS(ShugoTombBossAI::usedSkills)}; // Java: = new AtomicInteger()
	runtime::AtomicBoolean isCasting{AION_LOCK_CLASS(ShugoTombBossAI::isCasting)}; // Java: = new AtomicBoolean()

public:
	explicit ShugoTombBossAI(Npc& owner) : ShugoTombAttackerAI(owner) {}

protected:
	void handleSpawned() override;

	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	bool ask(AIQuestion question) override;
};

} // namespace aion::gameserver::handlers::ai::instance::shugoImperialTomb
