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
#include <widget.h>
#include <canvas_widget.h>
#include <event.h>
#include <font.h>
#include <font_small.h>
#include <mat.h>
#include <calc.h>
#include <cmd.h>
#include <fs.h>
#include <edit.h>
#include <user_progs.h>

static int exec_depth = 0;

static void tmux_autofit_window(int win);

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
    int input_pos;
    uint32_t input_row;
    int input_maxlen;
    int esc_state;
    int is_shell;
    uint32_t canvas_buf[1920*1080];
    int canvas_has_content;
    int canvas_w, canvas_h;
    int prompt_shown;
    int sticky_cid;
} pane_t;

static void tmux_exec_line(pane_t *pane, char *line);

typedef struct {
    int id;
    int active;
    char name[16];
    int root;
    int pane_count;
    int focused;
} window_t;
static multiboot_info_t *g_mb = 0;
static void canvas_save(pane_t *p){
    if(!p||!p->is_canvas||!g_mb) return;
    int vx=p->x+2, vy=p->y+2, vw=p->w-4, vh=p->h-4;
    log_debug("ui: canvas_save pane=%d cid=%d %dx%d has=%d", p->id, p->canvas_id, vw, vh, p->canvas_has_content);
    p->canvas_w=vw; p->canvas_h=vh;
    uint32_t *src_fb;
    uint32_t src_pitch;
    if(display_is_double_buffered()){
        src_fb=display_get_backbuffer();
        src_pitch=1920*4;
    }else{
        src_fb=(uint32_t*)(uintptr_t)g_mb->framebuffer_addr;
        src_pitch=g_mb->framebuffer_pitch;
    }
    if(!src_fb) return;
    for(int y=0;y<vh;y++){
        uint32_t *src=(uint32_t*)((uint8_t*)src_fb + (vy+y)*src_pitch + vx*4);
        memcpy(p->canvas_buf + y*1920, src, vw*4);
    }
    p->canvas_has_content=1;
}
static void canvas_restore(pane_t *p){
    if(!p||!p->is_canvas||!p->canvas_has_content||!g_mb) return;
    int vx=p->x+2, vy=p->y+2, vw=p->canvas_w, vh=p->canvas_h;
    if(vw!=p->w-4 || vh!=p->h-4){
        log_debug("ui: canvas_restore SKIP pane=%d size %dx%d vs %dx%d", p->id, vw, vh, p->w-4, p->h-4);
        return;
    }
    log_debug("ui: canvas_restore pane=%d cid=%d", p->id, p->canvas_id);
    uint32_t *dst_fb;
    uint32_t dst_pitch;
    if(display_is_double_buffered()){
        dst_fb=display_get_backbuffer();
        dst_pitch=1920*4;
    }else{
        dst_fb=(uint32_t*)(uintptr_t)g_mb->framebuffer_addr;
        dst_pitch=g_mb->framebuffer_pitch;
    }
    if(!dst_fb) return;
    for(int y=0;y<vh;y++){
        uint32_t *dst=(uint32_t*)((uint8_t*)dst_fb + (vy+y)*dst_pitch + vx*4);
        memcpy(dst, p->canvas_buf + y*1920, vw*4);
    }
    display_dirty(vx, vy, vw, vh);
}

