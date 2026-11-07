#pragma once
namespace GameSky {
inline constexpr const char* Shader=R"(
cbuffer SkyFrame:register(b13){float4 cameraRight;float4 cameraUp;float4 cameraForward;float4 primary;float4 secondary;float4 animation;float4 options;float4 screen;float4 bloom;float4 projection;float4 fogColour;float4 fogRange;float4 outlineOptions;float4 outlineColour;float4 hiddenColour;float4 outlineGlow;float4 outlineEffect;float4 weatherOptions;float4 weatherColour;float4 cameraPosition;float4 reflectionOptions;float4 aimPoint;float4 aimOptions;float4 waterOptions;};
Texture2D<float> sceneDepth:register(t0);Texture2D<float4> sceneColour:register(t1);SamplerState linearClamp:register(s0);
Texture2DMS<float> sceneDepthMS:register(t2);Texture2D<float4> silhouette:register(t3);Texture2D<float4> skyImage:register(t4);SamplerState skyWrap:register(s1);Texture2D<float4> reflectionResult:register(t5);
cbuffer GameFrame:register(b0){float4 gameView[4];};
struct Vertex{float4 p:SV_POSITION;float2 uv:TEXCOORD0;float3 ray:TEXCOORD1;float3 origin:TEXCOORD2;nointerpolation float4 right:TEXCOORD3;nointerpolation float4 up:TEXCOORD4;nointerpolation float4 forward:TEXCOORD5;nointerpolation float4 depthProjection:TEXCOORD6;nointerpolation float cameraValid:TEXCOORD7;};
Vertex vs(uint n:SV_VertexID){
 Vertex o;o.cameraValid=0;o.origin=cameraPosition.xyz;o.depthProjection=projection;float2 p=float2((n<<1)&2,n&2),ndc=p*float2(2,-2)+float2(-1,1);o.p=float4(ndc,0,1);o.uv=p;
 float3 r=cameraRight.xyz/cameraRight.w,u=cameraUp.xyz/cameraUp.w,f=cameraForward.xyz/cameraForward.w;
 float3 sizes=float3(length(gameView[0].xyz),length(gameView[1].xyz),length(gameView[3].xyz));float3 ratios=sizes/float3(cameraRight.w,cameraUp.w,cameraForward.w);
 if(all(sizes>.0001)&&all(abs(ratios-1)<.05)){
  float3 nr=gameView[0].xyz/sizes.x,nu=gameView[1].xyz/sizes.y,nf=gameView[3].xyz/sizes.z;
  if(abs(dot(nr,nu))<.01&&abs(dot(nr,nf))<.01&&abs(dot(nu,nf))<.01){o.cameraValid=1;r=nr/sizes.x;u=nu/sizes.y;f=nf/sizes.z;o.origin=-gameView[0].w/sizes.x*nr-gameView[1].w/sizes.y*nu-gameView[3].w/sizes.z*nf;float a=dot(gameView[2].xyz,nf)/sizes.z,b=gameView[2].w-a*gameView[3].w;if(isfinite(a)&&isfinite(b)&&abs(b)>.0000001)o.depthProjection=float4(a,b,sizes.z,b>0?1:0);}
 }
 o.right=float4(normalize(r),rcp(length(r)));o.up=float4(normalize(u),rcp(length(u)));o.forward=float4(normalize(f),rcp(length(f)));o.ray=r*ndc.x+u*ndc.y+f;return o;
}
float hash(float3 p){p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
float noise(float3 p){float3 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+1),f.x),f.y),f.z);}
float cloud(float3 p){return noise(p)*.57+noise(p*2.03+17.1)*.29+noise(p*4.09+31.7)*.14;}
float3 stars(float3 d,float time){float3 p=d*310,cell=floor(p);float3 delta=frac(p)-.5;float seed=hash(cell);float star=exp(-dot(delta,delta)*95)*step(.986,seed);float sparkle=.72+.28*sin(time*.8+seed*73);return star*sparkle*lerp(float3(.55,.69,1),float3(1,.82,.6),hash(cell+91))*1.5;}

)"
R"(
float2 skyHash2(float2 p){return frac(sin(float2(dot(p,float2(127.1,311.7)),dot(p,float2(269.5,183.3))))*43758.5453);}
float3 deepSky(float3 d){
 float3 ad=abs(d);float2 uv;float face;
 if(ad.x>ad.y&&ad.x>ad.z){uv=d.yz/ad.x;face=d.x>0?0:1;}
 else if(ad.y>ad.z){uv=d.xz/ad.y;face=d.y>0?2:3;}
 else{uv=d.xy/ad.z;face=d.z>0?4:5;}
 float3 colour=float3(.003,.004,.009);float dust=cloud(d*3.8+float3(11,7,2));
 colour+=float3(.016,.011,.029)*pow(saturate(dust*1.4-.23),3);
 [loop]for(int layer=0;layer<2;++layer){
  float density=layer==0?110:270;float2 p=(uv*.5+.5)*density,cell=floor(p);float2 seed=skyHash2(cell+face*613+layer*191);
  float2 starPosition=.18+.64*seed,delta=frac(p)-starPosition;float radius=layer==0?.055:.035;
  float width=max(length(float2(ddx(p.x),ddy(p.y)))*.65,.015);float spot=1-smoothstep(radius,radius+width,length(delta));
  float active=step(layer==0?.978:.995,skyHash2(cell+face*719+layer*397).x),coverage=radius*radius/((radius+width)*(radius+width));
  float3 tint=lerp(float3(.66,.78,1),float3(1,.84,.66),seed.y);colour+=tint*spot*active*coverage*(layer==0?1.8:1);
 }
 return colour;
}
float3 discLight(float3 p,float3 normal,float3 axis,float time){
 float radius=length(p),inner=3.05,outer=8.5;
 float envelope=smoothstep(inner,inner+.6,radius)*(1-smoothstep(outer-1.7,outer,radius));
 float3 tangent=cross(normal,axis);float angle=atan2(dot(p,tangent),dot(p,axis));
 float phase=angle-time*.9*pow(3.8/max(radius,inner),1.5);float2 orbit=float2(cos(phase),sin(phase));
 float turbulence=noise(float3(orbit*radius*1.4,radius*.7));float fine=noise(float3(orbit*radius*5,radius*3));
 float rings=.78+.10*sin(radius*24+turbulence*5)+.04*sin(radius*47+fine*3);
 float structure=rings*(.42+.75*turbulence);float hot=saturate((7-radius)/4);
 float3 colour=lerp(float3(.25,.43,.72),float3(.84,.91,1),hot);
 float velocity=dot(normalize(cross(normal,p)),normalize(-float3(0,.2873479,.9578263)));float beaming=clamp(1+velocity*.38,.55,1.45);
 return colour*envelope*structure*pow(4/max(radius,inner),1.5)*beaming*1.8;
}
float3 blackHoleSky(float3 direction){
 float cs=cos(options.x),sn=sin(options.x);float3 ray=float3(direction.x*cs+direction.z*sn,direction.y,direction.z*cs-direction.x*sn);
 float3 centre=normalize(float3(0,.30,1));float distance=18/max(animation.w,.4);float3 position=-centre*distance;
 float3 normal=normalize(float3(.35,.92,-.17)),axis=normalize(cross(normal,centre));
 float angular=dot(ray,centre),impact2=dot(cross(position,ray),cross(position,ray));float3 light=0;float transmission=1;
 if(angular>0&&impact2<100){
  float3 velocity=ray;float momentum=impact2;position+=ray*max(0,distance*angular-sqrt(max(0,144-impact2)));
  [loop]for(int stepIndex=0;stepIndex<96;++stepIndex){
   float radius=length(position);if(radius<1.02){transmission=0;break;}
   if(radius>max(distance,13)&&dot(position,velocity)>0)break;
   float delta=clamp(radius*.085,.025,1.2);float inv=rcp(radius);float3 acceleration=-1.5*momentum*position*(inv*inv*inv*inv*inv);
   float3 halfVelocity=velocity+acceleration*(delta*.5);float3 next=position+halfVelocity*delta;
   float before=dot(position,normal),after=dot(next,normal);
   if(before*after<0){float3 crossing=lerp(position,next,before/(before-after));float radiusAtDisc=length(crossing);
    if(radiusAtDisc>3.05&&radiusAtDisc<8.5){light+=discLight(crossing,normal,axis,animation.x)*transmission;transmission*=.28;}
   }
   float invNext=rsqrt(max(dot(next,next),.2));float3 nextAcceleration=-1.5*momentum*next*(invNext*invNext*invNext*invNext*invNext);
   velocity=halfVelocity+nextAcceleration*(delta*.5);position=next;
  }
  ray=normalize(lerp(ray,normalize(velocity),1-smoothstep(75,100,impact2)));
 }
 float3 background=deepSky(ray);float3 colour=background*transmission+pow(1-exp(-light*.9),.454545);
 float3 tint=lerp(float3(1,1,1),lerp(primary.rgb,secondary.rgb,saturate(direction.y*.5+.5))*1.25,saturate(reflectionOptions.w));
 return max(0,colour)*animation.z*tint;
}
)"
R"(
float3 skyDirection(float3 d){float cs=cos(options.x),sn=sin(options.x);return float3(d.x*cs+d.z*sn,d.y,d.z*cs-d.x*sn);}
float sphereIntersection(float3 origin,float3 ray,float radius){float b=dot(origin,ray),h=b*b-dot(origin,origin)+radius*radius;return h>0?max(0,-b-sqrt(h)):100000;}
float ringDensity(float radius){
 float edge=smoothstep(3.0,3.08,radius)*(1-smoothstep(5.55,5.63,radius));float gap=smoothstep(.025,.065,abs(radius-4.63));
 float width=max(fwidth(radius),.001);float bands=.89+.06*sin(radius*100)*exp(-width*width*5000)+.05*sin(radius*237)*exp(-width*width*28084);
 return saturate(edge*gap*bands);
}
float3 ringedPlanetSky(float3 direction){
 float3 ray=skyDirection(direction),centre=normalize(float3(0,.37,1))*7.2/max(animation.w,.4);
 float3 normal=normalize(float3(.31,.83,-.47)),sun=normalize(float3(-.7,.48,-.56));
 float hit=sphereIntersection(-centre,ray,2.55);float3 colour=deepSky(ray);float3 tint=float3(1,1,1);
 if(hit<100000){
  float3 p=ray*hit-centre,n=p/2.55;float time=animation.x*.035,cs=cos(time),sn=sin(time);
  float3 q=n*cs+cross(normal,n)*sn+normal*dot(normal,n)*(1-cs);float latitude=dot(q,normal);
  float weather=cloud(q*11+float3(3,8,1));float bands=.5+.27*sin(latitude*41+weather*2)+.14*sin(latitude*89+weather*4);
  float3 albedo=lerp(float3(.39,.29,.18),float3(.85,.78,.61),saturate(bands));
  float sunlight=saturate(dot(n,sun));float denom=dot(sun,normal);float toRings=abs(denom)>.0001?-dot(p,normal)/denom:-1;
  if(toRings>0){float opacity=ringDensity(length(p+sun*toRings));sunlight*=1-opacity*.85;}
  colour=albedo*(.008+sunlight*.9)+float3(.15,.18,.23)*pow(saturate(1-dot(n,-ray)),4)*sunlight*.18;
  colour=pow(max(0,colour),.454545);
 }
 float denom=dot(ray,normal),plane=abs(denom)>.0001?dot(centre,normal)/denom:-1;
 if(plane>0&&plane<hit){
  float3 p=ray*plane-centre;float radius=length(p),density=ringDensity(radius);
  if(density>0){float3 albedo=lerp(float3(.40,.34,.25),float3(.88,.83,.70),saturate((radius-3.1)/2));
   float shade=sphereIntersection(p+sun*.01,sun,2.55)<100000?.08:1;
   float3 rings=pow(albedo*(.18+.6*abs(dot(normal,sun)))*shade,.454545);colour=lerp(colour,rings,density);
  }
 }
 tint=lerp(tint,lerp(primary.rgb,secondary.rgb,saturate(direction.y*.5+.5))*1.25,saturate(reflectionOptions.w));return colour*animation.z*tint;
}
)"
R"(
float segmentDistance(float3 ray,float3 a,float3 b){float3 v=b-a;float rv=dot(ray,v),vv=dot(v,v),ra=dot(ray,a),va=dot(v,a);float along=saturate((rv*ra-va)/max(vv-rv*rv,.0001));return length(ray*max(0,ra+rv*along)-(a+v*along));}
float3 stormSky(float3 direction,bool golden){
 float3 d=skyDirection(direction);float time=animation.x;float scale=max(animation.w,.4);float3 drift=float3(time*.03,0,time*.012);
 float density=cloud(d*(4/scale)+drift),detail=cloud(d*(11/scale)-drift*.4);
 float lighting=saturate(.5+(cloud(d*(4/scale)+drift+float3(-.2,.3,-.15))-density)*2);
 float3 dark=golden?float3(.035,.022,.014):float3(.017,.020,.055),lit=golden?float3(.48,.32,.12):float3(.19,.16,.36);
 float3 colour=lerp(dark,lit,pow(density,2.3))*(.5+lighting)+detail*dark*.5;float energy=0;
 [loop]for(int bolt=0;bolt<3;++bolt){
  float angle=bolt*2.094395-.42;float3 forward=float3(sin(angle),0,cos(angle)),right=float3(cos(angle),0,-sin(angle));
  if(dot(d,forward)<.78)continue;
  float cycle=time*.7+bolt*.31,event=floor(cycle),phase=frac(cycle);float pulse=.14+1.3*exp(-phase*9)+.8*exp(-abs(phase-.16)*45);
  float seed=hash(float3(event,bolt,3));float3 previous=forward*45+right*(seed-.5)*10+float3(0,23,0);float nearest=100;
  [loop]for(int part=1;part<=10;++part){float height=23-part*3.6;float bend=(hash(float3(event,bolt,part))-.5)*5;float3 next=forward*(45+sin(part*1.7+seed)*2)+right*((seed-.5)*10+bend)+float3(0,height,0);nearest=min(nearest,segmentDistance(d,previous,next));
   if(part==4||part==7){float3 end=next+right*(seed>.5?7:-7)+float3(0,-5,0);nearest=min(nearest,segmentDistance(d,next,end));}previous=next;
  }
  float width=.07+length(float2(ddx(d.x),ddy(d.y)))*35;
  float core=exp(-nearest*nearest/max(.002,width*width)),halo=exp(-nearest*nearest/2.8);energy+=(core*5+halo*.6)*pulse;
  colour+=(golden?float3(.6,.36,.09):float3(.18,.16,.55))*exp(-pow(1-dot(d,forward),2)*70)*pulse*.15;
 }
 colour+=(golden?float3(1,.87,.58):float3(.8,.84,1))*energy;return pow(1-exp(-colour),.7);
}
float3 auroraSky(float3 direction){
 float3 d=skyDirection(direction),colour=deepSky(d);float azimuth=atan2(d.x,d.z),time=animation.x;
 float horizontal=max(length(d.xz),.0001),transmission=1;
 [loop]for(int sheet=0;sheet<6;++sheet){
  float radius=(24+sheet*6)/max(animation.w,.4),phi=azimuth+sheet*.11;float bottom=4.4+2.2*sin(phi*2.3+time*.09)+1.3*sin(phi*6.1-time*.055+sheet*.6);
  float height=d.y*radius/horizontal-bottom;float wave=.7+.3*sin(phi*3+sheet*1.1-time*.12);
  float folds=noise(float3(phi*17+sheet*2,time*.035,sheet)),threads=noise(float3(phi*125,height*.055-time*.04,sheet*3));
  float veil=smoothstep(-.45,.4,height)*exp(-max(height,0)*.11)*(1-smoothstep(30,48,height));
  float density=veil*(.13+.3*folds)*(.45+.55*threads)*wave;
  float3 tint=lerp(float3(.06,.95,.53),float3(.29,.32,.78),saturate(height/28));
  float lower=exp(-abs(height)*1.5)*(.3+.7*folds);colour+=transmission*(tint*density+float3(.14,.85,.65)*lower*.12);transmission*=1-density*.12;
 }
 return colour;
}
float3 galaxySky(float3 direction){
 float3 d=skyDirection(direction),colour=deepSky(d),centre=normalize(float3(0,.35,1))*16/max(animation.w,.4);
 float3 normal=normalize(float3(.2,.63,-.75)),axis=normalize(cross(normal,float3(0,1,0))),other=cross(normal,axis);float denom=dot(d,normal),transmission=1;
 if(abs(denom)>.001){
  [loop]for(int layer=0;layer<6;++layer){float distance=(dot(centre,normal)+(layer-2.5)*.32)/denom;if(distance<0)continue;
   float3 p=d*distance-centre;float2 q=float2(dot(p,axis),dot(p,other));float radius=length(q),angle=atan2(q.y,q.x)-animation.x*.035;
   float arms=.5+.5*cos(angle*3-log(max(radius,.35))*5.4);float edge=1-smoothstep(8,12,radius);
   float dust=cloud(p*.8+float3(11,4,7));float lane=pow(saturate(.5+.5*cos(angle*3-log(max(radius,.35))*5.4+.45)),12);
   float structure=pow(arms,5)*exp(-radius*.12)*edge*(.18+.5*dust)*(1-lane*.6);float core=exp(-radius*radius*.85);
   float3 light=lerp(float3(.28,.38,.78),float3(.77,.61,.48),exp(-radius*.18))*structure+float3(1,.86,.65)*core*.7;
   colour+=light*transmission;transmission*=1-structure*.15;
  }
 }
 return 1-exp(-colour*1.3);
}
float3 satinSky(float3 direction){
 float3 d=skyDirection(direction),colour=deepSky(d);float horizontal=max(length(d.xz),.0001),azimuth=atan2(d.x,d.z),time=animation.x;
 float3 light=normalize(float3(-.6,.8,-.2));
 [loop]for(int ribbon=5;ribbon>=0;--ribbon){
  float radius=(28+ribbon*3)/max(animation.w,.4),phase=azimuth*2+time*.06+ribbon*.6;
  float centre=-9+ribbon*5+4*sin(phase)+1.5*sin(azimuth*5-time*.04+ribbon);
  float width=3.6+sin(azimuth*3+ribbon);float offset=(d.y*radius/horizontal-centre)/width;
  float edge=1-smoothstep(.91,1,abs(offset));if(edge<=0)continue;
  float slope=(8*cos(phase)+7.5*cos(azimuth*5-time*.04+ribbon))/radius;
  float3 side=float3(cos(azimuth),slope,-sin(azimuth)),front=normalize(float3(d.x,0,d.z));
  float3 normal=normalize(-front*sqrt(saturate(1-offset*offset))+float3(0,offset,0)-side*.2);
  float shade=.22+.65*saturate(dot(normal,light));float gloss=pow(saturate(dot(reflect(-light,normal),-d)),42);
  float mixColour=.5+.5*sin(azimuth*.7+ribbon*.7);float3 tint=lerp(float3(.26,.14,.59),float3(.59,.10,.47),mixColour);
  if(ribbon%3==0)tint=lerp(float3(.07,.26,.24),tint,.3);
  float3 material=tint*shade+float3(.66,.72,.95)*gloss*.5;colour=lerp(colour,material,edge*.98);
 }
 return colour;
}
)"
R"(
float3 nebulaSky(float3 direction){
 float3 d=skyDirection(direction),colour=0;float transmission=1,time=animation.x;
 [loop]for(int sample=0;sample<6;++sample){
  float distance=9+sample*7.8;float3 p=d*distance*.12/max(animation.w,.4)+float3(time*.007,3,time*.003);
  float warp=noise(p*.5+7);float3 q=p+warp*1.4;float broad=noise(q),detail=noise(q*2.03+17.1);float density=saturate(broad*.72+detail*.28-.37);density=density*density*4.6;
  float lit=saturate(.45+(noise(q+float3(-.3,.4,-.1))-broad)*1.6);
  float3 tint=lerp(float3(.17,.075,.32),float3(.065,.30,.43),smoothstep(.35,.65,warp));
  colour+=tint*density*(.3+lit*.9+detail*.5)*transmission;transmission*=exp(-density*.7);
 }
 return colour*1.5+deepSky(d)*transmission;
}
float3 moonlitSky(float3 d){
 float horizon=exp(-abs(d.y)*6);float3 colour=lerp(float3(.008,.012,.04),float3(.14,.075,.18),horizon)+deepSky(d)*smoothstep(0,.15,d.y);
 float3 centre=normalize(float3(0,.2,1))*80/max(animation.w,.4);float hit=sphereIntersection(-centre,d,2.6);
 if(hit<100000){float3 n=(d*hit-centre)/2.6;float shade=.04+.96*saturate(dot(n,normalize(float3(-.5,.25,-1))));float craters=.7+.3*cloud(n*15);colour=float3(.85,.9,1)*shade*craters;}
 float wisps=cloud(d*9+float3(animation.x*.012,3,0));colour+=float3(.05,.045,.09)*pow(wisps,4)*exp(-abs(d.y-.08)*8);return colour;
}
float3 duskOceanSky(float3 direction){
 float3 d=skyDirection(direction);if(d.y>=-.001)return moonlitSky(d);
 float distance=-3/d.y;float3 p=d*distance/max(animation.w,.4);float time=animation.x;float atten=exp(-max(0,distance-20)*.003);
 float2 gradient=float2(.11*cos(p.x*.23+p.z*.12-time*.65)+.045*cos(p.x*.57-p.z*.15+time*.4),.07*cos(p.z*.32-time*.5)+.04*cos(p.x*.3+p.z*.43+time*.7))*atten;
 float3 normal=normalize(float3(-gradient.x,1,-gradient.y));float3 reflection=reflect(d,normal);
 float fresnel=.06+.94*pow(1-saturate(dot(-d,normal)),5);float3 colour=lerp(float3(.011,.021,.048),moonlitSky(reflection),fresnel*.88+.12);
 float3 light=normalize(float3(0,.2,1));float glint=pow(saturate(dot(reflect(-light,normal),-d)),180)*.28;return colour+float3(.6,.7,.9)*glint;
}
float3 synthwaveSky(float3 direction){
 float3 d=skyDirection(direction);float horizon=exp(-abs(d.y)*5);float3 colour=lerp(float3(.018,.008,.063),float3(.36,.065,.30),horizon)+deepSky(d)*smoothstep(.1,.4,d.y);
 float3 centre=normalize(float3(0,.26,1))*16/max(animation.w,.4);float hit=sphereIntersection(-centre,d,3.1);
 if(hit<100000){float3 p=d*hit-centre;float horizontal=smoothstep(.06,.1,frac((p.y+3.1)*2.5));float stripes=lerp(horizontal,1,smoothstep(-.2,1.8,p.y));float3 tint=lerp(float3(.96,.16,.43),float3(1,.83,.42),saturate((p.y+2)/4));colour=lerp(colour,tint,stripes);}
 float haze=pow(cloud(d*8+float3(animation.x*.025,0,3)),3)*exp(-abs(d.y-.12)*5);colour=lerp(colour,float3(.27,.10,.28),haze*.18);
 float azimuth=atan2(d.x,d.z);float hills=.006+.012*noise(float3(azimuth*24,5,7))+.025*noise(float3(azimuth*8,2,1));if(d.y<hills)colour=lerp(float3(.028,.009,.062),float3(.06,.018,.10),saturate(d.y*20));return colour;
}
float3 shadeSky(float3 d){
 float style=animation.y;if(style<.5)return blackHoleSky(d);if(style<1.5)return ringedPlanetSky(d);
 float3 colour=0;
 if(style<2.5)colour=stormSky(d,false);else if(style<3.5)colour=auroraSky(d);else if(style<4.5)colour=galaxySky(d);
 else if(style<5.5)colour=satinSky(d);else if(style<6.5)colour=nebulaSky(d);else if(style<7.5)colour=duskOceanSky(d);else if(style<8.5)colour=synthwaveSky(d);else colour=stormSky(d,true);
 float3 tint=lerp(float3(1,1,1),lerp(primary.rgb,secondary.rgb,saturate(d.y*.5+.5))*1.25,saturate(reflectionOptions.w));return max(0,colour)*animation.z*tint;
}

