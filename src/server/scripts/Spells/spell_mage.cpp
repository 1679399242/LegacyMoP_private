/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
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

/*
 * Scripts for spells with SPELLFAMILY_MAGE and SPELLFAMILY_GENERIC spells used by mage players.
 * Ordered alphabetically using scriptname.
 * Scriptnames of files in this file should be prefixed with "spell_mage_".
 */

#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "ScriptedCreature.h"

#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"

enum MageSpells
{
    SPELL_MAGE_COLD_SNAP                         = 11958,
    SPELL_MAGE_SQUIRREL_FORM                     = 32813,
    SPELL_MAGE_GIRAFFE_FORM                      = 32816,
    SPELL_MAGE_SERPENT_FORM                      = 32817,
    SPELL_MAGE_DRAGONHAWK_FORM                   = 32818,
    SPELL_MAGE_WORGEN_FORM                       = 32819,
    SPELL_MAGE_SHEEP_FORM                        = 32820,
    SPELL_MAGE_GLYPH_OF_ETERNAL_WATER            = 70937,
    SPELL_MAGE_SUMMON_WATER_ELEMENTAL_PERMANENT  = 70908,
    SPELL_MAGE_SUMMON_WATER_ELEMENTAL_TEMPORARY  = 70907,
    SPELL_MAGE_GLYPH_OF_BLAST_WAVE               = 62126,
    SPELL_MAGE_ALTER_TIME_OVERRIDED              = 127140,
    SPELL_MAGE_ALTER_TIME                        = 110909,
    SPELL_MAGE_TEMPORAL_DISPLACEMENT             = 80354,
    HUNTER_SPELL_INSANITY                        = 95809,
    SPELL_SHAMAN_SATED                           = 57724,
    SPELL_SHAMAN_EXHAUSTED                       = 57723,
    SPELL_MAGE_CONJURE_REFRESHMENT_R1            = 92739,
    SPELL_MAGE_CONJURE_REFRESHMENT_R2            = 92799,
    SPELL_MAGE_CONJURE_REFRESHMENT_R3            = 92802,
    SPELL_MAGE_CONJURE_REFRESHMENT_R4            = 92805,
    SPELL_MAGE_CONJURE_REFRESHMENT_R5            = 74625,
    SPELL_MAGE_CONJURE_REFRESHMENT_R6            = 42956,
    SPELL_MAGE_CONJURE_REFRESHMENT_R7            = 92727,
    SPELL_MAGE_CONJURE_REFRESHMENT_R8            = 116130,
    SPELL_MAGE_MANA_GEM_ENERGIZE                 = 10052,
    SPELL_MAGE_ARCANE_BRILLIANCE                 = 1459,
    SPELL_MAGE_INFERNO_BLAST                     = 108853,
    SPELL_MAGE_INFERNO_BLAST_IMPACT              = 118280,
    SPELL_MAGE_IGNITE                            = 12654,
    SPELL_MAGE_PYROBLAST                         = 11366,
    SPELL_MAGE_COMBUSTION_DOT                    = 83853,
    SPELL_MAGE_COMBUSTION_IMPACT                 = 118271,
    SPELL_MAGE_FROSTJAW                          = 102051,
    SPELL_MAGE_NETHER_TEMPEST_DIRECT_DAMAGE      = 114954,
    SPELL_MAGE_NETHER_TEMPEST_VISUAL             = 114924,
    SPELL_MAGE_NETHER_TEMPEST_MISSILE            = 114956,
    SPELL_MAGE_LIVING_BOMB_TRIGGERED             = 44461,
    SPELL_MAGE_FROST_BOMB_TRIGGERED              = 113092,
    SPELL_MAGE_INVOKERS_ENERGY                   = 116257,
    SPELL_MAGE_INVOCATION                        = 114003,
    SPELL_MAGE_GLYPH_OF_EVOCATION                = 56380,
    SPELL_MAGE_BRAIN_FREEZE                      = 44549,
    SPELL_MAGE_BRAIN_FREEZE_TRIGGERED            = 57761,
    SPELL_MAGE_SLOW                              = 31589,
    SPELL_MAGE_ARCANE_CHARGE                     = 36032,
    SPELL_MAGE_ARCANE_BARRAGE_TRIGGERED          = 50273,
    SPELL_MAGE_PYROMANIAC_AURA                   = 132209,
    SPELL_MAGE_PYROMANIAC_DAMAGE_DONE            = 132210,
    SPELL_MAGE_MIRROR_IMAGE_SUMMON               = 58832,
    SPELL_MAGE_CAUTERIZE                         = 87023,
    SPELL_MAGE_ARCANE_MISSILES                   = 79683,
    SPELL_MAGE_INCANTERS_WARD_ENERGIZE           = 113842,
    SPELL_MAGE_INCANTERS_ABSORBTION              = 116267,
    SPELL_MAGE_INCANTERS_ABSORBTION_PASSIVE      = 118858,
};

// Incanter's Ward (Cooldown marker) - 118859
class spell_mage_incanters_ward_cooldown : public SpellScriptLoader
{
    public:
        spell_mage_incanters_ward_cooldown() : SpellScriptLoader("spell_mage_incanters_ward_cooldown") { }

