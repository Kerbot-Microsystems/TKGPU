#ifndef TKGPU_TEXT_WIDGET_H
#define TKGPU_TEXT_WIDGET_H
#include "TKGPUUi.H"
typedef struct tg_text_document tg_text_document;
tg_text_document *tg_document_create(void);
tg_text_document *tg_document_clone(const tg_text_document *);
void tg_document_destroy(tg_text_document *);
int tg_document_history_option(tg_text_document *,const char *,const char *);
int tg_document_history_query(const tg_text_document *,const char *,char *,size_t);
int tg_document_command(tg_widget *,size_t,char **,char *,size_t);
void tg_document_view_fractions(const tg_widget *,double *,double *);
void tg_document_xview_fractions(const tg_widget *,double *,double *);
int tg_document_click(tg_widget *,int,int);
int tg_document_drag(tg_widget *,int,int);
int tg_document_caret_rect(const tg_widget *,tg_rect *);
const char *tg_document_input_text(const tg_widget *,size_t *,size_t *);
int tg_document_replace_bytes(tg_widget *,size_t,size_t,const char *);
int tg_document_key(tg_widget *,uint32_t,int);
void tg_document_paint(const tg_widget *,tg_canvas *,uint32_t,int);
#endif