float4 sky(Vertex i):SV_TARGET{
 float depth=sceneDepth.Load(int3(int2(i.p.xy),0));if(options.y>.5?depth>.0000001:depth<.9999999)discard;
 float3 d=normalize(i.ray);
 return float4(shadeSky(d),1);
}
float4 skyMS(Vertex i):SV_TARGET{
 uint width,height,count;sceneDepthMS.GetDimensions(width,height,count);float depth=options.y>.5?0:1;
 [loop]for(uint sample=0;sample<count;++sample){float value=sceneDepthMS.Load(int2(i.p.xy),sample);depth=options.y>.5?max(depth,value):min(depth,value);}
 if(options.y>.5?depth>.0000001:depth<.9999999)discard;
 return float4(shadeSky(normalize(i.ray)),1);
}
float4 glow(Vertex i):SV_TARGET{
 float2 uv=float2((i.uv.x-.5)/max(waterOptions.y,1)+.5,i.uv.y);
 float3 base=sceneColour.SampleLevel(linearClamp,uv,0).rgb;
 if(bloom.z<=0)return float4(base,1);float2 stepSize=bloom.x/screen.xy;
 float3 light=0;float weights=0;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){float weight=x==0&&y==0?4:(x==0||y==0?2:1);float3 c=sceneColour.SampleLevel(linearClamp,uv+float2(x,y)*stepSize,0).rgb;float luminance=max(c.r,max(c.g,c.b));light+=c*saturate((luminance-bloom.y)/max(luminance,.001))*weight;weights+=weight;}
 return float4(base+light/weights*bloom.z,1);
}

