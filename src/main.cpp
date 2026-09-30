// Touhou 9 for Nintendo Switch: native entry point.
//
// The Application struct is th09_web/cpp/sdl/Application.cpp with the
// browser removed: the same session, title, menu, ending, replay and score
// wiring. Host changes: files live on the SD card instead of IDBFS, the 60 Hz
// loop is driven by vsync instead of requestAnimationFrame, Switch
// controllers stand in for the two DirectInput pads (so local 2P versus
// works with two controllers), and browser netplay is not available.
#include "Assets.hpp"
#include "AudioDevice.hpp"
#include "FontDevice.hpp"
#include "../game/GamePresentation.hpp"
#include "../game/TitleMenus.hpp"
#include "../game/GameSession.hpp"
#include "../game/MusicCatalog.hpp"
#include "../game/GameConfiguration.hpp"
#include "../game/KeyboardInput.hpp"
#include "../game/NetworkInput.hpp"
#include <dirent.h>
#include <ctime>
#include <algorithm>
#include "../portable/input/TouchController.hpp"
#include "../portable/sdl/FrameCadence.hpp"
#include "Platform.hpp"
#include <SDL.h>
#include <memory>
#include <cmath>
#ifdef __SWITCH__
#include <switch.h>
#endif
// Browser netplay hooks: the relay lives in the web launcher, so on Switch the
// network menu is inert and these are no-ops.
static void th09_network_result(){}
static void th09_network_request(){}
namespace th09::sdl {
namespace {
std::string save_path(const char* name){return host::paths().save_file(name);}
i32 joy_button(i32);
NetworkInput network;
struct Application final:GameMedia,InGameMenuServices,TitleServices,EndingServices {
    Assets assets;GraphicsDevice graphics;FontDevice fonts{graphics};AudioDevice audio;
    EclWorldState state;AnmExecutor executor{state.random};GameResources resources{assets,graphics,executor};
    GamePresentation presentation{graphics,resources,*this};InGameMenus menus{resources,*this};
    std::array<u8,2> local_focus{};i32 local_versus=1;
    void release_network(){network.end();settings.auto_focus=local_focus;settings.versus=local_versus;sync_records();}
    PlayerRecords records;bool clock_running=true,host_music_enabled=true;TitleSettings settings;GameConfiguration saved_configuration;u32 storage_revision=0;std::vector<u8> import_bytes;std::unique_ptr<TitleMenus> title;bool in_title=false,launch_pending=false;std::map<std::string,u32> title_images;u32 background_image=0;
    std::unique_ptr<GameSession> session;GameWorld* world=nullptr;WorldConfiguration configuration;InputFrame device;
    ReplayFile pending_replay;u32 replay_stage=0;bool replay_pending=false,demo_pending=false;u32 ending_image=0,ending_width=640,ending_height=480;
    bool paused=false,over=false,complete=false;u32 frames=0;std::string error;Vec2 shake[3]{};
    bool initialize_platform(){if(!graphics.initialize()){error=graphics.error;return false;}if(!assets.open(host::paths().data_file("th09.dat").c_str())){error=assets.error;return false;}if(!fonts.initialize()){error=fonts.error;return false;}fonts.prewarm_game(assets);if(!audio.initialize(assets)){error=audio.error;return false;}host::make_directory(host::paths().save);host::make_directory(save_path("replay"));std::vector<u8> bytes;if(read_file(save_path("th09.cfg").c_str(),bytes))saved_configuration.load(bytes.data(),u32(bytes.size()));saved_configuration.apply(settings);title_configuration();if(!write_file(save_path("th09.cfg").c_str(),saved_configuration.data().data(),204)){error="Unable to save configuration";return false;}if(read_file(save_path("score.dat").c_str(),bytes))records.load(bytes.data(),u32(bytes.size()));sync_records();records.application_clock=records.game_clock=u32(SDL_GetTicks());return true;}
    void update_clocks(){const u32 now=u32(SDL_GetTicks());if(clock_running)records.update_application_clock(now);else records.application_clock=now;if(clock_running&&!in_title&&!paused&&!over&&!complete&&session&&session->phase==SessionPhase::match)records.update_game_clock(now);else records.game_clock=now;}
    void clock_pause(bool on){update_clocks();clock_running=!on;}
    bool open_title(){if(!initialize_platform())return false;state.random={0x7531,0,0};title=std::make_unique<TitleMenus>(resources,*this,state.random,settings);if(!title->initialize()||!resources.load(AnimationFile::ascii,"ascii.anm")||!presentation.ascii.initialize()){error="Title initialization "+title->error+resources.error;return false;}in_title=true;return true;}
    bool open(i32 left,i32 right,i32 mode,i32 diff){
        if(!initialize_platform())return false;
        state.random={0x7531,0,0};configuration.selection.characters[0]=left;configuration.selection.characters[1]=right;configuration.selection.mode=GameMode(mode);configuration.selection.difficulty=diff;configuration.selection.lives=2;configuration.selection.selector=left;
        configuration.controllers[1]=1;return start();
    }
    bool start(){
        presentation.renderer.flush();presentation.world=nullptr;world=nullptr;session.reset();executor.invalid=false;
        session=std::make_unique<GameSession>(state,resources,executor,presentation,*this,records);
        bool ok=false;if(replay_pending){replay_pending=false;ok=session->play(pending_replay,replay_stage,demo_pending);}
        else {ReplayMetadata meta;meta.configuration=saved_configuration.data();meta.mode=u8(configuration.selection.mode);meta.difficulty=u8(configuration.selection.difficulty);// Native replays describe the two controller types; web transport is not
            // part of the recorded simulation. Network play is human vs human.
            meta.versus=u8(settings.versus==4?0:settings.versus);for(i32 side=0;side<2;++side){meta.health[side]=u8(configuration.health[side]);meta.alternate[side]=configuration.alternate[side];meta.configuration[0xb4+side]=configuration.automatic_focus[side];}meta.configuration[0xac]=u8(configuration.starting_extra_lives);
            const auto now=std::time(nullptr);char date[10]="--/--/--";if(const auto* local=std::localtime(&now))std::strftime(date,sizeof(date),"%y/%m/%d",local);ok=session->begin(configuration,meta,date);}
        if(!ok){error=session->error;return false;}world=session->world.get();presentation.world=world;
        if(!presentation.ascii.initialize()){error="ASCII initialization failed";return false;}paused=over=complete=false;menus.pause={};menus.game_over={};menus.match_end={};requested_transition=-1;return true;
    }
    bool render_text(AnmVm& a,const char* s,u32 color,u32 shadow)override{if(!fonts.text(a,s,color,shadow)){error=fonts.error;return false;}return true;}
    void sound(i32 id,i32 pan)override{audio.effects.enqueue(id,pan);}
    void positioned_sound(i32 id,float x)override{audio.effects.positioned(id,x);}
    void music(i32 n)override{if(!audio.music(n))error=audio.error;else if(const auto* entry=music_track(n))if(!session||!session->is_replay)records.music_unlocked[entry->unlock]=1;}
    void fade_music()override{audio.fade_music();}
    void encountered(i32 c)override{if(!session||!session->is_replay)records.count_encounter(c);}
    void defeated(i32 c)override{if(c>=0&&c<16&&(!session||!session->is_replay))records.versus_unlocked[c]=1;}
    void overlay()override{if(paused)menus.draw_pause();if(over)menus.draw_game_over();if(complete)menus.draw_match_end();}
    void menu_sound(i32 n)override{sound(n,0);}
    void menu_action(InGameAction action)override{
        if(action==InGameAction::resume){paused=false;world->paused=false;audio.pause_music(false);}
        else {pending_action=i32(action);}
    }
    void menu_view()override{presentation.begin_field(2);}
    void menu_sprite(AnmVm& a)override{presentation.renderer.draw_no_rotation(a);}
    i32 pending_action=-1;
    bool tick_title(u16 left,u16 right,u16 keys,bool render=true){
        update_clocks();device.update(keys);left_device.update(left);right_device.update(right);if(!title||!error.empty())return false;
        title->update(left_device,right_device,device);if(network.active&&title->state.screen==TitleScreen::versus_type){release_network();title->state={};th09_network_result();}save_configuration();if(!title->error.empty()){error=title->error;return false;}
        if(launch_pending){launch_pending=false;in_title=false;if(!start())return false;}
        else{if(render)draw();audio.update();audio.pump();++frames;}return error.empty();
    }
    InputFrame left_device,right_device;
    bool title_background(const char* name)override{const auto it=title_images.find(name);if(it!=title_images.end()){background_image=it->second;return true;}std::vector<u8> bytes;if(!assets.read(name,bytes)){error="Missing title image "+std::string(name);return false;}const auto texture=graphics.image(bytes.data(),u32(bytes.size()));if(!texture.handle){error=graphics.error;return false;}background_image=title_images[name]=texture.handle;return true;}
    void title_sound(i32 id)override{sound(id,0);}
    void title_music(i32 id)override{music(id);}
    void title_music_file(const char* name)override{if(!audio.music_file(name))error=audio.error;}
    void title_music_pause(bool p)override{audio.pause_music(p);}
    void title_music_fade()override{audio.fade_music();}
    void title_text(AnmVm& a,const char* s,u32 c,u32 shadow)override{render_text(a,s,c,shadow);}
    void title_sprite(AnmVm& a,bool rotate)override{if(rotate)presentation.renderer.draw_2d(a);else presentation.renderer.draw_no_rotation(a);}
    void title_begin_draw()override{
        presentation.begin_field(2);presentation.renderer.flush();if(!background_image)return;PipelineState pipeline;pipeline.depthTest=false;pipeline.depthWrite=false;pipeline.fog=false;pipeline.blend=false;pipeline.color.operation=pipeline.alpha.operation=ColorOperation::First;pipeline.color.first=pipeline.alpha.first={ArgumentSource::Texture};
        const SpriteVertex corners[4]={{{-.5f,-.5f,0},1,0xffffffff,{0,0}},{{639.5f,-.5f,0},1,0xffffffff,{1,0}},{{-.5f,479.5f,0},1,0xffffffff,{0,1}},{{639.5f,479.5f,0},1,0xffffffff,{1,1}}};graphics.draw(pipeline,background_image,Topology::Strip,VertexLayout::ScreenColorUv,corners,4);
    }
    void title_configuration()override{audio.music_volume=settings.music_volume;audio.music_enabled=host_music_enabled&&settings.music_mode!=0;audio.effects.enabled=settings.effects!=0;audio.effects.master_volume=settings.sound_volume;audio.refresh_volume();}
    bool write_file(const char* path,const u8* bytes,u32 size){const bool ok=host::write_file(path,bytes,size);if(ok)++storage_revision;return ok;}
    void save_configuration(){if(network.active)return;if(saved_configuration.capture(settings)){title_configuration();const auto& bytes=saved_configuration.data();if(!write_file(save_path("th09.cfg").c_str(),bytes.data(),u32(bytes.size())))error="Unable to save th09.cfg";}}
    u32 title_clear_count(i32 c,i32 d)override{return records.clear_count(c,d);}
    PlayerRecords& title_records()override{return records;}
    void title_save_records()override{update_clocks();auto local_random=state.random;auto bytes=records.save(network.active?local_random:state.random);if(!write_file(save_path("score.dat").c_str(),bytes.data(),u32(bytes.size())))error="Unable to save score.dat";}
    void title_ascii(const Vec3& p,const char* text,u32 color,const Vec2& scale)override{presentation.ascii.color=color;presentation.ascii.scale=scale;presentation.ascii.field_view=0;presentation.ascii.text(p,text);}
    i32 title_joy_button(i32 device)override{return joy_button(device);}
    static std::string replay_path(const char* name){std::string path=name?name:"";if(path.rfind("./",0)==0)path.erase(0,2);if(path.rfind("replay/",0)!=0||path.find("..")!=std::string::npos||path.find('\\')!=std::string::npos||path.find('/',7)!=std::string::npos||path.size()>64)return {};return save_path(path.c_str());}
    bool title_read_replay(const char* name,std::vector<u8>& bytes)override{const auto path=replay_path(name);return !path.empty()&&read_file(path.c_str(),bytes);}
    std::vector<std::string> title_imported_replays()override{std::vector<std::string> names;auto* dir=opendir(save_path("replay").c_str());if(dir){while(auto* entry=readdir(dir)){std::string n=entry->d_name;if(n.rfind("th9_ud",0)==0&&n.size()==14)names.push_back(n);}closedir(dir);}std::sort(names.begin(),names.end());return names;}
    bool title_save_replay(const char* name,const char* player)override{const auto path=replay_path(name);if(!session||path.empty())return false;auto replay=session->save_replay(player);const auto bytes=replay.encode();if(bytes.empty()){error=session->error;return false;}const bool ok=write_file(path.c_str(),bytes.data(),u32(bytes.size()));if(!ok)error="Unable to write replay";return ok;}
    bool title_replay_exists(const char* name)override{const auto path=replay_path(name);return !path.empty()&&host::exists(path);}
    void title_play_replay(const ReplayFile& file,u32 stage,const char*)override{pending_replay=file;replay_stage=stage;replay_pending=launch_pending=true;demo_pending=false;}
    void title_launch(const WorldConfiguration& c)override{configuration=c;configuration.starting_extra_lives=saved_configuration.extra_lives();for(i32 n=0;n<2;++n)configuration.automatic_focus[n]=settings.auto_focus[n]!=0;launch_pending=true;}
    void title_demo(u32 index)override{char name[24];std::snprintf(name,sizeof(name),"demorpy%u.rpy",index%3);std::vector<u8> bytes;if(!assets.read(name,bytes)||!pending_replay.decode(bytes.data(),u32(bytes.size()))){error="Invalid demonstration replay";return;}replay_stage=9;replay_pending=launch_pending=demo_pending=true;}
    void title_network()override{th09_network_request();}
    void title_exit()override{title_save_records();save_configuration();requested_transition=0;}
    void sync_records(){settings.versus_unlocked=records.versus_unlocked;settings.story_unlocked=records.story_unlocked;settings.extra_unlocked=records.extra_unlocked;settings.music_unlocked=records.music_unlocked;}
    bool return_title(bool score){
        presentation.renderer.flush();const bool was_network=network.active;if(was_network)release_network();sync_records();title=std::make_unique<TitleMenus>(resources,*this,state.random,settings);if(!title->initialize()){error=title->error;return false;}
        title->leaving=false;in_title=true;paused=over=complete=false;audio.pause_music(false);
        if(session){settings.game_flags=session->world?session->world->battle->state.flags:0;if(session->is_replay)score=false;if(session->is_demo())settings.game_flags&=~10u;if(!session->recordable)settings.game_flags|=0x2000;title->score_candidate=session->result;title->result_mode=GameMode(session->result.difficulty==4?1:session->world?i32(session->world->rules.mode):0);settings.difficulty=session->result.difficulty;}
        title->state.screen=score?TitleScreen::score_name:TitleScreen::main;title->state.selection=0;device=left_device=right_device={};presentation.ascii.clear_text();title_save_records();if(was_network)th09_network_result();return true;
    }
    bool ending_picture(const char* path)override{std::string name=path?path:"";const auto slash=name.find_last_of("/\\");if(slash!=std::string::npos)name.erase(0,slash+1);std::vector<u8> bytes;if(!assets.read(name.c_str(),bytes))return false;const auto image=graphics.image(bytes.data(),u32(bytes.size()));if(!image.handle)return false;presentation.renderer.flush();if(ending_image)graphics.destroy(ending_image);ending_image=image.handle;ending_width=image.width;ending_height=image.height;return true;}
    void ending_music(i32 track)override{if(track<0||audio.statistics()[5]!=u32(track))music(track);}
    void ending_music_fade(i32 seconds)override{audio.fade_music(seconds*60);}
    void ending_text(AnmVm& vm,const char* text,u32 color)override{render_text(vm,text,color,0);}
    void ending_background(i32 x,i32 y)override{
        presentation.begin_field(2);presentation.renderer.flush();if(!ending_image)return;
        PipelineState pipeline;pipeline.depthTest=pipeline.depthWrite=pipeline.fog=pipeline.blend=false;pipeline.color.operation=pipeline.alpha.operation=ColorOperation::First;pipeline.color.first=pipeline.alpha.first={ArgumentSource::Texture};
        const float u0=float(x)/ending_width,v0=float(y)/ending_height,u1=float(x+640)/ending_width,v1=float(y+480)/ending_height;
        const SpriteVertex quad[4]={{{-.5f,-.5f,0},1,0xffffffff,{u0,v0}},{{639.5f,-.5f,0},1,0xffffffff,{u1,v0}},{{-.5f,479.5f,0},1,0xffffffff,{u0,v1}},{{639.5f,479.5f,0},1,0xffffffff,{u1,v1}}};graphics.draw(pipeline,ending_image,Topology::Strip,VertexLayout::ScreenColorUv,quad,4);
    }
    void ending_sprite(AnmVm& vm)override{presentation.renderer.draw_2d(vm);}
    void ending_cover(u32 color)override{presentation.renderer.rectangle(0,0,640,480,color,color);}
    bool tick(u16 keys){return tick_inputs(keys,0,keys);}
    bool tick_inputs(u16 left,u16 right,u16 keys,bool render=true){
        if(in_title)return tick_title(left,right,keys,render);if(!session||!world||!error.empty())return false;update_clocks();device.update(keys);mode=world->rules.mode;difficulty=world->configuration.selection.difficulty;game_flags=world->battle->state.flags;continues=session->continues;
        PopupFrame popup;popup.paused=paused;popup.game_over=over;popup.game_flags=game_flags;for(i32 s=0;s<2;++s)popup.field_flags[s]=world->battle->fields[s].script.flags;presentation.ascii.update(popup);
        if(paused)menus.update_pause(device);else if(over)menus.update_game_over(device);else if(complete)menus.update_match_end(device);else if(!session->is_demo()&&(device.pressed&8)&&session->phase==SessionPhase::match){paused=world->paused=true;sound(34,0);audio.pause_music(true);}
        if(pending_action>=0){const auto action=InGameAction(pending_action);pending_action=-1;
            if(action==InGameAction::retry||action==InGameAction::replay_retry||action==InGameAction::restart_extra){presentation.renderer.flush();if(!session->retry()){error=session->error;return false;}world=session->world.get();presentation.world=world;paused=over=complete=false;menus.pause={};menus.game_over={};menus.match_end={};audio.pause_music(false);}
            else if(action==InGameAction::continue_match){audio.music(-1);if(!session->continue_game()){error="Unable to continue";return false;}paused=over=complete=false;audio.pause_music(false);}
            else {session->finish();return return_title(action==InGameAction::save_score);}
        }
        if(!paused&&!over&&!complete){presentation.renderer.flush();if(!session->update(left,right,keys)){error=session->error;return false;}world=session->world.get();presentation.world=world;}
        over=session->phase==SessionPhase::game_over;complete=session->phase==SessionPhase::match_complete;
        if(session->phase==SessionPhase::finished)return return_title(!session->is_replay);
        requested_transition=world->transition_pending?100+i32(world->transition):-1;
        if(render)draw();audio.update();audio.pump();session->warm_resources();++frames;return error.empty();
    }
    void draw(){presentation.begin_frame();if(in_title){title->draw();presentation.ascii.draw_text();presentation.ascii.clear_text();}else if(session->phase==SessionPhase::ending)session->ending->draw();else world->draw();presentation.finish_frame();graphics.present();}
    i32 requested_transition=-1;
    // Switch port: '−' saves the current 640x480 frame as a BMP into
    // snapshot/ next to the saves, like the PC game's snapshot folder.
    bool snapshot(){
        presentation.renderer.flush();graphics.backend.read(GraphicsDevice::screen);auto* image=graphics.pixels(GraphicsDevice::screen);if(!image)return false;
        host::make_directory(save_path("snapshot"));char file[64];std::string path;
        for(u32 n=0;n<1000;++n){std::snprintf(file,sizeof(file),"snapshot/th09_%03u.bmp",n);path=save_path(file);if(!host::exists(path))break;}
        const u32 w=image->width,h=image->height,row=w*3,pad=(4-row%4)%4,size=54+(row+pad)*h;std::vector<u8> bmp(size,0);
        auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)bmp[at+i]=u8(v>>(8*i));};
        bmp[0]='B';bmp[1]='M';put(2,size,4);put(10,54,4);put(14,40,4);put(18,w,4);put(22,h,4);put(26,1,2);put(28,24,2);put(34,size-54,4);
        for(u32 y=0;y<h;++y){const u8* src=image->pixels.data()+(h-1-y)*image->pitch;u8* dst=bmp.data()+54+y*(row+pad);for(u32 x=0;x<w;++x){dst[x*3]=src[x*4];dst[x*3+1]=src[x*4+1];dst[x*3+2]=src[x*4+2];}}
        const bool ok=host::write_file(path,bmp);host::log("snapshot %s: %s",path.c_str(),ok?"ok":"failed");return ok;
    }
    bool import_file(u32 kind,u32 size){
        if(!in_title||size>import_bytes.size())return false;
        const u8* bytes=import_bytes.data();
        if(kind==0){PlayerRecords next;if(!next.load(bytes,size))return false;if(!write_file(save_path("score.dat").c_str(),bytes,size))return false;records=std::move(next);records.application_clock=records.game_clock=u32(SDL_GetTicks());sync_records();title->state.selection=0;title->change(TitleScreen::main);return true;}
        if(kind==1){ReplayFile file;touch::MotionTrack motion;if(!file.decode(bytes,size)||!motion.load(bytes,size,9))return false;bool found=false;for(u32 stage=0;stage<10;++stage)if(file.stream_offset(0,stage)){ReplayPlayback playback;ReplayRoundSettings header;if(!playback.begin(file,stage,header))return false;found=true;}if(!found)return false;
            for(u32 n=1;n<=9999;++n){char name[40];std::snprintf(name,sizeof(name),"replay/th9_ud%04u.rpy",n);if(!title_replay_exists(name)){const auto path=replay_path(name);return write_file(path.c_str(),bytes,size);}}return false;}
        if(kind==2){GameConfiguration next;if(!next.load(bytes,size))return false;if(!write_file(save_path("th09.cfg").c_str(),bytes,size))return false;saved_configuration=next;saved_configuration.apply(settings);title_configuration();return true;}return false;
    }
};
std::unique_ptr<Application> probe;std::string failure;
bool running=false,suspended=false;u32 loop_epoch=0;double previous_frame=-1;touhou::sdl::FrameCadence cadence;
struct Key {const char* code;const char* sdl;u32 scan,vk;bool hosted=false;SDL_Scancode native=SDL_SCANCODE_UNKNOWN;};
#include "../portable/input/KeyboardMap.inc"
touhou::input::TouchController gestures;
u32 pulse_ticks[16]{};u32 auto_fire_frame=0;
// Switch controllers stand in for the two DirectInput pads. Buttons are
// numbered so TH09's own default pad config (shot 0, charge 1, focus 2,
// skip 3, pause 4) matches the th10-switch layout; Key Config can remap:
//   0 B  1 A  2 L/ZL  3 R/ZR  4 +  5 X  6 Y
// The d-pad and sticks (clicks included) only move; they are never buttons.
// Pad 0 = handheld or player 1, pad 1 = player 2 (local versus).
struct Pad {bool connected=false;u32 buttons=0;i32 x=0,y=0;bool left=false,right=false,up=false,down=false,minus=false;};
Pad pads[2];
#ifdef __SWITCH__
PadState pad_states[2];
void initialize_pads(){
    padConfigureInput(2,HidNpadStyleSet_NpadStandard);
    padInitialize(&pad_states[0],HidNpadIdType_No1,HidNpadIdType_Handheld);padInitialize(&pad_states[1],HidNpadIdType_No2);
}
void update_pads(){
    for(u32 n=0;n<2;++n){auto& ps=pad_states[n];padUpdate(&ps);auto& p=pads[n];p={};p.connected=padIsConnected(&ps);if(!p.connected)continue;
        const u64 held=padGetButtons(&ps);const auto stick=padGetStickPos(&ps,0);
        const u64 map[7]{HidNpadButton_B,HidNpadButton_A,HidNpadButton_L|HidNpadButton_ZL,HidNpadButton_R|HidNpadButton_ZR,HidNpadButton_Plus,HidNpadButton_X,HidNpadButton_Y};
        for(u32 b=0;b<7;++b)if(held&map[b])p.buttons|=1u<<b;
        p.x=stick.x;p.y=-stick.y;p.left=held&HidNpadButton_Left;p.right=held&HidNpadButton_Right;p.up=held&HidNpadButton_Up;p.down=held&HidNpadButton_Down;p.minus=held&HidNpadButton_Minus;}
}
#else
void initialize_pads(){}
void update_pads(){}
#endif
void close_controllers(){}
i32 joy_button(i32 n){if(n<0||n>1||!pads[n].connected)return 32;for(i32 i=0;i<32;++i)if(pads[n].buttons&(1u<<i))return i;return 32;}
u16 joy_keys(i32 side){if(!probe||side<0||side>1)return 0;const u32 device=probe->settings.devices[side];if(device>1||!pads[device].connected)return 0;const auto& p=pads[device];u16 keys=0;for(i32 n=0;n<9;++n){const i32 button=probe->settings.bindings[side].gamepad[n];if(button>=0&&button<32&&(p.buttons&(1u<<button)))keys|=u16(1u<<n);}if(p.x<-19660||p.left)keys|=64;if(p.x>19660||p.right)keys|=128;if(p.y<-19660||p.up)keys|=16;if(p.y>19660||p.down)keys|=32;return keys;}
touhou::input::TouchState touch_state(){
    touhou::input::TouchState s;if(!probe)return s;if(probe->in_title||probe->paused||probe->over||probe->complete||!probe->world||!probe->session)return s;
    auto& run=*probe->session;auto& w=*probe->world;if(run.is_replay){s.context=3;return s;}if(run.phase==SessionPhase::ending){s.context=2;return s;}if(w.dialogue->id>=0){s.context=2;return s;}
    i32 side=network.active?network.side:w.configuration.controllers[0]?1:0;if(w.configuration.controllers[side])return s;auto& p=*w.battle->fields[side].player;s.context=1;s.instance=1+side+2*w.configuration.selection.stage+32*w.rules.progress.round;s.ready=(p.control.player_state==0||p.control.player_state==3)&&p.motion.health>0;
    s.x=p.motion.position.x;s.y=p.motion.position.y;s.fast=p.resource.movement.normal;s.slow=p.resource.movement.focused;const auto& limit=w.battle->state.limits;s.min_x=limit.origin.x;s.max_x=limit.origin.x+limit.extent.x;s.min_y=limit.origin.y;s.max_y=limit.origin.y+limit.extent.y;return s;
}
void clear_inputs(){for(auto& k:keyboard_map)k.hosted=false;gestures.reset();if(probe&&probe->session)probe->session->clear_motion();std::fill(std::begin(pulse_ticks),std::end(pulse_ticks),0);}
void sample_keys(u16 (&out)[3]){
    update_pads();
    bool keys[256]{};const auto* physical=SDL_GetKeyboardState(nullptr);for(const auto& k:keyboard_map)if(k.hosted||(k.native!=SDL_SCANCODE_UNKNOWN&&physical[k.native])){if(k.vk)keys[k.vk]=true;if(k.vk>=160&&k.vk<=165)keys[16+(k.vk-160)/2]=true;if(k.scan==28||k.scan==156)keys[13]=true;}
    const auto state=touch_state();auto sample=gestures.sample(state,SDL_GetTicks(),keys[16],keys[37]||keys[38]||keys[39]||keys[40]);
    // TH09 shoots ordinary bullets on repeated presses, while holding Z charges.
    // Auto-fire therefore produces real press/release input, retained by .rpy.
    if(state.context==1&&gestures.fire&&!keys[90])sample.keys[90]=((auto_fire_frame++%6)<3);
    for(u32 n=0;n<256;++n)keys[n]=keys[n]||sample.keys[n];
    if(probe->session)probe->session->clear_motion();
    if(sample.motion&&state.ready&&!keys[37]&&!keys[38]&&!keys[39]&&!keys[40]){
        const i32 side=network.active?network.side:probe->world->configuration.controllers[0]?1:0;const auto& player=*probe->world->battle->fields[side].player;const auto& m=player.motion;
        const bool focus=probe->world->configuration.automatic_focus[side]?player.input.fire_frames>=7:keys[16];const float speed=focus?player.resource.movement.focused:player.resource.movement.normal;
        const float sx=m.base_scale.x*m.effect_scale.x,sy=m.base_scale.y*m.effect_scale.y;float x=sx?(sample.x-state.x)/sx:0,y=sy?(sample.y-state.y)/sy:0;if(sample.motion!=2||network.active)touch::limit_vector(x,y,speed);
        probe->session->motion_input[side]={true,x,y,sample.motion==2&&!network.active};
    }
    out[2]=keyboard_input(keys,2)|joy_keys(0)|joy_keys(1);for(u32 n=0;n<16;++n)if(pulse_ticks[n]){out[2]|=u16(1u<<n);--pulse_ticks[n];}
    const bool paired=probe->in_title?(probe->settings.versus==0&&probe->title&&probe->title->state.screen==TitleScreen::versus_character):(probe->world&&probe->world->rules.mode==GameMode::versus&&!probe->world->configuration.controllers[0]&&!probe->world->configuration.controllers[1]);
    for(i32 side=0;side<2;++side)out[side]=paired?keyboard_input(keys,probe->settings.devices[side])|joy_keys(side):out[2];
}

}
// ---- Switch host (replaces the th09_* browser exports) ----
namespace {
bool snapshot_held=false,snapshot_requested=false;
void touch_event(const SDL_Event& e){
    // SDL2 reports touches normalised to the whole screen; the game wants the
    // 640x480 picture, which is pillarboxed inside it.
    if(!probe)return;const auto& p=probe->graphics.backend.picture;if(p.width<=0||p.height<=0)return;
    const float x=(e.tfinger.x*p.drawable_width-p.x)/p.width,y=(e.tfinger.y*p.drawable_height-p.y)/p.height;
    const int type=e.type==SDL_FINGERDOWN?0:e.type==SDL_FINGERUP?2:1;
    if(type==0&&(x<0||x>1||y<0||y>1))return;
    gestures.pointer(type,int(e.tfinger.fingerId),std::clamp(x,0.f,1.f),std::clamp(y,0.f,1.f),SDL_GetTicks(),touch_state(),false);
}
// th09_game_tick without the network branch.
u32 game_tick(bool render){
    if(!probe)return 0;u16 keys[3]{};
    sample_keys(keys);
    const bool minus=pads[0].minus||pads[1].minus;if(minus&&!snapshot_held)snapshot_requested=true;snapshot_held=minus;
    u32 ok=1;for(u32 extra=0;extra<3&&ok;++extra){ok=probe->tick_inputs(keys[0],keys[1],keys[2],false);if(!ok||probe->in_title||probe->paused||probe->over||probe->complete||!probe->session->is_replay||probe->session->phase!=SessionPhase::match||probe->session->playback_schedule()!=6)break;}
    if(ok&&render)probe->draw();return ok;
}
void fatal(const std::string& message){
    host::log("fatal: %s",message.c_str());
    // Tear down the game (GL context, window, audio) so the console can take
    // the framebuffer, then wait for +.
    probe.reset();SDL_Quit();
#ifdef __SWITCH__
    consoleInit(nullptr);
    std::printf("\n  Touhou 9: Phantasmagoria of Flower View - Switch port\n\n  Error: %s\n\n",message.c_str());
    std::printf("  Data folder: %s\n  Needed: th09.dat (v1.50a), msgothic.ttc (or the system font)\n  Optional: thbgm.dat\n\n  Press + to exit.\n",host::paths().data.c_str());
    consoleUpdate(nullptr);
    PadState pad;padInitializeDefault(&pad);
    while(appletMainLoop()){padUpdate(&pad);if(padGetButtonsDown(&pad)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}
    consoleExit(nullptr);
#else
    std::fprintf(stderr,"%s\n",message.c_str());
#endif
}
}
}

