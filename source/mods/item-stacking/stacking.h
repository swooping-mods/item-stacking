/* Synchronous, host-owned capsule transactions. Nothing survives this callback. */
typedef struct {float x,y,z,w;} SQuat;
typedef struct {int id,cargo;DVec position,up;SQuat rotation;double kilograms,capacity;float distance;} SItem;
typedef struct {DRoot world,walker,player;} SSession;
static PTR canisters,carried,id_field,cargo_field,position_field,up_field,rotation_field,kilograms_field,capacity_field;
static PTR transform,position,carry,settle,release,is_carried,is_flying,lookup,player_equal,carrier;
static unsigned char (*is_value)(PTR);
static int sand_value,stone_value;
static int count_limit=5,spacing_cm=65,radius_cm=300,allow_sand=1,allow_stone=1;
static const char *status="Moon Sweepers | Non-dust stacking | Ctrl+K stacks nearby Sand/Stone capsules";
static float distance2(DVec a,DVec b){float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
static int result(PTR method_ptr,PTR self,void **args,int *ok){PTR value=call(method_ptr,self,args,ok);if(!value||!*ok){*ok=0;return 0;}return *(unsigned char *)unbox(value)!=0;}
static int s_vec(DVec v){return v.x>=-1e9f&&v.x<=1e9f&&v.y>=-1e9f&&v.y<=1e9f&&v.z>=-1e9f&&v.z<=1e9f;}
static int s_valid(SItem a){
    float up=a.up.x*a.up.x+a.up.y*a.up.y+a.up.z*a.up.z;
    float q=a.rotation.x*a.rotation.x+a.rotation.y*a.rotation.y+a.rotation.z*a.rotation.z+a.rotation.w*a.rotation.w;
    return a.id>0&&s_vec(a.position)&&s_vec(a.up)&&up>.99f&&up<1.01f&&q>.99f&&q<1.01f&&
        a.kilograms>0&&a.kilograms<=1e12&&a.capacity>=a.kilograms&&a.capacity<=1e12;
}
static void s_record(PTR record,SItem *a){
    field_value(record,id_field,&a->id);field_value(record,cargo_field,&a->cargo);
    field_value(record,position_field,&a->position);field_value(record,up_field,&a->up);field_value(record,rotation_field,&a->rotation);
    field_value(record,kilograms_field,&a->kilograms);field_value(record,capacity_field,&a->capacity);
}
static int s_same_content(SItem a,SItem b){return a.id==b.id&&a.cargo==b.cargo&&a.kilograms==b.kilograms&&a.capacity==b.capacity;}
static int s_pose(SItem a,SItem b){
    return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&
        a.up.x==b.up.x&&a.up.y==b.up.y&&a.up.z==b.up.z&&a.rotation.x==b.rotation.x&&
        a.rotation.y==b.rotation.y&&a.rotation.z==b.rotation.z&&a.rotation.w==b.rotation.w;
}
static int s_read(SSession *s,int id,SItem *a,int *claimed,int *flying){
    int ok=1;PTR record=0;void *args[]={&id,&record};
    if(!result(lookup,dget(s->world),args,&ok)||!ok||!record)return 0;
    s_record(record,a);void *key[]={&id};
    *claimed=result(is_carried,dget(s->world),key,&ok);*flying=result(is_flying,dget(s->world),key,&ok);
    return ok&&a->id==id&&s_valid(*a);
}
static int s_context(SSession *s){
    int ok=1;PTR walker=dget(s->walker),world=dget(s->world);
    if(!walker||!world||!moon_local_actor_admit(walker)||sx_world(walker,&ok)!=world||!ok||!sx_host(&ok)||!ok)return 0;
    PTR player=call(sx.player,0,0,&ok);DRoot root=dhold(player);void *args[]={dget(s->player),dget(root)};
    int same=ok&&root&&result(player_equal,0,args,&ok)&&ok;ddrop(&root);return same;
}
static int s_idle(SSession *s){
    int ok=1;PTR busy=0;field_value(dget(s->world),carried,&busy);
    DRoot root=dhold(busy);if(!root)return 0;
    int count=integer(get_method(object_class(dget(root)),"get_Count",0),dget(root),&ok);ddrop(&root);
    return count==0&&ok;
}
static int s_ours(SSession *s,int id){
    int ok=1;void *key[]={&id};PTR owner=call(carrier,dget(s->world),key,&ok);
    DRoot root=dhold(owner);void *args[]={dget(s->player),dget(root)};
    int ours=ok&&root&&result(player_equal,0,args,&ok)&&ok;ddrop(&root);return ours;
}
static int s_unchanged(SSession *s,SItem expected){
    SItem now={0};int claimed=0,flying=0;
    return s_read(s,expected.id,&now,&claimed,&flying)&&!claimed&&!flying&&s_same_content(now,expected)&&s_pose(now,expected);
}
/* Cleanup requires a fresh observation. An exception can occur after mutation;
 * never assume either a false return or a thrown call left the record untouched. */
static int s_cleanup(SSession *s,SItem original){
    SItem now={0};int claimed=0,flying=0;
    if(!s_context(s)||!s_read(s,original.id,&now,&claimed,&flying)||flying||!s_same_content(now,original))return 0;
    if(!claimed)return s_pose(now,original);
    if(!s_ours(s,original.id)||!s_context(s))return 0;
    int ok=1;void *undo[]={dget(s->player),&original.id,&original.position,&original.rotation};
    result(settle,dget(s->world),undo,&ok);int uncertain=!ok;
    /* Settle may clear the claim before throwing. Re-read before any release. */
    if(!s_context(s)||!s_read(s,original.id,&now,&claimed,&flying)||flying||!s_same_content(now,original))return 0;
    if(claimed){
        if(!s_ours(s,original.id)||!s_context(s))return 0;
        ok=1;void *drop[]={dget(s->player),&original.id};call(release,dget(s->world),drop,&ok);
        if(!ok)uncertain=1;
    }
    return s_context(s)&&s_unchanged(s,original)?(uncertain?2:1):0;
}
static void s_disabled(const char *event,int id){sx.failed=1;status="Moon Sweepers | Stacking | Outcome uncertain; disabled; inspect capsules";sx_event(event,id);}
static void s_stack(SSession *s){
    int ok=1;PTR tr=call(transform,dget(s->walker),0,&ok);DRoot tr_root=dhold(tr);
    DVec origin={0};if(ok&&tr_root)origin=dvector(position,dget(tr_root),&ok);else ok=0;ddrop(&tr_root);
    if(!ok||!s_vec(origin)||!s_context(s))return;
    if(!s_idle(s)){status="Moon Sweepers | Stacking | Put down carried capsules first";return;}
    PTR dict=0;field_value(dget(s->world),canisters,&dict);
    DRoot dict_root=dhold(dict);if(!dict_root)return;
    PTR values=call(get_method(object_class(dget(dict_root)),"get_Values",0),dget(dict_root),0,&ok);DRoot values_root=dhold(values);
    PTR iterator=0;if(values_root&&ok)iterator=call(get_method(object_class(dget(values_root)),"GetEnumerator",0),dget(values_root),0,&ok);
    DRoot root=dhold(iterator);ddrop(&values_root);ddrop(&dict_root);if(!root||!ok){ddrop(&root);return;}PTR klass=object_class(iterator);
    PTR move=get_method(klass,"MoveNext",0),current=get_method(klass,"get_Current",0),dispose=get_method(klass,"Dispose",0);
    SItem nearby[32];int count=0,scanned=0;float radius=(float)radius_cm*.01f;
    while(ok){
        iterator=dget(root);PTR self=is_value(klass)?unbox(iterator):iterator;if(!boolean(move,self,&ok))break;
        if(++scanned>2048){ok=0;break;}PTR item=call(current,self,0,&ok);if(!item||!ok){ok=0;break;}
        SItem candidate={0};s_record(item,&candidate);
        if(!s_valid(candidate)||!((candidate.cargo==sand_value&&allow_sand)||(candidate.cargo==stone_value&&allow_stone)))continue;
        candidate.distance=distance2(candidate.position,origin);if(candidate.distance>radius*radius)continue;
        void *a[]={&candidate.id};if(result(is_carried,dget(s->world),a,&ok)||result(is_flying,dget(s->world),a,&ok))continue;
        /* Equal distances use stable IDs rather than dictionary iteration order. */
        int index=count;if(count==32){index=0;for(int i=1;i<count;i++)if(nearby[i].distance>nearby[index].distance||(nearby[i].distance==nearby[index].distance&&nearby[i].id>nearby[index].id))index=i;
            if(candidate.distance>nearby[index].distance||(candidate.distance==nearby[index].distance&&candidate.id>=nearby[index].id))continue;}
        else count++;nearby[index]=candidate;
    }
    iterator=dget(root);int disposed=1;if(dispose)call(dispose,is_value(klass)?unbox(iterator):iterator,0,&disposed);else disposed=0;ddrop(&root);
    if(!ok||!disposed){status="Moon Sweepers | Stacking | Capsule enumeration refused";sx_event("enumeration-failed",scanned);return;}
    if(count<2){status="Moon Sweepers | Stacking | Need two nearby Sand or Stone capsules";return;}
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(nearby[j].distance<nearby[i].distance||(nearby[j].distance==nearby[i].distance&&nearby[j].id<nearby[i].id)){SItem swap=nearby[i];nearby[i]=nearby[j];nearby[j]=swap;}
    SItem base=nearby[0];int placed=1,stopped=0;float step=(float)spacing_cm*.01f;
    for(int i=1;i<count&&placed<count_limit;i++){
        SItem item=nearby[i];if(item.cargo!=base.cargo)continue;
        if(!s_context(s)||!s_idle(s)||!s_unchanged(s,base)||!s_unchanged(s,item)){
            status="Moon Sweepers | Stacking | Stopped; world or capsules changed";sx_event("stack-stale",item.id);stopped=1;break;
        }
        DVec target={base.position.x+base.up.x*step*(float)placed,base.position.y+base.up.y*step*(float)placed,base.position.z+base.up.z*step*(float)placed};
        if(!s_vec(target)){s_disabled("target-invalid",item.id);stopped=1;break;}
        if(!s_context(s)){stopped=1;status="Moon Sweepers | Stacking | Stopped; local authority changed";break;}
        ok=1;void *take[]={dget(s->player),&item.id,&origin};int taken=result(carry,dget(s->world),take,&ok);
        if(!taken||!ok){
            int cleaned=s_cleanup(s,item);
            if(!cleaned){s_disabled("carry-cleanup-incomplete",item.id);stopped=1;break;}
            if(!ok||cleaned==2){s_disabled("carry-outcome-uncertain",item.id);stopped=1;break;}
            continue;
        }
        SItem held={0};int claimed=0,flying=0;
        if(!s_context(s)||!s_read(s,item.id,&held,&claimed,&flying)||!claimed||flying||!s_same_content(held,item)||!s_ours(s,item.id)){
            s_disabled("carry-outcome-uncertain",item.id);stopped=1;break;
        }
        if(!s_context(s)){s_disabled("carry-authority-lost",item.id);stopped=1;break;}
        void *put[]={dget(s->player),&item.id,&target,&base.rotation};int accepted=result(settle,dget(s->world),put,&ok);
        if(!accepted||!ok){
            int restored=s_cleanup(s,item);stopped=1;
            if(!restored)s_disabled("placement-cleanup-incomplete",item.id);
            else if(!ok||restored==2)s_disabled("placement-outcome-uncertain",item.id);
            else {status="Moon Sweepers | Stacking | Placement refused; capsule restored";sx_event("placement-refused-restored",item.id);}
            break;
        }
        /* Settle owns the radial up vector. Verify its observed pose rather than
         * assuming the base capsule's up is written by this API. */
        SItem expected=item;expected.position=target;expected.rotation=base.rotation;
        SItem observed={0};int observed_claim=0,observed_flying=0;
        if(s_read(s,item.id,&observed,&observed_claim,&observed_flying))expected.up=observed.up;
        if(!s_context(s)||!s_unchanged(s,expected)){s_disabled("placement-outcome-uncertain",item.id);stopped=1;break;}
        placed++;
    }
    if(placed>1){sx_event(stopped?"capsules-stacked-partial":"capsules-stacked",placed);if(!stopped)status="Moon Sweepers | Stacking | Stack placed; contents unchanged";}
    else if(!stopped)status="Moon Sweepers | Stacking | No matching reachable capsules could be placed";
}
static void stack(PTR world,PTR walker){
    int ok=1;SSession session={dhold(world),dhold(walker),0};
    PTR player=call(sx.player,0,0,&ok);if(ok)session.player=dhold(player);
    if(session.world&&session.walker&&session.player)s_stack(&session);
    ddrop(&session.player);ddrop(&session.walker);ddrop(&session.world);
}