float4 maskVisible():SV_TARGET{return float4(0,1,0,1);}
float4 maskAll():SV_TARGET{return float4(1,0,0,1);}
float4 fogValue(Vertex i,float z){
 float denominator=z-projection.x;
 float distance=abs(denominator)>.0000001?max(0,projection.y/denominator)*length(i.ray):fogRange.y;
 float weight=smoothstep(fogRange.x,max(fogRange.x+1,fogRange.y),distance);
 return float4(fogColour.rgb,weight*fogColour.a);
}
float4 fog(Vertex i):SV_TARGET{return fogValue(i,sceneDepth.Load(int3(int2(i.p.xy),0)));}
float4 fogMS(Vertex i):SV_TARGET{
 uint w,h,count;sceneDepthMS.GetDimensions(w,h,count);float z=options.y>.5?0:1;
 [loop]for(uint s=0;s<count;++s){float v=sceneDepthMS.Load(int2(i.p.xy),s);z=options.y>.5?max(z,v):min(z,v);}
 return fogValue(i,z);
}
float4 outline(Vertex i):SV_TARGET{
 float2 centre=silhouette.SampleLevel(linearClamp,i.uv,0).rg;
 float interior=outlineOptions.z>.5?max(centre.r,centre.g):centre.g;
 float2 edge=0;float2 halo=0;
 uint effects=(uint)outlineOptions.x;
 if(effects&1u) {
  [unroll]for(int n=0;n<8;++n){float angle=n*.7853981634;float2 d=float2(cos(angle),sin(angle))/screen.xy;
   edge=max(edge,silhouette.SampleLevel(linearClamp,i.uv+d*outlineOptions.y,0).rg);
  }
 }
 if(effects&2u) {
  [unroll]for(int n=0;n<8;++n){float angle=n*.7853981634;float2 d=float2(cos(angle),sin(angle))/screen.xy;
   halo+=silhouette.SampleLevel(linearClamp,i.uv+d*outlineEffect.y,0).rg;
  }
 }
 edge=saturate(edge-interior);halo=saturate(halo/8-interior);
 float visible=edge.g,hidden=outlineOptions.z>.5?saturate(edge.r-edge.g):0;
 float pulse=outlineEffect.z>1.5?.65+.35*sin(outlineEffect.w*3):1;
 if(outlineEffect.z>.5&&outlineEffect.z<1.5){float dash=step(.35,frac((i.p.x+i.p.y)*.07+outlineEffect.w*.25));visible*=dash;hidden*=dash;}
 float v=visible*outlineColour.a*outlineOptions.w,h=hidden*hiddenColour.a*outlineOptions.w;
 float g=(outlineOptions.z>.5?max(halo.r,halo.g):halo.g)*outlineGlow.a*outlineEffect.x;
 float total=(v+h+g)*pulse;float3 colour=(outlineColour.rgb*v+hiddenColour.rgb*h+outlineGlow.rgb*g)/max(v+h+g,.00001);
 return float4(colour,saturate(total));
}

