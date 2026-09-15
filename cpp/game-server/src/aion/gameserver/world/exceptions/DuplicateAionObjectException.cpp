#include "aion/gameserver/world/exceptions/DuplicateAionObjectException.h"

#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::world::exceptions {

namespace {

/** Java: sb.append(' ').append(((Player) object).getPosition()) for players */
void appendPlayerPosition(std::string& sb, model::gameobjects::AionObject& object) {
	if (auto* player = dynamic_cast<model::gameobjects::player::Player*>(&object)) {
		runtime::Ptr<WorldPosition> position = player->getPosition();
		sb += ' ';
		sb += position ? position->toString() : std::string("null");
	}
}

} // namespace

DuplicateAionObjectException::DuplicateAionObjectException(model::gameobjects::AionObject& object,
	runtime::Ptr<model::gameobjects::AionObject> presentObject)
	: runtime::Exception(createMessage(object, presentObject)) {
}

std::string DuplicateAionObjectException::createMessage(model::gameobjects::AionObject& object,
	runtime::Ptr<model::gameobjects::AionObject> presentObject) {
	std::string sb = "Duplicate object: ";
	sb += object.toString();
	appendPlayerPosition(sb, object);
	sb += ", already present object: ";
	if (presentObject) {
		sb += presentObject->toString();
		appendPlayerPosition(sb, *presentObject);
	} else {
		sb += "null";
	}
	return sb;
}

} // namespace aion::gameserver::world::exceptions
