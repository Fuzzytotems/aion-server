#include "Oracle.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <nlohmann/json.hpp>

#include "ChildProcess.h"

namespace aion::gameserver::scenario {

namespace {

using nlohmann::json;

std::string readFile(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		return {};
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

std::string environmentVariable(const char* name) {
	const char* value = std::getenv(name);
	return value == nullptr ? std::string() : std::string(value);
}

OracleSpot readSpot(const json& node) {
	OracleSpot spot;
	spot.npcId = node.value("npcId", 0);
	spot.x = node.value("x", 0.0f);
	spot.y = node.value("y", 0.0f);
	spot.z = node.value("z", 0.0f);
	spot.heading = node.value("h", 0);
	// A gatherable spot has no level: its template is a GatherableTemplate, so the oracle reports `"level": null` there (V2 only compares the
	// level of npcs it decoded from SM_NPC_INFO, and a gatherable is never one of them). "no level" and "level 0" must stay distinguishable:
	// an npc_template without a `level` attribute answers 0 (tools/oracle/m5a/spawns.py:186), and reading both as 0 made V2 assert
	// `level > 0` on a genuine level-0 npc instead of comparing it.
	if (const auto found = node.find("level"); found != node.end() && !found->is_null())
		spot.level = found->get<int32_t>();
	spot.spawned = node.value("spawned", true);
	spot.deterministic = node.value("deterministic", false);
	spot.distance = node.value("distance", 0.0);
	if (node.contains("flags")) {
		const json& flags = node.at("flags");
		spot.pool = flags.value("pool", false);
		spot.temporary = flags.value("temporary", false);
		spot.walker = flags.value("walker", false);
		spot.randomWalk = flags.value("randomWalk", false);
		spot.gatherable = flags.value("gatherable", false);
		spot.flag = flags.value("flag", false);
	}
	return spot;
}

} // namespace

OracleSpot Oracle::parseSpot(std::string_view spotJson) {
	return readSpot(json::parse(spotJson));
}

bool isPinnedToFixedSpots(const std::vector<OracleSpot>& spots, int32_t npcId) {
	bool any = false;
	for (const OracleSpot& spot : spots) {
		if (spot.npcId != npcId)
			continue;
		// A pool spot is NOT an excuse: a pooled group spawns `pool` of the coordinates the oracle already lists for that id
		// (SpawnEngine.java:160-168, resetPoolSpots then reserveRandomFreePoolSpot in a loop), so a pool npc always stands on one of them and
		// checkVisibility compares against every spot of the id. Only a walker or a random walker can legitimately be somewhere else.
		if (spot.walker || spot.randomWalk)
			return false;
		any = true;
	}
	return any;
}

std::vector<OracleItem> OracleCreation::equippedItems() const {
	std::vector<OracleItem> result;
	for (const OracleItem& item : items)
		if (item.equipped)
			result.push_back(item);
	return result;
}

Oracle::Oracle(std::filesystem::path pythonValue, std::filesystem::path scriptValue, std::filesystem::path workDirValue)
    : python(std::move(pythonValue)), script(std::move(scriptValue)), workDir(std::move(workDirValue)) {
	std::filesystem::create_directories(workDir);
}

std::optional<Oracle> Oracle::fromEnvironment(const std::filesystem::path& workDir) {
	const std::string python = environmentVariable("AION_TEST_PYTHON");
	if (python.empty() || !std::filesystem::is_regular_file(python))
		return std::nullopt;
	const std::filesystem::path script = std::filesystem::path(AION_SCENARIO_ORACLE_SCRIPT);
	if (!std::filesystem::is_regular_file(script))
		return std::nullopt;
	return Oracle(python, script, workDir);
}

std::string Oracle::run(const std::vector<std::string>& arguments) const {
	const std::string name = "oracle" + std::to_string(++runCounter);
	ChildProcess::Options options;
	options.executable = python;
	options.arguments = {script.string()};
	for (const std::string& argument : arguments)
		options.arguments.push_back(argument);
	options.workingDirectory = script.parent_path();
	options.logFile = workDir / (name + ".json");
	options.errorFile = workDir / (name + ".err.txt");
	std::string commandLine;
	for (const std::string& argument : options.arguments)
		commandLine += " " + argument;
	ChildProcess process(options);
	// the spawn oracle walks the whole spawn tree of a map, which takes minutes on a loaded machine
	const std::optional<int32_t> exitCode = process.waitForExit(std::chrono::minutes(20));
	const std::string output = readFile(options.logFile);
	const std::string errors = readFile(options.errorFile);
	if (!exitCode)
		throw std::runtime_error("oracle.py" + commandLine + " did not finish within 20 minutes");
	if (*exitCode != 0)
		throw std::runtime_error("oracle.py" + commandLine + " failed with exit code " + std::to_string(*exitCode) + ": " + errors);
	if (output.empty())
		throw std::runtime_error("oracle.py" + commandLine + " wrote no answer: " + errors);
	return output;
}

OracleCreation Oracle::creation(std::string_view race, std::string_view playerClass) const {
	const json answer = json::parse(run({"m5a-creation", "--race", std::string(race), "--class", std::string(playerClass)}));
	OracleCreation creation;
	const json& spawn = answer.at("spawn");
	creation.mapId = spawn.at("mapId").get<int32_t>();
	creation.x = spawn.at("x").get<float>();
	creation.y = spawn.at("y").get<float>();
	creation.z = spawn.at("z").get<float>();
	creation.heading = spawn.at("heading").get<int32_t>();
	for (const json& node : answer.at("items")) {
		OracleItem item;
		item.itemId = node.at("itemId").get<int32_t>();
		item.count = node.at("count").get<int64_t>();
		item.kinah = node.value("kinah", false);
		item.equipped = node.value("equipped", false);
		item.slot = node.value("slot", int64_t{0});
		creation.items.push_back(item);
	}
	for (const json& node : answer.at("skills")) {
		OracleSkill skill;
		skill.skillId = node.at("skillId").get<int32_t>();
		skill.level = node.at("level").get<int32_t>();
		creation.skills.push_back(skill);
	}
	creation.baseMaxHp = answer.at("baseStats").at("maxHp").get<int32_t>();
	creation.baseMaxMp = answer.at("baseStats").at("maxMp").get<int32_t>();
	return creation;
}

OracleSpawns Oracle::spawns(int32_t mapId, float x, float y, float z, int32_t gameHour, double radius) const {
	const json answer = json::parse(run({"m5a-spawns", "--map", std::to_string(mapId), "--x", std::to_string(x), "--y", std::to_string(y), "--z",
	                                     std::to_string(z), "--radius", std::to_string(radius), "--game-hour", std::to_string(gameHour)}));
	OracleSpawns spawns;
	spawns.mapId = answer.at("map").get<int32_t>();
	spawns.radius = answer.value("radius", 100.0);
	for (const json& node : answer.at("spots"))
		spawns.spots.push_back(readSpot(node));
	for (const json& node : answer.at("flagNpcs"))
		spawns.flagNpcs.push_back(readSpot(node));
	return spawns;
}

OracleBorderTarget Oracle::borderTarget(int32_t mapId, float x, float y, float z, int32_t gameHour) const {
	const json answer = json::parse(run({"m5a-border-target", "--map", std::to_string(mapId), "--x", std::to_string(x), "--y", std::to_string(y), "--z",
	                                     std::to_string(z), "--game-hour", std::to_string(gameHour)}));
	OracleBorderTarget target;
	target.mapId = answer.at("map").get<int32_t>();
	target.startX = answer.at("start")[0].get<float>();
	target.startY = answer.at("start")[1].get<float>();
	target.startZ = answer.at("start")[2].get<float>();
	target.targetX = answer.at("target")[0].get<float>();
	target.targetY = answer.at("target")[1].get<float>();
	target.targetZ = answer.at("target")[2].get<float>();
	target.distance = answer.value("distance", 0.0);
	for (const json& node : answer.at("appear"))
		target.appear.push_back(readSpot(node));
	for (const json& node : answer.at("disappear"))
		target.disappear.push_back(readSpot(node));
	return target;
}

} // namespace aion::gameserver::scenario
