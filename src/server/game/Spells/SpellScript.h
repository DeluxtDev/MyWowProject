/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
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

#ifndef __SPELL_SCRIPT_H
#define __SPELL_SCRIPT_H

#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "Util.h"
#include <memory>
#include <stack>

class Aura;
class AuraApplication;
class AuraEffect;
class Creature;
class Corpse;
class DamageInfo;
class DispelInfo;
class DynamicObject;
class GameObject;
class Item;
class ModuleReference;
class Player;
class ProcEventInfo;
class Spell;
class SpellEffectInfo;
class SpellInfo;
class SpellScript;
class Unit;
class WorldLocation;
class WorldObject;
struct SpellDestination;
struct SpellModifier;
struct SpellValue;

#define SPELL_EFFECT_ANY (uint16)-1
#define SPELL_AURA_ANY (uint16)-1

enum SpellScriptState
{
    SPELL_SCRIPT_STATE_NONE = 0,
    SPELL_SCRIPT_STATE_REGISTRATION,
    SPELL_SCRIPT_STATE_LOADING,
    SPELL_SCRIPT_STATE_UNLOADING
};
#define SPELL_SCRIPT_STATE_END SPELL_SCRIPT_STATE_UNLOADING + 1

// helper class from which SpellScript and SpellAura derive, use these classes instead
class TC_GAME_API _SpellScript
{
    // internal use classes & functions
    // DO NOT OVERRIDE THESE IN SCRIPTS
    protected:
        virtual bool _Validate(SpellInfo const* entry);

    public:
        _SpellScript() : m_currentScriptState(SPELL_SCRIPT_STATE_NONE), m_scriptName(nullptr), m_scriptSpellId(0) {}
        virtual ~_SpellScript() { }
        void _Register();
        void _Unload();
        void _Init(std::string const* scriptname, uint32 spellId);
        std::string const* _GetScriptName() const;

    protected:
        class TC_GAME_API EffectHook
        {
            public:
                EffectHook(uint8 _effIndex);
                virtual ~EffectHook() { }

                uint8 GetAffectedEffectsMask(SpellInfo const* spellInfo);
                bool IsEffectAffected(SpellInfo const* spellInfo, uint8 effIndex);
                virtual bool CheckEffect(SpellInfo const* spellInfo, uint8 effIndex) = 0;
                std::string EffIndexToString();
            protected:
                uint8 effIndex;
        };

        class TC_GAME_API EffectNameCheck
        {
            public:
                EffectNameCheck(uint16 _effName) { effName = _effName; }
                bool Check(SpellInfo const* spellInfo, uint8 effIndex);
                std::string ToString();
            private:
                uint16 effName;
        };

        class TC_GAME_API EffectAuraNameCheck
        {
            public:
                EffectAuraNameCheck(uint16 _effAurName) { effAurName = _effAurName; }
                bool Check(SpellInfo const* spellInfo, uint8 effIndex);
                std::string ToString();
            private:
                uint16 effAurName;
        };

        uint8 m_currentScriptState;
        std::string const* m_scriptName;
        uint32 m_scriptSpellId;

    private:

#ifdef TRINITY_API_USE_DYNAMIC_LINKING

        // Strong reference to keep the binary code loaded
        std::shared_ptr<ModuleReference> m_moduleReference;

#endif // #ifndef TRINITY_API_USE_DYNAMIC_LINKING

    public:
        //
        // SpellScript/AuraScript interface base
        // these functions are safe to override, see notes below for usage instructions
        //
        // Function in which handler functions are registered, must be implemented in script
        virtual void Register() = 0;
        // Function called on server startup, if returns false script won't be used in core
        // use for: dbc/template data presence/correctness checks
        virtual bool Validate(SpellInfo const* /*spellInfo*/) { return true; }
        // Function called when script is created, if returns false script will be unloaded afterwards
        // use for: initializing local script variables (DO NOT USE CONSTRUCTOR FOR THIS PURPOSE!)
        virtual bool Load() { return true; }
        // Function called when script is destroyed
        // use for: deallocating memory allocated by script
        virtual void Unload() { }
        // Helpers
        static bool ValidateSpellInfo(std::initializer_list<uint32> spellIds)
        {
            return _ValidateSpellInfo(spellIds.begin(), spellIds.end());
        }

        template <class T>
        static bool ValidateSpellInfo(T const& spellIds)
        {
            return _ValidateSpellInfo(std::begin(spellIds), std::end(spellIds));
        }

    private:
        template<typename Iterator>
        static bool _ValidateSpellInfo(Iterator begin, Iterator end)
        {
            bool allValid = true;
            while (begin != end)
            {
                if (!_ValidateSpellInfo(*begin))
                    allValid = false;

                ++begin;
            }
            return allValid;
        }

        static bool _ValidateSpellInfo(uint32 spellId);
};

// SpellScript interface - enum used for runtime checks of script function calls
enum SpellScriptHookType
{
    SPELL_SCRIPT_HOOK_EFFECT_LAUNCH = SPELL_SCRIPT_STATE_END,
    SPELL_SCRIPT_HOOK_EFFECT_LAUNCH_TARGET,
    SPELL_SCRIPT_HOOK_EFFECT_HIT,
    SPELL_SCRIPT_HOOK_EFFECT_HIT_TARGET,
    SPELL_SCRIPT_HOOK_EFFECT_SUCCESSFUL_DISPEL,
    SPELL_SCRIPT_HOOK_BEFORE_HIT,
    SPELL_SCRIPT_HOOK_HIT,
    SPELL_SCRIPT_HOOK_AFTER_HIT,
    SPELL_SCRIPT_HOOK_OBJECT_AREA_TARGET_SELECT,
    SPELL_SCRIPT_HOOK_OBJECT_TARGET_SELECT,
    SPELL_SCRIPT_HOOK_DESTINATION_TARGET_SELECT,
    SPELL_SCRIPT_HOOK_CHECK_CAST,
    SPELL_SCRIPT_HOOK_BEFORE_CAST,
    SPELL_SCRIPT_HOOK_ON_CAST,
    SPELL_SCRIPT_HOOK_ON_RESIST_ABSORB_CALCULATION,
    SPELL_SCRIPT_HOOK_AFTER_CAST
};

#define HOOK_SPELL_HIT_START SPELL_SCRIPT_HOOK_EFFECT_HIT
#define HOOK_SPELL_HIT_END SPELL_SCRIPT_HOOK_AFTER_HIT + 1
#define HOOK_SPELL_START SPELL_SCRIPT_HOOK_EFFECT
#define HOOK_SPELL_END SPELL_SCRIPT_HOOK_CHECK_CAST + 1
#define HOOK_SPELL_COUNT HOOK_SPELL_END - HOOK_SPELL_START

