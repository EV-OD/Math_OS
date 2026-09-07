#include <event.h>
#include <keyboard.h>

int event_is_prefix(KeyOutput *k, char ser, int has_ser){
    if(has_ser && ser==2) return 1;
    if(k && k->pressed && k->is_character){
        const KeyboardState *ks=keyboard_get_state();
        if((ks->lctrl||ks->rctrl) && (k->ascii=='b'||k->ascii=='B')) return 1;
    }
    return 0;
}
int event_from_key(event_t *out, KeyOutput *k, char ser, int has_ser){
    if(!out) return 0;
    out->type=EVT_NONE;
    out->has_key = k?1:0;
    out->has_serial = has_ser;
    if(k) out->key=*k;
    out->serial=ser;
    if(k && !k->pressed) return 0;
    if(k && k->is_character){
        if(k->ascii=='\n'||k->ascii=='\r'){ out->type=EVT_KEY; return 1; }
    }
    if(has_ser){
        if(ser=='\n'||ser=='\r'){ out->type=EVT_KEY; return 1; }
    }
    out->type=EVT_KEY;
    return 1;
}
