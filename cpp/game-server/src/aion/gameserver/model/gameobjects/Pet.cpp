#include "aion/gameserver/model/gameobjects/Pet.h"

#include <utility>

#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/pet/PetTemplate.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::model::gameobjects {

namespace {
/** Java: new CreatureMoveController<Pet>(this) {} (fieldmap callback key Pet$1: an anonymous subclass without overrides) */
struct Pet_CreatureMoveController final : controllers::movement::CreatureMoveController {
	explicit Pet_CreatureMoveController(Pet& pet) : CreatureMoveController(pet) {}
};
} // namespace

Pet::Pet(CreateKey key, const templates::pet::PetTemplate* petTemplate, std::unique_ptr<controllers::PetController> controller,
	player::PetCommonData& commonDataValue, player::Player& masterValue)
	: VisibleObject(key, commonDataValue.getObjectId(), std::move(controller), nullptr, petTemplate,
		  world::WorldPosition::create(masterValue.getWorldId()), false),
	  master(masterValue), moveController(std::make_unique<Pet_CreatureMoveController>(*this)), commonData(commonDataValue) {
}

Pet::~Pet() = default;

void Pet::postConstruct() {
	VisibleObject::postConstruct();
	getController().setOwner(*this);
}

std::string Pet::getName() {
	AION_UNPORTED();
}

controllers::movement::CreatureMoveController& Pet::getMoveController() const {
	return *moveController;
}

const templates::pet::PetTemplate* Pet::getObjectTemplate() const {
	return static_cast<const templates::pet::PetTemplate*>(VisibleObject::getObjectTemplate());
}

} // namespace aion::gameserver::model::gameobjects
