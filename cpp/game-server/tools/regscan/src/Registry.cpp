#include "Registry.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>
#include <tuple>

#include "SpawnIds.h"

namespace aion::gameserver::tools::regscan {

namespace fs = std::filesystem;

namespace {

constexpr std::string_view HANDLERS_DIR = "aion/gameserver/handlers";
constexpr std::string_view CLIENT_PACKETS_DIR = "aion/gameserver/network/aion/clientpackets";

std::optional<std::string> readFile(const fs::path& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return std::nullopt;
	std::ostringstream content;
	content << in.rdbuf();
	if (in.bad())
		return std::nullopt;
	return content.str();
}

void rootError(std::vector<Diagnostic>& errors, const fs::path& path, std::string message) {
	errors.push_back(Diagnostic{fs::path(path).make_preferred().string(), 0, 0, std::move(message)});
}

/** Collects the files with the given extensions below root/subdir, sorted by their path relative to root. */
std::vector<SourceFile> collect(const fs::path& root, std::string_view subdir, bool cpp, std::vector<Diagnostic>& errors) {
	std::vector<SourceFile> files;
	fs::path dir = subdir.empty() ? root : root / fs::path(subdir);
	std::error_code ec;
	if (!fs::is_directory(dir, ec)) {
		rootError(errors, dir, "directory not found");
		return files;
	}
	std::vector<std::pair<std::string, fs::path>> found;
	for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		if (!it->is_regular_file(ec))
			continue;
		const fs::path& path = it->path();
		std::string extension = path.extension().string();
		std::string relPath = fs::relative(path, root, ec).generic_string();
		if (cpp) {
			if (extension == ".hpp" || extension == ".cc" || extension == ".cxx" || extension == ".inl" || extension == ".ipp" || extension == ".hh") {
				rootError(errors, path, "unsupported C++ file extension " + extension + " (use .cpp and .h)");
				continue;
			}
			if (extension != ".cpp" && extension != ".h")
				continue;
		} else if (extension != ".java") {
			continue;
		}
		found.emplace_back(relPath, path);
	}
	if (ec)
		rootError(errors, dir, "cannot list directory: " + ec.message());
	std::ranges::sort(found, {}, &std::pair<std::string, fs::path>::first);
	for (auto& [relPath, path] : found) {
		auto content = readFile(path);
		if (!content) {
			rootError(errors, path, "cannot read file");
			continue;
		}
		files.push_back(SourceFile{fs::path(path).make_preferred().string(), relPath, std::move(*content)});
	}
	return files;
}

std::string firstSegment(std::string_view path) {
	size_t slash = path.find('/');
	return std::string(slash == std::string_view::npos ? std::string_view() : path.substr(0, slash));
}

std::string_view functionSuffix(MarkerKind kind) noexcept {
	switch (kind) {
		case MarkerKind::AI:
			return "_aiFactory";
		case MarkerKind::INSTANCE_HANDLER:
			return "_instanceFactory";
		case MarkerKind::ZONE_HANDLER:
			return "_zoneFactory";
		case MarkerKind::QUEST_HANDLER:
			return "_questFactory";
		case MarkerKind::ADMIN_COMMAND:
		case MarkerKind::PLAYER_COMMAND:
		case MarkerKind::CONSOLE_COMMAND:
			return "_commandFactory";
		case MarkerKind::CLIENT_PACKET:
			return "_clientPacketFactory";
	}
	return {};
}

std::string location(const Marker& marker) {
	return marker.file + "(" + std::to_string(marker.line) + ")";
}

std::vector<std::string> splitZoneNames(std::string_view names) {
	std::vector<std::string> result;
	while (!names.empty()) {
		size_t space = names.find(' ');
		result.emplace_back(names.substr(0, space));
		names = space == std::string_view::npos ? std::string_view() : names.substr(space + 1);
	}
	return result;
}

std::optional<CommandKind> commandKindOf(MarkerKind kind) noexcept {
	switch (kind) {
		case MarkerKind::ADMIN_COMMAND:
			return CommandKind::ADMIN;
		case MarkerKind::PLAYER_COMMAND:
			return CommandKind::PLAYER;
		case MarkerKind::CONSOLE_COMMAND:
			return CommandKind::CONSOLE;
		default:
			return std::nullopt;
	}
}

std::string_view commandKindName(CommandKind kind) noexcept {
	switch (kind) {
		case CommandKind::ADMIN:
			return "AdminCommand";
		case CommandKind::PLAYER:
			return "PlayerCommand";
		case CommandKind::CONSOLE:
			return "ConsoleCommand";
	}
	return {};
}

class Builder {
public:
	explicit Builder(const ScanInput& input) : input(input) {}

