#pragma once

#include <memory>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/fwd.h"
#include "aion/gameserver/controllers/movement/fwd.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/pet/fwd.h"

namespace aion::gameserver::model::gameobjects {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A visible object: `VisibleObject::create<Pet>(petTemplate, controller, commonData,
 * master)` (§10.1); postConstruct() runs `controller.setOwner(this)`. The move controller is Java's anonymous `new CreatureMoveController<Pet>(this)
 * {}` (fieldmap.toml: a part created by the constructor), the struct Pet_CreatureMoveController in Pet.cpp; the erased type is
 * CreatureMoveController (§8.1). getObjectTemplate() is Java's cast-only override: a narrowing redeclaration (§8.2). The base initializer needs
 * the master's world id (VisibleObject::getWorldId, unported), so the constructor reaches `AION_UNPORTED`.
 *
 * @author ATracer
 */
class Pet : public VisibleObject {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<player::Player> master;
	const std::unique_ptr<controllers::movement::CreatureMoveController> moveController;
	const runtime::Ref<player::PetCommonData> commonData;

protected:
	Pet(CreateKey key, const templates::pet::PetTemplate* petTemplate, std::unique_ptr<controllers::PetController> controller,
		player::PetCommonData& commonData, player::Player& master);
	~Pet() override;

	/** Java constructor body after super(...): controller.setOwner(this) */
	void postConstruct() override;

public:
	std::string getName() override;

	runtime::Ptr<player::Player> getMaster() const { return master; }

	/** Java final */
	runtime::Ptr<player::PetCommonData> getCommonData() const { return commonData; }

	/** Java final; Java return type CreatureMoveController<Pet> */
	controllers::movement::CreatureMoveController& getMoveController() const;

	/** Narrows VisibleObject::getObjectTemplate (Java final cast-only override) */
	const templates::pet::PetTemplate* getObjectTemplate() const;
};

} // namespace aion::gameserver::model::gameobjects
