/*
* Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
* Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "SpellMgr.h"
#include "SpellInfo.h"
#include "ObjectMgr.h"
#include "SpellAuras.h"
#include "SpellAuraDefines.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "World.h"
#include "Chat.h"
#include "Spell.h"
#include "BattlegroundMgr.h"
#include "CreatureAI.h"
#include "MapManager.h"
#include "BattlegroundIC.h"
#include "BattlefieldWG.h"
#include "BattlefieldMgr.h"

bool IsPrimaryProfessionSkill(uint32 skill)
{
    SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(skill);
    if (!pSkill)
        return false;

    if (pSkill->categoryId != SKILL_CATEGORY_PROFESSION)
        return false;

    return true;
}

bool IsPartOfSkillLine(uint32 skillId, uint32 spellId)
{
    SkillLineAbilityMapBounds skillBounds = sSpellMgr->GetSkillLineAbilityMapBounds(spellId);
    for (SkillLineAbilityMap::const_iterator itr = skillBounds.first; itr != skillBounds.second; ++itr)
        if (itr->second->skillId == skillId)
            return true;

    return false;
}

DiminishingGroup GetDiminishingReturnsGroupForSpell(SpellInfo const* spellproto, bool triggered)
{
    if (!spellproto || spellproto->IsPositive())
        return DIMINISHING_NONE;

    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (spellproto->Effects[i].ApplyAuraName == SPELL_AURA_MOD_TAUNT)
            return DIMINISHING_TAUNT;
    }

    // Explicit Diminishing Groups
    switch (spellproto->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            // Pet charge effects (Infernal Awakening, Demon Charge)
            if (spellproto->SpellVisual[0] == 2816 && spellproto->SpellIconID == 15)
                return DIMINISHING_CONTROLLED_STUN;
            // Gnaw
            else if (spellproto->Id == 47481)
                return DIMINISHING_CONTROLLED_STUN;
            break;
        }
        // Event spells
        case SPELLFAMILY_UNK1:
            return DIMINISHING_NONE;
        case SPELLFAMILY_MAGE:
        {
            // Frostbite
            if (spellproto->SpellFamilyFlags[1] & 0x80000000)
                return DIMINISHING_ROOT;
            // Shattered Barrier
            else if (spellproto->SpellVisual[0] == 12297)
                return DIMINISHING_ROOT;
            // Deep Freeze
            else if (spellproto->SpellIconID == 2939 && spellproto->SpellVisual[0] == 9963)
                return DIMINISHING_CONTROLLED_STUN;
            // Frost Nova / Freeze (Water Elemental)
            else if (spellproto->SpellIconID == 193)
                return DIMINISHING_CONTROLLED_ROOT;
            // Dragon's Breath
            else if (spellproto->SpellFamilyFlags[0] & 0x800000)
                return DIMINISHING_DRAGONS_BREATH;
            // Ring of Frost
            else if (spellproto->Id == 82691)
                return DIMINISHING_DISORIENT;
            // Slow
            else if (spellproto->Id == 31589)
                return DIMINISHING_LIMITONLY;
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            // Hamstring - limit duration to 10s in PvP
            if (spellproto->SpellFamilyFlags[0] & 0x2)
                return DIMINISHING_LIMITONLY;
            // Charge Stun (own diminishing)
            else if (spellproto->SpellFamilyFlags[0] & 0x01000000)
                return DIMINISHING_CHARGE;
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Curses/etc
            if ((spellproto->SpellFamilyFlags[0] & 0x80000000) || (spellproto->SpellFamilyFlags[1] & 0x200))
                return DIMINISHING_LIMITONLY;
            // Seduction
            else if (spellproto->Id == 6358 || spellproto->Id == 132412)
                return DIMINISHING_FEAR;
            break;
        }
        case SPELLFAMILY_DRUID:
        {
            // Pounce
            if (spellproto->SpellFamilyFlags[0] & 0x20000)
                return DIMINISHING_OPENING_STUN;
            // Entangling Roots
            // Nature's Grasp
            else if (spellproto->SpellFamilyFlags[0] & 0x00000200)
                return DIMINISHING_CONTROLLED_ROOT;
            // Faerie Fire
            else if (spellproto->SpellFamilyFlags[0] & 0x400)
                return DIMINISHING_LIMITONLY;
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // Gouge
            if (spellproto->SpellFamilyFlags[0] & 0x8)
                return DIMINISHING_DISORIENT;
            // Blind
            else if (spellproto->SpellFamilyFlags[0] & 0x1000000)
                return DIMINISHING_FEAR;
            // Paralytic Poison
             else if (spellproto->SpellFamilyFlags[2] & 0x00000001)
                 return DIMINISHING_OPENING_STUN;
            // Crippling poison - Limit to 10 seconds in PvP (No SpellFamilyFlags)
            else if (spellproto->SpellIconID == 163)
                return DIMINISHING_LIMITONLY;
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Hunter's Mark
            if ((spellproto->SpellFamilyFlags[0] & 0x400) && spellproto->SpellIconID == 538)
                return DIMINISHING_LIMITONLY;
            // Scatter Shot (own diminishing)
            else if ((spellproto->SpellFamilyFlags[0] & 0x40000) && spellproto->SpellIconID == 132)
                return DIMINISHING_SCATTER_SHOT;
            // Entrapment (own diminishing)
            else if (spellproto->SpellVisual[0] == 7484 && spellproto->SpellIconID == 20)
                return DIMINISHING_ENTRAPMENT;
            // Wyvern Sting mechanic is MECHANIC_SLEEP but the diminishing is DIMINISHING_DISORIENT
            else if ((spellproto->SpellFamilyFlags[1] & 0x1000) && spellproto->SpellIconID == 1721)
                return DIMINISHING_DISORIENT;
            // Freezing Arrow
            else if (spellproto->SpellFamilyFlags[0] & 0x8)
                return DIMINISHING_DISORIENT;
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Judgement of Justice - limit duration to 10s in PvP
            if (spellproto->SpellFamilyFlags[0] & 0x100000)
                return DIMINISHING_LIMITONLY;
            // Turn Evil
            else if ((spellproto->SpellFamilyFlags[1] & 0x804000) && spellproto->SpellIconID == 309)
                return DIMINISHING_FEAR;
            break;
        }
        case SPELLFAMILY_DEATHKNIGHT:
        {
            // Hungering Cold (no flags)
            if (spellproto->SpellIconID == 2797)
                return DIMINISHING_DISORIENT;
            // Mark of Blood
            else if ((spellproto->SpellFamilyFlags[0] & 0x10000000) && spellproto->SpellIconID == 2285)
                return DIMINISHING_LIMITONLY;
            break;
        }
        default:
            break;
    }

    // Lastly - Set diminishing depending on mechanic
    uint32 mechanic = spellproto->GetAllEffectsMechanicMask();
    if (mechanic & (1 << MECHANIC_CHARM))
        return DIMINISHING_MIND_CONTROL;
    if (mechanic & (1 << MECHANIC_SILENCE) && spellproto->Id != 81261)  // haxx: Solar Beam diminishing with itself
        return DIMINISHING_SILENCE;
    if (mechanic & (1 << MECHANIC_SLEEP))
        return DIMINISHING_SLEEP;
    if (mechanic & ((1 << MECHANIC_SAPPED) | (1 << MECHANIC_POLYMORPH) | (1 << MECHANIC_SHACKLE)))
        return DIMINISHING_DISORIENT;
    // Mechanic Knockout, except Blast Wave
    if (mechanic & (1 << MECHANIC_KNOCKOUT) && spellproto->SpellIconID != 292)
        return DIMINISHING_DISORIENT;
    if (mechanic & (1 << MECHANIC_DISARM))
        return DIMINISHING_DISARM;
    if (mechanic & (1 << MECHANIC_FEAR))
        return DIMINISHING_FEAR;
    if (mechanic & (1 << MECHANIC_STUN))
        return triggered ? DIMINISHING_STUN : DIMINISHING_CONTROLLED_STUN;
    if (mechanic & (1 << MECHANIC_BANISH))
        return DIMINISHING_BANISH;
    if (mechanic & (1 << MECHANIC_ROOT))
        return triggered ? DIMINISHING_ROOT : DIMINISHING_CONTROLLED_ROOT;
    if (mechanic & (1 << MECHANIC_HORROR))
        return DIMINISHING_HORROR;

    return DIMINISHING_NONE;
}

DiminishingReturnsType GetDiminishingReturnsGroupType(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_TAUNT:
        case DIMINISHING_CONTROLLED_STUN:
        case DIMINISHING_STUN:
        case DIMINISHING_OPENING_STUN:
        case DIMINISHING_CYCLONE:
        case DIMINISHING_CHARGE:
            return DRTYPE_ALL;
        case DIMINISHING_LIMITONLY:
        case DIMINISHING_NONE:
            return DRTYPE_NONE;
        default:
            return DRTYPE_PLAYER;
    }
}

DiminishingLevels GetDiminishingReturnsMaxLevel(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_TAUNT:
            return DIMINISHING_LEVEL_TAUNT_IMMUNE;
        default:
            return DIMINISHING_LEVEL_IMMUNE;
    }
}

int32 GetDiminishingReturnsLimitDuration(DiminishingGroup group, SpellInfo const* spellproto)
{
    if (!IsDiminishingReturnsGroupDurationLimited(group))
        return 0;

    // Explicit diminishing duration
    switch (spellproto->SpellFamilyName)
    {
        case SPELLFAMILY_DRUID:
        {
            // Faerie Fire - limit to 40 seconds in PvP (3.1)
            if (spellproto->SpellFamilyFlags[0] & 0x400)
                return 40 * IN_MILLISECONDS;
            break;
        }
        case SPELLFAMILY_HUNTER:
        {
            // Wyvern Sting
            if (spellproto->SpellFamilyFlags[1] & 0x1000)
                return 6 * IN_MILLISECONDS;
            // Hunter's Mark
            if (spellproto->SpellFamilyFlags[0] & 0x400)
                return 120 * IN_MILLISECONDS;
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Repentance - limit to 6 seconds in PvP
            if (spellproto->SpellFamilyFlags[0] & 0x4)
                return 6 * IN_MILLISECONDS;
            break;
        }
        case SPELLFAMILY_WARLOCK:
        {
            // Banish - limit to 6 seconds in PvP
            if (spellproto->SpellFamilyFlags[1] & 0x8000000)
                return 6 * IN_MILLISECONDS;
            // Curse of Tongues - limit to 12 seconds in PvP
            else if (spellproto->SpellFamilyFlags[2] & 0x800)
                return 12 * IN_MILLISECONDS;
            // Curse of Elements - limit to 120 seconds in PvP
            else if (spellproto->SpellFamilyFlags[1] & 0x200)
                return 120 * IN_MILLISECONDS;
            break;
        }
        default:
            break;
    }

    return 8 * IN_MILLISECONDS;
}

bool IsDiminishingReturnsGroupDurationLimited(DiminishingGroup group)
{
    switch (group)
    {
        case DIMINISHING_BANISH:
        case DIMINISHING_CONTROLLED_STUN:
        case DIMINISHING_CONTROLLED_ROOT:
        case DIMINISHING_CYCLONE:
        case DIMINISHING_DISORIENT:
        case DIMINISHING_ENTRAPMENT:
        case DIMINISHING_FEAR:
        case DIMINISHING_HORROR:
        case DIMINISHING_MIND_CONTROL:
        case DIMINISHING_OPENING_STUN:
        case DIMINISHING_ROOT:
        case DIMINISHING_STUN:
        case DIMINISHING_SLEEP:
        case DIMINISHING_LIMITONLY:
            return true;
        default:
            return false;
    }
}

SpellMgr::SpellMgr()
{
}

SpellMgr::~SpellMgr()
{
    UnloadSpellInfoStore();
}

/// Some checks for spells, to prevent adding deprecated/broken spells for trainers, spell book, etc
bool SpellMgr::IsSpellValid(SpellInfo const* spellInfo, Player* player, bool msg)
{
    // not exist
    if (!spellInfo)
        return false;

    bool need_check_reagents = false;

    // check effects
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        switch (spellInfo->Effects[i].Effect)
        {
        case 0:
            continue;

            // craft spell for crafting non-existed item (break client recipes list show)
        case SPELL_EFFECT_CREATE_ITEM:
        case SPELL_EFFECT_CREATE_ITEM_2:
            {
                if (spellInfo->Effects[i].ItemType == 0)
                {
                    // skip auto-loot crafting spells, its not need explicit item info (but have special fake items sometime)
                    if (!spellInfo->IsLootCrafting())
                    {
                        if (msg)
                        {
                            if (player)
                                ChatHandler(player).PSendSysMessage("Craft spell %u not have create item entry.", spellInfo->Id);
                            else
                                sLog->outError(LOG_FILTER_SQL, "Craft spell %u not have create item entry.", spellInfo->Id);
                        }
                        return false;
                    }

                }
                // also possible IsLootCrafting case but fake item must exist anyway
                else if (!sObjectMgr->GetItemTemplate(spellInfo->Effects[i].ItemType))
                {
                    if (msg)
                    {
                        if (player)
                            ChatHandler(player).PSendSysMessage("Craft spell %u create not-exist in DB item (Entry: %u) and then...", spellInfo->Id, spellInfo->Effects[i].ItemType);
                        else
                            sLog->outError(LOG_FILTER_SQL, "Craft spell %u create not-exist in DB item (Entry: %u) and then...", spellInfo->Id, spellInfo->Effects[i].ItemType);
                    }
                    return false;
                }

                need_check_reagents = true;
                break;
            }
        case SPELL_EFFECT_LEARN_SPELL:
            {
                SpellInfo const* spellInfo2 = sSpellMgr->GetSpellInfo(spellInfo->Effects[i].TriggerSpell);
                if (!IsSpellValid(spellInfo2, player, msg))
                {
                    if (msg)
                    {
                        if (player)
                            ChatHandler(player).PSendSysMessage("Spell %u learn to broken spell %u, and then...", spellInfo->Id, spellInfo->Effects[i].TriggerSpell);
                        else
                            sLog->outError(LOG_FILTER_SQL, "Spell %u learn to invalid spell %u, and then...", spellInfo->Id, spellInfo->Effects[i].TriggerSpell);
                    }
                    return false;
                }
                break;
            }
        }
    }

    if (need_check_reagents)
    {
        for (uint8 j = 0; j < MAX_SPELL_REAGENTS; ++j)
        {
            if (spellInfo->Reagent[j] > 0 && !sObjectMgr->GetItemTemplate(spellInfo->Reagent[j]))
            {
                if (msg)
                {
                    if (player)
                        ChatHandler(player).PSendSysMessage("Craft spell %u have not-exist reagent in DB item (Entry: %u) and then...", spellInfo->Id, spellInfo->Reagent[j]);
                    else
                        sLog->outError(LOG_FILTER_SQL, "Craft spell %u have not-exist reagent in DB item (Entry: %u) and then...", spellInfo->Id, spellInfo->Reagent[j]);
                }
                return false;
            }
        }
    }

    return true;
}

bool SpellMgr::IsSpellForbidden(uint32 spellid)
{
    std::list<uint32>::iterator Itr;

    for (Itr = mForbiddenSpells.begin(); Itr != mForbiddenSpells.end(); Itr++)
        if ((*Itr) == spellid)
            return true;

    return false;
}


uint32 SpellMgr::GetSpellDifficultyId(uint32 spellId) const
{
    SpellDifficultySearcherMap::const_iterator i = mSpellDifficultySearcherMap.find(spellId);
    return i == mSpellDifficultySearcherMap.end() ? 0 : (*i).second;
}

void SpellMgr::SetSpellDifficultyId(uint32 spellId, uint32 id)
{
    mSpellDifficultySearcherMap[spellId] = id;
}

uint32 SpellMgr::GetSpellIdForDifficulty(uint32 spellId, Unit const* caster) const
{
    // Dbc supprim�e au passage a MoP
    return spellId;
    /*if (!GetSpellInfo(spellId))
    return spellId;

    if (!caster || !caster->GetMap() || !caster->GetMap()->IsDungeon())
    return spellId;

    uint32 mode = uint32(caster->GetMap()->GetSpawnMode());
    if (mode >= MAX_DIFFICULTY)
    {
    sLog->outError(LOG_FILTER_SPELLS_AURAS, "SpellMgr::GetSpellIdForDifficulty: Incorrect Difficulty for spell %u.", spellId);
    return spellId; //return source spell
    }

    uint32 difficultyId = GetSpellDifficultyId(spellId);
    if (!difficultyId)
    return spellId; //return source spell, it has only REGULAR_DIFFICULTY

    SpellDifficultyEntry const* difficultyEntry = sSpellDifficultyStore.LookupEntry(difficultyId);
    if (!difficultyEntry)
    {
    sLog->outDebug(LOG_FILTER_SPELLS_AURAS, "SpellMgr::GetSpellIdForDifficulty: SpellDifficultyEntry not found for spell %u. This should never happen.", spellId);
    return spellId; //return source spell
    }

    if (difficultyEntry->SpellID[mode] <= 0 && mode > HEROIC_DIFFICULTY)
    {
    sLog->outDebug(LOG_FILTER_SPELLS_AURAS, "SpellMgr::GetSpellIdForDifficulty: spell %u mode %u spell is NULL, using mode %u", spellId, mode, mode - 2);
    mode -= 2;
    }

    if (difficultyEntry->SpellID[mode] <= 0)
    {
    sLog->outError(LOG_FILTER_SQL, "SpellMgr::GetSpellIdForDifficulty: spell %u mode %u spell is 0. Check spelldifficulty_dbc!", spellId, mode);
    return spellId;
    }

    sLog->outDebug(LOG_FILTER_SPELLS_AURAS, "SpellMgr::GetSpellIdForDifficulty: spellid for spell %u in mode %u is %d", spellId, mode, difficultyEntry->SpellID[mode]);
    return uint32(difficultyEntry->SpellID[mode]);*/
}

SpellChainNode const* SpellMgr::GetSpellChainNode(uint32 spell_id) const
{
    SpellChainMap::const_iterator itr = mSpellChains.find(spell_id);
    if (itr == mSpellChains.end())
        return NULL;

    return &itr->second;
}

uint32 SpellMgr::GetFirstSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->first->Id;

    return spell_id;
}

uint32 SpellMgr::GetLastSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->last->Id;

    return spell_id;
}

uint32 SpellMgr::GetNextSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        if (node->next)
            return node->next->Id;

    return 0;
}

uint32 SpellMgr::GetPrevSpellInChain(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        if (node->prev)
            return node->prev->Id;

    return 0;
}

uint8 SpellMgr::GetSpellRank(uint32 spell_id) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
        return node->rank;

    return 0;
}

uint32 SpellMgr::GetSpellWithRank(uint32 spell_id, uint32 rank, bool strict) const
{
    if (SpellChainNode const* node = GetSpellChainNode(spell_id))
    {
        if (rank != node->rank)
            return GetSpellWithRank(node->rank < rank ? node->next->Id : node->prev->Id, rank, strict);
    }
    else if (strict && rank > 1)
        return 0;
    return spell_id;
}

