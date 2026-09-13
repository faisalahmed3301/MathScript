const assert=require('node:assert/strict');const {sampleCurve}=require('../viewer/sampler2d.js');
const x=['var',0],y=['var',1],square=n=>['^',n,2];
function run(lhs,rhs,kind='implicit',lo=[-5,-5,0],hi=[5,5,0]){return sampleCurve({model:{lhs,rhs},kind,lo,hi,detail:200});}
function pairs(a){return Array.from({length:a.length/2},(_,i)=>[a[i*2],a[i*2+1]]);}
let r=run(y,['sin',x]);assert(r.segments.length);assert(pairs(r.positions).every(([x,y])=>Math.abs(y-Math.sin(x))<1e-6));
r=run(['+',square(x),square(y)],16);assert(r.positions.length>1000);assert(pairs(r.positions).every(([x,y])=>Math.abs(x*x+y*y-16)<1e-5));
r=run(x,square(y));assert(r.segments.length);assert(pairs(r.positions).some(p=>p[1]<-1));assert(pairs(r.positions).some(p=>p[1]>1));
r=run(['*',3,x],1,'numberline');assert.equal(r.positions.length,2);assert(Math.abs(r.positions[0]-1/3)<1e-7);
r=run(['sin',x],0,'numberline',[30,-1,0],[40,1,0]);assert(pairs(r.positions).some(p=>Math.abs(p[0]-10*Math.PI)<1e-5));assert(pairs(r.positions).some(p=>Math.abs(p[0]-12*Math.PI)<1e-5));
r=run(['/',1,['-',x,.137]],0,'numberline');assert.equal(r.positions.length,0);
r=run(['^',['-',y,.137],4],0);assert(r.positions.length);assert(pairs(r.positions).every(p=>Math.abs(p[1]-.137)<1e-6));
console.log('2D sampling: curves, circles, sideways branches, number lines, large-coordinate roots, poles and tangencies passed');
