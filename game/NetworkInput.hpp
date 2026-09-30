#pragma once
#include "Types.hpp"
#include <array>
#include <cmath>
namespace th09 {
// Ordered input exchange. A simulation frame exists only after both peers
// supplied its keys; waiting never substitutes predicted gameplay inputs.
class NetworkInput {
public:struct Motion {bool enabled=false;float x=0,y=0;};
private:
    struct Slot {u32 frame=0;u16 keys[2]{};u8 present=0;Motion motion[2];};
    static constexpr u32 capacity=512;
    std::array<Slot,capacity> slots{};u32 current=0,next_local=0,next_remote=0,lead=6;
public:
    i32 side=0;bool active=false,failed=false;
    void begin(i32 player,u32 delay=6){slots={};current=0;lead=delay;next_local=next_remote=delay;side=player;active=player>=0&&player<2&&delay>0&&delay<60;failed=!active;for(u32 n=0;active&&n<delay;++n)slots[n]={n,{0,0},3};}
    void end(){active=false;slots={};}
    u32 frame()const{return current;}
    bool wants_input()const{return active&&!failed&&next_local<=current+lead;}
    u32 sending_frame()const{return next_local;}
    bool submit(i32 player,u32 frame,u16 keys,bool moving=false,float x=0,float y=0){if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>16||std::abs(y)>16){failed=true;return false;}if(!active||failed||player<0||player>1)return false;auto& expected=player==side?next_local:next_remote;if(frame!=expected||frame<current||frame-current>=capacity){failed=true;return false;}auto& s=slots[frame%capacity];if(s.present&&s.frame!=frame){failed=true;return false;}s.frame=frame;s.keys[player]=keys;s.motion[player]={moving,x,y};s.present|=u8(1u<<player);++expected;return true;}
    bool take(u16 (&keys)[3],Motion* motion=nullptr){if(!active||failed)return false;auto& s=slots[current%capacity];if(s.frame!=current||s.present!=3)return false;keys[0]=s.keys[0];keys[1]=s.keys[1];keys[2]=keys[0]|keys[1];if(motion){motion[0]=s.motion[0];motion[1]=s.motion[1];}s={};++current;return true;}
};
}