	RegistryModel run() {
		scanCpp();
		checkTypesAndClasses();
		collectEntries();
		if (input.javaHandlersGiven)
			scanJava();
		if (input.javaClientPacketFactory)
			scanJavaClientPackets();
		buildReport();
		std::sort(model.errors.begin(), model.errors.end());
		model.errors.erase(std::unique(model.errors.begin(), model.errors.end()), model.errors.end());
		return std::move(model);
	}

private:
	void error(const Marker& marker, std::string message) { model.errors.push_back(Diagnostic{marker.file, marker.line, marker.column, std::move(message)}); }

	void scanCpp() {
		for (const SourceFile& source : input.handlerFiles) {
			std::string category = firstSegment(std::string_view(source.relPath).substr(std::min(source.relPath.size(), HANDLERS_DIR.size() + 1)));
			model.filesPerCategory[category.empty() ? "(root)" : category]++;
			addScan(scanCppFile(source.file, source.relPath, source.content, FileRole::HANDLER));
			if (category == "ai" || category == "instance" || category == "quest")
				findSpawnNpcIds(source.content, model.npcIds);
		}
		for (const SourceFile& source : input.clientPacketFiles) {
			model.filesPerCategory["clientpackets"]++;
			addScan(scanCppFile(source.file, source.relPath, source.content, FileRole::CLIENT_PACKET));
		}
	}

	void addScan(FileScan scan) {
		std::ranges::move(scan.errors, std::back_inserter(model.errors));
		std::ranges::move(scan.markers, std::back_inserter(markers));
		std::ranges::move(scan.types, std::back_inserter(types));
	}

	void checkTypesAndClasses() {
		for (const TypeDefinition& type : types) {
			auto [it, inserted] = typesByName.try_emplace(type.ns + "::" + type.name, &type);
			if (!inserted) {
				model.errors.push_back(Diagnostic{type.file, type.line, type.column,
					"type " + type.name + " is already defined in namespace " + type.ns + " at " + it->second->file + "(" + std::to_string(it->second->line) + ")"});
			}
		}
		std::map<std::string, const Marker*> registeredClasses;
		for (const Marker& marker : markers) {
			std::string key = marker.ns + "::" + marker.className;
			bool valid = true;
			if (!typesByName.contains(key)) {
				error(marker, "class " + marker.className + " of " + std::string(markerName(marker.kind)) + " is not defined in namespace " + marker.ns);
				valid = false;
			}
			auto [it, inserted] = registeredClasses.try_emplace(key, &marker);
			if (!inserted) {
				error(marker, "class " + marker.className + " is already registered by " + std::string(markerName(it->second->kind)) + " at " + location(*it->second));
				valid = false;
			}
			if (valid)
				validMarkers.push_back(&marker);
		}
	}

	RegistryEntry entryOf(const Marker& marker) {
		RegistryEntry entry;
		entry.kind = marker.kind;
		entry.javaClass = javaClassName(marker);
		entry.ns = marker.ns;
		entry.function = marker.className + std::string(functionSuffix(marker.kind));
		std::string_view rel = marker.relPath;
		std::string_view prefix = marker.kind == MarkerKind::CLIENT_PACKET ? std::string_view("aion/gameserver/") : std::string_view("aion/gameserver/handlers/");
		if (rel.starts_with(prefix))
			rel.remove_prefix(prefix.size());
		entry.source = std::string(rel) + ":" + std::to_string(marker.line);
		switch (marker.kind) {
			case MarkerKind::AI:
			case MarkerKind::ZONE_HANDLER:
				entry.key = marker.text;
				entry.number = marker.number.value_or(0);
				break;
			case MarkerKind::INSTANCE_HANDLER:
			case MarkerKind::QUEST_HANDLER:
				entry.number = *marker.number;
				break;
			case MarkerKind::CLIENT_PACKET:
				entry.key = marker.className;
				break;
			default:
				break;
		}
		return entry;
	}