class TC_GAME_API SpellScript : public _SpellScript
{
    // internal use classes & functions
    // DO NOT OVERRIDE THESE IN SCRIPTS
    public:
        #define SPELLSCRIPT_FUNCTION_TYPE_DEFINES(CLASSNAME) \
            typedef SpellCastResult(CLASSNAME::*SpellCheckCastFnType)(); \
            typedef void(CLASSNAME::*SpellEffectFnType)(SpellEffIndex); \
            typedef void(CLASSNAME::*SpellBeforeHitFnType)(SpellMissInfo missInfo); \
            typedef void(CLASSNAME::*SpellHitFnType)(); \
            typedef void(CLASSNAME::*SpellCastFnType)(); \
            typedef void(CLASSNAME::*SpellOnResistAbsorbCalculateFnType)(DamageInfo const& damageInfo, uint32& resistAmount, int32& absorbAmount); \
            typedef void(CLASSNAME::*SpellObjectAreaTargetSelectFnType)(std::list<WorldObject*>&); \
            typedef void(CLASSNAME::*SpellObjectTargetSelectFnType)(WorldObject*&); \
            typedef void(CLASSNAME::*SpellDestinationTargetSelectFnType)(SpellDestination&);

        SPELLSCRIPT_FUNCTION_TYPE_DEFINES(SpellScript)

        class TC_GAME_API CastHandler
        {
            public:
                CastHandler(SpellCastFnType _pCastHandlerScript);
                void Call(SpellScript* spellScript);
            private:
                SpellCastFnType pCastHandlerScript;
        };

        class TC_GAME_API CheckCastHandler
        {
            public:
                CheckCastHandler(SpellCheckCastFnType checkCastHandlerScript);
                SpellCastResult Call(SpellScript* spellScript);
            private:
                SpellCheckCastFnType _checkCastHandlerScript;
        };

        class TC_GAME_API EffectHandler : public  _SpellScript::EffectNameCheck, public _SpellScript::EffectHook
        {
            public:
                EffectHandler(SpellEffectFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                std::string ToString();
                bool CheckEffect(SpellInfo const* spellInfo, uint8 effIndex) override;
                void Call(SpellScript* spellScript, SpellEffIndex effIndex);
            private:
                SpellEffectFnType pEffectHandlerScript;
        };

        class TC_GAME_API HitHandler
        {
            public:
                HitHandler(SpellHitFnType _pHitHandlerScript);
                void Call(SpellScript* spellScript);
            private:
                SpellHitFnType pHitHandlerScript;
        };

        class TC_GAME_API BeforeHitHandler
        {
            public:
                BeforeHitHandler(SpellBeforeHitFnType pBeforeHitHandlerScript);
                void Call(SpellScript* spellScript, SpellMissInfo missInfo);
            private:
                SpellBeforeHitFnType _pBeforeHitHandlerScript;
        };

        class TC_GAME_API TargetHook : public _SpellScript::EffectHook
        {
            public:
                TargetHook(uint8 _effectIndex, uint16 _targetType, bool _area, bool _dest);
                bool CheckEffect(SpellInfo const* spellInfo, uint8 effIndex) override;
                std::string ToString();
                uint16 GetTarget() const { return targetType; }
            protected:
                uint16 targetType;
                bool area;
                bool dest;
        };

        class TC_GAME_API ObjectAreaTargetSelectHandler : public TargetHook
        {
            public:
                ObjectAreaTargetSelectHandler(SpellObjectAreaTargetSelectFnType _pObjectAreaTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType);
                void Call(SpellScript* spellScript, std::list<WorldObject*>& targets);
                bool HasSameTargetFunctionAs(ObjectAreaTargetSelectHandler const& other) const
                {
                    return pObjectAreaTargetSelectHandlerScript == other.pObjectAreaTargetSelectHandlerScript;
                }
            private:
                SpellObjectAreaTargetSelectFnType pObjectAreaTargetSelectHandlerScript;
        };

        class TC_GAME_API ObjectTargetSelectHandler : public TargetHook
        {
            public:
                ObjectTargetSelectHandler(SpellObjectTargetSelectFnType _pObjectTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType);
                void Call(SpellScript* spellScript, WorldObject*& target);
                bool HasSameTargetFunctionAs(ObjectTargetSelectHandler const& other) const
                {
                    return pObjectTargetSelectHandlerScript == other.pObjectTargetSelectHandlerScript;
                }
            private:
                SpellObjectTargetSelectFnType pObjectTargetSelectHandlerScript;
        };

        class TC_GAME_API DestinationTargetSelectHandler : public TargetHook
        {
            public:
                DestinationTargetSelectHandler(SpellDestinationTargetSelectFnType _DestinationTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType);
                void Call(SpellScript* spellScript, SpellDestination& target);
            private:
                SpellDestinationTargetSelectFnType DestinationTargetSelectHandlerScript;
        };

        class TC_GAME_API OnCalculateResistAbsorbHandler
        {
        public:
            OnCalculateResistAbsorbHandler(SpellOnResistAbsorbCalculateFnType _pOnCalculateResistAbsorbHandlerScript);
            void Call(SpellScript* spellScript, DamageInfo const& damageInfo, uint32& resistAmount, int32& absorbAmount);
        private:
            SpellOnResistAbsorbCalculateFnType pOnCalculateResistAbsorbHandlerScript;
        };