        class spell_mage_incanters_ward_cooldown_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_incanters_ward_cooldown_AuraScript);

            void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* caster = GetCaster())
                    caster->RemoveAura(SPELL_MAGE_INCANTERS_ABSORBTION_PASSIVE);
            }

            void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* caster = GetCaster())
                    if (!caster->HasAura(SPELL_MAGE_INCANTERS_ABSORBTION_PASSIVE))
                        caster->CastSpell(caster, SPELL_MAGE_INCANTERS_ABSORBTION_PASSIVE, true);
            }

            void Register()
            {
                OnEffectApply += AuraEffectApplyFn(spell_mage_incanters_ward_cooldown_AuraScript::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
                OnEffectRemove += AuraEffectRemoveFn(spell_mage_incanters_ward_cooldown_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_incanters_ward_cooldown_AuraScript();
        }
};

// Incanter's Ward - 1463
class spell_mage_incanters_ward : public SpellScriptLoader
{
    public:
        spell_mage_incanters_ward() : SpellScriptLoader("spell_mage_incanters_ward") { }

        class spell_mage_incanters_ward_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_incanters_ward_AuraScript);

            float absorbTotal;
            float absorbtionAmount;

            bool Load()
            {
                absorbTotal = 0.0f;
                absorbtionAmount = 0.0f;
                return true;
            }

            void CalculateAmount(AuraEffect const* , int32 & amount, bool & )
            {
                if (Unit* caster = GetCaster())
                    amount += caster->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_ARCANE);

                absorbtionAmount = float(amount);
            }

            void OnAbsorb(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount)
            {
                if (Unit* caster = dmgInfo.GetVictim())
                {
                    if (Unit* attacker = dmgInfo.GetAttacker())
                    {
                        absorbTotal += float(dmgInfo.GetDamage());

                        int32 pct = aurEff->GetSpellInfo()->Effects[EFFECT_1].CalcValue(GetCaster());
                        int32 manaGain = CalculatePct(caster->GetMaxPower(POWER_MANA), CalculatePct(((float(dmgInfo.GetDamage()) / absorbtionAmount) * 100.0f), pct));

                        caster->EnergizeBySpell(caster, SPELL_MAGE_INCANTERS_WARD_ENERGIZE, manaGain, POWER_MANA);
                    }
                }
            }

            void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Unit* caster = GetCaster())
                {
                    if(absorbTotal > absorbtionAmount)
                        absorbTotal = absorbtionAmount;      
                        
                    int32 damageGain = CalculatePct(sSpellMgr->GetSpellInfo(SPELL_MAGE_INCANTERS_ABSORBTION)->Effects[0].BasePoints, ((absorbTotal / absorbtionAmount) * 100.0f));
                    if (!damageGain)
                        return;

                    caster->CastCustomSpell(caster, SPELL_MAGE_INCANTERS_ABSORBTION, &damageGain, NULL, NULL, true);
                }
            }

            void Register()
            {
                DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_mage_incanters_ward_AuraScript::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
                OnEffectAbsorb += AuraEffectAbsorbFn(spell_mage_incanters_ward_AuraScript::OnAbsorb, EFFECT_0);
                OnEffectRemove += AuraEffectRemoveFn(spell_mage_incanters_ward_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_incanters_ward_AuraScript();
        }
};

// Arcane Missiles - 5143
class spell_mage_arcane_missile : public SpellScriptLoader
{
    public:
        spell_mage_arcane_missile() : SpellScriptLoader("spell_mage_arcane_missile") { }

        class spell_mage_arcane_missile_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_arcane_missile_AuraScript);

            void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (!GetCaster())
                    return;

                if (Player* _player = GetCaster()->ToPlayer())
                    if (Aura* arcaneMissiles = _player->GetAura(SPELL_MAGE_ARCANE_MISSILES))
                        arcaneMissiles->DropCharge();
            }

            void Register()
            {
                OnEffectApply += AuraEffectApplyFn(spell_mage_arcane_missile_AuraScript::OnApply, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL, AURA_EFFECT_HANDLE_REAL);
            }

        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_arcane_missile_AuraScript();
        }
};

// Cauterize - 86949
class spell_mage_cauterize : public SpellScriptLoader
{
    public:
        spell_mage_cauterize() : SpellScriptLoader("spell_mage_cauterize") { }

        class spell_mage_cauterize_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_cauterize_AuraScript);

            uint32 absorbChance;
            uint32 healtPct;

            bool Load()
            {
                absorbChance = GetSpellInfo()->Effects[EFFECT_0].CalcValue(GetCaster());
                healtPct = GetSpellInfo()->Effects[EFFECT_1].CalcValue(GetCaster());
                return GetUnitOwner()->ToPlayer();
            }

            void CalculateAmount(AuraEffect const* /*AuraEffect**/, int32& amount, bool& /*canBeRecalculated*/)
            {
                amount = -1;
            }

            void Absorb(AuraEffect* /*AuraEffect**/, DamageInfo& dmgInfo, uint32& absorbAmount)
            {
                Unit* target = GetTarget();

                if (dmgInfo.GetDamage() < target->GetHealth())
                    return;

                if (target->ToPlayer()->HasSpellCooldown(SPELL_MAGE_CAUTERIZE))
                    return;

                if (!roll_chance_i(absorbChance))
                    return;

                int bp1 = target->CountPctFromMaxHealth(healtPct);
                target->CastCustomSpell(target, SPELL_MAGE_CAUTERIZE, NULL, &bp1, NULL, true);
                target->ToPlayer()->AddSpellCooldown(SPELL_MAGE_CAUTERIZE, 0, time(NULL) + 60);

                absorbAmount = dmgInfo.GetDamage();
            }

            void Register()
            {
                DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_mage_cauterize_AuraScript::CalculateAmount, EFFECT_0, SPELL_AURA_SCHOOL_ABSORB);
                OnEffectAbsorb += AuraEffectAbsorbFn(spell_mage_cauterize_AuraScript::Absorb, EFFECT_0);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_cauterize_AuraScript();
        }
};

// Mirror Images - 55342
class spell_mage_mirror_images : public SpellScriptLoader
{
    public:
        spell_mage_mirror_images() : SpellScriptLoader("spell_mage_mirror_images") { }

        class spell_mage_mirror_images_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_mirror_images_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    _player->CastSpell(_player, SPELL_MAGE_MIRROR_IMAGE_SUMMON, true);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_mirror_images_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_mirror_images_SpellScript();
        }
};

// Called by Nether Tempest - 114923, Frost Bomb - 112948 and Living Bomb - 44457
// Pyromaniac - 132209
class spell_mage_pyromaniac : public SpellScriptLoader
{
    public:
        spell_mage_pyromaniac() : SpellScriptLoader("spell_mage_pyromaniac") { }

        class spell_mage_pyromaniac_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_pyromaniac_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    if (Unit* target = GetHitUnit())
                        if (_player->HasAura(SPELL_MAGE_PYROMANIAC_AURA))
                            _player->CastSpell(target, SPELL_MAGE_PYROMANIAC_DAMAGE_DONE, true);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_pyromaniac_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_pyromaniac_SpellScript();
        }
};