SpellRequiredMapBounds SpellMgr::GetSpellsRequiredForSpellBounds(uint32 spell_id) const
{
    return SpellRequiredMapBounds(mSpellReq.lower_bound(spell_id), mSpellReq.upper_bound(spell_id));
}

SpellsRequiringSpellMapBounds SpellMgr::GetSpellsRequiringSpellBounds(uint32 spell_id) const
{
    return SpellsRequiringSpellMapBounds(mSpellsReqSpell.lower_bound(spell_id), mSpellsReqSpell.upper_bound(spell_id));
}

bool SpellMgr::IsSpellRequiringSpell(uint32 spellid, uint32 req_spellid) const
{
    SpellsRequiringSpellMapBounds spellsRequiringSpell = GetSpellsRequiringSpellBounds(req_spellid);
    for (SpellsRequiringSpellMap::const_iterator itr = spellsRequiringSpell.first; itr != spellsRequiringSpell.second; ++itr)
    {
        if (itr->second == spellid)
            return true;
    }
    return false;
}

const SpellsRequiringSpellMap SpellMgr::GetSpellsRequiringSpell()
{
    return this->mSpellsReqSpell;
}

uint32 SpellMgr::GetSpellRequired(uint32 spell_id) const
{
    SpellRequiredMap::const_iterator itr = mSpellReq.find(spell_id);

    if (itr == mSpellReq.end())
        return 0;

    return itr->second;
}

SpellLearnSkillNode const* SpellMgr::GetSpellLearnSkill(uint32 spell_id) const
{
    SpellLearnSkillMap::const_iterator itr = mSpellLearnSkills.find(spell_id);
    if (itr != mSpellLearnSkills.end())
        return &itr->second;
    else
        return NULL;
}

SpellLearnSpellMapBounds SpellMgr::GetSpellLearnSpellMapBounds(uint32 spell_id) const
{
    return SpellLearnSpellMapBounds(mSpellLearnSpells.lower_bound(spell_id), mSpellLearnSpells.upper_bound(spell_id));
}

bool SpellMgr::IsSpellLearnSpell(uint32 spell_id) const
{
    return mSpellLearnSpells.find(spell_id) != mSpellLearnSpells.end();
}

bool SpellMgr::IsSpellLearnToSpell(uint32 spell_id1, uint32 spell_id2) const
{
    SpellLearnSpellMapBounds bounds = GetSpellLearnSpellMapBounds(spell_id1);
    for (SpellLearnSpellMap::const_iterator i = bounds.first; i != bounds.second; ++i)
        if (i->second.spell == spell_id2)
            return true;
    return false;
}

SpellTargetPosition const* SpellMgr::GetSpellTargetPosition(uint32 spell_id) const
{
    SpellTargetPositionMap::const_iterator itr = mSpellTargetPositions.find(spell_id);
    if (itr != mSpellTargetPositions.end())
        return &itr->second;
    return NULL;
}

SpellSpellGroupMapBounds SpellMgr::GetSpellSpellGroupMapBounds(uint32 spell_id) const
{
    spell_id = GetFirstSpellInChain(spell_id);
    return SpellSpellGroupMapBounds(mSpellSpellGroup.lower_bound(spell_id), mSpellSpellGroup.upper_bound(spell_id));
}

uint32 SpellMgr::IsSpellMemberOfSpellGroup(uint32 spellid, SpellGroup groupid) const
{
    SpellSpellGroupMapBounds spellGroup = GetSpellSpellGroupMapBounds(spellid);
    for (SpellSpellGroupMap::const_iterator itr = spellGroup.first; itr != spellGroup.second; ++itr)
    {
        if (itr->second == groupid)
            return true;
    }
    return false;
}

SpellGroupSpellMapBounds SpellMgr::GetSpellGroupSpellMapBounds(SpellGroup group_id) const
{
    return SpellGroupSpellMapBounds(mSpellGroupSpell.lower_bound(group_id), mSpellGroupSpell.upper_bound(group_id));
}

void SpellMgr::GetSetOfSpellsInSpellGroup(SpellGroup group_id, std::set<uint32>& foundSpells) const
{
    std::set<SpellGroup> usedGroups;
    GetSetOfSpellsInSpellGroup(group_id, foundSpells, usedGroups);
}

void SpellMgr::GetSetOfSpellsInSpellGroup(SpellGroup group_id, std::set<uint32>& foundSpells, std::set<SpellGroup>& usedGroups) const
{
    if (usedGroups.find(group_id) != usedGroups.end())
        return;
    usedGroups.insert(group_id);

    SpellGroupSpellMapBounds groupSpell = GetSpellGroupSpellMapBounds(group_id);
    for (SpellGroupSpellMap::const_iterator itr = groupSpell.first; itr != groupSpell.second; ++itr)
    {
        if (itr->second < 0)
        {
            SpellGroup currGroup = (SpellGroup)abs(itr->second);
            GetSetOfSpellsInSpellGroup(currGroup, foundSpells, usedGroups);
        }
        else
        {
            foundSpells.insert(itr->second);
        }
    }
}

bool SpellMgr::AddSameEffectStackRuleSpellGroups(SpellInfo const* spellInfo, int32 amount, std::map<SpellGroup, int32>& groups) const
{
    uint32 spellId = spellInfo->GetFirstRankSpell()->Id;
    SpellSpellGroupMapBounds spellGroup = GetSpellSpellGroupMapBounds(spellId);
    // Find group with SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT if it belongs to one
    for (SpellSpellGroupMap::const_iterator itr = spellGroup.first; itr != spellGroup.second; ++itr)
    {
        SpellGroup group = itr->second;
        SpellGroupStackMap::const_iterator found = mSpellGroupStack.find(group);
        if (found != mSpellGroupStack.end())
        {
            if (found->second == SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT)
            {
                // Put the highest amount in the map
                if (groups.find(group) == groups.end())
                    groups[group] = amount;
                else
                {
                    int32 curr_amount = groups[group];
                    // Take absolute value because this also counts for the highest negative aura
                    if (abs(curr_amount) < abs(amount))
                        groups[group] = amount;
                }
                // return because a spell should be in only one SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT group
                return true;
            }
        }
    }
    // Not in a SPELL_GROUP_STACK_RULE_EXCLUSIVE_SAME_EFFECT group, so return false
    return false;
}

SpellGroupStackRule SpellMgr::CheckSpellGroupStackRules(SpellInfo const* spellInfo1, SpellInfo const* spellInfo2) const
{
    uint32 spellid_1 = spellInfo1->GetFirstRankSpell()->Id;
    uint32 spellid_2 = spellInfo2->GetFirstRankSpell()->Id;
    if (spellid_1 == spellid_2)
        return SPELL_GROUP_STACK_RULE_DEFAULT;
    // find SpellGroups which are common for both spells
    SpellSpellGroupMapBounds spellGroup1 = GetSpellSpellGroupMapBounds(spellid_1);
    std::set<SpellGroup> groups;
    for (SpellSpellGroupMap::const_iterator itr = spellGroup1.first; itr != spellGroup1.second; ++itr)
    {
        if (IsSpellMemberOfSpellGroup(spellid_2, itr->second))
        {
            bool add = true;
            SpellGroupSpellMapBounds groupSpell = GetSpellGroupSpellMapBounds(itr->second);
            for (SpellGroupSpellMap::const_iterator itr2 = groupSpell.first; itr2 != groupSpell.second; ++itr2)
            {
                if (itr2->second < 0)
                {
                    SpellGroup currGroup = (SpellGroup)abs(itr2->second);
                    if (IsSpellMemberOfSpellGroup(spellid_1, currGroup) && IsSpellMemberOfSpellGroup(spellid_2, currGroup))
                    {
                        add = false;
                        break;
                    }
                }
            }
            if (add)
                groups.insert(itr->second);
        }
    }

    SpellGroupStackRule rule = SPELL_GROUP_STACK_RULE_DEFAULT;

    for (std::set<SpellGroup>::iterator itr = groups.begin(); itr!= groups.end(); ++itr)
    {
        SpellGroupStackMap::const_iterator found = mSpellGroupStack.find(*itr);
        if (found != mSpellGroupStack.end())
            rule = found->second;
        if (rule)
            break;
    }
    return rule;
}

const std::vector<SpellProcEventEntry>* SpellMgr::GetSpellProcEvent(uint32 spellId) const
{
    SpellProcEventMap::const_iterator itr = mSpellProcEventMap.find(spellId);
    if (itr != mSpellProcEventMap.end())
        return &itr->second;
    return NULL;
}

bool SpellMgr::IsSpellProcEventCanTriggeredBy(SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag, SpellInfo const* procSpell, uint32 procFlags, uint32 procExtra, bool active)
{
    // No extra req need
    uint32 procEvent_procEx = PROC_EX_NONE;

    // check prockFlags for condition
    if ((procFlags & EventProcFlag) == 0)
        return false;

    bool hasFamilyMask = false;

    /* Check Periodic Auras

    *Dots can trigger if spell has no PROC_FLAG_SUCCESSFUL_NEGATIVE_MAGIC_SPELL
    nor PROC_FLAG_TAKEN_POSITIVE_MAGIC_SPELL

    *Only Hots can trigger if spell has PROC_FLAG_TAKEN_POSITIVE_MAGIC_SPELL

    *Only dots can trigger if spell has both positivity flags or PROC_FLAG_SUCCESSFUL_NEGATIVE_MAGIC_SPELL

    *Aura has to have PROC_FLAG_TAKEN_POSITIVE_MAGIC_SPELL or spellfamily specified to trigger from Hot

    */

    if (procFlags & PROC_FLAG_DONE_PERIODIC)
    {
        if (EventProcFlag & PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_NEG)
        {
            if (!(procExtra & PROC_EX_INTERNAL_DOT))
                return false;
        }
        else if (procExtra & PROC_EX_INTERNAL_HOT)
            procExtra |= PROC_EX_INTERNAL_REQ_FAMILY;
        else if (EventProcFlag & PROC_FLAG_DONE_SPELL_MAGIC_DMG_CLASS_POS)
            return false;
    }

    if (procFlags & PROC_FLAG_TAKEN_PERIODIC)
    {
        if (EventProcFlag & PROC_FLAG_TAKEN_SPELL_MAGIC_DMG_CLASS_POS)
        {
            if (!(procExtra & PROC_EX_INTERNAL_DOT))
                return false;
        }
        else if (procExtra & PROC_EX_INTERNAL_HOT)
            procExtra |= PROC_EX_INTERNAL_REQ_FAMILY;
        else if (EventProcFlag & PROC_FLAG_TAKEN_SPELL_NONE_DMG_CLASS_POS)
            return false;
    }
    // Trap casts are active by default
    if (procFlags & PROC_FLAG_DONE_TRAP_ACTIVATION)
        active = true;

    // Always trigger for this
    if (procFlags & (PROC_FLAG_KILLED | PROC_FLAG_KILL | PROC_FLAG_DEATH))
        return true;

    if (spellProcEvent)     // Exist event data
    {
        // Store extra req
        procEvent_procEx = spellProcEvent->procEx;

        // For melee triggers
        if (procSpell == NULL)
        {
            // Check (if set) for school (melee attack have Normal school)
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
                return false;
        }
        else // For spells need check school/spell family/family mask
        {
            // Check (if set) for school
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & procSpell->SchoolMask) == 0)
                return false;

            // Check (if set) for spellFamilyName
            if (spellProcEvent->spellFamilyName && (spellProcEvent->spellFamilyName != procSpell->SpellFamilyName))
                return false;

            // spellFamilyName is Ok need check for spellFamilyMask if present
            if (spellProcEvent->spellFamilyMask)
            {
                if (!(spellProcEvent->spellFamilyMask & procSpell->SpellFamilyFlags))
                    return false;
                hasFamilyMask = true;
                // Some spells are not considered as active even with have spellfamilyflags
                if (!(procEvent_procEx & PROC_EX_ONLY_ACTIVE_SPELL))
                    active = true;
            }
        }
    }

    if (procExtra & (PROC_EX_INTERNAL_REQ_FAMILY))
    {
        if (!hasFamilyMask)
            return false;
    }

    // Check for extra req (if none) and hit/crit
    if (procEvent_procEx == PROC_EX_NONE)
    {
        // No extra req, so can trigger only for hit/crit - spell has to be active
        if ((procExtra & (PROC_EX_NORMAL_HIT|PROC_EX_CRITICAL_HIT)) && active)
            return true;
    }
    else // Passive spells hits here only if resist/reflect/immune/evade
    {
        if (procExtra & AURA_SPELL_PROC_EX_MASK)
        {
            // if spell marked as procing only from not active spells
            if (active && procEvent_procEx & PROC_EX_NOT_ACTIVE_SPELL)
                return false;
            // if spell marked as procing only from active spells
            if (!active && procEvent_procEx & PROC_EX_ONLY_ACTIVE_SPELL)
                return false;
            // Exist req for PROC_EX_EX_TRIGGER_ALWAYS
            if (procEvent_procEx & PROC_EX_EX_TRIGGER_ALWAYS)
                return true;
            // PROC_EX_NOT_ACTIVE_SPELL and PROC_EX_ONLY_ACTIVE_SPELL flags handle: if passed checks before
            if ((procExtra & (PROC_EX_NORMAL_HIT|PROC_EX_CRITICAL_HIT)) && ((procEvent_procEx & (AURA_SPELL_PROC_EX_MASK)) == 0))
                return true;
        }
        // Check Extra Requirement like (hit/crit/miss/resist/parry/dodge/block/immune/reflect/absorb and other)
        if (procEvent_procEx & procExtra)
            return true;
    }
    return false;
}

SpellProcEntry const* SpellMgr::GetSpellProcEntry(uint32 spellId) const
{
    SpellProcMap::const_iterator itr = mSpellProcMap.find(spellId);
    if (itr != mSpellProcMap.end())
        return &itr->second;
    return NULL;
}

bool SpellMgr::CanSpellTriggerProcOnEvent(SpellProcEntry const& procEntry, ProcEventInfo& eventInfo)
{
    // proc type doesn't match
    if (!(eventInfo.GetTypeMask() & procEntry.typeMask))
        return false;

    // check XP or honor target requirement
    if (procEntry.attributesMask & PROC_ATTR_REQ_EXP_OR_HONOR)
        if (Player* actor = eventInfo.GetActor()->ToPlayer())
            if (eventInfo.GetActionTarget() && !actor->isHonorOrXPTarget(eventInfo.GetActionTarget()))
                return false;

    // always trigger for these types
    if (eventInfo.GetTypeMask() & (PROC_FLAG_KILLED | PROC_FLAG_KILL | PROC_FLAG_DEATH))
        return true;

    // check school mask (if set) for other trigger types
    if (procEntry.schoolMask && !(eventInfo.GetSchoolMask() & procEntry.schoolMask))
        return false;

    // check spell family name/flags (if set) for spells
    if (eventInfo.GetTypeMask() & (PERIODIC_PROC_FLAG_MASK | SPELL_PROC_FLAG_MASK | PROC_FLAG_DONE_TRAP_ACTIVATION))
    {
        if (procEntry.spellFamilyName && (procEntry.spellFamilyName != eventInfo.GetSpellInfo()->SpellFamilyName))
            return false;

        if (procEntry.spellFamilyMask && !(procEntry.spellFamilyMask & eventInfo.GetSpellInfo()->SpellFamilyFlags))
            return false;
    }

    // check spell type mask (if set)
    if (eventInfo.GetTypeMask() & (SPELL_PROC_FLAG_MASK | PERIODIC_PROC_FLAG_MASK))
    {
        if (procEntry.spellTypeMask && !(eventInfo.GetSpellTypeMask() & procEntry.spellTypeMask))
            return false;
    }

    // check spell phase mask
    if (eventInfo.GetTypeMask() & REQ_SPELL_PHASE_PROC_FLAG_MASK)
    {
        if (!(eventInfo.GetSpellPhaseMask() & procEntry.spellPhaseMask))
            return false;
    }

    // check hit mask (on taken hit or on done hit, but not on spell cast phase)
    if ((eventInfo.GetTypeMask() & TAKEN_HIT_PROC_FLAG_MASK) || ((eventInfo.GetTypeMask() & DONE_HIT_PROC_FLAG_MASK) && !(eventInfo.GetSpellPhaseMask() & PROC_SPELL_PHASE_CAST)))
    {
        uint32 hitMask = procEntry.hitMask;
        // get default values if hit mask not set
        if (!hitMask)
        {
            // for taken procs allow normal + critical hits by default
            if (eventInfo.GetTypeMask() & TAKEN_HIT_PROC_FLAG_MASK)
                hitMask |= PROC_HIT_NORMAL | PROC_HIT_CRITICAL;
            // for done procs allow normal + critical + absorbs by default
            else
                hitMask |= PROC_HIT_NORMAL | PROC_HIT_CRITICAL | PROC_HIT_ABSORB;
        }
        if (!(eventInfo.GetHitMask() & hitMask))
            return false;
    }

    return true;
}

SpellBonusEntry const* SpellMgr::GetSpellBonusData(uint32 spellId) const
{
    // Lookup data
    SpellBonusMap::const_iterator itr = mSpellBonusMap.find(spellId);
    if (itr != mSpellBonusMap.end())
        return &itr->second;
    // Not found, try lookup for 1 spell rank if exist
    if (uint32 rank_1 = GetFirstSpellInChain(spellId))
    {
        SpellBonusMap::const_iterator itr2 = mSpellBonusMap.find(rank_1);
        if (itr2 != mSpellBonusMap.end())
            return &itr2->second;
    }
    return NULL;
}

SpellThreatEntry const* SpellMgr::GetSpellThreatEntry(uint32 spellID) const
{
    SpellThreatMap::const_iterator itr = mSpellThreatMap.find(spellID);
    if (itr != mSpellThreatMap.end())
        return &itr->second;
    else
    {
        uint32 firstSpell = GetFirstSpellInChain(spellID);
        itr = mSpellThreatMap.find(firstSpell);
        if (itr != mSpellThreatMap.end())
            return &itr->second;
    }
    return NULL;
}

SkillLineAbilityMapBounds SpellMgr::GetSkillLineAbilityMapBounds(uint32 spell_id) const
{
    return SkillLineAbilityMapBounds(mSkillLineAbilityMap.lower_bound(spell_id), mSkillLineAbilityMap.upper_bound(spell_id));
}

PetAura const* SpellMgr::GetPetAura(uint32 spell_id, uint8 eff)
{
    SpellPetAuraMap::const_iterator itr = mSpellPetAuraMap.find((spell_id<<8) + eff);
    if (itr != mSpellPetAuraMap.end())
        return &itr->second;
    else
        return NULL;
}

SpellEnchantProcEntry const* SpellMgr::GetSpellEnchantProcEvent(uint32 enchId) const
{
    SpellEnchantProcEventMap::const_iterator itr = mSpellEnchantProcEventMap.find(enchId);
    if (itr != mSpellEnchantProcEventMap.end())
        return &itr->second;
    return NULL;
}