static pane_t panes[MAX_PANES];
static window_t windows[MAX_WINDOWS];
static tnode_t nodes[MAX_NODES];
static widget_t *pane_widgets[MAX_PANES];
static widget_t *window_roots[MAX_WINDOWS];
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
    int cx0=0, cy0=0, cx1=W, cy1=H-BAR_H;
    int root=windows[win].root;
    if(root>=0) layout_node(root, cx0, cy0, cx1-cx0, cy1-cy0);
    if(window_roots[win]){
        window_roots[win]->x=cx0; window_roots[win]->y=cy0; window_roots[win]->w=cx1-cx0; window_roots[win]->h=cy1-cy0;
    }
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win){
        widget_t *wd = pane_widgets[i];
        if(wd){
            wd->x=panes[i].x; wd->y=panes[i].y; wd->w=panes[i].w; wd->h=panes[i].h;
            wd->focused = (panes[i].id==windows[win].focused);
            if(panes[i].is_canvas){
                wd->type=WIDGET_CANVAS;
                wd->canvas_id=panes[i].canvas_id;
            }else{
                wd->type=WIDGET_SHELL;
                wd->canvas_id=0;
            }
        }
    }
}
static void render_pane_chrome(int is_focused, pane_t *p){
    uint32_t col = p->is_canvas ? 0x2BD97C : (is_focused ? 0x2BD97C : 0x2A2E4A);
    draw_rect(p->x, p->y, p->w, 2, col);
    draw_rect(p->x, p->y+p->h-2, p->w, 2, col);
    draw_rect(p->x, p->y, 2, p->h, col);
    draw_rect(p->x+p->w-2, p->y, 2, p->h, col);
    if(p->is_canvas){
        char lab[24];
        sprintf(lab,"canvas %d", p->canvas_id);
        int prev=display_current();
        display_select(p->id);
        int tw = (int)strlen(lab)*9;
        int sx = p->x + p->w - tw - 12;
        int sy = p->y + 6;
        uint32_t bg = is_focused ? col : 0x1A1F3A;
        uint32_t fg = is_focused ? 0x0B1020 : 0xE8ECF5;
        draw_rect(sx-4, sy-2, tw+8, 12, bg);
        for(int i=0; lab[i]; i++){
            uint8_t bmp[FONT_H*FONT_BPR];
            get_font_bitmap(lab[i], bmp);
            for(int r=0;r<FONT_H;r+=2) for(int cc=0;cc<FONT_W;cc+=2){
                int sr = r/2, sc = cc/2;
                int byte = r*FONT_BPR + cc/8;
                int bit = 7 - (cc%8);
                if((bmp[byte]>>bit)&1) draw_pixel(sx+i*9+sc, sy+sr, fg);
            }
        }
        display_select(prev);
    }
}
static void tmux_render(void){
    uint32_t W=display_width(), H=display_height();
    display_cursor_hide();
    log_debug("ui: render win=%d foc=%d", cur_window, windows[cur_window].focused);
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==cur_window && panes[i].is_canvas && panes[i].canvas_has_content) canvas_save(&panes[i]);
    draw_rect(0,0,W,H-BAR_H,0x0B1020);
    int win = cur_window;
    int pane_count=0;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win) pane_count++;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win){
        int foc = (panes[i].id==windows[win].focused);
        if(!panes[i].is_canvas){
            draw_rect(panes[i].x, panes[i].y, panes[i].w, panes[i].h, 0x0B1020);
            display_render(i);
            render_pane_chrome(foc, &panes[i]);
        }else{
            if(panes[i].canvas_has_content) canvas_restore(&panes[i]);
            else{
                draw_rect(panes[i].x+2, panes[i].y+2, panes[i].w-4, panes[i].h-4, 0x0B1020);
                int vx=panes[i].x+2, vy=panes[i].y+2, vw=panes[i].w-4, vh=panes[i].h-4;
                for(int x=0;x<vw;x+=40) for(int y=0;y<vh;y++) draw_pixel(vx+x, vy+y, 0x1A1F3A);
                for(int y=0;y<vh;y+=40) for(int x=0;x<vw;x++) draw_pixel(vx+x, vy+y, 0x1A1F3A);
            }
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
        uint32_t fg = iscur ? 0x0B1020 : 0xE8ECF5;
        int tw = (int)strlen(tmp)*9;
        draw_rect(sx, H-BAR_H+10, tw, 16, bg);
        for(int k=0; tmp[k]; k++){
            uint8_t bmp[FONT_H*FONT_BPR];
            get_font_bitmap(tmp[k], bmp);
            for(int r=0;r<FONT_H;r+=2) for(int cc=0;cc<FONT_W;cc+=2){
                int sr=r/2, sc=cc/2;
                if((bmp[r*FONT_BPR+cc/8]>>(7-(cc%8)))&1) draw_pixel(sx+2+k*9+sc, H-BAR_H+14+sr, fg);
            }
        }
        sx+=tw+10;
    }
    char extra[64];
    sprintf(extra,"panes %d", pane_count);
    int prev2=display_current();
    display_select(0);
    for(int k=0; extra[k]; k++){
        uint8_t bmp[FONT_H*FONT_BPR];
        get_font_bitmap(extra[k], bmp);
        for(int r=0;r<FONT_H;r+=2) for(int cc=0;cc<FONT_W;cc+=2){
            if((bmp[r*FONT_BPR+cc/8]>>(7-(cc%8)))&1) draw_pixel(sx+10+k*9+cc/2, H-BAR_H+14+r/2, 0x8A93B2);
        }
    }
    display_select(prev2);
    if(prefix){
        const char *pfx=" PREFIX ";
        int pw=(int)strlen(pfx)*9;
        draw_rect(W-pw-12, H-BAR_H+10, pw, 16, 0xFFD60A);
        for(int k=0; pfx[k]; k++){
            uint8_t bmp[FONT_H*FONT_BPR];
            get_font_bitmap(pfx[k], bmp);
            for(int r=0;r<FONT_H;r+=2) for(int cc=0;cc<FONT_W;cc+=2){
                if((bmp[r*FONT_BPR+cc/8]>>(7-(cc%8)))&1) draw_pixel(W-pw-10+k*9+cc/2, H-BAR_H+14+r/2, 0x0B1020);
            }
        }
    }
    display_present();
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
    panes[pid].prompt_shown=0;
    panes[pid].canvas_has_content=0;
    panes[pid].sticky_cid=0;
    display_create(pid, 0,0, 100,100);
    widget_t *wd = widget_create(WIDGET_SHELL, 0,0,100,100,0);
    if(wd){
        wd->id = pid;
        pane_widgets[pid]=wd;
        if(window_roots[win]){
            widget_add_child(window_roots[win], wd);
        }
    }
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
    int W=display_width(), H=display_height();
    widget_t *root = widget_create(WIDGET_CONTAINER, 0, 0, W, H-BAR_H,0);
    if(root){
        root->split=SPLIT_NONE;
        window_roots[win]=root;
    }
    int node=alloc_node();
    int pane=create_pane_for_window(win);
    if(pane<0||node<0) return -1;
    nodes[node].used=1; nodes[node].is_leaf=1; nodes[node].pane_id=pane; nodes[node].parent=-1;
    windows[win].root=node;
    windows[win].pane_count=1;
    windows[win].focused=pane;
    if(pane_widgets[pane] && root){
        pane_widgets[pane]->x=0; pane_widgets[pane]->y=BAR_H; pane_widgets[pane]->w=W; pane_widgets[pane]->h=H-2*BAR_H;
    }
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
static int resolve_canvas(pane_t *pane){
    if(pane && pane->sticky_cid && tmux_has_canvas(pane->sticky_cid))
        return pane->sticky_cid;
    int dc = default_canvas ? default_canvas : tmux_first_canvas_id();
    if(dc && tmux_has_canvas(dc)) return dc;
    return 0;
}
int tmux_canvas_count(void){
    int n=0;
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas) n++;
    return n;
}
void tmux_set_default_canvas(int id){ default_canvas=id; }
int tmux_has_canvas(int id){ int x,y,w,h; return tmux_get_canvas_rect(id,&x,&y,&w,&h)==0; }
void tmux_mark_canvas_dirty(int cid){
    for(int i=0;i<MAX_PANES;i++)
        if(panes[i].active && panes[i].is_canvas && panes[i].canvas_id==cid){
            panes[i].canvas_has_content=1;
            log_debug("ui: canvas %d marked dirty by graph", cid);
        }
}
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
    panes[new_pane].prompt_shown=0; panes[new_pane].canvas_has_content=0; panes[new_pane].sticky_cid=0;
    display_create(new_pane, 0,0,100,100);
    widget_t *wd = widget_create(WIDGET_SHELL, 0,0,100,100,0);
    if(wd){ wd->id=new_pane; pane_widgets[new_pane]=wd; if(window_roots[win]) widget_add_child(window_roots[win], wd); }
    windows[win].pane_count++;
    windows[win].focused=new_pane;
    layout_window(win);
    tmux_render();
    int prev=display_current();
    display_select(new_pane);
    display_clear();
    display_select(prev);
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
    if(pane_widgets[pid]){
        if(window_roots[win]) widget_remove_child(window_roots[win], pane_widgets[pid]);
        widget_destroy(pane_widgets[pid]);
        pane_widgets[pid]=0;
    }
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
                tmux_autofit_window(win);
                return 0;
            }
        }
        if(p<0) break;
        cur=p;
    }
    return -1;
}