class CheckArcaneBarrageImpactPredicate
{
    public:
        CheckArcaneBarrageImpactPredicate(Unit* caster, Unit* mainTarget) : _caster(caster), _mainTarget(mainTarget) {}

        bool operator()(Unit* target)
        {
            if (!_caster || !_mainTarget)
                return true;

            if (!_caster->IsValidAttackTarget(target))
                return true;

            if (!target->IsWithinLOSInMap(_caster))
                return true;

            if (!_caster->isInFront(target))
                return true;

            if (target->GetGUID() == _caster->GetGUID())
                return true;

            if (target->GetGUID() == _mainTarget->GetGUID())
                return true;

            return false;
        }

    private:
        Unit* _caster;
        Unit* _mainTarget;
};

// Arcane Barrage - 44425
class spell_mage_arcane_barrage : public SpellScriptLoader
{
    public:
        spell_mage_arcane_barrage() : SpellScriptLoader("spell_mage_arcane_barrage") { }

        class spell_mage_arcane_barrage_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_arcane_barrage_SpellScript);

            void HandleAfterHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        uint8 chargeCount = 0;
                        int32 bp = 0;

                        if (Aura* arcaneCharge = _player->GetAura(SPELL_MAGE_ARCANE_CHARGE))
                        {
                            chargeCount = arcaneCharge->GetStackAmount();
                            _player->RemoveAura(arcaneCharge);
                        }

                        if (chargeCount)
                        {
                            bp = GetHitDamage() / 2;

                            std::list<Unit*> targetList;

                            target->GetAttackableUnitListInRange(targetList, 10.0f);
                            targetList.remove_if(CheckArcaneBarrageImpactPredicate(_player, target));

                            Trinity::Containers::RandomResizeList(targetList, chargeCount);

                            for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
                                target->CastCustomSpell(*itr, SPELL_MAGE_ARCANE_BARRAGE_TRIGGERED, &bp, NULL, NULL, true, 0, NULL, _player->GetGUID());
                        }
                    }
                }
            }

            void Register()
            {
                AfterHit += SpellHitFn(spell_mage_arcane_barrage_SpellScript::HandleAfterHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_arcane_barrage_SpellScript();
        }
};

// Arcane Explosion - 1449
class spell_mage_arcane_explosion : public SpellScriptLoader
{
    public:
        spell_mage_arcane_explosion() : SpellScriptLoader("spell_mage_arcane_explosion") { }

        class spell_mage_arcane_explosion_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_arcane_explosion_SpellScript);

            bool casted;

        public:
            spell_mage_arcane_explosion_SpellScript() : SpellScript()
            {
                casted = false;
            }

            void HandleOnHit()
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                Player* player = caster->ToPlayer();
                if (!player)
                    return;

                if (player->GetSpecializationId(player->GetActiveSpec()) != SPEC_MAGE_ARCANE)
                    return;

                if (!casted)
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (roll_chance_i(30))
                        {
                            casted = true;
                            caster->CastSpell(caster, SPELL_MAGE_ARCANE_CHARGE, true);
                        }
                    }
                }

                if (!casted)
                {
                    if (Unit* target = GetHitUnit())
                        if (Aura* arcaneCharge = caster->GetAura(SPELL_MAGE_ARCANE_CHARGE))
                            arcaneCharge->RefreshDuration();
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_arcane_explosion_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_arcane_explosion_SpellScript();
        }
};

//Arcane Missiles - 7268
class spell_mage_arcane_missiles : public SpellScriptLoader
{
    public:
        spell_mage_arcane_missiles() : SpellScriptLoader("spell_mage_arcane_missiles") { }

        class spell_mage_arcane_missiles_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_arcane_missiles_SpellScript);

            void HandleOnHit()
            {
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                Player* player = caster->ToPlayer();
                if (!player)
                    return;

                if (player->GetSpecializationId(player->GetActiveSpec()) != SPEC_MAGE_ARCANE)
                    return;

                caster->CastSpell(caster, SPELL_MAGE_ARCANE_CHARGE, true);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_arcane_missiles_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_arcane_missiles_SpellScript();
        }
};

// Slow - 31589
class spell_mage_slow : public SpellScriptLoader
{
    public:
        spell_mage_slow() : SpellScriptLoader("spell_mage_slow") { }

        class spell_mage_slow_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_slow_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (target->GetTypeId() == TYPEID_PLAYER)
                        {
                            if (Aura* frostjaw = target->GetAura(SPELL_MAGE_SLOW, _player->GetGUID()))
                            {
                                // Only half time against players
                                frostjaw->SetDuration(frostjaw->GetMaxDuration() / 2);
                                frostjaw->SetMaxDuration(frostjaw->GetDuration());
                            }
                        }
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_slow_SpellScript::HandleOnHit);
            }
        };

        class spell_mage_slow_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_slow_AuraScript);

            void CalculateAmount(AuraEffect const* /*aurEff*/, int32 & amount, bool & /*canBeRecalculated*/)
            {
                if(Unit* caster = GetCaster())
                    if(Player* player = caster->ToPlayer())
                        if (player->GetSelectedPlayer())
                            amount /= 2;
            }

            void Register()
            {
                DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_mage_slow_AuraScript::CalculateAmount, EFFECT_1, SPELL_AURA_HASTE_SPELLS);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_slow_AuraScript();
        }

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_slow_SpellScript();
        }
};

// Frostbolt - 116
class spell_mage_frostbolt : public SpellScriptLoader
{
    public:
        spell_mage_frostbolt() : SpellScriptLoader("spell_mage_frostbolt") { }

        class spell_mage_frostbolt_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_frostbolt_SpellScript);

            SpellCastResult CheckTarget()
            {
                if (!GetExplTargetUnit())
                    return SPELL_FAILED_NO_VALID_TARGETS;
                else if (GetExplTargetUnit()->GetGUID() == GetCaster()->GetGUID())
                    return SPELL_FAILED_BAD_TARGETS;

                return SPELL_CAST_OK;
            }

            void Register()
            {
                OnCheckCast += SpellCheckCastFn(spell_mage_frostbolt_SpellScript::CheckTarget);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_frostbolt_SpellScript();
        }
};