        #define SPELLSCRIPT_FUNCTION_CAST_DEFINES(CLASSNAME) \
        class CastHandlerFunction : public SpellScript::CastHandler { public: explicit CastHandlerFunction(SpellCastFnType _pCastHandlerScript) : SpellScript::CastHandler((SpellScript::SpellCastFnType)_pCastHandlerScript) { } }; \
        class CheckCastHandlerFunction : public SpellScript::CheckCastHandler { public: explicit CheckCastHandlerFunction(SpellCheckCastFnType _checkCastHandlerScript) : SpellScript::CheckCastHandler((SpellScript::SpellCheckCastFnType)_checkCastHandlerScript) { } }; \
        class EffectHandlerFunction : public SpellScript::EffectHandler { public: explicit EffectHandlerFunction(SpellEffectFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : SpellScript::EffectHandler((SpellScript::SpellEffectFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class HitHandlerFunction : public SpellScript::HitHandler { public: explicit HitHandlerFunction(SpellHitFnType _pHitHandlerScript) : SpellScript::HitHandler((SpellScript::SpellHitFnType)_pHitHandlerScript) { } }; \
        class BeforeHitHandlerFunction : public SpellScript::BeforeHitHandler { public: explicit BeforeHitHandlerFunction(SpellBeforeHitFnType pBeforeHitHandlerScript) : SpellScript::BeforeHitHandler((SpellScript::SpellBeforeHitFnType)pBeforeHitHandlerScript) { } }; \
        class OnCalculateResistAbsorbHandlerFunction : public SpellScript::OnCalculateResistAbsorbHandler { public: explicit OnCalculateResistAbsorbHandlerFunction(SpellOnResistAbsorbCalculateFnType _onCalculateResistAbsorbScript) : SpellScript::OnCalculateResistAbsorbHandler((SpellScript::SpellOnResistAbsorbCalculateFnType)_onCalculateResistAbsorbScript) { } }; \
        class ObjectAreaTargetSelectHandlerFunction : public SpellScript::ObjectAreaTargetSelectHandler { public: explicit ObjectAreaTargetSelectHandlerFunction(SpellObjectAreaTargetSelectFnType _pObjectAreaTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType) : SpellScript::ObjectAreaTargetSelectHandler((SpellScript::SpellObjectAreaTargetSelectFnType)_pObjectAreaTargetSelectHandlerScript, _effIndex, _targetType) { } }; \
        class ObjectTargetSelectHandlerFunction : public SpellScript::ObjectTargetSelectHandler { public: explicit ObjectTargetSelectHandlerFunction(SpellObjectTargetSelectFnType _pObjectTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType) : SpellScript::ObjectTargetSelectHandler((SpellScript::SpellObjectTargetSelectFnType)_pObjectTargetSelectHandlerScript, _effIndex, _targetType) { } }; \
        class DestinationTargetSelectHandlerFunction : public SpellScript::DestinationTargetSelectHandler { public: explicit DestinationTargetSelectHandlerFunction(SpellDestinationTargetSelectFnType _DestinationTargetSelectHandlerScript, uint8 _effIndex, uint16 _targetType) : SpellScript::DestinationTargetSelectHandler((SpellScript::SpellDestinationTargetSelectFnType)_DestinationTargetSelectHandlerScript, _effIndex, _targetType) { } }

        #define PrepareSpellScript(CLASSNAME) SPELLSCRIPT_FUNCTION_TYPE_DEFINES(CLASSNAME) SPELLSCRIPT_FUNCTION_CAST_DEFINES(CLASSNAME)
    public:
        SpellScript();
        ~SpellScript();
        bool _Validate(SpellInfo const* entry) override;
        bool _Load(Spell* spell);
        void _InitHit();
        bool _IsEffectPrevented(SpellEffIndex effIndex) { return (m_hitPreventEffectMask & (1 << effIndex)) != 0; }
        bool _IsDefaultEffectPrevented(SpellEffIndex effIndex) { return (m_hitPreventDefaultEffectMask & (1 << effIndex)) != 0; }
        void _PrepareScriptCall(SpellScriptHookType hookType);
        void _FinishScriptCall();
        bool IsInCheckCastHook() const;
        bool IsInTargetHook() const;
        bool IsInModifiableHook() const;
        bool IsInHitPhase() const;
        bool IsInEffectHook() const;
    private:
        Spell* m_spell;
        uint8 m_hitPreventEffectMask;
        uint8 m_hitPreventDefaultEffectMask;
    public:
        //
        // SpellScript interface
        // hooks to which you can attach your functions
        //
        // example: BeforeCast += SpellCastFn(class::function);
        HookList<CastHandler> BeforeCast;
        // example: OnCast += SpellCastFn(class::function);
        HookList<CastHandler> OnCast;
        // example: AfterCast += SpellCastFn(class::function);
        HookList<CastHandler> AfterCast;
        #define SpellCastFn(F) CastHandlerFunction(&F)

        // example: OnCheckCast += SpellCheckCastFn();
        // where function is SpellCastResult function()
        HookList<CheckCastHandler> OnCheckCast;
        #define SpellCheckCastFn(F) CheckCastHandlerFunction(&F)

        // example: OnCalculateResistAbsorb += SpellOnResistAbsorbCalculateFn(class::function);
        // where function is void function(DamageInfo const& damageInfo, uint32& resistAmount, int32& absorbAmount)
        HookList<OnCalculateResistAbsorbHandler> OnCalculateResistAbsorb;
        #define SpellOnResistAbsorbCalculateFn(F) OnCalculateResistAbsorbHandlerFunction(&F)

        // example: OnEffect**** += SpellEffectFn(class::function, EffectIndexSpecifier, EffectNameSpecifier);
        // where function is void function(SpellEffIndex effIndex)
        HookList<EffectHandler> OnEffectLaunch;
        HookList<EffectHandler> OnEffectLaunchTarget;
        HookList<EffectHandler> OnEffectHit;
        HookList<EffectHandler> OnEffectHitTarget;
        HookList<EffectHandler> OnEffectSuccessfulDispel;
        #define SpellEffectFn(F, I, N) EffectHandlerFunction(&F, I, N)

        // example: BeforeHit += BeforeSpellHitFn(class::function);
        // where function is void function(SpellMissInfo missInfo)
        HookList<BeforeHitHandler> BeforeHit;
        #define BeforeSpellHitFn(F) BeforeHitHandlerFunction(&F)

        // example: OnHit += SpellHitFn(class::function);
        HookList<HitHandler> OnHit;
        // example: AfterHit += SpellHitFn(class::function);
        HookList<HitHandler> AfterHit;
        // where function is: void function()
        #define SpellHitFn(F) HitHandlerFunction(&F)

        // example: OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(class::function, EffectIndexSpecifier, TargetsNameSpecifier);
        // where function is void function(std::list<WorldObject*>& targets)
        HookList<ObjectAreaTargetSelectHandler> OnObjectAreaTargetSelect;
        #define SpellObjectAreaTargetSelectFn(F, I, N) ObjectAreaTargetSelectHandlerFunction(&F, I, N)

        // example: OnObjectTargetSelect += SpellObjectTargetSelectFn(class::function, EffectIndexSpecifier, TargetsNameSpecifier);
        // where function is void function(WorldObject*& target)
        HookList<ObjectTargetSelectHandler> OnObjectTargetSelect;
        #define SpellObjectTargetSelectFn(F, I, N) ObjectTargetSelectHandlerFunction(&F, I, N)

        // example: OnDestinationTargetSelect += SpellDestinationTargetSelectFn(class::function, EffectIndexSpecifier, TargetsNameSpecifier);
        // where function is void function(SpellDestination& target)
        HookList<DestinationTargetSelectHandler> OnDestinationTargetSelect;
        #define SpellDestinationTargetSelectFn(F, I, N) DestinationTargetSelectHandlerFunction(&F, I, N)

        // hooks are executed in following order, at specified event of spell:
        // 1. BeforeCast - executed when spell preparation is finished (when cast bar becomes full) before cast is handled
        // 2. OnCheckCast - allows to override result of CheckCast function
        // 3a. OnObjectAreaTargetSelect - executed just before adding selected targets to final target list (for area targets)
        // 3b. OnObjectTargetSelect - executed just before adding selected target to final target list (for single unit targets)
        // 3c. OnDestinationTargetSelect - executed just before adding selected target to final target list (for destination targets)
        // 4. OnCast - executed just before spell is launched (creates missile) or executed
        // 5. AfterCast - executed after spell missile is launched and immediate spell actions are done
        // 6. OnEffectLaunch - executed just before specified effect handler call - when spell missile is launched
        // 7. OnEffectLaunchTarget - executed just before specified effect handler call - when spell missile is launched - called for each target from spell target map
        // 8. OnCalculateResistAbsorb - executed when damage resist/absorbs is calculated - before spell hit target
        // 9. OnEffectHit - executed just before specified effect handler call - when spell missile hits dest
        // 10. BeforeHit - executed just before spell hits a target - called for each target from spell target map
        // 11. OnEffectHitTarget - executed just before specified effect handler call - called for each target from spell target map
        // 12. OnHit - executed just before spell deals damage and procs auras - when spell hits target - called for each target from spell target map
        // 13. AfterHit - executed just after spell finishes all it's jobs for target - called for each target from spell target map

        // this hook is only executed after a successful dispel of any aura
        // OnEffectSuccessfulDispel - executed just after effect successfully dispelled aura(s)

        //
        // methods allowing interaction with Spell object
        //
        // methods useable during all spell handling phases
        Unit* GetCaster() const;
        GameObject* GetGObjCaster() const;
        Unit* GetOriginalCaster() const;
        SpellInfo const* GetSpellInfo() const;
        SpellEffectInfo const& GetEffectInfo(SpellEffIndex effIndex) const;
        SpellValue const* GetSpellValue() const;

        // methods useable after spell is prepared
        // accessors to the explicit targets of the spell
        // explicit target - target selected by caster (player, game client, or script - DoCast(explicitTarget, ...), required for spell to be cast
        // examples:
        // -shadowstep - explicit target is the unit you want to go behind of
        // -chain heal - explicit target is the unit to be healed first
        // -holy nova/arcane explosion - explicit target = NULL because target you are selecting doesn't affect how spell targets are selected
        // you can determine if spell requires explicit targets by dbc columns:
        // - Targets - mask of explicit target types
        // - ImplicitTargetXX set to TARGET_XXX_TARGET_YYY, _TARGET_ here means that explicit target is used by the effect, so spell needs one too

        // returns: WorldLocation which was selected as a spell destination or NULL
        WorldLocation const* GetExplTargetDest() const;

        void SetExplTargetDest(WorldLocation& loc);

        // returns: WorldObject which was selected as an explicit spell target or NULL if there's no target
        WorldObject* GetExplTargetWorldObject() const;

        // returns: Unit which was selected as an explicit spell target or NULL if there's no target
        Unit* GetExplTargetUnit() const;

        // returns: GameObject which was selected as an explicit spell target or NULL if there's no target
        GameObject* GetExplTargetGObj() const;

        // returns: Item which was selected as an explicit spell target or NULL if there's no target
        Item* GetExplTargetItem() const;

        // methods useable only during spell hit on target, or during spell launch on target:
        // returns: target of current effect if it was Unit otherwise NULL
        Unit* GetHitUnit() const;
        // returns: target of current effect if it was Creature otherwise NULL
        Creature* GetHitCreature() const;
        // returns: target of current effect if it was Player otherwise NULL
        Player* GetHitPlayer() const;
        // returns: target of current effect if it was Item otherwise NULL
        Item* GetHitItem() const;
        // returns: target of current effect if it was GameObject otherwise NULL
        GameObject* GetHitGObj() const;
        // returns: target of current effect if it was Corpse otherwise nullptr
        Corpse* GetHitCorpse() const;
        // returns: destination of current effect
        WorldLocation* GetHitDest() const;
        // setter/getter for for damage done by spell to target of spell hit
        // returns damage calculated before hit, and real dmg done after hit
        int32 GetHitDamage() const;
        void SetHitDamage(int32 damage);
        void PreventHitDamage() { SetHitDamage(0); }
        // setter/getter for for heal done by spell to target of spell hit
        // returns healing calculated before hit, and real dmg done after hit
        int32 GetHitHeal() const;
        void SetHitHeal(int32 heal);
        void PreventHitHeal() { SetHitHeal(0); }
        Spell* GetSpell() const { return m_spell; }
        // returns current spell hit target aura
        Aura* GetHitAura(bool dynObjAura = false) const;
        // prevents applying aura on current spell hit target
        void PreventHitAura();

        // prevents effect execution on current spell hit target
        // including other effect/hit scripts
        // will not work on aura/damage/heal
        // will not work if effects were already handled
        void PreventHitEffect(SpellEffIndex effIndex);

        // prevents default effect execution on current spell hit target
        // will not work on aura/damage/heal effects
        // will not work if effects were already handled
        void PreventHitDefaultEffect(SpellEffIndex effIndex);

        // method avalible only in EffectHandler method
        SpellEffectInfo const& GetEffectInfo() const;
        int32 GetEffectValue() const;
        void SetEffectValue(int32 value);

        // returns: cast item if present.
        Item* GetCastItem() const;

        // Creates item. Calls Spell::DoCreateItem method.
        void CreateItem(uint32 itemId);

        // Returns SpellInfo from the spell that triggered the current one
        SpellInfo const* GetTriggeringSpell() const;

        // finishes spellcast prematurely with selected error message
        void FinishCast(SpellCastResult result, uint32* param1 = nullptr, uint32* param2 = nullptr);

        void SetCustomCastResultMessage(SpellCustomErrors result);
};

// AuraScript interface - enum used for runtime checks of script function calls
enum AuraScriptHookType
{
    AURA_SCRIPT_HOOK_EFFECT_APPLY = SPELL_SCRIPT_STATE_END,
    AURA_SCRIPT_HOOK_EFFECT_AFTER_APPLY,
    AURA_SCRIPT_HOOK_EFFECT_REMOVE,
    AURA_SCRIPT_HOOK_EFFECT_AFTER_REMOVE,
    AURA_SCRIPT_HOOK_EFFECT_PERIODIC,
    AURA_SCRIPT_HOOK_EFFECT_UPDATE_PERIODIC,
    AURA_SCRIPT_HOOK_EFFECT_CALC_AMOUNT,
    AURA_SCRIPT_HOOK_EFFECT_CALC_PERIODIC,
    AURA_SCRIPT_HOOK_EFFECT_CALC_SPELLMOD,
    AURA_SCRIPT_HOOK_EFFECT_ABSORB,
    AURA_SCRIPT_HOOK_EFFECT_AFTER_ABSORB,
    AURA_SCRIPT_HOOK_EFFECT_MANASHIELD,
    AURA_SCRIPT_HOOK_EFFECT_AFTER_MANASHIELD,
    AURA_SCRIPT_HOOK_EFFECT_SPLIT,
    AURA_SCRIPT_HOOK_CHECK_AREA_TARGET,
    AURA_SCRIPT_HOOK_DISPEL,
    AURA_SCRIPT_HOOK_AFTER_DISPEL,
    // Spell Proc Hooks
    AURA_SCRIPT_HOOK_CHECK_PROC,
    AURA_SCRIPT_HOOK_CHECK_EFFECT_PROC,
    AURA_SCRIPT_HOOK_PREPARE_PROC,
    AURA_SCRIPT_HOOK_PROC,
    AURA_SCRIPT_HOOK_EFFECT_PROC,
    AURA_SCRIPT_HOOK_EFFECT_AFTER_PROC,
    AURA_SCRIPT_HOOK_AFTER_PROC,
    /*AURA_SCRIPT_HOOK_APPLY,
    AURA_SCRIPT_HOOK_REMOVE, */
};
/*
#define HOOK_AURA_EFFECT_START HOOK_AURA_EFFECT_APPLY
#define HOOK_AURA_EFFECT_END HOOK_AURA_EFFECT_CALC_SPELLMOD + 1
#define HOOK_AURA_EFFECT_COUNT HOOK_AURA_EFFECT_END - HOOK_AURA_EFFECT_START
*/
class TC_GAME_API AuraScript : public _SpellScript
{
    // internal use classes & functions
    // DO NOT OVERRIDE THESE IN SCRIPTS
    public:

    #define AURASCRIPT_FUNCTION_TYPE_DEFINES(CLASSNAME) \
        typedef bool(CLASSNAME::*AuraCheckAreaTargetFnType)(Unit* target); \
        typedef void(CLASSNAME::*AuraDispelFnType)(DispelInfo* dispelInfo); \
        typedef void(CLASSNAME::*AuraEffectApplicationModeFnType)(AuraEffect const*, AuraEffectHandleModes); \
        typedef void(CLASSNAME::*AuraEffectPeriodicFnType)(AuraEffect const*); \
        typedef void(CLASSNAME::*AuraEffectUpdatePeriodicFnType)(AuraEffect*); \
        typedef void(CLASSNAME::*AuraEffectCalcAmountFnType)(AuraEffect const*, int32 &, bool &); \
        typedef void(CLASSNAME::*AuraEffectCalcPeriodicFnType)(AuraEffect const*, bool &, int32 &); \
        typedef void(CLASSNAME::*AuraEffectCalcSpellModFnType)(AuraEffect const*, SpellModifier* &); \
        typedef void(CLASSNAME::*AuraEffectAbsorbFnType)(AuraEffect*, DamageInfo &, uint32 &); \
        typedef void(CLASSNAME::*AuraEffectSplitFnType)(AuraEffect*, DamageInfo &, uint32 &); \
        typedef bool(CLASSNAME::*AuraCheckProcFnType)(ProcEventInfo&); \
        typedef bool(CLASSNAME::*AuraCheckEffectProcFnType)(AuraEffect const*, ProcEventInfo&); \
        typedef void(CLASSNAME::*AuraProcFnType)(ProcEventInfo&); \
        typedef void(CLASSNAME::*AuraEffectProcFnType)(AuraEffect const*, ProcEventInfo&); \

        AURASCRIPT_FUNCTION_TYPE_DEFINES(AuraScript)

        class TC_GAME_API CheckAreaTargetHandler
        {
            public:
                CheckAreaTargetHandler(AuraCheckAreaTargetFnType pHandlerScript);
                bool Call(AuraScript* auraScript, Unit* target);
            private:
                AuraCheckAreaTargetFnType pHandlerScript;
        };
        class TC_GAME_API AuraDispelHandler
        {
            public:
                AuraDispelHandler(AuraDispelFnType pHandlerScript);
                void Call(AuraScript* auraScript, DispelInfo* dispelInfo);
            private:
                AuraDispelFnType pHandlerScript;
        };
        class TC_GAME_API EffectBase : public  _SpellScript::EffectAuraNameCheck, public _SpellScript::EffectHook
        {
            public:
                EffectBase(uint8 _effIndex, uint16 _effName);
                std::string ToString();
                bool CheckEffect(SpellInfo const* spellInfo, uint8 effIndex) override;
        };
        class TC_GAME_API EffectPeriodicHandler : public EffectBase
        {
            public:
                EffectPeriodicHandler(AuraEffectPeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                void Call(AuraScript* auraScript, AuraEffect const* _aurEff);
            private:
                AuraEffectPeriodicFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectUpdatePeriodicHandler : public EffectBase
        {
            public:
                EffectUpdatePeriodicHandler(AuraEffectUpdatePeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                void Call(AuraScript* auraScript, AuraEffect* aurEff);
            private:
                AuraEffectUpdatePeriodicFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectCalcAmountHandler : public EffectBase
        {
            public:
                EffectCalcAmountHandler(AuraEffectCalcAmountFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                void Call(AuraScript* auraScript, AuraEffect const* aurEff, int32 & amount, bool & canBeRecalculated);
            private:
                AuraEffectCalcAmountFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectCalcPeriodicHandler : public EffectBase
        {
            public:
                EffectCalcPeriodicHandler(AuraEffectCalcPeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                void Call(AuraScript* auraScript, AuraEffect const* aurEff, bool & isPeriodic, int32 & periodicTimer);
            private:
                AuraEffectCalcPeriodicFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectCalcSpellModHandler : public EffectBase
        {
            public:
                EffectCalcSpellModHandler(AuraEffectCalcSpellModFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName);
                void Call(AuraScript* auraScript, AuraEffect const* aurEff, SpellModifier* & spellMod);
            private:
                AuraEffectCalcSpellModFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectApplyHandler : public EffectBase
        {
            public:
                EffectApplyHandler(AuraEffectApplicationModeFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName, AuraEffectHandleModes _mode);
                void Call(AuraScript* auraScript, AuraEffect const* _aurEff, AuraEffectHandleModes _mode);
            private:
                AuraEffectApplicationModeFnType pEffectHandlerScript;
                AuraEffectHandleModes mode;
        };
        class TC_GAME_API EffectAbsorbHandler : public EffectBase
        {
            public:
                EffectAbsorbHandler(AuraEffectAbsorbFnType _pEffectHandlerScript, uint8 _effIndex);
                void Call(AuraScript* auraScript, AuraEffect* aurEff, DamageInfo & dmgInfo, uint32 & absorbAmount);
            private:
                AuraEffectAbsorbFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectManaShieldHandler : public EffectBase
        {
            public:
                EffectManaShieldHandler(AuraEffectAbsorbFnType _pEffectHandlerScript, uint8 _effIndex);
                void Call(AuraScript* auraScript, AuraEffect* aurEff, DamageInfo & dmgInfo, uint32 & absorbAmount);
            private:
                AuraEffectAbsorbFnType pEffectHandlerScript;
        };
        class TC_GAME_API EffectSplitHandler : public EffectBase
        {
            public:
                EffectSplitHandler(AuraEffectSplitFnType _pEffectHandlerScript, uint8 _effIndex);
                void Call(AuraScript* auraScript, AuraEffect* aurEff, DamageInfo & dmgInfo, uint32 & splitAmount);
            private:
                AuraEffectSplitFnType pEffectHandlerScript;
        };
        class TC_GAME_API CheckProcHandler
        {
            public:
                CheckProcHandler(AuraCheckProcFnType handlerScript);
                bool Call(AuraScript* auraScript, ProcEventInfo& eventInfo);
            private:
                AuraCheckProcFnType _HandlerScript;
        };
        class TC_GAME_API CheckEffectProcHandler : public EffectBase
        {
            public:
                CheckEffectProcHandler(AuraCheckEffectProcFnType handlerScript, uint8 effIndex, uint16 effName);
                bool Call(AuraScript* auraScript, AuraEffect const* aurEff, ProcEventInfo& eventInfo);
            private:
                AuraCheckEffectProcFnType _HandlerScript;
        };
        class TC_GAME_API AuraProcHandler
        {
            public:
                AuraProcHandler(AuraProcFnType handlerScript);
                void Call(AuraScript* auraScript, ProcEventInfo& eventInfo);
            private:
                AuraProcFnType _HandlerScript;
        };
        class TC_GAME_API EffectProcHandler : public EffectBase
        {
            public:
                EffectProcHandler(AuraEffectProcFnType effectHandlerScript, uint8 effIndex, uint16 effName);
                void Call(AuraScript* auraScript, AuraEffect const* aurEff, ProcEventInfo& eventInfo);
            private:
                AuraEffectProcFnType _EffectHandlerScript;
        };

        #define AURASCRIPT_FUNCTION_CAST_DEFINES(CLASSNAME) \
        class CheckAreaTargetFunction : public AuraScript::CheckAreaTargetHandler { public: explicit CheckAreaTargetFunction(AuraCheckAreaTargetFnType _pHandlerScript) : AuraScript::CheckAreaTargetHandler((AuraScript::AuraCheckAreaTargetFnType)_pHandlerScript) { } }; \
        class AuraDispelFunction : public AuraScript::AuraDispelHandler { public: explicit AuraDispelFunction(AuraDispelFnType _pHandlerScript) : AuraScript::AuraDispelHandler((AuraScript::AuraDispelFnType)_pHandlerScript) { } }; \
        class EffectPeriodicHandlerFunction : public AuraScript::EffectPeriodicHandler { public: explicit EffectPeriodicHandlerFunction(AuraEffectPeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : AuraScript::EffectPeriodicHandler((AuraScript::AuraEffectPeriodicFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class EffectUpdatePeriodicHandlerFunction : public AuraScript::EffectUpdatePeriodicHandler { public: explicit EffectUpdatePeriodicHandlerFunction(AuraEffectUpdatePeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : AuraScript::EffectUpdatePeriodicHandler((AuraScript::AuraEffectUpdatePeriodicFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class EffectCalcAmountHandlerFunction : public AuraScript::EffectCalcAmountHandler { public: explicit EffectCalcAmountHandlerFunction(AuraEffectCalcAmountFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : AuraScript::EffectCalcAmountHandler((AuraScript::AuraEffectCalcAmountFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class EffectCalcPeriodicHandlerFunction : public AuraScript::EffectCalcPeriodicHandler { public: explicit EffectCalcPeriodicHandlerFunction(AuraEffectCalcPeriodicFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : AuraScript::EffectCalcPeriodicHandler((AuraScript::AuraEffectCalcPeriodicFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class EffectCalcSpellModHandlerFunction : public AuraScript::EffectCalcSpellModHandler { public: explicit EffectCalcSpellModHandlerFunction(AuraEffectCalcSpellModFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName) : AuraScript::EffectCalcSpellModHandler((AuraScript::AuraEffectCalcSpellModFnType)_pEffectHandlerScript, _effIndex, _effName) { } }; \
        class EffectApplyHandlerFunction : public AuraScript::EffectApplyHandler { public: explicit EffectApplyHandlerFunction(AuraEffectApplicationModeFnType _pEffectHandlerScript, uint8 _effIndex, uint16 _effName, AuraEffectHandleModes _mode) : AuraScript::EffectApplyHandler((AuraScript::AuraEffectApplicationModeFnType)_pEffectHandlerScript, _effIndex, _effName, _mode) { } }; \
        class EffectAbsorbFunction : public AuraScript::EffectAbsorbHandler { public: explicit EffectAbsorbFunction(AuraEffectAbsorbFnType _pEffectHandlerScript, uint8 _effIndex) : AuraScript::EffectAbsorbHandler((AuraScript::AuraEffectAbsorbFnType)_pEffectHandlerScript, _effIndex) { } }; \
        class EffectManaShieldFunction : public AuraScript::EffectManaShieldHandler { public: explicit EffectManaShieldFunction(AuraEffectAbsorbFnType _pEffectHandlerScript, uint8 _effIndex) : AuraScript::EffectManaShieldHandler((AuraScript::AuraEffectAbsorbFnType)_pEffectHandlerScript, _effIndex) { } }; \
        class EffectSplitFunction : public AuraScript::EffectSplitHandler { public: explicit EffectSplitFunction(AuraEffectSplitFnType _pEffectHandlerScript, uint8 _effIndex) : AuraScript::EffectSplitHandler((AuraScript::AuraEffectSplitFnType)_pEffectHandlerScript, _effIndex) { } }; \
        class CheckProcHandlerFunction : public AuraScript::CheckProcHandler { public: explicit CheckProcHandlerFunction(AuraCheckProcFnType handlerScript) : AuraScript::CheckProcHandler((AuraScript::AuraCheckProcFnType)handlerScript) { } }; \
        class CheckEffectProcHandlerFunction : public AuraScript::CheckEffectProcHandler { public: explicit CheckEffectProcHandlerFunction(AuraCheckEffectProcFnType handlerScript, uint8 effIndex, uint16 effName) : AuraScript::CheckEffectProcHandler((AuraScript::AuraCheckEffectProcFnType)handlerScript, effIndex, effName) { } }; \
        class AuraProcHandlerFunction : public AuraScript::AuraProcHandler { public: explicit AuraProcHandlerFunction(AuraProcFnType handlerScript) : AuraScript::AuraProcHandler((AuraScript::AuraProcFnType)handlerScript) { } }; \
        class EffectProcHandlerFunction : public AuraScript::EffectProcHandler { public: explicit EffectProcHandlerFunction(AuraEffectProcFnType effectHandlerScript, uint8 effIndex, uint16 effName) : AuraScript::EffectProcHandler((AuraScript::AuraEffectProcFnType)effectHandlerScript, effIndex, effName) { } }

        #define PrepareAuraScript(CLASSNAME) AURASCRIPT_FUNCTION_TYPE_DEFINES(CLASSNAME) AURASCRIPT_FUNCTION_CAST_DEFINES(CLASSNAME)

    public:
        AuraScript();
        ~AuraScript();
        bool _Validate(SpellInfo const* entry) override;
        bool _Load(Aura* aura);
        void _PrepareScriptCall(AuraScriptHookType hookType, AuraApplication const* aurApp = nullptr);
        void _FinishScriptCall();
        bool _IsDefaultActionPrevented();
    private:
        Aura* m_aura;
        AuraApplication const* m_auraApplication;
        bool m_defaultActionPrevented;

        class TC_GAME_API ScriptStateStore
        {
        public:
            AuraApplication const* _auraApplication;
            uint8 _currentScriptState;
            bool _defaultActionPrevented;
            ScriptStateStore(uint8 currentScriptState, AuraApplication const* auraApplication, bool defaultActionPrevented)
                : _auraApplication(auraApplication), _currentScriptState(currentScriptState), _defaultActionPrevented(defaultActionPrevented)
            { }
        };
        typedef std::stack<ScriptStateStore> ScriptStateStack;
        ScriptStateStack m_scriptStates;

    public:
        //
        // AuraScript interface
        // hooks to which you can attach your functions
        //
        // executed when area aura checks if it can be applied on target
        // example: OnEffectApply += AuraEffectApplyFn(class::function);
        // where function is: bool function (Unit* target);
        HookList<CheckAreaTargetHandler> DoCheckAreaTarget;
        #define AuraCheckAreaTargetFn(F) CheckAreaTargetFunction(&F)

        // executed when aura is dispelled by a unit
        // example: OnDispel += AuraDispelFn(class::function);
        // where function is: void function (DispelInfo* dispelInfo);
        HookList<AuraDispelHandler> OnDispel;
        // executed after aura is dispelled by a unit
        // example: AfterDispel += AuraDispelFn(class::function);
        // where function is: void function (DispelInfo* dispelInfo);
        HookList<AuraDispelHandler> AfterDispel;
        #define AuraDispelFn(F) AuraDispelFunction(&F)

        // executed when aura effect is applied with specified mode to target
        // should be used when when effect handler preventing/replacing is needed, do not use this hook for triggering spellcasts/removing auras etc - may be unsafe
        // example: OnEffectApply += AuraEffectApplyFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier, AuraEffectHandleModes);
        // where function is: void function(AuraEffect const* aurEff, AuraEffectHandleModes mode);
        HookList<EffectApplyHandler> OnEffectApply;
        // executed after aura effect is applied with specified mode to target
        // example: AfterEffectApply += AuraEffectApplyFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier, AuraEffectHandleModes);
        // where function is: void function(AuraEffect const* aurEff, AuraEffectHandleModes mode);
        HookList<EffectApplyHandler> AfterEffectApply;
        #define AuraEffectApplyFn(F, I, N, M) EffectApplyHandlerFunction(&F, I, N, M)

        // executed after aura effect is removed with specified mode from target
        // should be used when effect handler preventing/replacing is needed, do not use this hook for triggering spellcasts/removing auras etc - may be unsafe
        // example: OnEffectRemove += AuraEffectRemoveFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier, AuraEffectHandleModes);
        // where function is: void function(AuraEffect const* aurEff, AuraEffectHandleModes mode);
        HookList<EffectApplyHandler> OnEffectRemove;
        // executed when aura effect is removed with specified mode from target
        // example: AfterEffectRemove += AuraEffectRemoveFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier, AuraEffectHandleModes);
        // where function is: void function(AuraEffect const* aurEff, AuraEffectHandleModes mode);
        HookList<EffectApplyHandler> AfterEffectRemove;
        #define AuraEffectRemoveFn(F, I, N, M) EffectApplyHandlerFunction(&F, I, N, M)

        // executed when periodic aura effect ticks on target
        // example: OnEffectPeriodic += AuraEffectPeriodicFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect const* aurEff);
        HookList<EffectPeriodicHandler> OnEffectPeriodic;
        #define AuraEffectPeriodicFn(F, I, N) EffectPeriodicHandlerFunction(&F, I, N)

        // executed when periodic aura effect is updated
        // example: OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect* aurEff);
        HookList<EffectUpdatePeriodicHandler> OnEffectUpdatePeriodic;
        #define AuraEffectUpdatePeriodicFn(F, I, N) EffectUpdatePeriodicHandlerFunction(&F, I, N)

        // executed when aura effect calculates amount
        // example: DoEffectCalcAmount += AuraEffectCalcAmounFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect* aurEff, int32& amount, bool& canBeRecalculated);
        HookList<EffectCalcAmountHandler> DoEffectCalcAmount;
        #define AuraEffectCalcAmountFn(F, I, N) EffectCalcAmountHandlerFunction(&F, I, N)

        // executed when aura effect calculates periodic data
        // example: DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect const* aurEff, bool& isPeriodic, int32& amplitude);
        HookList<EffectCalcPeriodicHandler> DoEffectCalcPeriodic;
        #define AuraEffectCalcPeriodicFn(F, I, N) EffectCalcPeriodicHandlerFunction(&F, I, N)

        // executed when aura effect calculates spellmod
        // example: DoEffectCalcSpellMod += AuraEffectCalcSpellModFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect const* aurEff, SpellModifier*& spellMod);
        HookList<EffectCalcSpellModHandler> DoEffectCalcSpellMod;
        #define AuraEffectCalcSpellModFn(F, I, N) EffectCalcSpellModHandlerFunction(&F, I, N)

        // executed when absorb aura effect is going to reduce damage
        // example: OnEffectAbsorb += AuraEffectAbsorbFn(class::function, EffectIndexSpecifier);
        // where function is: void function(AuraEffect const* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount);
        HookList<EffectAbsorbHandler> OnEffectAbsorb;
        #define AuraEffectAbsorbFn(F, I) EffectAbsorbFunction(&F, I)

        // executed after absorb aura effect reduced damage to target - absorbAmount is real amount absorbed by aura
        // example: AfterEffectAbsorb += AuraEffectAbsorbFn(class::function, EffectIndexSpecifier);
        // where function is: void function(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount);
        HookList<EffectAbsorbHandler> AfterEffectAbsorb;

        // executed when mana shield aura effect is going to reduce damage
        // example: OnEffectManaShield += AuraEffectManaShieldFn(class::function, EffectIndexSpecifier);
        // where function is: void function(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount);
        HookList<EffectManaShieldHandler> OnEffectManaShield;
        #define AuraEffectManaShieldFn(F, I) EffectManaShieldFunction(&F, I)

        // executed after mana shield aura effect reduced damage to target - absorbAmount is real amount absorbed by aura
        // example: AfterEffectManaShield += AuraEffectManaShieldFn(class::function, EffectIndexSpecifier);
        // where function is: void function(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& absorbAmount);
        HookList<EffectManaShieldHandler> AfterEffectManaShield;

        // executed when the caster of some spell with split dmg aura gets damaged through it
        // example: OnEffectSplit += AuraEffectSplitFn(class::function, EffectIndexSpecifier);
        // where function is: void function(AuraEffect* aurEff, DamageInfo& dmgInfo, uint32& splitAmount);
        HookList<EffectSplitHandler> OnEffectSplit;
        #define AuraEffectSplitFn(F, I) EffectSplitFunction(&F, I)

        // executed when aura checks if it can proc
        // example: DoCheckProc += AuraCheckProcFn(class::function);
        // where function is: bool function(ProcEventInfo& eventInfo);
        HookList<CheckProcHandler> DoCheckProc;
        #define AuraCheckProcFn(F) CheckProcHandlerFunction(&F)

        // executed when aura effect checks if it can proc the aura
        // example: DoCheckEffectProc += AuraCheckEffectProcFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is bool function(AuraEffect const* aurEff, ProcEventInfo& eventInfo);
        HookList<CheckEffectProcHandler> DoCheckEffectProc;
        #define AuraCheckEffectProcFn(F, I, N) CheckEffectProcHandlerFunction(&F, I, N)

        // executed before aura procs (possibility to prevent charge drop/cooldown)
        // example: DoPrepareProc += AuraProcFn(class::function);
        // where function is: void function(ProcEventInfo& eventInfo);
        HookList<AuraProcHandler> DoPrepareProc;
        // executed when aura procs
        // example: OnProc += AuraProcFn(class::function);
        // where function is: void function(ProcEventInfo& eventInfo);
        HookList<AuraProcHandler> OnProc;
        // executed after aura proced
        // example: AfterProc += AuraProcFn(class::function);
        // where function is: void function(ProcEventInfo& eventInfo);
        HookList<AuraProcHandler> AfterProc;
        #define AuraProcFn(F) AuraProcHandlerFunction(&F)

        // executed when aura effect procs
        // example: OnEffectProc += AuraEffectProcFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect const* aurEff, ProcEventInfo& procInfo);
        HookList<EffectProcHandler> OnEffectProc;
        // executed after aura effect proced
        // example: AfterEffectProc += AuraEffectProcFn(class::function, EffectIndexSpecifier, EffectAuraNameSpecifier);
        // where function is: void function(AuraEffect const* aurEff, ProcEventInfo& procInfo);
        HookList<EffectProcHandler> AfterEffectProc;
        #define AuraEffectProcFn(F, I, N) EffectProcHandlerFunction(&F, I, N)

        // AuraScript interface - hook/effect execution manipulators

        // prevents default action of a hook from being executed (works only while called in a hook which default action can be prevented)
        void PreventDefaultAction();

        // AuraScript interface - functions which are redirecting to Aura class

        // returns proto of the spell
        SpellInfo const* GetSpellInfo() const;
        SpellEffectInfo const& GetEffectInfo(SpellEffIndex effIndex) const;
        // returns spellid of the spell
        uint32 GetId() const;

        // returns guid of object which cast the aura (m_originalCaster of the Spell class)
        ObjectGuid GetCasterGUID() const;
        // returns unit which cast the aura or NULL if not avalible (caster logged out for example)
        Unit* GetCaster() const;
        // returns gameobject which cast the aura or NULL if not available
        GameObject* GetGObjCaster() const;
        // returns object on which aura was cast, target for non-area auras, area aura source for area auras
        WorldObject* GetOwner() const;
        // returns owner if it's unit or unit derived object, NULL otherwise (only for persistent area auras NULL is returned)
        Unit* GetUnitOwner() const;
        // returns owner if it's dynobj, NULL otherwise
        DynamicObject* GetDynobjOwner() const;

        // removes aura with remove mode (see AuraRemoveMode enum)
        void Remove(AuraRemoveMode removeMode = AURA_REMOVE_BY_DEFAULT);
        // returns aura object of script
        Aura* GetAura() const;

        // returns type of the aura, may be dynobj owned aura or unit owned aura
        AuraObjectType GetType() const;

        // aura duration manipulation - when duration goes to 0 aura is removed
        int32 GetDuration() const;
        void SetDuration(int32 duration, bool withMods = false);
        // sets duration to maxduration
        void RefreshDuration();
        time_t GetApplyTime() const;
        int32 GetMaxDuration() const;
        void SetMaxDuration(int32 duration);
        int32 CalcMaxDuration() const;
        // expired - duration just went to 0
        bool IsExpired() const;
        // permament - has infinite duration
        bool IsPermanent() const;

        // charges manipulation - 0 - not charged aura
        uint8 GetCharges() const;
        void SetCharges(uint8 charges);
        uint8 CalcMaxCharges() const;
        bool ModCharges(int8 num, AuraRemoveMode removeMode = AURA_REMOVE_BY_DEFAULT);
        // returns true if last charge dropped
        bool DropCharge(AuraRemoveMode removeMode = AURA_REMOVE_BY_DEFAULT);

        // stack amount manipulation
        uint8 GetStackAmount() const;
        void SetStackAmount(uint8 num);
        bool ModStackAmount(int32 num, AuraRemoveMode removeMode = AURA_REMOVE_BY_DEFAULT);

        // passive - "working in background", not saved, not removed by immunities, not seen by player
        bool IsPassive() const;
        // death persistent - not removed on death
        bool IsDeathPersistent() const;

        // check if aura has effect of given effindex
        bool HasEffect(uint8 effIndex) const;
        // returns aura effect of given effect index or NULL
        AuraEffect* GetEffect(uint8 effIndex) const;

        // check if aura has effect of given aura type
        bool HasEffectType(AuraType type) const;

        // AuraScript interface - functions which are redirecting to AuraApplication class
        // Do not call these in hooks in which AuraApplication is not avalible, otherwise result will differ from expected (the functions will return NULL)

        // returns currently processed target of an aura
        // Return value does not need to be NULL-checked, the only situation this will (always)
        // return NULL is when the call happens in an unsupported hook, in other cases, it is always valid
        Unit* GetTarget() const;
        // returns AuraApplication object of currently processed target
        AuraApplication const* GetTargetApplication() const;
};

//
// definitions:
//
// EffectIndexSpecifier - specifies conditions for effects
// EFFECT_0 - first effect matches
// EFFECT_1 - second effect matches
// EFFECT_2 - third effect matches
// EFFECT_FIRST_FOUND - first effect matching other conditions matches
// EFFECT_ALL - all effects of spell match
//
// EffectNameSpecifier - specifies conditions for spell effect names
// SPELL_EFFECT_ANY - any effect but not 0 matches condition
// SPELL_EFFECT_XXX - one of values of enum SpellEffects - effect with equal name matches
//

#endif // __SPELL_SCRIPT_H
