// Execute each generated viewer with a recording Canvas and controlled RAF clock.
// This verifies animation math and controls, not browser visual appearance.
const assert=require('node:assert/strict');
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),vm=require('node:vm');
const {spawnSync}=require('node:child_process');
const directory=fs.mkdtempSync(path.join(os.tmpdir(),'mathscript-viewer-'));
const close=(a,b)=>assert(Math.abs(a-b)<1e-10,`${a} != ${b}`);
try {
  fs.mkdirSync(path.join(directory,'output'));
  for(const [mode,axis,direction] of [['vccw','y',1],['hccw','x',1],['vcw','y',-1],['hcw','x',-1]]) {
    const run=spawnSync(path.resolve(__dirname,'../build/mathscript'),[],{
      cwd:directory,input:`/tac off\n/graph3d\n${mode}\nx^2+y^2+z^2=25\n/exit\n`,encoding:'utf8'
    });
    assert.equal(run.status,0); assert.equal(run.stderr,'');
    assert(run.stdout.includes('Choose the 3D rotation before entering an equation:'));
    assert(run.stdout.includes(`Rotation: ${mode} (${axis} axis, ${direction>0?'counterclockwise':'clockwise'})`));
    const filename=run.stdout.match(/Interactive 3D graph: (\S+)/)[1];
    const html=fs.readFileSync(path.join(directory,filename),'utf8');
    const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
    let draws=[],frame=null,labels=[],axisDots=[],clock=0;
    const events={},attributes={};
    const canvas={clientWidth:1000,clientHeight:600,
      getContext:()=>({scale(){},clearRect(){draws=[];labels=[];axisDots=[];},beginPath(){},fill(){},moveTo(){},
        arc(...values){assert(values.every(Number.isFinite));axisDots.push(values);},
        fillText(text){labels.push(text);},fillRect(...values){assert(values.every(Number.isFinite));draws.push(values);}}),
      addEventListener:(name,fn)=>events[name]=fn,focus(){},setPointerCapture(){}
    };
    const reset={},pause={setAttribute:(key,value)=>attributes[key]=value};
    const document={hidden:false,getElementById:id=>({view:canvas,reset,pause})[id],addEventListener:(name,fn)=>events[name]=fn};
    const context=vm.createContext({document,window:{addEventListener:(name,fn)=>events[name]=fn},
      devicePixelRatio:2,requestAnimationFrame:fn=>{assert(!frame,'duplicate RAF loop');frame=fn;}});
    const read=code=>vm.runInContext(code,context);
    const flush=(time=clock+16)=>{assert(frame,'missing frame');clock=time;const fn=frame;frame=null;fn(time);};
    read(source); flush(0);
    assert.equal(read('rotation.mode'),mode); assert.equal(read('rotation.axis'),axis);
    assert.equal(read('rotation.direction'),direction);
    assert(draws.length>1000); assert.equal(canvas.width,2000);
    assert(labels.includes('x 10')&&labels.includes('y 10')&&labels.includes('z 10'));
    assert(axisDots.filter(p=>p[2]===.75).length>1500,'axes must use thousands of small dots');
    assert.equal(axisDots.filter(p=>p[2]===2.5).length,63,'retain larger whole-unit markers');
    for(let a=0;a<3;a++) assert.deepEqual(Array.from(read(`axisUnits(${a},2000)`)),Array.from({length:21},(_,i)=>i-10));
    const initial=JSON.stringify(draws),initialDots=JSON.stringify(axisDots);
    flush(100); close(read('angle'),direction*Math.PI/60);
    assert.notEqual(JSON.stringify(draws),initial,'graph did not animate automatically');
    assert.notEqual(JSON.stringify(axisDots),initialDots,'axis dots did not follow animation');
    // CCW is the right-handed positive angle viewed from the positive axis end.
    const basis=Array.from(read(`rotateModel(${axis==='x'?'[0,1,0]':'[1,0,0]'},rotation.direction*Math.PI/2)`));
    const expected=axis==='x'?[0,0,direction]:[0,0,-direction];
    basis.forEach((value,i)=>close(value,expected[i]));
    pause.onclick(); flush();
    assert.equal(attributes['aria-pressed'],'true'); assert.equal(frame,null);
    const frozen=read('angle');
    events.resize(); flush(1000); close(read('angle'),frozen);
    reset.onclick(); flush(); close(read('angle'),0); assert.equal(JSON.stringify(draws),initial);
    canvas.onpointerdown({clientX:100,clientY:100,pointerId:1});
    canvas.onpointermove({clientX:170,clientY:130}); flush();
    assert.notEqual(JSON.stringify(draws),initial,'drag failed'); canvas.onpointerup();
    reset.onclick(); flush(); assert.equal(JSON.stringify(draws),initial);
    let prevented=false;
    events.wheel({deltaY:-200,preventDefault(){prevented=true;}}); flush();
    assert(prevented); assert.notEqual(JSON.stringify(draws),initial,'zoom failed');
    reset.onclick(); flush();
    canvas.onkeydown({key:'ArrowRight',preventDefault(){}}); flush();
    assert.notEqual(JSON.stringify(draws),initial,'keyboard failed');
    canvas.clientWidth=600; events.resize(); flush(); assert.equal(canvas.width,1200);
    // Resume and hidden-tab transitions must not introduce large time jumps.
    pause.onclick(); flush(); assert.equal(attributes['aria-pressed'],'false');
    const resumed=read('angle'); flush(clock+100); close(read('angle'),resumed+direction*Math.PI/60);
    document.hidden=true; events.visibilitychange(); flush(); assert.equal(frame,null);
    const hidden=read('angle'); document.hidden=false; events.visibilitychange(); flush(clock+60000); close(read('angle'),hidden);
    read('lo[0]=-2.5;hi[0]=3.5');
    assert.deepEqual(Array.from(read('axisUnits(0,2000)')),[-2,-1,0,1,2,3]);
    console.log(`${mode}: axis, direction, automatic animation, dense dotted axes and viewer controls passed`);
  }
} finally {fs.rmSync(directory,{recursive:true,force:true});}
