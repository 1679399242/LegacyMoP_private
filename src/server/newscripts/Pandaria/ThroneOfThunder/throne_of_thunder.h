//UWoWCore
//Throne of Thunder

#ifndef THRONEOFTHUNDER
#define THRONEOFTHUNDER

#include "SpellScript.h"
#include "Map.h"
#include "Creature.h"
#include "CreatureAIImpl.h"

enum eData
{
    DATA_STORM_CALLER      = 1, //Mini Boss
    DATA_JINROKH           = 2,
    DATA_STORMBRINGER      = 3, //Mini Boss
    DATA_HORRIDON          = 4,
    DATA_COUNCIL_OF_ELDERS = 5,
    DATA_TORTOS            = 6,
    DATA_MEGAERA           = 7,
    DATA_JI_KUN            = 8,
    DATA_DURUMU            = 9,
    DATA_PRIMORDIUS        = 10,
    DATA_DARK_ANIMUS       = 11,
    DATA_IRON_QON          = 12,
    DATA_TWIN_CONSORTS     = 13,
    DATA_LEI_SHEN          = 14,
    DATA_RA_DEN            = 15,
};

enum eCreatures
{
    //Minibosses
    NPC_STORM_CALLER       = 70236,
    NPC_STORMBRINGER       = 70445,

    //Npc
    NPC_LIGHTNING_BALL     = 69232, 
    NPC_WHIRL_TURTLE       = 67966,
    NPC_VAMPIRIC_CAVE_BAT  = 69352,
    NPC_CINDERS            = 70432,
    NPC_LIVING_FLUID       = 69069,

    //Bosses
    NPC_JINROKH            = 69465,
    NPC_HORRIDON           = 68476,
    NPC_JALAK              = 69374,
    //Council of Elders
    NPC_FROST_KING_MALAKK  = 69131,
    NPC_PRINCESS_MARLI     = 69132,
    NPC_KAZRAJIN           = 69134,
    NPC_SUL_SANDCRAWLER    = 69078,
    //
    NPC_TORTOS             = 67977,
    //Megaera
    NPC_FLAMING_HEAD       = 70212,
    NPC_FROZEN_HEAD        = 70235,
    NPC_VENOMOUS_HEAD      = 70247,
    //
    NPC_JI_KUN             = 69712,
    NPC_DURUMU             = 68036,
    NPC_PRIMORDIUS         = 69017,
    NPC_DARK_ANIMUS        = 69427,
    NPC_IRON_QON           = 68078,
    //Twin consorts
    NPC_SULIN              = 68904,
    NPC_LULIN              = 68905,
    //
    NPC_LEI_SHEN           = 68397,
    NPC_RA_DEN             = 69473,
};

enum eGameObjects
{  
    //Jinrokh
    GO_JINROKH_PRE_DOOR    = 218665,
    GO_JINROKH_ENT_DOOR    = 218664,
    GO_MOGU_SR             = 218675,
    GO_MOGU_NR             = 218676,
    GO_MOGU_NL             = 218677,
    GO_MOGU_SL             = 218678,
    GO_JINROKH_EX_DOOR     = 218663,
    //
    //Horridon
    GO_HORRIDON_PRE_DOOR   = 218669,
    GO_HORRIDON_ENT_DOOR   = 218667,
    GO_MAIN_GATE           = 218674,
    GO_FARRAK_GATE         = 218672,
    GO_GURUBASHI_GATE      = 218670,
    GO_DRAKKARI_GATE       = 218671,
    GO_AMANI_GATE          = 218673,
    GO_HORRIDON_EX_DOOR    = 218666,
    //
    //Council of Elders
    GO_COUNCIL_LENT_DOOR   = 218655,
    GO_COUNCIL_RENT_DOOR   = 218656,
    GO_COUNCIL_EX_DOOR     = 218657,
    GO_COUNCIL_EX2_DOOR    = 218469,
    //
    //Tortos
    GO_TORTOS_EX_DOOR      = 218980,
    GO_TORTOS_EX2_DOOR     = 218987,
    //
    //Megaera
    GO_MEGAERA_EX_DOOR     = 218746,
    //
    //Ji Kun
    GO_JI_KUN_FEATHER      = 218543,
    GO_JI_KUN_EX_DOOR      = 218888,
    //
    //Durumu
    GO_DURUMU_EX_DOOR      = 218390,
    //
    //Primordius
    GO_PRIMORDIUS_ENT_DOOR = 218584,
    GO_PRIMORDIUS_EX_DOOR  = 218585,
    //Secret Ra Den door
    GO_S_RA_DEN_ENT_DOOR   = 218553,
    //
};

#endif THRONEOFTHUNDER
