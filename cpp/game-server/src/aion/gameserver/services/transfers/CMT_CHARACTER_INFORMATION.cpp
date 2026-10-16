#include "aion/gameserver/services/transfers/CMT_CHARACTER_INFORMATION.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "aion/commons/database/DateTimeValue.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/PlayerBindPointDAO.h"
#include "aion/gameserver/dao/PlayerNpcFactionsDAO.h"
#include "aion/gameserver/dao/PlayerTitleListDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Macros.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/emotion/EmotionList.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/ENpcFactionQuestState.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFaction.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"
#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/items/storage/StorageTypeInfo.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/item/ItemSocketService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/EnumValueOf.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::services::transfers {

namespace {

using configs::main::PlayerTransferConfig;
using model::gameobjects::player::Player;

/** Java Float.parseFloat: surrounding whitespace ignored, NumberFormatException for anything else than a decimal float */
float parseFloat(std::string_view s) {
	const std::string trimmed(commons::utils::StringUtils::trim(s));
	float value = 0;
	const char* end = trimmed.data() + trimmed.size();
	auto [ptr, ec] = std::from_chars(trimmed.data(), end, value);
	if (trimmed.empty() || ec != std::errc() || ptr != end)
		throw commons::utils::NumberFormatException("For input string: \"" + std::string(s) + "\"");
	return value;
}

/** Java Byte.parseByte: Integer.parseInt in the byte range, NumberFormatException outside it */
int8_t parseByte(std::string_view s) {
	const int32_t value = commons::utils::parseInt(s);
	if (value < -128 || value > 127)
		throw commons::utils::NumberFormatException("Value out of range. Value:\"" + std::string(s) + "\" Radix:10");
	return static_cast<int8_t>(value);
}

/** Java byte[] for PlayerSettings: null for a length of 0 */
runtime::Ptr<runtime::Array<int8_t>> toArray(const std::vector<uint8_t>& bytes) {
	runtime::Ref<runtime::Array<int8_t>> array = runtime::Array<int8_t>::make(static_cast<int32_t>(bytes.size()));
	for (size_t i = 0; i < bytes.size(); ++i)
		(*array)[static_cast<int32_t>(i)] = static_cast<int8_t>(bytes[i]);
	return array;
}

} // namespace

// Java CMT_CHARACTER_INFORMATION.java:47-49: super(byteBuffer, 0); the buffer is PlayerTransfer.getDB's, little endian
CMT_CHARACTER_INFORMATION::CMT_CHARACTER_INFORMATION(commons::utils::ByteBuffer byteBuffer) : BaseClientPacket(std::move(byteBuffer), 0) {
}

