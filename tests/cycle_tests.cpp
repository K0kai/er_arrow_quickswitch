#include "../src/cycle.hpp"
#include <cassert>
#include <cstdio>
int main() {
    using arrow_switch::Arrow;
    using arrow_switch::next;
    std::vector<Arrow> a={{10,1,30,0,81},{20,2,0,1,81},{30,3,50,2,83},{40,4,5,3,81}};
    assert(next(a,10)->id==40); // skip empty and incompatible ammo
    assert(next(a,40)->id==10); // wrap
    assert(next(a,20)->id==40); // depleted current stack
    assert(next(a,30)==nullptr); // only one greatarrow
    assert(next(a,10,1,0xFFFFFFFFu,false)->id==30); // include greatarrows when configured
    assert(next(a,30,1,0xFFFFFFFFu,false)->id==40); // return to normal arrows
    assert(next(a,99)==nullptr); // unknown current ammunition
    assert(next({},10)==nullptr);
    assert(next({{10,1,1,0,81}},10)==nullptr);
    assert(arrow_switch::is_arrow_type(81));
    assert(arrow_switch::is_arrow_type(83));
    assert(!arrow_switch::is_arrow_type(85));
    assert(!arrow_switch::is_arrow_type(86));
    assert(!arrow_switch::is_arrow_type(1));
    assert(arrow_switch::plausible_param_name_offset(0x334350));
    assert(!arrow_switch::plausible_param_name_offset(0));
    assert(!arrow_switch::plausible_param_name_offset(0x4000001));
    std::puts("7 cycle cases and 8 parameter/classification regression checks passed");
}
