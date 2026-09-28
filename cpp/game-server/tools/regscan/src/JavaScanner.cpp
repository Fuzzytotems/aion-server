#include "JavaScanner.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

#include "SpawnIds.h"

namespace aion::gameserver::tools::regscan {

namespace {

struct AnnotationValue {
	std::optional<std::string> text;
	std::optional<int32_t> number;
};

struct Annotation {
	std::string name; // simple name
	std::map<std::string, AnnotationValue, std::less<>> values; // "value" for the single-element form
	const Token* at = nullptr;
};

class JavaFileScanner {
public:
	JavaFileScanner(std::string_view file, std::string_view relPath, std::string_view content, JavaHandlerScan& scan)
		: file(file), relPath(relPath), scan(scan), lexed(lex(content, Language::JAVA)), t(lexed.tokens) {
		for (Diagnostic& d : lexed.errors) {
			d.file = std::string(file);
			scan.errors.push_back(std::move(d));
		}
		size_t slash = relPath.find('/');
		category = slash == std::string_view::npos ? std::string_view() : relPath.substr(0, slash);
		sourceIndex = scan.sources.size();
		scan.sources.push_back(JavaSource{std::string(category), {}, {}, {}});
	}

	void run() {
		std::vector<Annotation> annotations;
		Modifiers modifiers;
		size_t i = 0;
		while (i < t.size()) {
			const Token& tok = t[i];
			if (tok.isIdentifier("package")) {
				i++;
				std::string package;
				while (i < t.size() && !t[i].isPunct(";")) {
					package += t[i].text;
					i++;
				}
				i++;
				source().package = std::move(package);
			} else if (tok.isIdentifier("import")) {
				i = parseImport(i);
			} else if (tok.isPunct("@") && i + 1 < t.size() && t[i + 1].kind == TokenKind::IDENTIFIER && t[i + 1].text != "interface") {
				Annotation annotation;
				i = parseAnnotation(i, annotation);
				annotations.push_back(std::move(annotation));
			} else if (modifiers.apply(tok)) {
				i++;
			} else if (isTypeDeclarationStart(i)) {
				i = parseTypeDeclaration(i + (tok.isPunct("@") ? 1 : 0), annotations, modifiers, "");
				annotations.clear();
				modifiers = {};
			} else if (tok.isPunct("{")) {
				i = matchingBrace(i) + 1; // stray block at top level (should not occur)
			} else {
				if (tok.isPunct(";")) {
					annotations.clear();
					modifiers = {};
				}
				i++;
			}
		}
	}

private:
	struct Modifiers {
		bool isPublic = false;
		bool isAbstract = false;
		bool isStatic = false;

		/** @return true if the token is one of the modifiers the class listeners check (other modifiers are skipped like any token) */
		bool apply(const Token& tok) noexcept {
			if (tok.isIdentifier("public"))
				isPublic = true;
			else if (tok.isIdentifier("abstract"))
				isAbstract = true;
			else if (tok.isIdentifier("static"))
				isStatic = true;
			else
				return false;
			return true;
		}
	};

	JavaSource& source() { return scan.sources[sourceIndex]; }

	void error(const Token& tok, std::string message) { scan.errors.push_back(Diagnostic{std::string(file), tok.line, tok.column, std::move(message)}); }

	/** i at `import`; returns the index after the `;` */
	size_t parseImport(size_t i) {
		i++;
		bool isStatic = i < t.size() && t[i].isIdentifier("static");
		if (isStatic)
			i++;
		std::string name;
		while (i < t.size() && !t[i].isPunct(";")) {
			name += t[i].text;
			i++;
		}
		if (!isStatic && !name.empty()) {
			if (name.ends_with(".*")) {
				source().onDemandImports.push_back(name.substr(0, name.size() - 2));
			} else {
				size_t dot = name.rfind('.');
				source().singleImports[dot == std::string::npos ? name : name.substr(dot + 1)] = name;
			}
		}
		return i + 1;
	}