	void collectEntries() {
		std::map<std::string, const Marker*> aiNames;
		std::map<int32_t, const Marker*> mapIds;
		std::map<std::string, const Marker*> zoneNames;
		std::map<int32_t, const Marker*> questIds;
		auto duplicate = [&](const Marker& marker, const std::string& what, const Marker& first) {
			error(marker, what + " is already registered by " + first.className + " at " + location(first));
		};
		for (const Marker* marker : validMarkers) {
			bool unique = true;
			switch (marker->kind) {
				case MarkerKind::AI: {
					auto [it, inserted] = aiNames.try_emplace(marker->text, marker);
					if (!inserted) {
						duplicate(*marker, "AI name \"" + marker->text + "\"", *it->second);
						unique = false;
					}
					break;
				}
				case MarkerKind::INSTANCE_HANDLER: {
					auto [it, inserted] = mapIds.try_emplace(*marker->number, marker);
					if (!inserted) {
						duplicate(*marker, "map id " + std::to_string(*marker->number), *it->second);
						unique = false;
					}
					break;
				}
				case MarkerKind::ZONE_HANDLER: {
					std::set<std::string> own;
					for (const std::string& name : splitZoneNames(marker->text)) {
						if (!own.insert(name).second) {
							error(*marker, "zone name " + name + " is listed twice");
							unique = false;
							continue;
						}
						auto [it, inserted] = zoneNames.try_emplace(name, marker);
						if (!inserted) {
							duplicate(*marker, "zone name " + name, *it->second);
							unique = false;
						}
					}
					break;
				}
				case MarkerKind::QUEST_HANDLER: {
					auto [it, inserted] = questIds.try_emplace(*marker->number, marker);
					if (!inserted) {
						duplicate(*marker, "quest id " + std::to_string(*marker->number), *it->second);
						unique = false;
					}
					break;
				}
				default:
					break;
			}
			if (!unique)
				continue;
			RegistryEntry entry = entryOf(*marker);
			switch (marker->kind) {
				case MarkerKind::AI:
					model.ai.push_back(std::move(entry));
					break;
				case MarkerKind::INSTANCE_HANDLER:
					model.instances.push_back(std::move(entry));
					break;
				case MarkerKind::ZONE_HANDLER:
					model.zones.push_back(std::move(entry));
					break;
				case MarkerKind::QUEST_HANDLER:
					model.quests.push_back(std::move(entry));
					break;
				case MarkerKind::CLIENT_PACKET:
					model.clientPackets.push_back(std::move(entry));
					break;
				default:
					model.commands.push_back(std::move(entry));
					break;
			}
			entryMarkers.push_back(marker);
		}
		std::ranges::sort(model.ai, {}, &RegistryEntry::key);
		std::ranges::sort(model.instances, {}, &RegistryEntry::number);
		std::ranges::sort(model.zones, {}, &RegistryEntry::key);
		std::ranges::sort(model.quests, {}, &RegistryEntry::number);
		std::ranges::sort(model.commands, [](const RegistryEntry& a, const RegistryEntry& b) { return std::tie(a.kind, a.javaClass) < std::tie(b.kind, b.javaClass); });
		std::ranges::sort(model.clientPackets, {}, &RegistryEntry::key);
	}

