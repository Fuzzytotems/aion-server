#include "aion/gameserver/handlers/consolecommands/Wish.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/AdminService.h"
#include "aion/gameserver/services/EnchantService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/ChatUtil.h"

namespace aion::gameserver::handlers::consolecommands {

/**
 * The read-only stream of Wish.findIdInXml (Java: XMLInputFactory.createXMLStreamReader over a FileReader): reads the file in chunks and
 * hands each start tag's name and attributes to the caller, skipping comments, processing instructions, CDATA sections and declarations.
 */
struct WishXmlStream {
public:
	explicit WishXmlStream(const std::string& file) : in(file, std::ios::binary) {
		if (!in)
			throw commons::utils::IOException(file + " (The system cannot find the file specified)"); // Java: FileNotFoundException
	}

	/** the next start tag; false at the end of the file */
	bool nextStartTag(std::string& name, std::map<std::string, std::string, std::less<>>& attributes) {
		int c;
		while ((c = get()) != EOF) {
			if (c != '<')
				continue;
			int next = peek();
			if (next == '!') { // a comment, CDATA or a declaration
				get();
				if (startsWith("--"))
					skipPast("-->");
				else if (startsWith("[CDATA["))
					skipPast("]]>");
				else
					skipPast(">");
				continue;
			}
			if (next == '?') { // a processing instruction or the XML declaration
				skipPast("?>");
				continue;
			}
			if (next == '/') { // an end tag
				skipPast(">");
				continue;
			}
			name.clear();
			attributes.clear();
			while ((c = get()) != EOF && !isSpace(c) && c != '>' && c != '/')
				name.push_back(static_cast<char>(c));
			while (c != EOF && c != '>') {
				while (c != EOF && (isSpace(c) || c == '/'))
					c = get();
				if (c == '>' || c == EOF)
					break;
				std::string attribute;
				while (c != EOF && c != '=' && !isSpace(c) && c != '>')
					attribute.push_back(static_cast<char>(c)), c = get();
				while (c != EOF && c != '=' && c != '>')
					c = get();
				if (c != '=')
					break;
				do
					c = get();
				while (c != EOF && isSpace(c));
				if (c != '"' && c != '\'')
					break;
				const int quote = c;
				std::string value;
				while ((c = get()) != EOF && c != quote)
					value.push_back(static_cast<char>(c));
				attributes.emplace(std::move(attribute), unescape(value));
				c = get();
			}
			return true;
		}
		return false;
	}

private:
	std::ifstream in;

