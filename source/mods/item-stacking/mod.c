/* M20 initial coverage: physical Sand/Stone capsule stacks, using vanilla carry
 * and settle transactions so contents, IDs and world-save ownership survive. */
#include "../../runtime/native/gameplay.h"
#include "support.h"
#include "../../runtime/native/input.h"
#include "../../runtime/native/local_actor.h"
#include "stacking.h"
static int s_type(PTR type,const char *expected){
    if(!type)return 0;char *name=da.type_name(type);int matches=deq(name,expected);if(name)da.free(name);return matches;
}
static int update(int id,Context *c){
    (void)id;PTR walker=(PTR)c->rcx;if(!moon_local_actor_admit(walker)||!sx_thread())return 0;int ok=1;PTR world=sx_world(walker,&ok);
    int allowed=world&&ok&&sx_input(walker,&ok)&&sx_host(&ok)&&ok;
    MoonInputState input=moon_input_action("item-stacking.stack","!Shift+!Alt+Ctrl+K",allowed);
    if(allowed){if(input.pressed){sx_event("stack-request",0);stack(world,walker);}sx_title(status);}
    return 0;
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(!sx_start(domain,game,"item-stacking","MOONLAB_STACKING_CONFIG")||!moon_local_actor_bind())return 0;
    PTR world=find_class("MoonCleaners","MoonWorld"),item=find_class("MoonCleaners","MoonDroppedCanister");
    canisters=get_field(world,"canisters");carried=get_field(world,"carriedCanisters");id_field=get_field(item,"id");cargo_field=get_field(item,"cargo");
    position_field=get_field(item,"position");up_field=get_field(item,"up");rotation_field=get_field(item,"rotation");kilograms_field=get_field(item,"kilograms");capacity_field=get_field(item,"capacity");
    transform=dmethod("UnityEngine","Component","get_transform","");position=dmethod("UnityEngine","Transform","get_position","");
    carry=dmethod("MoonCleaners","MoonWorld","CarryDustCanister","System.String;System.Int32;UnityEngine.Vector3");
    settle=dmethod("MoonCleaners","MoonWorld","SettleCarriedCanister","System.String;System.Int32;UnityEngine.Vector3;UnityEngine.Quaternion");
    release=dmethod("MoonCleaners","MoonWorld","ReleaseCarriedCanister","System.String;System.Int32");
    is_carried=dmethod("MoonCleaners","MoonWorld","IsCanisterCarried","System.Int32");is_flying=dmethod("MoonCleaners","MoonWorld","IsCanisterFlying","System.Int32");
    lookup=dmethod("MoonCleaners","MoonWorld","TryGetCanister","System.Int32;MoonCleaners.MoonDroppedCanister&");
    carrier=dmethod("MoonCleaners","MoonWorld","CanisterCarrier","System.Int32");
    player_equal=dmethod("System","String","op_Equality","System.String;System.String");
    is_value=(unsigned char(*)(PTR))GetProcAddress(game,"il2cpp_class_is_valuetype");
    if(!canisters||!carried||!id_field||!cargo_field||!position_field||!up_field||!rotation_field||!transform||!position||!carry||!settle||!release||!is_carried||!is_flying||!lookup||!player_equal||!carrier||!kilograms_field||!capacity_field||!is_value)return 0;
    if(method_token(carry)!=100664754||method_token(settle)!=100664759||method_token(release)!=100664761||method_token(lookup)!=100664748||method_token(is_carried)!=100664752||method_token(is_flying)!=100664753||method_token(carrier)!=100665038)return 0;
    PTR (*field_type)(PTR)=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_field_get_type");
    PTR (*return_type)(PTR)=(PTR(*)(PTR))GetProcAddress(game,"il2cpp_method_get_return_type");
    if(!field_type||!return_type||!s_type(field_type(kilograms_field),"System.Double")||!s_type(field_type(capacity_field),"System.Double")||
       !s_type(return_type(carrier),"System.String")||!s_type(return_type(sx.player),"System.String")||!s_type(return_type(player_equal),"System.Boolean"))return 0;
    PTR cargo=find_class("MoonCleaners","MoonCargo"),sand=get_field(cargo,"Sand"),stone=get_field(cargo,"Stone");
    void (*static_get)(PTR,void *)=(void(*)(PTR,void *))GetProcAddress(game,"il2cpp_field_static_get_value");
    if(!static_get||!sand||!stone)return 0;static_get(sand,&sand_value);static_get(stone,&stone_value);
    count_limit=sx_limit("stacking","max_capsules",5,2,12);spacing_cm=sx_limit("stacking","spacing_cm",65,40,120);radius_cm=sx_limit("stacking","radius_cm",300,100,600);
    allow_sand=sx_limit("stacking","sand",1,0,1);allow_stone=sx_limit("stacking","stone",1,0,1);
    if(!sx_install(game,update))return 0;sx_event("ready",count_limit);return 1;
}
