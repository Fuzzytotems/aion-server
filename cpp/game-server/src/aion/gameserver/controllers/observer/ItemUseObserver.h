#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Cancels an item use (a delayed item action) on any disturbing event of the user.
 * <p>
 * Abstract RefCounted ActionObserver (fieldmap K4). Java's anonymous subclasses (item actions, ItemActionService, StigmaService,
 * Equipment, PvpMapHandler) are the fieldmap callback structs of their creating classes' .cpp files (hub-headers.md §7.3), deriving from this
 * class; ObserveController::abortItemUseObservers finds them with a dynamic cast.
 *
 * @author MrPoke
 */
class ItemUseObserver : public ActionObserver {
	AION_MAKE_REF_FRIEND
protected:
	ItemUseObserver();
	~ItemUseObserver() override;

public:
	void attack(model::gameobjects::Creature& creature, int32_t skillId) override final;

	void attacked(model::gameobjects::Creature& creature, int32_t skillId) override final;

	void died(model::gameobjects::Creature& creature) override final;

	void dotattacked(model::gameobjects::Creature& creature, skillengine::model::Effect& dotEffect) override final;

	void equip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) override final;

	void unequip(model::gameobjects::Item& item, model::gameobjects::player::Player& owner) override final;

	void moved() override final;

	void startSkillCast(skillengine::model::Skill& skill) override final;

	void sit() override final;

	void endSkillCast(skillengine::model::Skill& skill) override;

	void itemused(model::gameobjects::Item& item) override;

	void boostSkillCost(skillengine::model::Skill& skill) override;

	virtual void abort() = 0;
};

} // namespace aion::gameserver::controllers::observer
