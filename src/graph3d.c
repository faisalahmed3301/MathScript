#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "graph.h"
#include "eval.h"
#include "symtab.h"
#include "rootfind.h"
#include "errors.h"
#include "ir.h"
#include "util.h"

typedef struct { const char *code; char axis; int direction; } Rotation;
static const Rotation rotations[] = {
    {"vccw",'y',1}, {"hccw",'x',1}, {"vcw",'y',-1}, {"hcw",'x',-1}
};
static int selected_rotation=-1;
int graph3d_needs_rotation(void) { return selected_rotation<0; }
void graph3d_begin(void) {
    selected_rotation=-1;
    printf("Choose the 3D rotation before entering an equation:\n"
           "  vccw : vertical (y axis), counterclockwise\n"
           "  hccw : horizontal (x axis), counterclockwise\n"
           "  vcw  : vertical (y axis), clockwise\n"
           "  hcw  : horizontal (x axis), clockwise\n"
           "Direction is viewed from the positive end of the selected axis.\n");
}
int graph3d_rotation_command(const char *line) {
    for (int i=0;i<4;i++) if (!strcmp(line,rotations[i].code)) {
        selected_rotation=i;
        printf("Rotation set: %s - %c axis, %s. Enter the equation.\n",
               rotations[i].code,rotations[i].axis,rotations[i].direction>0?"counterclockwise":"clockwise");
        return 1;
    }
    return 0;
}

