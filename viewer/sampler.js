/* Bounded numerical sampling. Heavy geometry and normal calculations stay in a worker. */
function sampleSurface({model,lo,hi,detail,maxMs=1800}){
  const started=performance.now(),deadline=started+maxMs,positions=[],triangles=[],MAX_POINTS=120000;
  let capped=false,explicit=null;
  for(const [left,right] of [[model.lhs,model.rhs],[model.rhs,model.lhs]])if(Array.isArray(left)&&left[0]==='var'&&!usesCoordinate(right,left[1])){explicit={axis:left[1],fn:compileExpression(right)};break;}
  const complexity=n=>Array.isArray(n)?1+n.slice(1).reduce((s,c)=>s+complexity(c),0):1;
  const cost=complexity(model.lhs)+complexity(model.rhs),ceiling=explicit?128:Math.max(24,Math.min(96,Math.floor(Math.cbrt(18000000/(cost*6)))));
  const n=Math.max(4,Math.min(ceiling,Math.round(detail))),adaptive=n<detail;
  if(explicit){
    const {axis,fn}=explicit,a=(axis+1)%3,b=(axis+2)%3,steps=n*2,grid=new Int32Array((steps+1)*(steps+1)).fill(-1),p=[0,0,0];
    outer:for(let i=0;i<=steps;i++){
      if(performance.now()>deadline){capped=true;break;}
      for(let j=0;j<=steps;j++){
        p[a]=lo[a]+(hi[a]-lo[a])*i/steps;p[b]=lo[b]+(hi[b]-lo[b])*j/steps;p[axis]=fn(p);
        if(Number.isFinite(p[axis])&&p[axis]>=lo[axis]&&p[axis]<=hi[axis]){grid[i*(steps+1)+j]=positions.length/3;positions.push(p[0],p[1],p[2]);}
      }
    }
    const limit=(hi[axis]-lo[axis])*.2;
    function triangle(a,b,c){if(a<0||b<0||c<0)return;const x=positions[a*3+axis],y=positions[b*3+axis],z=positions[c*3+axis];if(Math.max(x,y,z)-Math.min(x,y,z)<limit)triangles.push(a,b,c);}
    for(let i=0;i<steps;i++)for(let j=0;j<steps;j++){const k=i*(steps+1)+j,a=grid[k],b=grid[k+1],c=grid[k+steps+1],d=grid[k+steps+2];triangle(a,b,c);triangle(b,d,c);}
  }else{
    const lhs=compileExpression(model.lhs),rhs=compileExpression(model.rhs),p=[0,0,0],scans=n*2;
    outer:for(let i=0;i<=n;i++)for(let j=0;j<=n;j++)for(let axis=0;axis<3;axis++){
      if(performance.now()>deadline||positions.length/3>=MAX_POINTS){capped=true;break outer;}
      const a=(axis+1)%3,b=(axis+2)%3;p[a]=lo[a]+(hi[a]-lo[a])*i/n;p[b]=lo[b]+(hi[b]-lo[b])*j/n;
      const at=x=>{p[axis]=x;return lhs(p)-rhs(p);};
      if(!scanRoots(at,lo[axis],hi[axis],scans,x=>{p[axis]=x;if(positions.length/3<MAX_POINTS)positions.push(p[0],p[1],p[2]);},deadline)){capped=true;break outer;}
    }
  }
  const vertices=new Float32Array(positions),indices=new Uint32Array(triangles),normals=new Float32Array(vertices.length);
  for(let i=0;i<indices.length;i+=3){const a=indices[i]*3,b=indices[i+1]*3,c=indices[i+2]*3;
    const ux=vertices[b]-vertices[a],uy=vertices[b+1]-vertices[a+1],uz=vertices[b+2]-vertices[a+2],vx=vertices[c]-vertices[a],vy=vertices[c+1]-vertices[a+1],vz=vertices[c+2]-vertices[a+2];
    const nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx;
    for(const v of [a,b,c]){normals[v]+=nx;normals[v+1]+=ny;normals[v+2]+=nz;}
  }
  for(let i=0;i<normals.length;i+=3){const length=Math.hypot(normals[i],normals[i+1],normals[i+2]);if(length){normals[i]/=length;normals[i+1]/=length;normals[i+2]/=length;}}
  return {positions:vertices,triangles:indices,normals,capped,adaptive,detail:n,milliseconds:performance.now()-started};
}
if(typeof module!=='undefined'){Object.assign(globalThis,require('./math.js'));module.exports={sampleSurface};}
