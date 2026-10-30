#pragma once
namespace NativeCommandFlow {
inline bool TextureOwnsReply(int phase){return phase==1||phase==2||phase==4||phase==6;}
inline bool NativeOwnsReply(int phase){return phase==1||phase==3||phase==4;}
inline bool DrainTextureFirst(int texturePhase,int nativePhase){return TextureOwnsReply(texturePhase)&&!NativeOwnsReply(nativePhase);}
}