static void tmux_autofit_window(int win){
    if(gpu_owned()) return;
    int prev = display_current();
    for(int i=0;i<MAX_PANES;i++){
        if(!panes[i].active || panes[i].window!=win || !panes[i].is_canvas) continue;
        int x,y,w,h;
        if(tmux_get_canvas_rect(panes[i].canvas_id,&x,&y,&w,&h)) continue;
        gpu_set_view(x,y,w,h);
        display_select(panes[i].id);
        mat_set_target(panes[i].canvas_id);
        if(mat_refit_plot(panes[i].canvas_id) == 0)
            tmux_mark_canvas_dirty(panes[i].canvas_id);
    }
    display_select(prev);
    tmux_render();
}
static void tmux_new_window(void){
    int win=create_window_with_pane();
    if(win>=0){ cur_window=win; tmux_render(); }
}
static void tmux_switch_window(int delta){
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas && panes[i].window==cur_window) canvas_save(&panes[i]);
    int start=cur_window;
    for(int i=1;i<MAX_WINDOWS;i++){
        int w=(start+delta*i+MAX_WINDOWS)%MAX_WINDOWS;
        if(windows[w].active){ cur_window=w; tmux_render(); for(int j=0;j<MAX_PANES;j++) if(panes[j].active && panes[j].is_canvas && panes[j].window==w) canvas_restore(&panes[j]); return; }
    }
}
static void tmux_select_window(int idx){
    if(idx>=0&&idx<MAX_WINDOWS&&windows[idx].active){
        for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas && panes[i].window==cur_window) canvas_save(&panes[i]);
        cur_window=idx; tmux_render();
        for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].is_canvas && panes[i].window==idx) canvas_restore(&panes[i]);
    }
}
static int canvas_in_pane(pane_t *p){
    if(!p) return -1;
    int win=p->window;
    int foc=p->id;
    if(foc<0) return -1;
    if(p->is_canvas) return -1;
    p->is_canvas=1;
    p->canvas_id=next_canvas++;
    if(!default_canvas) default_canvas=p->canvas_id;
    int vx=p->x+2, vy=p->y+2, vw=p->w-4, vh=p->h-4;
    gpu_set_view(vx,vy,vw,vh);
    gpu_clear(0x0B1020);
    for(int x=0;x<vw;x+=40) gpu_line(x,0,x,vh,0x1A1F3A);
    for(int y=0;y<vh;y+=40) gpu_line(0,y,vw,y,0x1A1F3A);
    gpu_rect(0,0,vw,1,0x2A2E4A);
    gpu_rect(0,vh-1,vw,1,0x2A2E4A);
    gpu_rect(0,0,1,vh,0x2A2E4A);
    gpu_rect(vw-1,0,1,vh,0x2A2E4A);
    gpu_text(vw/2-40, vh/2-16, "Canvas", 0x8A93B2);
    char lab[16]; sprintf(lab,"%d", p->canvas_id);
    gpu_text(vw/2+20, vh/2-16, lab, 0x2BD97C);
    gpu_present();
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win && !panes[i].is_canvas && i!=foc){ windows[win].focused=i; break; }
    tmux_render();
    log_info("tmux: canvas %d in pane %d win %d", p->canvas_id, foc, win);
    return 0;
}
int tmux_create_canvas_in(int pane_id){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return -1;
    return canvas_in_pane(&panes[pane_id]);
}
int tmux_pane_is_shell(int pane_id){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return 0;
    return !panes[pane_id].is_canvas;
}
int tmux_resolve_canvas(int pane_id){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return 0;
    return resolve_canvas(&panes[pane_id]);
}
void tmux_begin_canvas(int cid){
    int x,y,w,h;
    if(tmux_get_canvas_rect(cid,&x,&y,&w,&h)) return;
    gpu_set_view(x,y,w,h);
    mat_set_target(cid);
}
static int try_run_file(pane_t *pane, const char *name);
int tmux_try_run_file(int pane_id, const char *name){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return -1;
    return try_run_file(&panes[pane_id], name);
}
int tmux_pane_canvas_id(int pane_id){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return 0;
    return panes[pane_id].canvas_id;
}
int tmux_sticky_set(int pane_id, int cid){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return -1;
    panes[pane_id].sticky_cid=cid;
    return 0;
}
void tmux_sticky_clear(int pane_id){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return;
    panes[pane_id].sticky_cid=0;
}
int tmux_edit_file(int pane_id, const char *path){
    if(pane_id<0||pane_id>=MAX_PANES||!panes[pane_id].active) return -1;
    pane_t *pane=&panes[pane_id];
    if(pane->is_canvas) return -1;
    edit_file(path, pane->x, pane->y, pane->w, pane->h);
    tmux_render();
    return 0;
}
int tmux_list_panes(int pane_id){
    display_select(pane_id);
    printf("win %d panes %d focused %d\n", cur_window, windows[cur_window].pane_count, windows[cur_window].focused);
    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==cur_window){
        char st[8];
        if(panes[i].sticky_cid) sprintf(st, "c%d", panes[i].sticky_cid);
        else st[0]=0;
        printf(" pane %d %s %s%s %dx%d @%d,%d\n", panes[i].id, panes[i].is_canvas?"canvas":"shell", panes[i].id==windows[cur_window].focused?"*":" ", st, panes[i].w, panes[i].h, panes[i].x, panes[i].y);
    }
    return 0;
}
int tmux_list_windows(int pane_id){
    display_select(pane_id);
    for(int i=0;i<MAX_WINDOWS;i++) if(windows[i].active) printf(" win %d %s panes %d focused %d\n", i, i==cur_window?"*":" ", windows[i].pane_count, windows[i].focused);
    return 0;
}
static int tmux_run_script(pane_t *pane, const char *path) {
    if (exec_depth >= 3) {
        display_select(pane->id);
        printf("run: nesting too deep\n");
        return -1;
    }
    char content[4096];
    if (fs_read(path, content, sizeof(content)) < 0) return -1;
    exec_depth++;
    char line[256];
    int li = 0;
    for (int i = 0; ; i++) {
        char c = content[i];
        if (!pane->active) break;
        if (c == '\n' || c == 0) {
            line[li] = 0;
            const char *s = line;
            while (*s == ' ' || *s == '\t') s++;
            if (*s && *s != '#') {
                display_select(pane->id);
                tmux_exec_line(pane, (char *)s);
            }
            li = 0;
            if (!c) break;
        } else if (c != '\r' && li < 250) {
            line[li++] = c;
        }
    }
    exec_depth--;
    return 0;
}

