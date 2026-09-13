/* Compile only validated tagged math nodes; raw equation text is never executed. */
function compileExpression(node) {
  const functions=new Set(['sqrt','abs','sin','cos','tan','log','pow']);
  const operators=new Set(['+','-','*','/','<','>','>=','<=','==','!=']);
  function source(n){
    if(typeof n==='number')return Number.isFinite(n)?'('+n+')':'NaN';
    if(!Array.isArray(n))throw new Error('Invalid expression node');
    const op=n[0];
    if(op==='var'){if(!Number.isInteger(n[1])||n[1]<0||n[1]>2)throw new Error('Invalid coordinate');return 'p['+n[1]+']';}
    if(op==='neg')return '(-'+source(n[1])+')';
    if(op==='^')return 'Math.pow('+source(n[1])+','+source(n[2])+')';
    if(operators.has(op))return '(+('+source(n[1])+op+source(n[2])+'))';
    if(functions.has(op))return 'Math.'+op+'('+n.slice(1).map(source).join(',')+')';
    throw new Error('Unsupported math operation');
  }
  return new Function('p','"use strict";return '+source(node)+';');
}
function usesCoordinate(n,a){return Array.isArray(n)&&(n[0]==='var'?n[1]===a:n.slice(1).some(c=>usesCoordinate(c,a)));}
function formatExpression(n,names){if(typeof n==='number')return String(Number(n.toPrecision(7)));if(n[0]==='var')return names[n[1]];if(n[0]==='neg')return '−'+formatExpression(n[1],names);if(['+','-','*','/','^','<','>','>=','<=','==','!='].includes(n[0]))return '('+formatExpression(n[1],names)+' '+n[0]+' '+formatExpression(n[2],names)+')';return n[0]+'('+n.slice(1).map(c=>formatExpression(c,names)).join(', ')+')';}
/* Residual-based root refinement also rejects poles and retains touching roots. */
function scanRoots(at,lo,hi,steps,emit,deadline=Infinity){
  let history=0,px=0,pf=0,ppx=0,ppf=0,last=-Infinity;
  const output=x=>{if(Math.abs(x-last)>1e-8*(1+Math.abs(x))){emit(x);last=x;}};
  for(let k=0;k<=steps;k++){
    if((k&63)===0&&performance.now()>deadline)return false;
    const x=lo+(hi-lo)*k/steps,value=at(x);
    if(!Number.isFinite(value)){history=0;continue;}
    if(value===0)output(x);
    if(history&&value!==0&&pf!==0&&(value<0)!==(pf<0)){
      let low=px,high=x,fl=pf;const scale=Math.min(Math.abs(pf),Math.abs(value));
      for(let m=0;m<52;m++){
        const mid=low+(high-low)/2,fm=at(mid);if(!Number.isFinite(fm))break;
        if(fm===0||Math.abs(fm)<scale*1e-10){output(mid);break;}
        if(mid===low||mid===high){if(Math.abs(fm)<scale*1e-6)output(mid);break;}
        if((fm<0)!==(fl<0))high=mid;else{low=mid;fl=fm;}
      }
    }
    if(history>=2&&pf!==0&&Math.abs(pf)<Math.abs(ppf)&&Math.abs(pf)<Math.abs(value)){
      let l=ppx,r=x;const ratio=.6180339887498949;let c=r-ratio*(r-l),d=l+ratio*(r-l),fc=Math.abs(at(c)),fd=Math.abs(at(d));
      for(let q=0;q<52&&Number.isFinite(fc+fd);q++){
        if(fc<fd){r=d;d=c;fd=fc;c=r-ratio*(r-l);fc=Math.abs(at(c));}
        else{l=c;c=d;fc=fd;d=l+ratio*(r-l);fd=Math.abs(at(d));}
      }
      if(Math.min(fc,fd)<Math.min(Math.abs(ppf),Math.abs(value))*1e-11)output(fc<fd?c:d);
    }
    ppx=px;ppf=pf;px=x;pf=value;if(history<2)history++;
  }
  return true;
}
if(typeof module!=='undefined')module.exports={compileExpression,usesCoordinate,formatExpression,scanRoots};
