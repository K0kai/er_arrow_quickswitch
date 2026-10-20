#include "../src/ammo_effects.hpp"
#include <cassert>
#include <string>

int main(){
    using namespace arrow_switch::effects;
    Evidence spiritflame;
    spiritflame.damage[4]=30; // Convergence EquipParamWeapon 50240000.
    spiritflame.buildup[4]=65; // Its SpEffect 6702.
    const auto mask=classify(spiritflame);
    assert(mask==(bit(holy)|bit(frost)));
    assert(label(mask)==L"HOLY + FROST");
    assert((mask&bit(fire))==0);
    Evidence poisonbone;
    poisonbone.damage[0]=25;poisonbone.buildup[0]=60;
    assert(classify(poisonbone)==(bit(physical)|bit(poison)));
    assert(label(classify(poisonbone))==L"POISON + PHYSICAL");
    Evidence plain;plain.damage[0]=45;
    assert(label(classify(plain))==L"PHYSICAL");
}