typedef struct { double v[3]; } Point3;
typedef struct {
    Point3 *points;
    size_t count, capacity;
    Point3 current;
    int axis, failed;
} Cloud;
static void add_point(double root,void *context) {
    Cloud *c=context;
    if (c->failed || !isfinite(root)) return;
    for (int i=0;i<3;i++) if (i!=c->axis && !isfinite(c->current.v[i])) return;
    if (c->count==c->capacity) {
        size_t capacity=c->capacity?c->capacity*2:8192;
        Point3 *p=realloc(c->points,capacity*sizeof(*p));
        if (!p) { c->failed=1; return; }
        c->points=p; c->capacity=capacity;
    }
    c->current.v[c->axis]=root;
    c->points[c->count++]=c->current;
}
static int compare_points(const void *a,const void *b) {
    const Point3 *p=a,*q=b;
    for (int i=0;i<3;i++) {
        if (p->v[i]<q->v[i]) return -1;
        if (p->v[i]>q->v[i]) return 1;
    }
    return 0;
}
static void project(Point3 p,double *u,double *v,double *depth) {
    double scale=0;
    for (int i=0;i<3;i++) scale=fmax(scale,graph3d_settings.hi[i]-graph3d_settings.lo[i]);
    for (int i=0;i<3;i++) p.v[i]=(p.v[i]-(graph3d_settings.lo[i]/2+graph3d_settings.hi[i]/2))/scale;
    *u=(p.v[0]+p.v[2])*0.7071067811865475;
    *v=p.v[1]*0.816496580927726+(p.v[0]-p.v[2])*0.408248290463863;
    *depth=(-p.v[0]+p.v[1]+p.v[2])*0.577350269189626;
}
#define WIDTH3 120
#define HEIGHT3 45
static void preview(const Cloud *c,char names[][64]) {
    char canvas[HEIGHT3][WIDTH3+1]; double depths[HEIGHT3][WIDTH3];
    for (int r=0;r<HEIGHT3;r++) {
        memset(canvas[r],' ',WIDTH3); canvas[r][WIDTH3]=0;
        for (int col=0;col<WIDTH3;col++) depths[r][col]=-INFINITY;
    }
    double near=-INFINITY,far=INFINITY;
    for (size_t i=0;i<c->count;i++) {
        double u,v,d; project(c->points[i],&u,&v,&d);
        near=fmax(near,d); far=fmin(far,d);
    }
    for (size_t i=0;i<c->count;i++) {
        double u,v,d; project(c->points[i],&u,&v,&d);
        int col=(int)round((WIDTH3-1)/2.0+u*50),r=(int)round((HEIGHT3-1)/2.0-v*25);
        if (r<0 || r>=HEIGHT3 || col<0 || col>=WIDTH3 || d<depths[r][col]) continue;
        depths[r][col]=d;
        int shade=near>far?(int)((d-far)/(near-far)*4):2; if (shade<0) shade=0; if (shade>4) shade=4;
        canvas[r][col]=":oO@#"[shade];
    }
    /* Dotted reference axes pass through zero whenever it is visible. */
    for (int axis=0;axis<3;axis++) {
        Point3 p;
        for (int j=0;j<3;j++) p.v[j]=fmax(graph3d_settings.lo[j],fmin(0,graph3d_settings.hi[j]));
        /* Oversample the projected line so every crossed terminal cell
         * receives a dot, independently of mathematical unit spacing. */
        const int samples=WIDTH3*8;
        for (int k=0;k<=samples;k++) {
            p.v[axis]=graph3d_settings.lo[axis]+(graph3d_settings.hi[axis]-graph3d_settings.lo[axis])*(k/(double)samples);
            double u,v,d; project(p,&u,&v,&d);
            int col=(int)round((WIDTH3-1)/2.0+u*50),r=(int)round((HEIGHT3-1)/2.0-v*25);
            if (r>=0 && r<HEIGHT3 && col>=0 && col<WIDTH3) canvas[r][col]='.';
        }
        double marks[WIDTH3*8];
        int n=graph_axis_marks(graph3d_settings.lo[axis],graph3d_settings.hi[axis],WIDTH3*8,marks);
        for (int i=0;i<n;i++) {
            p.v[axis]=marks[i]; double u,v,d; project(p,&u,&v,&d);
            int col=(int)round((WIDTH3-1)/2.0+u*50),row=(int)round((HEIGHT3-1)/2.0-v*25);
            if (row>=0 && row<HEIGHT3 && col>=0 && col<WIDTH3) canvas[row][col]='o';
        }
        p.v[axis]=graph3d_settings.hi[axis];
        double u,v,d; project(p,&u,&v,&d);
        int col=2+(int)round((WIDTH3-1)/2.0+u*50),r=(int)round((HEIGHT3-1)/2.0-v*25);
        if (r>=0 && r<HEIGHT3 && col>=0 && col<WIDTH3) canvas[r][col]="XYZ"[axis];
    }
    printf("\n3D isometric preview | %d x %d canvas\n",WIDTH3,HEIGHT3);
    for (int r=0;r<HEIGHT3;r++) printf("  %s\n",canvas[r]);
    printf("  Axes X=%s, Y=%s, Z=%s; axes: dotted lines, o at 1/3-unit intervals; : o O @ # shade by depth.\n",names[0],names[1],names[2]);
}

/* An offline point-cloud viewer: every sampled branch remains available
 * when rotating, including the back of closed surfaces. No CDN required. */