static int try_run_file(pane_t *pane, const char *name) {
    if (fs_exists(name)) {
        display_select(pane->id);
        printf("run %s\n", name);
        return tmux_run_script(pane, name);
    }
    char with_ext[64];
    int i = 0;
    while (name[i] && i < 59) { with_ext[i] = name[i]; i++; }
    with_ext[i++] = '.';
    with_ext[i++] = 'e';
    with_ext[i++] = 'z';
    with_ext[i] = 0;
    if (fs_exists(with_ext)) {
        display_select(pane->id);
        printf("run %s\n", with_ext);
        return tmux_run_script(pane, with_ext);
    }
    return -1;
}

static void tmux_exec_line(pane_t *pane, char *line){
    while(*line==' '||*line=='\t') line++;
    {
        char lbuf[48];
        int li=0;
        while(line[li] && li<47){ lbuf[li]=line[li]; li++; }
        lbuf[li]=0;
        log_debug("ui: exec pane=%d line='%s'", pane->id, lbuf);
    }
    cmd_ctx_t ctx;
    ctx.pane_id=pane->id;
    ctx.canvas_id=0;
    ctx.has_canvas=0;
    ctx.strict=0;
    int prev=display_current();
    display_select(pane->id);
    {
        int n=0, i=0;
        while(line[i]>='0'&&line[i]<='9'){ n=n*10+line[i]-'0'; i++; }
        if(i>0 && line[i]=='#' && line[i+1]==0 && tmux_has_canvas(n)){
            tmux_sticky_set(pane->id, n);
            printf("shell sticky to canvas %d\n", n);
            goto done;
        }
    }
    {
        int cid;
        const char *rest;
        if(tmux_parse_canvas_prefix(line, &cid, &rest)){
            ctx.canvas_id=cid;
            if(tmux_has_canvas(cid)){
                tmux_begin_canvas(cid);
                ctx.has_canvas=1;
                ctx.strict=0;
            }else{
                ctx.has_canvas=0;
                ctx.strict=1;
            }
            if(!cmd_dispatch(&ctx, (char*)rest))
                printf("unknown: %s (try help)\n", rest);
            goto done;
        }
    }
    if(!cmd_dispatch(&ctx, line))
        printf("unknown: %s (try help)\n", line);
done:
    display_select(prev);
    tmux_render();
}
multiboot_info_t *tmux_mb_info=0;
void tmux_init(multiboot_info_t *mb){
    tmux_mb_info=mb;
    g_mb=mb;
    log_debug("ui: tmux_init fb=%x", mb ? mb->framebuffer_addr : 0);
    memset(panes,0,sizeof(panes));
    memset(windows,0,sizeof(windows));
    memset(nodes,0,sizeof(nodes));
    for(int i=0;i<MAX_NODES;i++) nodes[i].a=nodes[i].b=-1;
    memset(pane_widgets,0,sizeof(pane_widgets));
    memset(window_roots,0,sizeof(window_roots));
    uint32_t *fb=0; uint32_t pitch=0, sw=0, sh=0;
    if(mb){ fb=(uint32_t*)(uintptr_t)mb->framebuffer_addr; pitch=mb->framebuffer_pitch; sw=mb->framebuffer_width; sh=mb->framebuffer_height; }
    widget_system_init(fb, pitch, sw, sh);
    canvas_widget_init();
    cur_window=0;
    create_window_with_pane();
    display_enable_double_buffer(1);
    tmux_render();
}
static void input_reset(pane_t *pane){
    pane->input_len=0;
    pane->input[0]=0;
    pane->input_pos=0;
    pane->input_maxlen=0;
    pane->esc_state=0;
}

