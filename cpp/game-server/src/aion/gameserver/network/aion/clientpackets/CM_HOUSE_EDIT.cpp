#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_EDIT.h"

#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/HouseDecoration.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/HouseTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/DecorateAction.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/controllers/HouseController.h"
#include "aion/gameserver/controllers/PlaceableObjectController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_EDIT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_REGISTRY.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/services/item/HouseObjectFactory.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemDeleteType.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/network/aion/AionConnection.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

using model::gameobjects::HouseObject;
using model::gameobjects::player::Player;
using model::house::House;
using serverpackets::SM_HOUSE_EDIT;
using serverpackets::SM_HOUSE_REGISTRY;


CM_HOUSE_EDIT::CM_HOUSE_EDIT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_EDIT.java:43-70
void CM_HOUSE_EDIT::readImpl() {
	action = readUC();
	if (action == 3) {
		itemObjectId = readD();
	} else if (action == 4) {
		itemObjectId = readD();
	} else if (action == 5) {
		itemObjectId = readD();
		x = readF();
		y = readF();
		z = readF();
		rotation = readUH();
	} else if (action == 6) {
		itemObjectId = readD();
		x = readF();
		y = readF();
		z = readF();
		rotation = readUH();
	} else if (action == 7) {
		itemObjectId = readD();
	} else if (action == 16) {
		buildingId = readD();
	}
}

// Java CM_HOUSE_EDIT.java:73-155. A null house of a player without one is Java's NullPointerException on the arms that read it
void CM_HOUSE_EDIT::runImpl() {
	const runtime::Ptr<Player> player = getConnection()->getActivePlayer();
	if (!player)
		return;
	const runtime::Ptr<House> house = player->getActiveHouse();
	const auto requireHouse = [&]() -> House& {
		if (!house)
			throw runtime::NullPointerException("the player has no active house");
		return *house;
	};
	if (action == 1) { // Enter Decoration mode
		sendPacket(SM_HOUSE_EDIT(action));
		sendPacket(SM_HOUSE_REGISTRY(action));
		sendPacket(SM_HOUSE_REGISTRY(action + 1));
	} else if (action == 2) { // Exit Decoration mode
		sendPacket(SM_HOUSE_EDIT(action));
	} else if (action == 3) { // Add item
		const runtime::Ptr<model::gameobjects::Item> item = player->getInventory().getItemByObjId(itemObjectId);
		if (!item)
			return;
		const model::templates::item::ItemTemplate* template_ = item->getItemTemplate();
		player->getInventory().delete_(*item, services::item::ItemPacketService_ItemDeleteType::REGISTER);
		if (!template_->getActions()) // Java: template.getActions().getDecorateAction() on null actions
			throw runtime::NullPointerException("the item template has no actions");
		const model::templates::item::actions::DecorateAction* decorateAction = template_->getActions()->getDecorateAction();
		if (decorateAction) {
			runtime::Ref<model::gameobjects::HouseDecoration> decor =
				model::gameobjects::HouseDecoration::create(utils::idfactory::IDFactory::getInstance().nextId(), decorateAction->getTemplateId());
			requireHouse().getRegistry()->putDecor(*decor, true);
			sendPacket(SM_HOUSE_EDIT(action, 2, decor->getObjectId()));
		} else {
			runtime::Ref<HouseObject> obj = services::item::HouseObjectFactory::createNew(requireHouse(), template_);
			requireHouse().getRegistry()->putObject(*obj, true);
			sendPacket(SM_HOUSE_EDIT(action, 1, obj->getObjectId()));
		}
	} else if (action == 4) { // Delete item
		const runtime::Ptr<HouseObject> object = requireHouse().getRegistry()->getObjectByObjId(itemObjectId);
		if (!object) // Java: discardObject(null, false) dereferences it
			throw runtime::NullPointerException("no house object " + std::to_string(itemObjectId));
		requireHouse().getRegistry()->discardObject(*object, false);
		sendPacket(SM_HOUSE_EDIT(action, 1, itemObjectId));
		sendPacket(SM_HOUSE_EDIT(4, 1, itemObjectId));
	} else if (action == 5) { // spawn object
		const runtime::Ptr<HouseObject> obj = requireHouse().getRegistry()->getObjectByObjId(itemObjectId);
		if (!obj)
			return;
		obj->setX(x);
		obj->setY(y);
		obj->setZ(z);
		obj->setRotation(rotation);
		sendPacket(SM_HOUSE_EDIT(action, itemObjectId, x, y, z, rotation));
		obj->spawn();
		requireHouse().getRegistry()->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
		sendPacket(SM_HOUSE_EDIT(4, 1, itemObjectId));
		questEngine::QuestEngine::getInstance().onHouseItemUseEvent(*questEngine::model::QuestEnv::create(nullptr, *player, 0));
	} else if (action == 6) { // move object
		const runtime::Ptr<HouseObject> obj = requireHouse().getRegistry()->getObjectByObjId(itemObjectId);
		if (!obj)
			return;
		sendPacket(SM_HOUSE_EDIT(action + 1, 0, itemObjectId));
		obj->getController().delete_();
		obj->setX(x);
		obj->setY(y);
		obj->setZ(z);
		obj->setRotation(rotation);
		if (obj->getPersistentState() == model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED)
			requireHouse().getRegistry()->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
		sendPacket(SM_HOUSE_EDIT(action - 1, itemObjectId, x, y, z, rotation));
		obj->spawn();
	} else if (action == 7) { // despawn object
		const runtime::Ptr<HouseObject> obj = requireHouse().getRegistry()->getObjectByObjId(itemObjectId);
		if (!obj)
			return;
		sendPacket(SM_HOUSE_EDIT(action, 0, itemObjectId));
		obj->removeFromHouse();
		sendPacket(SM_HOUSE_EDIT(3, 1, itemObjectId)); // place it back
	} else if (action == 14) { // enter renovation mode
		sendPacket(SM_HOUSE_EDIT(14));
	} else if (action == 15) { // exit renovation mode
		sendPacket(SM_HOUSE_EDIT(15));
	} else if (action == 16) {
		if (!removeRenovationCoupon(*player, requireHouse())) {
			utils::audit::AuditLogger::log(*player, "attempted house renovation without coupon");
			return;
		}
		services::HousingService::getInstance().switchHouseBuilding(requireHouse(), buildingId);
		requireHouse().getController().updateAppearance();
	}
}

// Java CM_HOUSE_EDIT.java:157-165
bool CM_HOUSE_EDIT::removeRenovationCoupon(Player& player, House& house) {
	int32_t typeId = getId(house.getHouseType());
	if (typeId == 0)
		return false; // studio
	int32_t itemId = (player.getRace() == model::Race::ELYOS ? 169661004 : 169661008) - typeId;
	if (player.getInventory().getItemCountByItemId(itemId) > 0)
		return player.getInventory().decreaseByItemId(itemId, 1);
	return false;
}

AION_CLIENT_PACKET(CM_HOUSE_EDIT);

} // namespace aion::gameserver::network::aion::clientpackets