// Called by Evocation - 12051
// Invocation - 114003
class spell_mage_invocation : public SpellScriptLoader
{
    public:
        spell_mage_invocation() : SpellScriptLoader("spell_mage_invocation") { }

        class spell_mage_invocation_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_invocation_AuraScript);

            void AfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
            {
                AuraRemoveMode removeMode = GetTargetApplication()->GetRemoveMode();
                if (removeMode != AURA_REMOVE_BY_EXPIRE)
                    return;

                if (Unit* caster = GetCaster())
                {
                    if (caster->HasAura(SPELL_MAGE_INVOCATION))
                    {
                        caster->CastSpell(caster, SPELL_MAGE_INVOKERS_ENERGY, true);

                        if (caster->HasAura(SPELL_MAGE_GLYPH_OF_EVOCATION))
                            caster->HealBySpell(caster, sSpellMgr->GetSpellInfo(12051), caster->CountPctFromMaxHealth(40), false);
                    }
                }
            }

            void Register()
            {
                AfterEffectRemove += AuraEffectRemoveFn(spell_mage_invocation_AuraScript::AfterRemove, EFFECT_0, SPELL_AURA_OBS_MOD_POWER, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_invocation_AuraScript();
        }
};

// Frost Bomb - 112948
class spell_mage_frost_bomb : public SpellScriptLoader
{
    public:
        spell_mage_frost_bomb() : SpellScriptLoader("spell_mage_frost_bomb") { }

        class spell_mage_frost_bomb_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_frost_bomb_AuraScript);

            void AfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
            {
                AuraRemoveMode removeMode = GetTargetApplication()->GetRemoveMode();
                if (removeMode != AURA_REMOVE_BY_EXPIRE)
                    return;

                if (Unit* caster = GetCaster())
                {
                    caster->CastSpell(GetTarget(), SPELL_MAGE_FROST_BOMB_TRIGGERED, true);

                    if (caster->HasAura(SPELL_MAGE_BRAIN_FREEZE))
                        caster->CastSpell(caster, SPELL_MAGE_BRAIN_FREEZE_TRIGGERED, true);
                }
            }

            void Register()
            {
                AfterEffectRemove += AuraEffectRemoveFn(spell_mage_frost_bomb_AuraScript::AfterRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_frost_bomb_AuraScript();
        }
};

class CheckNetherImpactPredicate
{
    public:
        CheckNetherImpactPredicate(Unit* caster, Unit* mainTarget) : _caster(caster), _mainTarget(mainTarget) {}

        bool operator()(Unit* target)
        {
            if (!_caster || !_mainTarget)
                return true;

            if (!_caster->IsValidAttackTarget(target))
                return true;

            if (!target->IsWithinLOSInMap(_caster))
                return true;

            if (!_caster->isInFront(target))
                return true;

            if (target->GetGUID() == _caster->GetGUID())
                return true;

            if (target->GetGUID() == _mainTarget->GetGUID())
                return true;

            return false;
        }

    private:
        Unit* _caster;
        Unit* _mainTarget;
};

// Nether Tempest - 114923
class spell_mage_nether_tempest : public SpellScriptLoader
{
    public:
        spell_mage_nether_tempest() : SpellScriptLoader("spell_mage_nether_tempest") { }

        class spell_mage_nether_tempest_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_nether_tempest_AuraScript);

            void OnTick(AuraEffect const* aurEff)
            {
                if (GetCaster())
                {
                    if (Player* _player = GetCaster()->ToPlayer())
                    {
                        std::list<Unit*> targetList;

                        GetTarget()->GetAttackableUnitListInRange(targetList, 10.0f);
                        targetList.remove_if(CheckNetherImpactPredicate(_player, GetTarget()));

                        Trinity::Containers::RandomResizeList(targetList, 1);

                        for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
                        {
                            GetCaster()->CastSpell(*itr, SPELL_MAGE_NETHER_TEMPEST_DIRECT_DAMAGE, true);
                            GetCaster()->CastSpell(*itr, SPELL_MAGE_NETHER_TEMPEST_VISUAL, true);
                            GetTarget()->CastSpell(*itr, SPELL_MAGE_NETHER_TEMPEST_MISSILE, true);
                        }

                        if (GetCaster()->HasAura(SPELL_MAGE_BRAIN_FREEZE))
                            if (roll_chance_i(10))
                                GetCaster()->CastSpell(GetCaster(), SPELL_MAGE_BRAIN_FREEZE_TRIGGERED, true);
                    }
                }
            }

            void Register()
            {
                OnEffectPeriodic += AuraEffectPeriodicFn(spell_mage_nether_tempest_AuraScript::OnTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_nether_tempest_AuraScript();
        }
};

// Blazing Speed - 108843
class spell_mage_blazing_speed : public SpellScriptLoader
{
    public:
        spell_mage_blazing_speed() : SpellScriptLoader("spell_mage_blazing_speed") { }

        class spell_mage_blazing_speed_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_blazing_speed_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    _player->RemoveMovementImpairingAuras();
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_blazing_speed_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_blazing_speed_SpellScript();
        }
};

// Frostjaw - 102051
class spell_mage_frostjaw : public SpellScriptLoader
{
    public:
        spell_mage_frostjaw() : SpellScriptLoader("spell_mage_frostjaw") { }

        class spell_mage_frostjaw_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_frostjaw_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        if (target->GetTypeId() == TYPEID_PLAYER)
                        {
                            if (Aura* frostjaw = target->GetAura(SPELL_MAGE_FROSTJAW, _player->GetGUID()))
                            {
                                // Only half time against players
                                frostjaw->SetDuration(frostjaw->GetMaxDuration() / 2);
                                frostjaw->SetMaxDuration(frostjaw->GetDuration());
                            }
                        }
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_frostjaw_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_frostjaw_SpellScript();
        }
};

// Combustion - 11129
class spell_mage_combustion : public SpellScriptLoader
{
    public:
        spell_mage_combustion() : SpellScriptLoader("spell_mage_combustion") { }

        class spell_mage_combustion_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_combustion_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        _player->CastSpell(target, SPELL_MAGE_COMBUSTION_IMPACT, true);

                        if (_player->HasSpellCooldown(SPELL_MAGE_INFERNO_BLAST))
                            _player->RemoveSpellCooldown(SPELL_MAGE_INFERNO_BLAST, true);

