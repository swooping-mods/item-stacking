/* M20 initial coverage: physical Sand/Stone capsule stacks, using vanilla carry
 * and settle transactions so contents, IDs and world-save ownership survive. */
#include "../../runtime/native/gameplay.h"
#include "support.h"
#include "../../runtime/native/input.h"
typedef struct {float x,y,z,w;} SQuat;
typedef struct {int id,cargo;DVec position,up;SQuat rotation;float distance;} SItem;
static PTR canisters,carried,id_field,cargo_field,position_field,up_field,rotation_field;
static PTR transform,position,carry,settle,release,is_carried,is_flying;
static unsigned char (*is_value)(PTR);
static int sand_value,stone_value;
static int count_limit=5,spacing_cm=65,radius_cm=300,allow_sand=1,allow_stone=1;
static const char *status="Moon Sweepers | Non-dust stacking | Ctrl+K stacks nearby Sand/Stone capsules";
static float distance2(DVec a,DVec b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
static int result(PTR method_ptr,PTR self,void **args,int *ok){PTR value=call(method_ptr,self,args,ok);if(!value){*ok=0;return 0;}return *(unsigned char *)unbox(value)!=0;}
static void stack(PTR world,PTR walker){
    int ok=1;PTR tr=call(transform,walker,0,&ok);DVec origin=dvector(position,tr,&ok);
    PTR player=call(sx.player,0,0,&ok),dict=0,busy=0;field_value(world,canisters,&dict);field_value(world,carried,&busy);
    if(!ok||!player||!dict||!busy)return;
    /* Never interrupt an existing vanilla carry session, including another peer. */
    if(integer(get_method(object_class(busy),"get_Count",0),busy,&ok)){status="Moon Sweepers | Stacking | Put down carried capsules first";return;}
    PTR values=call(get_method(object_class(dict),"get_Values",0),dict,0,&ok);if(!values||!ok)return;
    PTR iterator=call(get_method(object_class(values),"GetEnumerator",0),values,0,&ok);if(!iterator||!ok)return;
    DRoot root=dhold(iterator);if(!root)return;PTR klass=object_class(iterator);
    PTR move=get_method(klass,"MoveNext",0),current=get_method(klass,"get_Current",0),dispose=get_method(klass,"Dispose",0);
    SItem nearby[32];int count=0,scanned=0;float radius=(float)radius_cm*.01f;
    while(ok){
        iterator=dget(root);PTR self=is_value(klass)?unbox(iterator):iterator;if(!boolean(move,self,&ok))break;
        if(++scanned>2048){ok=0;break;}PTR item=call(current,self,0,&ok);if(!item||!ok)break;
        SItem candidate={0};field_value(item,id_field,&candidate.id);field_value(item,cargo_field,&candidate.cargo);
        if(!((candidate.cargo==sand_value&&allow_sand)||(candidate.cargo==stone_value&&allow_stone)))continue;
        field_value(item,position_field,&candidate.position);field_value(item,up_field,&candidate.up);field_value(item,rotation_field,&candidate.rotation);
        candidate.distance=distance2(candidate.position,origin);if(candidate.distance>radius*radius)continue;
        void *a[]={&candidate.id};if(result(is_carried,world,a,&ok)||result(is_flying,world,a,&ok))continue;
        /* Keep the nearest 32 records; iteration order cannot select distant loot. */
        int index=count;if(count==32){index=0;for(int i=1;i<count;i++)if(nearby[i].distance>nearby[index].distance)index=i;if(candidate.distance>=nearby[index].distance)continue;}
        else count++;nearby[index]=candidate;
    }
    iterator=dget(root);if(dispose)call(dispose,is_value(klass)?unbox(iterator):iterator,0,&ok);ddrop(&root);
    if(!ok){status="Moon Sweepers | Stacking | Capsule enumeration refused";sx_event("enumeration-failed",scanned);return;}
    if(count<2){status="Moon Sweepers | Stacking | Need two nearby Sand or Stone capsules";return;}
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(nearby[j].distance<nearby[i].distance){SItem swap=nearby[i];nearby[i]=nearby[j];nearby[j]=swap;}
    SItem base=nearby[0];int placed=1;float step=(float)spacing_cm*.01f;
    for(int i=1;i<count&&placed<count_limit;i++){
        SItem item=nearby[i];if(item.cargo!=base.cargo)continue;
        DVec target={base.position.x+base.up.x*step*(float)placed,base.position.y+base.up.y*step*(float)placed,base.position.z+base.up.z*step*(float)placed};
        void *take[]={player,&item.id,&origin};if(!result(carry,world,take,&ok)){if(!ok)break;continue;}
        void *put[]={player,&item.id,&target,&base.rotation};int accepted=result(settle,world,put,&ok);
        if(!accepted||!ok){
            /* A placement refusal must not leave an invisible carry claim. */
            int cleanup=1;void *undo[]={player,&item.id,&item.position,&item.rotation};int restored=result(settle,world,undo,&cleanup);
            void *drop[]={player,&item.id};call(release,world,drop,&cleanup);
            status=restored&&cleanup?"Moon Sweepers | Stacking | Placement refused; capsule restored":"Moon Sweepers | Stacking | Placement failed; inspect capsule position";sx_event(restored&&cleanup?"placement-refused-restored":"placement-cleanup-incomplete",item.id);break;
        }
        placed++;
    }
    if(placed>1){status="Moon Sweepers | Stacking | Stack placed; contents unchanged";sx_event("capsules-stacked",placed);}
    else if(ok)status="Moon Sweepers | Stacking | No matching reachable capsules could be placed";
    if(!ok){sx.failed=1;status="Moon Sweepers | Stacking | Adapter error; disabled";sx_event("adapter-failed",0);}
    (void)origin;
}
static int update(int id,Context *c){
    (void)id;if(!sx_thread())return 0;PTR walker=(PTR)c->rcx;int ok=1;PTR world=sx_world(walker,&ok);
    int allowed=world&&ok&&sx_input(walker,&ok)&&sx_host(&ok)&&ok;
    MoonInputState input=moon_input_action("item-stacking.stack","!Shift+!Alt+Ctrl+K",allowed);
    if(allowed){if(input.pressed){sx_event("stack-request",0);stack(world,walker);}sx_title(status);}
    return 0;
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(!sx_start(domain,game,"item-stacking","MOONLAB_STACKING_CONFIG"))return 0;
    PTR world=find_class("MoonCleaners","MoonWorld"),item=find_class("MoonCleaners","MoonDroppedCanister");
    canisters=get_field(world,"canisters");carried=get_field(world,"carriedCanisters");id_field=get_field(item,"id");cargo_field=get_field(item,"cargo");
    position_field=get_field(item,"position");up_field=get_field(item,"up");rotation_field=get_field(item,"rotation");
    transform=dmethod("UnityEngine","Component","get_transform","");position=dmethod("UnityEngine","Transform","get_position","");
    carry=dmethod("MoonCleaners","MoonWorld","CarryDustCanister","System.String;System.Int32;UnityEngine.Vector3");
    settle=dmethod("MoonCleaners","MoonWorld","SettleCarriedCanister","System.String;System.Int32;UnityEngine.Vector3;UnityEngine.Quaternion");
    release=dmethod("MoonCleaners","MoonWorld","ReleaseCarriedCanister","System.String;System.Int32");
    is_carried=dmethod("MoonCleaners","MoonWorld","IsCanisterCarried","System.Int32");is_flying=dmethod("MoonCleaners","MoonWorld","IsCanisterFlying","System.Int32");
    is_value=(unsigned char(*)(PTR))GetProcAddress(game,"il2cpp_class_is_valuetype");
    if(!canisters||!carried||!id_field||!cargo_field||!position_field||!up_field||!rotation_field||!transform||!position||!carry||!settle||!release||!is_carried||!is_flying||!is_value)return 0;
    if(method_token(carry)!=100664754||method_token(settle)!=100664759)return 0;
    PTR cargo=find_class("MoonCleaners","MoonCargo"),sand=get_field(cargo,"Sand"),stone=get_field(cargo,"Stone");
    void (*static_get)(PTR,void *)=(void(*)(PTR,void *))GetProcAddress(game,"il2cpp_field_static_get_value");
    if(!static_get||!sand||!stone)return 0;static_get(sand,&sand_value);static_get(stone,&stone_value);
    count_limit=sx_limit("stacking","max_capsules",5,2,12);spacing_cm=sx_limit("stacking","spacing_cm",65,40,120);radius_cm=sx_limit("stacking","radius_cm",300,100,600);
    allow_sand=sx_limit("stacking","sand",1,0,1);allow_stone=sx_limit("stacking","stone",1,0,1);
    if(!sx_install(game,update))return 0;sx_event("ready",count_limit);return 1;
}
