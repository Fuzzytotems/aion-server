#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::housing {

int8_t PlaceableHouseObject::getTypeId() const {
	// the Java overrides of HousingChair ... HousingUseableItem (their C++ getters return the same values)
	std::string_view className = javaClassName();
	if (className == "HousingChair")
		return 5;
	if (className == "HousingEmblem")
		return 11;
	if (className == "HousingJukeBox")
		return 6;
	if (className == "HousingMoveableItem")
		return 0;
	if (className == "HousingMovieJukeBox")
		return 0; // unknown
	if (className == "HousingNpc")
		return 7;
	if (className == "HousingPassiveItem")
		return 0;
	if (className == "HousingPicture")
		return 0;
	if (className == "HousingPostbox")
		return 3;
	if (className == "HousingStorage")
		return 2;
	if (className == "HousingUseableItem")
		return 1;
	throw runtime::IllegalStateException("No getTypeId for house object class " + std::string(className));
}

} // namespace aion::gameserver::model::templates::housing
