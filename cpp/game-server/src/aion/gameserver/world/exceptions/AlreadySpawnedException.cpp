#include "aion/gameserver/world/exceptions/AlreadySpawnedException.h"

#include <typeinfo>

#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/VisibleObjectTemplate.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::world::exceptions {

AlreadySpawnedException::AlreadySpawnedException(model::gameobjects::VisibleObject& object) : runtime::Exception(createMessage(object)) {
}

std::string AlreadySpawnedException::createMessage(model::gameobjects::VisibleObject& object) {
	std::string sb = utils::simpleClassName(typeid(object));
	sb += " ";
	sb += object.getName();
	if (object.getObjectTemplate() != nullptr && dynamic_cast<model::gameobjects::player::Player*>(&object) == nullptr)
		sb += " (ID: " + std::to_string(object.getObjectTemplate()->getTemplateId()) + ")";
	sb += " is already spawned at ";
	runtime::Ptr<WorldPosition> position = object.getPosition();
	sb += position ? position->toString() : std::string("null");
	return sb;
}

} // namespace aion::gameserver::world::exceptions