bool SpellMgr::IsArenaAllowedEnchancment(uint32 ench_id) const
{
    return mEnchantCustomAttr[ench_id];
}

const std::vector<SpellLinked>* SpellMgr::GetSpellLinked(int32 spell_id) const
{
    SpellLinkedMap::const_iterator itr = mSpellLinkedMap.find(spell_id);
    return itr != mSpellLinkedMap.end() ? &(itr->second) : NULL;
}

const std::vector<SpellTalentLinked>* SpellMgr::GetSpelltalentLinked(int32 spell_id) const
{
    SpellTalentLinkedMap::const_iterator itr = mSpellTalentLinkedMap.find(spell_id);
    return itr != mSpellTalentLinkedMap.end() ? &(itr->second) : NULL;
}

const uint32 SpellMgr::GetMountListId(uint32 spell_id, uint32 teamid) const
{
    uint32 faction = 0; //both
    if(teamid == TEAM_ALLIANCE)
        faction = 1;
    if(teamid == TEAM_HORDE)
        faction = 2;

    SpellMountListMap::const_iterator itr = mSpellMountListMap.find(spell_id);
    if (itr != mSpellMountListMap.end())
    {
        SpellMountList* _mountList =  itr->second;
        if(_mountList->side == 0 || _mountList->side == faction)
            return spell_id;
        if(_mountList->side != faction)
            return _mountList->spellIdS;
    }
    else
        return spell_id;

    return 0;
}

const std::vector<SpellPrcoCheck>* SpellMgr::GetSpellPrcoCheck(int32 spell_id) const
{
    SpellPrcoCheckMap::const_iterator itr = mSpellPrcoCheckMap.find(spell_id);
    return itr != mSpellPrcoCheckMap.end() ? &(itr->second) : NULL;
}

const std::vector<SpellTriggered>* SpellMgr::GetSpellTriggered(int32 spell_id) const
{
    SpellTriggeredMap::const_iterator itr = mSpellTriggeredMap.find(spell_id);
    return itr != mSpellTriggeredMap.end() ? &(itr->second) : NULL;
}

PetLevelupSpellSet const* SpellMgr::GetPetLevelupSpellList(uint32 petFamily) const
{
    PetLevelupSpellMap::const_iterator itr = mPetLevelupSpellMap.find(petFamily);
    if (itr != mPetLevelupSpellMap.end())
        return &itr->second;
    else
        return NULL;
}

PetDefaultSpellsEntry const* SpellMgr::GetPetDefaultSpellsEntry(int32 id) const
{
    PetDefaultSpellsMap::const_iterator itr = mPetDefaultSpellsMap.find(id);
    if (itr != mPetDefaultSpellsMap.end())
        return &itr->second;
    return NULL;
}

SpellAreaMapBounds SpellMgr::GetSpellAreaMapBounds(uint32 spell_id) const
{
    return mSpellAreaMap.equal_range(spell_id);
}

SpellAreaForQuestMapBounds SpellMgr::GetSpellAreaForQuestMapBounds(uint32 quest_id) const
{
    return mSpellAreaForQuestMap.equal_range(quest_id);
}

SpellAreaForQuestMapBounds SpellMgr::GetSpellAreaForQuestEndMapBounds(uint32 quest_id) const
{
    return mSpellAreaForQuestEndMap.equal_range(quest_id);
}

SpellAreaForAuraMapBounds SpellMgr::GetSpellAreaForAuraMapBounds(uint32 spell_id) const
{
    return mSpellAreaForAuraMap.equal_range(spell_id);
}

SpellAreaForAreaMapBounds SpellMgr::GetSpellAreaForAreaMapBounds(uint32 area_id) const
{
    return mSpellAreaForAreaMap.equal_range(area_id);
}

bool SpellArea::IsFitToRequirements(Player const* player, uint32 newZone, uint32 newArea) const
{
    if (gender != GENDER_NONE)                   // not in expected gender
        if (!player || gender != player->getGender())
            return false;

    if (raceMask)                                // not in expected race
        if (!player || !(raceMask & player->getRaceMask()))
            return false;

    if (areaId > 0)                                  // not in expected zone
        if (newZone != areaId && newArea != areaId)
            return false;

    if (areaId < 0 && player)
        if(player->GetMapId() != abs(areaId))
            return false;

    if (questStart)                              // not in expected required quest state
        if (!player || ((questStartStatus & (1 << player->GetQuestStatus(questStart))) == 0))
            return false;

    if (questEnd)                                // not in expected forbidden quest state
        if (!player || (questEndStatus & (1 << player->GetQuestStatus(questEnd))))
            return false;

    if (auraSpell)                               // not have expected aura
        if (!player || (auraSpell > 0 && !player->HasAura(auraSpell)) || (auraSpell < 0 && player->HasAura(-auraSpell)))
            return false;

    // Extra conditions -- leaving the possibility add extra conditions...
    switch (spellId)
    {
        case 58600: // No fly Zone - Dalaran
        {
            if (!player)
                return false;

            AreaTableEntry const* pArea = GetAreaEntryByAreaID(player->GetAreaId());
            if (!(pArea && pArea->flags & AREA_FLAG_NO_FLY_ZONE))
                return false;
            if (!player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) && !player->HasAuraType(SPELL_AURA_FLY))
                return false;
            break;
        }
        case 91604: // No fly Zone - Wintergrasp
        {
            if (!player)
                return false;

            //Battlefield* Bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId());
            if (/*!Bf || Bf->CanFlyIn() || */(!player->HasAuraType(SPELL_AURA_MOD_INCREASE_MOUNTED_FLIGHT_SPEED) && !player->HasAuraType(SPELL_AURA_FLY)))
                return false;
            break;
        }
        case 68719: // Oil Refinery - Isle of Conquest.
        case 68720: // Quarry - Isle of Conquest.
        {
            if (!player || player->GetBattlegroundTypeId() != BATTLEGROUND_IC || !player->GetBattleground())
                return false;

            uint8 nodeType = spellId == 68719 ? NODE_TYPE_REFINERY : NODE_TYPE_QUARRY;
            uint8 nodeState = player->GetTeamId() == TEAM_ALLIANCE ? NODE_STATE_CONTROLLED_A : NODE_STATE_CONTROLLED_H;

            BattlegroundIC* pIC = static_cast<BattlegroundIC*>(player->GetBattleground());
            if (pIC->GetNodeState(nodeType) == nodeState)
                return true;

            return false;
        }
        case 56618: // Horde Controls Factory Phase Shift
        case 56617: // Alliance Controls Factory Phase Shift
        {
            if (!player)
                return false;

            Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId());

            if (!bf || bf->GetTypeId() != BATTLEFIELD_WG)
                return false;

            // team that controls the workshop in the specified area
            uint32 team = bf->GetData(newArea);

            if (team == TEAM_HORDE)
                return spellId == 56618;
            else if (team == TEAM_ALLIANCE)
                return spellId == 56617;
            break;
        }
        case 57940: // Essence of Wintergrasp - Northrend
        case 58045: // Essence of Wintergrasp - Wintergrasp
        {
            if (!player)
                return false;

            if (Battlefield* battlefieldWG = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG))
                return battlefieldWG->IsEnabled() && (player->GetTeamId() == battlefieldWG->GetDefenderTeam()) && !battlefieldWG->IsWarTime();
            break;
        }
        case 74411: // Battleground - Dampening
        {
            if (!player)
                return false;

            if (Battlefield* bf = sBattlefieldMgr->GetBattlefieldToZoneId(player->GetZoneId()))
                return bf->IsWarTime();
            break;
        }


    }

    return true;
}

void SpellMgr::LoadSpellRanks()
{
    uint32 oldMSTime = getMSTime();

    // cleanup core data before reload - remove reference to ChainNode from SpellInfo
    for (SpellChainMap::iterator itr = mSpellChains.begin(); itr != mSpellChains.end(); ++itr)
    {
        mSpellInfoMap[itr->first]->ChainEntry = NULL;
    }
    mSpellChains.clear();
    //                                                     0             1      2
    QueryResult result = WorldDatabase.Query("SELECT first_spell_id, spell_id, rank from spell_ranks ORDER BY first_spell_id, rank");

    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell rank records. DB table `spell_ranks` is empty.");

        return;
    }

    uint32 count = 0;
    bool finished = false;

    do
    {
        // spellid, rank
        std::list < std::pair < int32, int32 > > rankChain;
        int32 currentSpell = -1;
        int32 lastSpell = -1;

        // fill one chain
        while (currentSpell == lastSpell && !finished)
        {
            Field* fields = result->Fetch();

            currentSpell = fields[0].GetUInt32();
            if (lastSpell == -1)
                lastSpell = currentSpell;
            uint32 spell_id = fields[1].GetUInt32();
            uint32 rank = fields[2].GetUInt8();

            // don't drop the row if we're moving to the next rank
            if (currentSpell == lastSpell)
            {
                rankChain.push_back(std::make_pair(spell_id, rank));
                if (!result->NextRow())
                    finished = true;
            }
            else
                break;
        }
        // check if chain is made with valid first spell
        SpellInfo const* first = GetSpellInfo(lastSpell);
        if (!first)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell rank identifier(first_spell_id) %u listed in `spell_ranks` does not exist!", lastSpell);
            continue;
        }
        // check if chain is long enough
        if (rankChain.size() < 2)
        {
            sLog->outError(LOG_FILTER_SQL, "There is only 1 spell rank for identifier(first_spell_id) %u in `spell_ranks`, entry is not needed!", lastSpell);
            continue;
        }
        int32 curRank = 0;
        bool valid = true;
        // check spells in chain
        for (std::list<std::pair<int32, int32> >::iterator itr = rankChain.begin(); itr!= rankChain.end(); ++itr)
        {
            SpellInfo const* spell = GetSpellInfo(itr->first);
            if (!spell)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u (rank %u) listed in `spell_ranks` for chain %u does not exist!", itr->first, itr->second, lastSpell);
                valid = false;
                break;
            }
            ++curRank;
            if (itr->second != curRank)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u (rank %u) listed in `spell_ranks` for chain %u does not have proper rank value(should be %u)!", itr->first, itr->second, lastSpell, curRank);
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;
        int32 prevRank = 0;
        // insert the chain
        std::list<std::pair<int32, int32> >::iterator itr = rankChain.begin();
        do
        {
            ++count;
            int32 addedSpell = itr->first;
            mSpellChains[addedSpell].first = GetSpellInfo(lastSpell);
            mSpellChains[addedSpell].last = GetSpellInfo(rankChain.back().first);
            mSpellChains[addedSpell].rank = itr->second;
            mSpellChains[addedSpell].prev = GetSpellInfo(prevRank);
            mSpellInfoMap[addedSpell]->ChainEntry = &mSpellChains[addedSpell];
            prevRank = addedSpell;
            ++itr;
            if (itr == rankChain.end())
            {
                mSpellChains[addedSpell].next = NULL;
                break;
            }
            else
                mSpellChains[addedSpell].next = GetSpellInfo(itr->first);
        }
        while (true);
    } while (!finished);

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell rank records in %u ms", count, GetMSTimeDiffToNow(oldMSTime));

}

void SpellMgr::LoadSpellRequired()
{
    uint32 oldMSTime = getMSTime();

    mSpellsReqSpell.clear();                                   // need for reload case
    mSpellReq.clear();                                         // need for reload case

    //                                                   0        1
    QueryResult result = WorldDatabase.Query("SELECT spell_id, req_spell from spell_required");

    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell required records. DB table `spell_required` is empty.");

        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();
        uint32 spell_req = fields[1].GetUInt32();

        // check if chain is made with valid first spell
        SpellInfo const* spell = GetSpellInfo(spell_id);
        if (!spell)
        {
            sLog->outError(LOG_FILTER_SQL, "spell_id %u in `spell_required` table is not found in dbcs, skipped", spell_id);
            continue;
        }

        SpellInfo const* req_spell = GetSpellInfo(spell_req);
        if (!req_spell)
        {
            sLog->outError(LOG_FILTER_SQL, "req_spell %u in `spell_required` table is not found in dbcs, skipped", spell_req);
            continue;
        }

        if (GetFirstSpellInChain(spell_id) == GetFirstSpellInChain(spell_req))
        {
            sLog->outError(LOG_FILTER_SQL, "req_spell %u and spell_id %u in `spell_required` table are ranks of the same spell, entry not needed, skipped", spell_req, spell_id);
            continue;
        }

        if (IsSpellRequiringSpell(spell_id, spell_req))
        {
            sLog->outError(LOG_FILTER_SQL, "duplicated entry of req_spell %u and spell_id %u in `spell_required`, skipped", spell_req, spell_id);
            continue;
        }

        mSpellReq.insert (std::pair<uint32, uint32>(spell_id, spell_req));
        mSpellsReqSpell.insert (std::pair<uint32, uint32>(spell_req, spell_id));
        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell required records in %u ms", count, GetMSTimeDiffToNow(oldMSTime));

}

