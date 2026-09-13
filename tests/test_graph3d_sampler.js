const assert=require('node:assert/strict');
const {sampleSurface}=require('../viewer/sampler.js');
const x=['var',0],y=['var',1],z=['var',2],sq=a=>['^',a,2];
function sample(lhs,rhs,lo=[-5,-5,-5],hi=[5,5,5],detail=16){return sampleSurface({model:{lhs,rhs},lo,hi,detail});}
function points(result){return Array.from({length:result.positions.length/3},(_,i)=>Array.from(result.positions.slice(i*3,i*3+3)));}
let r=sample(z,['+',['sin',x],['cos',y]]);
assert(r.triangles.length>0);assert(points(r).every(([x,y,z])=>Math.abs(z-Math.sin(x)-Math.cos(y))<1e-6));
r=sample(['+',['+',sq(x),sq(y)],sq(z)],16);
assert(points(r).length>300);assert(points(r).every(([x,y,z])=>Math.abs(x*x+y*y+z*z-16)<1e-5));
assert.equal(new Set(points(r).filter(p=>p.every(v=>v!==0)).map(p=>p.map(v=>v>0).join(','))).size,8);
r=sample(x,.137);assert(points(r).every(p=>Math.abs(p[0]-.137)<1e-7));assert(r.triangles.length);
r=sample(['^',['-',z,.137],4],0,[-1,-1,-1],[1,1,1],12);assert(points(r).length>0);assert(points(r).every(p=>Math.abs(p[2]-.137)<1e-6));
r=sample(['/',1,['-',z,.137]],0);assert.equal(r.positions.length,0,'poles must not be plotted');
r=sample(z,['log',x]);assert(points(r).every(([x,y,z])=>x>0&&Math.abs(z-Math.log(x))<1e-6));
r=sample(['+',['+',sq(x),sq(y)],sq(z)],-1);assert.equal(r.positions.length,0);
r=sample(z,['/',x,10],[-500,-500,-500],[500,500,500]);assert(points(r).some(p=>p[0]===500));assert(points(r).some(p=>p[1]===-500));assert(points(r).every(([x,y,z])=>Math.abs(z-x/10)<1e-6));
r=sample(['+',sq(x),sq(y)],9);assert(points(r).some(p=>p[2]===-5));assert(points(r).some(p=>p[2]===5));
console.log('Browser sampling: explicit meshes, sphere branches, tangent roots, domains, poles, cylinders and 1,000-unit ranges passed');