                        int32 combustionBp = 0;

                        Unit::AuraEffectList const& aurasPereodic = target->GetAuraEffectsByType(SPELL_AURA_PERIODIC_DAMAGE);
                        for (Unit::AuraEffectList::const_iterator i = aurasPereodic.begin(); i !=  aurasPereodic.end(); ++i)
                        {
                            if ((*i)->GetCasterGUID() != _player->GetGUID() || (*i)->GetSpellInfo()->SchoolMask != SPELL_SCHOOL_MASK_FIRE)
                                continue;

                            if (!(*i)->GetAmplitude())
                                continue;

                            combustionBp += _player->SpellDamageBonusDone(target, (*i)->GetSpellInfo(), (*i)->GetAmount(), DOT) * 1000 / (*i)->GetAmplitude();
                        }

                        if (combustionBp)
                            _player->CastCustomSpell(target, SPELL_MAGE_COMBUSTION_DOT, &combustionBp, NULL, NULL, true);
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_combustion_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_combustion_SpellScript();
        }
};

class CheckInfernoBlastImpactPredicate
{
    public:
        CheckInfernoBlastImpactPredicate(Unit* caster, Unit* mainTarget) : _caster(caster), _mainTarget(mainTarget) {}

        bool operator()(Unit* target)
        {
            if (!_caster || !_mainTarget)
                return true;

            if (!_caster->IsValidAttackTarget(target))
                return true;

            if (!target->IsWithinLOSInMap(_caster))
                return true;

            if (!_caster->isInFront(target))
                return true;

            if (target->GetGUID() == _caster->GetGUID())
                return true;

            if (target->GetGUID() == _mainTarget->GetGUID())
                return true;

            return false;
        }

    private:
        Unit* _caster;
        Unit* _mainTarget;
};

// Inferno Blast - 108853
class spell_mage_inferno_blast : public SpellScriptLoader
{
    public:
        spell_mage_inferno_blast() : SpellScriptLoader("spell_mage_inferno_blast") { }

        class spell_mage_inferno_blast_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_inferno_blast_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (Unit* target = GetHitUnit())
                    {
                        std::list<Unit*> targetList;
                        int32 combustionBp = 0;

                        _player->CastSpell(target, SPELL_MAGE_INFERNO_BLAST_IMPACT, true);

                        // Spreads any Pyroblast, Ignite, and Combustion effects to up to 2 nearby enemy targets within 10 yards

                        target->GetAttackableUnitListInRange(targetList, 10.0f);

                        targetList.remove_if(CheckInfernoBlastImpactPredicate(_player, target));

                        if (targetList.size() > 2)
                            Trinity::Containers::RandomResizeList(targetList, 2);

                        for (std::list<Unit*>::const_iterator itr = targetList.begin(); itr != targetList.end(); ++itr)
                        {
                            Unit* unit = *itr;
                            // 1 : Ignite
                            if (target->HasAura(SPELL_MAGE_IGNITE, _player->GetGUID()))
                            {
                                float value = _player->GetFloatValue(PLAYER_MASTERY) * 1.5f / 100.0f;

                                int32 igniteBp = 0;

                                if (unit->HasAura(SPELL_MAGE_IGNITE, _player->GetGUID()))
                                    igniteBp += unit->GetRemainingPeriodicAmount(_player->GetGUID(), SPELL_MAGE_IGNITE, SPELL_AURA_PERIODIC_DAMAGE);

                                igniteBp += int32(GetHitDamage() * value / 2);

                                _player->CastCustomSpell(unit, SPELL_MAGE_IGNITE, &igniteBp, NULL, NULL, true);
                            }

                            // 2 : Pyroblast
                            if (target->HasAura(SPELL_MAGE_PYROBLAST, _player->GetGUID()))
                                _player->AddAura(SPELL_MAGE_PYROBLAST, unit);

                            // 3 : Combustion
                            if (target->HasAura(SPELL_MAGE_COMBUSTION_DOT, _player->GetGUID()))
                            {
                                if (unit->HasAura(SPELL_MAGE_PYROBLAST, _player->GetGUID()))
                                {
                                    combustionBp += _player->CalculateSpellDamage(target, sSpellMgr->GetSpellInfo(SPELL_MAGE_PYROBLAST), 1);
                                    combustionBp = _player->SpellDamageBonusDone(target, sSpellMgr->GetSpellInfo(SPELL_MAGE_PYROBLAST), combustionBp, DOT);
                                }
                                if (unit->HasAura(SPELL_MAGE_IGNITE, _player->GetGUID()))
                                    combustionBp += unit->GetRemainingPeriodicAmount(_player->GetGUID(), SPELL_MAGE_IGNITE, SPELL_AURA_PERIODIC_DAMAGE);

                                if (combustionBp)
                                    _player->CastCustomSpell(unit, SPELL_MAGE_COMBUSTION_DOT, &combustionBp, NULL, NULL, true);
                            }
                        }
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_inferno_blast_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_inferno_blast_SpellScript();
        }
};

// Arcane Brillance - 1459
class spell_mage_arcane_brilliance : public SpellScriptLoader
{
    public:
        spell_mage_arcane_brilliance() : SpellScriptLoader("spell_mage_arcane_brilliance") { }

        class spell_mage_arcane_brilliance_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_arcane_brilliance_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    _player->AddAura(SPELL_MAGE_ARCANE_BRILLIANCE, _player);

                    std::list<Unit*> memberList;
                    _player->GetPartyMembers(memberList);

                    for (std::list<Unit*>::const_iterator itr = memberList.begin(); itr != memberList.end(); ++itr)
                        _player->AddAura(SPELL_MAGE_ARCANE_BRILLIANCE, *itr);
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_arcane_brilliance_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_arcane_brilliance_SpellScript();
        }
};

// Replenish Mana - 5405
class spell_mage_replenish_mana : public SpellScriptLoader
{
    public:
        spell_mage_replenish_mana() : SpellScriptLoader("spell_mage_replenish_mana") { }

        class spell_mage_replenish_mana_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_replenish_mana_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    _player->CastSpell(_player, 10052, true);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_replenish_mana_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_replenish_mana_SpellScript();
        }
};

