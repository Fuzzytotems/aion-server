#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dredgion {

/**
 * The AI of a Shulack drudge in the Dredgion ("shulackdrudge"): its dialog gives the race's Dredgion supplies once, after which it turns
 * peaceful.
 * <p>
 * Java: data/handlers/ai/instance/dredgion/ShulackDrudgeAI.java, @AIName("shulackdrudge").
 *
 * @author cheatkiller
 */
class ShulackDrudgeAI : public GeneralNpcAI {
public:
	explicit ShulackDrudgeAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleDialogFinish(Player& player) override;

	void handleDialogStart(Player& player) override;

private:
	void addItems(Player& player);
};

} // namespace aion::gameserver::handlers::ai::instance::dredgion
