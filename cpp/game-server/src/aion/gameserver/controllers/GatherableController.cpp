#include "aion/gameserver/controllers/GatherableController.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/ControllerSupport.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/gameobjects/Gatherable.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Rates.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/gather/ExMaterials.h"
#include "aion/gameserver/model/templates/gather/GatherableTemplate.h"
#include "aion/gameserver/model/templates/gather/Material.h"
#include "aion/gameserver/model/templates/gather/Materials.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GATHER_UPDATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/PunishmentService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/skillengine/task/GatheringTask.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/captcha/CAPTCHAUtil.h"
#include "aion/gameserver/world/geo/GeoService.h"

namespace aion::gameserver::controllers {

using configs::main::SecurityConfig;
using model::gameobjects::player::Player;
using model::templates::gather::GatherableTemplate;
using model::templates::gather::Material;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

GatherableController::GatherableController() = default;

GatherableController::~GatherableController() = default;

model::gameobjects::Gatherable& GatherableController::getOwner() const {
	return static_cast<model::gameobjects::Gatherable&>(VisibleObjectController::getOwner());
}

void GatherableController::startGathering(model::gameobjects::player::Player& player) {
	const GatherableTemplate* template_ = getOwner().getObjectTemplate();
	if (player.getLevel() < template_->getLevelLimit()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_GATHERING_B_LEVEL_CHECK(template_->getLevelLimit()));
		return;
	}
	if (player.isInPlayerMode(model::actions::PlayerMode::RIDE) && !player.hasPermission(configs::main::MembershipConfig::GATHERING_ALLOW_ON_MOUNT)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_GATHER_RESTRICTION_RIDE());
		return;
	}
	if (player.getInventory().isFull()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_INVENTORY_IS_FULL());
		return;
	}
	if (player.getController().isUnderStance()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_GATHER_WHILE_IN_CURRENT_STANCE());
		return;
	}
	if (!utils::PositionUtil::isInRange(getOwner(), player, 3, false)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_TOO_FAR_FROM_GATHER_SOURCE());
		return;
	}
	if (!world::geo::GeoService::getInstance().canSee(player, getOwner())) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_OBSTACLE_EXIST());
		return;
	}
	if (player.isGatherRestricted()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_REMAIN_RESTRICT_TIME(player.getGatherRestrictionDurationSeconds()));
		return;
	}

	if (!checkPlayerSkill(player, template_))
		return;

	const std::vector<Material>* materials = getMaterials(player, template_);
	if (materials == nullptr)
		return;

	// CAPTCHA
	if (SecurityConfig::CAPTCHA_ENABLE) {
		std::shared_ptr<const std::string> captchaAppear = SecurityConfig::CAPTCHA_APPEAR.get();
		if (*captchaAppear == template_->getSourceType() || *captchaAppear == "ALL") {
			int32_t rate = SecurityConfig::CAPTCHA_APPEAR_RATE;
			if (template_->getCaptchaRate() > 0)
				rate = detail::toInt(static_cast<float>(template_->getCaptchaRate()) * 0.1f);

			if (commons::utils::Rnd::chance() < static_cast<float>(rate)) {
				player.setCaptchaWord(utils::captcha::CAPTCHAUtil::getRandomWord());
				// Java: CAPTCHAUtil.createCAPTCHA(player.getCaptchaWord()).array() (NullPointerException for a null buffer)
				std::optional<commons::utils::ByteBuffer> captcha =
					utils::captcha::CAPTCHAUtil::createCAPTCHA(detail::unbox(player.getCaptchaWord(), "player.getCaptchaWord()"));
				if (!captcha)
					throw runtime::NullPointerException("CAPTCHAUtil.createCAPTCHA returned null");
				std::span<const uint8_t> bytes = std::as_const(*captcha).span();
				runtime::Ref<runtime::Array<int8_t>> image = runtime::Array<int8_t>::make(static_cast<int32_t>(bytes.size()));
				for (size_t i = 0; i < bytes.size(); ++i)
					(*image)[static_cast<int32_t>(i)] = static_cast<int8_t>(bytes[i]);
				player.setCaptchaImage(image);
				services::PunishmentService::setIsNotGatherable(player, 0, true, static_cast<int64_t>(SecurityConfig::CAPTCHA_EXTRACTION_BAN_TIME) * 1000);
			}
		}
	}

	int32_t chance = commons::utils::Rnd::nextInt(10000000);
	int32_t current = 0;
	const Material* curMaterial = nullptr;
	for (const Material& mat : *materials) {
		current = detail::add(current, mat.getRate());
		if (current >= chance) {
			curMaterial = &mat;
			break;
		}
	}

	SYNCHRONIZED(*this) {
		// lockdep: gatheringTask.get() reads the Field<Ref<GatheringTask>>, it is not a Future wait
		if (gatheringTask.get()) {
			// sends STR_EXTRACT_GATHER_OCCUPIED_BY_OTHER and makes the client deselect the targeted gatherable
			PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_GATHER_UPDATE(template_, curMaterial, 0, 0, 8, 0, 0));
			return;
		}
		int32_t skillLvlDiff = detail::sub(player.getSkillList()->getSkillLevel(template_->getHarvestSkill()), template_->getSkillLevel());
		runtime::Ref<skillengine::task::GatheringTask> task =
			skillengine::task::GatheringTask::create(player, getOwner(), curMaterial, skillLvlDiff);
		gatheringTask = task;
		task->start();
	}
}