	/** class/interface/enum/@interface, or `record Name(`/`record Name<` (record is a contextual keyword); never the `X.class` expression */
	bool isTypeDeclarationStart(size_t i) const {
		const Token& tok = t[i];
		if (i > 0 && t[i - 1].isPunct("."))
			return false;
		if (tok.isIdentifier("class") || tok.isIdentifier("interface") || tok.isIdentifier("enum"))
			return true;
		if (tok.isPunct("@"))
			return i + 1 < t.size() && t[i + 1].isIdentifier("interface");
		return tok.isIdentifier("record") && i + 2 < t.size() && t[i + 1].kind == TokenKind::IDENTIFIER && (t[i + 2].isPunct("(") || t[i + 2].isPunct("<"));
	}

	size_t matchingBrace(size_t open) const {
		int depth = 0;
		for (size_t k = open; k < t.size(); k++) {
			if (t[k].isPunct("{"))
				depth++;
			else if (t[k].isPunct("}") && --depth == 0)
				return k;
		}
		return t.size();
	}

	std::optional<int32_t> parseInt(const Token& tok) {
		std::string digits;
		for (char c : tok.text) {
			if (c != '_')
				digits += c;
		}
		int32_t value = 0;
		auto [ptr, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), value);
		if (digits.empty() || ec != std::errc() || ptr != digits.data() + digits.size() || (digits.size() > 1 && digits[0] == '0'))
			return std::nullopt;
		return value;
	}

	std::optional<std::string> parseString(const Token& tok) {
		if (tok.kind != TokenKind::STRING || !tok.plainString || tok.text.find('\\') != std::string_view::npos)
			return std::nullopt;
		return std::string(tok.text.substr(1, tok.text.size() - 2));
	}

	std::optional<AnnotationValue> parseValue(size_t& i) {
		AnnotationValue value;
		bool negative = false;
		if (i < t.size() && t[i].isPunct("-")) {
			negative = true;
			i++;
		}
		if (i >= t.size())
			return std::nullopt;
		if (t[i].kind == TokenKind::NUMBER) {
			auto number = parseInt(t[i]);
			if (!number)
				return std::nullopt;
			value.number = negative ? -*number : *number;
		} else if (!negative && t[i].kind == TokenKind::STRING) {
			value.text = parseString(t[i]);
			if (!value.text)
				return std::nullopt;
		} else {
			return std::nullopt;
		}
		i++;
		return value;
	}

	/** i at '@'; returns the index after the annotation */
	size_t parseAnnotation(size_t i, Annotation& annotation) {
		annotation.at = &t[i];
		i++;
		while (i < t.size() && t[i].kind == TokenKind::IDENTIFIER) {
			annotation.name = std::string(t[i].text); // qualified names keep the last segment
			i++;
			if (i < t.size() && t[i].isPunct(".")) {
				i++;
				continue;
			}
			break;
		}
		if (i >= t.size() || !t[i].isPunct("("))
			return i;
		size_t close = i;
		int depth = 0;
		for (; close < t.size(); close++) {
			if (t[close].isPunct("("))
				depth++;
			else if (t[close].isPunct(")") && --depth == 0)
				break;
		}
		if (!isKeyAnnotation(annotation.name))
			return close + 1;
		size_t k = i + 1;
		bool ok = true;
		if (k < close && t[k].kind == TokenKind::IDENTIFIER && k + 1 < close && t[k + 1].isPunct("=")) {
			while (ok && k < close) {
				if (t[k].kind != TokenKind::IDENTIFIER || k + 1 >= close || !t[k + 1].isPunct("=")) {
					ok = false;
					break;
				}
				std::string key(t[k].text);
				k += 2;
				auto value = parseValue(k);
				if (!value) {
					ok = false;
					break;
				}
				annotation.values[key] = *value;
				if (k < close && t[k].isPunct(","))
					k++;
				else if (k != close)
					ok = false;
			}
		} else if (k < close) {
			auto value = parseValue(k);
			if (value && k == close)
				annotation.values["value"] = *value;
			else
				ok = false;
		}
		if (!ok)
			error(*annotation.at, "unsupported @" + annotation.name + " arguments (expected literal values)");
		return close + 1;
	}