// Evocation - 12051
class spell_mage_evocation : public SpellScriptLoader
{
    public:
        spell_mage_evocation() : SpellScriptLoader("spell_mage_evocation") { }

        class spell_mage_evocation_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_evocation_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    _player->EnergizeBySpell(_player, GetSpellInfo()->Id, int32(_player->GetMaxPower(POWER_MANA) * 0.15), POWER_MANA);
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_evocation_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_evocation_SpellScript();
        }
};

// Conjure Refreshment - 42955
class spell_mage_conjure_refreshment : public SpellScriptLoader
{
    public:
        spell_mage_conjure_refreshment() : SpellScriptLoader("spell_mage_conjure_refreshment") { }

        class spell_mage_conjure_refreshment_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_conjure_refreshment_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (_player->getLevel() < 44)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R1, true);
                    else if (_player->getLevel() < 54)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R2, true);
                    else if (_player->getLevel() < 64)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R3, true);
                    else if (_player->getLevel() < 74)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R4, true);
                    else if (_player->getLevel() < 80)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R5, true);
                    else if (_player->getLevel() < 85)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R6, true);
                    else if (_player->getLevel() < 90)
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R7, true);
                    else
                        _player->CastSpell(_player, SPELL_MAGE_CONJURE_REFRESHMENT_R8, true);
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_conjure_refreshment_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_conjure_refreshment_SpellScript();
        }
};

// Conjure Refreshment Table - 43987
class spell_mage_conjure_refreshment_table : public SpellScriptLoader
{
    public:
        spell_mage_conjure_refreshment_table() : SpellScriptLoader("spell_mage_conjure_refreshment_table") { }

        class spell_mage_conjure_refreshment_table_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_conjure_refreshment_table_SpellScript);

            void HandleOnHit()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                {
                    if (_player->getLevel() < 80)
                        _player->CastSpell(_player, 74650, true);   // Conjure Refreshment Table (Rank 1)
                    else if (_player->getLevel() < 85)
                        _player->CastSpell(_player, 92824, true);   // Conjure Refreshment Table (Rank 2)
                    else if (_player->getLevel() < 90)
                        _player->CastSpell(_player, 92827, true);   // Conjure Refreshment Table (Rank 3)
                    else
                        _player->CastSpell(_player, 116133, true);  // Conjure Refreshment Table (Rank 3)
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_conjure_refreshment_table_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_conjure_refreshment_table_SpellScript();
        }
};

// Time Warp - 80353
class spell_mage_time_warp : public SpellScriptLoader
{
    public:
        spell_mage_time_warp() : SpellScriptLoader("spell_mage_time_warp") { }

        class spell_mage_time_warp_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_time_warp_SpellScript);

            void RemoveInvalidTargets(std::list<WorldObject*>& targets)
            {
                targets.remove_if(Trinity::UnitAuraCheck(true, HUNTER_SPELL_INSANITY));
                targets.remove_if(Trinity::UnitAuraCheck(true, SPELL_SHAMAN_EXHAUSTED));
                targets.remove_if(Trinity::UnitAuraCheck(true, SPELL_SHAMAN_SATED));
                targets.remove_if(Trinity::UnitAuraCheck(true, SPELL_MAGE_TEMPORAL_DISPLACEMENT));
            }

            void ApplyDebuff()
            {
                if (Unit* target = GetHitUnit())
                    target->CastSpell(target, SPELL_MAGE_TEMPORAL_DISPLACEMENT, true);
            }

            void Register()
            {
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_mage_time_warp_SpellScript::RemoveInvalidTargets, EFFECT_0, TARGET_UNIT_CASTER_AREA_RAID);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_mage_time_warp_SpellScript::RemoveInvalidTargets, EFFECT_1, TARGET_UNIT_CASTER_AREA_RAID);
                OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_mage_time_warp_SpellScript::RemoveInvalidTargets, EFFECT_2, TARGET_UNIT_CASTER_AREA_RAID);
                AfterHit += SpellHitFn(spell_mage_time_warp_SpellScript::ApplyDebuff);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_time_warp_SpellScript();
        }
};

// Alter Time - 127140 (overrided)
class spell_mage_alter_time_overrided : public SpellScriptLoader
{
    public:
        spell_mage_alter_time_overrided() : SpellScriptLoader("spell_mage_alter_time_overrided") { }

        class spell_mage_alter_time_overrided_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_alter_time_overrided_SpellScript);

            bool Validate(SpellInfo const* /*spellEntry*/)
            {
                if (!sSpellMgr->GetSpellInfo(SPELL_MAGE_ALTER_TIME_OVERRIDED))
                    return false;
                return true;
            }

            void HandleAfterCast()
            {
                if (Player* _player = GetCaster()->ToPlayer())
                    if (_player->HasAura(SPELL_MAGE_ALTER_TIME))
                        _player->RemoveAura(SPELL_MAGE_ALTER_TIME);
            }

            void Register()
            {
                AfterCast += SpellCastFn(spell_mage_alter_time_overrided_SpellScript::HandleAfterCast);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_alter_time_overrided_SpellScript();
        }
};

struct auraData
{
    auraData(uint32 id, int32 duration) : m_id(id), m_duration(duration) {}
    uint32 m_id;
    int32 m_duration;
};

// Alter Time - 110909
class spell_mage_alter_time : public SpellScriptLoader
{
    public:
        spell_mage_alter_time() : SpellScriptLoader("spell_mage_alter_time") { }

