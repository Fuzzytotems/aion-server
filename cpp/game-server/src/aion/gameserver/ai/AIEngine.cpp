#include "aion/gameserver/ai/AIEngine.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <typeinfo>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/ai/AITemplate.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/GameServerError.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::ai {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.ai.AIEngine");

template <class T>
class AIEngine::DummyAI final : public AITemplate<T> {
public:
	explicit DummyAI(T& owner) : AITemplate<T>(owner) {}
};

namespace {

/** Java: Class.getSimpleName() of the owner's class; stands for utils::simpleClassName (P4-05, handlers-and-porting-plan.md §1.10) */
std::string simpleClassNameOf(const std::type_info& type) {
	std::string name = type.name(); // MSVC: "class aion::gameserver::model::gameobjects::Npc"
	if (size_t space = name.rfind(' '); space != std::string::npos)
		name.erase(0, space + 1);
	if (size_t colons = name.rfind("::"); colons != std::string::npos)
		name.erase(0, colons + 2);
	return name;
}

/** Java Class.toString() of a handler class: "class ai.GeneralNpcAI" */
std::string classNameOf(const handlers::AIHandlerEntry& entry) {
	return "class " + std::string(entry.javaClass);
}

/** C++ only: the AI names newAI replaced by a DummyAI (gameserver.dev.missing_ai_handlers=warn), each warned once */
struct MissingAiNames {
	std::mutex mutex;
	std::set<std::string, std::less<>> names;
};

MissingAiNames& missingAiNames() {
	static auto* instance = new MissingAiNames(); // leaked immortal: newAI may run during static destruction of test fixtures
	return *instance;
}

/** @return true the first time a missing AI name is passed */
bool firstMissingUse(std::string_view name) {
	MissingAiNames& missing = missingAiNames();
	std::lock_guard lock(missing.mutex);
	return missing.names.emplace(name).second;
}

} // namespace

AIEngine::AIEngine() = default;

AIEngine::~AIEngine() = default;

void AIEngine::init() {
	// Java: a ScriptManager with an AIHandlerClassListener loads AIConfig.HANDLER_DIRECTORY and calls registerAI for each AI class; C++: the
	// handlers are compiled in and listed by the AI handler registry (AION_AI markers, HandlerRegistry.h), so registerAI runs per entry.
	std::span<const handlers::AIHandlerEntry> entries = handlers::aiHandlerEntries();
	for (const handlers::AIHandlerEntry& entry : entries)
		registerAI(entry);
	validateScripts();
	log.info("Loaded " + std::to_string(entries.size()) + " AI handlers.");
}

void AIEngine::reload() {
	AION_UNPORTED();
}

void AIEngine::registerAI(const handlers::AIHandlerEntry& aiClass) {
	// Java: aiHandlers.putIfAbsent(nameAnnotation.value(), aiClass) for classes with an @AIName; every registry entry has a name, and the registry
	// (sorted by name) stands for the map, so the entry already present under the name is the one findAIHandler returns.
	const handlers::AIHandlerEntry* presentClass = handlers::findAIHandler(handlers::aiHandlerEntries(), aiClass.name);
	if (presentClass != nullptr && presentClass != &aiClass)
		throw runtime::IllegalArgumentException(
			"Duplicate AIs with name " + std::string(aiClass.name) + " (" + classNameOf(aiClass) + ", " + classNameOf(*presentClass) + ")");
}

std::unique_ptr<AbstractAI> AIEngine::newAI(std::optional<std::string_view> name, model::gameobjects::Creature& owner) {
	std::unique_ptr<AbstractAI> aiInstance;
	if (!name) {
		aiInstance = std::make_unique<DummyAI<model::gameobjects::Creature>>(owner);
	} else {
		const handlers::AIHandlerEntry* entry = handlers::findAIHandler(handlers::aiHandlerEntries(), *name);
		if (entry == nullptr) {
			if (!isMissingAiHandlersWarn())
				throw runtime::IllegalArgumentException("No AI found for name " + std::string(*name));
			// C++ only (gameserver.dev.missing_ai_handlers=warn, docs/deviations/P4-01.md): an NPC whose AI is not ported gets a DummyAI
			if (firstMissingUse(*name))
				log.warn("No AI found for name " + std::string(*name) + ", creating a DummyAI instead (gameserver.dev.missing_ai_handlers=warn)");
			aiInstance = std::make_unique<DummyAI<model::gameobjects::Creature>>(owner);
		} else {
			try {
				aiInstance = entry->create(owner);
			} catch (const runtime::UnportedException&) {
				throw; // an unported constructor body keeps its own trace (runtime/base/Unported.h)
			} catch (const std::exception&) {
				// Java: "Could not instantiate AI for class " + aiClass + " (owner: " + owner + ")"
				throw runtime::IllegalArgumentException(
					"Could not instantiate AI for class " + classNameOf(*entry) + " (owner: " + owner.toString() + ")", std::current_exception());
			}
			if (aiInstance == nullptr) // Java: findConstructor(aiClass, owner.getClass(), false) == null
				throw runtime::IllegalArgumentException(handlers::aiOwnerMismatchMessage(*entry, simpleClassNameOf(typeid(owner))));
			aiInstance->setRegistryEntry(entry);
		}
	}
	if (configs::main::AIConfig::ONCREATE_DEBUG.load())
		aiInstance->setLogging(true);
	return aiInstance;
}

void AIEngine::validateScripts() {
	// Java first checks each handler class for its generic owner type and a constructor taking that owner ("Faulty AI handler: ..."); C++: the
	// AION_AI marker checks both at compile time (HandlerRegistry.h AIHandlerClass).
	std::set<std::string> npcAINames; // Java: a HashSet (unspecified order); C++: sorted, so the message is deterministic
	for (const model::templates::npc::NpcTemplate* npcTemplate : dataholders::DataManager::NPC_DATA->getNpcData()) {
		if (std::optional<std::string> aiName = npcTemplate->getAiName())
			npcAINames.insert(std::move(*aiName));
	}
	std::erase_if(npcAINames, [](const std::string& aiName) { return handlers::findAIHandler(handlers::aiHandlerEntries(), aiName) != nullptr; });
	if (!npcAINames.empty()) {
		std::string names;
		for (const std::string& aiName : npcAINames)
			names.append(names.empty() ? "" : ", ").append(aiName);
		std::string message = "No AIs could be found for the following npc_template AI names: " + names;
		if (!isMissingAiHandlersWarn())
			throw GameServerError(message); // Java: GameServerError
		// C++ only (gameserver.dev.missing_ai_handlers=warn): the NPCs get a DummyAI from newAI
		log.warn(message + " (gameserver.dev.missing_ai_handlers=warn: these NPCs get a DummyAI)");
	}
}

bool AIEngine::isMissingAiHandlersWarn() {
	std::shared_ptr<const std::string> value = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
	return value && commons::utils::StringUtils::equalsIgnoreCase(*value, "warn");
}

std::vector<std::string> AIEngine::missingAiNamesSeen() {
	MissingAiNames& missing = missingAiNames();
	std::lock_guard lock(missing.mutex);
	return std::vector<std::string>(missing.names.begin(), missing.names.end());
}

AIEngine& AIEngine::getInstance() {
	static AIEngine instance; // Java: SingletonHolder
	return instance;
}

} // namespace aion::gameserver::ai
