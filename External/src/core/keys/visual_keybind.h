#pragma once
namespace VisualKeybind {
struct State {
 bool active=false, lastDown=false; int oldTarget=0,oldMode=0,oldKey=0;
 void Update(bool enabled,int target,int mode,int key,bool down,bool acceptInput,bool& native,bool& mesh){
  const bool usable=enabled && key>0;
  const bool changed=active!=usable || oldTarget!=target || oldMode!=mode || oldKey!=key;
  if(changed){
   if(active && oldMode==1)(oldTarget==0?native:mesh)=false;
   lastDown=down;
  }
  if(usable){
   bool& value=target==0?native:mesh;
   if(mode==1)value=acceptInput && down;
   else if(acceptInput && down && !lastDown)value=!value;
  }
  active=usable;oldTarget=target;oldMode=mode;oldKey=key;lastDown=down;
 }
};
}