static void input_redraw(pane_t *pane){
    int prev = display_current();
    display_select(pane->id);
    display_cursor_hide();
    display_ensure_live();
    pane->input_row = display_cursor_row();
    display_set_cell(pane->input_row, 0);
    for(int i=0;i<pane->input_maxlen+1;i++) draw_char(' ');
    display_set_cell(pane->input_row, 0);
    for(int i=0;i<pane->input_len;i++){
        char s[2]={pane->input[i],0};
        draw_string(s);
    }
    display_set_cell(pane->input_row, (uint32_t)pane->input_pos);
    display_select(prev);
}

static void input_insert(pane_t *pane, char c){
    if(pane->input_len>=250) return;
    if(pane->input_pos<0) pane->input_pos=0;
    if(pane->input_pos>pane->input_len) pane->input_pos=pane->input_len;
    memmove(pane->input+pane->input_pos+1, pane->input+pane->input_pos, (unsigned)(pane->input_len-pane->input_pos));
    pane->input[pane->input_pos]=c;
    pane->input_len++;
    pane->input_pos++;
    pane->input[pane->input_len]=0;
    if(pane->input_len>pane->input_maxlen) pane->input_maxlen=pane->input_len;
    input_redraw(pane);
}

static void input_backspace(pane_t *pane){
    if(pane->input_pos<=0) return;
    if(pane->input_pos>pane->input_len) pane->input_pos=pane->input_len;
    memmove(pane->input+pane->input_pos-1, pane->input+pane->input_pos, (unsigned)(pane->input_len-pane->input_pos));
    pane->input_pos--;
    pane->input_len--;
    pane->input[pane->input_len]=0;
    input_redraw(pane);
}

