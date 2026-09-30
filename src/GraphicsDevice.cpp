#include "GraphicsDevice.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "../portable/sdl/third_party/stb_image.h"
namespace th09::sdl {
touhou::sdl::Surface GraphicsDevice::resolve(void* opaque,u32 id){auto& device=*static_cast<GraphicsDevice*>(opaque);const auto it=device.textures.find(id);if(it==device.textures.end())return {};auto& t=it->second;auto& i=t.image;return {id,i.width,i.height,i.format,i.pitch,i.pixels.data(),u32(i.pixels.size()),t.revision};}
bool GraphicsDevice::initialize(){
    for(u32 id:{screen,depth}){auto& i=textures[id].image;i.width=640;i.height=480;i.format=id==screen?PixelFormat::Bgra8:PixelFormat::Depth16;i.pitch=640*(id==screen?4:2);if(id==screen)i.pixels.resize(i.pitch*i.height);}
    backend.title="Touhou 9: Phantasmagoria of Flower View";
    if(!backend.initialize()){error=backend.error();return false;}backend.state.target=screen;backend.state.depth=depth;return true;
}
TextureAllocation GraphicsDevice::create(const AnmTextureSource& source,const u8* bytes,u32 count){
    Texture t;if(!t.image.load(source,bytes,count)){error="Invalid ANM texture: "+source.name;return {};}
    const u32 id=next++,width=t.image.width,height=t.image.height;textures.emplace(id,std::move(t));backend.prepare(id);return {id,width,height};
}
TextureAllocation GraphicsDevice::image(const u8* data,u32 size){
    i32 width=0,height=0,channels=0;u8* raw=stbi_load_from_memory(data,i32(size),&width,&height,&channels,4);if(!raw){error=stbi_failure_reason();return {};}
    Texture t;auto& i=t.image;i.width=width;i.height=height;i.pitch=width*4;i.format=PixelFormat::Bgra8;i.pixels.resize(size_t(i.pitch)*height);
    for(size_t p=0;p<i.pixels.size();p+=4){i.pixels[p]=raw[p+2];i.pixels[p+1]=raw[p+1];i.pixels[p+2]=raw[p];i.pixels[p+3]=raw[p+3];}stbi_image_free(raw);
    const u32 id=next++;textures.emplace(id,std::move(t));backend.prepare(id);return {id,u32(width),u32(height)};
}
void GraphicsDevice::destroy(u32 id){if(id<3)return;backend.flush();backend.release(id);textures.erase(id);}
TextureImage* GraphicsDevice::pixels(u32 id){auto it=textures.find(id);return it==textures.end()?nullptr:&it->second.image;}
void GraphicsDevice::changed(u32 id){auto it=textures.find(id);if(it!=textures.end())++it->second.revision;}
void GraphicsDevice::viewport(const Viewport& v){backend.viewport({v.x,v.y,v.width,v.height,v.near_depth,v.far_depth});}
void GraphicsDevice::transform(MatrixKind kind,const Matrix4& value){backend.transform(kind,&value);}
void GraphicsDevice::clear(bool color,bool depth,u32 rgba,float z){backend.clear((color?1u:0u)|(depth?2u:0u),rgba,z,0);}
void GraphicsDevice::state(const PipelineState& p,u32 texture,VertexLayout layout){backend.state.pipeline=p;backend.state.texture=texture;backend.state.layout=attributes(layout);}
void GraphicsDevice::draw(const PipelineState& p,u32 texture,Topology primitive,VertexLayout layout,const void* data,u32 vertices){
    const u32 min=primitive==Topology::Points?1:primitive==Topology::Lines||primitive==Topology::LineStrip?2:3;if(vertices<min)return;
    const u32 count=primitive==Topology::Triangles?vertices/3:primitive==Topology::Lines?vertices/2:primitive==Topology::LineStrip?vertices-1:primitive==Topology::Strip||primitive==Topology::Fan?vertices-2:vertices;
    state(p,texture,layout);backend.draw(primitive,count,data,stride(layout));
}
void GraphicsDevice::triangles(const PipelineState& p,u32 texture,const SpriteVertex* data,u32 vertices){if(vertices<3)return;state(p,texture,VertexLayout::ScreenColorUv);backend.draw_batch(vertices/3,data,sizeof(SpriteVertex));}
void GraphicsDevice::present(){backend.present(screen);}
}
