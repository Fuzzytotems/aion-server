#include "aion/chatserver/model/channel/JobChannel.h"

#include <algorithm>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"

namespace aion::chatserver::model::channel {

namespace {

using AliasSet = std::vector<std::string_view>;

/** Java: aliasSets */
const std::vector<AliasSet>& aliasSets() {
	// Order of languages: NA English, GF English, German, Spanish, Italian, French, Polish, Turkish, Russian, Chinese, Korean
	static const auto* sets = new std::vector<AliasSet>{
		{"Gladiator", "Gladiador", "Gladiatore", "Gladiateur", "Gladyatör", "Гладиатор", "剑星", "검성"},
		{"Templar", "Templer", "Templario", "Templare", "Templier", "Templariusz", "Tapınakçı", "Страж", "守护星", "수호성"},
		{"Assassin", "Assassine", "Asesino", "Assassino", "Asasyn", "Suikastçı", "Убийца", "杀星", "살성"},
		{"Ranger", "Jäger", "Cazador", "Cacciatore", "Rôdeur", "Łowca", "Avcı", "Стрелок", "弓星", "궁성"},
		{"Sorcerer", "Zauberer", "Hechicero", "Fattucchiere", "Sorcier", "Czarodziej", "Sihirbaz", "Волшебник", "魔道星", "마도성"},
		{"Spiritmaster", "Beschwörer", "Invocador", "Incantatore", "Spiritualiste", "Zaklinacz", "Ruh Çağırıcı", "Заклинатель", "精灵星", "정령성"},
		{"Cleric", "Kleriker", "Clérigo", "Chierico", "Clerc", "Kleryk", "Ruhban", "Целитель", "治愈星", "치유성"},
		{"Chanter", "Kantor", "Cantor", "Cantore", "Aède", "Чародей", "护法星", "호법성"},
		{"Aethertech", "Äthertech", "Técnico del éter", "Tecnico dell'etere", "Éthertech", "EterTech", "Etertek", "Пилот", "机甲星", "기갑성"},
		{"Gunslinger", "Gunner", "Schütze", "Tirador", "Tiratore", "Pistolero", "Strzelec", "Nişancı", "Снайпер", "枪炮星", "사격성"},
		{"Songweaver", "Bard", "Barde", "Bardo", "Ozan", "Бард", "吟游星", "음유성"},
	}; // leaked: channels may be created while static objects are destroyed
	return *sets;
}

/** Java: classIdentifier.split("\\[f:")[0] */
std::string withoutFemaleSuffix(std::string_view classIdentifier) {
	std::vector<std::string> parts = commons::utils::StringUtils::splitJava(classIdentifier, "[f:");
	if (parts.empty())
		throw commons::utils::IndexOutOfBoundsException("Index 0 out of bounds for length 0");
	return parts[0];
}

} // namespace

JobChannel::JobChannel(int32_t gameServerId, std::optional<Race> race, std::string_view classIdentifier)
	: RaceChannel(ChannelType::JOB, gameServerId, race), classIdentifiers(withAliases(withoutFemaleSuffix(classIdentifier))) {
}

// parameters renamed from the Java names, which would hide members (see RaceChannel::matches)
bool JobChannel::matches(std::optional<ChannelType> type, int32_t gsId, std::optional<Race> requestedRace, std::string_view requestedClass) const {
	return RaceChannel::matches(type, gsId, requestedRace, requestedClass) && std::ranges::find(classIdentifiers, requestedClass) != classIdentifiers.end();
}

std::string JobChannel::name() const {
	return classIdentifiers.front() + " (" + raceInitial() + ")";
}

std::vector<std::string> JobChannel::withAliases(std::string_view classIdentifier) {
	for (const AliasSet& aliases : aliasSets()) {
		if (std::ranges::find(aliases, classIdentifier) != aliases.end())
			return std::vector<std::string>(aliases.begin(), aliases.end());
	}
	return {std::string(classIdentifier)};
}

} // namespace aion::chatserver::model::channel
