// Shared fake-only store assertions, compiled by the existing layer/fence targets.
static void rawSnapshotStoreChecks() {
    using Store=dbce::input::RawSnapshotStore;
    Store store;int a=0,b=0,blind=0;
    const GUID guid={0xabcdef01,0,0,{0}},other={0xabcdef02,0,0,{0}},unknown{};
    const auto ga=store.Register(&a,guid,true);
    auto format=store.BeginMutation(&a,true);store.EndMutation(format,2);
    DIJOYSTATE2 raw{};raw.lX=111;raw.lRz=777;
    Store::Snapshot out;
    auto epoch=store.BeginInvocation();auto read=store.BeginRead(&a,100);
    store.CompleteRead(read,DI_OK,sizeof raw,&raw,105);store.Seal(epoch);
    check(store.ReadUnique(guid,epoch,106,20,out)&&out.generation==ga&&out.state.lX==111&&out.state.lRz==777,
          "raw snapshot: exact owned standard pair/identity/current invocation");
    raw.lX=999;
    check(store.ReadUnique(guid,epoch,106,20,out)&&out.state.lX==111,"raw snapshot: caller buffer never retained");
    check(!store.ReadUnique(guid,epoch,200,20,out)&&out.generation==0,"raw snapshot: age from start, failed copy clears output");
    check(!store.ReadUnique(guid,epoch,104,20,out),"raw snapshot: future completion refused");
    store.EndInvocation(epoch);
    check(!store.ReadUnique(guid,epoch,106,20,out),"raw snapshot: ended invocation cannot be reused");
    epoch=store.BeginInvocation();store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,110,20,out),"raw snapshot: new invocation with no read has no last-good sample");
    store.EndInvocation(epoch);

    epoch=store.BeginInvocation();read=store.BeginRead(&a,120);
    store.CompleteRead(read,DI_OK,sizeof raw,&raw,180);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,181,20,out),"raw snapshot: late completion cannot refresh an old read start");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,119);
    store.CompleteRead(read,DI_OK,sizeof raw,&raw,121);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,122,20,out),"raw snapshot: backward start clock cannot admit a sample");
    store.EndInvocation(epoch);

    epoch=store.BeginInvocation();auto older=store.BeginRead(&a,200),newer=store.BeginRead(&a,201);
    raw.lX=222;store.CompleteRead(newer,DI_OK,sizeof raw,&raw,202);
    raw.lX=111;store.CompleteRead(older,DI_OK,sizeof raw,&raw,203);store.Seal(epoch);
    check(store.ReadUnique(guid,epoch,204,20,out)&&out.serial==newer.serial&&out.state.lX==222,
          "raw snapshot: backward completions cannot overwrite newer success");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,205);
    std::thread foreign([&] {
        store.CompleteRead(read,DI_OK,sizeof raw,&raw,206);
        store.EndInvocation(epoch);
    });foreign.join();const bool ownerStillActive=store.Seal(epoch);
    check(ownerStillActive&&!store.ReadUnique(guid,epoch,207,20,out),"raw snapshot: foreign completion cannot publish or end owner invocation");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,208);store.Seal(epoch);
    store.CompleteRead(read,DI_OK,sizeof raw,&raw,209);
    check(!store.ReadUnique(guid,epoch,209,20,out),"raw snapshot: completion after seal cannot publish");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();older=store.BeginRead(&a,210);newer=store.BeginRead(&a,211);
    store.CompleteRead(newer,DIERR_INPUTLOST,sizeof raw,&raw,212);
    store.CompleteRead(older,DI_OK,sizeof raw,&raw,213);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,214,20,out),"raw snapshot: failure vetoes older in-flight success");
    store.EndInvocation(epoch);

    epoch=store.BeginInvocation();older=store.BeginRead(&a,300);
    auto mutation=store.BeginMutation(&a,true);auto during=store.BeginRead(&a,301);
    store.CompleteRead(during,DI_OK,sizeof raw,&raw,302);store.EndMutation(mutation,2);
    store.CompleteRead(older,DI_OK,sizeof raw,&raw,303);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,304,20,out),"raw snapshot: read during mutation and old completion remain unavailable");
    store.EndInvocation(epoch);
    mutation=store.BeginMutation(&a,true);store.EndMutation(mutation); // failed format: raw descriptor unknown
    epoch=store.BeginInvocation();read=store.BeginRead(&a,310);store.CompleteRead(read,DI_OK,sizeof raw,&raw,311);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,312,20,out),"raw snapshot: failed layout mutation does not revive descriptor/sample");
    store.EndInvocation(epoch);
    mutation=store.BeginMutation(&a,true);store.EndMutation(mutation,2);
    auto nested=store.BeginMutation(&a),nested2=store.BeginMutation(&a,true);
    store.EndMutation(nested2,2);store.EndMutation(nested);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,320);store.CompleteRead(read,DI_OK,sizeof raw,&raw,321);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,322,20,out),"raw snapshot: overlapping mutations require new descriptor validation");
    store.EndInvocation(epoch);
    mutation=store.BeginMutation(&a,true);store.EndMutation(mutation,2);

    const auto gb=store.Register(&b,guid,true);format=store.BeginMutation(&b,true);store.EndMutation(format,2);
    check(gb>ga,"raw snapshot: same GUID retains distinct monotonic object generations");
    epoch=store.BeginInvocation();read=store.BeginRead(&a,400);store.CompleteRead(read,DI_OK,sizeof raw,&raw,401);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,402,20,out),"raw snapshot: duplicate live GUID is ambiguous");
    store.Retire(&b,gb);
    check(!store.ReadUnique(guid,epoch,403,20,out),"raw snapshot: retiring ambiguity cannot revive old sample");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();older=store.BeginRead(&a,410);store.EndInvocation(epoch);
    store.Retire(&a,ga);const auto replacement=store.Register(&a,guid,true);
    format=store.BeginMutation(&a,true);store.EndMutation(format,2);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,411);store.CompleteRead(read,DI_OK,sizeof raw,&raw,412);
    store.CompleteRead(older,DIERR_INPUTLOST,sizeof raw,&raw,413);store.Retire(&a,ga);store.Seal(epoch);
    check(store.ReadUnique(guid,epoch,414,20,out)&&out.generation==replacement&&replacement>gb,
          "raw snapshot: old failure/final-release cleanup cannot touch reused address");
    const auto copied=out;store.Invalidate(&a,replacement);
    check(copied.state.lRz==raw.lRz&&!store.ReadUnique(guid,epoch,415,20,out),"raw snapshot: returned value copy survives invalidation; next admission neutral");
    store.EndInvocation(epoch);

    const auto unverified=store.Register(&blind,unknown,false);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,500);store.CompleteRead(read,DI_OK,sizeof raw,&raw,501);store.Seal(epoch);
    check(!store.ReadUnique(guid,epoch,502,20,out),"raw snapshot: unverified identity cannot stand in for requested GUID");
    store.Retire(&blind,unverified);store.EndInvocation(epoch);
    const auto different=store.Register(&b,other,true);format=store.BeginMutation(&b,true);store.EndMutation(format,2);
    epoch=store.BeginInvocation();read=store.BeginRead(&a,510);store.CompleteRead(read,DI_OK,sizeof raw,&raw,511);
    auto unrelated=store.BeginRead(&b,512);store.CompleteRead(unrelated,DIERR_INPUTLOST,sizeof raw,&raw,513);store.Seal(epoch);
    check(store.ReadUnique(guid,epoch,514,20,out),"raw snapshot: unrelated device failure does not invalidate selected GUID");
    store.EndInvocation(epoch);
    epoch=store.BeginInvocation();check(store.BeginInvocation()==0&&store.BeginInvocation()==0&&!store.Seal(epoch),"raw snapshot: nested owner poisons admission but retains occupancy");
    store.EndInvocation(epoch);auto fresh=store.BeginInvocation();check(fresh>epoch,"raw snapshot: outer End permits independent owner");store.EndInvocation(fresh);

    format=store.BeginMutation(&a,true);store.EndMutation(format,1);
    DIJOYSTATE legacyState{};legacyState.lX=123;legacyState.rgbButtons[31]=0x80;
    epoch=store.BeginInvocation();read=store.BeginRead(&a,600);store.CompleteRead(read,S_FALSE,sizeof legacyState,&legacyState,601);store.Seal(epoch);
    check(store.ReadUnique(guid,epoch,602,20,out)&&out.bytes==sizeof legacyState&&out.state.rgbButtons[31]==0x80&&out.state.rgbButtons[32]==0,
          "raw snapshot: successful legacy standard format copies only valid prefix");
    store.EndInvocation(epoch);
    Store another;int c=0;check(another.Register(&c,guid,true)>different,"raw snapshot: generations do not reset with another store");
    Store refused;int bad=0;check(refused.Register(&bad,guid,true,false)==0&&refused.BeginInvocation()==0,"raw snapshot: hook-admission failure disables optional store");
}