float readDepth(int2 p){return screen.z>1?sceneDepthMS.Load(p,0):sceneDepth.Load(int3(p,0));}
float clipDistance(float z){return abs(z-projection.x)>.0000001?max(0,projection.y/(z-projection.x)):100000;}
struct WeatherVertex {
 float4 p:SV_POSITION;float2 uv:TEXCOORD0;nointerpolation float4 tint:TEXCOORD1;nointerpolation float2 depthTerms:TEXCOORD2;
};
WeatherVertex weatherVS(uint vertex:SV_VertexID,uint instance:SV_InstanceID){
 WeatherVertex o=(WeatherVertex)0;Vertex camera=vs(0);uint side=weatherOptions.y<.5?16:12,layers=weatherOptions.y<.5?8:weatherOptions.y<1.5?6:4;float spacing=weatherOptions.y<.5?6:8;
 float3 cell=floor(camera.origin/spacing)+float3(int(instance%side)-int(side/2),int(instance/(side*side))-int(layers/2),int((instance/side)%side)-int(side/2));
 float a=hash(cell+float3(13,7,11)),b=hash(cell+float3(3,19,5)),seed=hash(cell+float3(29,2,17));
 if(seed>weatherOptions.z||weatherOptions.x<.5){o.p=float4(2,2,0,1);return o;}
 float seconds=cameraPosition.w*weatherOptions.w,fall=weatherOptions.y<.5?14:weatherOptions.y<1.5?2.2:.3;
 float phase=frac(b-seconds*fall/spacing);float3 centre=cell*spacing+float3(a*spacing,phase*spacing,seed*spacing);
 if(weatherOptions.y>.5){centre.x+=sin(seconds*.7+b*13)*.6;centre.z+=cos(seconds*.6+a*11)*.6;}
 float2 corners[6]={float2(-1,-1),float2(-1,1),float2(1,1),float2(-1,-1),float2(1,1),float2(1,-1)};float2 uv=corners[vertex];float3 u,v;float brightness=1;
 if(weatherOptions.y<.5){u=camera.right.xyz*.012;v=float3(.10,.7+.4*b,0);}
 else if(weatherOptions.y<1.5){float yaw=seconds*.7+a*6.28,tilt=seconds*1.1+b*6.28;u=float3(cos(yaw),0,sin(yaw));v=float3(-sin(yaw)*sin(tilt),cos(tilt),cos(yaw)*sin(tilt));float3 normal=cross(u,v);brightness=.4+.6*abs(dot(normal,normalize(float3(.4,.8,.3))));float size=.11+.08*seed;u*=size;v*=size*1.5;}
 else{u=camera.right.xyz*.15;v=camera.up.xyz*.15;brightness=.6+.4*sin(seconds*2+seed*41);}
 float3 delta=centre+u*uv.x+v*uv.y-camera.origin;float w=dot(delta,camera.forward.xyz)*camera.forward.w;
 o.p=float4(dot(delta,camera.right.xyz)*camera.right.w,dot(delta,camera.up.xyz)*camera.up.w,camera.depthProjection.x*w+camera.depthProjection.y,w);
 if(w<.5)o.p=float4(2,2,0,1);
 o.uv=uv;o.depthTerms=camera.depthProjection.xy;o.tint=float4(weatherColour.rgb*brightness,weatherColour.a*smoothstep(0,.1,phase)*(1-smoothstep(.9,1,phase))*saturate(w*.25)*exp(-max(w,0)*.012));return o;
}
float4 weather(WeatherVertex i):SV_TARGET{
 float z=readDepth(int2(i.p.xy));bool reverse=i.depthTerms.y>0;if(reverse?i.p.z<z-.0000001:i.p.z>z+.0000001)discard;
 float opacity=0;
 if(weatherOptions.y<.5)opacity=(1-smoothstep(.3,1,abs(i.uv.x)))*(1-smoothstep(.55,1,abs(i.uv.y)));
 else if(weatherOptions.y<1.5){float shape=i.uv.x*i.uv.x+i.uv.y*i.uv.y;opacity=(1-smoothstep(.6,1,shape))*(.7+.3*(1-abs(i.uv.x)));}
 else{float radius=dot(i.uv,i.uv);opacity=exp(-radius*7)*.45+(1-smoothstep(.03,.09,radius))*.55;}
 return float4(i.tint.rgb,saturate(opacity*i.tint.a));
}

)"
R"(
float reflectionDepth(int2 pixel,Vertex camera){
 pixel=clamp(pixel,int2(0,0),int2(screen.xy)-1);
 if(screen.z<1.5)return sceneDepth.Load(int3(pixel,0));
 uint width,height,count;sceneDepthMS.GetDimensions(width,height,count);float z=camera.depthProjection.y>0?0:1;
 [loop]for(uint s=0;s<count;++s){float value=sceneDepthMS.Load(pixel,s);z=camera.depthProjection.y>0?max(z,value):min(z,value);}return z;
}
float reflectionDistance(float z,Vertex camera){float denominator=z-camera.depthProjection.x;return abs(denominator)>.0000001?max(0,camera.depthProjection.y/denominator):100000;}
float3 reflectionRay(float2 uv,Vertex camera){float2 ndc=uv*float2(2,-2)+float2(-1,1);return camera.right.xyz*ndc.x/camera.right.w+camera.up.xyz*ndc.y/camera.up.w+camera.forward.xyz/camera.forward.w;}
float3 reflectionPosition(int2 pixel,Vertex camera){float z=reflectionDepth(pixel,camera);return reflectionRay((pixel+.5)/screen.xy,camera)*reflectionDistance(z,camera);}
float3 reflectionNormal(int2 pixel,float3 surface,Vertex camera){
 float3 left=reflectionPosition(pixel-int2(1,0),camera),right=reflectionPosition(pixel+int2(1,0),camera),top=reflectionPosition(pixel-int2(0,1),camera),bottom=reflectionPosition(pixel+int2(0,1),camera);
 float3 dx=dot(right-surface,right-surface)<dot(surface-left,surface-left)?right-surface:surface-left;
 float3 dy=dot(bottom-surface,bottom-surface)<dot(surface-top,surface-top)?bottom-surface:surface-top;
 float3 normal=normalize(cross(dx,dy));return dot(normal,-surface)<0?-normal:normal;
}
float3 projectReflection(float3 surface,Vertex camera){float w=dot(surface,camera.forward.xyz)*camera.forward.w;return float3(float2(dot(surface,camera.right.xyz)*camera.right.w,-dot(surface,camera.up.xyz)*camera.up.w)/max(w,.00001)*.5+.5,w);}
bool reflectionSurface(Vertex camera,out float3 surface,out float3 normal){
 int2 pixel=int2(camera.uv*screen.xy);float z=reflectionDepth(pixel,camera);
 if(camera.depthProjection.y>0?z<.0000001:z>.9999999){surface=normal=0;return false;}
 surface=reflectionPosition(pixel,camera);normal=reflectionNormal(pixel,surface,camera);
 bool floor=normal.y>.75;
 bool wall=fogRange.z>.5&&abs(normal.y)<.25;
 return all(isfinite(normal))&&(floor||wall)&&length(surface)>.8;
}
float4 reflection(Vertex camera):SV_TARGET{
 float3 surface,normal;if(!reflectionSurface(camera,surface,normal))return 0;
 float3 direction=normalize(reflect(normalize(surface),normal));float3 origin=surface+normal*.035;
 float w0=dot(origin,camera.forward.xyz)*camera.forward.w,dw=dot(direction,camera.forward.xyz)*camera.forward.w;
 float reach=reflectionOptions.z;if(dw<0)reach=min(reach,max(0,(.12-w0)/dw));if(reach<=.03)return 0;
 float3 start=projectReflection(origin,camera),finish=projectReflection(origin+direction*reach,camera);
 if(any(start.xy<0)||any(start.xy>1))return 0;
 float2 projected=finish.xy-start.xy;
 float2 edge=float2(projected.x>0?1:0,projected.y>0?1:0);
 float2 clip=float2(abs(projected.x)>.00001?(edge.x-start.x)/projected.x:1,
                   abs(projected.y)>.00001?(edge.y-start.y)/projected.y:1);
 float visible=min(1,min(clip.x,clip.y));
 if(visible<=0)return 0;
 finish=float3(lerp(start.xy,finish.xy,visible),rcp(lerp(rcp(start.z),rcp(finish.z),visible)));
 float2 delta=(finish.xy-start.xy)*screen.xy;float span=max(abs(delta.x),abs(delta.y));if(span<1)return 0;
 int steps=screen.z>4?64:screen.z>1?96:128;
 float stride=max(1,span/steps),increment=stride/span,previous=0;
 [loop]for(int n=1;n<=steps;++n){
  float alpha=min(1,n*increment);float2 uv=lerp(start.xy,finish.xy,alpha);if(any(uv<.001)||any(uv>.999))break;
  float a0=max(previous,alpha-increment*.55),a1=min(1,alpha+increment*.55);
  float wNear=rcp(lerp(rcp(start.z),rcp(finish.z),a0)),wFar=rcp(lerp(rcp(start.z),rcp(finish.z),a1));
  float z=reflectionDepth(int2(uv*screen.xy),camera);float sceneW=reflectionDistance(z,camera);float thickness=max(.025,sceneW*.002);
  if(n>1&&sceneW>=min(wNear,wFar)-thickness&&sceneW<=max(wNear,wFar)+thickness){
   float lo=a0,hi=a1;
   [unroll]for(int refine=0;refine<5;++refine){float mid=(lo+hi)*.5;float2 q=lerp(start.xy,finish.xy,mid);float real=reflectionDistance(reflectionDepth(int2(q*screen.xy),camera),camera);float rayW=rcp(lerp(rcp(start.z),rcp(finish.z),mid));if(rayW>real)hi=mid;else lo=mid;}
   float hit=(lo+hi)*.5;float2 hitUV=lerp(start.xy,finish.xy,hit);float hitW=reflectionDistance(reflectionDepth(int2(hitUV*screen.xy),camera),camera);
   float rayW=rcp(lerp(rcp(start.z),rcp(finish.z),hit));float3 pointOnSurface=reflectionRay(hitUV,camera)*hitW;
   float planeDistance=dot(pointOnSurface-surface,normal);
   if(abs(rayW-hitW)<thickness*2.5&&planeDistance>.055){
    float edgeFade=saturate(min(min(hitUV.x,hitUV.y),min(1-hitUV.x,1-hitUV.y))*20);
    float facing=saturate(dot(normal,normalize(-surface))),fresnel=.35+.65*pow(1-facing,5);
    float3 c=sceneColour.Load(int3(int2(hitUV*screen.xy),0)).rgb;float coverage=edgeFade*fresnel;return float4(c*coverage,coverage);
   }
  }
  previous=alpha;if(alpha>=1)break;
 }
 return 0;
}
float4 reflectionComposite(Vertex camera):SV_TARGET{
 if(screen.w>0){
  float radius=reflectionOptions.y*8;
  float2 axis=(screen.w<1.5?float2(1,0):float2(0,1))*radius/screen.xy;
  float4 total=0;
  if(reflectionOptions.y<.25){
   [unroll]for(int x=-1;x<=1;++x)total+=reflectionResult.SampleLevel(linearClamp,camera.uv+x*axis,0)*(x==0?.5:.25);
  }else if(reflectionOptions.y<.6){
   const float kernel[5]={1,4,6,4,1};
   [unroll]for(int x=-2;x<=2;++x)total+=reflectionResult.SampleLevel(linearClamp,camera.uv+x*axis*.5,0)*(kernel[x+2]/16);
  }else{
   const float kernel[9]={1,8,28,56,70,56,28,8,1};
   [unroll]for(int x=-4;x<=4;++x)total+=reflectionResult.SampleLevel(linearClamp,camera.uv+x*axis*.25,0)*(kernel[x+4]/256);
  }
  return total;
 }
 float4 c=reflectionResult.SampleLevel(linearClamp,camera.uv,0);
 if(c.a<=.00001)return 0;
 float3 surface,normal;if(!reflectionSurface(camera,surface,normal))return 0;
 return float4(c.rgb/max(c.a,.00001),saturate(c.a*reflectionOptions.x));
}


