#include "KeyboardInput.hpp"
namespace th09 {
u16 keyboard_input(const bool (&keys)[256],u32 device){
    struct Binding {u8 key;u16 mask;};
    constexpr Binding common[]={{13,4096},{17,256},{27,8},{36,2048},{68,8192},{80,2048},{81,512},{82,16384},{83,1024}};
    constexpr Binding all[]={{16,4},{37,64},{38,16},{39,128},{40,32},{88,2},{90,1},{97,96},{98,32},{99,160},{100,64},{102,128},{103,80},{104,16},{105,144}};
    constexpr Binding left[]={{16,4},{66,32},{70,64},{72,128},{78,160},{82,80},{84,16},{86,96},{88,2},{89,144},{90,1}};
    constexpr Binding right[]={{37,4},{39,2},{40,1},{97,96},{98,32},{99,160},{100,64},{102,128},{103,80},{104,16},{105,144}};
    u16 result=0;for(const auto& b:common)if(keys[b.key])result|=b.mask;
    const auto add=[&](const auto& list){for(const auto& b:list)if(keys[b.key])result|=b.mask;};
    if(device==2)add(all);else if(device==3)add(left);else if(device==4)add(right);return result;
}
}