static void export_html(const Cloud *c,char names[][64]) {
    const Rotation *rotation=&rotations[selected_rotation];
    static unsigned serial=1;
    char path[96]; FILE *fp=NULL;
    for (;serial<1000000;serial++) {
        snprintf(path,sizeof(path),"output/graph3d_%03u.html",serial);
        FILE *existing=fopen(path,"r");
        if (existing) { fclose(existing); continue; }
        fp=fopen(path,"wx"); serial++; break;
    }
    if (!fp) { printf("Could not save interactive view in output/ (ensure this directory exists and is writable).\n"); return; }
    fputs("<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>MathScript 3D graph</title>\n"
          "<style>body{margin:0;background:#0b1221;color:#e2e8f0;font:16px system-ui}header{padding:20px 28px}h1{font-size:24px;margin:0 0 8px}p{margin:8px 0;color:#abbcd0}button{padding:8px 14px;border:0;border-radius:6px;cursor:pointer}canvas{display:block;width:100%;height:76vh;touch-action:none}button:focus{outline:3px solid #38bdf8}</style>"
          "<header><h1>MathScript · 3D graph</h1><p>Drag to rotate · Scroll to zoom · Arrow keys to rotate · + / − to zoom</p>",fp);
    fprintf(fp,"<p>%s [%g, %g] · %s [%g, %g] · %s [%g, %g] · %zu sampled points</p>",
            names[0],graph3d_settings.lo[0],graph3d_settings.hi[0],names[1],graph3d_settings.lo[1],graph3d_settings.hi[1],names[2],graph3d_settings.lo[2],graph3d_settings.hi[2],c->count);
    fprintf(fp,"<p>Rotation: <strong>%s</strong> · %c axis · %s · 30°/second</p>",rotation->code,rotation->axis,rotation->direction>0?"counterclockwise":"clockwise");
    fputs("<button id=pause aria-pressed=\"false\">Pause rotation</button> <button id=reset>Reset view</button> <label>Resolution <select id=quality><option value=2>High (2x)</option><option value=4 selected>Ultra (4x)</option></select></label><p>Axis marks are 1/3 unit apart: three intervals equal one unit. Small dots keep the lines continuous. Ultra resolution sharpens the display; /samples refines the actual surface. A finite numerical sample. Increase /samples or change /range in MathScript to refine the graph.</p></header><canvas id=view tabindex=0 aria-label=\"Interactive three dimensional graph; use arrow keys to rotate\"></canvas>\n<script>\nconst points=[\n",fp);
    for (size_t i=0;i<c->count;i++) fprintf(fp,"%s[%.17g,%.17g,%.17g]",i?",\n":"",c->points[i].v[0],c->points[i].v[1],c->points[i].v[2]);
    fprintf(fp,"\n];\nconst names=['%s','%s','%s'],lo=[%.17g,%.17g,%.17g],hi=[%.17g,%.17g,%.17g];\n",names[0],names[1],names[2],graph3d_settings.lo[0],graph3d_settings.lo[1],graph3d_settings.lo[2],graph3d_settings.hi[0],graph3d_settings.hi[1],graph3d_settings.hi[2]);
    fprintf(fp,"const rotation={mode:'%s',axis:'%c',direction:%d};\n",rotation->code,rotation->axis,rotation->direction);
    fputs("const canvas=document.getElementById('view'),ctx=canvas.getContext('2d');let yaw=.75,pitch=.5,zoom=1,drag=null,queued=false,angle=0,playing=true,lastTime=null;\n"
          "const span=Math.max(...hi.map((v,i)=>v-lo[i])),center=lo.map((v,i)=>v/2+hi[i]/2);const normalized=points.map(p=>p.map((v,i)=>(v-center[i])/span)),projected=points.map(()=>[0,0,0]),palette=Array.from({length:256},(_,i)=>{const d=i/255*1.8-.9;return `hsl(${195-d*45} 85% ${55+d*22}%)`});\n"
          "function rotateModel(p,radians){const [x,y,z]=p,c=Math.cos(radians),s=Math.sin(radians);return rotation.axis==='x'?[x,y*c-z*s,y*s+z*c]:[x*c+z*s,y,-x*s+z*c]}\n"
          "function project(p){const [x,y,z]=rotateModel(p.map((v,i)=>(v-center[i])/span),angle),a=x*Math.cos(yaw)+z*Math.sin(yaw),b=-x*Math.sin(yaw)+z*Math.cos(yaw);return [a,y*Math.cos(pitch)-b*Math.sin(pitch),y*Math.sin(pitch)+b*Math.cos(pitch)]}\n"
          "function viewVector(p){const [x,y,z]=rotateModel(p,angle),a=x*Math.cos(yaw)+z*Math.sin(yaw),b=-x*Math.sin(yaw)+z*Math.cos(yaw);return [a,y*Math.cos(pitch)-b*Math.sin(pitch),y*Math.sin(pitch)+b*Math.cos(pitch)]}\n"
          "function renderScale(w,h){return Math.min(Number(document.getElementById('quality').value),Math.sqrt(16000000/Math.max(1,w*h)))}\n"
          "function draw(){const w=canvas.clientWidth,h=canvas.clientHeight,dpr=renderScale(w,h),width=Math.max(1,Math.round(w*dpr)),height=Math.max(1,Math.round(h*dpr));if(canvas.width!==width||canvas.height!==height){canvas.width=width;canvas.height=height}ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);const s=Math.min(w,h)*.65*zoom;const xy=p=>[w/2+p[0]*s,h/2-p[1]*s];\n"
          "const m=[[1,0,0],[0,1,0],[0,0,1]].map(viewVector);for(let i=0;i<normalized.length;i++){const p=normalized[i],out=projected[i];for(let a=0;a<3;a++)out[a]=m[0][a]*p[0]+m[1][a]*p[1]+m[2][a]*p[2]}projected.sort((a,b)=>a[2]-b[2]);const size=Math.max(.6,1.1*Math.sqrt(zoom));for(const p of projected){const x=w/2+p[0]*s,y=h/2-p[1]*s;if(x<0||x>w||y<0||y>h)continue;ctx.fillStyle=palette[Math.max(0,Math.min(255,Math.round((p[2]+.9)/1.8*255)))];ctx.fillRect(x-size/2,y-size/2,size,size)}drawAxes(xy,w,h);if(!points.length){ctx.fillStyle='#e2e8f0';ctx.fillText('No real surface points found in this view.',24,36)}}\n"
          "function axisMarks(a,capacity,divisions=3){const values=[];for(let i=0;i<capacity;i++){const lower=lo[a]+(hi[a]-lo[a])*Math.max(0,(i-.5)/(capacity-1)),upper=lo[a]+(hi[a]-lo[a])*Math.min(1,(i+.5)/(capacity-1)),v=Math.ceil(lower*divisions)/divisions;if(Number.isFinite(v)&&v>=lo[a]&&v<=upper&&v<=hi[a]&&(!values.length||v>values[values.length-1]))values.push(v===0?0:v)}return values}\n"
          "function axisUnits(a,capacity){return axisMarks(a,capacity,1)}\n"
          "function drawAxes(xy,w,h){ctx.font='14px system-ui';const dpr=renderScale(w,h);for(let a=0;a<3;a++){const p=lo.map((v,i)=>Math.max(v,Math.min(0,hi[i])));ctx.fillStyle=['#fb7185','#4ade80','#60a5fa'][a];p[a]=lo[a];const u=xy(project(p));p[a]=hi[a];const v=xy(project(p));const steps=Math.max(1,Math.ceil(Math.hypot(v[0]-u[0],v[1]-u[1])*dpr));ctx.beginPath();for(let k=0;k<=steps;k++){const t=k/steps,x=u[0]+(v[0]-u[0])*t,y=u[1]+(v[1]-u[1])*t;if(x<0||x>w||y<0||y>h)continue;ctx.moveTo(x+.75,y);ctx.arc(x,y,.75,0,2*Math.PI)}ctx.fill();ctx.beginPath();for(const value of axisMarks(a,Math.max(2,Math.ceil(Math.hypot(w,h)*zoom*2)))){p[a]=value;const [x,y]=xy(project(p));if(x<0||x>w||y<0||y>h)continue;const radius=Math.abs(value-Math.round(value))<1e-9?2.5:1.5;ctx.moveTo(x+radius,y);ctx.arc(x,y,radius,0,2*Math.PI)}ctx.fill();ctx.fillText(names[a]+' '+hi[a],v[0]+7,v[1]);ctx.fillText(String(lo[a]),u[0]+7,u[1]+16*a)}}\n"
          "function animate(time){queued=false;if(lastTime!==null&&playing&&!drag&&!document.hidden){const seconds=Math.max(0,Math.min(.1,(time-lastTime)/1000));angle=(angle+rotation.direction*seconds*Math.PI/6)%(2*Math.PI)}lastTime=time;draw();if(playing&&!document.hidden)schedule()}\n"
          "function schedule(){if(!queued){queued=true;requestAnimationFrame(animate)}}\n"
          "canvas.onpointerdown=e=>{drag=[e.clientX,e.clientY];canvas.setPointerCapture(e.pointerId);canvas.focus()};canvas.onpointermove=e=>{if(!drag)return;yaw+=(e.clientX-drag[0])*.008;pitch+=(e.clientY-drag[1])*.008;drag=[e.clientX,e.clientY];schedule()};canvas.onpointerup=canvas.onpointercancel=()=>drag=null;canvas.addEventListener('wheel',e=>{e.preventDefault();zoom=Math.max(.15,Math.min(8,zoom*Math.exp(-e.deltaY*.001)));schedule()},{passive:false});\n"
          "canvas.onkeydown=e=>{if(e.key==='ArrowLeft')yaw-=.1;else if(e.key==='ArrowRight')yaw+=.1;else if(e.key==='ArrowUp')pitch-=.1;else if(e.key==='ArrowDown')pitch+=.1;else if(e.key==='+')zoom=Math.min(8,zoom*1.1);else if(e.key==='-')zoom=Math.max(.15,zoom/1.1);else return;e.preventDefault();schedule()};document.getElementById('reset').onclick=()=>{yaw=.75;pitch=.5;zoom=1;angle=0;lastTime=null;schedule()};const pause=document.getElementById('pause');pause.onclick=()=>{playing=!playing;lastTime=null;pause.textContent=playing?'Pause rotation':'Resume rotation';pause.setAttribute('aria-pressed',String(!playing));schedule()};document.addEventListener('visibilitychange',()=>{lastTime=null;if(!document.hidden)schedule()});document.getElementById('quality').onchange=schedule;window.addEventListener('resize',schedule);schedule();\n</script></html>\n",fp);
    int failed=ferror(fp); if (fclose(fp)!=0) failed=1;
    if (failed) { remove(path); printf("Could not finish saving interactive view.\n"); }
    else printf("Interactive 3D graph: %s (open in a browser; drag to rotate, scroll to zoom).\n",path);
}

