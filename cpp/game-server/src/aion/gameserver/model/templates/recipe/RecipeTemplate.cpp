#include "aion/gameserver/model/templates/recipe/RecipeTemplate.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::recipe {

std::optional<int32_t> RecipeTemplate::getComboProduct(int32_t num) const {
	if (comboproduct.empty()) // Java: comboproduct == null (a plain element list is never present and empty)
		return std::nullopt;
	if (num < 1 || static_cast<size_t>(num) > comboproduct.size())
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(num - 1) + " out of bounds for length " + std::to_string(comboproduct.size()));
	return comboproduct[static_cast<size_t>(num - 1)].getItemId();
}

} // namespace aion::gameserver::model::templates::recipe
