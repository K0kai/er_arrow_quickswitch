#pragma once
#include <cstddef>
#include <cstdint>

namespace arrow_switch::list_navigation {
// Positive steps advance through the full inventory; pages are only a view.
struct Cursor {
    int index=-1;
    bool navigated=false;
    void open(int equippedIndex=-1){index=equippedIndex;navigated=false;}
    void move(int steps,std::size_t count){
        if(!count){index=-1;navigated=false;return;}
        if(!steps)return;
        const auto total=static_cast<std::int64_t>(count);
        const auto start=index<0?(steps>0?-1LL:0LL):static_cast<std::int64_t>(index);
        auto next=(start+steps)%total;
        if(next<0)next+=total;
        index=static_cast<int>(next);navigated=true;
    }
};
}
