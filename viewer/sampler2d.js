function sampleCurve({model,lo,hi,kind,detail=400,maxMs=1200}){
  const deadline=performance.now()+maxMs,positions=[],segments=[];let capped=false,explicit=null;
  const lhs=compileExpression(model.lhs),rhs=compileExpression(model.rhs),p=[0,0,0];
  if(kind==='numberline'){
    scanRoots(x=>{p[0]=x;return lhs(p)-rhs(p);},lo[0],hi[0],Math.max(1024,detail*4),x=>positions.push(x,0),deadline);
  }else{
    for(const [left,right] of [[model.lhs,model.rhs],[model.rhs,model.lhs]])if(Array.isArray(left)&&left[0]==='var'&&left[1]<2&&!usesCoordinate(right,left[1])){explicit={axis:left[1],fn:compileExpression(right)};break;}
    if(explicit){
      const {axis,fn}=explicit,a=1-axis,n=Math.max(1024,detail*8);let last=null;
      for(let i=0;i<=n;i++){
        if((i&255)===0&&performance.now()>deadline){capped=true;break;}
        p[a]=lo[a]+(hi[a]-lo[a])*i/n;p[axis]=fn(p);
        if(!Number.isFinite(p[axis])||p[axis]<lo[axis]||p[axis]>hi[axis]){last=null;continue;}
        positions.push(p[0],p[1]);if(last&&Math.abs(last[axis]-p[axis])<(hi[axis]-lo[axis])*.15)segments.push(last[0],last[1],p[0],p[1]);last=[p[0],p[1]];
      }
    }else{
      const n=Math.max(64,Math.min(700,detail));
      outer:for(let i=0;i<=n;i++)for(let axis=0;axis<2;axis++){
        if(performance.now()>deadline||positions.length>200000){capped=true;break outer;}
        const a=1-axis;p[a]=lo[a]+(hi[a]-lo[a])*i/n;
        if(!scanRoots(x=>{p[axis]=x;return lhs(p)-rhs(p);},lo[axis],hi[axis],n,x=>{p[axis]=x;positions.push(p[0],p[1]);},deadline)){capped=true;break outer;}
      }
    }
  }
  return {positions:new Float32Array(positions),segments:new Float32Array(segments),capped};
}
if(typeof module!=='undefined'){Object.assign(globalThis,require('./math.js'));module.exports={sampleCurve};}
