/* Local reflection/input boundary; no runtime dependency on another feature. */
#include "api.h"
#include "../../runtime/native/input.h"
int _fltused=0;
static struct {
    PTR log,world_field,restoring,typing,blocked,focused,alive,joined,host,local,player;
    short (*key)(int);PTR (*foreground)(void);DWORD (*pid)(PTR,DWORD *);
    BOOL (*title)(PTR,const char *);DWORD thread;int failed;
    DWORD (__stdcall *ini_int)(const char *,const char *,int,const char *);
    DWORD (__stdcall *ini_string)(const char *,const char *,const char *,char *,DWORD,const char *);
    char config[1100];
} sx;
static inline void sx_event(const char *name,int amount){
    char b[512],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\",\"amount\":");
    if(amount<0){p=cat(p,"-");amount=-amount;}p=num(p,(U64)amount);p=cat(p,"}\n");
    DWORD n=0;if(sx.log){WriteFile(sx.log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(sx.log);}
}
static inline int sx_down(int vk){return moon_input_key((unsigned)vk);}
static inline int sx_host(int *ok){
    if(!boolean(sx.joined,0,ok))return *ok;
    PTR h=call(sx.host,0,0,ok),l=call(sx.local,0,0,ok);U64 a=0,b=0;
    if(h)dcopy(&a,unbox(h),8);if(l)dcopy(&b,unbox(l),8);return *ok&&a&&a==b;
}
static inline PTR sx_world(PTR walker,int *ok){
    PTR world=0;field_value(walker,sx.world_field,&world);if(!world)return 0;
    void *a[]={world};PTR live=call(sx.alive,0,a,ok);
    if(!*ok||!live||!*(unsigned char *)unbox(live)||boolean(sx.restoring,world,ok))return 0;
    return *ok?world:0;
}
static inline int sx_input(PTR walker,int *ok){
    PTR win=sx.foreground();DWORD pid=0;if(win)sx.pid(win,&pid);
    return pid==GetCurrentProcessId()&&boolean(sx.focused,0,ok)&&!boolean(sx.typing,0,ok)&&!boolean(sx.blocked,walker,ok)&&*ok;
}
static inline void sx_title(const char *text){PTR win=sx.foreground();DWORD pid=0;if(win)sx.pid(win,&pid);if(pid==GetCurrentProcessId())sx.title(win,text);}
static inline int sx_limit(const char *section,const char *key,int fallback,int low,int high){int v=(int)sx.ini_int(section,key,fallback,sx.config);return v<low||v>high?fallback:v;}
static inline int sx_start(PTR domain,PTR game,const char *id,const char *config_env){
    if(!bind_runtime(domain,game)||!dapi(game))return 0;
    char path[1200];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",path,1000);
    if(n&&n<1000){char *p=cat(path+n,"\\");p=cat(p,id);cat(p,".jsonl");sx.log=CreateFileA(path,0x40000000,1,0,1,0x80,0);if(sx.log==(PTR)(U64)-1)sx.log=0;}
    n=GetEnvironmentVariableA(config_env,sx.config,sizeof(sx.config));
    if(!n||n>=sizeof(sx.config)){char *p=cat(sx.config,".\\MoonLab\\mods\\");p=cat(p,id);cat(p,"\\settings.ini");}
    PTR k=GetModuleHandleA("kernel32.dll"),u=LoadLibraryA("user32.dll");if(!k||!u)return 0;
    sx.ini_int=(DWORD(__stdcall *)(const char *,const char *,int,const char *))GetProcAddress(k,"GetPrivateProfileIntA");
    sx.ini_string=(DWORD(__stdcall *)(const char *,const char *,const char *,char *,DWORD,const char *))GetProcAddress(k,"GetPrivateProfileStringA");
    sx.key=(short(*)(int))GetProcAddress(u,"GetAsyncKeyState");sx.foreground=(PTR(*)(void))GetProcAddress(u,"GetForegroundWindow");
    sx.pid=(DWORD(*)(PTR,DWORD *))GetProcAddress(u,"GetWindowThreadProcessId");sx.title=(BOOL(*)(PTR,const char *))GetProcAddress(u,"SetWindowTextA");
#define SX(F,N,T,M,S) sx.F=dmethod(N,T,M,S);if(!sx.F)return 0
    SX(restoring,"MoonCleaners","MoonWorld","get_IsRestoringRendering","");SX(typing,"MoonCleaners","MoonInput","get_Typing","");
    SX(blocked,"MoonCleaners","MoonWalker","get_MenuBlocked","");SX(focused,"UnityEngine","Application","get_isFocused","");
    SX(alive,"UnityEngine","Object","op_Implicit","UnityEngine.Object");SX(joined,"MoonCleaners","MoonCrew","get_Joined","");
    SX(host,"MoonCleaners","MoonCrew","get_HostId","");SX(local,"Steamworks","SteamUser","GetSteamID","");SX(player,"MoonCleaners","MoonCrew","get_LocalPlayerId","");
#undef SX
    sx.world_field=get_field(find_class("MoonCleaners","MoonWalker"),"world");
    return sx.world_field&&sx.ini_int&&sx.ini_string&&sx.key&&sx.foreground&&sx.pid&&sx.title;
}
static inline int sx_thread(void){DWORD thread=GetCurrentThreadId();if(!sx.thread)sx.thread=thread;if(thread!=sx.thread)sx.failed=1;return !sx.failed;}
static inline int sx_install(PTR game,int (*callback)(int,Context *)){
    if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game))return 0;
    before_hook=callback;return install_hooks();
}