	static bool isKeyAnnotation(std::string_view name) noexcept { return name == "AIName" || name == "InstanceID" || name == "ZoneNameAnnotation"; }

	const Annotation* findAnnotation(const std::vector<Annotation>& annotations, std::string_view name) const {
		auto it = std::ranges::find(annotations, name, &Annotation::name);
		return it == annotations.end() ? nullptr : &*it;
	}

	/** i at the class/interface/enum/record keyword; returns the index after the type body */
	size_t parseTypeDeclaration(size_t i, const std::vector<Annotation>& annotations, const Modifiers& modifiers, const std::string& enclosing) {
		const Token& keyword = t[i];
		bool isClass = keyword.isIdentifier("class");
		i++;
		if (i >= t.size() || t[i].kind != TokenKind::IDENTIFIER) {
			error(keyword, "malformed type declaration");
			return i;
		}
		const Token& nameToken = t[i];
		std::string extends;
		size_t k = i + 1;
		int angle = 0;
		while (k < t.size() && !(angle == 0 && t[k].isPunct("{"))) {
			if (t[k].isPunct("<"))
				angle++;
			else if (t[k].isPunct(">"))
				angle--;
			else if (angle == 0 && t[k].isIdentifier("extends")) {
				size_t e = k + 1;
				while (e < t.size() && t[e].kind == TokenKind::IDENTIFIER) {
					extends += t[e].text; // qualified names keep all segments (resolveJavaHandlerRegistrations)
					if (e + 2 < t.size() && t[e + 1].isPunct(".") && t[e + 2].kind == TokenKind::IDENTIFIER) {
						extends += '.';
						e += 2;
					} else {
						break;
					}
				}
			}
			k++;
		}
		size_t open = k;
		size_t close = matchingBrace(open);
		if (close >= t.size()) {
			error(keyword, "unbalanced braces in type declaration");
			return t.size();
		}
		if (!isClass)
			return close + 1; // interfaces, enums, records and annotation types never register (nested classes in them are not scanned)

		std::string simpleName(nameToken.text);
		const std::string& package = source().package;
		JavaHandlerClass cls;
		cls.javaClass = !enclosing.empty() ? enclosing + "." + simpleName : package.empty() ? simpleName : package + "." + simpleName;
		cls.file = std::string(file);
		cls.line = nameToken.line;
		cls.column = nameToken.column;
		cls.extends = std::move(extends);
		cls.enclosingClass = enclosing;
		cls.source = sourceIndex;
		// Class.getModifiers of a member class: a non-static (inner) class has no no-argument constructor for the loaders to call
		cls.instantiable = modifiers.isPublic && !modifiers.isAbstract && (enclosing.empty() || modifiers.isStatic);
		if (cls.instantiable) {
			if (category == "ai") {
				if (const Annotation* a = findAnnotation(annotations, "AIName")) {
					auto it = a->values.find("value");
					if (it == a->values.end() || !it->second.text || it->second.text->empty())
						error(*a->at, "@AIName without a string value");
					else
						cls.aiName = *it->second.text;
					cls.registered = true;
				}
			} else if (category == "instance") {
				if (const Annotation* a = findAnnotation(annotations, "InstanceID")) {
					auto it = a->values.find("value");
					if (it == a->values.end() || !it->second.number)
						error(*a->at, "@InstanceID without an int value");
					else
						cls.instanceId = *it->second.number;
					cls.registered = true;
				}
			} else if (category == "zone") {
				if (const Annotation* a = findAnnotation(annotations, "ZoneNameAnnotation")) {
					auto value = a->values.find("value");
					auto questId = a->values.find("questId");
					if (value == a->values.end() || !value->second.text || (questId != a->values.end() && !questId->second.number))
						error(*a->at, "@ZoneNameAnnotation without a string value or with a non-int questId");
					else
						cls.zone = JavaZoneKey{*value->second.text, questId == a->values.end() ? 0 : *questId->second.number};
					cls.registered = true;
				}
			} else if (category == "quest") {
				cls.constructorSuperId = findQuestId(open, close); // used if the superclass chain reaches AbstractQuestHandler
			}
		}
		std::string javaClass = cls.javaClass;
		scan.classes.push_back(std::move(cls));
		scanClassBody(open, close, javaClass);
		return close + 1;
	}