bool GatherableController::checkPlayerSkill(model::gameobjects::player::Player& player,
	const model::templates::gather::GatherableTemplate* template_) {
	int32_t harvestSkillId = template_->getHarvestSkill();
	if (!player.getSkillList()->isSkillPresent(harvestSkillId)) {
		if (harvestSkillId == 30001) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_INCORRECT_SKILL());
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_LEARN_SKILL(
				detail::nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(harvestSkillId), "SKILL_DATA.getSkillTemplate(harvestSkillId)")
					->getL10n()));
		}
		return false;
	}
	if (player.getSkillList()->getSkillLevel(harvestSkillId) < template_->getSkillLevel()) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_GATHER_OUT_OF_SKILL_POINT(
			detail::nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(harvestSkillId), "SKILL_DATA.getSkillTemplate(harvestSkillId)")
				->getL10n()));
		return false;
	}
	return true;
}

const std::vector<model::templates::gather::Material>* GatherableController::getMaterials(model::gameobjects::player::Player& player,
	const model::templates::gather::GatherableTemplate* template_) {
	if (template_->getRequiredItemId() > 0) {
		if (template_->getCheckType() == 1) {
			bool hasRequiredItemEquipped = !player.getEquipment().getEquippedItemsByItemId(template_->getRequiredItemId()).empty();
			if (hasRequiredItemEquipped)
				return &detail::nonNull(template_->getExtraMaterials(), "template.getExtraMaterials()")->getMaterial();
		} else if (template_->getCheckType() == 2) {
			if (player.getInventory().getItemCountByItemId(template_->getRequiredItemId()) < template_->getEraseValue()) {
				std::string requiredItemL10n = utils::ChatUtil::l10n(template_->getRequiredItemNameId());
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_GATHERING_B_ITEM_CHECK(requiredItemL10n));
				return nullptr;
			}
			return &detail::nonNull(template_->getExtraMaterials(), "template.getExtraMaterials()")->getMaterial();
		}
	}
	return &detail::nonNull(template_->getMaterials(), "template.getMaterials()")->getMaterial();
}

void GatherableController::completeInteraction() {
	SYNCHRONIZED(*this) {
		gatheringTask = nullptr;
		if (++gatherCount == getOwner().getObjectTemplate()->getHarvestCount()) {
			if (getOwner().isInInstance())
				getOwner().getController().delete_();
			else
				getOwner().getController().deleteAndScheduleRespawn();
		}
	}
}

void GatherableController::rewardPlayer(runtime::Ptr<model::gameobjects::player::Player> player) {
	if (player) {
		int32_t skillLvl = getOwner().getObjectTemplate()->getSkillLevel();
		int32_t xpReward = detail::toInt((0.0031 * (skillLvl + 5.3) * (skillLvl + 1592.8) + 60));

		int32_t skillId = getOwner().getObjectTemplate()->getHarvestSkill();
		int32_t gainedGatherXp = calcResult(model::gameobjects::player::Rates::SKILL_XP_GATHERING, *player, xpReward);
		std::optional<model::stats::container::StatEnum> boostStat = detail::statEnumGetModifier(skillId);
		if (boostStat) // Java: gainedGatherXp *= current / 100f (int * float, narrowed back to int)
			gainedGatherXp = detail::toInt(static_cast<float>(gainedGatherXp) *
				(static_cast<float>(player->getGameStats()->getStat(*boostStat, 100.0f)->getCurrent()) / 100.0f));
		gainedGatherXp = std::max(1, gainedGatherXp);

		if (player->getSkillList()->addSkillXp(*player, skillId, gainedGatherXp, skillLvl)) {
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_EXTRACT_GATHERING_SUCCESS_GETEXP());
			player->getCommonData()->addExp(xpReward, model::gameobjects::player::Rates::XP_GATHERING);
		} else
			PacketSendUtility::sendPacket(*player, SM_SYSTEM_MESSAGE::STR_MSG_DONT_GET_PRODUCTION_EXP(
				detail::nonNull(dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId), "SKILL_DATA.getSkillTemplate(skillId)")->getL10n()));
	}
}

void GatherableController::onDespawn() {
	cancelGathering();
	VisibleObjectController::onDespawn();
}

void GatherableController::cancelGathering() {
	SYNCHRONIZED(*this) {
		// lockdep: gatheringTask.get() reads the Field<Ref<GatheringTask>>, it is not a Future wait
		runtime::Ptr<skillengine::task::GatheringTask> task = gatheringTask.get();
		if (!task)
			return;
		task->abort();
		gatheringTask = nullptr;
	}
}

int32_t GatherableController::getGatheringPlayerId() {
	// Java: synchronized (this) { return gatheringTask == null ? 0 : gatheringTask.getGathererId(); }
	int32_t gathererId = 0;
	SYNCHRONIZED(*this) {
		// lockdep: gatheringTask.get() reads the Field<Ref<GatheringTask>>, it is not a Future wait
		if (runtime::Ptr<skillengine::task::GatheringTask> task = gatheringTask.get())
			gathererId = task->getGathererId();
	}
	return gathererId;
}

} // namespace aion::gameserver::controllers
