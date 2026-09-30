// Host-side tests for the TH09 Switch port layer (Linux; no game data needed).
//   thbgm stream - PCM offsets, seeking and intro/loop points
//   renderer     - SDL2/GLES3 port of the shared renderer (needs a display)
//   text         - SDL2_ttf port of FontDevice drawing Shift-JIS into an
//                  ANM texture through the upstream outline/resample path
//                  (needs a display and a CJK font)
#include "../game/Types.hpp"
#include "../src/BgmStream.hpp"
#include "../src/FontDevice.hpp"
#include "../src/Platform.hpp"
#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace th09;
static int failures=0;
#define CHECK(cond) do{if(!(cond)){std::printf("  FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond);++failures;}}while(0)

static void test_bgm_stream(const std::string& dir){
    std::puts("thbgm stream");
    const std::string path=dir+"/thbgm.dat";const u32 offset=20,frames=10000,intro=2500;
    {FILE* f=std::fopen(path.c_str(),"wb");std::fwrite("ZWAV",1,4,f);for(u32 i=4;i<offset;++i)std::fputc(0,f);
     for(u32 n=0;n<frames;++n){const i16 l=i16(n),r=i16(-i32(n));std::fwrite(&l,2,1,f);std::fwrite(&r,2,1,f);}std::fclose(f);}
    host::PcmStream s;s.file=std::fopen(path.c_str(),"rb");s.offset=offset;s.frames=frames;
    auto config=ma_data_source_config_init();config.vtable=&host::pcm_vtable;
    CHECK(ma_data_source_init(&config,&s.base)==MA_SUCCESS);CHECK(host::pcm_seek(&s,0)==MA_SUCCESS);
    CHECK(ma_data_source_set_loop_point_in_pcm_frames(&s,intro,frames)==MA_SUCCESS);CHECK(ma_data_source_set_looping(&s,MA_TRUE)==MA_SUCCESS);
    // Read in engine-sized chunks across several loops.
    std::vector<float> pcm(2*1024);bool ordered=true;u32 at=0;
    while(at<40000&&ordered){ma_uint64 read=0;ma_data_source_read_pcm_frames(&s,pcm.data(),1024,&read);if(read!=1024){ordered=false;break;}
        for(u32 i=0;i<1024;++i,++at){const u32 expect=at<frames?at:intro+(at-frames)%(frames-intro);if(pcm[i*2]!=float(i16(expect))/32768.f){ordered=false;std::printf("  frame %u wrong\n",at);break;}}}
    CHECK(ordered);ma_data_source_uninit(&s.base);std::fclose(s.file);
}

// Minimal PNG writer (stored deflate) so the test can use GraphicsDevice::image.
static void be32(std::vector<u8>& v,u32 x){for(int i=3;i>=0;--i)v.push_back(u8(x>>(i*8)));}
static u32 crc(const u8* p,size_t n){u32 c=~0u;for(size_t i=0;i<n;++i){c^=p[i];for(int k=0;k<8;++k)c=c&1?0xedb88320u^(c>>1):c>>1;}return ~c;}
static void chunk(std::vector<u8>& png,const char* type,const std::vector<u8>& data){be32(png,u32(data.size()));const size_t start=png.size();png.insert(png.end(),type,type+4);png.insert(png.end(),data.begin(),data.end());be32(png,crc(png.data()+start,png.size()-start));}
static std::vector<u8> blank_png(u32 w,u32 h){
    std::vector<u8> raw;for(u32 y=0;y<h;++y){raw.push_back(0);raw.insert(raw.end(),w*4,0);}
    std::vector<u8> z{0x78,0x01};u32 a=1,b=0;for(u8 c:raw){a=(a+c)%65521;b=(b+a)%65521;}
    for(size_t at=0;at<raw.size();){const size_t n=std::min<size_t>(65535,raw.size()-at);z.push_back(at+n==raw.size());z.push_back(u8(n));z.push_back(u8(n>>8));z.push_back(u8(~n));z.push_back(u8(~n>>8));z.insert(z.end(),raw.begin()+at,raw.begin()+at+n);at+=n;}
    be32(z,(b<<16)|a);
    std::vector<u8> png{0x89,'P','N','G',13,10,26,10},ihdr;be32(ihdr,w);be32(ihdr,h);ihdr.insert(ihdr.end(),{8,6,0,0,0});
    chunk(png,"IHDR",ihdr);chunk(png,"IDAT",z);chunk(png,"IEND",{});return png;
}

static void test_renderer_and_text(){
    std::puts("renderer + text (SDL2 + GLES3 + SDL2_ttf)");
    if(!std::getenv("DISPLAY")){std::puts("  skipped: no display");return;}
    sdl::GraphicsDevice graphics;const bool ok=graphics.initialize();CHECK(ok);if(!ok){std::printf("  %s\n",graphics.error.c_str());return;}
    // Game text sprites are 512 wide: upstream's scratch area scales with the sprite width.
    const auto png=blank_png(512,64);const auto texture=graphics.image(png.data(),u32(png.size()));CHECK(texture.handle!=0);
    CHECK(texture.width==512&&texture.height==64);
    sdl::FontDevice fonts{graphics};const bool font_ok=fonts.initialize();
    if(!font_ok){std::printf("  text skipped: %s\n",fonts.error.c_str());return;}
    AnmLoadedSprite sprite{};sprite.texture=texture.handle;sprite.width=512;sprite.height=32;sprite.scaleFactor={1,1};
    AnmVm vm;vm.loadedSprite=&sprite;vm.fontWidth=15;vm.fontHeight=15;
    const char* text="\x93\x8c\x95\xfb\x89\xd4\x89\x66\x92\xcb"; // 東方花映塚
    const double begin=host::seconds();CHECK(fonts.text(vm,text,0xffffff,0));std::printf("  first text draw %.1f ms\n",(host::seconds()-begin)*1000);
    CHECK(fonts.error.empty());
    u32 lit=0;const auto* image=graphics.pixels(texture.handle);for(size_t i=3;i<image->pixels.size();i+=4)if(image->pixels[i])++lit;
    std::printf("  %u opaque texels written\n",lit);CHECK(lit>300);
    if(const char* dump=std::getenv("TH09_DUMP_TEXT")){
        const u32 w=image->width,h=image->height,size=54+w*3*h;std::vector<u8> bmp(size,0);auto put=[&](u32 at,u32 v,u32 n){for(u32 i=0;i<n;++i)bmp[at+i]=u8(v>>(8*i));};
        bmp[0]='B';bmp[1]='M';put(2,size,4);put(10,54,4);put(14,40,4);put(18,w,4);put(22,h,4);put(26,1,2);put(28,24,2);
        for(u32 y=0;y<h;++y)for(u32 x=0;x<w;++x){const u8* s=image->pixels.data()+(h-1-y)*image->pitch+x*4;u8* d=bmp.data()+54+(y*w+x)*3;const u32 a=s[3];for(int c=0;c<3;++c)d[c]=u8((s[c]*a+96*(255-a))/255);}
        host::write_file(dump,bmp);
    }
    graphics.present();CHECK(glGetError()==GL_NO_ERROR);
    const auto& p=graphics.backend.picture;CHECK(p.width*3==p.height*4);
}

int main(){
    const std::string dir=std::string(std::getenv("TMPDIR")?std::getenv("TMPDIR"):"/tmp")+"/th09-switch-test";
    host::make_directory(dir);host::paths().data=host::paths().save=dir;
    test_bgm_stream(dir);test_renderer_and_text();
    std::printf(failures?"FAILED (%d)\n":"all host tests passed\n",failures);
    SDL_Quit();return failures?1:0;
}