void SpellMgr::LoadSpellLearnSkills()
{
    uint32 oldMSTime = getMSTime();

    mSpellLearnSkills.clear();                              // need for reload case

    // search auto-learned skills and add its to map also for use in unlearn spells/talents
    uint32 dbc_count = 0;
    for (uint32 spell = 0; spell < GetSpellInfoStoreSize(); ++spell)
    {
        SpellInfo const* entry = GetSpellInfo(spell);

        if (!entry)
            continue;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (entry->Effects[i].Effect == SPELL_EFFECT_SKILL)
            {
                SpellLearnSkillNode dbc_node;
                dbc_node.skill = entry->Effects[i].MiscValue;
                dbc_node.step  = entry->Effects[i].CalcValue();
                if (dbc_node.skill != SKILL_RIDING)
                    dbc_node.value = 1;
                else
                    dbc_node.value = dbc_node.step * 75;
                dbc_node.maxvalue = dbc_node.step * 75;
                mSpellLearnSkills[spell] = dbc_node;
                ++dbc_count;
                break;
            }
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u Spell Learn Skills from DBC in %u ms", dbc_count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellLearnSpells()
{
    uint32 oldMSTime = getMSTime();

    mSpellLearnSpells.clear();                              // need for reload case

    //                                                  0      1        2
    QueryResult result = WorldDatabase.Query("SELECT entry, SpellID, Active FROM spell_learn_spell");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell learn spells. DB table `spell_learn_spell` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();

        SpellLearnSpellNode node;
        node.spell       = fields[1].GetUInt32();
        node.active      = fields[2].GetBool();
        node.autoLearned = false;

        if (!GetSpellInfo(spell_id))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_learn_spell` does not exist", spell_id);
            continue;
        }

        if (!GetSpellInfo(node.spell))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_learn_spell` learning not existed spell %u", spell_id, node.spell);
            continue;
        }

        /*if (GetTalentSpellCost(node.spell))
        {
        sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_learn_spell` attempt learning talent spell %u, skipped", spell_id, node.spell);
        continue;
        }*/

        mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell_id, node));

        ++count;
    } while (result->NextRow());

    // search auto-learned spells and add its to map also for use in unlearn spells/talents
    uint32 dbc_count = 0;
    for (uint32 spell = 0; spell < GetSpellInfoStoreSize(); ++spell)
    {
        SpellInfo const* entry = GetSpellInfo(spell);

        if (!entry)
            continue;

        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (entry->Effects[i].Effect == SPELL_EFFECT_LEARN_SPELL)
            {
                SpellLearnSpellNode dbc_node;
                dbc_node.spell = entry->Effects[i].TriggerSpell;
                dbc_node.active = true;                     // all dbc based learned spells is active (show in spell book or hide by client itself)

                // ignore learning not existed spells (broken/outdated/or generic learnig spell 483
                if (!GetSpellInfo(dbc_node.spell))
                    continue;

                // talent or passive spells or skill-step spells auto-casted and not need dependent learning,
                // pet teaching spells must not be dependent learning (casted)
                // other required explicit dependent learning
                dbc_node.autoLearned = entry->Effects[i].TargetA.GetTarget() == TARGET_UNIT_PET || /*GetTalentSpellCost(spell) > 0 ||*/ entry->IsPassive() || entry->HasEffect(SPELL_EFFECT_SKILL_STEP);

                SpellLearnSpellMapBounds db_node_bounds = GetSpellLearnSpellMapBounds(spell);

                bool found = false;
                for (SpellLearnSpellMap::const_iterator itr = db_node_bounds.first; itr != db_node_bounds.second; ++itr)
                {
                    if (itr->second.spell == dbc_node.spell)
                    {
                        sLog->outError(LOG_FILTER_SQL, "Spell %u auto-learn spell %u in spell.dbc then the record in `spell_learn_spell` is redundant, please fix DB.",
                            spell, dbc_node.spell);
                        found = true;
                        break;
                    }
                }

                if (!found)                                  // add new spell-spell pair if not found
                {
                    mSpellLearnSpells.insert(SpellLearnSpellMap::value_type(spell, dbc_node));
                    ++dbc_count;
                }
            }
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell learn spells + %u found in DBC in %u ms", count, dbc_count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellTargetPositions()
{
    uint32 oldMSTime = getMSTime();

    mSpellTargetPositions.clear();                                // need for reload case

    //                                                0      1              2                  3                  4                  5
    QueryResult result = WorldDatabase.Query("SELECT id, target_map, target_position_x, target_position_y, target_position_z, target_orientation FROM spell_target_position");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell target coordinates. DB table `spell_target_position` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 Spell_ID = fields[0].GetUInt32();

        SpellTargetPosition st;

        st.target_mapId       = fields[1].GetUInt16();
        st.target_X           = fields[2].GetFloat();
        st.target_Y           = fields[3].GetFloat();
        st.target_Z           = fields[4].GetFloat();
        st.target_Orientation = fields[5].GetFloat();

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell (ID:%u) target map (ID: %u) does not exist in `Map.dbc`.", Spell_ID, st.target_mapId);
            continue;
        }

        if (st.target_X==0 && st.target_Y==0 && st.target_Z==0)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell (ID:%u) target coordinates not provided.", Spell_ID);
            continue;
        }

        SpellInfo const* spellInfo = GetSpellInfo(Spell_ID);
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell (ID:%u) listed in `spell_target_position` does not exist.", Spell_ID);
            continue;
        }

        bool found = false;
        for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
        {
            if (spellInfo->Effects[i].TargetA.GetTarget() == TARGET_DEST_DB || spellInfo->Effects[i].TargetB.GetTarget() == TARGET_DEST_DB)
            {
                // additional requirements
                if (spellInfo->Effects[i].Effect == SPELL_EFFECT_BIND && spellInfo->Effects[i].MiscValue)
                {
                    uint32 area_id = sMapMgr->GetAreaId(st.target_mapId, st.target_X, st.target_Y, st.target_Z);
                    if (area_id != uint32(spellInfo->Effects[i].MiscValue))
                    {
                        sLog->outError(LOG_FILTER_SQL, "Spell (Id: %u) listed in `spell_target_position` expected point to zone %u bit point to zone %u.", Spell_ID, spellInfo->Effects[i].MiscValue, area_id);
                        break;
                    }
                }

                found = true;
                break;
            }
        }
        if (!found)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell (Id: %u) listed in `spell_target_position` does not have target TARGET_DEST_DB (17).", Spell_ID);
            continue;
        }

        mSpellTargetPositions[Spell_ID] = st;
        ++count;

    } while (result->NextRow());

    /*
    // Check all spells
    for (uint32 i = 1; i < GetSpellInfoStoreSize; ++i)
    {
    SpellInfo const* spellInfo = GetSpellInfo(i);
    if (!spellInfo)
    continue;

    bool found = false;
    for (int j = 0; j < MAX_SPELL_EFFECTS; ++j)
    {
    switch (spellInfo->Effects[j].TargetA)
    {
    case TARGET_DEST_DB:
    found = true;
    break;
    }
    if (found)
    break;
    switch (spellInfo->Effects[j].TargetB)
    {
    case TARGET_DEST_DB:
    found = true;
    break;
    }
    if (found)
    break;
    }
    if (found)
    {
    if (!sSpellMgr->GetSpellTargetPosition(i))
    sLog->outDebug(LOG_FILTER_SPELLS_AURAS, "Spell (ID: %u) does not have record in `spell_target_position`", i);
    }
    }*/

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell teleport coordinates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellGroups()
{
    uint32 oldMSTime = getMSTime();

    mSpellSpellGroup.clear();                                  // need for reload case
    mSpellGroupSpell.clear();

    //                                                0     1
    QueryResult result = WorldDatabase.Query("SELECT id, spell_id FROM spell_group");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell group definitions. DB table `spell_group` is empty.");
        return;
    }

    std::set<uint32> groups;
    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 group_id = fields[0].GetUInt32();
        if (group_id <= SPELL_GROUP_DB_RANGE_MIN && group_id >= SPELL_GROUP_CORE_RANGE_MAX)
        {
            sLog->outError(LOG_FILTER_SQL, "SpellGroup id %u listed in `spell_group` is in core range, but is not defined in core!", group_id);
            continue;
        }
        int32 spell_id = fields[1].GetInt32();

        groups.insert(std::set<uint32>::value_type(group_id));
        mSpellGroupSpell.insert(SpellGroupSpellMap::value_type((SpellGroup)group_id, spell_id));

    } while (result->NextRow());

    for (SpellGroupSpellMap::iterator itr = mSpellGroupSpell.begin(); itr!= mSpellGroupSpell.end();)
    {
        if (itr->second < 0)
        {
            if (groups.find(abs(itr->second)) == groups.end())
            {
                sLog->outError(LOG_FILTER_SQL, "SpellGroup id %u listed in `spell_group` does not exist", abs(itr->second));
                mSpellGroupSpell.erase(itr++);
            }
            else
                ++itr;
        }
        else
        {
            SpellInfo const* spellInfo = GetSpellInfo(itr->second);

            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_group` does not exist", itr->second);
                mSpellGroupSpell.erase(itr++);
            }
            else if (spellInfo->GetRank() > 1)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_group` is not first rank of spell", itr->second);
                mSpellGroupSpell.erase(itr++);
            }
            else
                ++itr;
        }
    }

    for (std::set<uint32>::iterator groupItr = groups.begin(); groupItr != groups.end(); ++groupItr)
    {
        std::set<uint32> spells;
        GetSetOfSpellsInSpellGroup(SpellGroup(*groupItr), spells);

        for (std::set<uint32>::iterator spellItr = spells.begin(); spellItr != spells.end(); ++spellItr)
        {
            ++count;
            mSpellSpellGroup.insert(SpellSpellGroupMap::value_type(*spellItr, SpellGroup(*groupItr)));
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell group definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellGroupStackRules()
{
    uint32 oldMSTime = getMSTime();

    mSpellGroupStack.clear();                                  // need for reload case

    //                                                       0         1
    QueryResult result = WorldDatabase.Query("SELECT group_id, stack_rule FROM spell_group_stack_rules");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell group stack rules. DB table `spell_group_stack_rules` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 group_id = fields[0].GetUInt32();
        uint8 stack_rule = fields[1].GetInt8();
        if (stack_rule >= SPELL_GROUP_STACK_RULE_MAX)
        {
            sLog->outError(LOG_FILTER_SQL, "SpellGroupStackRule %u listed in `spell_group_stack_rules` does not exist", stack_rule);
            continue;
        }

        SpellGroupSpellMapBounds spellGroup = GetSpellGroupSpellMapBounds((SpellGroup)group_id);

        if (spellGroup.first == spellGroup.second)
        {
            sLog->outError(LOG_FILTER_SQL, "SpellGroup id %u listed in `spell_group_stack_rules` does not exist", group_id);
            continue;
        }

        mSpellGroupStack[(SpellGroup)group_id] = (SpellGroupStackRule)stack_rule;

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell group stack rules in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadForbiddenSpells()
{
    uint32 oldMSTime = getMSTime();

    mForbiddenSpells.clear();

    uint32 count = 0;

    QueryResult result = WorldDatabase.Query("SELECT spell_id FROM spell_forbidden");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell group definitions", count);
        return;
    }

    do
    {
        Field *fields = result->Fetch();
    
        mForbiddenSpells.push_back(fields[0].GetUInt32());

        ++count;
    } while (result->NextRow());


    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u forbidden spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}


void SpellMgr::LoadSpellProcEvents()
{
    uint32 oldMSTime = getMSTime();

    mSpellProcEventMap.clear();                             // need for reload case

    //                                               0      1           2                3                 4                 5                 6                 7          8       9        10            11        12
    QueryResult result = WorldDatabase.Query("SELECT entry, SchoolMask, SpellFamilyName, SpellFamilyMask0, SpellFamilyMask1, SpellFamilyMask2, SpellFamilyMask3, procFlags, procEx, ppmRate, CustomChance, Cooldown, effectmask FROM spell_proc_event");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell proc event conditions. DB table `spell_proc_event` is empty.");
        return;
    }

    uint32 count = 0;
    uint32 customProc = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        SpellInfo const* spell = GetSpellInfo(entry);
        if (!spell)
        {
            WorldDatabase.PExecute("DELETE FROM spell_proc_event WHERE entry = %u;", entry);
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc_event` does not exist", entry);
            continue;
        }

        SpellProcEventEntry spe;

        spe.schoolMask      = fields[1].GetInt8();
        spe.spellFamilyName = fields[2].GetUInt16();
        spe.spellFamilyMask[0] = fields[3].GetUInt32();
        spe.spellFamilyMask[1] = fields[4].GetUInt32();
        spe.spellFamilyMask[2] = fields[5].GetUInt32();
        spe.spellFamilyMask[3] = fields[6].GetUInt32();
        spe.procFlags       = fields[7].GetUInt32();
        spe.procEx          = fields[8].GetUInt32();
        spe.ppmRate         = fields[9].GetFloat();
        spe.customChance    = fields[10].GetFloat();
        spe.cooldown        = fields[11].GetUInt32();
        spe.effectMask        = fields[12].GetUInt32();

        mSpellProcEventMap[entry].push_back(spe);

        if (spell->ProcFlags == 0)
        {
            if (spe.procFlags == 0)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc_event` probally not triggered spell", entry);
                continue;
            }
            customProc++;
        }
        ++count;
    } while (result->NextRow());

    if (customProc)
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u extra and %u custom spell proc event conditions in %u ms",  count, customProc, GetMSTimeDiffToNow(oldMSTime));
    else
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u extra spell proc event conditions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));

}

void SpellMgr::LoadSpellProcs()
{
    uint32 oldMSTime = getMSTime();

    mSpellProcMap.clear();                             // need for reload case

    //                                               0        1           2                3                 4                 5                 6                 7         8              9               10       11              12             13      14        15       16
    QueryResult result = WorldDatabase.Query("SELECT spellId, schoolMask, spellFamilyName, spellFamilyMask0, spellFamilyMask1, spellFamilyMask2, spellFamilyMask3, typeMask, spellTypeMask, spellPhaseMask, hitMask, attributesMask, ratePerMinute, chance, cooldown, charges, modcharges FROM spell_proc");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell proc conditions and data. DB table `spell_proc` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 spellId = fields[0].GetInt32();

        bool allRanks = false;
        if (spellId <=0)
        {
            allRanks = true;
            spellId = -spellId;
        }

        SpellInfo const* spellEntry = GetSpellInfo(spellId);
        if (!spellEntry)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc` does not exist", spellId);
            continue;
        }

        if (allRanks)
        {
            if (GetFirstSpellInChain(spellId) != uint32(spellId))
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc` is not first rank of spell.", fields[0].GetInt32());
                continue;
            }
        }

        SpellProcEntry baseProcEntry;

        baseProcEntry.schoolMask      = fields[1].GetInt8();
        baseProcEntry.spellFamilyName = fields[2].GetUInt16();
        baseProcEntry.spellFamilyMask[0] = fields[3].GetUInt32();
        baseProcEntry.spellFamilyMask[1] = fields[4].GetUInt32();
        baseProcEntry.spellFamilyMask[2] = fields[5].GetUInt32();
        baseProcEntry.spellFamilyMask[3] = fields[6].GetUInt32();
        baseProcEntry.typeMask        = fields[7].GetUInt32();
        baseProcEntry.spellTypeMask   = fields[8].GetUInt32();
        baseProcEntry.spellPhaseMask  = fields[9].GetUInt32();
        baseProcEntry.hitMask         = fields[10].GetUInt32();
        baseProcEntry.attributesMask  = fields[11].GetUInt32();
        baseProcEntry.ratePerMinute   = fields[12].GetFloat();
        baseProcEntry.chance          = fields[13].GetFloat();
        float cooldown                = fields[14].GetFloat();
        baseProcEntry.cooldown        = uint32(cooldown);
        baseProcEntry.charges         = fields[15].GetUInt32();
        baseProcEntry.modcharges      = fields[16].GetUInt32();

        while (true)
        {
            if (mSpellProcMap.find(spellId) != mSpellProcMap.end())
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc` has duplicate entry in the table", spellId);
                break;
            }
            SpellProcEntry procEntry = SpellProcEntry(baseProcEntry);

            // take defaults from dbcs
            if (!procEntry.typeMask)
                procEntry.typeMask = spellEntry->ProcFlags;
            if (!procEntry.charges)
                procEntry.charges = spellEntry->ProcCharges;
            if (!procEntry.chance && !procEntry.ratePerMinute)
                procEntry.chance = float(spellEntry->ProcChance);

            // validate data
            if (procEntry.schoolMask & ~SPELL_SCHOOL_MASK_ALL)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has wrong `schoolMask` set: %u", spellId, procEntry.schoolMask);
            if (procEntry.spellFamilyName && (procEntry.spellFamilyName < 3 || procEntry.spellFamilyName > 17 || procEntry.spellFamilyName == 14 || procEntry.spellFamilyName == 16))
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has wrong `spellFamilyName` set: %u", spellId, procEntry.spellFamilyName);
            if (procEntry.chance < 0)
            {
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has negative value in `chance` field", spellId);
                procEntry.chance = 0;
            }
            if (procEntry.ratePerMinute < 0)
            {
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has negative value in `ratePerMinute` field", spellId);
                procEntry.ratePerMinute = 0;
            }
            if (cooldown < 0)
            {
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has negative value in `cooldown` field", spellId);
                procEntry.cooldown = 0;
            }
            if (procEntry.chance == 0 && procEntry.ratePerMinute == 0)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u doesn't have `chance` and `ratePerMinute` values defined, proc will not be triggered", spellId);
            if (procEntry.charges > 99)
            {
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has too big value in `charges` field", spellId);
                procEntry.charges = 99;
            }
            if (!procEntry.typeMask)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u doesn't have `typeMask` value defined, proc will not be triggered", spellId);
            if (procEntry.spellTypeMask & ~PROC_SPELL_PHASE_MASK_ALL)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has wrong `spellTypeMask` set: %u", spellId, procEntry.spellTypeMask);
            if (procEntry.spellTypeMask && !(procEntry.typeMask & (SPELL_PROC_FLAG_MASK | PERIODIC_PROC_FLAG_MASK)))
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has `spellTypeMask` value defined, but it won't be used for defined `typeMask` value", spellId);
            if (!procEntry.spellPhaseMask && procEntry.typeMask & REQ_SPELL_PHASE_PROC_FLAG_MASK)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u doesn't have `spellPhaseMask` value defined, but it's required for defined `typeMask` value, proc will not be triggered", spellId);
            if (procEntry.spellPhaseMask & ~PROC_SPELL_PHASE_MASK_ALL)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has wrong `spellPhaseMask` set: %u", spellId, procEntry.spellPhaseMask);
            if (procEntry.spellPhaseMask && !(procEntry.typeMask & REQ_SPELL_PHASE_PROC_FLAG_MASK))
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has `spellPhaseMask` value defined, but it won't be used for defined `typeMask` value", spellId);
            if (procEntry.hitMask & ~PROC_HIT_MASK_ALL)
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has wrong `hitMask` set: %u", spellId, procEntry.hitMask);
            if (procEntry.hitMask && !(procEntry.typeMask & TAKEN_HIT_PROC_FLAG_MASK || (procEntry.typeMask & DONE_HIT_PROC_FLAG_MASK && (!procEntry.spellPhaseMask || procEntry.spellPhaseMask & (PROC_SPELL_PHASE_HIT | PROC_SPELL_PHASE_FINISH)))))
                sLog->outError(LOG_FILTER_SQL, "`spell_proc` table entry for spellId %u has `hitMask` value defined, but it won't be used for defined `typeMask` and `spellPhaseMask` values", spellId);

            mSpellProcMap[spellId] = procEntry;

            if (allRanks)
            {
                spellId = GetNextSpellInChain(spellId);
                spellEntry = GetSpellInfo(spellId);
            }
            else
                break;
        }
        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell proc conditions and data in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellBonusess()
{
    uint32 oldMSTime = getMSTime();

    mSpellBonusMap.clear();                             // need for reload case

    //                                                0      1             2          3         4                   5           6
    QueryResult result = WorldDatabase.Query("SELECT entry, direct_bonus, dot_bonus, ap_bonus, ap_dot_bonus, damage_bonus, heal_bonus FROM spell_bonus_data");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell bonus data. DB table `spell_bonus_data` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 entry = fields[0].GetUInt32();

        SpellInfo const* spell = GetSpellInfo(entry);
        if (!spell)
        {
            WorldDatabase.PExecute("DELETE FROM spell_bonus_data WHERE entry = %u;", entry);
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_bonus_data` does not exist", entry);
            continue;
        }

        SpellBonusEntry& sbe = mSpellBonusMap[entry];
        sbe.direct_damage = fields[1].GetFloat();
        sbe.dot_damage    = fields[2].GetFloat();
        sbe.ap_bonus      = fields[3].GetFloat();
        sbe.ap_dot_bonus   = fields[4].GetFloat();
        sbe.damage_bonus   = fields[5].GetFloat();
        sbe.heal_bonus   = fields[6].GetFloat();

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u extra spell bonus data in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellThreats()
{
    uint32 oldMSTime = getMSTime();

    mSpellThreatMap.clear();                                // need for reload case

    //                                                0      1        2       3
    QueryResult result = WorldDatabase.Query("SELECT entry, flatMod, pctMod, apPctMod FROM spell_threat");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 aggro generating spells. DB table `spell_threat` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();

        if (!GetSpellInfo(entry))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_threat` does not exist", entry);
            continue;
        }

        SpellThreatEntry ste;
        ste.flatMod  = fields[1].GetInt32();
        ste.pctMod   = fields[2].GetFloat();
        ste.apPctMod = fields[3].GetFloat();

        mSpellThreatMap[entry] = ste;
        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u SpellThreatEntries in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSkillLineAbilityMap()
{
    uint32 oldMSTime = getMSTime();

    mSkillLineAbilityMap.clear();

    uint32 count = 0;

    for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
    {
        SkillLineAbilityEntry const* SkillInfo = sSkillLineAbilityStore.LookupEntry(i);
        if (!SkillInfo)
            continue;

        mSkillLineAbilityMap.insert(SkillLineAbilityMap::value_type(SkillInfo->spellId, SkillInfo));
        ++count;
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u SkillLineAbility MultiMap Data in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellPetAuras()
{
    uint32 oldMSTime = getMSTime();

    mSpellPetAuraMap.clear();                                  // need for reload case

    //                                                  0       1       2    3
    QueryResult result = WorldDatabase.Query("SELECT spell, effectId, pet, aura FROM spell_pet_auras");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell pet auras. DB table `spell_pet_auras` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell = fields[0].GetUInt32();
        uint8 eff = fields[1].GetUInt8();
        uint32 pet = fields[2].GetUInt32();
        uint32 aura = fields[3].GetUInt32();

        SpellPetAuraMap::iterator itr = mSpellPetAuraMap.find((spell<<8) + eff);
        if (itr != mSpellPetAuraMap.end())
            itr->second.AddAura(pet, aura);
        else
        {
            SpellInfo const* spellInfo = GetSpellInfo(spell);
            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_pet_auras` does not exist", spell);
                continue;
            }
            if (spellInfo->Effects[eff].Effect != SPELL_EFFECT_DUMMY &&
                (spellInfo->Effects[eff].Effect != SPELL_EFFECT_APPLY_AURA ||
                spellInfo->Effects[eff].ApplyAuraName != SPELL_AURA_DUMMY))
            {
                sLog->outError(LOG_FILTER_SPELLS_AURAS, "Spell %u listed in `spell_pet_auras` does not have dummy aura or dummy effect", spell);
                continue;
            }

            SpellInfo const* spellInfo2 = GetSpellInfo(aura);
            if (!spellInfo2)
            {
                sLog->outError(LOG_FILTER_SQL, "Aura %u listed in `spell_pet_auras` does not exist", aura);
                continue;
            }

            PetAura pa(pet, aura, spellInfo->Effects[eff].TargetA.GetTarget() == TARGET_UNIT_PET, spellInfo->Effects[eff].CalcValue());
            mSpellPetAuraMap[(spell<<8) + eff] = pa;
        }

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell pet auras in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