	int get() { return in.get(); }
	int peek() { return in.peek(); }
	static bool isSpace(int c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

	bool startsWith(std::string_view text) {
		for (char expected : text) {
			if (peek() != expected)
				return false;
			get();
		}
		return true;
	}

	void skipPast(std::string_view end) {
		std::string window;
		int c;
		while ((c = get()) != EOF) {
			window.push_back(static_cast<char>(c));
			if (window.size() > end.size())
				window.erase(0, 1);
			if (window == end)
				return;
		}
	}

	/** the predefined entities and character references (StAX reports attribute values unescaped) */
	static std::string unescape(const std::string& value) {
		std::string out;
		for (size_t i = 0; i < value.size(); i++) {
			if (value[i] != '&') {
				out.push_back(value[i]);
				continue;
			}
			size_t end = value.find(';', i);
			if (end == std::string::npos) {
				out.push_back(value[i]);
				continue;
			}
			std::string_view entity(value.data() + i + 1, end - i - 1);
			if (entity == "amp")
				out.push_back('&');
			else if (entity == "lt")
				out.push_back('<');
			else if (entity == "gt")
				out.push_back('>');
			else if (entity == "quot")
				out.push_back('"');
			else if (entity == "apos")
				out.push_back('\'');
			else if (!entity.empty() && entity[0] == '#') {
				char32_t codePoint = static_cast<char32_t>(entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')
					? std::stoul(std::string(entity.substr(2)), nullptr, 16) : std::stoul(std::string(entity.substr(1)), nullptr, 10));
				out += commons::utils::StringUtils::toUtf8(std::u16string(1, static_cast<char16_t>(codePoint)));
			} else {
				out.append("&").append(entity).append(";");
			}
			i = end;
		}
		return out;
	}
};

AION_CONSOLE_COMMAND(Wish);

// Java Wish.java:32-38
Wish::Wish()
	: ConsoleCommand("wish", "Spawns NPCs and adds items.",
		  "<npc name> - Spawns the specified NPC on your targets position.\n"
		  "<count> <item name> - Adds the specified item to your target.\n"
		  "<item name> <enchant> - Adds the specified item with the enchant level to your target.\n") {
}

// Java Wish.java:40-114
void Wish::execute(Player& admin, std::span<const std::string> params) {
	if (params.empty()) {
		sendInfo(admin);
		return;
	}

	if (params.size() == 1) { // spawn npc
		std::string npcName = params[0];
		int32_t npcId = findNpcId(npcName);
		if (npcId == 0) {
			sendInfo(admin, "There is no NPC with that name.");
			return;
		}
		runtime::Ref<SpawnTemplate> spawn = SpawnEngine::newSpawn(admin.getWorldId(), npcId, admin.getX(), admin.getY(), admin.getZ(), admin.getHeading(), 0); // parity= SpawnTemplate spawn = SpawnEngine.newSpawn(admin.getWorldId(), npcId, admin.getX(), admin.getY(), admin.getZ(), admin.getHeading(), 0);
		runtime::Ptr<VisibleObject> visibleObject = SpawnEngine::spawnObject(*spawn, admin.getInstanceId()); // parity= VisibleObject visibleObject = SpawnEngine.spawnObject(spawn, admin.getInstanceId());
		if (visibleObject == nullptr) {
			sendInfo(admin, "Could not spawn npc with ID " + std::to_string(npcId) + "."); // parity= sendInfo(admin, "Could not spawn npc with ID " + npcId + ".");
			return;
		}
		sendInfo(admin, "Spawned " + name(*visibleObject) + "."); // parity= sendInfo(admin, "Spawned " + name(visibleObject) + ".");
	} else { // add item
		runtime::Ptr<Player> targetPlayer = runtime::as<Player>(admin.getTarget()); // parity= Player target = admin.getTarget() instanceof Player targetPlayer ? targetPlayer : admin;
		Player& target = targetPlayer != nullptr ? *targetPlayer : admin; // parity: (the same)
		std::string itemName = params[0];
		int64_t addCount = 1;
		int32_t enchant = 0;
		try {
			addCount = commons::utils::parseInt(params[0]); // parity= addCount = Integer.parseInt(params[0]);
			itemName = params[1];
		} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException _) {
			try {
				enchant = commons::utils::parseInt(params[1]); // parity= enchant = Integer.parseInt(params[1]);
			} catch (const commons::utils::NumberFormatException&) { // parity= } catch (NumberFormatException _) {
			}
		}
		int32_t itemId = findItemId(itemName);
		if (itemId == 0) {
			sendInfo(admin, "There is no item named " + itemName + ".");
			return;
		}
		if (!AdminService::getInstance().canOperate(admin, runtime::Ptr<Player>(&target), itemId, "command ///wish")) // parity= if (!AdminService.getInstance().canOperate(admin, target, itemId, "command ///wish"))
			return;

		int64_t addedCount;
		if (enchant > 0) {
			runtime::Ref<Item> newItem = ItemFactory::newItem(itemId); // parity= Item newItem = ItemFactory.newItem(itemId);

			if (newItem == nullptr)
				return;
			enchant = std::min(enchant, 255); // parity= enchant = Math.min(enchant, 255);
			if (newItem->getItemTemplate()->getEquipmentType() != EquipType::PLUME) {
				if (newItem->getItemTemplate()->canTune() && newItem->getItemTemplate()->getMaxEnchantBonus() > 0)
					enchant = std::min(enchant, newItem->getItemTemplate()->getMaxEnchantLevel()); // parity= enchant = Math.min(enchant, newItem.getItemTemplate().getMaxEnchantLevel());
				newItem->setEnchantLevel(enchant);
				if (enchant > newItem->getItemTemplate()->getMaxEnchantLevel()) {
					newItem->setAmplified(true);
					if (enchant >= 20)
						newItem->setBuffSkill(EnchantService::getEquipBuff(*newItem)); // parity= newItem.setBuffSkill(EnchantService.getEquipBuff(newItem));
				}
			} else {
				newItem->setTempering(enchant);
			}
			addedCount = addCount - ItemService::addItem(target, *newItem); // parity= addedCount = addCount - ItemService.addItem(target, newItem);
		} else {
			addedCount = addCount - ItemService::addItem(target, itemId, addCount, true);
		}

		if (addedCount <= 0) {
			sendInfo(admin, "Item couldn't be added.");
		} else if (!admin.equals(target)) {
			sendInfo(admin, "You gave " + std::to_string(addedCount) + " " + ChatUtil::item(itemId) + " to " + name(target) + "."); // parity= sendInfo(admin, "You gave " + addedCount + " " + ChatUtil.item(itemId) + " to " + name(target) + ".");
			sendInfo(target, "You received " + std::to_string(addedCount) + " " + ChatUtil::item(itemId) + " from " + name(admin) + "."); // parity= sendInfo(target, "You received " + addedCount + " " + ChatUtil.item(itemId) + " from " + name(admin) + ".");
		}
	}
}

// Java Wish.java:116-118
int32_t Wish::findNpcId(std::string_view npcName) {
	return findIdInXml("./data/handlers/consolecommands/data/npcs.xml", "npc", "name", npcName);
}

// Java Wish.java:120-122
int32_t Wish::findItemId(std::string_view itemName) {
	return findIdInXml("./data/static_data/items/item_templates.xml", "item_template", "cName", itemName);
}

// Java Wish.java:124-141
int32_t Wish::findIdInXml(const std::string& xml, std::string_view elementName, std::string_view attributeName, std::string_view attributeValue) {
	try { // parity= try (FileReader fis = new FileReader(xml)) {
		WishXmlStream reader(xml); // parity= XMLStreamReader reader = XMLInputFactory.newInstance().createXMLStreamReader(fis);
		std::string name; // parity: the start tag the stream reports
		std::map<std::string, std::string, std::less<>> attributes; // parity: (the same)
		while (reader.nextStartTag(name, attributes)) { // parity= try { while (reader.hasNext()) {
			auto attribute = attributes.find(attributeName); // parity: reader.getAttributeValue(null, attributeName), below
			if (elementName == name && attribute != attributes.end() && commons::utils::StringUtils::equalsIgnoreCase(attributeValue, attribute->second)) { // parity= if (reader.next() == XMLStreamReader.START_ELEMENT && elementName.equals(reader.getLocalName()) && attributeValue.equalsIgnoreCase(reader.getAttributeValue(null, attributeName))) {
				auto id = attributes.find("id"); // parity: reader.getAttributeValue(null, "id"), below
				if (id == attributes.end()) // parity: Java's Integer.parseInt(null)
					throw commons::utils::NumberFormatException("Cannot parse null string: null"); // parity: (the same)
				return commons::utils::parseInt(id->second); // parity= return Integer.parseInt(reader.getAttributeValue(null, "id"));
			}
		}
		// parity= } } finally { reader.close(); }
	} catch (const std::exception& e) { // parity= } catch (Exception e) {
		throw commons::utils::Exception(e.what(), std::current_exception()); // parity= throw new RuntimeException(e);
	}
	return 0;
}

} // namespace aion::gameserver::handlers::consolecommands
