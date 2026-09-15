#include "aion/gameserver/network/loginserver/serverpackets/SM_PTRANSFER_CONTROL.h"

#include <chrono>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/Emotion.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/EnumIds.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/services/transfers/TransferablePlayer.h"

namespace aion::gameserver::network::loginserver::serverpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.serverpackets.SM_PTRANSFER_CONTROL"));
	return *logger;
}

/** Java: Timestamp.getTime() */
int64_t millisOf(const commons::database::Timestamp& timestamp) {
	return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
}

void writeStones(commons::utils::ByteBuffer& buf, runtime::Ptr<runtime::RcTreeSet<runtime::Ref<model::items::ManaStone>>> itemStones) {
	std::vector<runtime::Ptr<model::items::ManaStone>> stones;
	for (runtime::Ptr<model::items::ManaStone> stone : *itemStones)
		stones.push_back(stone);
	buf.put(static_cast<int8_t>(stones.size()));
	for (runtime::Ptr<model::items::ManaStone> stone : stones) {
		buf.putInt(stone->getItemId());
		buf.putInt(stone->getSlot());
	}
}

} // namespace

using model::gameobjects::Item;
using model::gameobjects::player::Player;

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(int8_t typeValue, int32_t taskIdValue) : LsServerPacket(13), type(typeValue), taskId(taskIdValue) {
}

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(int8_t typeValue, services::transfers::TransferablePlayer& tp)
	: LsServerPacket(13), type(typeValue), player(tp.player.get()), taskId(tp.taskId.get()) {
}

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(int8_t typeValue, services::transfers::TransferablePlayer& tp, std::string_view resultValue)
	: LsServerPacket(13), type(typeValue), result(resultValue) {
	static_cast<void>(tp); // Java: neither taskId nor player is taken from tp here (the packet carries task id 0)
}

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(int8_t typeValue, int32_t taskIdValue, std::string_view resultValue)
	: LsServerPacket(13), type(typeValue), result(resultValue), taskId(taskIdValue) {
}

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(const SM_PTRANSFER_CONTROL& other) = default;

SM_PTRANSFER_CONTROL::SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL&& other) noexcept = default;

SM_PTRANSFER_CONTROL::~SM_PTRANSFER_CONTROL() = default;

