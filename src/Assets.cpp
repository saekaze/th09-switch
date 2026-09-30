#include "Assets.hpp"
#include "../game/ResourceCrypt.hpp"
#include "Platform.hpp"
#include <limits>
namespace th09::sdl {
bool read_file(const char* name,std::vector<u8>& out){return host::read_file(name,out,std::numeric_limits<u32>::max());}
bool Assets::open(const char* path){
    cancel_prepare();cache.clear();cached=0;archive.close();if(file)std::fclose(file);file=std::fopen(path,"rb");if(!file){error=std::string("Unable to open ")+path;return false;}
    buffer.resize(256*1024);std::setvbuf(file,buffer.data(),_IOFBF,buffer.size());
    std::fseek(file,0,SEEK_END);const long n=std::ftell(file);if(n<=0||u64(n)>0xffffffffull){error="Invalid TH09 archive size";return false;}length=u32(n);
    if(!archive.open(*this)){error="Invalid TH09 resource archive";return false;}error.clear();return true;
}
bool Assets::read(u32 offset,u8* output,u32 count){return file&&offset<=length&&count<=length-offset&&std::fseek(file,long(offset),SEEK_SET)==0&&std::fread(output,1,count,file)==count;}
bool Assets::read(const char* name,std::vector<u8>& out){
    if(!name)return false;const char* short_name=name;for(const char* p=name;*p;++p)if(*p=='/'||*p=='\\')short_name=p+1;
    if(prepared_ready&&prepared_name==short_name){out=std::move(prepared_bytes);cancel_prepare();return true;}
    const auto found=cache.find(short_name);if(found!=cache.end()){out=found->second;return true;}if(archive.read(short_name,out)){if(out.size()<=512*1024&&cached+out.size()<=12*1024*1024){cached+=out.size();cache.emplace(short_name,out);}return true;}error=std::string("Missing TH09 resource: ")+short_name;return false;
}
void Assets::cancel_prepare(){prepared_name.clear();packed_bytes.clear();prepared_bytes.clear();decoder.reset();prepared_ready=false;}
i32 Assets::prepare(const char* name){
    if(!name)return -1;if(cache.find(name)!=cache.end())return 1;if(prepared_name!=name){cancel_prepare();prepared_name=name;u32 size=0;if(!archive.packed(name,packed_bytes,size)){cancel_prepare();return -1;}prepared_bytes.resize(size);decoder=std::make_unique<LzssStream>();return 0;}
    if(prepared_ready)return 1;if(!decoder||!decoder->step(packed_bytes.data(),u32(packed_bytes.size()),prepared_bytes.data(),u32(prepared_bytes.size()))){cancel_prepare();return -1;}
    if(!decoder->done())return 0;if(decoder->size()!=prepared_bytes.size()||!resource_unwrap(prepared_bytes)){cancel_prepare();return -1;}prepared_ready=true;decoder.reset();packed_bytes.clear();return 1;
}

}
