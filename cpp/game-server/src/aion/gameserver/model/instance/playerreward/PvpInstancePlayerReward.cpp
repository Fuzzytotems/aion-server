#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"

#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::instance::playerreward {

// Java PvpInstancePlayerReward.java:28-31
PvpInstancePlayerReward::PvpInstancePlayerReward(int32_t objectId, Race raceValue) : InstancePlayerReward(objectId), race(raceValue) {
}

PvpInstancePlayerReward::~PvpInstancePlayerReward() = default;

runtime::Ref<PvpInstancePlayerReward> PvpInstancePlayerReward::create(int32_t objectId, Race race) {
	return runtime::makeRef<PvpInstancePlayerReward>(objectId, race);
}

int32_t PvpInstancePlayerReward::valueOf(const runtime::Field<runtime::Ref<runtime::Array<int32_t>>>& array, int32_t index) {
	runtime::Ptr<runtime::Array<int32_t>> values = array.get();
	return values == nullptr ? 0 : values->get(index);
}

// Java PvpInstancePlayerReward.java:45-47
void PvpInstancePlayerReward::setReward1(int32_t itemId, int32_t itemCount, int32_t bonusCount) {
	reward1.set(runtime::Array<int32_t>::of({itemId, itemCount, bonusCount}));
}

// Java PvpInstancePlayerReward.java:61-63
void PvpInstancePlayerReward::setReward2(int32_t itemId, int32_t itemCount, int32_t bonusCount) {
	reward2.set(runtime::Array<int32_t>::of({itemId, itemCount, bonusCount}));
}

// Java PvpInstancePlayerReward.java:73-75
void PvpInstancePlayerReward::setReward3(int32_t itemId, int32_t itemCount) {
	reward3.set(runtime::Array<int32_t>::of({itemId, itemCount}));
}

// Java PvpInstancePlayerReward.java:85-87
void PvpInstancePlayerReward::setReward4(int32_t itemId, int32_t itemCount) {
	reward4.set(runtime::Array<int32_t>::of({itemId, itemCount}));
}

// Java PvpInstancePlayerReward.java:97-99
void PvpInstancePlayerReward::setBonusReward(int32_t itemId, int32_t itemCount) {
	bonusReward.set(runtime::Array<int32_t>::of({itemId, itemCount}));
}

// Java PvpInstancePlayerReward.java:145-200
int32_t PvpInstancePlayerReward::getMythicKunaxEquipment(gameobjects::player::Player& player) {
	std::vector<int32_t> weapons;
	std::vector<int32_t> armor;
	switch (player.getPlayerClass()) { // armor: Chest, Gloves, Shoulders, Pants, Shoes, Helmet
		case PlayerClass::TEMPLAR:
			weapons = {100901305, 100001682, 115001702}; // Greatsword, Sword, Shield
			armor = {110601549, 111601512, 112601494, 113601495, 114601502, 125003995};
			break;
		case PlayerClass::GLADIATOR:
			weapons = {100901305, 101301215, 101701319}; // Greatsword, Polearm, Bow
			armor = {110601549, 111601512, 112601494, 113601495, 114601502, 125003995};
			break;
		case PlayerClass::RANGER:
			weapons = {101701319}; // Bow
			armor = {110301751, 111301689, 112301628, 113301720, 114301757, 125003997};
			break;
		case PlayerClass::ASSASSIN:
			weapons = {100001682, 100201455, 101701319}; // Sword, Dagger, Bow
			armor = {110301751, 111301689, 112301628, 113301720, 114301757, 125003997};
			break;
		case PlayerClass::GUNNER:
			weapons = {101801170, 101801170, 101901081}; // Pistol, Pistol, Cannon
			armor = {110301751, 111301689, 112301628, 113301720, 114301757, 125003997};
			break;
		case PlayerClass::SORCERER:
			weapons = {100601378, 100601378, 100501268}; // Tome, Tome, Orb
			armor = {110101754, 111101579, 112101529, 113101591, 114101625, 125003998};
			break;
		case PlayerClass::SPIRIT_MASTER:
			weapons = {100601378, 100501268, 100501268}; // Tome, Orb, Orb
			armor = {110101754, 111101579, 112101529, 113101591, 114101625, 125003998};
			break;
		case PlayerClass::BARD:
			weapons = {102001197};
			armor = {110101754, 111101579, 112101529, 113101591, 114101625, 125003998};
			break;
		case PlayerClass::CLERIC:
			weapons = {101501304, 100101281, 115001702}; // Staff, Mace, Shield
			armor = {110551084, 111501643, 112501582, 113501661, 114501671, 125003996};
			break;
		case PlayerClass::CHANTER:
			weapons = {101501304, 101501304, 100101281, 115001702}; // Staff, Staff, Mace, Shield
			armor = {110551084, 111501643, 112501582, 113501661, 114501671, 125003996};
			break;
		case PlayerClass::RIDER:
			weapons = {102100969};
			armor = {110551084, 111501643, 112501582, 113501661, 114501671, 125003996};
			break;
		default:
			commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.instance.playerreward.PvpInstancePlayerReward")
				.warn("Couldn't get mythic Kunax equipment for " + player.toString() + ". Rewards for " +
					std::string(xml::enumName(player.getPlayerClass())) + " are not implemented");
			return 0;
	}
	const std::vector<int32_t>& possibleRewards = commons::utils::Rnd::chance() < 17.5f ? weapons : armor;
	return commons::utils::Rnd::get(possibleRewards);
}

} // namespace aion::gameserver::model::instance::playerreward