static void input_delete_at(pane_t *pane){
    if(pane->input_pos<0) pane->input_pos=0;
    if(pane->input_pos>=pane->input_len) return;
    memmove(pane->input+pane->input_pos, pane->input+pane->input_pos+1, (unsigned)(pane->input_len-pane->input_pos-1));
    pane->input_len--;
    pane->input[pane->input_len]=0;
    input_redraw(pane);
}

static void input_move(pane_t *pane, int delta){
    int np=pane->input_pos+delta;
    if(np<0) np=0;
    if(np>pane->input_len) np=pane->input_len;
    if(np==pane->input_pos) return;
    pane->input_pos=np;
    int prev = display_current();
    display_select(pane->id);
    display_cursor_hide();
    display_ensure_live();
    if(display_cursor_row()==pane->input_row) display_set_cell(pane->input_row, (uint32_t)pane->input_pos);
    display_select(prev);
}

void tmux_run(multiboot_info_t *mb){
    tmux_init(mb);
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
                    if(ch=='\r'||ch=='\n'||ch=='\b'||ch==127||(ch>=32&&ch<127)){
                        int sh=-1;
                        for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win && !panes[i].is_canvas){ sh=i; break; }
                        if(sh>=0 && sh!=foc){ windows[win].focused=sh; tmux_render(); log_debug("ui: canvas typing -> shell pane=%d", sh); }
                        break;
                    }
                }
                display_present();
                __asm__ volatile("hlt");
            }
            for(int i=0;i<n;i++){
                KeyOutput k=batch[i];
                if(!k.pressed) continue;
                const KeyboardState *ks=keyboard_get_state();
                int ctrl = ks->lctrl||ks->rctrl;
                int alt = ks->lalt||ks->ralt;
                if(!prefix && ctrl && !alt && (k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN)){
                    int dir=-1;
                    if(k.key_enum==KEY_LEFT) dir=0;
                    else if(k.key_enum==KEY_RIGHT) dir=1;
                    else if(k.key_enum==KEY_UP) dir=2;
                    else if(k.key_enum==KEY_DOWN) dir=3;
                    if(dir>=0){ int nb=find_adjacent_pane(dir); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                    continue;
                }
                if(!prefix && ctrl && alt && (k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN)){
                    int dir=-1;
                    if(k.key_enum==KEY_LEFT) dir=0;
                    else if(k.key_enum==KEY_RIGHT) dir=1;
                    else if(k.key_enum==KEY_UP) dir=2;
                    else if(k.key_enum==KEY_DOWN) dir=3;
                    if(dir>=0) tmux_resize(dir,10);
                    continue;
                }
                if((ks->lctrl||ks->rctrl) && k.is_character && (k.ascii=='b'||k.ascii=='B')){ prefix=1; prefix_time=timer_now(); continue; }
                if(!prefix && !ctrl && !alt && k.is_character && (k.ascii=='\r'||k.ascii=='\n'||k.ascii=='\b'||(k.ascii>=32&&k.ascii<127))){
                    int sh2=-1;
                    for(int i=0;i<MAX_PANES;i++) if(panes[i].active && panes[i].window==win && !panes[i].is_canvas){ sh2=i; break; }
                    if(sh2>=0 && sh2!=foc){ windows[win].focused=sh2; tmux_render(); log_debug("ui: canvas typing -> shell pane=%d", sh2); goto pane_break; }
                    continue;
                }
                if(prefix){
                    prefix=0;
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
                        else if(k.ascii=='H'){ tmux_resize(0,10); continue; }
                        else if(k.ascii=='L'){ tmux_resize(1,10); continue; }
                        else if(k.ascii=='K'){ tmux_resize(2,10); continue; }
                        else if(k.ascii=='J'){ tmux_resize(3,10); continue; }
                    }else{
                        if(k.key_enum==KEY_LEFT){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_RIGHT){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_UP){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_DOWN){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
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
        if(!pane->prompt_shown){
            char prompt[32];
            sprintf(prompt, "shell %d >\n", pane->id);
            log_debug("ui: prompt pane=%d", pane->id);
            display_set_fg(0x2BD97C);
            draw_string(prompt);
            display_set_fg(0xE8ECF5);
            pane->prompt_shown=1;
        } else {
            log_debug("ui: prompt skip pane=%d", pane->id);
        }
        input_reset(pane);
        pane->input_row = display_cursor_row();
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
                        pane->prompt_shown=0;
                        tmux_exec_line(pane, pane->input);
                        goto pane_break;
                    }
                    if(pane->esc_state==1){
                        pane->esc_state = (ch=='[') ? 2 : 0;
                        continue;
                    }
                    if(pane->esc_state==2){
                        if(ch=='D'){ pane->esc_state=0; input_move(pane,-1); }
                        else if(ch=='C'){ pane->esc_state=0; input_move(pane,1); }
                        else if(ch=='3'){ pane->esc_state=3; }
                        else pane->esc_state=0;
                        continue;
                    }
                    if(pane->esc_state==3){
                        pane->esc_state=0;
                        if(ch=='~') input_delete_at(pane);
                        continue;
                    }
                    if(ch==0x1B){ pane->esc_state=1; continue; }
                    if(ch=='\r'||ch=='\n'){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        pane->prompt_shown=0;
                        tmux_exec_line(pane, pane->input);
                        goto pane_break;
                    }
                    if(ch==0x03){
                        draw_string("^C\n");
                        pane->prompt_shown=0;
                        tmux_exec_line(pane,"");
                        goto pane_break;
                    }
                    if(ch==0x7f || ch==8){
                        input_backspace(pane);
                        continue;
                    }
                    if(ch>=32&&ch<127){
                        input_insert(pane, ch);
                    }
                }
                if(prefix && timer_now()-prefix_time>200) { prefix=0; tmux_render(); }
                display_cursor_tick();
                display_present();
                __asm__ volatile("hlt");
            }
            prefix_serial:
            for(int i=0;i<n;i++){
                KeyOutput k=batch[i];
                if(!k.pressed) continue;
                const KeyboardState *ks=keyboard_get_state();
                int ctrl = ks->lctrl||ks->rctrl;
                int alt = ks->lalt||ks->ralt;
                if(!prefix && ctrl && !alt && (k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN)){
                    int dir=-1;
                    if(k.key_enum==KEY_LEFT) dir=0;
                    else if(k.key_enum==KEY_RIGHT) dir=1;
                    else if(k.key_enum==KEY_UP) dir=2;
                    else if(k.key_enum==KEY_DOWN) dir=3;
                    if(dir>=0){ int nb=find_adjacent_pane(dir); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                    continue;
                }
                if(!prefix && ctrl && alt && (k.key_enum==KEY_LEFT||k.key_enum==KEY_RIGHT||k.key_enum==KEY_UP||k.key_enum==KEY_DOWN)){
                    int dir=-1;
                    if(k.key_enum==KEY_LEFT) dir=0;
                    else if(k.key_enum==KEY_RIGHT) dir=1;
                    else if(k.key_enum==KEY_UP) dir=2;
                    else if(k.key_enum==KEY_DOWN) dir=3;
                    if(dir>=0) tmux_resize(dir,10);
                    continue;
                }
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
                        if(k.key_enum==KEY_LEFT){ int nb=find_adjacent_pane(0); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_RIGHT){ int nb=find_adjacent_pane(1); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_UP){ int nb=find_adjacent_pane(2); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
                        else if(k.key_enum==KEY_DOWN){ int nb=find_adjacent_pane(3); if(nb>=0){ windows[win].focused=nb; tmux_render(); goto pane_break; } }
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
                        input_reset(pane);
                        pane->prompt_shown=0;
                        goto pane_break;
                    }
                    if(k.ascii=='\n'||k.ascii=='\r'){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        pane->prompt_shown=0;
                        char linecopy[256]; strcpy(linecopy,pane->input);
                        tmux_exec_line(pane, linecopy);
                        goto pane_break;
                    }
                    if(k.ascii=='\b'){
                        input_backspace(pane);
                        continue;
                    }
                    if(k.ascii=='\t') continue;
                    input_insert(pane, k.ascii);
                }else{
                    if(k.key_enum==KEY_LEFT && !ks->lctrl && !ks->rctrl && !ks->lalt && !ks->ralt){ input_move(pane,-1); continue; }
                    if(k.key_enum==KEY_RIGHT && !ks->lctrl && !ks->rctrl && !ks->lalt && !ks->ralt){ input_move(pane,1); continue; }
                    if(k.key_enum==KEY_DELETE){ input_delete_at(pane); continue; }
                    if(k.key_enum==KEY_ENTER){
                        draw_string("\n");
                        pane->input[pane->input_len]=0;
                        pane->prompt_shown=0;
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