	/** Finds the member type declarations of a class body (tokens open..close are the braces); method bodies and initializers are skipped. */
	void scanClassBody(size_t open, size_t close, const std::string& enclosing) {
		std::vector<Annotation> annotations;
		Modifiers modifiers;
		size_t i = open + 1;
		while (i < close) {
			const Token& tok = t[i];
			if (tok.isPunct("@") && i + 1 < close && t[i + 1].kind == TokenKind::IDENTIFIER && t[i + 1].text != "interface") {
				Annotation annotation;
				i = parseAnnotation(i, annotation);
				annotations.push_back(std::move(annotation));
			} else if (modifiers.apply(tok)) {
				i++;
			} else if (isTypeDeclarationStart(i)) {
				i = parseTypeDeclaration(i + (tok.isPunct("@") ? 1 : 0), annotations, modifiers, enclosing);
				annotations.clear();
				modifiers = {};
			} else if (tok.isPunct("{")) {
				i = matchingBrace(i) + 1; // method body, initializer block, array initializer or anonymous class body
				annotations.clear();
				modifiers = {};
			} else {
				if (tok.isPunct(";")) {
					annotations.clear();
					modifiers = {};
				}
				i++;
			}
		}
	}

	/** Java: the quest id passed to super(...) in the constructor: an int literal or an int constant of the class; nullopt if not found */
	std::optional<int32_t> findQuestId(size_t open, size_t close) {
		std::map<std::string, int32_t, std::less<>> constants;
		const Token* superArgument = nullptr;
		int depth = 0;
		for (size_t k = open; k < close; k++) {
			const Token& tok = t[k];
			if (tok.isPunct("{")) {
				depth++;
				continue;
			}
			if (tok.isPunct("}")) {
				depth--;
				continue;
			}
			if (depth == 1 && tok.isIdentifier("int") && k + 4 < close && t[k + 1].kind == TokenKind::IDENTIFIER && t[k + 2].isPunct("=") &&
				t[k + 3].kind == TokenKind::NUMBER && t[k + 4].isPunct(";")) {
				if (auto value = parseInt(t[k + 3]))
					constants[std::string(t[k + 1].text)] = *value;
			}
			if (depth == 2 && superArgument == nullptr && tok.isIdentifier("super") && !t[k - 1].isPunct(".") && k + 4 < close && t[k + 1].isPunct("(") &&
				t[k + 3].isPunct(")") && t[k + 4].isPunct(";")) {
				superArgument = &t[k + 2];
			}
		}
		if (superArgument != nullptr) {
			if (superArgument->kind == TokenKind::NUMBER) {
				if (auto value = parseInt(*superArgument))
					return value;
			} else if (superArgument->kind == TokenKind::IDENTIFIER) {
				if (auto it = constants.find(superArgument->text); it != constants.end())
					return it->second;
			}
		}
		return std::nullopt;
	}

	std::string_view file;
	std::string_view relPath;
	std::string_view category;
	JavaHandlerScan& scan;
	LexResult lexed;
	const std::vector<Token>& t;
	size_t sourceIndex = 0;
};

} // namespace

void scanJavaHandlerFile(std::string_view file, std::string_view relPath, std::string_view content, JavaHandlerScan& scan) {
	if (scan.resolved)
		throw std::logic_error("scanJavaHandlerFile after resolveJavaHandlerRegistrations");
	scan.files++;
	JavaFileScanner(file, relPath, content, scan).run();
	if (relPath.starts_with("ai/") || relPath.starts_with("instance/") || relPath.starts_with("quest/"))
		findSpawnNpcIds(content, scan.npcIds);
}

