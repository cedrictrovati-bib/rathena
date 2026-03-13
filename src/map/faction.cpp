// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "faction.hpp"

#include <unordered_map>

#include "../common/db.hpp"
#include "../common/malloc.hpp"
#include "../common/nullpo.hpp"
#include "../common/socket.hpp"
#include "../common/showmsg.hpp"
#include "../common/strlib.hpp"

#include "atcommand.hpp"
#include "elemental.hpp"
#include "homunculus.hpp"
#include "map.hpp"
#include "mercenary.hpp"
#include "mapreg.hpp"
#include "mob.hpp"
#include "npc.hpp"
#include "pc.hpp"
#include "pet.hpp"
#include "skill.hpp"
#include "status.hpp"
#include "script.hpp"
#include "channel.hpp"

#include <stdlib.h>

using namespace rathena;

FactionDatabase faction_db;
static DBMap* voting_db; // int char_id -> struct voting_data*

const std::string FactionDatabase::getDefaultLocation() {
	return std::string(db_path) + "/faction_db.yml";
}

/**
 * Reads and parses an entry from the faction_db
 * @param node: The YAML node containing the entry
 * @return count of successfully parsed rows
 */
uint64 FactionDatabase::parseBodyNode(const ryml::NodeRef& node) {
	uint32 id;

	if (!this->asUInt32(node, "Id", id))
		return 0;

	std::shared_ptr<s_faction_db> fc = this->find(id);
	bool exists = fc != nullptr;

	if (!exists) {
		if (!this->nodesExist(node, { "Name" }))
			return 0;

		fc = std::make_shared<s_faction_db>();
		fc->id = id;
	}

	if (this->nodeExists(node, "Name")) {
		std::string name;

		if (!this->asString(node, "Name", name))
			return 0;

		if (name.size() > NAME_LENGTH) {
			this->invalidWarning(node["Name"], "Name \"%s\" exceeds maximum of %d characters, capping...\n", name.c_str(), NAME_LENGTH - 1);
		}

		name.resize(NAME_LENGTH);
		fc->name = name;
	}

	if (this->nodeExists(node, "PlayerName")) {
		std::string playerName;

		if (!this->asString(node, "PlayerName", playerName))
			return 0;

		if (playerName.size() > NAME_LENGTH) {
			this->invalidWarning(node["PlayerName"], "PlayerName \"%s\" exceeds maximum of %d characters, capping...\n", playerName.c_str(), NAME_LENGTH - 1);
		}

		playerName.resize(NAME_LENGTH);
		fc->pl_name = playerName;
	}

	if (this->nodeExists(node, "HomeMap")) {
		std::string map_name;
		uint16 mapindex;

		if (!this->asString(node, "HomeMap", map_name))
			return 0;

		if (map_name.size() > MAP_NAME_LENGTH) {
			this->invalidWarning(node["HomeMap"], "HomeMap \"%s\" exceeds maximum of %d characters, capping...\n", map_name.c_str(), MAP_NAME_LENGTH - 1);
		}

		mapindex = mapindex_name2id(map_name.c_str());

		if (mapindex == 0) {
			this->invalidWarning(node["HomeMap"], "Invalid HomeMap name %s, skipping.\n", map_name.c_str());
			return 0;
		}
		fc->map = map_name;
	}

	if (this->nodeExists(node, "HomeX")) {
		int x;

		if (!this->asInt32(node, "HomeX", x))
			return 0;

		if (x < 0 || x > 512) {
			this->invalidWarning(node["HomeX"], "HomeX position %d cannot be more than 0 and less than 512, capping to 0.\n", x);
			x = 0;
		}

		fc->x = x;
	}
	else {
		if (!exists)
			fc->x = 0;
	}

	if (this->nodeExists(node, "HomeY")) {
		int y;

		if (!this->asInt32(node, "HomeY", y))
			return 0;

		if (y < 0 || y > 512) {
			this->invalidWarning(node["HomeY"], "HomeY position %d cannot be more than 0 and less than 512, capping to 0.\n", y);
			y = 0;
		}

		fc->y = y;
	}
	else {
		if (!exists)
			fc->y = 0;
	}

	if (this->nodeExists(node, "Size")) {
		std::string size;

		if (!this->asString(node, "Size", size))
			return 0;

		std::string size_constant = "Size_" + size;
		int64 constant;

		if (!script_get_constant(size_constant.c_str(), &constant)) {
			this->invalidWarning(node["Size"], "Unknown size %s, defaulting to Medium.\n", size.c_str());
			constant = SZ_MEDIUM;
		}

		if (constant < SZ_SMALL || constant > SZ_BIG) {
			this->invalidWarning(node["Size"], "Invalid size %s, defaulting to Medium.\n", size.c_str());
			constant = SZ_MEDIUM;
		}

		fc->size = static_cast<e_size>(constant);
	}
	else {
		if (!exists)
			fc->size = SZ_MEDIUM;
	}

	if (this->nodeExists(node, "Race")) {
		std::string race;

		if (!this->asString(node, "Race", race))
			return 0;

		std::string race_constant = "RC_" + race;
		int64 constant;

		if (!script_get_constant(race_constant.c_str(), &constant)) {
			this->invalidWarning(node["Race"], "Unknown race %s, defaulting to Demi-Human.\n", race.c_str());
			constant = RC_DEMIHUMAN;
		}

		if (!CHK_RACE(constant)) {
			this->invalidWarning(node["Race"], "Invalid race %s, defaulting to Demi-Human.\n", race.c_str());
			constant = RC_DEMIHUMAN;
		}

		fc->race = static_cast<e_race>(constant);
	}
	else {
		if (!exists)
			fc->race = RC_DEMIHUMAN;
	}

	if (this->nodeExists(node, "Element")) {
		std::string ele;

		if (!this->asString(node, "Element", ele))
			return 0;

		std::string ele_constant = "ELE_" + ele;
		int64 constant;

		if (!script_get_constant(ele_constant.c_str(), &constant)) {
			this->invalidWarning(node["Element"], "Unknown element %s, defaulting to Neutral.\n", ele.c_str());
			constant = ELE_NEUTRAL;
		}

		if (!CHK_ELEMENT(constant)) {
			this->invalidWarning(node["Element"], "Invalid element %s, defaulting to Neutral.\n", ele.c_str());
			constant = ELE_NEUTRAL;
		}

		fc->ele = static_cast<e_element>(constant);
	}
	else {
		if (!exists)
			fc->ele = ELE_NEUTRAL;
	}

	if (this->nodeExists(node, "ElementLevel")) {
		uint16 level;

		if (!this->asUInt16(node, "ElementLevel", level))
			return 0;

		if (!CHK_ELEMENT_LEVEL(level)) {
			this->invalidWarning(node["ElementLevel"], "Invalid element level %hu, defaulting to 1.\n", level);
			level = 1;
		}

		fc->ele_lvl = static_cast<uint8>(level);
	}
	else {
		if (!exists)
			fc->ele_lvl = 1;
	}

	if (this->nodeExists(node, "rpRace")) {
		std::string rprace;

		if (!this->asString(node, "rpRace", rprace))
			return 0;

		rprace.resize(NAME_LENGTH);
		fc->rp_race = rprace;
	}

	if (this->nodeExists(node, "HairColor")) {
		int hairColor;

		if (!this->asInt32(node, "HairColor", hairColor))
			return 0;

		if (hairColor < battle_config.min_hair_color || hairColor > battle_config.max_hair_color) {
			this->invalidWarning(node["HairColor"], "HairColor %d cannot be more than %d and less than %d, capping to min.\n", hairColor, battle_config.min_hair_color, battle_config.max_hair_color);
			hairColor = battle_config.min_hair_color;
		}

		fc->hcolor = hairColor;
	}
	else {
		if (!exists)
			fc->hcolor = battle_config.min_hair_color;
	}

	if (this->nodeExists(node, "HairStyle")) {
		int hairStyle;

		if (!this->asInt32(node, "HairStyle", hairStyle))
			return 0;

		if (hairStyle < battle_config.min_hair_style || hairStyle > battle_config.max_hair_style) {
			this->invalidWarning(node["HairStyle"], "HairStyle %d cannot be more than %d and less than %d, capping to min.\n", hairStyle, battle_config.min_hair_style, battle_config.max_hair_style);
			hairStyle = battle_config.min_hair_style;
		}

		fc->hstyle = hairStyle;
	}
	else {
		if (!exists)
			fc->hstyle = battle_config.min_hair_style;
	}

	if (this->nodeExists(node, "ClothColor")) {
		int clothColor;

		if (!this->asInt32(node, "ClothColor", clothColor))
			return 0;

		if (clothColor < battle_config.min_cloth_color || clothColor > battle_config.max_cloth_color) {
			this->invalidWarning(node["ClothColor"], "ClothColor %d cannot be more than %d and less than %d, capping to min.\n", clothColor, battle_config.min_cloth_color, battle_config.max_cloth_color);
			clothColor = battle_config.min_cloth_color;
		}

		fc->ccolor = clothColor;
	}
	else {
		if (!exists)
			fc->ccolor = battle_config.min_cloth_color;
	}

	if (this->nodeExists(node, "MsgColor")) {
		std::string msgColor;

		if (!this->asString(node, "MsgColor", msgColor))
			return 0;

		if (!(msgColor.compare(0, 2, "0x") == 0 && msgColor.size() == 8 && msgColor.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
			this->invalidWarning(node["MsgColor"], "msgColor %s hexadecimal format not correct, defaulting to black.\n", msgColor.c_str());
			msgColor = "0x000000";
		}

		fc->chat_color = strtoul(msgColor.c_str(), NULL, 0);
	}
	else {
		if (!exists)
			fc->chat_color = strtoul("0x000000", NULL, 0);
	}

#ifdef FUNCTORAURA
	if (this->nodeExists(node, "FunctorAura")) {
		std::string functorAura;
		unsigned long aura_data;

		if (!this->asString(node, "FunctorAura", functorAura))
			return 0;

		if (!(functorAura.compare(0, 2, "0x") == 0 && functorAura.size() == 8 && functorAura.find_first_not_of("0123456789abcdefABCDEF", 2) == std::string::npos)) {
			this->invalidWarning(node["FunctorAura"], "functorAura %s hexadecimal format not correct.\n", functorAura.c_str());
			fc->aura_data = 0;
		}
		else {
			fc->auras.part_1 = std::stoi(functorAura.substr(2, 4));
			fc->auras.part_2 = std::stoi(functorAura.substr(4, 6));
			fc->auras.part_3 = std::stoi(functorAura.substr(6, 8));
			fc->auras.option = 0;

			fc->aura_data = std::stoi(functorAura.c_str(), nullptr, 0);
		}
	}
	else {
		if (!exists)
			fc->aura_data = 0;
	}
#endif

	if (this->nodeExists(node, "Aura")) {
		const ryml::NodeRef& auraNode = node["Aura"];
		int i = 0;

		for (const auto& aurait : auraNode) {
			uint32 aura_id;
			c4::atou(aurait.key(), &aura_id);

			if (i >= MAX_AURA_EFF) {
				this->invalidWarning(node["Aura"], "Maximum amount (%d) of aura effect reached, skipping.\n", MAX_AURA_EFF);
				break;
			}
			fc->aura[i] = aura_id;
			i++;
		}
	}

#ifdef FUNCTORNICK
	if (this->nodeExists(node, "FunctorNick")) {
		int color_nicks_group_id;

		if (!this->asInt32(node, "FunctorNick", color_nicks_group_id))
			return 0;

		fc->color_nicks_group_id = color_nicks_group_id;
	}
	else {
		if (!exists)
			fc->color_nicks_group_id = 0;
	}
#endif

	if (this->nodeExists(node, "Script")) {
		std::string script;

		if (!this->asString(node, "Script", script))
			return 0;

		if (exists && fc->script) {
			script_free_code(fc->script);
			fc->script = nullptr;
		}

		fc->script = parse_script(script.c_str(), this->getCurrentFile().c_str(), this->getLineNumber(node["Script"]), SCRIPT_IGNORE_EXTERNAL_BRACKETS);
	}
	else {
		if (!exists)
			fc->script = nullptr;
	}

	if (this->nodeExists(node, "LeaderAuraBonus")) {
		std::string aura_bonus;

		if (!this->asString(node, "LeaderAuraBonus", aura_bonus))
			return 0;

		if (exists && fc->aura_bonus) {
			script_free_code(fc->aura_bonus);
			fc->aura_bonus = nullptr;
		}

		fc->aura_bonus = parse_script(aura_bonus.c_str(), this->getCurrentFile().c_str(), this->getLineNumber(node["Script"]), SCRIPT_IGNORE_EXTERNAL_BRACKETS);
	}
	else {
		if (!exists)
			fc->aura_bonus = nullptr;
	}

	if (this->nodeExists(node, "Alliance")) {
		const ryml::NodeRef& allianceNode = node["Alliance"];

		for (const auto it : allianceNode) {
			uint32 alliance_faction_id;
			c4::atou(it.key(), &alliance_faction_id);
			bool active;

			if (!this->asBool(allianceNode, std::to_string(alliance_faction_id), active))
				return 0;

			if (active) {
				if (std::find(fc->alliance_ids.begin(), fc->alliance_ids.end(), alliance_faction_id) != fc->alliance_ids.end()) {
					this->invalidWarning(allianceNode, "Alliance faction %d is already part of the list, skipping.\n", alliance_faction_id);
					continue;
				}

				if (fc->alliance_ids.size() >= MAX_FACTION_ALLIANCE) {
					this->invalidWarning(allianceNode, "Maximum amount (%d) of alliance faction reached, skipping.\n", MAX_FACTION_ALLIANCE);
					break;
				}

				fc->alliance_ids.push_back(alliance_faction_id);
			}
			else
				util::vector_erase_if_exists(fc->alliance_ids, alliance_faction_id);
		}
	}

	if (!exists)
		this->put(id, fc);

	return 1;
}

void faction_change_leader(int faction_id, int char_id)
{
	TBL_PC* sd = NULL, * new_sd = NULL;
	std::shared_ptr<s_skill_unit_group> group;
	char output[CHAT_SIZE_MAX];
	std::shared_ptr<s_faction_db> fdb = faction_db.find(faction_id);
	status_change* sc = NULL;

	if (fdb == nullptr)
		return;

	if (fdb->leader_id == char_id)
		return;

	if ((sd = map_charid2sd(fdb->leader_id))) {
		sc = status_get_sc(sd);
		if (sc->getSCE(SC_FACTION_AURA) && (group = skill_id2group(sc->getSCE(SC_FACTION_AURA)->val4))) {
			skill_delunitgroup(group);
			status_change_end(sd, SC_FACTION_AURA, INVALID_TIMER);
		}
	}

	memset(output, '\0', sizeof(output));
	sprintf(output, "$faction_leader_id_%d", faction_id);
	mapreg_setreg(add_str(output), char_id);
	fdb->leader_id = char_id;

	if ((new_sd = map_charid2sd(char_id)))
		faction_factionaura(new_sd);
}

void faction_voting_add(map_session_data* sd, map_session_data* ssd, int votes)
{
	struct voting_data* vdb;

	if (!ssd->status.faction_id)
		return;

	if ((vdb = voting_search(ssd->status.char_id)) == NULL) {
		CREATE(vdb, struct voting_data, 1);
		memcpy(vdb->name, ssd->status.name, NAME_LENGTH);
		vdb->faction_id = ssd->status.faction_id;
		vdb->char_id = ssd->status.char_id;
		vdb->votes = votes;
		idb_put(voting_db, ssd->status.char_id, vdb);
	}
	else vdb->votes += votes;

	if (sd) {
		if ((vdb = voting_search(sd->status.char_id)) == NULL) {
			CREATE(vdb, struct voting_data, 1);
			memcpy(vdb->name, sd->status.name, NAME_LENGTH);
			vdb->faction_id = sd->status.faction_id;
			vdb->char_id = sd->status.char_id;
			vdb->voted = true;
			idb_put(voting_db, sd->status.char_id, vdb);

		}
		else vdb->voted = true;
	}
}

void faction_voting_finish(int faction_id)
{
	std::shared_ptr<s_faction_db> fdb;
	struct voting_data* vdb;
	int max = 0, char_id = 0, k = 0;
	DBIterator* iter;

	if ((fdb = faction_db.find(faction_id)) == nullptr)
		return;

	fdb->voting_active = false;

	iter = db_iterator(voting_db);
	for (vdb = (struct voting_data*)dbi_first(iter); dbi_exists(iter); vdb = (struct voting_data*)dbi_next(iter)) {
		if (vdb->faction_id == faction_id && vdb->votes > max) {
			max = vdb->votes;
			char_id = vdb->char_id;
		}
		k++;
	}
	dbi_destroy(iter);

	faction_change_leader(faction_id, char_id);
}

void faction_voting_start(int faction_id)
{
	std::shared_ptr<s_faction_db> fdb;
	struct voting_data* vdb;
	DBIterator* iter;

	if ((fdb = faction_db.find(faction_id)) == nullptr)
		return;

	iter = db_iterator(voting_db);

	for (vdb = (struct voting_data*)dbi_first(iter); dbi_exists(iter); vdb = (struct voting_data*)dbi_next(iter))
		if (vdb->faction_id == faction_id)
			idb_remove(voting_db, vdb->char_id);

	dbi_destroy(iter);

	fdb->voting_active = true;
}

void faction_voting_info(int faction_id)
{
	int j = 0;
	struct voting_data* vdb = NULL;
	DBIterator* iter = db_iterator(voting_db);
	for (vdb = (struct voting_data*)dbi_first(iter); dbi_exists(iter); vdb = (struct voting_data*)dbi_next(iter)) {
		if (vdb->faction_id == faction_id) {
			mapreg_setreg(reference_uid(add_str("$@voting_charid"), j), vdb->char_id);
			mapreg_setregstr(reference_uid(add_str("$@voting_charname$"), j), vdb->name);
			mapreg_setreg(reference_uid(add_str("$@voting_votes"), j), vdb->votes);
			mapreg_setreg(reference_uid(add_str("$@voting_voted"), j), vdb->voted);
			j++;
		}
	}
	dbi_destroy(iter);
	mapreg_setreg(add_str("$@votinglist_count"), j);
	return;
}

struct voting_data* voting_search(int char_id)
{
	return (struct voting_data*)idb_get(voting_db, char_id);
}

/**
 * Charge le statut de leader pour un joueur lors de sa connexion
 * @param sd : Données de session du joueur
 */
void faction_load_leader(map_session_data* sd) {
	char reg_name[CHAT_SIZE_MAX];
	std::shared_ptr<s_faction_db> fdb;

	if ((fdb = faction_db.find(faction_get_id(sd))) == nullptr)
		return;

	sprintf(reg_name, "$faction_leader_id_%d", sd->status.faction_id);
	int32 leader_id = mapreg_readreg(add_str(reg_name));

	if (fdb->leader_id != leader_id) {
		fdb->leader_id = leader_id;
	}
}

int faction_check_leader(map_session_data* sd)
{
	std::shared_ptr<s_faction_db> fdb;

	if (sd == nullptr) {
		return 0;
	}

	if (sd->status.faction_id <= 0) {
		return 0;
	}

	if ((fdb = faction_db.find(sd->status.faction_id)) == nullptr) {
		return 0;
	}

	if (fdb->leader_id == sd->status.char_id) {
		return 1;
	}

	return 0;
}

int faction_reload_fvf_sub(struct block_list* bl, va_list ap)
{
	if (!faction_get_id(bl))
		return 0;

	switch (bl->type) {
	case BL_PC:
	{
		TBL_PC* sd = (TBL_PC*)bl;
		status_calc_pc(sd, SCO_NONE);
		if (!pc_isdead(sd))
			pc_setpos(sd, sd->mapindex, bl->x, bl->y, CLR_RESPAWN);
	}
	break;

	case BL_NPC:
	case BL_MOB:
	{
		status_change* sc = status_get_sc(bl);
		if (sc->option & (OPTION_HIDE | OPTION_CLOAK | OPTION_CHASEWALK | OPTION_INVISIBLE) || sc->getSCE(SC_CAMOUFLAGE))
			break;
	}
	default:
		clif_spawn(bl);
		break;
	}
	return 0;
}

int faction_relic_change_sub(map_session_data* sd, va_list ap)
{
	int faction_id = va_arg(ap, int);

	if (!sd->status.faction_id || sd->status.faction_id != faction_id)
		return 0;

	faction_calc(sd);
	return 0;
}

void faction_factionaura(map_session_data* sd)
{
	std::shared_ptr<s_skill_unit_group> group;
	status_change* sc;

	if (sd == nullptr)
		return;

	if (!sd->status.faction_id)
		return;

	if (!faction_check_leader(sd))
		return;

	sc = status_get_sc(sd);
	if (sc == nullptr)
		return;

	if (sc->getSCE(SC_FACTION_AURA) && (group = skill_id2group(sc->getSCE(SC_FACTION_AURA)->val4))) {
		skill_delunitgroup(group);
		status_change_end(sd, SC_FACTION_AURA, INVALID_TIMER);
	}

	group = skill_unitsetting(sd, FACTION_AURA, 1, sd->x, sd->y, 0);
	if (group == nullptr)
		return;

	group->faction_id = sd->status.faction_id; // << important

	sc_start4(sd, sd, SC_FACTION_AURA, 100, 1, sd->status.faction_id, 0, group->group_id, 600000);
}

void faction_calc(struct block_list* bl)
{
	std::shared_ptr<s_faction_db> fdb;

	if ((fdb = faction_db.find(faction_get_id(bl))) == nullptr)
		return;

	if (bl->type == BL_PC) {
		TBL_PC* sd = (TBL_PC*)bl;
		std::shared_ptr<item_data> item_data = NULL;
		status_change* sc = status_get_sc(bl);
		int i;

		if (fdb->script)
			run_script(fdb->script, 0, sd->id, 0);

		if (sc && sc->getSCE(SC_FACTION_AURA) && sc->getSCE(SC_FACTION_AURA)->val2 && sc->getSCE(SC_FACTION_AURA)->val2 == sd->status.faction_id) {
			std::shared_ptr<s_faction_db> t_fdb = faction_db.find(sc->getSCE(SC_FACTION_AURA)->val2);
			if (t_fdb->aura_bonus)
				run_script(t_fdb->aura_bonus, 0, sd->id, 0);
		}

		for (i = 0; i < MAX_RELIC; i++)
			if (((map[bl->m].faction.id == sd->status.faction_id && map[bl->m].faction.relic == i) || fdb->relic[i].active) &&
				(item_data = item_db.find(fdb->relic[i].item_id)) && item_data->script)
				run_script(item_data->script, 0, sd->id, 0);

		if (map_getmapflag(bl->m, MF_FVF)) {
			std::string bonus_hp_sp;
			bonus_hp_sp = "bonus bMaxHPRate," + std::to_string(battle_config.fvf_add_hp_rate) + ";\nbonus bMaxSPRate," + std::to_string(battle_config.fvf_add_sp_rate) + ";";
			struct script_code* script = parse_script(bonus_hp_sp.c_str(), "bonus_script_fvf_hp_sp", 0, SCRIPT_IGNORE_EXTERNAL_BRACKETS);
			run_script(script, 0, sd->id, 0);
			script_free_code(script);
		}
	}

	if (battle_config.faction_status_bl & bl->type) {
		struct status_data* status = bl->type == BL_MOB ? status_get_status_data(*bl) : status_get_base_status(bl);

		status->race = fdb->race;
		status->def_ele = fdb->ele;
		status->ele_lv = fdb->ele_lvl;
		status->size = fdb->size;
	}
}

#ifdef FACTION_ICONS
void faction_effect_icon(map_session_data* sd) {
	int i;

	if (sd->status.faction_id) {
		std::shared_ptr<s_faction_db> fdb;
		if ((fdb = faction_db.find(sd->status.faction_id)) != nullptr) {

			for (i = SC_FACTION1; i <= SC_FACTION2; i++) //Put the min and max here
				status_change_end(sd, (sc_type)i, INVALID_TIMER);

			switch (fdb->id) {
			case 2://Hell
				sc_start(sd, sd, SC_FACTION1, 100, 0, INFINITE_TICK);
				break;
			case 3://Earth
				sc_start(sd, sd, SC_FACTION2, 100, 0, INFINITE_TICK);
				break;
			}
		}
	}
}
#endif

#ifdef FACTION_RACE_ICONS
void faction_race_effect_icon(map_session_data* sd) {
	int i;

	if (sd->status.faction_id) {
		std::shared_ptr<s_faction_db> fdb;
		if ((fdb = faction_db.find(sd->status.faction_id)) != nullptr) {

			for (i = SC_RACE1; i <= SC_RACE2; i++) //Put the min and max here
				status_change_end(sd, (sc_type)i, INVALID_TIMER);

			if (strcmp(fdb->rp_race.c_str(), "Dark Elves") == 0)
				sc_start(sd, sd, SC_RACE1, 100, 0, INFINITE_TICK);
			else if (strcmp(fdb->rp_race.c_str(), "Human") == 0)
				sc_start(sd, sd, SC_RACE2, 100, 0, INFINITE_TICK);
		}
	}
}
#endif

void faction_hp(const map_session_data* sd)
{
	uint8 buf[34];
	const int cmd = 0x2e0;
	nullpo_retv(sd);

	WBUFW(buf, 0) = cmd;
	WBUFL(buf, 2) = sd->status.account_id;
	memcpy(WBUFP(buf, 6), sd->status.name, NAME_LENGTH);

	if (sd->battle_status.max_hp > INT16_MAX) {
		WBUFW(buf, 30) = sd->battle_status.hp / (sd->battle_status.max_hp / 100);
		WBUFW(buf, 32) = 100;
	}
	else {
		WBUFW(buf, 30) = sd->battle_status.hp;
		WBUFW(buf, 32) = sd->battle_status.max_hp;
	}
	clif_send(buf, packet_len(cmd), sd, FACTION_AREA_WOS);
}

void faction_spawn(const struct block_list* bl)
{
	std::shared_ptr<s_faction_db> fdb;
	uint8 buf[33];

	if ((fdb = faction_db.find(faction_get_id(bl))) == nullptr)
		return;

	if (map_getmapflag(bl->m, MF_FVF)) {
		if (battle_config.faction_ally_info_bl) {
			if (battle_config.faction_ally_info_bl & bl->type) {
				WBUFW(buf, 0) = 0x2dd;
				WBUFL(buf, 2) = bl->id;
				safestrncpy((char*)WBUFP(buf, 6), status_get_name(*bl), NAME_LENGTH);
				WBUFW(buf, 30) = faction_get_id(bl);
				clif_send(buf, packet_len(0x2dd), bl, FVF_OTHER_AREA_CHAT);
			}
		}
		else {
			WBUFW(buf, 0) = 0x1b4;
			WBUFL(buf, 2) = bl->id;
			WBUFL(buf, 6) = fdb->id;
			WBUFW(buf, 10) = faction_check_leader(((TBL_PC*)bl)) ? fdb->l_emblem_id : fdb->emblem_id;
			clif_send(buf, 12, bl, AREA_WOS);
		}
	}

	if (battle_config.faction_size_bl & bl->type && ((battle_config.fvf_visual_size & 1 && map_getmapflag(bl->m, MF_FVF)) || battle_config.fvf_visual_size & 2)) {
		if (fdb->size == SZ_BIG)
			clif_specialeffect(bl, 423, AREA);
		else if (fdb->size == SZ_SMALL)
			clif_specialeffect(bl, 421, AREA);
	}

	faction_show_aura(bl);
}

void faction_show_aura(const struct block_list* bl)
{
	std::shared_ptr<s_faction_db> fdb = faction_db.find(faction_get_id(bl));
	const status_change* sc = NULL;
	int i;

	if (bl->type & (BL_CHAR | BL_NPC)) {
		sc = status_get_sc(bl);
		if (sc->option & (OPTION_HIDE | OPTION_CLOAK | OPTION_CHASEWALK | OPTION_INVISIBLE) || sc->getSCE(SC_CAMOUFLAGE))
			return;
	}

	if (!((battle_config.faction_aura_settings & 1 && map_getmapflag(bl->m, MF_FVF)) || battle_config.faction_aura_settings & 2))
		return;

	if (battle_config.faction_aura_bl & bl->type) {
		for (i = 0; i < MAX_AURA_EFF; i++)
			if (fdb->aura[i] > 0)
				clif_specialeffect(bl, fdb->aura[i], AREA);
	}
}

void faction_getareachar_unit(map_session_data* sd, struct block_list* bl)
{
	std::shared_ptr<s_faction_db> fdb;
	status_change* sc = NULL;
	int i, fd;

	if (!sd->status.faction_id || (fdb = faction_db.find(faction_get_id(bl))) == NULL)
		return;

	fd = sd->fd;
	if (map_getmapflag(bl->m, MF_FVF)) {
		if (battle_config.faction_ally_info_bl) {
			if (battle_config.faction_ally_info_bl & bl->type && !faction_check_alliance(sd, bl)) {
				WFIFOHEAD(fd, 32);
				WFIFOW(fd, 0) = 0x2dd;
				WFIFOL(fd, 2) = bl->id;
				safestrncpy((char*)WFIFOP(fd, 6), status_get_name(*bl), NAME_LENGTH);
				WFIFOW(fd, 30) = faction_get_id(bl);
				WFIFOSET(fd, packet_len(0x2dd));
			}
		}
		else {
			if (faction_check_leader(((TBL_PC*)bl))) {
				WFIFOHEAD(fd, fdb->l_emblem_len + 12);
				WFIFOW(fd, 2) = fdb->l_emblem_len + 12;
				WFIFOL(fd, 8) = fdb->l_emblem_id;
				memcpy(WFIFOP(fd, 12), fdb->l_emblem_data, fdb->l_emblem_len);
			}
			else {
				WFIFOHEAD(fd, fdb->emblem_len + 12);
				WFIFOW(fd, 2) = fdb->emblem_len + 12;
				WFIFOL(fd, 8) = fdb->emblem_id;
				memcpy(WFIFOP(fd, 12), fdb->emblem_data, fdb->emblem_len);
			}
			WFIFOW(fd, 0) = 0x152;
			WFIFOL(fd, 4) = fdb->id;
			WFIFOSET(fd, WFIFOW(fd, 2));
		}
	}

	if (battle_config.faction_size_bl & bl->type && ((battle_config.fvf_visual_size & 1 && map_getmapflag(bl->m, MF_FVF)) || battle_config.fvf_visual_size & 2)) {
		if (fdb->size == SZ_BIG)
			clif_specialeffect_single(bl, 423, fd);
		else if (fdb->size == SZ_SMALL)
			clif_specialeffect_single(bl, 421, fd);
	}

	if (bl->type & (BL_CHAR | BL_NPC)) {
		sc = status_get_sc(bl);
		if (sc->option & (OPTION_HIDE | OPTION_CLOAK | OPTION_CHASEWALK | OPTION_INVISIBLE) || sc->getSCE(SC_CAMOUFLAGE))
			return;
	}

	if (!((battle_config.faction_aura_settings & 1 && map_getmapflag(bl->m, MF_FVF)) || battle_config.faction_aura_settings & 2)) {
		return;
	}

	if (battle_config.faction_aura_bl & bl->type) {
		for (i = 0; i < MAX_AURA_EFF; i++) {
			if (fdb->aura[i] > 0) {
				clif_specialeffect_single(bl, fdb->aura[i], fd);
			}
		}
	}
}

int faction_aura_clear(struct block_list* bl, va_list ap)
{
	map_session_data* sd = BL_CAST(BL_PC, bl);
	struct block_list* tbl = va_arg(ap, struct block_list*);

	if (bl == tbl)
		return 0;

	clif_getareachar_unit(sd, tbl);
	return 0;
}

/* return 0 if players are not ally
* 					 	not in same faction
* 						or one of them doesn't have a faction
*  return 1 otherwise
*/
int faction_check_alliance(const struct block_list* bl, const struct block_list* t_bl)
{
	std::shared_ptr<s_faction_db> fdb, t_fdb;

	if ((fdb = faction_db.find(faction_get_id(bl))) == NULL ||
		(t_fdb = faction_db.find(faction_get_id(t_bl))) == NULL)
		return 0;

	if (faction_get_id(bl) == faction_get_id(t_bl))
		return 1;

	auto it = std::find(fdb->alliance_ids.begin(), fdb->alliance_ids.end(), t_fdb->id);

	if (it != fdb->alliance_ids.end())
		return 1;

	return 0;
}

int faction_get_id(const struct block_list* bl)
{
	if (!bl)
		return 0;

	switch (bl->type) {
	case BL_PC:
		return ((const TBL_PC*)bl)->status.faction_id;

	case BL_PET:
		return ((const TBL_PET*)bl)->master
			? ((const TBL_PET*)bl)->master->status.faction_id
			: 0;

	case BL_MOB:
	{
		map_session_data* msd;
		const struct mob_data* md = (const TBL_MOB*)bl;

		if (md->special_state.ai && (msd = map_id2sd(md->master_id)) != NULL)
			return msd->status.faction_id;

		return md->faction_id;
	}

	case BL_NPC:
		return ((const TBL_NPC*)bl)->faction_id;

	case BL_HOM:
		return ((const TBL_HOM*)bl)->master
			? ((const TBL_HOM*)bl)->master->status.faction_id
			: 0;

	case BL_MER:
		return ((const TBL_MER*)bl)->master
			? ((const TBL_MER*)bl)->master->status.faction_id
			: 0;

	case BL_ELEM:
		return ((const TBL_ELEM*)bl)->master
			? ((const TBL_ELEM*)bl)->master->status.faction_id
			: 0;

	case BL_SKILL:
		return ((const TBL_SKILL*)bl)->group->faction_id;

	default:
		return 0;
	}
}

static void destroy_faction_data(struct s_faction_db* self, int free_self)
{
	if (self == NULL)
		return;
	if (self->script)
		script_free_code(self->script);
	if (self->aura_bonus)
		script_free_code(self->aura_bonus);
	if (free_self)
		aFree(self);
}

static int voting_final_sub(DBKey key, DBData* data, va_list ap)
{
	struct voting_data* vdb = (struct voting_data*)db_data2ptr(data);

	if (vdb != NULL)
		aFree(vdb);

	return 0;
}

void do_init_faction() {
	if (!voting_db)
		voting_db = idb_alloc(DB_OPT_BASE);
	faction_db.load();
}

void do_final_faction() {
	int i = 1;
	voting_db->destroy(voting_db, voting_final_sub);
	std::shared_ptr<s_faction_db> fdb;
	while ((fdb = faction_db.find(i)) != nullptr) {
		channel_delete(fdb->channel, false);
		i++;
	}
	faction_db.clear();
}

void faction_db_reload(void)
{
	do_final_faction();
	do_init_faction();
}
