// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef FACTION_HPP
#define FACTION_HPP

#include "../common/mmo.hpp"

#include "unit.hpp"

// Max factions
#define MAX_FACTION 3
// Max alliances of each faction
#define MAX_FACTION_ALLIANCE 2
// Max effects for faction aura
#define MAX_AURA_EFF 3
// Max relics of each faction
#define MAX_RELIC 1
// Uncomment to enable
///#define FUNCTORAURA 1

// Voting data
struct voting_data {
	int char_id, faction_id, votes;
	char name[NAME_LENGTH];
	bool voted;
};

// Faction data
struct s_faction_db {
	int id;
	int alliance[MAX_FACTION_ALLIANCE];
	std::string name;
	std::string pl_name;
	std::string map;
	std::string rp_race;
	uint16 x, y;
	uint16 hcolor, hstyle, ccolor;
	int leader_id;
#ifdef FUNCTORNICK
	int color_nicks_group_id;
#endif
	int race, ele, ele_lvl, size, aura_leader;
	int aura[MAX_AURA_EFF];
	unsigned long chat_color;
#ifdef FUNCTORAURA
	struct
	{
		unsigned char part_3;
		unsigned char part_2;
		unsigned char part_1;
		unsigned char option;
	} auras;

	unsigned int aura_data;
#endif
	struct script_code* script;
	struct script_code* aura_bonus;
	std::vector<uint32> alliance_ids;
	bool voting_active;

	int emblem_len, emblem_id;
	char emblem_data[2048];

	int l_emblem_len, l_emblem_id;
	char l_emblem_data[2048];

	struct {
		uint16 item_id;
		bool active;
	} relic[MAX_RELIC];
	Channel* channel;

	~s_faction_db() {
		if (this->script) {
			script_free_code(this->script);
			this->script = nullptr;
		}

		if (this->aura_bonus) {
			script_free_code(this->aura_bonus);
			this->aura_bonus = nullptr;
		}
	}
};

class FactionDatabase : public TypesafeYamlDatabase<uint32, s_faction_db> {
public:
	FactionDatabase() : TypesafeYamlDatabase("FACTION_DB", 1) {

	}

	const std::string getDefaultLocation() override;
	uint64 parseBodyNode(const ryml::NodeRef& node) override;
};

extern FactionDatabase faction_db;

#define faction_check_chat(sd) ( (sd)->status.faction_id>0 && map_getmapflag((sd)->m, MF_FVF) && battle_config.faction_chat_settings )

#define faction_check_hp(sd,dstsd) ( (sd)->status.faction_id>0 && map_getmapflag((dstsd)->m, MF_FVF) && battle_config.fvf_hp_bar && \
		(sd)->status.faction_id == (dstsd)->status.faction_id )

#define faction_check_name(src,tbl) ( faction_get_id(src) && faction_get_id(tbl) && \
		(faction_check_alliance(src,tbl) || \
		status_get_party_id(src)>0 && status_get_party_id(src) == status_get_party_id(tbl) || \
		status_get_guild_id(src)>0 && status_get_guild_id(src) == status_get_guild_id(tbl)) )

#define faction_check_skill_use(src,tbl) ( battle_config.faction_heal_settings && battle_config.faction_heal_bl&(tbl)->type && \
		((battle_config.faction_heal_settings&1 && faction_get_id(src) == faction_get_id(tbl)) || \
		(battle_config.faction_heal_settings&2 && faction_check_alliance(src,tbl)) || \
		(status_get_party_id(src)>0 && status_get_party_id(src) == status_get_party_id(tbl)) || \
		(status_get_guild_id(src)>0 && status_get_guild_id(src) == status_get_guild_id(tbl))) )

class map_session_data;
struct block_list;

void faction_load_leader(map_session_data*);
void faction_change_leader(int, int);
void faction_voting_add(map_session_data*, map_session_data*, int);
void faction_voting_finish(int);
void faction_voting_start(int);
void faction_voting_info(int);
struct voting_data* voting_search(int);

int faction_check_leader(map_session_data*);

int faction_reload_fvf_sub(struct block_list*, va_list);
int faction_relic_change_sub(map_session_data*, va_list);

void faction_factionaura(map_session_data*);

void faction_calc(struct block_list*);
void faction_hp(const map_session_data*);
#ifdef FACTION_ICONS
void faction_effect_icon(map_session_data* sd);
#endif
#ifdef FACTION_RACE_ICONS
void faction_race_effect_icon(map_session_data* sd);
#endif

void faction_spawn(const struct block_list*);
void faction_show_aura(const struct block_list*);
void faction_show_aura_leader(const struct block_list*);
void faction_getareachar_unit(map_session_data*, struct block_list*);
int faction_aura_clear(struct block_list*, va_list);
int faction_check_alliance(const struct block_list*, const struct block_list*);
int faction_get_id(const struct block_list*);

void do_init_faction();
void do_final_faction();
void faction_db_reload();

#endif /* FACTION_HPP */