// Java CMT_CHARACTER_INFORMATION.java:63-396
runtime::Ptr<Player> CMT_CHARACTER_INFORMATION::readInfo(std::string_view name, int32_t targetAccount, std::string_view accountName,
	const std::vector<int32_t>& rsList, const commons::logging::Logger& textLog) {
	int64_t st = commons::utils::currentTimeMillis();
	runtime::Ref<model::gameobjects::player::PlayerCommonData> playerCommonData =
		model::gameobjects::player::PlayerCommonData::create(utils::idfactory::IDFactory::getInstance().nextId());
	playerCommonData->setName(name);
	// read common data
	playerCommonData->setPlayerClass(model::getPlayerClassById(static_cast<int8_t>(readD())));
	playerCommonData->setExp(readQ());
	playerCommonData->setRace(readD() == 0 ? model::Race::ELYOS : model::Race::ASMODIANS);
	playerCommonData->setGender(readD() == 0 ? model::Gender::MALE : model::Gender::FEMALE);
	playerCommonData->setTitleId(readD());
	playerCommonData->setDp(readD());
	playerCommonData->setQuestExpands(readD());
	playerCommonData->setNpcExpands(readD());
	playerCommonData->setItemExpands(readD());
	playerCommonData->setWhNpcExpands(readD());

	runtime::Ref<model::gameobjects::player::PlayerAppearance> playerAppearance = model::gameobjects::player::PlayerAppearance::create();
	playerAppearance->setSkinRGB(readD());
	playerAppearance->setHairRGB(readD());
	playerAppearance->setEyeRGB(readD());
	playerAppearance->setLipRGB(readD());
	playerAppearance->setFace(readUC());
	playerAppearance->setHair(readUC());
	playerAppearance->setDeco(readUC());
	playerAppearance->setTattoo(readUC());
	playerAppearance->setFaceContour(readUC());
	playerAppearance->setExpression(readUC());
	playerAppearance->setJawLine(readUC());
	playerAppearance->setForehead(readUC());
	playerAppearance->setEyeHeight(readUC());
	playerAppearance->setEyeSpace(readUC());
	playerAppearance->setEyeWidth(readUC());
	playerAppearance->setEyeSize(readUC());
	playerAppearance->setEyeShape(readUC());
	playerAppearance->setEyeAngle(readUC());
	playerAppearance->setBrowHeight(readUC());
	playerAppearance->setBrowAngle(readUC());
	playerAppearance->setBrowShape(readUC());
	playerAppearance->setNose(readUC());
	playerAppearance->setNoseBridge(readUC());
	playerAppearance->setNoseWidth(readUC());
	playerAppearance->setNoseTip(readUC());
	playerAppearance->setCheek(readUC());
	playerAppearance->setLipHeight(readUC());
	playerAppearance->setMouthSize(readUC());
	playerAppearance->setLipSize(readUC());
	playerAppearance->setSmile(readUC());
	playerAppearance->setLipShape(readUC());
	playerAppearance->setJawHeigh(readUC());
	playerAppearance->setChinJut(readUC());
	playerAppearance->setEarShape(readUC());
	playerAppearance->setHeadSize(readUC());
	playerAppearance->setNeck(readUC());
	playerAppearance->setNeckLength(readUC());
	playerAppearance->setShoulderSize(readUC());
	playerAppearance->setTorso(readUC());
	playerAppearance->setChest(readUC());
	playerAppearance->setWaist(readUC());
	playerAppearance->setHips(readUC());
	playerAppearance->setArmThickness(readUC());
	playerAppearance->setHandSize(readUC());
	playerAppearance->setLegThickness(readUC());
	playerAppearance->setFootSize(readUC());
	playerAppearance->setFacialRate(readUC());
	playerAppearance->setArmLength(readUC());
	playerAppearance->setLegLength(readUC());
	playerAppearance->setShoulders(readUC());
	playerAppearance->setFaceShape(readUC());
	playerAppearance->setVoice(readUC());
	playerAppearance->setHeight(readF());

	// Java: new PlayerAccountData(playerCommonData, playerAppearance) before AccountService.loadAccount(targetAccount); the C++ part is
	// constructed with its owner, so the account comes first. Java never adds it to the account: the part is retired to the Reclaimer at every
	// exit (destroyed once no Ref holds it), as CM_CREATE_CHARACTER retires one it did not add
	runtime::Ref<model::account::Account> account = AccountService::loadAccount(targetAccount);
	auto accPlData = std::make_unique<model::account::PlayerAccountData>(*account, *playerCommonData, *playerAppearance);
	auto retireAccountData = runtime::finally([&accPlData, &account]() noexcept {
		if (accPlData)
			runtime::Reclaimer::retirePart(*account, std::move(accPlData));
	});
	account->setName(accountName);
	runtime::Ref<Player> player = player::PlayerService::newPlayer(*accPlData, *account);
	float x = readF();
	float y = readF();
	float z = readF();
	int8_t h = readC();
	int32_t worldId = readD();
	runtime::Ref<world::WorldPosition> pos = world::World::getInstance().createPosition(worldId, x, y, z, h, 1);
	player->setPosition(pos);

	if (!player::PlayerService::storeNewPlayer(*player, accountName, targetAccount)) {
		textLog.info("failed to store new player to " + std::string(accountName));
		utils::idfactory::IDFactory::getInstance().releaseId(playerCommonData->getPlayerObjId());
		return nullptr;
	}
	// read items data
	int32_t cnt = readD();
	std::string sb;
	for (int32_t a = 0; a < cnt; a++) { // inventory
		int32_t objIdOld = readD();
		int32_t itemId = readD();
		int64_t itemCnt = readQ();
		std::optional<int32_t> itemColor = readD();
		if (itemColor == -1)
			itemColor = std::nullopt;

		std::string itemCreator = readS();
		int32_t itemExpireTime = readD();
		int32_t itemActivationCnt = readD();
		bool itemEquipped = readC() == 1;

		bool itemSoulBound = readC() == 1;
		int64_t equipSlot = readQ();
		int32_t location = readD();
		int32_t enchant = readD();
		int32_t enchantBonus = readD();

		int32_t skinId = readD();
		int32_t fusionId = readD();
		int32_t optSocket = readD();
		int32_t optFusion = readD();

		int32_t charge = readD();
		std::vector<std::array<int32_t, 2>> manastones, fusions;
		int8_t len = readC();
		for (int8_t b = 0; b < len; b++) {
			int32_t stoneId = readD(); // Java: new int[] { readD(), readD() } evaluates left to right
			manastones.push_back({stoneId, readD()});
		}
		len = readC();
		for (int8_t b = 0; b < len; b++) {
			int32_t stoneId = readD();
			fusions.push_back({stoneId, readD()});
		}
		int32_t godstone = readD();
		int32_t colorExpires = readD();
		int32_t tuneCount = readD();
		int32_t bonusStatsId = readD();
		int32_t fusionedItemBonusStatsId = readD();
		int32_t tempering = readD();
		int32_t packCount = readD();
		bool itemAmplified = readUC() == 1;
		int32_t buffSkill = readUH();
		if (!(location == model::items::storage::getId(model::items::storage::StorageType::CUBE) && PlayerTransferConfig::ALLOW_INV.load()
				|| location == model::items::storage::getId(model::items::storage::StorageType::REGULAR_WAREHOUSE) &&
					   PlayerTransferConfig::ALLOW_WAREHOUSE.load())) {
			continue;
		}
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(itemId);
		if (itemTemplate == nullptr) {
			textLog.warn("(accId=" + std::to_string(targetAccount) + ") item with id " + std::to_string(itemId) + " was not found in templates");
			continue;
		}

		if (itemTemplate->isStigma() && !PlayerTransferConfig::ALLOW_STIGMA.load()) {
			continue;
		}

		int32_t newId = utils::idfactory::IDFactory::getInstance().nextId();
		// bonus probably is lost, don't know [RR]
		// dye expiration is lost
		// plume Bonus is lost
		runtime::Ref<model::gameobjects::Item> item = model::gameobjects::Item::create(newId, itemId, itemCnt, itemColor, colorExpires, itemCreator,
			itemExpireTime, itemActivationCnt, itemEquipped, itemSoulBound, equipSlot, location, enchant, enchantBonus, skinId, fusionId, optSocket,
			optFusion, charge, tuneCount, bonusStatsId, fusionedItemBonusStatsId, tempering, packCount, itemAmplified, buffSkill, 0);
		if (!manastones.empty())
			for (const std::array<int32_t, 2>& stone : manastones)
				item::ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item>(item), stone[0], stone[1], false);

		if (!fusions.empty())
			for (const std::array<int32_t, 2>& stone : fusions)
				item::ItemSocketService::addManaStone(runtime::Ptr<model::gameobjects::Item>(item), stone[0], stone[1], true);

		if (godstone != 0)
			item->addGodStone(godstone);

		sb += "\n(old objId=" + std::to_string(objIdOld) + ") -> " + item->toString();
		item->setPersistentState(model::gameobjects::Persistable::PersistentState::NEW);
		player->getInventory().add_CharacterTransfer(*item);
	}
	dao::InventoryDAO::store(*player);

	textLog.info(sb);

	// read data
	cnt = readD();
	textLog.info("EmotionList:" + std::to_string(cnt));
	player->setEmotions(std::make_unique<model::gameobjects::player::emotion::EmotionList>(*player));
	for (int32_t a = 0; a < cnt; a++) { // emotes
		int32_t id = readD(), remainTime = readD();

		if (PlayerTransferConfig::ALLOW_EMOTIONS.load())
			player->getEmotions()->add(id, remainTime, true);
	}

	cnt = readD();
	textLog.info("MotionList:" + std::to_string(cnt));
	player->setMotions(std::make_unique<model::gameobjects::player::motion::MotionList>(*player));
	for (int32_t i = 0; i < cnt; i++) { // motions
		int32_t id = readD(), expiryTime = readD();
		bool active = readC() == 1;

		if (PlayerTransferConfig::ALLOW_MOTIONS.load())
			player->getMotions().add(*model::gameobjects::player::motion::Motion::create(id, expiryTime, active), true);
	}

	cnt = readD();
	textLog.info("Macros:" + std::to_string(cnt));
	player->setMacros(model::gameobjects::player::Macros::create());
	for (int32_t a = 0; a < cnt; a++) { // macros
		int32_t id = readD();
		std::string xml = readS();

		if (PlayerTransferConfig::ALLOW_MACRO.load())
			player::PlayerService::addMacro(*player, id, xml);
	}

	cnt = readD();
	textLog.info("NpcFactions:" + std::to_string(cnt));
	player->setNpcFactions(std::make_unique<model::gameobjects::player::npcFaction::NpcFactions>(*player));
	for (int32_t a = 0; a < cnt; a++) { // npc factions
		int32_t id = readD(), time = readD();
		bool active = readC() == 1;
		std::string state = readS();
		int32_t questId = readD();

		if (PlayerTransferConfig::ALLOW_NPCFACTIONS.load())
			player->getNpcFactions().addNpcFaction(*model::gameobjects::player::npcFaction::NpcFaction::create(id, time, active,
				utils::enumValueOf<model::gameobjects::player::npcFaction::ENpcFactionQuestState>(state), questId));
	}
	if (cnt > 0 && PlayerTransferConfig::ALLOW_NPCFACTIONS.load())
		dao::PlayerNpcFactionsDAO::storeNpcFactions(*player);

	cnt = readD();
	textLog.info("Pets:" + std::to_string(cnt));
	for (int32_t i = 0; i < cnt; i++) { // pets
		int32_t petId = readD();
		int32_t decorationId = readD();
		int64_t bday = readQ();
		std::string petname = readS();
		int32_t expiryTime = readD();

		if (PlayerTransferConfig::ALLOW_PETS.load()) {
			if (bday == 0)
				bday = commons::utils::currentTimeMillis();

			player->getPetList().addPet(*player, petId, decorationId, bday, petname, expiryTime);
		}
	}

	cnt = readD();
	textLog.info("TitleList:" + std::to_string(cnt));
	player->setTitleList(std::make_unique<model::gameobjects::player::title::TitleList>());
	for (int32_t a = 0; a < cnt; a++) { // titles
		int32_t id = readD(), remainTime = readD();

		if (PlayerTransferConfig::ALLOW_TITLES.load())
			player->getTitleList().addEntry(id, remainTime);
	}
	if (cnt > 0 && PlayerTransferConfig::ALLOW_TITLES.load())
		for (const runtime::Ptr<model::gameobjects::player::title::Title>& t : player->getTitleList().getTitles()) {
			dao::PlayerTitleListDAO::storeTitles(*player, *t);
		}

	std::vector<std::string> posBind;
	switch (player->getRace()) {
		case model::Race::ELYOS:
			posBind = commons::utils::StringUtils::splitJava(*PlayerTransferConfig::BIND_ELYOS.get(), " ");
			break;
		default:
			posBind = commons::utils::StringUtils::splitJava(*PlayerTransferConfig::BIND_ASMO.get(), " ");
			break;
	}
	if (posBind.size() < 5) // Java: posBind[4] of a shorter split
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(std::min<size_t>(posBind.size(), 4)) + " out of bounds for length " +
												 std::to_string(posBind.size()));

	player->setBindPoint(model::gameobjects::player::BindPointPosition::create(commons::utils::parseInt(posBind[0]), parseFloat(posBind[1]),
		parseFloat(posBind[2]), parseFloat(posBind[3]), parseByte(posBind[4])));
	dao::PlayerBindPointDAO::store(*player);

	int32_t uilen = readD(), shortlen = readD();
	std::vector<uint8_t> ui = readB(uilen), sc = readB(shortlen);
	int32_t deny = readD(), penalty = readD();
	player->setPlayerSettings(model::gameobjects::player::PlayerSettings::create(uilen > 0 ? toArray(ui) : nullptr, shortlen > 0 ? toArray(sc) : nullptr,
		nullptr, deny, penalty));
	player->setAbyssRank(model::gameobjects::player::AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0));

	// read skill data
	cnt = readD();
	textLog.info("PlayerSkillList:" + std::to_string(cnt));
	player->setSkillList(model::skill::PlayerSkillList::create());
	bool rsCheck = !rsList.empty();
	for (int32_t a = 0; a < cnt; a++) { // skills
		int32_t skillId = readD();
		int32_t skillLvl = readD();

		if (rsCheck && std::ranges::find(rsList, skillId) != rsList.end())
			continue;

		const skillengine::model::SkillTemplate* temp = dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
		if (temp == nullptr) {
			textLog.error("null skillid:" + std::to_string(skillId) + " name:" + std::string(name)); // Java: String.format("null skillid:%d name:%s")
			continue;
		}

		if (!PlayerTransferConfig::ALLOW_SKILLS.load()) {
			if (temp->isPassive())
				player->getSkillList()->addSkill(*player, skillId, skillLvl);
		} else
			player->getSkillList()->addSkill(*player, skillId, skillLvl);
	}

	// read recipe data
	cnt = readD();
	textLog.info("RecipeList:" + std::to_string(cnt));
	player->setRecipeList(model::gameobjects::player::RecipeList::create());
	for (int32_t a = 0; a < cnt; a++) { // recipes
		int32_t recipeId = readD();

		if (PlayerTransferConfig::ALLOW_RECIPES.load())
			player->getRecipeList()->addRecipe(*player, recipeId);
	}

	// read quest data
	cnt = readD();
	textLog.info("QuestStateList:" + std::to_string(cnt));
	player->setQuestStateList(model::gameobjects::player::QuestStateList::create());
	for (int32_t a = 0; a < cnt; a++) { // quests
		int32_t questId = readD();
		std::string status = readS();
		int32_t qvars = readD(), completeCount = readD(), reward = readD();
		commons::database::Timestamp completeTime{std::chrono::milliseconds(readQ())};
		commons::database::Timestamp nextRepeatTime{std::chrono::milliseconds(readQ())};
		int32_t flags = readD();

		if (PlayerTransferConfig::ALLOW_QUESTS.load()) {
			player->getQuestStateList()->addQuest(questId,
				*questEngine::model::QuestState::create(questId, utils::enumValueOf<questEngine::model::QuestStatus>(status), qvars, flags, completeCount,
					nextRepeatTime, reward == -1 ? std::nullopt : std::optional<int32_t>(reward), completeTime));
		}
	}

	player::PlayerService::storePlayer(*player);
	textLog.info("finished in " + std::to_string(commons::utils::currentTimeMillis() - st) + " ms");
	return player;
}

} // namespace aion::gameserver::services::transfers