namespace {

enum class BaseKind { NONE, QUEST, ADMIN, PLAYER, CONSOLE, CHAT, UNRESOLVED, CYCLE };

constexpr std::string_view QUEST_HANDLERS = "com.aionemu.gameserver.questEngine.handlers";
constexpr std::string_view CHAT_HANDLERS = "com.aionemu.gameserver.utils.chathandlers";

struct CoreClass {
	std::string_view fqn;
	std::string_view superclass; // empty for the roots
	BaseKind kind;               // NONE: look at the superclass
};

/**
 * The game-server core classes a handler can extend to become a quest handler or command: the roots and every core subclass of them
 * (game-server/src: `grep -rE "extends (AbstractTemplateQuestHandler|AbstractQuestHandler|MonsterHunt|ChatCommand)"`).
 */
constexpr CoreClass CORE_CLASSES[] = {
	{"com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler", "", BaseKind::QUEST},
	{"com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler", "com.aionemu.gameserver.questEngine.handlers.AbstractQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.CraftingRewards", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.FountainRewards", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.ItemCollecting", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.ItemOrders", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.KillInWorld", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.KillInZone", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.KillSpawned", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.MentorMonsterHunt", "com.aionemu.gameserver.questEngine.handlers.template.MonsterHunt",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.MonsterHunt", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.RelicRewards", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.ReportOnLevelUp", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.ReportTo", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.ReportToMany", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.SkillUse", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.WorkOrders", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.questEngine.handlers.template.XmlQuest", "com.aionemu.gameserver.questEngine.handlers.template.AbstractTemplateQuestHandler",
		BaseKind::NONE},
	{"com.aionemu.gameserver.utils.chathandlers.AdminCommand", "com.aionemu.gameserver.utils.chathandlers.ChatCommand", BaseKind::ADMIN},
	{"com.aionemu.gameserver.utils.chathandlers.ChatCommand", "", BaseKind::CHAT},
	{"com.aionemu.gameserver.utils.chathandlers.ConsoleCommand", "com.aionemu.gameserver.utils.chathandlers.ChatCommand", BaseKind::CONSOLE},
	{"com.aionemu.gameserver.utils.chathandlers.PlayerCommand", "com.aionemu.gameserver.utils.chathandlers.ChatCommand", BaseKind::PLAYER},
};

const CoreClass* findCore(std::string_view fqn) noexcept {
	for (const CoreClass& core : CORE_CLASSES) {
		if (core.fqn == fqn)
			return &core;
	}
	return nullptr;
}

const CoreClass* findCoreBySimpleName(std::string_view simpleName) noexcept {
	for (const CoreClass& core : CORE_CLASSES) {
		if (core.fqn.substr(core.fqn.rfind('.') + 1) == simpleName)
			return &core;
	}
	return nullptr;
}

bool inPackage(std::string_view fqn, std::string_view package) noexcept {
	return fqn.size() > package.size() && fqn.starts_with(package) && fqn[package.size()] == '.';
}

/** a class of the core handler packages: extending one that is not in CORE_CLASSES leaves the chain unknown */
bool inHandlerPackages(std::string_view fqn) noexcept {
	return inPackage(fqn, QUEST_HANDLERS) || inPackage(fqn, CHAT_HANDLERS);
}

/** kind of a core class, following its core superclasses */
BaseKind coreKind(const CoreClass* core) noexcept {
	for (int depth = 0; core != nullptr && depth < 16; depth++) {
		if (core->kind != BaseKind::NONE)
			return core->kind;
		core = findCore(core->superclass);
	}
	return BaseKind::UNRESOLVED;
}

class RegistrationResolver {
public:
	explicit RegistrationResolver(JavaHandlerScan& scan) : scan(scan) {
		for (size_t i = 0; i < scan.classes.size(); i++)
			byName.try_emplace(scan.classes[i].javaClass, i);
	}

