#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/dragonLordsRefuge/CalindiFlamelordAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of the anguished Calindi Flamelord ("IDTiamat_HM_calindi_flamelord"): her own Surkanas (731629, 731630) and flames (856298),
 * 21887 and a 3 % blaze (21888: 856299).
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/HM_CalindiFlamelordAI.java, @AIName("IDTiamat_HM_calindi_flamelord") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class HM_CalindiFlamelordAI : public CalindiFlamelordAI {
public:
	explicit HM_CalindiFlamelordAI(Npc& owner) : CalindiFlamelordAI(owner) {}

protected:
	void startHallucinatoryVictoryEvent() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void blazeEngraving() override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