	void scanJava() {
		for (const SourceFile& source : input.javaHandlerFiles)
			scanJavaHandlerFile(source.file, source.relPath, source.content, java);
		resolveJavaHandlerRegistrations(java); // quest handlers and commands register through their whole superclass chain
		std::ranges::move(java.errors, std::back_inserter(model.errors));

		auto javaError = [&](const JavaHandlerClass& cls, std::string message) {
			model.errors.push_back(Diagnostic{cls.file, cls.line, 1, std::move(message)});
		};
		for (const JavaHandlerClass& cls : java.classes) {
			javaByClass.try_emplace(cls.javaClass, &cls);
			if (!cls.registered)
				continue;
			if (cls.aiName && !javaAiNames.try_emplace(*cls.aiName, &cls).second)
				javaError(cls, "Java: duplicate @AIName(\"" + *cls.aiName + "\") (also " + javaAiNames[*cls.aiName]->javaClass + ")");
			if (cls.instanceId && !javaInstances.try_emplace(*cls.instanceId, &cls).second)
				javaError(cls, "Java: duplicate @InstanceID(" + std::to_string(*cls.instanceId) + ") (also " + javaInstances[*cls.instanceId]->javaClass + ")");
			if (cls.zone) {
				for (const std::string& name : splitZoneNames(cls.zone->names)) {
					if (!javaZoneNames.try_emplace(name, &cls).second)
						javaError(cls, "Java: duplicate zone name " + name + " (also " + javaZoneNames[name]->javaClass + ")");
				}
			}
			if (cls.questId && !javaQuests.try_emplace(*cls.questId, &cls).second)
				javaError(cls, "Java: duplicate quest handler for quest " + std::to_string(*cls.questId) + " (also " + javaQuests[*cls.questId]->javaClass + ")");
			if (cls.command)
				javaCommands.try_emplace(cls.javaClass, &cls);
		}

		for (const Marker* marker : entryMarkers) {
			if (marker->kind == MarkerKind::CLIENT_PACKET)
				continue;
			std::string javaClass = javaClassName(*marker);
			auto it = javaByClass.find(javaClass);
			const JavaHandlerClass* cls = it == javaByClass.end() ? nullptr : it->second;
			switch (marker->kind) {
				case MarkerKind::AI:
					if (auto owner = javaAiNames.find(marker->text); owner != javaAiNames.end() && owner->second->javaClass != javaClass)
						error(*marker, "AI name \"" + marker->text + "\" belongs to Java class " + owner->second->javaClass);
					else if (cls && !cls->aiName)
						error(*marker, "Java class " + javaClass + " registers no @AIName");
					else if (cls && *cls->aiName != marker->text)
						error(*marker, "AI name \"" + marker->text + "\" differs from Java @AIName(\"" + *cls->aiName + "\") of " + javaClass);
					break;
				case MarkerKind::INSTANCE_HANDLER:
					if (auto owner = javaInstances.find(*marker->number); owner != javaInstances.end() && owner->second->javaClass != javaClass)
						error(*marker, "map id " + std::to_string(*marker->number) + " belongs to Java class " + owner->second->javaClass);
					else if (cls && !cls->instanceId)
						error(*marker, "Java class " + javaClass + " registers no @InstanceID");
					else if (cls && *cls->instanceId != *marker->number)
						error(*marker, "map id " + std::to_string(*marker->number) + " differs from Java @InstanceID(" + std::to_string(*cls->instanceId) + ") of " +
							javaClass);
					break;
				case MarkerKind::ZONE_HANDLER:
					if (cls && !cls->zone)
						error(*marker, "Java class " + javaClass + " registers no @ZoneNameAnnotation");
					else if (cls && (cls->zone->names != marker->text || cls->zone->questId != marker->number.value_or(0)))
						error(*marker, "zone names/questId (\"" + marker->text + "\", " + std::to_string(marker->number.value_or(0)) +
							") differ from Java @ZoneNameAnnotation(\"" + cls->zone->names + "\", " + std::to_string(cls->zone->questId) + ") of " + javaClass);
					for (const std::string& name : splitZoneNames(marker->text)) {
						if (auto owner = javaZoneNames.find(name); owner != javaZoneNames.end() && owner->second->javaClass != javaClass)
							error(*marker, "zone name " + name + " belongs to Java class " + owner->second->javaClass);
					}
					break;
				case MarkerKind::QUEST_HANDLER:
					if (auto owner = javaQuests.find(*marker->number); owner != javaQuests.end() && owner->second->javaClass != javaClass)
						error(*marker, "quest id " + std::to_string(*marker->number) + " belongs to Java class " + owner->second->javaClass);
					else if (cls && !cls->questId)
						error(*marker, "Java class " + javaClass + " is no quest handler with a known quest id");
					else if (cls && *cls->questId != *marker->number)
						error(*marker, "quest id " + std::to_string(*marker->number) + " differs from Java super(" + std::to_string(*cls->questId) + ") of " + javaClass);
					break;
				default: {
					CommandKind kind = *commandKindOf(marker->kind);
					if (cls && !cls->command)
						error(*marker, "Java class " + javaClass + " is no command");
					else if (cls && *cls->command != kind)
						error(*marker, std::string(markerName(marker->kind)) + " does not match Java " + javaClass + ", which extends " +
							std::string(commandKindName(*cls->command)));
					break;
				}
			}
		}
	}

