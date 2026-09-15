#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.h"

#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates::recipe {

/** Java com.aionemu.gameserver.model.templates.recipe.RecipeTemplate. C++: Java's Integer returns of int fields are plain values. @author ATracer */
class RecipeTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.xml.inc"
public:
	/** Java returns Collections.emptyList() without components */
	const std::vector<ComponentsData>& getComponents() const { return componentsData; }

	/**
	 * @return the item id of the num-th combo product (1-based), nullopt (Java null) without combo products
	 * @throws IndexOutOfBoundsException (Java) for a num outside the combo products
	 */
	std::optional<int32_t> getComboProduct(int32_t num) const;

	int32_t getComboProductSize() const { return static_cast<int32_t>(comboproduct.size()); }

	int32_t getQuantity() const { return quantity; }

	int32_t getProductId() const { return productid; }

	int32_t getDp() const { return dp; }

	int32_t getSkillpoint() const { return skillpoint; }

	int32_t getSkillId() const { return skillid; }

	int32_t getItemId() const { return itemid; }

	int32_t getL10nId() const override { return nameid; }

	int32_t getId() const { return id; }
};

} // namespace aion::gameserver::model::templates::recipe
