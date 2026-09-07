#include <tmux.h>
#include <display.h>
#include <gpu.h>
#include <keyboard.h>
#include <serial.h>
#include <process.h>
#include <string.h>
#include <stdio.h>
#include <ui.h>
#include <log.h>

#define MAX_WINDOWS 4
#define MAX_PANES 8
#define MAX_NODES 16
#define BAR_H 40

typedef struct {
    int used;
    int is_leaf;
    int pane_id;
    int split;
    int ratio;
    int a, b;
    int parent;
    int x, y, w, h;
} tnode_t;

typedef struct {
    int id;
    int active;
    int window;
    int x, y, w, h;
    int is_canvas;
    int canvas_id;
    char input[256];
    int input_len;
    int is_shell;
} pane_t;

typedef struct {
    int id;
    int active;
    char name[16];
    int root;
    int pane_count;
    int focused;
} window_t;

static pane_t panes[MAX_PANES];
static window_t windows[MAX_WINDOWS];
static tnode_t nodes[MAX_NODES];
static int cur_window = 0;
static int next_canvas = 1;
static int default_canvas = 0;
static int prefix = 0;
static uint32_t prefix_time = 0;

static int alloc_node(void){
    for(int i=0;i<MAX_NODES;i++) if(!nodes[i].used){ nodes[i].used=1; nodes[i].is_leaf=1; nodes[i].pane_id=-1; nodes[i].a=nodes[i].b=-1; nodes[i].parent=-1; nodes[i].ratio=128; return i; }
    return -1;
}
static void free_node(int idx){ if(idx>=0&&idx<MAX_NODES) nodes[idx].used=0; }
static int alloc_pane(void){
    for(int i=0;i<MAX_PANES;i++) if(!panes[i].active){ return i; }
    return -1;
}
static int find_leaf_for_pane(int win, int pane_id){
    int root = windows[win].root;
    if(root<0) return -1;
    for(int i=0;i<MAX_NODES;i++) if(nodes[i].used && nodes[i].is_leaf && nodes[i].pane_id==pane_id){
        int p = i;
        while(nodes[p].parent!=-1) p=nodes[p].parent;
        if(p==root) return i;
    }
    return -1;
}
static void layout_node(int idx, int x,int y,int w,int h){
    if(idx<0||!nodes[idx].used) return;
    nodes[idx].x=x; nodes[idx].y=y; nodes[idx].w=w; nodes[idx].h=h;
    if(nodes[idx].is_leaf){
        int pid=nodes[idx].pane_id;
        if(pid>=0&&pid<MAX_PANES&&panes[pid].active){
            panes[pid].x=x; panes[pid].y=y; panes[pid].w=w; panes[pid].h=h;
            int prev=display_current();
            display_select(pid);
            if(panes[pid].is_canvas){
                display_set_region(x+2,y+2,x+w-2,y+h-2);
            }else{
                display_set_region(x+2,y+2,x+w-2,y+h-2);
            }
            display_select(prev);
        }
        return;
    }
    int a=nodes[idx].a, b=nodes[idx].b;
    if(a<0||b<0) return;
    if(nodes[idx].split==0){
        int lw = w * nodes[idx].ratio / 256;
        if(lw<80) lw=80;
        if(lw>w-80) lw=w-80;
        int rw=w-lw;
        layout_node(a, x, y, lw, h);
        layout_node(b, x+lw, y, rw, h);
    }else{
        int th = h * nodes[idx].ratio / 256;
        if(th<40) th=40;
        if(th>h-40) th=h-40;
        int bh=h-th;
        layout_node(a, x, y, w, th);
        layout_node(b, x, y+th, w, bh);
    }
}
static void layout_window(int win){
    if(win<0||win>=MAX_WINDOWS||!windows[win].active) return;
    int W=display_width(), H=display_height();
    int cx0=0, cy0=BAR_H, cx1=W, cy1=H-BAR_H;
    int root=windows[win].root;
    if(root>=0) layout_node(root, cx0, cy0, cx1-cx0, cy1-cy0);
}
static void render_pane_chrome(int is_focused, pane_t *p){
    uint32_t col = p->is_canvas ? 0x2BD97C : (is_focused ? 0x2BD97C : 0x2A2E4A);
    draw_rect(p->x, p->y, p->w, 2, col);
    draw_rect(p->x, p->y+p->h-2, p->w, 2, col);
    draw_rect(p->x, p->y, 2, p->h, col);
    draw_rect(p->x+p->w-2, p->y, 2, p->h, col);
    char lab[24];
    if(p->is_canvas) sprintf(lab,"canvas %d", p->canvas_id);
    else sprintf(lab,"shell %d", p->id);
    int prev=display_current();
    display_select(p->id);
    uint32_t sx=p->x+8, sy=p->y+6;
    uint32_t bg = is_focused ? col : 0x1A1F3A;
    draw_rect(sx-4, sy-2, (uint32_t)strlen(lab)*18+8, 20, bg);
    display_set_fg(is_focused ? 0x0B1020 : 0xE8ECF5);
    display_set_bg(bg);
    display_text_at(sx,sy,lab);
    display_set_fg(0xE8ECF5);
    display_set_bg(0x0B1020);
    display_select(prev);
}
static void tmux_render(void){
    uint32_t W=display_width(), H=display_height();
    draw_rect(0,0,W,BAR_H,0x16213E);
    draw_rect(0,BAR_H-2,W,2,0x2BD97C);
    int prev0=display_current();
    display_select(0);
    display_set_fg(0xE8ECF5);
    display_set_bg(0x16213E);
    display_text_at(12,4,"MyOS // tmux");
    char wb[64];
    sprintf(wb,"win %d/%d", cur_window, MAX_WINDOWS);
    display_text_at(W-120,4,wb);
    display_set_bg(0x0B1020);
    display_select(prev0);
    draw_rect(0,BAR_H,W,H-2*BAR_H,0x0B1020);
    int win = cur_window;
    int pane_count=0;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win) pane_count++;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win){
        int foc = (panes[i].id==windows[win].focused);
        draw_rect(panes[i].x, panes[i].y, panes[i].w, panes[i].h, 0x0B1020);
        if(!panes[i].is_canvas){
            display_render(i);
            render_pane_chrome(foc, &panes[i]);
        }else{
            int vx=panes[i].x+2, vy=panes[i].y+2, vw=panes[i].w-4, vh=panes[i].h-4;
            gpu_set_view(vx,vy,vw,vh);
            gpu_clear(0x0B1020);
            gpu_text(10,10,"canvas",0x8A93B2);
            char lab[16]; sprintf(lab,"%d", panes[i].canvas_id);
            gpu_text(80,10,lab,0x2BD97C);
            gpu_present();
            render_pane_chrome(foc, &panes[i]);
        }
    }
    draw_rect(0,H-BAR_H,W,BAR_H,0x11162B);
    draw_rect(0,H-BAR_H,W,2,0x2BD97C);
    int sx=12;
    for(int i=0;i<MAX_WINDOWS;i++) if(windows[i].active){
        int iscur=i==cur_window;
        char tmp[16];
        sprintf(tmp," %d:%s ", i, windows[i].name);
        uint32_t bg = iscur ? 0x2BD97C : 0x1A1F3A;
        uint32_t fg = iscur ? 0x0B1020 : 0x8A93B2;
        int tw = (int)strlen(tmp)*18;
        draw_rect(sx, H-BAR_H+8, tw, 24, bg);
        int prev=display_current();
        display_select(0);
        display_set_fg(fg);
        display_set_bg(bg);
        display_text_at(sx+2, H-BAR_H+10, tmp);
        display_set_fg(0xE8ECF5);
        display_set_bg(0x0B1020);
        display_select(prev);
        sx+=tw+6;
    }
    char extra[64];
    sprintf(extra,"panes %d", pane_count);
    int prev2=display_current();
    display_select(0);
    display_set_fg(0x8A93B2);
    display_set_bg(0x11162B);
    display_text_at(sx+10, H-BAR_H+10, extra);
    display_set_fg(0xE8ECF5);
    display_set_bg(0x0B1020);
    display_select(prev2);
    if(prefix){
        const char *pfx=" PREFIX ";
        int pw=(int)strlen(pfx)*18;
        draw_rect(W-pw-12, H-BAR_H+8, pw, 24, 0xFFD60A);
        int prev=display_current();
        display_select(0);
        display_set_fg(0x0B1020);
        display_set_bg(0xFFD60A);
        display_text_at(W-pw-10, H-BAR_H+10, (char*)pfx);
        display_set_fg(0xE8ECF5);
        display_set_bg(0x0B1020);
        display_select(prev);
    }
}
static int create_pane_for_window(int win){
    int pid=alloc_pane();
    if(pid<0) return -1;
    panes[pid].id=pid;
    panes[pid].active=1;
    panes[pid].window=win;
    panes[pid].is_canvas=0;
    panes[pid].canvas_id=0;
    panes[pid].input_len=0;
    panes[pid].input[0]=0;
    panes[pid].is_shell=1;
    display_create(pid, 0,0, 100,100);
    return pid;
}
static int create_window_with_pane(void){
    int win=-1;
    for(int i=0;i<MAX_WINDOWS;i++) if(!windows[i].active){ win=i; break; }
    if(win<0) return -1;
    windows[win].active=1;
    windows[win].id=win;
    sprintf(windows[win].name,"win%d",win);
    windows[win].pane_count=0;
    windows[win].focused=-1;
    int node=alloc_node();
    int pane=create_pane_for_window(win);
    if(pane<0||node<0) return -1;
    nodes[node].used=1; nodes[node].is_leaf=1; nodes[node].pane_id=pane; nodes[node].parent=-1;
    windows[win].root=node;
    windows[win].pane_count=1;
    windows[win].focused=pane;
    layout_window(win);
    return win;
}
int tmux_get_canvas_rect(int canvas_id, int *x,int *y,int *w,int *h){
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas && panes[i].canvas_id==canvas_id){
        if(x) *x=panes[i].x+2;
        if(y) *y=panes[i].y+2;
        if(w) *w=panes[i].w-4;
        if(h) *h=panes[i].h-4;
        return 0;
    }
    return -1;
}
int tmux_first_canvas_id(void){
    int best=0;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas){
        if(!best || panes[i].canvas_id<best) best=panes[i].canvas_id;
    }
    return best;
}
int tmux_canvas_count(void){
    int n=0;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas) n++;
    return n;
}
void tmux_set_default_canvas(int id){ default_canvas=id; }
int tmux_has_canvas(int id){ int x,y,w,h; return tmux_get_canvas_rect(id,&x,&y,&w,&h)==0; }
int tmux_parse_canvas_prefix(const char *line, int *canvas_id, const char **rest){
    while(*line==' '||*line=='\t') line++;
    int n=0, has=0;
    const char *p=line;
    while(*p>='0'&&*p<='9'){ n=n*10+(*p-'0'); p++; has=1; }
    while(*p==' '||*p=='\t') p++;
    if(has && *p=='>'){
        p++;
        while(*p==' '||*p=='\t') p++;
        if(canvas_id) *canvas_id=n;
        if(rest) *rest=p;
        return 1;
    }
    return 0;
}
static int find_adjacent_pane(int dir){
    int win=cur_window;
    int foc=windows[win].focused;
    if(foc<0) return -1;
    pane_t *fp=&panes[foc];
    int best=-1, bestdist=1000000;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win && i!=foc){
        pane_t *q=&panes[i];
        int cand=0;
        if(dir==0){
            if(q->x+q->w <= fp->x && !(q->y+q->h <= fp->y || q->y >= fp->y+fp->h)) cand=1;
        }else if(dir==1){
            if(q->x >= fp->x+fp->w && !(q->y+q->h <= fp->y || q->y >= fp->y+fp->h)) cand=1;
        }else if(dir==2){
            if(q->y+q->h <= fp->y && !(q->x+q->w <= fp->x || q->x >= fp->x+fp->w)) cand=1;
        }else if(dir==3){
            if(q->y >= fp->y+fp->h && !(q->x+q->w <= fp->x || q->x >= fp->x+fp->w)) cand=1;
        }
        if(cand){
            int dx=(fp->x - q->x); if(dx<0) dx=-dx;
            int dy=(fp->y - q->y); if(dy<0) dy=-dy;
            int d=dx+dy;
            if(d<bestdist){ bestdist=d; best=i; }
        }
    }
    return best;
}
static int tmux_split(int vertical){
    int win=cur_window;
    int foc=windows[win].focused;
    if(foc<0) return -1;
    int leaf=find_leaf_for_pane(win,foc);
    if(leaf<0) return -1;
    int new_pane=alloc_pane();
    if(new_pane<0) return -1;
    int na=alloc_node(), nb=alloc_node();
    if(na<0||nb<0) return -1;
    int old_pane=nodes[leaf].pane_id;
    nodes[leaf].is_leaf=0;
    nodes[leaf].split=vertical?0:1;
    nodes[leaf].ratio=128;
    nodes[leaf].a=na; nodes[leaf].b=nb;
    nodes[na].used=1; nodes[na].is_leaf=1; nodes[na].pane_id=old_pane; nodes[na].parent=leaf; nodes[na].a=nodes[na].b=-1;
    nodes[nb].used=1; nodes[nb].is_leaf=1; nodes[nb].pane_id=new_pane; nodes[nb].parent=leaf; nodes[nb].a=nodes[nb].b=-1;
    panes[new_pane].id=new_pane; panes[new_pane].active=1; panes[new_pane].window=win;
    panes[new_pane].is_canvas=0; panes[new_pane].canvas_id=0; panes[new_pane].input_len=0; panes[new_pane].input[0]=0;
    display_create(new_pane, 0,0,100,100);
    windows[win].pane_count++;
    windows[win].focused=new_pane;
    layout_window(win);
    tmux_render();
    log_info("tmux: split %s win %d pane %d -> %d panes", vertical?"vert":"horiz", win, foc, windows[win].pane_count);
    return 0;
}
static int tmux_kill_pane(int pid){
    if(pid<0||pid>=MAX_PANES||!panes[pid].active) return -1;
    int win=panes[pid].window;
    if(windows[win].pane_count<=1) return -1;
    int leaf=find_leaf_for_pane(win,pid);
    if(leaf<0) return -1;
    int parent=nodes[leaf].parent;
    if(parent<0) return -1;
    int sibling = (nodes[parent].a==leaf)?nodes[parent].b:nodes[parent].a;
    int grand=nodes[parent].parent;
    if(grand>=0){
        if(nodes[grand].a==parent) nodes[grand].a=sibling;
        else nodes[grand].b=sibling;
        nodes[sibling].parent=grand;
    }else{
        windows[win].root=sibling;
        nodes[sibling].parent=-1;
    }
    free_node(leaf);
    free_node(parent);
    int was_canvas = panes[pid].is_canvas;
    int was_cid = panes[pid].canvas_id;
    if(was_canvas){
        gpu_set_view(0,0,display_width(),display_height());
    }
    panes[pid].active=0;
    if(was_canvas && was_cid==default_canvas){
        default_canvas=0;
        int best=0;
        for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas){
            if(!best || panes[i].canvas_id<best) best=panes[i].canvas_id;
        }
        default_canvas=best;
    }
    windows[win].pane_count--;
    if(windows[win].focused==pid){
        windows[win].focused=nodes[sibling].is_leaf?nodes[sibling].pane_id:0;
        if(!windows[win].focused) for(int i=0;i<MAX_PANES;i++) if(panes[i].active&&panes[i].window==win){ windows[win].focused=i; break; }
    }
    layout_window(win);
    tmux_render();
    return 0;
}
static int tmux_resize(int dir, int delta){
    int win=cur_window;
    int foc=windows[win].focused;
    if(foc<0) return -1;
    int leaf=find_leaf_for_pane(win,foc);
    if(leaf<0) return -1;
    int cur=leaf;
    while(cur>=0){
        int p=nodes[cur].parent;
        if(p>=0){
            int want = (dir==0||dir==1)?0:1;
            if(nodes[p].split==want){
                int is_a = (nodes[p].a==cur);
                if(dir==0||dir==2){
                    if(is_a) { nodes[p].ratio-=delta; if(nodes[p].ratio<32) nodes[p].ratio=32; }
                    else { nodes[p].ratio+=delta; if(nodes[p].ratio>224) nodes[p].ratio=224; }
                }else{
                    if(is_a) { nodes[p].ratio+=delta; if(nodes[p].ratio>224) nodes[p].ratio=224; }
                    else { nodes[p].ratio-=delta; if(nodes[p].ratio<32) nodes[p].ratio=32; }
                }
                layout_window(win);
                tmux_render();
                return 0;
            }
        }
        if(p<0) break;
        cur=p;
    }
    return -1;
}
static void tmux_new_window(void){
    int win=create_window_with_pane();
    if(win>=0){ cur_window=win; tmux_render(); }
}
static void tmux_switch_window(int delta){
    int start=cur_window;
    for(int i=1;i<MAX_WINDOWS;i++){
        int w=(start+delta*i+MAX_WINDOWS)%MAX_WINDOWS;
        if(windows[w].active){ cur_window=w; tmux_render(); return; }
    }
}
static void tmux_select_window(int idx){
    if(idx>=0&&idx<MAX_WINDOWS&&windows[idx].active){ cur_window=idx; tmux_render(); }
}
static int tmux_handle_canvas(void){
    int win=cur_window;
    int foc=windows[win].focused;
    if(foc<0) return -1;
    pane_t *p=&panes[foc];
    if(p->is_canvas) return -1;
    p->is_canvas=1;
    p->canvas_id=next_canvas++;
    if(!default_canvas) default_canvas=p->canvas_id;
    int vx=p->x+2, vy=p->y+2, vw=p->w-4, vh=p->h-4;
    gpu_set_view(vx,vy,vw,vh);
    gpu_clear(0x040610);
    gpu_text(10,10,"canvas",0xE8ECF5);
    char lab[16]; sprintf(lab,"%d", p->canvas_id);
    gpu_text(80,10,lab,0x2BD97C);
    gpu_present();
    tmux_render();
    return 0;
}
extern void shell_exec(char *line);
static void tmux_exec_line(pane_t *pane, char *line){
    while(*line==' '||*line=='\t') line++;
    if(strcmp(line,"canvas")==0){ int r=tmux_handle_canvas(); display_select(pane->id); if(r==0) printf("canvas %d created\n", panes[pane->id].canvas_id); else printf("canvas failed\n"); return; }
    if(strcmp(line,"panes")==0){
        display_select(pane->id);
        printf("win %d panes %d focused %d\n", cur_window, windows[cur_window].pane_count, windows[cur_window].focused);
        for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==cur_window){
            printf(" pane %d %s %s %dx%d @%d,%d\n", panes[i].id, panes[i].is_canvas?"canvas":"shell", panes[i].id==windows[cur_window].focused?"*":" ", panes[i].w, panes[i].h, panes[i].x, panes[i].y);
        }
        return;
    }
    if(strcmp(line,"windows")==0){
        display_select(pane->id);
        for(int i=0;i<MAX_WINDOWS;i++) if(windows[i].active) printf(" win %d %s panes %d focused %d\n", i, i==cur_window?"*":" ", windows[i].pane_count, windows[i].focused);
        return;
    }
    int cid; const char *rest;
    if(tmux_parse_canvas_prefix(line, &cid, &rest)){
        if(tmux_has_canvas(cid)){
            int x,y,w,h;
            tmux_get_canvas_rect(cid,&x,&y,&w,&h);
            gpu_set_view(x,y,w,h);
            if(strncmp(rest,"plot ",5)==0){
                extern void cmd_plot(const char *a);
                char tmp[256]; strncpy(tmp,rest+5,255); tmp[255]=0;
                display_select(pane->id);
                cmd_plot(tmp);
                return;
            }else if(strncmp(rest,"fft",3)==0){
                extern void cmd_fft(const char *a);
                display_select(pane->id);
                cmd_fft(rest[3]?rest+4:"");
                return;
            }
        }else{
            display_select(pane->id);
            printf("no canvas %d\n", cid);
            return;
        }
    }
    if(strncmp(line,"plot ",5)==0){
        int dc = default_canvas ? default_canvas : tmux_first_canvas_id();
        if(dc && tmux_has_canvas(dc)){
            int x,y,w,h; tmux_get_canvas_rect(dc,&x,&y,&w,&h);
            gpu_set_view(x,y,w,h);
            display_select(pane->id);
            extern void cmd_plot(const char *a);
            cmd_plot(line+5);
            return;
        }
    }
    if(strncmp(line,"fft",3)==0 && (line[3]==0||line[3]==' ')){
        int dc = default_canvas ? default_canvas : tmux_first_canvas_id();
        if(dc && tmux_has_canvas(dc)){
            int x,y,w,h; tmux_get_canvas_rect(dc,&x,&y,&w,&h);
            gpu_set_view(x,y,w,h);
            display_select(pane->id);
            extern void cmd_fft(const char *a);
            cmd_fft(line[3]?line+4:"");
            return;
        }
    }
    int prev=display_current();
    display_select(pane->id);
    shell_exec(line);
    display_select(prev);
    tmux_render();
}
void tmux_init(void){
    memset(panes,0,sizeof(panes));
    memset(windows,0,sizeof(windows));
    memset(nodes,0,sizeof(nodes));
    for(int i=0;i<MAX_NODES;i++) nodes[i].a=nodes[i].b=-1;
    cur_window=0;
    create_window_with_pane();
    tmux_render();
}
void tmux_run(void){
    tmux_init();
    printf("tmux: Ctrl+b prefix | \" horiz | %%/v vert | arrows focus | Ctrl+arrows resize | x kill | c new win | n/p win | 0-9 win | canvas cmd\n");
    for(;;){
        int win=cur_window;
        int foc=windows[win].focused;
        if(foc<0){ __asm__ volatile("hlt"); continue; }
        pane_t *pane=&panes[foc];
        if(pane->is_canvas){
            KeyOutput batch[16];
            int n=0;
            while((n=keyboard_poll_keys(batch,16))==0){
                char ch;
                if(serial_try_getc(&ch)){
                    if(ch==2){ prefix=1; prefix_time=timer_now(); continue; }
                    if(prefix){
                        prefix=0;
                        if(ch=='"'||ch=='s'){ tmux_split(0); break; }
                        else if(ch=='%'||ch=='v'){ tmux_split(1); break; }
                        else if(ch=='c'){ tmux_new_window(); break; }
                        else if(ch=='n'){ tmux_switch_window(1); break; }
                        else if(ch=='p'){ tmux_switch_window(-1); break; }
                        else if(ch>='0'&&ch<='9'){ tmux_select_window(ch-'0'); break; }
                        else if(ch=='x'){ tmux_kill_pane(foc); break; }
                        else if(ch=='h'){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); break; } }
                        else if(ch=='l'){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); break; } }
                        else if(ch=='k'){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); break; } }
                        else if(ch=='j'){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); break; } }
                        continue;
                    }
                }
                __asm__ volatile("hlt");
            }
            for(int i=0;i<n;i++){
                KeyOutput k=batch[i];
                if(!k.pressed) continue;
                const KeyboardState *ks=keyboard_get_state();
                if((ks->lctrl||ks->rctrl) && k.is_character && (k.ascii=='b'||k.ascii=='B')){ prefix=1; prefix_time=timer_now(); continue; }
                if(prefix){
                    prefix=0;
                    if(k.is_character){
                        if(k.ascii=='"'||k.ascii=='s'){ tmux_split(0); continue; }
                        else if(k.ascii=='%'||k.ascii=='v'){ tmux_split(1); continue; }
                        else if(k.ascii=='c'){ tmux_new_window(); continue; }
                        else if(k.ascii=='n'){ tmux_switch_window(1); continue; }
                        else if(k.ascii=='p'){ tmux_switch_window(-1); continue; }
                        else if(k.ascii>='0'&&k.ascii<='9'){ tmux_select_window(k.ascii-'0'); continue; }
                        else if(k.ascii=='x'){ tmux_kill_pane(foc); continue; }
                        else if(k.ascii=='h'){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); } continue; }
                        else if(k.ascii=='l'){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); } continue; }
                        else if(k.ascii=='k'){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); } continue; }
                        else if(k.ascii=='j'){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); } continue; }
                        else if(k.ascii=='H'){ tmux_resize(0,10); continue; }
                        else if(k.ascii=='L'){ tmux_resize(1,10); continue; }
                        else if(k.ascii=='K'){ tmux_resize(2,10); continue; }
                        else if(k.ascii=='J'){ tmux_resize(3,10); continue; }
                    }else{
                        if(k.key_enum==KEY_LEFT){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); } }
                        else if(k.key_enum==KEY_RIGHT){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); } }
                        else if(k.key_enum==KEY_UP){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); } }
                        else if(k.key_enum==KEY_DOWN){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); } }
                        if(k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN){
                            if(ks->lctrl||ks->rctrl){
                                int dir=-1;
                                if(k.key_enum==KEY_LEFT) dir=0;
                                else if(k.key_enum==KEY_RIGHT) dir=1;
                                else if(k.key_enum==KEY_UP) dir=2;
                                else if(k.key_enum==KEY_DOWN) dir=3;
                                if(dir>=0) tmux_resize(dir,10);
                            }
                        }
                    }
                    continue;
                }
            }
            if(timer_now()-prefix_time>200 && prefix) prefix=0;
            continue;
        }
        display_select(pane->id);
        char prompt[32];
        sprintf(prompt,"[%d:%d] shell> ", win, foc);
        display_set_fg(0x2BD97C);
        draw_string(prompt);
        display_set_fg(0xE8ECF5);
        pane->input_len=0;
        pane->input[0]=0;
        for(;;){
            KeyOutput batch[16];
            int n=0;
            while((n=keyboard_poll_keys(batch,16))==0){
                char ch;
                while(serial_try_getc(&ch)){
                    if(ch==2){ prefix=1; prefix_time=timer_now(); goto prefix_serial; }
                    if(prefix){
                        prefix=0;
                        if(ch=='"'||ch=='s'){ tmux_split(0); goto pane_break; }
                        if(ch=='%'||ch=='v'){ tmux_split(1); goto pane_break; }
                        if(ch=='c'){ tmux_new_window(); goto pane_break; }
                        if(ch=='n'){ tmux_switch_window(1); goto pane_break; }
                        if(ch=='p'){ tmux_switch_window(-1); goto pane_break; }
                        if(ch>='0'&&ch<='9'){ tmux_select_window(ch-'0'); goto pane_break; }
                        if(ch=='x'){ tmux_kill_pane(foc); goto pane_break; }
                        if(ch=='h'){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        if(ch=='l'){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        if(ch=='k'){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        if(ch=='j'){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        if(ch=='H'){ tmux_resize(0,10); goto pane_break; }
                        if(ch=='L'){ tmux_resize(1,10); goto pane_break; }
                        if(ch=='K'){ tmux_resize(2,10); goto pane_break; }
                        if(ch=='J'){ tmux_resize(3,10); goto pane_break; }
                        continue;
                    }
                    if(ch=='\r'||ch=='\n'){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        tmux_exec_line(pane, pane->input);
                        goto pane_break;
                    }
                    if(ch==0x03){
                        draw_string("^C\n");
                        tmux_exec_line(pane,"");
                        goto pane_break;
                    }
                    if(ch==0x7f || ch==8){
                        if(pane->input_len>0){ pane->input_len--; pane->input[pane->input_len]=0; draw_string("\b"); }
                        continue;
                    }
                    if(ch>=32&&ch<127 && pane->input_len<250){
                        pane->input[pane->input_len++]=ch; pane->input[pane->input_len]=0;
                        char s[2]={ch,0}; draw_string(s);
                    }
                }
                if(prefix && timer_now()-prefix_time>200) { prefix=0; tmux_render(); }
                __asm__ volatile("hlt");
            }
            prefix_serial:
            for(int i=0;i<n;i++){
                KeyOutput k=batch[i];
                if(!k.pressed) continue;
                const KeyboardState *ks=keyboard_get_state();
                if((ks->lctrl||ks->rctrl) && k.is_character && (k.ascii=='b'||k.ascii=='B')){
                    prefix=1; prefix_time=timer_now(); tmux_render(); continue;
                }
                if(prefix){
                    prefix=0;
                    tmux_render();
                    if(k.is_character){
                        if(k.ascii=='"'||k.ascii=='s'){ tmux_split(0); goto pane_break; }
                        else if(k.ascii=='%'||k.ascii=='v'){ tmux_split(1); goto pane_break; }
                        else if(k.ascii=='c'){ tmux_new_window(); goto pane_break; }
                        else if(k.ascii=='n'){ tmux_switch_window(1); goto pane_break; }
                        else if(k.ascii=='p'){ tmux_switch_window(-1); goto pane_break; }
                        else if(k.ascii>='0'&&k.ascii<='9'){ tmux_select_window(k.ascii-'0'); goto pane_break; }
                        else if(k.ascii=='x'){ tmux_kill_pane(foc); goto pane_break; }
                        else if(k.ascii=='h'){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.ascii=='l'){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.ascii=='k'){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.ascii=='j'){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.ascii=='H'){ tmux_resize(0,10); goto pane_break; }
                        else if(k.ascii=='L'){ tmux_resize(1,10); goto pane_break; }
                        else if(k.ascii=='K'){ tmux_resize(2,10); goto pane_break; }
                        else if(k.ascii=='J'){ tmux_resize(3,10); goto pane_break; }
                    }else{
                        if(k.key_enum==KEY_LEFT){ int nb=find_adjacent_pane(0); if(nb>=0) windows[win].focused=nb; tmux_render(); }
                        else if(k.key_enum==KEY_RIGHT){ int nb=find_adjacent_pane(1); if(nb>=0) windows[win].focused=nb; tmux_render(); }
                        else if(k.key_enum==KEY_UP){ int nb=find_adjacent_pane(2); if(nb>=0) windows[win].focused=nb; tmux_render(); }
                        else if(k.key_enum==KEY_DOWN){ int nb=find_adjacent_pane(3); if(nb>=0) windows[win].focused=nb; tmux_render(); }
                    }
                    if(k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN){
                        if(ks->lctrl||ks->rctrl){
                            int dir=-1;
                            if(k.key_enum==KEY_LEFT) dir=0;
                            else if(k.key_enum==KEY_RIGHT) dir=1;
                            else if(k.key_enum==KEY_UP) dir=2;
                            else if(k.key_enum==KEY_DOWN) dir=3;
                            if(dir>=0) tmux_resize(dir, 10);
                        }else{
                            int nb=-1;
                            if(k.key_enum==KEY_LEFT) nb=find_adjacent_pane(0);
                            else if(k.key_enum==KEY_RIGHT) nb=find_adjacent_pane(1);
                            else if(k.key_enum==KEY_UP) nb=find_adjacent_pane(2);
                            else if(k.key_enum==KEY_DOWN) nb=find_adjacent_pane(3);
                            if(nb>=0) windows[win].focused=nb;
                            tmux_render();
                        }
                    }
                    continue;
                }
                if(k.is_character){
                    if((ks->lctrl||ks->rctrl) && (k.ascii=='c'||k.ascii=='C')){
                        draw_string("^C\n");
                        int pid=proc_kill_latest();
                        if(pid>=0){ char b[32]; sprintf(b,"killed pid=%d\n",pid); draw_string(b); }
                        pane->input_len=0; pane->input[0]=0;
                        goto pane_break;
                    }
                    if(k.ascii=='\n'||k.ascii=='\r'){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        char linecopy[256]; strcpy(linecopy,pane->input);
                        tmux_exec_line(pane, linecopy);
                        goto pane_break;
                    }
                    if(k.ascii=='\b'){
                        if(pane->input_len>0){ pane->input_len--; pane->input[pane->input_len]=0; draw_string("\b"); }
                        continue;
                    }
                    if(k.ascii=='\t') continue;
                    if(pane->input_len<250){
                        pane->input[pane->input_len++]=k.ascii; pane->input[pane->input_len]=0;
                        char s[2]={k.ascii,0}; draw_string(s);
                    }
                }else{
                    if(k.key_enum==KEY_ENTER){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        char linecopy[256]; strcpy(linecopy,pane->input);
                        tmux_exec_line(pane, linecopy);
                        goto pane_break;
                    }else if(k.key_enum==KEY_BACKSPACE){
                        if(pane->input_len>0){ pane->input_len--; pane->input[pane->input_len]=0; draw_string("\b"); }
                    }else if(k.key_enum==KEY_PAGE_UP){
                        display_scroll_up();
                    }else if(k.key_enum==KEY_PAGE_DOWN){
                        display_scroll_down();
                    }
                }
            }
        }
        pane_break:;
    }
}
void tmux_status_tick(void){
    tmux_render();
}