	void scanJavaClientPackets() {
		const SourceFile& factory = *input.javaClientPacketFactory;
		std::vector<JavaClientPacket> packets = scanJavaClientPacketFactory(factory.file, factory.content, model.errors);
		for (const JavaClientPacket& packet : packets)
			javaClientPackets.insert(packet.name);
		for (const Marker* marker : entryMarkers) {
			if (marker->kind == MarkerKind::CLIENT_PACKET && !javaClientPackets.contains(marker->className))
				error(*marker, marker->className + " is not in the client packet table of AionClientPacketFactory.java");
		}
	}

	void buildReport() {
		bool haveJava = input.javaHandlersGiven;
		auto section = [&](std::string title, size_t ported, std::optional<size_t> javaCount) {
			ReportSection s;
			s.title = std::move(title);
			s.ported = ported;
			s.java = javaCount;
			return s;
		};
		auto javaClassKnown = [&](const RegistryEntry& entry) { return javaByClass.contains(entry.javaClass); };

		// ai
		ReportSection ai = section("ai", model.ai.size(), haveJava ? std::optional(javaAiNames.size()) : std::nullopt);
		std::set<std::string> cppAiNames;
		for (const RegistryEntry& entry : model.ai) {
			cppAiNames.insert(entry.key);
			if (haveJava && !javaClassKnown(entry))
				ai.unknownToJava.push_back(entry.key + "\t" + entry.javaClass);
		}
		for (const auto& [name, cls] : javaAiNames) {
			if (!cppAiNames.contains(name))
				ai.missing.push_back(name + "\t" + cls->javaClass);
		}
		model.report.push_back(std::move(ai));

		// instance
		ReportSection instance = section("instance", model.instances.size(), haveJava ? std::optional(javaInstances.size()) : std::nullopt);
		std::set<int32_t> cppMapIds;
		for (const RegistryEntry& entry : model.instances) {
			cppMapIds.insert(entry.number);
			if (haveJava && !javaClassKnown(entry))
				instance.unknownToJava.push_back(std::to_string(entry.number) + "\t" + entry.javaClass);
		}
		for (const auto& [id, cls] : javaInstances) {
			if (!cppMapIds.contains(id))
				instance.missing.push_back(std::to_string(id) + "\t" + cls->javaClass);
		}
		model.report.push_back(std::move(instance));

		// zone names
		std::set<std::string> cppZoneNames;
		ReportSection zone = section("zone names", 0, haveJava ? std::optional(javaZoneNames.size()) : std::nullopt);
		for (const RegistryEntry& entry : model.zones) {
			for (const std::string& name : splitZoneNames(entry.key)) {
				cppZoneNames.insert(name);
				if (haveJava && !javaClassKnown(entry))
					zone.unknownToJava.push_back(name + "\t" + entry.javaClass);
			}
		}
		zone.ported = cppZoneNames.size();
		for (const auto& [name, cls] : javaZoneNames) {
			if (!cppZoneNames.contains(name))
				zone.missing.push_back(name + "\t" + cls->javaClass);
		}
		model.report.push_back(std::move(zone));

		// quest
		ReportSection quest = section("quest", model.quests.size(), haveJava ? std::optional(javaQuests.size()) : std::nullopt);
		std::set<int32_t> cppQuestIds;
		for (const RegistryEntry& entry : model.quests) {
			cppQuestIds.insert(entry.number);
			if (haveJava && !javaClassKnown(entry))
				quest.unknownToJava.push_back(std::to_string(entry.number) + "\t" + entry.javaClass);
		}
		for (const auto& [id, cls] : javaQuests) {
			if (!cppQuestIds.contains(id))
				quest.missing.push_back(std::to_string(id) + "\t" + cls->javaClass);
		}
		model.report.push_back(std::move(quest));

		// commands, per kind
		for (CommandKind kind : {CommandKind::ADMIN, CommandKind::PLAYER, CommandKind::CONSOLE}) {
			std::string title = kind == CommandKind::ADMIN ? "admin commands" : kind == CommandKind::PLAYER ? "player commands" : "console commands";
			std::set<std::string> cppClasses;
			ReportSection commands = section(title, 0, std::nullopt);
			for (const RegistryEntry& entry : model.commands) {
				if (*commandKindOf(entry.kind) != kind)
					continue;
				cppClasses.insert(entry.javaClass);
				if (haveJava && !javaClassKnown(entry))
					commands.unknownToJava.push_back(entry.javaClass);
			}
			commands.ported = cppClasses.size();
			if (haveJava) {
				size_t count = 0;
				for (const auto& [javaClass, cls] : javaCommands) {
					if (*cls->command != kind)
						continue;
					count++;
					if (!cppClasses.contains(javaClass))
						commands.missing.push_back(javaClass);
				}
				commands.java = count;
			}
			model.report.push_back(std::move(commands));
		}

		// client packets
		ReportSection packets = section("client packets", model.clientPackets.size(),
			input.javaClientPacketFactory ? std::optional(javaClientPackets.size()) : std::nullopt);
		std::set<std::string> cppPackets;
		for (const RegistryEntry& entry : model.clientPackets)
			cppPackets.insert(entry.key);
		for (const std::string& name : javaClientPackets) {
			if (!cppPackets.contains(name))
				packets.missing.push_back(name);
		}
		model.report.push_back(std::move(packets));

		// npc ids
		ReportSection npcIds = section("npc ids spawned by handlers", model.npcIds.size(), haveJava ? std::optional(java.npcIds.size()) : std::nullopt);
		if (haveJava) {
			for (int32_t id : java.npcIds) {
				if (!model.npcIds.contains(id))
					npcIds.missing.push_back(std::to_string(id));
			}
			for (int32_t id : model.npcIds) {
				if (!java.npcIds.contains(id))
					npcIds.unknownToJava.push_back(std::to_string(id));
			}
		}
		model.report.push_back(std::move(npcIds));
	}