	void run() {
		for (JavaHandlerClass& cls : scan.classes) {
			if (!cls.instantiable)
				continue;
			std::string detail;
			BaseKind kind = chainKind(cls, detail);
			switch (kind) {
				case BaseKind::QUEST:
					if (scan.sources[cls.source].category != "quest")
						break; // QuestHandlerLoader only sees data/handlers/quest
					cls.registered = true;
					cls.questId = cls.constructorSuperId;
					if (!cls.questId)
						error(cls, "cannot determine the quest id of " + simpleName(cls) + ": expected super(<int literal or int constant>) in its constructor");
					break;
				case BaseKind::ADMIN:
				case BaseKind::PLAYER:
				case BaseKind::CONSOLE:
					cls.registered = true;
					cls.command = kind == BaseKind::ADMIN ? CommandKind::ADMIN : kind == BaseKind::PLAYER ? CommandKind::PLAYER : CommandKind::CONSOLE;
					break;
				case BaseKind::CHAT:
					error(cls, cls.javaClass + " extends ChatCommand without AdminCommand, PlayerCommand or ConsoleCommand; regscan has no command kind for it");
					break;
				case BaseKind::UNRESOLVED:
					error(cls, "cannot resolve the superclass chain of " + cls.javaClass + ": " + detail +
						" is not a core handler base class known to regscan (JavaScanner.cpp CORE_CLASSES)");
					break;
				case BaseKind::CYCLE:
					error(cls, "cyclic superclass chain of " + cls.javaClass + " (" + detail + ")");
					break;
				case BaseKind::NONE:
					break;
			}
		}
	}

private:
	enum class Found { SCANNED, CORE, EXTERNAL, UNRESOLVED };
	struct Resolution {
		Found found = Found::EXTERNAL;
		size_t index = 0;               // SCANNED
		const CoreClass* core = nullptr; // CORE
		std::string name;               // qualified name as far as known (messages)
	};

	static std::string simpleName(const JavaHandlerClass& cls) { return cls.javaClass.substr(cls.javaClass.rfind('.') + 1); }

	void error(const JavaHandlerClass& cls, std::string message) {
		scan.errors.push_back(Diagnostic{cls.file, cls.line, cls.column, std::move(message)});
	}

	/** a fully qualified candidate: a scanned class, a core class, or nothing */
	std::optional<Resolution> lookup(const std::string& fqn) const {
		if (auto it = byName.find(fqn); it != byName.end())
			return Resolution{Found::SCANNED, it->second, nullptr, fqn};
		if (const CoreClass* core = findCore(fqn))
			return Resolution{Found::CORE, 0, core, fqn};
		return std::nullopt;
	}

	/** Java name resolution of a superclass name as written in `cls` (JLS 6.5.5, as far as handler sources need it) */
	Resolution resolve(const JavaHandlerClass& cls, std::string_view written) const {
		const JavaSource& source = scan.sources[cls.source];
		size_t dot = written.find('.');
		std::string first(written.substr(0, dot));
		std::string rest = dot == std::string_view::npos ? std::string() : std::string(written.substr(dot));

		// 1. member types of the enclosing classes
		for (std::string enclosing = cls.enclosingClass; !enclosing.empty();) {
			if (auto found = lookup(enclosing + "." + first + rest))
				return *found;
			auto it = byName.find(enclosing);
			enclosing = it == byName.end() ? std::string() : scan.classes[it->second].enclosingClass;
		}
		// 2. single-type imports
		if (auto it = source.singleImports.find(first); it != source.singleImports.end()) {
			std::string fqn = it->second + rest;
			if (auto found = lookup(fqn))
				return *found;
			return Resolution{inHandlerPackages(fqn) ? Found::UNRESOLVED : Found::EXTERNAL, 0, nullptr, fqn};
		}
		// 3. the class's own package
		if (auto found = lookup(source.package.empty() ? first + rest : source.package + "." + first + rest))
			return *found;
		// 4. on-demand imports
		std::optional<std::string> unknownInHandlerPackage;
		for (const std::string& package : source.onDemandImports) {
			std::string fqn = package + "." + first + rest;
			if (auto found = lookup(fqn))
				return *found;
			if (!unknownInHandlerPackage && inHandlerPackages(fqn))
				unknownInHandlerPackage = fqn;
		}
		// 5. a fully qualified name as written
		if (!rest.empty()) {
			std::string fqn(written);
			if (auto found = lookup(fqn))
				return *found;
			if (inHandlerPackages(fqn))
				return Resolution{Found::UNRESOLVED, 0, nullptr, fqn};
		}
		// 6. a core base by its simple name (handler sources without the import, e.g. unit test snippets)
		if (rest.empty()) {
			if (const CoreClass* core = findCoreBySimpleName(first))
				return Resolution{Found::CORE, 0, core, std::string(core->fqn)};
		}
		if (unknownInHandlerPackage)
			return Resolution{Found::UNRESOLVED, 0, nullptr, *unknownInHandlerPackage};
		return Resolution{Found::EXTERNAL, 0, nullptr, std::string(written)}; // java.lang, other core packages, libraries
	}