float4 visibility(Vertex i):SV_TARGET{
 if(i.cameraValid<.5||aimOptions.x<.5||aimPoint.w<1)return 0;
 if(i.p.x>=2)return float4(i.forward.xyz,1);
 if(i.p.x>=1)return float4(i.origin,1);
 float3 delta=aimPoint.xyz-i.origin;
 float w=dot(delta,i.forward.xyz)*i.forward.w;
 if(!isfinite(w)||w<=.01)return 0;
 float2 ndc=float2(dot(delta,i.right.xyz)*i.right.w,dot(delta,i.up.xyz)*i.up.w)/w;
 int2 at=int2((ndc*float2(.5,-.5)+.5)*screen.xy);
 if(any(at<1)||any(at>=int2(screen.xy)-1))return 0;
 float expected=i.depthProjection.x+i.depthProjection.y/w;
 if(!isfinite(expected)||expected<0||expected>1)return 0;
 float clear=1,valid=1;
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x){
  float d=sceneDepth.Load(int3(at+int2(x,y),0));
  valid*=isfinite(d)&&d>=0&&d<=1?1:0;
  clear*=i.depthProjection.y>0?(expected>=d?1:0):(expected<=d?1:0);
 }
 return float4(clear,w,expected,valid);
}
float resolveNativeDepth(Vertex i):SV_TARGET{
 uint width,height,samples;sceneDepthMS.GetDimensions(width,height,samples);bool reverse=i.depthProjection.y>0;float z=reverse?0:1;
 [loop]for(uint s=0;s<samples;++s){float v=sceneDepthMS.Load(int2(i.p.xy),s);z=reverse?max(z,v):min(z,v);}return z;
}
float4 waterEnvironment(Vertex i):SV_TARGET{
 float longitude=(i.uv.x-.5)*6.2831853,latitude=(.5-i.uv.y)*3.14159265;float3 d=float3(sin(longitude)*cos(latitude),sin(latitude),cos(longitude)*cos(latitude));
 float3 colour;
 if(options.z>.5)colour=shadeSky(d);
 else{float sunAngle=(waterOptions.z-6)*.2617994;float3 sun=normalize(float3(cos(sunAngle)*.55,sin(sunAngle),.3));float day=smoothstep(-.15,.2,sun.y);colour=lerp(float3(.008,.014,.03),lerp(float3(.46,.65,.82),float3(.055,.19,.38),pow(saturate(d.y),.45)),day);colour+=pow(saturate(dot(d,sun)),800)*float3(5,4.3,3.2)*day;}
 return float4(max(colour,0),1);
}
)";
}