        class spell_mage_alter_time_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_alter_time_AuraScript);

            int32 mana;
            int32 health;
            float posX;
            float posY;
            float posZ;
            float orientation;
            uint32 map;
            std::set<auraData*> auras;

            void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Player* _player = GetTarget()->ToPlayer())
                {
                    posX = _player->GetPositionX();
                    posY = _player->GetPositionY();
                    posZ = _player->GetPositionZ();
                    orientation = _player->GetOrientation();
                    map = _player->GetMapId();
                    Unit::AuraApplicationMap const& appliedAuras = _player->GetAppliedAuras();
                    for (Unit::AuraApplicationMap::const_iterator itr = appliedAuras.begin(); itr != appliedAuras.end(); ++itr)
                    {
                        if (Aura* aura = itr->second->GetBase())
                        {
                            SpellInfo const* auraInfo = aura->GetSpellInfo();

                            if (auraInfo->Attributes & SPELL_ATTR0_HIDDEN_CLIENTSIDE)
                                continue;

                            if (auraInfo->Id == SPELL_MAGE_ALTER_TIME)
                                continue;

                            auras.insert(new auraData(auraInfo->Id, aura->GetDuration()));
                        }
                    }
                    mana = _player->GetPower(POWER_MANA);
                    health = _player->GetHealth();
                }
            }

            void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
            {
                if (Player* _player = GetTarget()->ToPlayer())
                {
                    for (std::set<auraData*>::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                    {
                        Aura* aura = !_player->HasAura((*itr)->m_id) ? _player->AddAura((*itr)->m_id, _player) : _player->GetAura((*itr)->m_id);
                        if (aura)
                        {
                            aura->SetDuration((*itr)->m_duration);
                            aura->SetNeedClientUpdateForTargets();
                        }

                        delete *itr;
                    }

                    auras.clear();

                    _player->SetPower(POWER_MANA, mana);
                    _player->SetHealth(health);
                    if (map && posX && posY && posZ && orientation)
                        _player->TeleportTo(map, posX, posY, posZ, orientation);
                }
            }

            void Register()
            {
                AfterEffectApply += AuraEffectApplyFn(spell_mage_alter_time_AuraScript::OnApply, EFFECT_0, SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS, AURA_EFFECT_HANDLE_REAL);
                OnEffectRemove += AuraEffectRemoveFn(spell_mage_alter_time_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_OVERRIDE_ACTIONBAR_SPELLS, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_alter_time_AuraScript();
        }
};

class spell_mage_cold_snap : public SpellScriptLoader
{
    public:
        spell_mage_cold_snap() : SpellScriptLoader("spell_mage_cold_snap") { }

        class spell_mage_cold_snap_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_cold_snap_SpellScript);

            bool Load()
            {
                return GetCaster()->GetTypeId() == TYPEID_PLAYER;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {

                Player* caster = GetCaster()->ToPlayer();
                // immediately finishes the cooldown on Frost spells
                const SpellCooldowns& cm = caster->GetSpellCooldownMap();
                for (SpellCooldowns::const_iterator itr = cm.begin(); itr != cm.end();)
                {
                    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(itr->first);

                    if ( spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_MAGE &&
                        (spellInfo->GetSchoolMask() & SPELL_SCHOOL_MASK_FROST) &&
                        spellInfo->Id != SPELL_MAGE_COLD_SNAP && spellInfo->GetRecoveryTime() > 0)
                    {
                        caster->RemoveSpellCooldown((itr++)->first, true);
                    }
                    else
                        ++itr;
                }
            }

            void Register()
            {
                // add dummy effect spell handler to Cold Snap
                OnEffectHit += SpellEffectFn(spell_mage_cold_snap_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_cold_snap_SpellScript();
        }
};

enum SilvermoonPolymorph
{
    NPC_AUROSALIA   = 18744,
};

// TODO: move out of here and rename - not a mage spell
class spell_mage_polymorph_cast_visual : public SpellScriptLoader
{
    public:
        spell_mage_polymorph_cast_visual() : SpellScriptLoader("spell_mage_polymorph_visual") { }

        class spell_mage_polymorph_cast_visual_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_polymorph_cast_visual_SpellScript);

            static const uint32 PolymorhForms[6];

            bool Validate(SpellInfo const* /*spellEntry*/)
            {
                // check if spell ids exist in dbc
                for (uint32 i = 0; i < 6; i++)
                    if (!sSpellMgr->GetSpellInfo(PolymorhForms[i]))
                        return false;
                return true;
            }

            void HandleDummy(SpellEffIndex /*effIndex*/)
            {
                if (Unit* target = GetCaster()->FindNearestCreature(NPC_AUROSALIA, 30.0f))
                    if (target->GetTypeId() == TYPEID_UNIT)
                        target->CastSpell(target, PolymorhForms[urand(0, 5)], true);
            }

            void Register()
            {
                // add dummy effect spell handler to Polymorph visual
                OnEffectHitTarget += SpellEffectFn(spell_mage_polymorph_cast_visual_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_polymorph_cast_visual_SpellScript();
        }
};

const uint32 spell_mage_polymorph_cast_visual::spell_mage_polymorph_cast_visual_SpellScript::PolymorhForms[6] =
{
    SPELL_MAGE_SQUIRREL_FORM,
    SPELL_MAGE_GIRAFFE_FORM,
    SPELL_MAGE_SERPENT_FORM,
    SPELL_MAGE_DRAGONHAWK_FORM,
    SPELL_MAGE_WORGEN_FORM,
    SPELL_MAGE_SHEEP_FORM
};

class spell_mage_incanters_absorbtion_base_AuraScript : public AuraScript
{
    public:
        enum Spells
        {
            SPELL_MAGE_INCANTERS_ABSORBTION_TRIGGERED = 44413,
            SPELL_MAGE_INCANTERS_ABSORBTION_R1 = 44394,
        };

        bool Validate(SpellInfo const* /*spellEntry*/)
        {
            return sSpellMgr->GetSpellInfo(SPELL_MAGE_INCANTERS_ABSORBTION_TRIGGERED)
                && sSpellMgr->GetSpellInfo(SPELL_MAGE_INCANTERS_ABSORBTION_R1);
        }

        void Trigger(AuraEffect* aurEff, DamageInfo & /*dmgInfo*/, uint32 & absorbAmount)
        {
            Unit* target = GetTarget();

            if (AuraEffect* talentAurEff = target->GetAuraEffectOfRankedSpell(SPELL_MAGE_INCANTERS_ABSORBTION_R1, EFFECT_0))
            {
                int32 bp = CalculatePct(absorbAmount, talentAurEff->GetAmount());
                target->CastCustomSpell(target, SPELL_MAGE_INCANTERS_ABSORBTION_TRIGGERED, &bp, NULL, NULL, true, NULL, aurEff);
            }
        }
};

// Incanter's Absorption
class spell_mage_incanters_absorbtion_absorb : public SpellScriptLoader
{
public:
    spell_mage_incanters_absorbtion_absorb() : SpellScriptLoader("spell_mage_incanters_absorbtion_absorb") { }

    class spell_mage_incanters_absorbtion_absorb_AuraScript : public spell_mage_incanters_absorbtion_base_AuraScript
    {
        PrepareAuraScript(spell_mage_incanters_absorbtion_absorb_AuraScript);

        void Register()
        {
             AfterEffectAbsorb += AuraEffectAbsorbFn(spell_mage_incanters_absorbtion_absorb_AuraScript::Trigger, EFFECT_0);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_mage_incanters_absorbtion_absorb_AuraScript();
    }
};

// Incanter's Absorption
class spell_mage_incanters_absorbtion_manashield : public SpellScriptLoader
{
public:
    spell_mage_incanters_absorbtion_manashield() : SpellScriptLoader("spell_mage_incanters_absorbtion_manashield") { }

    class spell_mage_incanters_absorbtion_manashield_AuraScript : public spell_mage_incanters_absorbtion_base_AuraScript
    {
        PrepareAuraScript(spell_mage_incanters_absorbtion_manashield_AuraScript);

        void Register()
        {
             AfterEffectManaShield += AuraEffectManaShieldFn(spell_mage_incanters_absorbtion_manashield_AuraScript::Trigger, EFFECT_0);
        }
    };

    AuraScript* GetAuraScript() const
    {
        return new spell_mage_incanters_absorbtion_manashield_AuraScript();
    }
};

// Living Bomb - 44457
class spell_mage_living_bomb : public SpellScriptLoader
{
    public:
        spell_mage_living_bomb() : SpellScriptLoader("spell_mage_living_bomb") { }

        class spell_mage_living_bomb_AuraScript : public AuraScript
        {
            PrepareAuraScript(spell_mage_living_bomb_AuraScript);

            void AfterApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
            {
                Unit* target = GetTarget();
                Unit* caster = GetCaster();
                if (!caster)
                    return;

                std::deque<uint64>::iterator curr = std::find(caster->m_livingBombTargets.begin(), caster->m_livingBombTargets.end(), target->GetGUID());
                if (curr != caster->m_livingBombTargets.end())
                    caster->m_livingBombTargets.erase(curr);

                // Living Bomb can only be at 3 targets at once
                while (caster->m_livingBombTargets.size() >= 3)
                {
                    if (Unit* toRemove = Unit::GetUnit(*caster, caster->m_livingBombTargets[0]))
                        toRemove->RemoveAurasDueToSpell(aurEff->GetId(), caster->GetGUID());

                    caster->m_livingBombTargets.pop_front();
                }

                caster->m_livingBombTargets.push_back(target->GetGUID());
            }

            void AfterRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
            {
                AuraRemoveMode removeMode = GetTargetApplication()->GetRemoveMode();
                if (removeMode != AURA_REMOVE_BY_DEATH && removeMode != AURA_REMOVE_BY_EXPIRE)
                    return;

                if (Unit* caster = GetCaster())
                {
                    caster->CastSpell(GetTarget(), SPELL_MAGE_LIVING_BOMB_TRIGGERED, true);

                    if (caster->HasAura(SPELL_MAGE_BRAIN_FREEZE))
                        caster->CastSpell(caster, SPELL_MAGE_BRAIN_FREEZE_TRIGGERED, true);
                }
            }

            void Register()
            {
                AfterEffectApply += AuraEffectApplyFn(spell_mage_living_bomb_AuraScript::AfterApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
                AfterEffectRemove += AuraEffectRemoveFn(spell_mage_living_bomb_AuraScript::AfterRemove, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
            }
        };

        AuraScript* GetAuraScript() const
        {
            return new spell_mage_living_bomb_AuraScript();
        }
};

class spell_mage_frost_nova : public SpellScriptLoader
{
    public:
        spell_mage_frost_nova() : SpellScriptLoader("spell_mage_frost_nova") { }

        class spell_mage_frost_nova_SpellScript : public SpellScript
        {
            PrepareSpellScript(spell_mage_frost_nova_SpellScript)

            bool Validate(SpellEntry const * /*spellEntry*/)
            {
                return true;
            }

            void HandleOnHit()
            {
                if (Unit * caster = GetCaster())
                {
                    if (caster->GetTypeId() == TYPEID_PLAYER)
                    {
                        caster->ToPlayer()->KilledMonsterCredit(44175, 0);
                        caster->ToPlayer()->KilledMonsterCredit(44548, 0);
                    }
                }
            }

            void Register()
            {
                OnHit += SpellHitFn(spell_mage_frost_nova_SpellScript::HandleOnHit);
            }
        };

        SpellScript* GetSpellScript() const
        {
            return new spell_mage_frost_nova_SpellScript();
        }
};

void AddSC_mage_spell_scripts()
{
    new spell_mage_incanters_ward_cooldown();
    new spell_mage_incanters_ward();
    new spell_mage_arcane_missile();
    new spell_mage_cauterize();
    new spell_mage_mirror_images();
    new spell_mage_pyromaniac();
    new spell_mage_arcane_barrage();
    new spell_mage_arcane_explosion();
    new spell_mage_arcane_missiles();
    new spell_mage_slow();
    new spell_mage_frostbolt();
    new spell_mage_invocation();
    new spell_mage_frost_bomb();
    new spell_mage_nether_tempest();
    new spell_mage_blazing_speed();
    new spell_mage_frostjaw();
    new spell_mage_combustion();
    new spell_mage_inferno_blast();
    new spell_mage_arcane_brilliance();
    new spell_mage_replenish_mana();
    new spell_mage_evocation();
    new spell_mage_conjure_refreshment();
    new spell_mage_conjure_refreshment_table();
    new spell_mage_time_warp();
    new spell_mage_alter_time_overrided();
    new spell_mage_alter_time();
    new spell_mage_cold_snap();
    new spell_mage_incanters_absorbtion_absorb();
    new spell_mage_incanters_absorbtion_manashield();
    new spell_mage_polymorph_cast_visual();
    new spell_mage_living_bomb();
    new spell_mage_frost_nova();
}