void graph3d_run(ASTNode *stmt,int show_tac) {
    if (graph3d_needs_rotation()) { printf("Choose a rotation first: vccw, hccw, vcw, or hcw.\n"); return; }
    const Rotation *rotation=&rotations[selected_rotation];
    ASTNode *lhs,*rhs,*owned=NULL;
    char found[8][64],names[3][64];
    if (stmt->kind==N_BINOP && stmt->op=='=') { lhs=stmt->left; rhs=stmt->right; }
    else {
        int n=graph_free_vars(stmt,NULL,found),has_z=0;
        for (int i=0;i<n;i++) if (!strcmp(found[i],"z")) has_z=1;
        if (n>2 || has_z) { semantic_error("/graph3d expects an equation, e.g. x^2+y^2+z^2=25, or a bare expression in x and y"); return; }
        owned=ast_var("z"); lhs=owned; rhs=stmt;
    }
    int n=graph_free_vars(lhs,rhs,found);
    if (n<1 || n>3) { semantic_error("/graph3d supports equations in one to three variables, found %d",n); ast_free(owned); return; }
    int standard=1;
    for (int i=0;i<n;i++) if (strcmp(found[i],"x") && strcmp(found[i],"y") && strcmp(found[i],"z")) standard=0;
    if (standard) { strcpy(names[0],"x"); strcpy(names[1],"y"); strcpy(names[2],"z"); }
    else {
        for (int i=0;i<n;i++) memcpy(names[i],found[i],64);
        for (int i=0;i<n;i++) for (int j=i+1;j<n;j++) if (strcmp(names[i],names[j])>0) {
            char temp[64]; memcpy(temp,names[i],64); memcpy(names[i],names[j],64); memcpy(names[j],temp,64);
        }
        for (int i=n;i<3;i++) {
            for (int k=0;k<3;k++) {
                char candidate[2]={"xyz"[k],0}; int used=0;
                for (int j=0;j<i;j++) if (!strcmp(names[j],candidate)) used=1;
                if (!used) { strcpy(names[i],candidate); break; }
            }
        }
    }
    double saved[3]={0}; int had[3];
    for (int i=0;i<3;i++) { had[i]=symtab_lookup(names[i],saved+i); symtab_set(names[i],0); }
    Cloud cloud={0};
    if (!eval_validate(lhs) || !eval_validate(rhs)) goto cleanup;
    if (show_tac) {
        TACProgram tac; tac_init(&tac); ASTNode *diff=ast_binop('-',lhs,rhs);
        char operand[32]; snprintf(operand,sizeof(operand),"%s",tac_build(&tac,diff)); free(diff);
        printf("IR (TAC):\n"); tac_print(&tac);
        printf("SURFACE %s = 0\nPLOT3D %s, %s, %s\n",operand,names[0],names[1],names[2]);
    }
    int samples=graph3d_settings.samples;
    /* Each axis becomes the solved variable in turn. This finds both
     * halves of spheres, vertical planes/cylinders and multiple sheets. */
    for (int axis=0;axis<3 && !cloud.failed;axis++) {
        cloud.axis=axis; int a=(axis+1)%3,b=(axis+2)%3;
        for (int i=0;i<=samples && !cloud.failed;i++) for (int j=0;j<=samples && !cloud.failed;j++) {
            cloud.current.v[a]=graph3d_settings.lo[a]+(graph3d_settings.hi[a]-graph3d_settings.lo[a])*(i/(double)samples);
            cloud.current.v[b]=graph3d_settings.lo[b]+(graph3d_settings.hi[b]-graph3d_settings.lo[b])*(j/(double)samples);
            symtab_set(names[a],cloud.current.v[a]); symtab_set(names[b],cloud.current.v[b]);
            rootfind_visit(lhs,rhs,names[axis],graph3d_settings.lo[axis],graph3d_settings.hi[axis],samples*4,add_point,&cloud);
        }
    }
    if (cloud.failed) { printf("Unable to allocate the 3D point cloud; reduce /samples.\n"); goto cleanup; }
    if (cloud.count) {
        qsort(cloud.points,cloud.count,sizeof(Point3),compare_points);
        size_t unique=1;
        for (size_t i=1;i<cloud.count;i++) if (compare_points(cloud.points+i,cloud.points+unique-1)) cloud.points[unique++]=cloud.points[i];
        cloud.count=unique;
    }
    printf("3D graph generated (implicit surface in %s, %s, %s).\n",names[0],names[1],names[2]);
    printf("Range: %s [%g, %g], %s [%g, %g], %s [%g, %g].\n",names[0],graph3d_settings.lo[0],graph3d_settings.hi[0],names[1],graph3d_settings.lo[1],graph3d_settings.hi[1],names[2],graph3d_settings.lo[2],graph3d_settings.hi[2]);
    printf("%zu sampled surface points; %d subdivisions per axis, scans along all three axes.\n",cloud.count,samples);
    if (!cloud.count) printf("No real surface points found in this view.\n");
    printf("Rotation: %s (%c axis, %s). The HTML view rotates automatically; the terminal shows a static preview.\n",
           rotation->code,rotation->axis,rotation->direction>0?"counterclockwise":"clockwise");
    preview(&cloud,names); export_html(&cloud,names);
    printf("Finite numerical sampling; use /range and /samples to inspect other regions or finer detail.\n");
cleanup:
    free(cloud.points); ast_free(owned);
    for (int i=0;i<3;i++) if (had[i]) symtab_set(names[i],saved[i]); else symtab_unset(names[i]);
}
