/*
 * Faim et soif - unit hooks.
 *
 * Eating and drinking are credited through the aura apply hook so the manager
 * can restore satiety/hydration without the module inspecting spells itself.
 * Deaths are only interesting when the manager flagged the pending cause:
 * the snapshot is then handed to the manager for logging.
 *
 * Released under GNU GPL v2; redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HungerThirst.h"
#include "Player.h"
#include "ScriptMgr.h"

namespace HungerThirst
{
namespace
{
class HungerThirst_UnitScript : public UnitScript
{
public:
    HungerThirst_UnitScript() : UnitScript("HungerThirst_UnitScript", true,
        {
            UNITHOOK_ON_AURA_APPLY,
            UNITHOOK_ON_UNIT_DEATH
        })
    {
    }

    void OnAuraApply(Unit* unit, Aura* aura) override
    {
        if (unit->IsPlayer())
            CreditConsumable(unit->ToPlayer(), aura);
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        if (!unit->IsPlayer())
            return;

        Player* player = unit->ToPlayer();
        DeathCause cause = ConsumePendingDeathCause();
        if (cause == DeathCause::None || player->IsAlive())
            return;

        Snapshot snapshot;
        if (GetSnapshot(player, snapshot))
            LogDeath(player, cause, snapshot);
    }
};
}

void AddHungerThirstUnitScript()
{
    new HungerThirst_UnitScript();
}
}