// Fill custom data about enchancments
void SpellMgr::LoadEnchantCustomAttr()
{
    uint32 oldMSTime = getMSTime();

    uint32 size = sSpellItemEnchantmentStore.GetNumRows();
    mEnchantCustomAttr.resize(size);

    for (uint32 i = 0; i < size; ++i)
        mEnchantCustomAttr[i] = 0;

    uint32 count = 0;
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        SpellInfo const* spellInfo = GetSpellInfo(i);
        if (!spellInfo)
            continue;

        // TODO: find a better check
        if (!(spellInfo->AttributesEx2 & SPELL_ATTR2_PRESERVE_ENCHANT_IN_ARENA) || !(spellInfo->Attributes & SPELL_ATTR0_NOT_SHAPESHIFT))
            continue;

        for (uint32 j = 0; j < MAX_SPELL_EFFECTS; ++j)
        {
            if (spellInfo->Effects[j].Effect == SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY)
            {
                uint32 enchId = spellInfo->Effects[j].MiscValue;
                SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchId);
                if (!ench)
                    continue;
                mEnchantCustomAttr[enchId] = true;
                ++count;
                break;
            }
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u custom enchant attributes in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellEnchantProcData()
{
    uint32 oldMSTime = getMSTime();

    mSpellEnchantProcEventMap.clear();                             // need for reload case

    //                                                  0         1           2         3
    QueryResult result = WorldDatabase.Query("SELECT entry, customChance, PPMChance, procEx FROM spell_enchant_proc_data");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell enchant proc event conditions. DB table `spell_enchant_proc_data` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 enchantId = fields[0].GetUInt32();

        SpellItemEnchantmentEntry const* ench = sSpellItemEnchantmentStore.LookupEntry(enchantId);
        if (!ench)
        {
            sLog->outError(LOG_FILTER_SQL, "Enchancment %u listed in `spell_enchant_proc_data` does not exist", enchantId);
            continue;
        }

        SpellEnchantProcEntry spe;

        spe.customChance = fields[1].GetUInt32();
        spe.PPMChance = fields[2].GetFloat();
        spe.procEx = fields[3].GetUInt32();

        mSpellEnchantProcEventMap[enchantId] = spe;

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u enchant proc data definitions in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellLinked()
{
    uint32 oldMSTime = getMSTime();

    mSpellLinkedMap.clear();    // need for reload case

    //                                                0              1             2        3        4          5        7         8
    QueryResult result = WorldDatabase.Query("SELECT spell_trigger, spell_effect, type, hastalent, hastalent2, chance, cooldown, type2 FROM spell_linked_spell");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 linked spells. DB table `spell_linked_spell` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 trigger = fields[0].GetInt32();
        int32 effect = fields[1].GetInt32();
        int32 type = fields[2].GetUInt8();
        int32 hastalent = fields[3].GetInt32();
        int32 hastalent2 = fields[4].GetInt32();
        int32 chance = fields[5].GetInt32();
        int32 cooldown = fields[6].GetUInt8();
        int32 type2 = fields[7].GetUInt8();

        SpellInfo const* spellInfo = GetSpellInfo(abs(trigger));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_linked_spell` does not exist", abs(trigger));
            continue;
        }
        spellInfo = GetSpellInfo(abs(effect));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_linked_spell` does not exist", abs(effect));
            continue;
        }

        if (type) //we will find a better way when more types are needed
        {
            if (trigger > 0)
                trigger += SPELL_LINKED_MAX_SPELLS * type;
            else
                trigger -= SPELL_LINKED_MAX_SPELLS * type;
        }
        SpellLinked templink;
        templink.effect = effect;
        templink.hastalent = hastalent;
        templink.hastalent2 = hastalent2;
        templink.chance = chance;
        templink.cooldown = cooldown;
        templink.type2 = type2;
        mSpellLinkedMap[trigger].push_back(templink);

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u linked spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadTalentSpellLinked()
{
    uint32 oldMSTime = getMSTime();

    mSpellTalentLinkedMap.clear();    // need for reload case

    //                                                  0        1        2
    QueryResult result = WorldDatabase.Query("SELECT spellid, spelllink, type FROM spell_talent_linked_spell");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 linked talent spells. DB table `spell_talent_linked_spell` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 talent = fields[0].GetInt32();
        int32 triger = fields[1].GetInt32();
        int32 type   = fields[2].GetUInt8();

        SpellInfo const* spellInfo = GetSpellInfo(abs(talent));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_talent_linked_spell` does not exist", abs(talent));
            continue;
        }
        spellInfo = GetSpellInfo(abs(triger));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_talent_linked_spell` does not exist", abs(triger));
            continue;
        }

        if (type) //we will find a better way when more types are needed
        {
            if (talent > 0)
                talent += SPELL_LINKED_MAX_SPELLS * type;
            else
                talent -= SPELL_LINKED_MAX_SPELLS * type;
        }
        SpellTalentLinked templink;
        templink.talent = talent;
        templink.triger = triger;
        templink.type   = type;
        mSpellTalentLinkedMap[talent].push_back(templink);

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u linked talent spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadmSpellMountList()
{
    uint32 oldMSTime = getMSTime();

    mSpellMountListMap.clear();    // need for reload case

    //                                                0      1      2       3
    QueryResult result = WorldDatabase.Query("SELECT spell, side, spellS, sideS FROM mount_list");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 linked spells. DB table `mount_list` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 spell = fields[0].GetInt32();
        int32 side = fields[1].GetInt32();
        int32 spellS = fields[2].GetUInt32();
        int32 sideS = fields[3].GetInt32();

        SpellInfo const* spellInfo = GetSpellInfo(spell);
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `mount_list` does not exist", spell);
            continue;
        }
        spellInfo = GetSpellInfo(spellS);
        if (spellS != 0 && !spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `mount_list` does not exist", spellS);
            continue;
        }

        SpellMountList* templist = new SpellMountList;
        templist->spellId = spell;
        templist->side = side;
        templist->spellIdS = spellS;
        templist->sideS = sideS;
        mSpellMountListMap[spell] = templist;

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u mount list in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellPrcoCheck()
{
    uint32 oldMSTime = getMSTime();

    mSpellPrcoCheckMap.clear();    // need for reload case

    //                                                0        1       2      3             4         5      6          7
    QueryResult result = WorldDatabase.Query("SELECT entry, entry2, entry3, checkspell, hastalent, chance, target, effectmask FROM spell_proc_check");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 proc check spells. DB table `spell_proc_check` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 entry = fields[0].GetInt32();
        int32 entry2 = fields[1].GetInt32();
        int32 entry3 = fields[2].GetInt32();
        int32 checkspell = fields[3].GetInt32();
        int32 hastalent = fields[4].GetInt32();
        int32 chance = fields[5].GetInt32();
        int32 target = fields[6].GetInt32();
        int32 effectmask = fields[7].GetInt32();

        SpellInfo const* spellInfo = GetSpellInfo(abs(entry));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_proc_check` does not exist", abs(entry));
            continue;
        }

        SpellPrcoCheck templink;
        templink.checkspell = checkspell;
        templink.hastalent = hastalent;
        templink.chance = chance;
        templink.target = target;
        templink.effectmask = effectmask;
        mSpellPrcoCheckMap[entry].push_back(templink);
        if(entry2)
            mSpellPrcoCheckMap[entry2].push_back(templink);
        if(entry3)
            mSpellPrcoCheckMap[entry3].push_back(templink);

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u proc check spells in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellTriggered()
{
    uint32 oldMSTime = getMSTime();

    mSpellTriggeredMap.clear();    // need for reload case

    //                                                    0           1             2           3       4      5      6         7           8
    QueryResult result = WorldDatabase.Query("SELECT `spell_id`, `spell_trigger`, `option`, `target`, `bp0`, `bp1`, `bp2`, `effectmask`, `aura` FROM `spell_trigger`");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 triggered spells. DB table `spell_trigger` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        int32 spell_id = fields[0].GetInt32();
        int32 spell_trigger = fields[1].GetInt32();
        int32 option = fields[2].GetInt32();
        int32 target = fields[3].GetInt32();
        int32 bp0 = fields[4].GetInt32();
        int32 bp1 = fields[5].GetInt32();
        int32 bp2 = fields[6].GetInt32();
        int32 effectmask = fields[7].GetInt32();
        int32 aura = fields[8].GetInt32();

        SpellInfo const* spellInfo = GetSpellInfo(abs(spell_id));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_id` does not exist", abs(spell_id));
            continue;
        }
        spellInfo = GetSpellInfo(abs(spell_trigger));
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_trigger` does not exist", abs(spell_trigger));
            continue;
        }

        SpellTriggered temptrigger;
        temptrigger.spell_id = spell_id;
        temptrigger.spell_trigger = spell_trigger;
        temptrigger.option = option;
        temptrigger.target = target;
        temptrigger.bp0 = bp0;
        temptrigger.bp1 = bp1;
        temptrigger.bp2 = bp2;
        temptrigger.effectmask = effectmask;
        temptrigger.aura = aura;
        mSpellTriggeredMap[spell_id].push_back(temptrigger);

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u triggered spell in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadPetLevelupSpellMap()
{
    uint32 oldMSTime = getMSTime();

    mPetLevelupSpellMap.clear();                                   // need for reload case

    uint32 count = 0;
    uint32 family_count = 0;

    for (uint32 i = 0; i < sCreatureFamilyStore.GetNumRows(); ++i)
    {
        CreatureFamilyEntry const* creatureFamily = sCreatureFamilyStore.LookupEntry(i);
        if (!creatureFamily)                                     // not exist
            continue;

        for (uint8 j = 0; j < 2; ++j)
        {
            if (!creatureFamily->skillLine[j])
                continue;

            for (uint32 k = 0; k < sSkillLineAbilityStore.GetNumRows(); ++k)
            {
                SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(k);
                if (!skillLine)
                    continue;

                //if (skillLine->skillId != creatureFamily->skillLine[0] &&
                //    (!creatureFamily->skillLine[1] || skillLine->skillId != creatureFamily->skillLine[1]))
                //    continue;

                if (skillLine->skillId != creatureFamily->skillLine[j])
                    continue;

                if (skillLine->learnOnGetSkill != ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL)
                    continue;

                SpellInfo const* spell = GetSpellInfo(skillLine->spellId);
                if (!spell) // not exist or triggered or talent
                    continue;

                if (!spell->SpellLevel)
                    continue;

                PetLevelupSpellSet& spellSet = mPetLevelupSpellMap[creatureFamily->ID];
                if (spellSet.empty())
                    ++family_count;

                spellSet.insert(PetLevelupSpellSet::value_type(spell->SpellLevel, spell->Id));
                ++count;
            }
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u pet levelup and default spells for %u families in %u ms", count, family_count, GetMSTimeDiffToNow(oldMSTime));
}

bool LoadPetDefaultSpells_helper(CreatureTemplate const* cInfo, PetDefaultSpellsEntry& petDefSpells)
{
    // skip empty list;
    bool have_spell = false;
    for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
    {
        if (petDefSpells.spellid[j])
        {
            have_spell = true;
            break;
        }
    }
    if (!have_spell)
        return false;

    // remove duplicates with levelupSpells if any
    if (PetLevelupSpellSet const* levelupSpells = cInfo->family ? sSpellMgr->GetPetLevelupSpellList(cInfo->family) : NULL)
    {
        for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
        {
            if (!petDefSpells.spellid[j])
                continue;

            for (PetLevelupSpellSet::const_iterator itr = levelupSpells->begin(); itr != levelupSpells->end(); ++itr)
            {
                if (itr->second == petDefSpells.spellid[j])
                {
                    petDefSpells.spellid[j] = 0;
                    break;
                }
            }
        }
    }

    // skip empty list;
    have_spell = false;
    for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
    {
        if (petDefSpells.spellid[j])
        {
            have_spell = true;
            break;
        }
    }

    return have_spell;
}

void SpellMgr::LoadPetDefaultSpells()
{
    uint32 oldMSTime = getMSTime();

    mPetDefaultSpellsMap.clear();

    uint32 countCreature = 0;
    uint32 countData = 0;

    CreatureTemplateContainer const* ctc = sObjectMgr->GetCreatureTemplates();
    for (CreatureTemplateContainer::const_iterator itr = ctc->begin(); itr != ctc->end(); ++itr)
    {

        if (!itr->second.PetSpellDataId)
            continue;

        // for creature with PetSpellDataId get default pet spells from dbc
        CreatureSpellDataEntry const* spellDataEntry = sCreatureSpellDataStore.LookupEntry(itr->second.PetSpellDataId);
        if (!spellDataEntry)
            continue;

        int32 petSpellsId = -int32(itr->second.PetSpellDataId);
        PetDefaultSpellsEntry petDefSpells;
        for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
            petDefSpells.spellid[j] = spellDataEntry->spellId[j];

        if (LoadPetDefaultSpells_helper(&itr->second, petDefSpells))
        {
            mPetDefaultSpellsMap[petSpellsId] = petDefSpells;
            ++countData;
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded addition spells for %u pet spell data entries in %u ms", countData, GetMSTimeDiffToNow(oldMSTime));

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, "Loading summonable creature templates...");
    oldMSTime = getMSTime();

    // different summon spells
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        SpellInfo const* spellEntry = GetSpellInfo(i);
        if (!spellEntry)
            continue;

        for (uint8 k = 0; k < MAX_SPELL_EFFECTS; ++k)
        {
            if (spellEntry->Effects[k].Effect == SPELL_EFFECT_SUMMON || spellEntry->Effects[k].Effect == SPELL_EFFECT_SUMMON_PET)
            {
                uint32 creature_id = spellEntry->Effects[k].MiscValue;
                CreatureTemplate const* cInfo = sObjectMgr->GetCreatureTemplate(creature_id);
                if (!cInfo)
                    continue;

                // already loaded
                if (cInfo->PetSpellDataId)
                    continue;

                // for creature without PetSpellDataId get default pet spells from creature_template
                int32 petSpellsId = cInfo->Entry;
                if (mPetDefaultSpellsMap.find(cInfo->Entry) != mPetDefaultSpellsMap.end())
                    continue;

                PetDefaultSpellsEntry petDefSpells;
                for (uint8 j = 0; j < MAX_CREATURE_SPELL_DATA_SLOT; ++j)
                    petDefSpells.spellid[j] = cInfo->spells[j];

                if (LoadPetDefaultSpells_helper(cInfo, petDefSpells))
                {
                    mPetDefaultSpellsMap[petSpellsId] = petDefSpells;
                    ++countCreature;
                }
            }
        }
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u summonable creature templates in %u ms", countCreature, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadSpellAreas()
{
    uint32 oldMSTime = getMSTime();

    mSpellAreaMap.clear();                                  // need for reload case
    mSpellAreaForQuestMap.clear();
    mSpellAreaForActiveQuestMap.clear();
    mSpellAreaForQuestEndMap.clear();
    mSpellAreaForAuraMap.clear();

    //                                                  0     1         2              3               4                 5          6          7       8         9
    QueryResult result = WorldDatabase.Query("SELECT spell, area, quest_start, quest_start_status, quest_end_status, quest_end, aura_spell, racemask, gender, autocast FROM spell_area");
    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell area requirements. DB table `spell_area` is empty.");

        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell = fields[0].GetUInt32();
        SpellArea spellArea;
        spellArea.spellId             = spell;
        spellArea.areaId              = fields[1].GetInt32();
        spellArea.questStart          = fields[2].GetUInt32();
        spellArea.questStartStatus    = fields[3].GetUInt32();
        spellArea.questEndStatus      = fields[4].GetUInt32();
        spellArea.questEnd            = fields[5].GetUInt32();
        spellArea.auraSpell           = fields[6].GetInt32();
        spellArea.raceMask            = fields[7].GetUInt32();
        spellArea.gender              = Gender(fields[8].GetUInt8());
        spellArea.autocast            = fields[9].GetBool();

        if (SpellInfo const* spellInfo = GetSpellInfo(spell))
        {
            if (spellArea.autocast)
                const_cast<SpellInfo*>(spellInfo)->Attributes |= SPELL_ATTR0_CANT_CANCEL;
        }
        else
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` does not exist", spell);
            continue;
        }

        {
            bool ok = true;
            SpellAreaMapBounds sa_bounds = GetSpellAreaMapBounds(spellArea.spellId);
            for (SpellAreaMap::const_iterator itr = sa_bounds.first; itr != sa_bounds.second; ++itr)
            {
                if (spellArea.spellId != itr->second.spellId)
                    continue;
                if (spellArea.areaId != itr->second.areaId)
                    continue;
                if (spellArea.questStart != itr->second.questStart)
                    continue;
                if (spellArea.auraSpell != itr->second.auraSpell)
                    continue;
                if ((spellArea.raceMask & itr->second.raceMask) == 0)
                    continue;
                if (spellArea.gender != itr->second.gender)
                    continue;

                // duplicate by requirements
                ok =false;
                break;
            }

            if (!ok)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` already listed with similar requirements.", spell);
                continue;
            }
        }

        if (spellArea.areaId > 0 && !GetAreaEntryByAreaID(spellArea.areaId))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong area (%u) requirement", spell, spellArea.areaId);
            continue;
        }

        if (spellArea.areaId < 0 && !sMapStore.LookupEntry(abs(spellArea.areaId)))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong mapid (%u) requirement", spell, abs(spellArea.areaId));
            continue;
        }

        if (spellArea.questStart && !sObjectMgr->GetQuestTemplate(spellArea.questStart))
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong start quest (%u) requirement", spell, spellArea.questStart);
            continue;
        }

        if (spellArea.questEnd)
        {
            if (!sObjectMgr->GetQuestTemplate(spellArea.questEnd))
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong end quest (%u) requirement", spell, spellArea.questEnd);
                continue;
            }
        }

        if (spellArea.auraSpell)
        {
            SpellInfo const* spellInfo = GetSpellInfo(abs(spellArea.auraSpell));
            if (!spellInfo)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong aura spell (%u) requirement", spell, abs(spellArea.auraSpell));
                continue;
            }

            if (uint32(abs(spellArea.auraSpell)) == spellArea.spellId)
            {
                sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have aura spell (%u) requirement for itself", spell, abs(spellArea.auraSpell));
                continue;
            }

            // not allow autocast chains by auraSpell field (but allow use as alternative if not present)
            if (spellArea.autocast && spellArea.auraSpell > 0)
            {
                bool chain = false;
                SpellAreaForAuraMapBounds saBound = GetSpellAreaForAuraMapBounds(spellArea.spellId);
                for (SpellAreaForAuraMap::const_iterator itr = saBound.first; itr != saBound.second; ++itr)
                {
                    if (itr->second->autocast && itr->second->auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }

                SpellAreaMapBounds saBound2 = GetSpellAreaMapBounds(spellArea.auraSpell);
                for (SpellAreaMap::const_iterator itr2 = saBound2.first; itr2 != saBound2.second; ++itr2)
                {
                    if (itr2->second.autocast && itr2->second.auraSpell > 0)
                    {
                        chain = true;
                        break;
                    }
                }

                if (chain)
                {
                    sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have aura spell (%u) requirement that itself autocast from aura", spell, spellArea.auraSpell);
                    continue;
                }
            }
        }

        if (spellArea.raceMask && (spellArea.raceMask & RACEMASK_ALL_PLAYABLE) == 0)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong race mask (%u) requirement", spell, spellArea.raceMask);
            continue;
        }

        if (spellArea.gender != GENDER_NONE && spellArea.gender != GENDER_FEMALE && spellArea.gender != GENDER_MALE)
        {
            sLog->outError(LOG_FILTER_SQL, "Spell %u listed in `spell_area` have wrong gender (%u) requirement", spell, spellArea.gender);
            continue;
        }

        SpellArea const* sa = &mSpellAreaMap.insert(SpellAreaMap::value_type(spell, spellArea))->second;

        // for search by current zone/subzone at zone/subzone change
        if (spellArea.areaId)
            mSpellAreaForAreaMap.insert(SpellAreaForAreaMap::value_type(spellArea.areaId, sa));

        // for search at quest start/reward
        if (spellArea.questStart)
            mSpellAreaForQuestMap.insert(SpellAreaForQuestMap::value_type(spellArea.questStart, sa));

        // for search at quest start/reward
        if (spellArea.questEnd)
            mSpellAreaForQuestEndMap.insert(SpellAreaForQuestMap::value_type(spellArea.questEnd, sa));

        // for search at aura apply
        if (spellArea.auraSpell)
            mSpellAreaForAuraMap.insert(SpellAreaForAuraMap::value_type(abs(spellArea.auraSpell), sa));

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell area requirements in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

static const uint32 SkillClass[MAX_CLASSES] = {0, 840, 800, 795, 921, 804, 796, 924, 904, 849, 829, 798};

void SpellMgr::LoadSpellClassInfo()
{
    mSpellClassInfo.resize(MAX_CLASSES);
    for (int ClassID = 0; ClassID < MAX_CLASSES; ClassID++)
    {
        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(ClassID);
        if(!classEntry)
            continue;

        // Player damage reduction (40% base resilience)
        mSpellClassInfo[ClassID].insert(115043);
        // Player mastery activation
        mSpellClassInfo[ClassID].insert(114585);

        // All Rune for DK
        if (ClassID == CLASS_DEATH_KNIGHT)
        {
            mSpellClassInfo[ClassID].insert(53323);
            mSpellClassInfo[ClassID].insert(54447);
            mSpellClassInfo[ClassID].insert(53342);
            mSpellClassInfo[ClassID].insert(53331);
            mSpellClassInfo[ClassID].insert(54446);
            mSpellClassInfo[ClassID].insert(53323);
            mSpellClassInfo[ClassID].insert(53344);
            mSpellClassInfo[ClassID].insert(70164);
            mSpellClassInfo[ClassID].insert(62158);
        }

        // Swift Flight Form
        if (ClassID == CLASS_DRUID)
            mSpellClassInfo[ClassID].insert(40120);

        // Dark Soul
        if (ClassID == CLASS_WARLOCK)
            mSpellClassInfo[ClassID].insert(77801);

        // All portals and teleports for mages
        if (ClassID == CLASS_MAGE)
        {
            mSpellClassInfo[ClassID].insert(3561);
            mSpellClassInfo[ClassID].insert(3562);
            mSpellClassInfo[ClassID].insert(3563);
            mSpellClassInfo[ClassID].insert(3565);
            mSpellClassInfo[ClassID].insert(3566);
            mSpellClassInfo[ClassID].insert(3567);
            mSpellClassInfo[ClassID].insert(32271);
            mSpellClassInfo[ClassID].insert(32272);
            mSpellClassInfo[ClassID].insert(49359);
            mSpellClassInfo[ClassID].insert(49360);
            mSpellClassInfo[ClassID].insert(32266);
            mSpellClassInfo[ClassID].insert(32267);
            mSpellClassInfo[ClassID].insert(10059);
            mSpellClassInfo[ClassID].insert(11416);
            mSpellClassInfo[ClassID].insert(11417);
            mSpellClassInfo[ClassID].insert(11418);
            mSpellClassInfo[ClassID].insert(11419);
            mSpellClassInfo[ClassID].insert(11420);
            mSpellClassInfo[ClassID].insert(49358);
            mSpellClassInfo[ClassID].insert(49361);
            mSpellClassInfo[ClassID].insert(35715);
            mSpellClassInfo[ClassID].insert(33690);
            mSpellClassInfo[ClassID].insert(33691);
            mSpellClassInfo[ClassID].insert(35717);
            mSpellClassInfo[ClassID].insert(53140);
            mSpellClassInfo[ClassID].insert(53142);
            mSpellClassInfo[ClassID].insert(88342);
            mSpellClassInfo[ClassID].insert(88344);
            mSpellClassInfo[ClassID].insert(88345);
            mSpellClassInfo[ClassID].insert(88346);
        }

        // Ancestral Focus
        if (ClassID == CLASS_SHAMAN)
            mSpellClassInfo[ClassID].insert(89920);

        // Plate Mail skill
        if (ClassID == CLASS_PALADIN || ClassID == CLASS_WARRIOR)
            mSpellClassInfo[ClassID].insert(750);

        // Mail skill
        if (ClassID == CLASS_SHAMAN || ClassID == CLASS_HUNTER)
            mSpellClassInfo[ClassID].insert(8737);

        // Dual Wield
        if (ClassID == CLASS_HUNTER || ClassID == CLASS_ROGUE || ClassID == CLASS_DEATH_KNIGHT)
            mSpellClassInfo[ClassID].insert(674);

        // Natural Insight druid
        if (ClassID == CLASS_DRUID)
            mSpellClassInfo[ClassID].insert(112857);
        
        // Sinister Strike Enabler
        if (ClassID == CLASS_ROGUE)
            mSpellClassInfo[ClassID].insert(79327);

        // Opening gameobject
        if (ClassID == CLASS_MONK)
        {
            mSpellClassInfo[ClassID].insert(3365);
            mSpellClassInfo[ClassID].insert(6247);
            mSpellClassInfo[ClassID].insert(6477);
            mSpellClassInfo[ClassID].insert(6478);
            mSpellClassInfo[ClassID].insert(21651);
            mSpellClassInfo[ClassID].insert(22810);
            mSpellClassInfo[ClassID].insert(61437);
            mSpellClassInfo[ClassID].insert(68398);
            mSpellClassInfo[ClassID].insert(96220);
        }

        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(i);
            if (!skillLine)
                continue;

            SpellInfo const* spellEntry = sSpellMgr->GetSpellInfo(skillLine->spellId);
            if (!spellEntry)
                continue;

            if (spellEntry->SpellLevel == 0)
                continue;

            if (skillLine->skillId !=  SkillClass[ClassID] || skillLine->learnOnGetSkill != ABILITY_LEARNED_ON_GET_RACE_OR_CLASS_SKILL)
                continue;

            // See CGSpellBook::InitFutureSpells in client
            if (spellEntry->Attributes & SPELL_ATTR0_TRADESPELL || spellEntry->Attributes & SPELL_ATTR0_HIDDEN_CLIENTSIDE
                || spellEntry->AttributesEx8 & SPELL_ATTR8_UNK13 || spellEntry->AttributesEx4 & SPELL_ATTR4_UNK15)
                continue;

            if (sSpellMgr->IsTalent(spellEntry->Id))
                continue;

            mSpellClassInfo[ClassID].insert(spellEntry->Id);
        }

        for (uint32 i = 0; i < sSpecializationSpellStore.GetNumRows(); ++i)
        {
            SpecializationSpellEntry const* specializationInfo = sSpecializationSpellStore.LookupEntry(i);
            if (!specializationInfo)
                continue;

            ChrSpecializationsEntry const* chrSpec = sChrSpecializationsStore.LookupEntry(specializationInfo->SpecializationEntry);
            if (!chrSpec)
                continue;

            mSpellClassInfo[chrSpec->classId].insert(specializationInfo->LearnSpell);
        }
    }


}

struct spellDifficultyLoadInfo
{
    uint32 id;
    std::list<uint32> difficultyList;
};

void SpellMgr::LoadSpellInfoStore()
{
    uint32 oldMSTime = getMSTime();

    UnloadSpellInfoStore();
    mSpellInfoMap.resize(sSpellStore.GetNumRows(), NULL);

    for (uint32 i = 0; i < sSpellStore.GetNumRows(); ++i)
        if (SpellEntry const* spellEntry = sSpellStore.LookupEntry(i))
            mSpellInfoMap[i] = new SpellInfo(spellEntry);

    for (uint32 i = 0; i < sSpellPowerStore.GetNumRows(); i++)
    {
        SpellPowerEntry const* spellPower = sSpellPowerStore.LookupEntry(i);
        if (!spellPower)
            continue;

        SpellInfo* spell = mSpellInfoMap[spellPower->SpellId];
        if (!spell)
            continue;

        spell->ManaCost = spellPower->manaCost;
        spell->ManaCostPercentage = spellPower->ManaCostPercentage;
        spell->ManaPerSecond = spellPower->manaPerSecond;
        spell->PowerType = spellPower->powerType;

        if (!spell->AddPowerData(spellPower))
            sLog->outInfo(LOG_FILTER_WORLDSERVER, "Spell - %u has more powers than two.", spell->Id);
    }

    for (uint32 i = 0; i < sTalentStore.GetNumRows(); i++)
    {
        TalentEntry const* talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
            continue;

        SpellInfo * spellEntry = mSpellInfoMap[talentInfo->spellId];
        if(spellEntry)
            spellEntry->talentId = talentInfo->Id;
    }

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded spell info store in %u ms", GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::UnloadSpellInfoStore()
{
    for (uint32 i = 0; i < mSpellInfoMap.size(); ++i)
    {
        if (mSpellInfoMap[i])
            delete mSpellInfoMap[i];
    }
    mSpellInfoMap.clear();
}

void SpellMgr::UnloadSpellInfoImplicitTargetConditionLists()
{
    for (uint32 i = 0; i < mSpellInfoMap.size(); ++i)
    {
        if (mSpellInfoMap[i])
            mSpellInfoMap[i]->_UnloadImplicitTargetConditionLists();
    }
}

void SpellMgr::LoadSpellCustomAttr()
{
    uint32 oldMSTime = getMSTime();

    SpellInfo* spellInfo = NULL;
    for (uint32 i = 0; i < GetSpellInfoStoreSize(); ++i)
    {
        {
            spellInfo = mSpellInfoMap[i];
            if (!spellInfo)
                continue;

            for (uint8 j = 0; j < MAX_SPELL_EFFECTS; ++j)
            {
                switch (spellInfo->Effects[j].ApplyAuraName)
                {
                    case SPELL_AURA_MOD_POSSESS:
                    case SPELL_AURA_MOD_CONFUSE:
                    case SPELL_AURA_MOD_CHARM:
                    case SPELL_AURA_AOE_CHARM:
                    case SPELL_AURA_MOD_FEAR:
                    case SPELL_AURA_MOD_STUN:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                        break;
                    case SPELL_AURA_PERIODIC_HEAL:
                    case SPELL_AURA_PERIODIC_DAMAGE:
                    case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                    case SPELL_AURA_PERIODIC_LEECH:
                    case SPELL_AURA_PERIODIC_MANA_LEECH:
                    case SPELL_AURA_PERIODIC_HEALTH_FUNNEL:
                    case SPELL_AURA_PERIODIC_ENERGIZE:
                    case SPELL_AURA_OBS_MOD_HEALTH:
                    case SPELL_AURA_OBS_MOD_POWER:
                    case SPELL_AURA_POWER_BURN:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_NO_INITIAL_THREAT;
                        break;
                }

                switch (spellInfo->Effects[j].Effect)
                {
                    case SPELL_EFFECT_SCHOOL_DAMAGE:
                    case SPELL_EFFECT_WEAPON_DAMAGE:
                    case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
                    case SPELL_EFFECT_NORMALIZED_WEAPON_DMG:
                    case SPELL_EFFECT_WEAPON_PERCENT_DAMAGE:
                    case SPELL_EFFECT_HEAL:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_DIRECT_DAMAGE;
                        break;
                    case SPELL_EFFECT_POWER_DRAIN:
                    case SPELL_EFFECT_POWER_BURN:
                    case SPELL_EFFECT_HEAL_MAX_HEALTH:
                    case SPELL_EFFECT_HEALTH_LEECH:
                    case SPELL_EFFECT_HEAL_PCT:
                    case SPELL_EFFECT_ENERGIZE_PCT:
                    case SPELL_EFFECT_ENERGIZE:
                    case SPELL_EFFECT_HEAL_MECHANICAL:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_NO_INITIAL_THREAT;
                        break;
                    case SPELL_EFFECT_CHARGE:
                    case SPELL_EFFECT_CHARGE_DEST:
                    case SPELL_EFFECT_JUMP:
                    case SPELL_EFFECT_JUMP_DEST:
                    case SPELL_EFFECT_LEAP_BACK:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_CHARGE;
                        break;
                    case SPELL_EFFECT_PICKPOCKET:
                        spellInfo->AttributesCu |= SPELL_ATTR0_CU_PICKPOCKET;
                        break;
                    case SPELL_EFFECT_ENCHANT_ITEM:
                    case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
                    case SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC:
                    case SPELL_EFFECT_ENCHANT_HELD_ITEM:
                    {
                        // only enchanting profession enchantments procs can stack
                        if (IsPartOfSkillLine(SKILL_ENCHANTING, i))
                        {
                            uint32 enchantId = spellInfo->Effects[j].MiscValue;
                            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchantId);
                            for (uint8 s = 0; s < MAX_ITEM_ENCHANTMENT_EFFECTS; ++s)
                            {
                                if (enchant->type[s] != ITEM_ENCHANTMENT_TYPE_COMBAT_SPELL)
                                    continue;

                                SpellInfo* procInfo = (SpellInfo*)GetSpellInfo(enchant->spellid[s]);
                                if (!procInfo)
                                    continue;

                                // if proced directly from enchantment, not via proc aura
                                // NOTE: Enchant Weapon - Blade Ward also has proc aura spell and is proced directly
                                // however its not expected to stack so this check is good
                                if (procInfo->HasAura(SPELL_AURA_PROC_TRIGGER_SPELL))
                                    continue;

                                procInfo->AttributesCu |= SPELL_ATTR0_CU_ENCHANT_PROC;
                            }
                        }
                        break;
                    }
                    case SPELL_EFFECT_CREATE_ITEM:
                    case SPELL_EFFECT_CREATE_ITEM_2:
                        mSpellCreateItemList.push_back(i);
                        break;
                }
            }

            if (!spellInfo->_IsPositiveEffect(EFFECT_0, false))
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_NEGATIVE_EFF0;

            if (!spellInfo->_IsPositiveEffect(EFFECT_1, false))
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_NEGATIVE_EFF1;

            if (!spellInfo->_IsPositiveEffect(EFFECT_2, false))
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_NEGATIVE_EFF2;

            if (spellInfo->SpellVisual[0] == 3879)
                spellInfo->AttributesCu |= SPELL_ATTR0_CU_CONE_BACK;

            switch (spellInfo->Id)
            {
                case 79136: // Venomous Wound
                    spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_INITIAL_AGGRO;
                    spellInfo->Speed = 25.f;
                    break;
                case 125831: // Taste for Blood
                    spellInfo->ProcCharges = 1;
                    break;
                case 124465: // Mastery: Vampiric Touch
                    spellInfo->CastTimeMin = 0;
                    spellInfo->CastTimeMax = 0;
                    break;
                case 2094: // Blind
                    spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_INITIAL_AGGRO;
                    spellInfo->AttributesEx3 &= ~SPELL_ATTR3_CANT_TRIGGER_PROC;
                    break;
                case 123154: // Fists of Fury Visual Target
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(36);
                    break;
                case 8676:   // Ambush
                    spellInfo->Effects[0].ScalingMultiplier = 1.787f;
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET;
                    break;
                case 122013: // Glyph of Incite (Protection)
                    spellInfo->AttributesEx3 = SPELL_ATTR3_CAN_PROC_WITH_TRIGGERED;
                    break;
                case 53: // Backstab
                    spellInfo->Effects[0].ScalingMultiplier = 0.845f;
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET;
                    break;
                case 127424:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CONE_ENEMY_54;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 1776: // Gouge
                case 1777:
                case 8629:
                case 11285:
                case 11286:
                case 12540:
                case 13579:
                case 24698:
                case 28456:
                case 29425:
                case 34940:
                case 36862:
                case 38764:
                case 38863:
                case 52743: // Head Smack
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_REQ_TARGET_FACING_CASTER;
                    break;
                case 2589:
                case 2590:
                case 2591:
                case 8721:
                case 11279:
                case 11280:
                case 11281:
                case 25300:
                case 26863:
                case 48656:
                case 48657:
                case 8631:
                case 8632:
                case 8633:
                case 11289:
                case 11290:
                case 26839:
                case 26884:
                case 48675:
                case 48676:
                case 5221: // Shred
                case 6800:
                case 8992:
                case 9829:
                case 9830:
                case 27001:
                case 27002:
                case 48571:
                case 48572:
                case 8724:
                case 8725:
                case 11267:
                case 11268:
                case 11269:
                case 27441:
                case 48689:
                case 48690:
                case 48691:
                case 6787:
                case 9866:
                case 9867:
                case 27005:
                case 48578:
                case 48579:
                case 21987: // Lash of Pain
                case 23959: // Test Stab R50
                case 24825: // Test Backstab
                case 58563: // Assassinate Restless Lookout
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_REQ_CASTER_BEHIND_TARGET;
                    break;
                // Pandashan's Dragon Gun
                case 120751:
                case 120876:
                case 120964:
                case 124347:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CONE_ENEMY_54;
                    break;
                case 26029: // Dark Glare
                case 37433: // Spout
                case 43140: // Flame Breath
                case 43215: // Flame Breath
                case 70461: // Coldflame Trap
                case 72133: // Pain and Suffering
                case 73788: // Pain and Suffering
                case 73789: // Pain and Suffering
                case 73790: // Pain and Suffering
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_CONE_LINE;
                    break;
                case 24340: // Meteor
                case 26558: // Meteor
                case 28884: // Meteor
                case 36837: // Meteor
                case 38903: // Meteor
                case 41276: // Meteor
                case 57467: // Meteor
                case 26789: // Shard of the Fallen Star
                case 31436: // Malevolent Cleave
                case 35181: // Dive Bomb
                case 40810: // Saber Lash
                case 43267: // Saber Lash
                case 43268: // Saber Lash
                case 42384: // Brutal Swipe
                case 45150: // Meteor Slash
                case 64688: // Sonic Screech
                case 72373: // Shared Suffering
                case 71904: // Chaos Bane
                case 70492: // Ooze Eruption
                case 72505: // Ooze Eruption
                case 72624: // Ooze Eruption
                case 72625: // Ooze Eruption
                    // ONLY SPELLS WITH SPELLFAMILY_GENERIC and EFFECT_SCHOOL_DAMAGE
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_SHARE_DAMAGE;
                    break;
                case 18500: // Wing Buffet
                case 33086: // Wild Bite
                case 49749: // Piercing Blow
                case 52890: // Penetrating Strike
                case 53454: // Impale
                case 59446: // Impale
                case 62383: // Shatter
                case 64777: // Machine Gun
                case 65239: // Machine Gun
                case 65919: // Impale
                case 67858: // Impale
                case 67859: // Impale
                case 67860: // Impale
                case 69293: // Wing Buffet
                case 74439: // Machine Gun
                case 63278: // Mark of the Faceless (General Vezax)
                case 62544: // Thrust (Argent Tournament)
                case 64588: // Thrust (Argent Tournament)
                case 66479: // Thrust (Argent Tournament)
                case 68505: // Thrust (Argent Tournament)
                case 62709: // Counterattack! (Argent Tournament)
                case 62626: // Break-Shield (Argent Tournament, Player)
                case 64590: // Break-Shield (Argent Tournament, Player)
                case 64342: // Break-Shield (Argent Tournament, NPC)
                case 64686: // Break-Shield (Argent Tournament, NPC)
                case 65147: // Break-Shield (Argent Tournament, NPC)
                case 68504: // Break-Shield (Argent Tournament, NPC)
                case 62874: // Charge (Argent Tournament, Player)
                case 68498: // Charge (Argent Tournament, Player)
                case 64591: // Charge (Argent Tournament, Player)
                case 63003: // Charge (Argent Tournament, NPC)
                case 63010: // Charge (Argent Tournament, NPC)
                case 68321: // Charge (Argent Tournament, NPC)
                case 72255: // Mark of the Fallen Champion (Deathbringer Saurfang)
                case 72444: // Mark of the Fallen Champion (Deathbringer Saurfang)
                case 72445: // Mark of the Fallen Champion (Deathbringer Saurfang)
                case 72446: // Mark of the Fallen Champion (Deathbringer Saurfang)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_IGNORE_ARMOR;
                    break;
                case 64422: // Sonic Screech (Auriaya)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_SHARE_DAMAGE;
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_IGNORE_ARMOR;
                    break;
                case 72293: // Mark of the Fallen Champion (Deathbringer Saurfang)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_NEGATIVE_EFF0;
                    break;
                // Custom MoP Script
                case 82691: // Ring of Frost
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = 0;
                    spellInfo->AttributesEx |= SPELL_ATTR1_CANT_BE_REFLECTED;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(13);
                    break;
                case 91021: // Find Weakness
                    spellInfo->Effects[0].BasePoints = 70;
                    break;
                case 76577: // Smoke Bomb
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 130616:// Glyph of Fear
                    spellInfo->AttributesEx3 &= ~SPELL_ATTR3_IGNORE_HIT_RESULT;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_STUN;
                    spellInfo->AuraInterruptFlags &= ~AURA_INTERRUPT_FLAG_TAKE_DAMAGE2;
                    break;
                case 118699:// Fear Effect
                    spellInfo->AttributesEx3 &= ~SPELL_ATTR3_IGNORE_HIT_RESULT;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_FEAR;
                    spellInfo->AuraInterruptFlags &= ~AURA_INTERRUPT_FLAG_TAKE_DAMAGE2;
                    break;
                case 124991:// Nature's Vigil (Damage)
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 124988:// Nature's Vigil (Heal)
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ALLY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 122355:// Molten Core
                    spellInfo->StackAmount = 255;
                    break;
                case 6203:  // Soulstone
                    spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_DEAD;
                    break;
                case 105622:// Clemency
                    spellInfo->Effects[4].Effect = 0;
                    spellInfo->Effects[4].BasePoints = 0;
                    spellInfo->Effects[4].ApplyAuraName = 0;
                    spellInfo->Effects[5].Effect = 0;
                    spellInfo->Effects[5].BasePoints = 0;
                    spellInfo->Effects[5].ApplyAuraName = 0;
                    spellInfo->Effects[6].Effect = 0;
                    spellInfo->Effects[6].BasePoints = 0;
                    spellInfo->Effects[6].ApplyAuraName = 0;
                    spellInfo->Effects[7].Effect = 0;
                    spellInfo->Effects[7].BasePoints = 0;
                    spellInfo->Effects[8].ApplyAuraName = 0;
                    break;
                case 118291:// Greater Fire Elemental
                case 118323:// Greater Earth Elemental
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_SUMMON_PET;
                    break;
                case 114942:// Healing Tide
                    spellInfo->SpellFamilyFlags[0] = 0x00002000;
                    break;
                case 108283:// Echo of the Elements
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 62099: // Shamanism
                    spellInfo->Effects[0].SpellClassMask[2] |= 0x8000;
                    break;
                case 116943:// Earthgrab
                    spellInfo->AttributesEx5 |= SPELL_ATTR5_START_PERIODIC_AT_APPLY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 6544:  // Heroic Leap
                    spellInfo->Effects[2].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[2].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->Effects[2].TargetA = TARGET_UNIT_CASTER;
                    spellInfo->Effects[2].TargetB = 0;
                    spellInfo->Effects[2].BasePoints = 0;
                    break;
                case 116198:// Enfeeblement Aura
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    spellInfo->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[1].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[1].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    spellInfo->Effects[2].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[2].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[2].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    spellInfo->Effects[4].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[4].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    break;
                case 116202:// Elements Aura
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    spellInfo->Effects[2].TargetA = TARGET_DEST_TARGET_ENEMY;
                    spellInfo->Effects[2].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    break;
                case 94339: // Fungal Area
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(18); // 20s
                    break;
                case 81282: // Fungal Growth
                    spellInfo->Effects[0].BasePoints = 100;
                    break;
                case 108446:// Soul Link
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_PET;
                    break;
                case 974:   // Earth Shield
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_HEALING_RECEIVED;
                    break;
                case 86529: // Mail Specialization (Shaman)
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 61999: // Raise Ally
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_TARGET_ALLY;
                    break;
                case 49016: // Unholy Frenzy
                    spellInfo->Effects[0].BasePoints = 30;
                    break;
                case 31935: // Avenger's Shield
                    spellInfo->DmgClass = SPELL_DAMAGE_CLASS_MAGIC;
                    break;
                case 44203: // Tranquility (triggered)
                    spellInfo->MaxAffectedTargets = 5;
                    break;
                case 121118:// Dire Beast summons
                case 122802:
                case 122804:
                case 122806:
                case 122807:
                case 122809:
                case 122811:
                case 126213:
                case 126214:
                case 126215:
                case 126216:
                case 132764:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 19574: // Bestial Wrath
                    spellInfo->Effects[3].Effect = 0;
                    spellInfo->Effects[3].ApplyAuraName = 0;
                    break;
                case 87935: // Serpent Spread
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(21); // -1s
                    break;
                case 53257: // Cobra Strikes
                    spellInfo->Effects[0].ApplyAuraName = 0;
                    spellInfo->Effects[0].Effect = 0;
                    spellInfo->Effects[0].BasePoints = 0;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_CRIT_PCT;
                    break;
                case 1126:  // Mark of the Wild
                case 19740: // Blessing of Might
                case 20217: // Blessing of Kings
                case 21562: // Power Word : Fortitude
                //case 24907: // Moonkin Aura
                case 109773:// Dark Intent
                case 116781:// Legacy of the White Tiger
                case 127830:// Spirit Beast Blessing
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CASTER;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_CASTER_AREA_RAID;
                    break;
                case 1459:  // Arcane Illumination
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CASTER_AREA_RAID;
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_CASTER_AREA_RAID;
                    break;
                case 61316: // Dalaran Illumination
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CASTER_AREA_RAID;
                    spellInfo->Effects[2].TargetA = TARGET_UNIT_CASTER_AREA_RAID;
                    break;
                case 116033:// Sparring (stacks)
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 115073:// Spinning Fire Blossom
                    spellInfo->Effects[0].TargetA = 0;
                    spellInfo->Effects[0].Effect = 0;
                    spellInfo->Effects[0].BasePoints = 0;
                    break;
                case 86150: // Guardian of Ancient Kings
                    spellInfo->Effects[0].TargetA = TARGET_CHECK_ENTRY;
                    break;
                case 86674: // Ancient Healer
                    spellInfo->ProcCharges = 5;
                    break;
                case 86657: // Ancient Guardian
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ANY;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN;
                    break;
                case 5782:  // Fear
                    spellInfo->Mechanic = 0;
                    spellInfo->Effects[0].Mechanic = MECHANIC_NONE;
                    break;
                case 51460: // Runic Corruption
                    spellInfo->Effects[EFFECT_1].Effect = 0;
                    spellInfo->Effects[EFFECT_0].ApplyAuraName = SPELL_AURA_MOD_POWER_REGEN_PERCENT;
                    spellInfo->Effects[EFFECT_0].MiscValue = 5;
                    spellInfo->Effects[EFFECT_0].MiscValueB = NUM_RUNE_TYPES;
                    break;
                case 45204: // Mirror Image - Clone Me!
                    spellInfo->AttributesEx6 |= SPELL_ATTR6_CAN_TARGET_INVISIBLE;
                    spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_NOT_IN_LOS;
                    break;
                case 41055: // Copy Weapon Spells
                case 45206:
                case 63416:
                case 69891:
                case 69892:
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_DUMMY;
                    spellInfo->Mechanic = 0;
                    break;
                case 115175:// Soothing Mists
                case 116694:// Surging Mists
                case 117952:// Crackling Jade Lightning
                    spellInfo->PreventionType = SPELL_PREVENTION_TYPE_SILENCE;
                    break;
                case 102793:
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_DECREASE_SPEED;
                    break;
                case 172:   // Corruption
                case 30108: // Unstable Affliction
                    spellInfo->Effects[2].Effect = 0;
                    spellInfo->Effects[2].ApplyAuraName = 0;
                    spellInfo->Effects[2].BasePoints = 0;
                    break;
                case 64904: // Hymn of Hope
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_INCREASE_ENERGY_PERCENT;
                    break;
                case 81751: // Atonement
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ALLY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 47515: // Divine Aegis
                    spellInfo->Effects[0].BasePoints = 50;
                    break;
                case 108201:// Desecrated Ground
                    spellInfo->AttributesEx5 |= SPELL_ATTR5_USABLE_WHILE_FEARED;
                    spellInfo->AttributesEx5 |= SPELL_ATTR5_USABLE_WHILE_STUNNED;
                    spellInfo->AttributesEx5 |= SPELL_ATTR5_USABLE_WHILE_CONFUSED;
                    spellInfo->AttributesEx |= SPELL_ATTR1_DISPEL_AURAS_ON_IMMUNITY;
                    break;
                case 61336: // Survival Instincts
                    spellInfo->Effects[0].BasePoints = -50;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN;
                    break;
                case 33763: // Lifebloom
                    spellInfo->AttributesEx5 |= SPELL_ATTR5_SINGLE_TARGET_SPELL;
                    break;
                case 1160:  // Demoralizing Shout
                case 1966:  // Feint
                case 50256:  // Demoralizing Roar
                case 108212:  // Burst of Speed
                    spellInfo->AttributesEx |= SPELL_ATTR1_NOT_BREAK_STEALTH;
                    break;
                case 130493:// Nightstalker
                    spellInfo->Effects[1].Effect = 0;
                    spellInfo->Effects[1].ApplyAuraName = 0;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE;
                    spellInfo->Effects[0].MiscValue = SPELL_SCHOOL_MASK_ALL;
                    break;
                case 84745: // Shallow Insight
                case 84746: // Moderate Insight
                case 84747: // Deep Insight
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_DAMAGE_PERCENT_DONE;
                    spellInfo->Effects[0].MiscValue = SPELL_SCHOOL_MASK_ALL;
                    break;
                case 117895:// Eminence (statue)
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ALLY;
                    spellInfo->Effects[0].TargetB = NULL;
                    break;
                case 115294:// Mana Tea
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(36); // 1s
                    break;
                case 44457: // Living Bomb
                    spellInfo->AttributesEx5 &= ~SPELL_ATTR5_SINGLE_TARGET_SPELL;
                    break;
                case 44461: // Living Bomb
                    spellInfo->MaxAffectedTargets = 3;
                    break;
                case 23691: // Berzerker Rage Effect
                    spellInfo->Effects[0].BasePoints = 100;
                    break;
                case 114205:// Demoralizing Banner
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(8); // 15s
                    break;
                case 127630:// Cascade - damage trigger
                case 120786:// Cascade - heal trigger
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_TARGET_ANY;
                    spellInfo->Effects[1].TargetB = 0;
                    break;
                case 122128:// Divine Star (shadow)
                case 110745:// Divine Star (other)
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(29);
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(29);
                    break;
                case 122507:// Rallying Cry
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_INCREASE_HEALTH_PERCENT;
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(1);
                    break;
                case 111771:// Demonic Gateway
                    spellInfo->Effects[2].Effect = 0;
                    spellInfo->Effects[2].TriggerSpell = 0;
                    break;
                case 117828:// Backdraft
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
                    spellInfo->Effects[0].MiscValue = SPELLMOD_CASTING_TIME;
                    spellInfo->Effects[0].BasePoints = -30;
                    spellInfo->Effects[0].SpellClassMask[1] |= 0x2000;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
                    spellInfo->Effects[1].MiscValue = SPELLMOD_COST;
                    spellInfo->Effects[1].BasePoints = -30;
                    spellInfo->Effects[1].SpellClassMask[1] |= 0x80;
                    spellInfo->Effects[1].SpellClassMask[0] |= 0x1000;
                    spellInfo->Effects[2].ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
                    spellInfo->Effects[2].MiscValue = SPELLMOD_CASTING_TIME;
                    spellInfo->Effects[2].BasePoints = -30;
                    spellInfo->Effects[2].SpellClassMask[1] |= 0x80;
                    spellInfo->Effects[2].SpellClassMask[0] |= 0x1000;
                    break;
                case 109259:// Powershot
                    spellInfo->Effects[1].BasePoints = 60;
                    spellInfo->Effects[2].BasePoints = 800;
                    break;
                case 82926: // Fire ! (for Master Marksman)
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_ADD_PCT_MODIFIER;
                    spellInfo->Effects[0].MiscValue = SPELLMOD_CASTING_TIME;
                    spellInfo->Effects[0].BasePoints = -100;
                    spellInfo->Effects[0].SpellClassMask[0] |= 0x20000;
                    break;
                case 7384: // Overpower
                    spellInfo->AttributesEx |= SPELL_ATTR1_ENABLE_AT_DODGE;
                    break;
                case 114695:// Pursuit of Justice
                    spellInfo->Effects[0].BasePoints = 0;
                    break;
                case 6262:  // Health Stone
                case 34299: // Leader of the pack - healing
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_HEAL_PCT;
                    spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
                    break;
                case 90259: // Glyph of Frost Pillar (Root Aura)
                    spellInfo->Effects[0].MiscValue = 0;
                    spellInfo->Effects[0].MiscValueB = 0;
                    break;
                case 49821: // Mind Sear
                    spellInfo->Effects[0].TargetA = TARGET_DEST_CHANNEL_TARGET;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_DEST_AREA_ENEMY;
                    break;
                case 10326: // Turn Evil
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_MOD_FEAR;
                    break;
                case 117418:// Fists of Fury (damage)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_SHARE_DAMAGE;
                    break;
                case 113656:// Fists of Fury
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_TRIGGER_SPELL;
                    spellInfo->PreventionType = SPELL_PREVENTION_TYPE_SILENCE;
                    break;
                case 115315:// Summon Black Ox Statue
                    spellInfo->Effects[1].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_CASTER;
                    break;
                case 6346:  // Fear Ward
                case 110717:// Fear Ward (Symbiosis)
                case 48108: // Hot Streak
                case 57761: // Brain Freeze
                case 132158:// Nature's Swiftness
                case 74434: // Soul Burn
                case 34936: // Backlash
                //case 50334: // Berserk (bear)
                case 23920: // Spell Reflection
                case 114028:// Mass Spell Reflection
                case 113002:// Spell Reflection (Symbiosis)
                case 124430:// Divine Insight (Shadow)
                case 81292: // Glyph of Mind Spike
                case 114250:// Selfless Healer
                case 90174: // Divine Purpose
                case 89485: // Inner Focus
                case 131567:// Holy Spark
                    spellInfo->ProcCharges = 1;
                    break;
                case 110600:// Ice Trap (Symbiosis)
                    spellInfo->Effects[0].MiscValue = 164639;
                    break;
                case 110588:// Misdirection (Symbiosis)
                    spellInfo->Effects[2].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[2].ApplyAuraName = SPELL_AURA_MOD_SCALE;
                    spellInfo->Effects[2].BasePoints = 30;
                    break;
                case 122292:// Intervene (Symbiosis)
                    spellInfo->Effects[1].BasePoints = 100;
                    break;
                case 6358:  // Seduce (succubus)
                    spellInfo->SpellFamilyName = SPELLFAMILY_WARLOCK;
                    break;
                case 980:   // Agony
                    spellInfo->StackAmount = 10;
                    break;
                case 131740:// Corruption (Malefic Grasp)
                case 131736:// Unstable Affliction (Malefic Grasp)
                case 132566:// Seed of Corruption (Malefic Grasp)
                case 131737:// Agony (Malefic Grasp)
                    spellInfo->AttributesEx3 |= SPELL_ATTR3_NO_DONE_BONUS;
                    break;
                case 131116:// Allow to use Raging Blow
                    spellInfo->ProcCharges = 2;
                    break;
                case 44544: // Fingers of Frost
                    // affect Ice Lance
                    spellInfo->Effects[0].SpellClassMask[0] |= 0x20000;
                    break;
                case 85222: // Light of Dawn
                    spellInfo->MaxAffectedTargets = 6;
                    break;
                case 8122:  // Psychic Scream
                    spellInfo->Effects[2].ApplyAuraName = SPELL_AURA_MOD_FEAR;
                    spellInfo->MaxAffectedTargets = 5;
                    break;
                case 2641:  // Dismiss Pet
                    spellInfo->AttributesEx2 |= SPELL_ATTR2_CAN_TARGET_DEAD;
                    break;
                case 52042: // Healing Stream - Totem
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_HEAL;
                    spellInfo->Effects[0].BasePoints = 31;
                    break;
                case 324:   // Lightning Shield
                    spellInfo->ProcCharges = 0;
                    break;
                case 116740:// Tigereye Brew
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(1);
                    break;
                case 117993:// Chi Torpedo : Heal
                case 124040:// Chi Torpedo : Damage
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(9);
                    break;
                case 80325: // Camouflage
                case 119450:// Glyph of Camouflage
                    spellInfo->Effects[1].Effect = 0;
                    break;
                case 115295:// Guard
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_HEALING_DONE_PERCENT;
                    spellInfo->Effects[1].BasePoints = 30;
                    break;
                case 124273:// Heavy Stagger
                case 124274:// Moderate Stagger
                case 124275:// Light Stagger
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_APPLY_AURA;
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DAMAGE;
                    spellInfo->Effects[0].Amplitude = 1000;
                    spellInfo->Effects[0].BasePoints = 0;
                    break;
                case 126451:// Clash - Impact
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(29);
                    break;
                case 121253:// Keg Smash
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(14);
                    spellInfo->MaxAffectedTargets = 3;
                    break;
                case 115308:// Elusive Brew
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(1);
                    break;
                case 115129:// Expel Harm - Damage to a nearby ennemy within 10 yards
                    spellInfo->Effects[0].TargetA = TARGET_SRC_CASTER;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_NEARBY_ENEMY;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(13);
                    break;
                case 126892:// Zen Pilgrimage
                case 126895:// Zen Pilgrimage : Return
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_DUMMY;
                    break;
                case 130320:// Rising Sun Kick - Monks abilities deal 10% more damage
                    spellInfo->Effects[0].TargetA = TARGET_SRC_CASTER;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_SRC_AREA_ENEMY;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(14);
                    break;
                case 101546:// Spinning Crane Kick - Decrease Speed
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_MOD_DECREASE_SPEED;
                    spellInfo->Effects[1].BasePoints = -30;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(14);
                    break;
                case 107270:// Spinning Crane Kick - Radius
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(14);
                    break;
                case 127626:// Devouring plague - Heal
                    spellInfo->Effects[0].BasePoints = 1;
                    break;
                case 107223:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    break;
                case 106909:
                    {
                        SpellInfo const* spell = sSpellMgr->GetSpellInfo(113379);
                        if (!spell)
                            break;
                        spellInfo->DurationEntry = spell->DurationEntry;
                    }
                    break;
                case 113315:
                    {
                        SpellInfo const* spell = sSpellMgr->GetSpellInfo(113379);
                        if (!spell)
                            break;
                        spellInfo->DurationEntry = spell->DurationEntry;
                    }
                    break;
                case 106736:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_TARGET_ENEMY;
                    break;
                case 106113:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = TARGET_UNIT_TARGET_ENEMY;
                    break;
                case 119922: //Shockwave
                case 119929:
                case 119930:
                case 119931:
                case 119932:
                case 119933:
                    spellInfo->Speed = 5.f;
                    break;
                case 106112:
                    {
                        const SpellRadiusEntry* radius = sSpellRadiusStore.LookupEntry(22);
                        if (!radius)
                            break;

                        spellInfo->Effects[0].RadiusEntry = radius; //200yards.
                    }
                    break;
                case 106847:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ANY;
                    // Wise Mari Hydrolance damage
                case 106267:
                    //spellInfo->Effects[0].TargetB = TARGET_UNIT_TARGET_ENEMY;
                    break;
                    // Wise Mari Wash Away
                case 106334:
                    spellInfo->AttributesEx3 &= ~ SPELL_ATTR3_ONLY_TARGET_PLAYERS;
                    break;
                case 120552:
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(16);
                    break;
                case 119684:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CONE_ENEMY_24;
                    spellInfo->Effects[0].TargetB = 0;
                    spellInfo->Effects[1].TargetA = TARGET_UNIT_CONE_ENEMY_24;
                    spellInfo->Effects[1].TargetB = 0;
                    break;
                case 106853:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ENEMY;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 112060:
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 118685:
                    spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(5);
                    break;
                 // Malygos Enrage
                case 60670:
                    spellInfo->Effects[1].TriggerSpell = 0;
                    spellInfo->Effects[2].TriggerSpell = 0;
                    break;
                case 114746:
                    spellInfo->Effects[2].TargetA = TARGET_UNIT_TARGET_ALLY;
                    spellInfo->Effects[2].TargetB = 0;
                    break;
                case 104855:
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_TARGET_ANY;
                    break;
                // Add Server-Side dummy spell for Fishing
                // TODO : Add more generic system to load server-side spell
                case 7733:
                case 7734:
                case 18249:
                case 54083:
                case 54084:
                case 51293:
                case 88869:
                case 110412:
                {
                    SpellInfo* fishingDummy = new SpellInfo(sSpellStore.LookupEntry(131474));
                    fishingDummy->Id = spellInfo->Effects[0].TriggerSpell;
                    mSpellInfoMap[spellInfo->Effects[0].TriggerSpell] = fishingDummy;
                    break;
                }

                // Siege of the Niuzoa temple
                case 119941: //Puddle Void Zone
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(22);
                    break;
                case 124290: //Blade Rush Dmg trigger(sword)
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(26);
                    spellInfo->Effects[0].TargetA = 22;
                    spellInfo->Effects[0].TargetB = 15;
                    break;
                case 124312: //Blade Rush Charge
                    spellInfo->Effects[0].TargetA = 25;
                    spellInfo->Effects[0].TargetB = 0;
                    break;
                case 119875: //Templest
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(48);
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(48);
                    break;

                // Stormstout brewery
                case 112944: //Carrot Breath - set 4 second becaus rotate not work
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(35);
                    break;

                //Scholomance
                case 114062: //Gravity Flux
                    spellInfo->Effects[1].Effect = 0;
                    spellInfo->Effects[1].ApplyAuraName = 0;
                    break;
                case 113996: //Bone Armor
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 111628: //Shadow blaze dmg
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(44);
                    break;

                // Mogu'shan Vault
                // Stone Guards
                case 129428: //Dummy Searcher(cobalt mine)
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(29);
                    break;
                //
                // Feng
                case 116364: //Arcane Velocity
                case 116018: //Epicenter
                case 116157: //Lightning fists
                case 116374: //Lightning fists (trigger dmg)
                    spellInfo->CasterAuraSpell = 0;
                    break;
                case 116417: //Arcane Resonance
                    spellInfo->Effects[0].TargetA = 6;
                    spellInfo->Effects[0].TargetB = 0;
                    spellInfo->Effects[1].TargetA = 6;
                    spellInfo->Effects[1].TargetB = 0;
                    break;
                case 116040: //Epicenter(trigger dmg)
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(48);//60 yards
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(48);
                    spellInfo->Effects[2].RadiusEntry = sSpellRadiusStore.LookupEntry(48);
                    break;
                case 116365: //Arcane Velocrity (trigger dmg)
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(48);//60 yards
                    break;
                case 116434: //Arcane Resonance(trigger dmg)
                    spellInfo->Effects[0].TargetB = 30;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(13);
                    break;
                //
                //Elegon
                case 129724: //Energy tendrols (trigger spell - grip)
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_JUMP;
                    break;
                //
                case 116782:
                case 116803: //Titan Gase
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(8); //5yards
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(8); //5yards
                    break;
                case 118327: //Titan Gase
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(8); //5yards
                    break;
                case 116000:
                    spellInfo->AttributesEx3 |= SPELL_ATTR3_STACK_FOR_DIFF_CASTERS;
                    break;
                case 116161:
                    spellInfo->Effects[EFFECT_1].MiscValue = 2; // Set Phase to 2
                    spellInfo->Effects[EFFECT_3].Effect    = 0; // No need to summon
                    break;
                case 116272:
                    spellInfo->Effects[EFFECT_0].MiscValue = 2; // Set Phase to 2
                    break;
                case 116606:
                    spellInfo->Effects[EFFECT_0].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 118303:
                    spellInfo->Effects[EFFECT_0].TargetA = TARGET_UNIT_TARGET_ANY;
                    spellInfo->Effects[EFFECT_0].TargetB = 0;
                    spellInfo->Effects[EFFECT_0].ApplyAuraName = SPELL_AURA_DUMMY;
                    break;
                case 15850: // Chilled
                case 16927: // Chilled
                case 20005: // Chilled
                    spellInfo->Mechanic = MECHANIC_SNARE;
                    break;
                case 128405: // Narrow Escape
                    spellInfo->AuraInterruptFlags |= AURA_INTERRUPT_FLAG_TAKE_DAMAGE;
                    break;
                case 127802: // Touch of the Grave
                case 117050:
                    spellInfo->Speed = 25.f;
                    break;

                //World Boss
                //Sha of Anger
                case 119487: //Anger
                    spellInfo->Effects[0].TargetA = 22;
                    spellInfo->Effects[0].TargetB = 15;
                    spellInfo->Effects[0].RadiusEntry = sSpellRadiusStore.LookupEntry(30);
                    spellInfo->Effects[1].TargetA = 22;
                    spellInfo->Effects[1].TargetB = 15;
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(30);
                    break;
                case 119610: //Bitter thoughts
                    spellInfo->Effects[0].TargetB = 15;
                    break;
                case 119489: //Unleashed Wrath
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(10);
                    break;
                // Galion
                case 121787: //Stomp
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(12);
                    spellInfo->Effects[2].RadiusEntry = sSpellRadiusStore.LookupEntry(12);
                    break;
                case 121577: //Barrage
                    spellInfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(1);
                    break;
                case 121600: //Barrage Dmg
                    spellInfo->Effects[1].RadiusEntry = sSpellRadiusStore.LookupEntry(13);
                    spellInfo->Effects[2].RadiusEntry = sSpellRadiusStore.LookupEntry(13);
                    break;

                // Dalaran arena knockback
                case 61698:
                    spellInfo->Attributes = 536871296;
                    spellInfo->AttributesEx = 269058048;
                    spellInfo->AttributesEx2 = 67108868;
                    spellInfo->AttributesEx3 = 268894272;
                    spellInfo->AttributesEx4 = 2048;
                    spellInfo->AttributesEx6 = 1024;
                    spellInfo->CastTimeEntry = sSpellCastTimesStore.LookupEntry(1);
                    spellInfo->RangeEntry = sSpellRangeStore.LookupEntry(1);
                    spellInfo->EquippedItemClass = -1;
                    spellInfo->Effects[0].Effect = SPELL_EFFECT_DUMMY;
                    spellInfo->Effects[0].TargetA = SpellImplicitTargetInfo(25);
                    break;
                case 130649: // Life Spirit
                    spellInfo->Effects[0]. BasePoints = 10000;
                    break;
                case 130650: // Water Spirit
                    spellInfo->Effects[0]. BasePoints = 5000;
                    break;
                case 105709: // Master Mana Potion
                    spellInfo->Effects[0]. BasePoints = 30000;   
                    break;
                case 126349:
                case 126413:
                case 126549:
                case 126550:
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(26); // 240 seconds
                    break; 
                case 88764:// Rolling Thunder
                    spellInfo->Effects[0].TriggerSpell = 0;    
                    break;
                case 58423:
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_DUMMY;
                    spellInfo->Effects[0].SpellClassMask[2] = 0;
                    spellInfo->Effects[0].SpellClassMask[1] = 0;
                    spellInfo->Effects[0].SpellClassMask[0] = 0;
                    spellInfo->Effects[0].TriggerSpell = 0;
                    break;
                case 115191:
                    spellInfo->AuraInterruptFlags = 0x08020C00;
                    break;
                case 50371: // Improved Blood Presence disable not need efect
                    spellInfo->Effects[1].BasePoints = 0;
                    break;
                case 43265: //Death and Decay
                    spellInfo->Effects[1].BasePoints = 260;
                    spellInfo->Effects[1].TargetA = 28;
                    spellInfo->Effects[1].TargetB = 0;
                    spellInfo->Effects[1].Amplitude = 1100;
                    spellInfo->Effects[1].ApplyAuraName = SPELL_AURA_PERIODIC_DAMAGE;
                    break;
                case 11327: //Vanish
                    spellInfo->AttributesEx4 |= SPELL_ATTR4_TRIGGERED;
                    spellInfo->AuraInterruptFlags &= ~AURA_INTERRUPT_FLAG_TAKE_DAMAGE2;
                    break;
                case 8177: //totem
                    spellInfo->RecoveryTime = 25000;
                    spellInfo->CategoryRecoveryTime = 0;
                    break;
                case 45284: //Lightning Bolt
                case 117014: //Elemental Blast
                    spellInfo->SpellFamilyFlags[0] = 0x00000001;
                    spellInfo->SpellFamilyFlags[2] = 0;
                    break;
                case 117679:    // Incarnation (Passive)
                    spellInfo->Attributes &= ~SPELL_ATTR0_CANT_CANCEL;
                    break;
                case 55442: //Glyph of Capacitor Totem
                    spellInfo->Effects[0].SpellClassMask[0] = 0x00008000;
                    break;
                case 116186:    // Glyph of Prowl
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_ADD_FLAT_MODIFIER;
                    break;
                case 131537:    // Item - Druid PvP Set Feral 4P Bonus
                    spellInfo->Effects[0].ApplyAuraName = SPELL_AURA_PERIODIC_DUMMY;
                    spellInfo->Effects[0].Amplitude = 30000;
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(21); // -1s
                    break;
                case 132402:    // Savage Defense
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(32); // 6s
                    break;
                case 76724: // Offering
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(3); // 60s
                    break;
                case 122557: // The Challenger's Ring: Summon Snow Blossom Fighter
                case 122746: // The Challenger's Ring: Yalia Sagewhisper
                case 62772: // Summon Gorat's Spirit
                case 62814: // Summon Elendilad
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(5); // 300s
                    break;
                case 37062: // To Catch A Thistlehead
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(4); // 120s
                    break;
                case 76856: // Mastery: Unshackled Fury
                    spellInfo->CasterAuraState = 0;
                    break;
                case 114714:// Grilled Plainshawk Leg
                    spellInfo->Effects[0].TargetA = TARGET_UNIT_CASTER;
                    break;
                // Hack these until proc flags of CC auras are implemented
                case 339:       // Entangling Roots
                case 19975:     // Entangling Roots
                case 51514:     // Hex
                case 102359:    // Mass Entanglement
                case 104239:    // Horror (Soulburn)
                    spellInfo->AuraInterruptFlags &= ~AURA_INTERRUPT_FLAG_TAKE_DAMAGE2;
                    break;
                case 112965:    // Fingers of Frost
                    for (int i = 0; i < 3; ++i)
                    {
                        spellInfo->Effects[i].Effect = SPELL_EFFECT_APPLY_AURA;
                        spellInfo->Effects[i].ApplyAuraName = SPELL_AURA_DUMMY;
                        spellInfo->Effects[i].TargetA = TARGET_UNIT_CASTER;
                    }
                    break;
                case 122137: // Summon Ghosts from Urns - Summons
                    spellInfo->Effects[0].BasePoints = 5;
                    spellInfo->DurationEntry = sSpellDurationStore.LookupEntry(25); // 180s
                    break;
                case 30451: // Arcane Blast
                    // trigger spell is handled in script
                    spellInfo->Effects[1].Effect = 0;
                    break;
                case 74793:// Summoning Ritual
                    spellInfo->AreaGroupId = -1;
                    break;
                default:
                    break;
            }

            switch (spellInfo->SpellFamilyName)
            {
            case SPELLFAMILY_WARRIOR:
                // Shout
                if (spellInfo->SpellFamilyFlags[0] & 0x20000 || spellInfo->SpellFamilyFlags[1] & 0x20)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                break;
            case SPELLFAMILY_DRUID:
                // Roar
                if (spellInfo->SpellFamilyFlags[0] & 0x8)
                    spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CC;
                break;
            default:
                break;
            }

            // This must be re-done if targets changed since the spellinfo load
            spellInfo->ExplicitTargetMask = spellInfo->_GetExplicitTargetMask();

            switch (spellInfo->Id)
            {
                case 73680: // Unleash Elements
                    spellInfo->ExplicitTargetMask |= TARGET_FLAG_UNIT_ALLY;
                    spellInfo->ExplicitTargetMask |= TARGET_FLAG_UNIT_ENEMY;
                    break;
                case 107223:
                    spellInfo->ExplicitTargetMask = TARGET_FLAG_UNIT_MASK;
                   break;
                case 106736:
                    spellInfo->ExplicitTargetMask = TARGET_FLAG_UNIT_MASK;
                    break;
                case 106112:
                    spellInfo->ExplicitTargetMask |= TARGET_FLAG_DEST_LOCATION;
                    break;
                case 106113:
                    spellInfo->ExplicitTargetMask = TARGET_FLAG_UNIT_MASK;
                    break;
            }
        }
    }

    CreatureAI::FillAISpellInfo();

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded spell custom attributes in %u ms", GetMSTimeDiffToNow(oldMSTime));

    oldMSTime = getMSTime();
    //                                                   0            1             2              3              4              5
    QueryResult result = WorldDatabase.Query("SELECT spell_id, effectradius0, effectradius1, effectradius2, effectradius3, effectradius4 from spell_radius");

    if (!result)
    {
        sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded 0 spell effect radius records. DB table `spell_radius` is empty.");
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();

        uint32 spell_id = fields[0].GetUInt32();
        uint32 effectradius0 = fields[1].GetUInt32();
        uint32 effectradius1 = fields[2].GetUInt32();
        uint32 effectradius2 = fields[3].GetUInt32();
        uint32 effectradius3 = fields[4].GetUInt32();
        uint32 effectradius4 = fields[5].GetUInt32();

        // check if valid radius
        SpellInfo* spellInfo = mSpellInfoMap[spell_id];
        if (!spellInfo)
        {
            sLog->outError(LOG_FILTER_SQL, "spell_id %u in `spell_radius` table is not found in dbcs, skipped", spell_id);
            continue;
        }

        if (effectradius0 && sSpellRadiusStore.LookupEntry(effectradius0))
        {
            spellInfo->Effects[EFFECT_0].RadiusEntry = sSpellRadiusStore.LookupEntry(effectradius0);
        }

        if (effectradius1 && sSpellRadiusStore.LookupEntry(effectradius1))
        {
            spellInfo->Effects[EFFECT_1].RadiusEntry = sSpellRadiusStore.LookupEntry(effectradius1);
        }

        if (effectradius2 && sSpellRadiusStore.LookupEntry(effectradius2))
        {
            spellInfo->Effects[EFFECT_2].RadiusEntry = sSpellRadiusStore.LookupEntry(effectradius2);
        }

        if (effectradius3 && sSpellRadiusStore.LookupEntry(effectradius3))
        {
            spellInfo->Effects[EFFECT_3].RadiusEntry = sSpellRadiusStore.LookupEntry(effectradius3);
        }

        if (effectradius4 && sSpellRadiusStore.LookupEntry(effectradius4))
        {
            spellInfo->Effects[EFFECT_4].RadiusEntry = sSpellRadiusStore.LookupEntry(effectradius4);
        }

        ++count;
    } while (result->NextRow());

    sLog->outInfo(LOG_FILTER_SERVER_LOADING, ">> Loaded %u spell effect radius records in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void SpellMgr::LoadTalentSpellInfo()
{
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); i++)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(i);
        if (!talent)
            continue;

        mTalentSpellInfo.insert(talent->spellId);
    }
}

void SpellMgr::LoadSpellPowerInfo()
{
    mSpellPowerInfo.resize(sSpellStore.GetNumRows());
    for (uint32 i = 0; i < sSpellPowerStore.GetNumRows(); i++)
    {
        SpellPowerEntry const* spellPower = sSpellPowerStore.LookupEntry(i);
        if (!spellPower)
            continue;

        mSpellPowerInfo[spellPower->SpellId].push_back(spellPower->Id);
    }
}

SpellPowerEntry const* SpellMgr::GetSpellPowerEntryByIdAndPower(uint32 id, Powers power) const
{
    for (std::list<uint32>::iterator itr = GetSpellPowerList(id).begin(); itr != GetSpellPowerList(id).end(); ++itr)
    {
        SpellPowerEntry const* spellPower = sSpellPowerStore.LookupEntry(*itr);
        if(!spellPower)
            continue;

        if(spellPower->powerType == power)
            return spellPower;
    }

    SpellInfo const* spell = sSpellMgr->GetSpellInfo(id);
    return spell->spellPower;
}
