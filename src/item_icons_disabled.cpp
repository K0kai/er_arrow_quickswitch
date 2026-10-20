// Diagnostic replacement linked only by build.cmd --test-no-icons.
// Normal builds continue to use item_icons.cpp and the archive/decoder sources.
#include "item_icons.hpp"
namespace arrow_switch::item_icons {
void initialize(const std::filesystem::path&,Logger logger){
    if(logger)logger("Item artwork extraction is excluded from this diagnostic build.");
}
void set_override(const std::filesystem::path&){}
std::shared_ptr<const Image> request(unsigned){return {};}
void prefetch(const std::vector<unsigned>&){}
std::uint64_t revision(){return 0;}
std::shared_ptr<const Image> extract(const std::filesystem::path&,const std::filesystem::path&,unsigned,Logger){return {};}
}
