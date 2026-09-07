#include <widget.h>
#include <display.h>
#include <string.h>
#include <stdio.h>

widget_style_t style_shell = {0x0B1020, 0xE8ECF5, 0x2A2E4A, 0x2BD97C, 2, 4};
widget_style_t style_canvas = {0x0B1020, 0xE8ECF5, 0x2A2E4A, 0x2BD97C, 2, 4};
widget_style_t style_container = {0x0B1020, 0xE8ECF5, 0, 0, 0, 0};
widget_style_t style_status = {0x11162B, 0x8A93B2, 0x2BD97C, 0, 2, 0};

static widget_t pool[WIDGET_MAX];
static int pool_used[WIDGET_MAX];
static widget_t *focused = 0;

void widget_system_init(uint32_t *fb, uint32_t pitch, uint32_t sw, uint32_t sh){
    (void)fb; (void)pitch; (void)sw; (void)sh;
    memset(pool,0,sizeof(pool));
    memset(pool_used,0,sizeof(pool_used));
    focused=0;
}
widget_t *widget_create(widget_type_t type,int x,int y,int w,int h, const char *label){
    for(int i=0;i<WIDGET_MAX;i++) if(!pool_used[i]){
        pool_used[i]=1;
        widget_t *wh=&pool[i];
        memset(wh,0,sizeof(*wh));
        wh->id=i;
        wh->type=type;
        wh->x=x; wh->y=y; wh->w=w; wh->h=h;
        wh->visible=1;
        wh->split=SPLIT_NONE;
        wh->ratio=128;
        if(label) strncpy(wh->label,label,31);
        if(type==WIDGET_SHELL) wh->style=style_shell;
        else if(type==WIDGET_CANVAS) wh->style=style_canvas;
        else if(type==WIDGET_CONTAINER) wh->style=style_container;
        else wh->style=style_status;
        if(type==WIDGET_SHELL){
            display_create(i, x+2,y+20,x+w-2,y+h-2);
        }
        return wh;
    }
    return 0;
}
void widget_destroy(widget_t *w){
    if(!w) return;
    for(int i=0;i<w->child_count;i++) if(w->children[i]) widget_destroy(w->children[i]);
    if(w->parent) widget_remove_child(w->parent, w);
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
void widget_set_bounds(widget_t *w,int x,int y,int w_,int h_){
    if(!w) return;
    w->x=x; w->y=y; w->w=w_; w->h=h_;
    if(w->type==WIDGET_SHELL){
        int prev=display_current();
        display_select(w->id);
        display_set_region(x+2,y+20,x+w_-2,y+h_-2);
        display_select(prev);
    }else if(w->type==WIDGET_CANVAS){
        int prev=display_current();
        display_select(w->id);
        display_set_region(x+2,y+2,x+w_-2,y+h_-2);
        display_select(prev);
    }
}
void widget_layout(widget_t *root){
    if(!root||!root->visible) return;
    if(root->type==WIDGET_CONTAINER && root->child_count==2){
        widget_t *a=root->children[0], *b=root->children[1];
        if(!a||!b) return;
        if(root->split==SPLIT_VERTICAL){
            int lw=root->w * root->ratio /256;
            if(lw<80) lw=80;
            if(lw>root->w-80) lw=root->w-80;
            widget_set_bounds(a, root->x, root->y, lw, root->h);
            widget_set_bounds(b, root->x+lw, root->y, root->w-lw, root->h);
        }else if(root->split==SPLIT_HORIZONTAL){
            int th=root->h * root->ratio /256;
            if(th<40) th=40;
            if(th>root->h-40) th=root->h-40;
            widget_set_bounds(a, root->x, root->y, root->w, th);
            widget_set_bounds(b, root->x, root->y+th, root->w, root->h-th);
        }
        widget_layout(a);
        widget_layout(b);
    }else{
        for(int i=0;i<root->child_count;i++) widget_layout(root->children[i]);
    }
}
void widget_draw_tree(widget_t *root){
    (void)root;
}
int widget_dispatch(event_t *ev){
    (void)ev;
    return 0;
}
void widget_invalidate(void){}
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
int widget_canvas_count(void){
    int n=0;
    for(int i=0;i<WIDGET_MAX;i++) if(pool_used[i] && pool[i].type==WIDGET_CANVAS) n++;
    return n;
}
