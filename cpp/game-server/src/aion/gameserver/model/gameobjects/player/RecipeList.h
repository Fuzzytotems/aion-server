#pragma once

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/runtime/collections/HashSet.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.recipeList`), created with create(). Java's
 * constructor replaces the set with the given HashSet; the C++ shim copies its elements (the caller's set is a DAO local).
 *
 * @author MrPoke, Neon
 */
class RecipeList : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::HashSet<int32_t> recipeList{AION_LOCK_CLASS(RecipeList::recipeList)};

protected:
	explicit RecipeList(const std::unordered_set<int32_t>& recipeList);
	RecipeList();
	~RecipeList() override;

public:
	/** Java: new RecipeList(recipeList) */
	static runtime::Ref<RecipeList> create(const std::unordered_set<int32_t>& recipeList);

	/** Java: new RecipeList() */
	static runtime::Ref<RecipeList> create();

	runtime::HashSet<int32_t>& getRecipeList() { return recipeList; }

	bool addRecipe(Player& player, int32_t recipeId);

	bool deleteRecipe(Player& player, int32_t recipeId);

	bool isRecipePresent(int32_t recipeId);

	int32_t size();
};

} // namespace aion::gameserver::model::gameobjects::player
