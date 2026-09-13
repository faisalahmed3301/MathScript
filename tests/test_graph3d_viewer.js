// Offline viewer behavior with a recording canvas. Real browser layout is checked separately.
const assert=require('node:assert/strict');
const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),vm=require('node:vm');
const {spawnSync}=require('node:child_process');
const {sampleSurface}=require('../viewer/sampler.js');
const directory=fs.mkdtempSync(path.join(os.tmpdir(),'mathscript-viewer-'));
const close=(a,b)=>assert(Math.abs(a-b)<1e-9,`${a} != ${b}`);
async function main(){
  fs.mkdirSync(path.join(directory,'output'));
  for(const [mode,axis,direction] of [['vccw','y',1],['hccw','x',1],['vcw','y',-1],['hcw','x',-1]]){
    const run=spawnSync(path.resolve(__dirname,'../build/mathscript'),[],{cwd:directory,input:`/tac off\n/graph3d\n${mode}\n/samples 24\nx^2+y^2+z^2=625\n/exit\n`,encoding:'utf8'});
    assert.equal(run.status,0);assert.equal(run.stderr,'');
    const html=fs.readFileSync(path.join(directory,run.stdout.match(/Interactive 3D graph: (\S+)/)[1]),'utf8');
    const source=html.match(/<script>([\s\S]*?)<\/script>/)[1];
    const nodes={},events={},workers=[];let frame=null,clock=0,draws=0,axisDots=0;
    const ctx={setTransform(){},clearRect(){draws=0;axisDots=0;},beginPath(){},fill(){},stroke(){},moveTo(){},lineTo(){},setLineDash(){},arc(...v){assert(v.every(Number.isFinite));axisDots++;},fillText(){},fillRect(...v){assert(v.every(Number.isFinite));draws++;}};
    function element(id=''){
      const node={id,value:'',textContent:'',checked:false,hidden:false,style:{},attributes:{},clientWidth:1000,clientHeight:650,
        setAttribute(k,v){this.attributes[k]=v;},addEventListener(k,f){events[id+':'+k]=f;},appendChild(){},focus(){},setPointerCapture(){},observe(){},
        getContext:type=>type==='2d'?ctx:null,querySelector(){return this.child||(this.child={textContent:''});}};
      Object.defineProperty(node,'innerHTML',{set:parse});
      Object.defineProperty(node,'selectedOptions',{get:()=>[{textContent:node.value==='4'?'Ultra (4×)':'High (2×)'}]});return node;
    }
    function parse(markup){for(const m of markup.matchAll(/<[^>]+\bid="([^"]+)"[^>]*>/g)){const n=nodes[m[1]]||(nodes[m[1]]=element(m[1]));n.value=(m[0].match(/\bvalue="([^"]*)"/)||[])[1]||n.value;n.checked=/\bchecked\b/.test(m[0]);}}
    parse(html);nodes.quality.value='auto';nodes.detail.value='64';
    const document={hidden:false,fullscreenElement:null,getElementById:id=>nodes[id],createElement:()=>element(),addEventListener:(k,f)=>events[k]=f,
      async exitFullscreen(){this.fullscreenElement=null;events.fullscreenchange();}};
    nodes.workspace.requestFullscreen=async()=>{document.fullscreenElement=nodes.workspace;events.fullscreenchange();};
    class Worker{
      constructor(){workers.push(this);}
      postMessage(data){this.data=data;}
      terminate(){this.terminated=true;}
      finish(){this.onmessage({data:sampleSurface(this.data)});}
    }
    const context=vm.createContext({document,window:{addEventListener:(k,f)=>events[k]=f},matchMedia:()=>({matches:false}),
      requestAnimationFrame:f=>{assert(!frame,'duplicate animation loop');frame=f;},ResizeObserver:class{observe(){}},Worker,Blob:class{},
      URL:{createObjectURL:()=> 'blob:test',revokeObjectURL(){}},performance:{now:()=>clock}});
    const read=code=>vm.runInContext(code,context),flush=(time=clock+16)=>{assert(frame,'missing animation frame');clock=time;const f=frame;frame=null;f(time);};
    read(source);assert.equal(workers.length,1);workers[0].finish();flush(0);workers.length=0;
    assert.equal(read('rotation.axis'),axis);assert.equal(read('rotation.direction'),direction);
    assert.equal(nodes['range-summary'].textContent,'100 × 100 × 100');
    assert.deepEqual(Array.from(read('bounds.lo')),[-50,-50,-50]);assert.deepEqual(Array.from(read('bounds.hi')),[50,50,50]);
    assert(draws>100);assert(axisDots>0&&axisDots<100);assert.equal(nodes['range-error'].textContent,'');
    flush(100);close(read('angle'),direction*Math.PI/60);
    nodes.pause.onclick();flush();assert.equal(nodes.pause.attributes['aria-pressed'],'true');assert.equal(frame,null);
    nodes['rotation-axis'].value='z';nodes['rotation-direction'].value='-1';nodes['rotation-axis'].onchange();flush();
    assert.equal(read('rotation.axis'),'z');assert.equal(read('rotation.direction'),-1);
    const basis=Array.from(read('rotateModel([1,0,0],Math.PI/2)'));basis.forEach((v,i)=>close(v,[0,1,0][i]));
    nodes['zoom-in'].onclick();flush();assert(read('zoom')>1);
    nodes.reset.onclick();flush();close(read('zoom'),1);close(read('angle'),0);
    nodes.view.onpointerdown({clientX:100,clientY:100,pointerId:1});nodes.view.onpointermove({clientX:160,clientY:130});flush();assert.notEqual(read('yaw'),.75);nodes.view.onpointerup();
    nodes.axes.checked=false;nodes.axes.onchange();flush();assert.equal(axisDots,0);
    nodes.axes.checked=true;nodes.axes.onchange();flush();assert(axisDots>0&&axisDots<100);
    nodes.opacity.value='35';nodes.opacity.oninput();flush();assert.equal(nodes['opacity-value'].value,'35%');
    await nodes.fullscreen.onclick();flush();assert.equal(document.fullscreenElement,nodes.workspace);assert.equal(nodes.fullscreen.attributes['aria-pressed'],'true');
    await nodes.fullscreen.onclick();flush();assert.equal(document.fullscreenElement,null);
    const submit=()=>nodes['range-form'].onsubmit({preventDefault(){}});
    nodes['max-0'].value='2000';submit();assert(nodes['range-error'].textContent.includes('1,000'));assert.equal(workers.length,0);
    nodes['max-0'].value='-50';submit();assert(nodes['range-error'].textContent.includes('increasing'));
    nodes['min-0'].value='';submit();assert(nodes['range-error'].textContent);
    for(let a=0;a<3;a++){nodes['min-'+a].value='-500';nodes['max-'+a].value='500';}nodes.detail.value='8';submit();
    assert.equal(workers.length,1);assert.equal(nodes['graph-status'].textContent,'Calculating…');workers[0].finish();flush();
    assert.deepEqual(Array.from(read('bounds.lo')),[-500,-500,-500]);assert.deepEqual(Array.from(read('bounds.hi')),[500,500,500]);assert(read('positions.length')>0);
    assert.equal(nodes['graph-status'].textContent,'Ready');
    // A new range request cancels old work; an old result cannot overwrite it.
    submit();const old=workers.at(-1);nodes['range-reset'].onclick();const current=workers.at(-1);assert(old.terminated);old.finish();assert.equal(nodes['graph-status'].textContent,'Calculating…');current.finish();flush();
    assert.deepEqual(Array.from(read('bounds.hi')),[50,50,50]);
    submit();workers.at(-1).onerror();assert.equal(nodes['graph-status'].textContent,'Previous graph retained');assert.deepEqual(Array.from(read('bounds.hi')),[50,50,50]);
    nodes.pause.onclick();flush();const before=read('angle');flush(clock+250);close(read('angle'),before-Math.PI/24);
    document.hidden=true;events.visibilitychange();flush();assert.equal(frame,null);const hidden=read('angle');document.hidden=false;events.visibilitychange();flush(clock+60000);close(read('angle'),hidden);
    console.log(`${mode}: rotation, ranges, validation, cancellation, fullscreen, appearance and camera passed`);
  }
}
main().catch(e=>{console.error(e);process.exitCode=1;}).finally(()=>fs.rmSync(directory,{recursive:true,force:true}));
