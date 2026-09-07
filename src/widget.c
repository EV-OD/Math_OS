#include <widget.h>
#include <display.h>
#include <gpu.h>
#include <string.h>
#include <stdio.h>

static widget_t pool[WIDGET_MAX];
static int pool_used[WIDGET_MAX];
static widget_t *focused = 0;

static void draw_shell(widget_t *w){
    display_render(w->id);
    uint32_t col = w->focused ? 0x2BD97C : 0x2A2E4A;
    draw_rect(w->x, w->y, w->w, 2, col);
    draw_rect(w->x, w->y+w->h-2, w->w, 2, col);
    draw_rect(w->x, w->y, 2, w->h, col);
    draw_rect(w->x+w->w-2, w->y, 2, w->h, col);
    char lab[24];
    sprintf(lab,"shell %d", w->id);
    int prev=display_current();
    display_select(w->id);
    uint32_t bg = w->focused ? col : 0x1A1F3A;
    draw_rect(w->x+8, w->y+6, (uint32_t)strlen(lab)*18+8, 20, bg);
    display_set_fg(w->focused?0x0B1020:0xE8ECF5);
    display_set_bg(bg);
    display_text_at(w->x+8, w->y+6, lab);
    display_set_fg(0xE8ECF5);
    display_set_bg(0x0B1020);
    display_select(prev);
}
static void draw_canvas_w(widget_t *w){
    uint32_t col = w->focused ? 0x2BD97C : 0x2A2E4A;
    draw_rect(w->x, w->y, w->w, 2, col);
    draw_rect(w->x, w->y+w->h-2, w->w, 2, col);
    draw_rect(w->x, w->y, 2, w->h, col);
    draw_rect(w->x+w->w-2, w->y, 2, w->h, col);
    int vx=w->x+2, vy=w->y+2, vw=w->w-4, vh=w->h-4;
    gpu_set_view(vx,vy,vw,vh);
    gpu_clear(0x0B1020);
    char lab[24];
    sprintf(lab,"canvas %d", w->canvas_id);
    gpu_text(10,10,lab,0x2BD97C);
    gpu_present();
    int prev=display_current();
    display_select(w->id);
    uint32_t bg = w->focused ? col : 0x1A1F3A;
    draw_rect(w->x+8, w->y+6, (uint32_t)strlen(lab)*18+8, 20, bg);
    display_set_fg(w->focused?0x0B1020:0xE8ECF5);
    display_set_bg(bg);
    display_text_at(w->x+8, w->y+6, lab);
    display_set_fg(0xE8ECF5);
    display_set_bg(0x0B1020);
    display_select(prev);
}
static void draw_container(widget_t *w){
    for(int i=0;i<w->child_count;i++) if(w->children[i] && w->children[i]->visible) widget_draw_tree(w->children[i]);
}
void widget_system_init(void){
    memset(pool,0,sizeof(pool));
    memset(pool_used,0,sizeof(pool_used));
    focused=0;
}
widget_t *widget_create(widget_type_t type,int x,int y,int w,int h){
    for(int i=0;i<WIDGET_MAX;i++) if(!pool_used[i]){
        pool_used[i]=1;
        widget_t *wh=&pool[i];
        memset(wh,0,sizeof(*wh));
        wh->id=i;
        wh->type=type;
        wh->x=x; wh->y=y; wh->w=w; wh->h=h;
        wh->visible=1;
        wh->focused=0;
        wh->split=SPLIT_NONE;
        wh->ratio=128;
        wh->canvas_id=0;
        if(type==WIDGET_SHELL) wh->draw=draw_shell;
        else if(type==WIDGET_CANVAS) wh->draw=draw_canvas_w;
        else if(type==WIDGET_CONTAINER) wh->draw=draw_container;
        else wh->draw=0;
        if(type==WIDGET_SHELL || type==WIDGET_CANVAS){
            display_create(i, x+2,y+2,x+w-2,y+h-2);
        }
        return wh;
    }
    return 0;
}
void widget_destroy(widget_t *w){
    if(!w) return;
    for(int i=0;i<w->child_count;i++) if(w->children[i]) widget_destroy(w->children[i]);
    if(w->parent){
        widget_remove_child(w->parent, w);
    }
    if(w->id>=0 && w->id<WIDGET_MAX) pool_used[w->id]=0;
    if(focused==w) focused=0;
}
void widget_add_child(widget_t *parent, widget_t *child){
    if(!parent||!child||parent->child_count>=WIDGET_CHILDREN) return;
    parent->children[parent->child_count++]=child;
    child->parent=parent;
}
void widget_remove_child(widget_t *parent, widget_t *child){
    if(!parent||!child) return;
    for(int i=0;i<parent->child_count;i++) if(parent->children[i]==child){
        for(int j=i;j<parent->child_count-1;j++) parent->children[j]=parent->children[j+1];
        parent->child_count--;
        child->parent=0;
        break;
    }
}
void widget_set_focus(widget_t *w){
    if(focused) focused->focused=0;
    focused=w;
    if(w) w->focused=1;
}
widget_t *widget_focused(void){ return focused; }
void widget_layout(widget_t *root){
    if(!root||!root->visible) return;
    if(root->type==WIDGET_CONTAINER && root->child_count==2){
        widget_t *a=root->children[0], *b=root->children[1];
        if(!a||!b) return;
        if(root->split==SPLIT_VERTICAL){
            int lw=root->w * root->ratio /256;
            if(lw<80) lw=80;
            if(lw>root->w-80) lw=root->w-80;
            a->x=root->x; a->y=root->y; a->w=lw; a->h=root->h;
            b->x=root->x+lw; b->y=root->y; b->w=root->w-lw; b->h=root->h;
        }else if(root->split==SPLIT_HORIZONTAL){
            int th=root->h * root->ratio /256;
            if(th<40) th=40;
            if(th>root->h-40) th=root->h-40;
            a->x=root->x; a->y=root->y; a->w=root->w; a->h=th;
            b->x=root->x; b->y=root->y+th; b->w=root->w; b->h=root->h-th;
        }
        for(int i=0;i<2;i++){
            widget_t *c=root->children[i];
            if(c->type==WIDGET_SHELL || c->type==WIDGET_CANVAS){
                int prev=display_current();
                display_select(c->id);
                display_set_region(c->x+2,c->y+2,c->x+c->w-2,c->y+c->h-2);
                display_select(prev);
                if(c->type==WIDGET_CANVAS){
                    gpu_set_view(c->x+2,c->y+2,c->w-4,c->h-4);
                }
            }
            widget_layout(c);
        }
    }else{
        for(int i=0;i<root->child_count;i++) widget_layout(root->children[i]);
    }
}
void widget_draw_tree(widget_t *root){
    if(!root||!root->visible) return;
    if(root->draw) root->draw(root);
    if(root->type==WIDGET_CONTAINER){
        for(int i=0;i<root->child_count;i++) widget_draw_tree(root->children[i]);
    }
}
int widget_dispatch(event_t *ev){
    if(!ev) return 0;
    if(ev->type==EVT_SPLIT_V || ev->type==EVT_SPLIT_H){
        widget_t *f=widget_focused();
        if(!f) return 0;
        widget_t *parent=f->parent;
        if(!parent) return 0;
        widget_t *cont=widget_create(WIDGET_CONTAINER, f->x,f->y,f->w,f->h);
        if(!cont) return 0;
        cont->split = (ev->type==EVT_SPLIT_V)?SPLIT_VERTICAL:SPLIT_HORIZONTAL;
        cont->ratio=128;
        widget_t *neww = widget_create(WIDGET_SHELL, 0,0,10,10);
        if(!neww) return 0;
        for(int i=0;i<parent->child_count;i++) if(parent->children[i]==f){
            parent->children[i]=cont;
            cont->parent=parent;
            break;
        }
        widget_add_child(cont, f);
        widget_add_child(cont, neww);
        widget_layout(parent);
        widget_set_focus(neww);
        return 1;
    }
    if(focused && focused->on_event) return focused->on_event(focused, ev);
    return 0;
}
void widget_set_canvas_id(widget_t *w,int cid){ if(w) w->canvas_id=cid; }
widget_t *widget_find_by_canvas(int cid){
    for(int i=0;i<WIDGET_MAX;i++) if(pool_used[i] && pool[i].type==WIDGET_CANVAS && pool[i].canvas_id==cid) return &pool[i];
    return 0;
}
widget_t *widget_find_focused_shell(void){
    widget_t *f=widget_focused();
    if(f && f->type==WIDGET_SHELL) return f;
    return 0;
}
