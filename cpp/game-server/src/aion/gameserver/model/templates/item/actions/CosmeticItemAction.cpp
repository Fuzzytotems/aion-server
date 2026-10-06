#include "aion/gameserver/model/templates/item/actions/CosmeticItemAction.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/dataholders/CosmeticItemsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/cosmeticitems/CosmeticItemTemplate.h"
#include "aion/gameserver/model/templates/cosmeticitems/CosmeticItemTemplate_Preset.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::templates::item::actions {

// Java CosmeticItemAction.java:31-52
bool CosmeticItemAction::canAct(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> /*targetItem*/, std::initializer_list<std::any> /*params*/) const {
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	const cosmeticitems::CosmeticItemTemplate* template_ = dataholders::DataManager::COSMETIC_ITEMS_DATA->getCosmeticItemsTemplate(cosmeticName);
	if (template_ == nullptr) {
		return false;
	}
	if (!template_->getRace()) // Java: template.getRace().equals(...) on null
		throw runtime::NullPointerException("CosmeticItemTemplate.getRace()");
	if (*template_->getRace() != player.getRace()) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_RACE());
		return false;
	}
	if (template_->getGenderPermitted() != "ALL") {
		// Java: player.getGender().toString(), the constant's name
		if (std::string(xml::enumName(player.getGender())) != template_->getGenderPermitted()) {
			utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ITEM_INVALID_GENDER());
			return false;
		}
	}
	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE)) {
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ITEM_RESTRICTION_RIDE());
		return false;
	}
	return true;
}

// Java CosmeticItemAction.java:54-86. Kept as in Java (proposed corrections, docs/deviations/P5-07.md): the preset arm sets the skin colour to
// the preset's eye colour, and the item deleted is targetItem (CM_USE_ITEM's target, null for a plain use: Storage.delete(null) throws after
// the new appearance was stored, so the item stays and onChangedPlayerAttributes is not called)
void CosmeticItemAction::act(gameobjects::player::Player& player, runtime::Ptr<gameobjects::Item> /*parentItem*/,
	runtime::Ptr<gameobjects::Item> targetItem, std::initializer_list<std::any> /*params*/) const {
	const cosmeticitems::CosmeticItemTemplate* template_ = dataholders::DataManager::COSMETIC_ITEMS_DATA->getCosmeticItemsTemplate(cosmeticName);
	if (template_ == nullptr) // Java: template.getType() on null
		throw runtime::NullPointerException("CosmeticItemsData.getCosmeticItemsTemplate(" + cosmeticName + ")");
	runtime::Ptr<gameobjects::player::PlayerAppearance> playerAppearance = player.getPlayerAppearance();
	const std::string& type = template_->getType();
	int32_t id = template_->getId();
	if (type == "hair_color")
		playerAppearance->setHairRGB(id);
	else if (type == "face_color")
		playerAppearance->setSkinRGB(id);
	else if (type == "lip_color")
		playerAppearance->setLipRGB(id);
	else if (type == "eye_color")
		playerAppearance->setEyeRGB(id);
	else if (type == "hair_type")
		playerAppearance->setHair(id);
	else if (type == "face_type")
		playerAppearance->setFace(id);
	else if (type == "voice_type")
		playerAppearance->setVoice(id);
	else if (type == "makeup_type")
		playerAppearance->setTattoo(id);
	else if (type == "tattoo_type")
		playerAppearance->setDeco(id);
	else if (type == "preset_name") {
		const cosmeticitems::CosmeticItemTemplate_Preset* preset = template_->getPreset();
		if (preset == nullptr) // Java: preset.getEyeColor() on null
			throw runtime::NullPointerException("CosmeticItemTemplate.getPreset()");
		playerAppearance->setEyeRGB(preset->getEyeColor());
		playerAppearance->setLipRGB(preset->getLipColor());
		playerAppearance->setHairRGB(preset->getHairColor());
		playerAppearance->setSkinRGB(preset->getEyeColor()); // java-bug kept: the eye colour (proposed correction: getSkinColor)
		playerAppearance->setHair(preset->getHairType());
		playerAppearance->setFace(preset->getFaceType());
		playerAppearance->setHeight(preset->getScale());
		player.getAccountData()->updateBoundingRadius();
	} else {
		// Java: LoggerFactory.getLogger(getClass())
		commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.templates.item.actions.CosmeticItemAction")
			.warn("Unhandled cosmetic item type: " + type);
		return;
	}
	dao::PlayerAppearanceDAO::store(player);
	if (targetItem == nullptr) // java-bug kept: Storage.delete(null) (proposed correction: delete parentItem)
		throw runtime::NullPointerException("Storage.delete(null)");
	player.getInventory().delete_(*targetItem);
	player.getController().onChangedPlayerAttributes();
}

} // namespace aion::gameserver::model::templates::item::actions