	const ScanInput& input;
	RegistryModel model;
	std::vector<Marker> markers;
	std::vector<TypeDefinition> types;
	std::map<std::string, const TypeDefinition*> typesByName;
	std::vector<const Marker*> validMarkers;
	std::vector<const Marker*> entryMarkers;
	JavaHandlerScan java;
	std::map<std::string, const JavaHandlerClass*> javaByClass;
	std::map<std::string, const JavaHandlerClass*> javaAiNames;
	std::map<int32_t, const JavaHandlerClass*> javaInstances;
	std::map<std::string, const JavaHandlerClass*> javaZoneNames;
	std::map<int32_t, const JavaHandlerClass*> javaQuests;
	std::map<std::string, const JavaHandlerClass*> javaCommands;
	std::set<std::string> javaClientPackets;
};

} // namespace

ScanInput readInput(const Options& options, std::vector<Diagnostic>& errors) {
	ScanInput input;
	if (options.handlersRoot) {
		input.handlersGiven = true;
		input.handlerFiles = collect(*options.handlersRoot, HANDLERS_DIR, true, errors);
	}
	if (options.clientPacketsRoot) {
		input.clientPacketsGiven = true;
		input.clientPacketFiles = collect(*options.clientPacketsRoot, CLIENT_PACKETS_DIR, true, errors);
	}
	if (options.javaHandlers) {
		input.javaHandlersGiven = true;
		input.javaHandlerFiles = collect(*options.javaHandlers, "", false, errors);
	}
	if (options.javaClientPacketFactory) {
		auto content = readFile(*options.javaClientPacketFactory);
		if (!content)
			rootError(errors, *options.javaClientPacketFactory, "cannot read file");
		else
			input.javaClientPacketFactory = SourceFile{options.javaClientPacketFactory->string(), options.javaClientPacketFactory->filename().generic_string(), *content};
	}
	return input;
}

RegistryModel buildRegistry(const ScanInput& input) {
	return Builder(input).run();
}

std::string javaClassName(const Marker& marker) {
	if (marker.kind == MarkerKind::CLIENT_PACKET)
		return marker.className;
	std::string_view rel = marker.relPath;
	std::string_view prefix = "aion/gameserver/handlers/";
	if (rel.starts_with(prefix))
		rel.remove_prefix(prefix.size());
	size_t slash = rel.rfind('/');
	std::string_view dir = slash == std::string_view::npos ? std::string_view() : rel.substr(0, slash);
	std::string result;
	while (!dir.empty()) {
		size_t sep = dir.find('/');
		std::string_view segment = dir.substr(0, sep);
		if (segment.ends_with('_') && isCppKeyword(segment.substr(0, segment.size() - 1)))
			segment.remove_suffix(1);
		result.append(segment).append(".");
		dir = sep == std::string_view::npos ? std::string_view() : dir.substr(sep + 1);
	}
	return result + marker.className;
}

} // namespace aion::gameserver::tools::regscan