int main(int argc,char** argv){
    using namespace th09::sdl;using th09::u16;using th09::u32;
    th09::host::locate_data(argc,argv);
#ifdef __SWITCH__
    // Same CPU boost the other native ports use; the GPU stays with the applet.
    if(R_SUCCEEDED(clkrstInitialize())){ClkrstSession cpu;if(R_SUCCEEDED(clkrstOpenSession(&cpu,PcvModuleId_CpuBus,3))){clkrstSetClockRate(&cpu,1785000000);clkrstCloseSession(&cpu);}clkrstExit();}
    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleep);
#endif
    initialize_pads();
    th09::host::log("th09-switch: data %s",th09::host::paths().data.c_str());
    if(!th09::host::exists(th09::host::paths().data_file("th09.dat"))){fatal("th09.dat not found");return 1;}
    // th09_game_open: the browser passes a random seed; use the clock.
    for(auto& k:keyboard_map)k.native=SDL_GetScancodeFromName(k.sdl);
    clear_inputs();
    probe=std::make_unique<Application>();
    if(!probe->open_title()){fatal(probe->error);return 1;}
    probe->state.random={u16(std::time(nullptr)^SDL_GetTicks()),0,0};
    // Vsync paces the loop at the panel's 60 Hz: one game tick per swap.
    // Measured time only adds catch-up ticks after a real stall (bounded to
    // 4 like upstream FrameCadence), so vsync jitter never drops a frame.
    constexpr double interval=touhou::sdl::FrameCadence::interval;
    double debt=0,previous=th09::host::seconds();bool running=true;u32 ok=1;
    while(running&&ok
#ifdef __SWITCH__
          &&appletMainLoop()
#endif
    ){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT)running=false;
            else if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP||e.type==SDL_FINGERMOTION)touch_event(e);
            else if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST)clear_inputs();
        }
        const double now=th09::host::seconds();debt+=std::clamp(now-previous,0.,.1);previous=now;
        u32 ticks=u32(std::floor((debt+interval*.5)/interval));ticks=std::min(4u,std::max(ticks,1u));
        debt=std::clamp(debt-ticks*interval,-interval*.5,.1);
        for(u32 n=0;n<ticks&&ok;++n)ok=game_tick(n+1==ticks);
        if(ok&&snapshot_requested){snapshot_requested=false;probe->snapshot();}
        probe->audio.pump();
        if(probe->in_title&&probe->requested_transition==0)running=false; // title menu Quit
    }
    if(!ok){fatal(probe->error);return 1;}
    probe->title_save_records();probe->save_configuration();
    probe.reset();
    SDL_Quit();
    return 0;
}
