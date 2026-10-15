#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::sauroBase {

/**
 * The AI of Brigade General Sheba in Sauro Supply Base ("brigade_general_sheba"): her big skills hit at 55 % while cast and are announced;
 * the henchman skill brings four henchmen, the channeling skill alternates one centre and eight ring channelers; back home or dead she takes
 * them away.
 * <p>
 * Java: data/handlers/ai/instance/sauroBase/BrigadeGeneralShebaAI.java, @AIName("brigade_general_sheba") (the marker is in the .cpp).
 *
 * @author Estrayl, Yeats
 */
class BrigadeGeneralShebaAI : public AggressiveNpcAI {
public:
	explicit BrigadeGeneralShebaAI(Npc& owner) : AggressiveNpcAI(owner) {}

private:
	runtime::Field<float> multiplier{1.0f};
	runtime::Field<bool> spawnCenter{true};

public:
	float modifyOwnerDamage(float damage, Creature& effected, runtime::Ptr<Effect> effect) override;

	void onStartUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleBackHome() override;

	void handleDied() override;

private:
	void removeAdds();
};

} // namespace aion::gameserver::handlers::ai::instance::sauroBase
