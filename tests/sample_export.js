// Sample a compact exported equation using the exact browser worker implementation.
const fs=require('node:fs'),vm=require('node:vm');const {sampleSurface}=require('../viewer/sampler.js');
const html=fs.readFileSync(process.argv[2],'utf8');
const source=['names','graphKind','model'].map(name=>html.match(new RegExp('const '+name+'=.*;'))[0]).join('\n');
const data=vm.runInNewContext(source+'\n({model,lo,hi,detail:requestedDetail,maxMs:4000})');
const result=sampleSurface(data),points=Array.from({length:result.positions.length/3},(_,i)=>Array.from(result.positions.slice(i*3,i*3+3)));
process.stdout.write(JSON.stringify(points));