	BaseKind chainKind(const JavaHandlerClass& start, std::string& detail) const {
		std::set<const JavaHandlerClass*> visited{&start};
		const JavaHandlerClass* current = &start;
		while (!current->extends.empty()) {
			Resolution resolution = resolve(*current, current->extends);
			switch (resolution.found) {
				case Found::SCANNED: {
					const JavaHandlerClass* next = &scan.classes[resolution.index];
					if (!visited.insert(next).second) {
						detail = next->javaClass;
						return BaseKind::CYCLE;
					}
					current = next;
					break;
				}
				case Found::CORE: {
					BaseKind kind = coreKind(resolution.core);
					if (kind == BaseKind::UNRESOLVED)
						detail = resolution.name;
					return kind;
				}
				case Found::UNRESOLVED:
					detail = resolution.name;
					return BaseKind::UNRESOLVED;
				case Found::EXTERNAL:
					return BaseKind::NONE;
			}
		}
		return BaseKind::NONE;
	}

	JavaHandlerScan& scan;
	std::map<std::string, size_t, std::less<>> byName;
};

} // namespace

void resolveJavaHandlerRegistrations(JavaHandlerScan& scan) {
	if (scan.resolved)
		throw std::logic_error("resolveJavaHandlerRegistrations called twice");
	scan.resolved = true;
	RegistrationResolver(scan).run();
}

std::vector<JavaClientPacket> scanJavaClientPacketFactory(std::string_view file, std::string_view content, std::vector<Diagnostic>& errors) {
	LexResult lexed = lex(content, Language::JAVA);
	for (Diagnostic& d : lexed.errors) {
		d.file = std::string(file);
		errors.push_back(std::move(d));
	}
	const std::vector<Token>& t = lexed.tokens;
	std::vector<JavaClientPacket> packets;
	// packets [ N ] = new PacketInfo < > ( CM_X . class
	for (size_t i = 0; i + 10 < t.size(); i++) {
		if (!t[i].isIdentifier("packets") || !t[i + 1].isPunct("[") || t[i + 2].kind != TokenKind::NUMBER || !t[i + 3].isPunct("]") || !t[i + 4].isPunct("="))
			continue;
		if (!t[i + 5].isIdentifier("new") || !t[i + 6].isIdentifier("PacketInfo") || !t[i + 7].isPunct("<") || !t[i + 8].isPunct(">") ||
			!t[i + 9].isPunct("(") || t[i + 10].kind != TokenKind::IDENTIFIER) {
			errors.push_back(Diagnostic{std::string(file), t[i].line, t[i].column, "unexpected packet table entry"});
			continue;
		}
		int32_t opcode = 0;
		auto [ptr, ec] = std::from_chars(t[i + 2].text.data(), t[i + 2].text.data() + t[i + 2].text.size(), opcode);
		if (ec != std::errc() || ptr != t[i + 2].text.data() + t[i + 2].text.size()) {
			errors.push_back(Diagnostic{std::string(file), t[i].line, t[i].column, "unexpected opcode in packet table entry"});
			continue;
		}
		packets.push_back(JavaClientPacket{std::string(t[i + 10].text), opcode, t[i].line});
	}
	return packets;
}

} // namespace aion::gameserver::tools::regscan