void SM_PTRANSFER_CONTROL::writeImpl(LoginServerConnection* con, commons::utils::ByteBuffer& buf) {
	writeC(buf, type);
	switch (type) {
		case OK:
			writeD(buf, this->taskId);
			break;
		case ERROR_:
			writeD(buf, this->taskId);
			writeS(buf, this->result);
			break;
		case TASK_STOP:
			writeD(buf, this->taskId);
			writeS(buf, this->result);
			break;
		case CHARACTER_INFORMATION: {
			writeD(buf, this->taskId);
			writeS(buf, this->player->getName());
			writeD(buf, detail::classIdOf(this->player->getPlayerClass()));
			writeQ(buf, this->player->getCommonData()->getExp());
			writeD(buf, detail::raceIdOf(this->player->getRace()));
			writeD(buf, detail::genderIdOf(this->player->getCommonData()->getGender()));
			writeD(buf, this->player->getCommonData()->getTitleId());
			writeD(buf, this->player->getCommonData()->getDp());
			writeD(buf, this->player->getCommonData()->getQuestExpands());
			writeD(buf, this->player->getCommonData()->getNpcExpands());
			writeD(buf, this->player->getCommonData()->getItemExpands());
			writeD(buf, this->player->getCommonData()->getWhNpcExpands());
			runtime::Ptr<model::gameobjects::player::PlayerAppearance> playerAppearance = this->player->getPlayerAppearance();
			writeD(buf, playerAppearance->getSkinRGB());
			writeD(buf, playerAppearance->getHairRGB());
			writeD(buf, playerAppearance->getEyeRGB());
			writeD(buf, playerAppearance->getLipRGB());
			writeC(buf, playerAppearance->getFace());
			writeC(buf, playerAppearance->getHair());
			writeC(buf, playerAppearance->getDeco());
			writeC(buf, playerAppearance->getTattoo());
			writeC(buf, playerAppearance->getFaceContour());
			writeC(buf, playerAppearance->getExpression());
			writeC(buf, playerAppearance->getJawLine());
			writeC(buf, playerAppearance->getForehead());
			writeC(buf, playerAppearance->getEyeHeight());
			writeC(buf, playerAppearance->getEyeSpace());
			writeC(buf, playerAppearance->getEyeWidth());
			writeC(buf, playerAppearance->getEyeSize());
			writeC(buf, playerAppearance->getEyeShape());
			writeC(buf, playerAppearance->getEyeAngle());
			writeC(buf, playerAppearance->getBrowHeight());
			writeC(buf, playerAppearance->getBrowAngle());
			writeC(buf, playerAppearance->getBrowShape());
			writeC(buf, playerAppearance->getNose());
			writeC(buf, playerAppearance->getNoseBridge());
			writeC(buf, playerAppearance->getNoseWidth());
			writeC(buf, playerAppearance->getNoseTip());
			writeC(buf, playerAppearance->getCheek());
			writeC(buf, playerAppearance->getLipHeight());
			writeC(buf, playerAppearance->getMouthSize());
			writeC(buf, playerAppearance->getLipSize());
			writeC(buf, playerAppearance->getSmile());
			writeC(buf, playerAppearance->getLipShape());
			writeC(buf, playerAppearance->getJawHeigh());
			writeC(buf, playerAppearance->getChinJut());
			writeC(buf, playerAppearance->getEarShape());
			writeC(buf, playerAppearance->getHeadSize());
			writeC(buf, playerAppearance->getNeck());
			writeC(buf, playerAppearance->getNeckLength());
			writeC(buf, playerAppearance->getShoulderSize());
			writeC(buf, playerAppearance->getTorso());
			writeC(buf, playerAppearance->getChest()); // only woman
			writeC(buf, playerAppearance->getWaist());
			writeC(buf, playerAppearance->getHips());
			writeC(buf, playerAppearance->getArmThickness());
			writeC(buf, playerAppearance->getHandSize());
			writeC(buf, playerAppearance->getLegThickness());
			writeC(buf, playerAppearance->getFootSize());
			writeC(buf, playerAppearance->getFacialRate());
			writeC(buf, playerAppearance->getArmLength());
			writeC(buf, playerAppearance->getLegLength());
			writeC(buf, playerAppearance->getShoulders());
			writeC(buf, playerAppearance->getFaceShape());
			writeC(buf, playerAppearance->getVoice());
			writeF(buf, playerAppearance->getHeight());
			writeF(buf, this->player->getX());
			writeF(buf, this->player->getY());
			writeF(buf, this->player->getZ());
			writeC(buf, this->player->getHeading());
			writeD(buf, this->player->getWorldId());
		} break;
		case ITEMS_INFORMATION: {
			writeD(buf, this->taskId);
			// inventory
			std::vector<runtime::Ref<Item>> inv = dao::InventoryDAO::loadItems(player->getObjectId(), model::items::storage::StorageType::CUBE);
			for (runtime::Ref<Item>& item : dao::InventoryDAO::loadItems(player->getObjectId(), model::items::storage::StorageType::REGULAR_WAREHOUSE))
				inv.push_back(std::move(item));
			std::vector<runtime::Ptr<Item>> borrowed(inv.begin(), inv.end());
			dao::ItemStoneListDAO::load(borrowed);
			writeD(buf, static_cast<int32_t>(inv.size()));
			for (runtime::Ptr<Item> item : borrowed) {
				writeD(buf, item->getObjectId());
				writeD(buf, item->getItemId());
				writeQ(buf, item->getItemCount());
				writeD(buf, item->getItemColor().value_or(-1));
				writeS(buf, item->getItemCreator());
				writeD(buf, item->getExpireTime());
				writeD(buf, item->getActivationCount());
				writeC(buf, item->isEquipped() ? 1 : 0);
				writeC(buf, item->isSoulBound() ? 1 : 0);
				writeQ(buf, item->getEquipmentSlot());
				writeD(buf, item->getItemLocation());
				writeD(buf, item->getEnchantLevel());
				writeD(buf, item->getEnchantBonus());
				writeD(buf, item->getItemSkinTemplate()->getTemplateId());
				writeD(buf, item->getFusionedItemId());
				writeD(buf, item->getOptionalSockets());
				writeD(buf, item->getFusionedItemOptionalSockets());
				writeD(buf, item->getChargePoints());
				writeStones(buf, item->getItemStones());
				writeStones(buf, item->getFusionStones());
				writeD(buf, item->getGodStoneId());
				writeD(buf, item->getColorExpireTime());
				writeD(buf, item->getTuneCount());
				writeD(buf, item->getBonusStatsId());
				writeD(buf, item->getFusionedItemBonusStatsId());
				writeD(buf, item->getTempering());
				writeD(buf, item->getPackCount());
				writeC(buf, item->isAmplified() ? 1 : 0);
				writeH(buf, item->getBuffSkill());
			}
		} break;
		case DATA_INFORMATION: {
			writeD(buf, this->taskId);
			std::vector<runtime::Ptr<model::gameobjects::player::emotion::Emotion>> emotions = this->player->getEmotions()->getEmotions();
			writeD(buf, static_cast<int32_t>(emotions.size()));
			for (runtime::Ptr<model::gameobjects::player::emotion::Emotion> e : emotions) {
				writeD(buf, e->getId());
				writeD(buf, e->secondsUntilExpiration());
			}
			runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>>> motions =
				this->player->getMotions().getMotions();
			std::vector<runtime::Ptr<model::gameobjects::player::motion::Motion>> motionList;
			for (runtime::Ptr<model::gameobjects::player::motion::Motion> motion : motions->values())
				motionList.push_back(motion);
			writeD(buf, static_cast<int32_t>(motionList.size()));
			for (runtime::Ptr<model::gameobjects::player::motion::Motion> motion : motionList) {
				writeD(buf, motion->getId());
				writeD(buf, motion->getExpireTime());
				writeC(buf, motion->isActive() ? 1 : 0);
			}
			std::vector<runtime::Ptr<model::gameobjects::player::Macros::Macro>> macros = player->getMacros()->getAll();
			writeD(buf, static_cast<int32_t>(macros.size()));
			for (runtime::Ptr<model::gameobjects::player::Macros::Macro> m : macros) {
				writeD(buf, m->id());
				writeS(buf, m->xml());
			}
			std::vector<runtime::Ptr<model::gameobjects::player::npcFaction::NpcFaction>> factions = this->player->getNpcFactions().getNpcFactions();
			writeD(buf, static_cast<int32_t>(factions.size()));
			for (runtime::Ptr<model::gameobjects::player::npcFaction::NpcFaction> f : factions) {
				writeD(buf, f->getId());
				writeD(buf, f->getTime());
				writeC(buf, f->isActive() ? 1 : 0);
				writeS(buf, xml::enumName(f->getState()));
				writeD(buf, f->getQuestId());
			}
			std::vector<runtime::Ptr<model::gameobjects::player::PetCommonData>> pets = this->player->getPetList().getPets();
			writeD(buf, static_cast<int32_t>(pets.size()));
			for (runtime::Ptr<model::gameobjects::player::PetCommonData> pet : pets) {
				writeD(buf, pet->getTemplateId());
				writeD(buf, pet->getDecoration());
				std::optional<commons::database::Timestamp> birthday = pet->getBirthdayTimestamp();
				writeQ(buf, !birthday ? 0 : millisOf(*birthday));
				writeS(buf, pet->getName());
				writeD(buf, pet->getExpireTime()); // 26-08-2013 kid
			}
			std::vector<runtime::Ptr<model::gameobjects::player::title::Title>> titles = this->player->getTitleList().getTitles();
			writeD(buf, static_cast<int32_t>(titles.size()));
			for (runtime::Ptr<model::gameobjects::player::title::Title> t : titles) {
				writeD(buf, t->getId());
				writeD(buf, t->secondsUntilExpiration());
			}
			runtime::Ptr<model::gameobjects::player::PlayerSettings> ps = this->player->getPlayerSettings();
			runtime::Ptr<runtime::Array<int8_t>> uiSettings = ps->getUiSettings();
			runtime::Ptr<runtime::Array<int8_t>> shortcuts = ps->getShortcuts();
			writeD(buf, !uiSettings ? 0 : uiSettings->length());
			writeD(buf, !shortcuts ? 0 : shortcuts->length());
			if (uiSettings) {
				for (int8_t b : *uiSettings)
					writeC(buf, b);
			}
			if (shortcuts) {
				for (int8_t b : *shortcuts)
					writeC(buf, b);
			}
			writeD(buf, ps->getDeny());
			writeD(buf, ps->getDisplay());
		} break;
		case SKILL_INFORMATION: {
			writeD(buf, this->taskId);
			runtime::Ptr<model::skill::PlayerSkillList> skillList = this->player->getSkillList();
			// discard stigma skills
			std::vector<runtime::Ptr<model::skill::PlayerSkillEntry>> skills;
			for (runtime::Ptr<model::skill::PlayerSkillEntry> sk : skillList->getAllSkills()) {
				if (!sk->isStigmaSkill()) {
					skills.push_back(sk);
				}
			}
			writeD(buf, static_cast<int32_t>(skills.size()));
			for (runtime::Ptr<model::skill::PlayerSkillEntry> sk : skills) {
				writeD(buf, sk->getSkillId());
				writeD(buf, sk->getSkillLevel());
			}
		} break;
		case RECIPE_INFORMATION: {
			writeD(buf, this->taskId);
			runtime::Ptr<model::gameobjects::player::RecipeList> rec = this->player->getRecipeList();
			std::vector<int32_t> recipes = rec->getRecipeList().snapshot();
			writeD(buf, static_cast<int32_t>(recipes.size()));
			for (int32_t id : recipes) {
				writeD(buf, id);
			}
		} break;
		case QUEST_INFORMATION: {
			writeD(buf, this->taskId);
			runtime::Ptr<model::gameobjects::player::QuestStateList> qsl = this->player->getQuestStateList();
			std::vector<runtime::Ptr<questEngine::model::QuestState>> quests;
			for (runtime::Ptr<questEngine::model::QuestState> qs : qsl->getAllQuestState()) {
				if (!qs) {
					log().warn("there are null quest on player " + this->player->getName() + ". taskId #" + std::to_string(this->taskId) +
						". transfer skip that");
					continue;
				}
				quests.push_back(qs);
			}
			writeD(buf, static_cast<int32_t>(quests.size()));
			for (runtime::Ptr<questEngine::model::QuestState> qs : quests) {
				writeD(buf, qs->getQuestId());
				writeS(buf, xml::enumName(qs->getStatus()));
				writeD(buf, qs->getQuestVars()->getQuestVars());
				writeD(buf, qs->getCompleteCount());
				writeD(buf, qs->getRewardGroup().value_or(-1));
				writeQ(buf, millisOf(qs->getLastCompleteTime().value())); // Java NPE on null
				writeQ(buf, millisOf(qs->getNextRepeatTime().value()));
				writeD(buf, qs->getFlags());
			}
		} break;
	}
}

} // namespace aion::gameserver::network::loginserver::serverpackets
