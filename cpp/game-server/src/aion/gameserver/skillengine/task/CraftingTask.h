#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/model/templates/recipe/fwd.h"
#include "aion/gameserver/skillengine/task/AbstractCraftTask.h"
#include "aion/gameserver/skillengine/task/fwd.h"

namespace aion::gameserver::skillengine::task {

/**
 * One crafting interaction of a player with a crafting station (or with nothing, for a morph): the craft bar, its packets and the product.
 * <p>
 * The shell of m5c-plan.md I-02, drafted with tools/gen/skeleton.py --draft from the fieldmap.json row, so that CraftService (C-01, which
 * constructs one, CraftService.java:123) and this class's bodies (C-02) compile against the same header. RefCounted (fieldmap K4, the class
 * tree of AbstractInteractionTask: held by `Player::interactionTask`), created with create(). Every body is AION_UNPORTED until C-02.
 *
 * @author Mr. Poke, synchro2, Yeats
 */
class CraftingTask : public AbstractCraftTask {
	AION_MAKE_REF_FRIEND
private:
	const gameserver::model::templates::recipe::RecipeTemplate* recipeTemplate;
	const int32_t maxCritCount;
	const int32_t bonus;
	runtime::Field<const gameserver::model::templates::item::ItemTemplate*> itemTemplate{};
	runtime::Field<int32_t> critCount{};
	runtime::Field<int32_t> showBarDelay{};
	runtime::Field<int32_t> executionSpeed{};

protected:
	/**
	 * @param responder nullable: CraftService.startCrafting casts the player's known object with the target's id to StaticObject, and a morph
	 *   (skill 40009) needs no target, so the cast of a missing target is null (CraftService.java:101, 123, 149-157); AbstractInteractionTask
	 *   then makes the requester the responder (AbstractInteractionTask.java:29-32)
	 */
	CraftingTask(gameserver::model::gameobjects::player::Player& requester, runtime::Ptr<gameserver::model::gameobjects::StaticObject> responder,
		const gameserver::model::templates::recipe::RecipeTemplate* recipeTemplate, int32_t skillLvlDiff, int32_t bonus);
	~CraftingTask() override;

public:
	/** Java: new CraftingTask(requester, responder, recipeTemplate, skillLvlDiff, bonus) */
	static runtime::Ref<CraftingTask> create(gameserver::model::gameobjects::player::Player& requester,
		runtime::Ptr<gameserver::model::gameobjects::StaticObject> responder,
		const gameserver::model::templates::recipe::RecipeTemplate* recipeTemplate, int32_t skillLvlDiff, int32_t bonus);

protected:
	void onFailureFinish() override;

	bool onSuccessFinish() override;

private:
	bool calculateCrit();

protected:
	void sendInteractionUpdate() override;

	void onInteractionAbort() override;

	void onInteractionFinish() override;

	void onInteractionStart() override;

	void analyzeInteraction() override final;
};

} // namespace aion::gameserver::skillengine::task
