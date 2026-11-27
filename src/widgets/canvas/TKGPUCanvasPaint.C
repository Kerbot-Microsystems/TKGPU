#include "TKGPUCanvasItems.H"
#include "../TKGPUWidgetInternal.H"
#include "TKGPURaster.H"
#include "TKGPUText.H"
#include "../image/TKGPUImageRegistry.H"
#include "../../render/TKGPUFrameBufferInternal.H"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static int min(int a,int b) { return a<b?a:b; }
static int max(int a,int b) { return a>b?a:b; }
static int angle_in_arc(double ang,double start,double extent) {
    if(extent>=360.0||extent<=-360.0) return 1;
    if(extent==0.0) return 0;
    double diff=fmod(ang-start,360.0);
    if(extent>0.0) {
        if(diff<0.0) diff+=360.0;
        return diff<=extent;
    } else {
        if(diff>0.0) diff-=360.0;
        return diff>=extent;
    }
}
static void paint_arc_ring(tg_canvas *c,tg_rect clip,tg_rect q,int width,uint32_t rgb,int is_arc,double start,double extent,int filled) {
    if(q.w<=0||q.h<=0) return;
    double cx=q.x+q.w*0.5, cy=q.y+q.h*0.5;
    double rx=q.w*0.5, ry=q.h*0.5;
    double half_w=filled?0.0:(width>1?width*0.5:0.75);
    double rx_out=rx+half_w, ry_out=ry+half_w;
    double rx_in=filled?0.0:(rx-half_w), ry_in=filled?0.0:(ry-half_w);
    if(rx_in<0.0) rx_in=0.0;
    if(ry_in<0.0) ry_in=0.0;
    int pad=(int)(half_w+2.0);
    tg_rect bounds={q.x-pad,q.y-pad,q.w+2*pad,q.h+2*pad}, vis;
    if(!tg_rect_intersect(bounds,clip,&vis)||!tg_rect_intersect(vis,tg_canvas_render_bounds(c),&vis)) return;
    const double rad2deg=57.295779513082320876;
    for(int py=vis.y;py<vis.y+vis.h;py++) {
        double dy=(py+0.5)-cy;
        int run_start=-1;
        for(int px=vis.x;px<vis.x+vis.w;px++) {
            double dx=(px+0.5)-cx;
            double d_out=(dx*dx)/(rx_out*rx_out)+(dy*dy)/(ry_out*ry_out);
            int hit=0;
            if(d_out<=1.0) {
                int inside_in=(rx_in>0.0&&ry_in>0.0)&&(((dx*dx)/(rx_in*rx_in)+(dy*dy)/(ry_in*ry_in))<1.0);
                if(!inside_in) {
                    if(!is_arc) hit=1;
                    else {
                        double ang=atan2(-dy,dx)*rad2deg;
                        hit=angle_in_arc(ang,start,extent);
                    }
                }
            }
            if(hit) {
                if(run_start<0) run_start=px;
            } else if(run_start>=0) {
                tg_raster_fill(c,clip,(tg_rect){run_start,py,px-run_start,1},rgb);
                run_start=-1;
            }
        }
        if(run_start>=0) tg_raster_fill(c,clip,(tg_rect){run_start,py,vis.x+vis.w-run_start,1},rgb);
    }
}
/* Rasterize a thick segment as clipped scanline spans. Round caps/joins
   use a circle stamp; the opacity layer composites overlapping spans once. */
static void disk(tg_canvas *c,tg_rect clip,int x,int y,int width,uint32_t rgb) {
    tg_raster_ellipse(c,clip,(tg_rect){x-width/2,y-width/2,width,width},rgb,1);
}
static void thick_segment(tg_canvas *c,tg_rect clip,int x0,int y0,int x1,int y1,int width,uint32_t rgb) {
    if(width<1) width=1;
    if(y0==y1) {
        int l=min(x0,x1),r=max(x0,x1);
        tg_raster_fill(c,clip,(tg_rect){l,y0-width/2,r-l+1,width},rgb);
        return;
    }
    if(x0==x1) {
        int t=min(y0,y1),b=max(y0,y1);
        tg_raster_fill(c,clip,(tg_rect){x0-width/2,t,width,b-t+1},rgb);
        return;
    }
    if(width<=1) { tg_raster_line(c,clip,x0,y0,x1,y1,rgb); return; }
    int dx=x1-x0,dy=y1-y0;
    if(!dx&&!dy) { disk(c,clip,x0,y0,width,rgb); return; }
    /* Intersect the strip and its endpoint planes with each scanline.
       A binary-sized interval avoids per-pixel work for long diagonals. */
    double norm2=(double)dx*dx+(double)dy*dy;
    double half=width*0.5;
    double radius=half;
    /* sqrt without a new math-library dependency. */
    double len=max(abs(dx),abs(dy));
    for(int k=0;k<8;k++) len=(len+norm2/len)*0.5;
    double cross=radius*len;
    tg_rect bounds={min(x0,x1)-width,min(y0,y1)-width,abs(dx)+2*width+1,abs(dy)+2*width+1},q;
    if(!tg_rect_intersect(bounds,clip,&q)||!tg_rect_intersect(q,tg_canvas_render_bounds(c),&q)) return;
    for(int y=q.y;y<q.y+q.h;y++) {
        double yy=(double)y-y0,lo=q.x-x0,hi=q.x+q.w-1-x0;
        if(dx) {
            double a=-yy*dy/dx,b=(norm2-yy*dy)/dx;
            if(a>b) { double tmp=a;a=b;b=tmp; }
            if(a>lo) lo=a; if(b<hi) hi=b;
        } else if(yy*dy<0||yy*dy>norm2) continue;
        if(dy) {
            double a=(yy*dx-cross)/dy,b=(yy*dx+cross)/dy;
            if(a>b) { double tmp=a;a=b;b=tmp; }
            if(a>lo) lo=a; if(b<hi) hi=b;
        } else if(yy*dx< -cross||yy*dx>cross) continue;
        if(lo>hi) continue;
        int l=(int)lo,r=(int)hi;
        if(l<lo) l++; if(r>hi) r--;
        if(l<=r) tg_raster_fill(c,clip,(tg_rect){x0+l,y,r-l+1,1},rgb);
    }
}
static unsigned eval_smooth_points(const int *in_pts,unsigned in_count,int closed,int steps,int *out_pts,unsigned max_out) {
    if(in_count<3) {
        unsigned n=in_count<max_out?in_count:max_out;
        memcpy(out_pts,in_pts,n*2*sizeof(int));
        return n;
    }
    if(steps<2) steps=8;
    if(steps>12) steps=12;
    unsigned out=0;
    if(!closed) {
        out_pts[out*2]=in_pts[0]; out_pts[out*2+1]=in_pts[1]; out++;
        for(unsigned i=1;i+1<in_count;i++) {
            double x0=(i==1)?(double)in_pts[0]:0.5*(in_pts[(i-1)*2]+in_pts[i*2]);
            double y0=(i==1)?(double)in_pts[1]:0.5*(in_pts[(i-1)*2+1]+in_pts[i*2+1]);
            double cx=(double)in_pts[i*2], cy=(double)in_pts[i*2+1];
            double x1=(i+2==in_count)?(double)in_pts[(i+1)*2]:0.5*(in_pts[i*2]+in_pts[(i+1)*2]);
            double y1=(i+2==in_count)?(double)in_pts[(i+1)*2+1]:0.5*(in_pts[i*2+1]+in_pts[(i+1)*2+1]);
            for(int s=1;s<=steps&&out<max_out;s++) {
                double t=(double)s/(double)steps, u=1.0-t;
                int px=(int)(u*u*x0+2.0*u*t*cx+t*t*x1+0.5);
                int py=(int)(u*u*y0+2.0*u*t*cy+t*t*y1+0.5);
                if(px!=out_pts[(out-1)*2]||py!=out_pts[(out-1)*2+1]) {
                    out_pts[out*2]=px; out_pts[out*2+1]=py; out++;
                }
            }
        }
    } else {
        for(unsigned i=0;i<in_count;i++) {
            unsigned prev=(i+in_count-1)%in_count, next=(i+1)%in_count;
            double x0=0.5*(in_pts[prev*2]+in_pts[i*2]);
            double y0=0.5*(in_pts[prev*2+1]+in_pts[i*2+1]);
            double cx=(double)in_pts[i*2], cy=(double)in_pts[i*2+1];
            double x1=0.5*(in_pts[i*2]+in_pts[next*2]);
            double y1=0.5*(in_pts[i*2+1]+in_pts[next*2+1]);
            int colinear=((in_pts[prev*2]==in_pts[i*2]&&in_pts[i*2]==in_pts[next*2])||
                          (in_pts[prev*2+1]==in_pts[i*2+1]&&in_pts[i*2+1]==in_pts[next*2+1]));
            int segs=colinear?1:steps;
            for(int s=1;s<=segs&&out<max_out;s++) {
                double t=(double)s/(double)segs, u=1.0-t;
                int px=(int)(u*u*x0+2.0*u*t*cx+t*t*x1+0.5);
                int py=(int)(u*u*y0+2.0*u*t*cy+t*t*y1+0.5);
                if(!out||px!=out_pts[(out-1)*2]||py!=out_pts[(out-1)*2+1]) {
                    out_pts[out*2]=px; out_pts[out*2+1]=py; out++;
                }
            }
        }
    }
    return out;
}
static void paint_filled_polygon(tg_canvas *c,tg_rect clip,const int *pts,unsigned count,int ox,int oy,uint32_t rgb) {
    if(count<3) return;
    int min_y=oy+pts[1], max_y=min_y;
    for(unsigned j=1;j<count;j++) {
        int py=oy+pts[j*2+1];
        if(py<min_y) min_y=py;
        if(py>max_y) max_y=py;
    }
    tg_rect rbounds=tg_canvas_render_bounds(c), vis;
    if(!tg_rect_intersect(clip,rbounds,&vis)) return;
    int y0=max(min_y,vis.y), y1=min(max_y,vis.y+vis.h-1);
    int nodes[256];
    for(int y=y0;y<=y1;y++) {
        double sy=y+0.5;
        int n=0;
        for(unsigned j=0,k=count-1;j<count;k=j++) {
            double yi=oy+pts[j*2+1], yk=oy+pts[k*2+1];
            if((yi<sy&&yk>=sy)||(yk<sy&&yi>=sy)) {
                double xi=ox+pts[j*2], xk=ox+pts[k*2];
                double ix=xi+(sy-yi)/(yk-yi)*(xk-xi);
                if(n<256) nodes[n++]=(int)(ix+0.5);
            }
        }
        for(int a=1;a<n;a++) {
            int key=nodes[a], b=a-1;
            while(b>=0&&nodes[b]>key) { nodes[b+1]=nodes[b]; b--; }
            nodes[b+1]=key;
        }
        for(int a=0;a+1<n;a+=2) {
            int l=max(nodes[a],vis.x), r=min(nodes[a+1],vis.x+vis.w-1);
            if(l<=r) tg_raster_fill(c,clip,(tg_rect){l,y,r-l+1,1},rgb);
        }
    }
}
int tg_canvas_item_bounds(const tg_ui *u,const tg_canvas_item *p,tg_rect *out) {
    int l=min(p->x0,p->x1),t=min(p->y0,p->y1);
    tg_rect q={l,t,max(p->x0,p->x1)-l,max(p->y0,p->y1)-t};
    if(p->kind==TG_ITEM_RECT||p->kind==TG_ITEM_OVAL) {
        int stroke=p->has_outline?(p->width?(int)p->width:1):1;
        if(q.w<=0&&(p->has_outline||p->has_fill)) q.w=stroke;
        if(q.h<=0&&(p->has_outline||p->has_fill)) q.h=stroke;
    }
    if(p->kind==TG_ITEM_IMAGE) {
        const tg_photo *photo=tg_image_lookup(u,p->image_id);
        if(!photo) return 0;
        q=(tg_rect){p->x0,p->y0,tg_photo_width(photo),tg_photo_height(photo)};
        unsigned anchor=p->text_anchor?p->text_anchor-1:(p->anchor_nw?0:4);
        q.x-=q.w*(int)(anchor%3)/2; q.y-=q.h*(int)(anchor/3)/2;
    } else if(p->kind==TG_ITEM_WINDOW) {
        const tg_widget *child=tg_ui_get(u,p->window_id);
        if(!child||child->layout!=TG_FREE) return 0;
        int width=p->window_width?p->window_width:child->request.w;
        int height=p->window_height?p->window_height:child->request.h;
        unsigned anchor=p->text_anchor?p->text_anchor-1:4;
        q=(tg_rect){p->x0-width*(int)(anchor%3)/2,p->y0-height*(int)(anchor/3)/2,width,height};
    } else if(p->kind==TG_ITEM_TEXT) {
        int width,height;
        if((tg_text_native_enabled()?tg_text_font_measure(p->text,tg_ui_font_spec(u,p->font),&width,&height):tg_text_measure(p->text,(int)p->text_scale,&width,&height))!=TG_OK) return 0;
        unsigned anchor=p->text_anchor?p->text_anchor-1:4;
        q=(tg_rect){p->x0-width*(int)(anchor%3)/2,p->y0-height*(int)(anchor/3)/2,width,height};
    } else if(p->kind==TG_ITEM_LINE||p->kind==TG_ITEM_POLYGON) {
        int r=max(p->x0,p->x1),b=max(p->y0,p->y1);
        for(unsigned j=0;j<p->point_count;j++) {
            l=min(l,p->points[j*2]); t=min(t,p->points[j*2+1]);
            r=max(r,p->points[j*2]); b=max(b,p->points[j*2+1]);
        }
        int pad=(int)p->width;
        q=(tg_rect){l-pad,t-pad,r-l+2*pad+1,b-t+2*pad+1};
    }
    *out=q; return q.w>0&&q.h>0;
}
int tg_canvas_items_paint(const tg_ui *u,const tg_widget *w,tg_canvas *c) {
    if(!u->drawing||w->rect.w<=0||w->rect.h<=0) return TG_OK;
    tg_rect band_clip;
    if(!tg_rect_intersect(w->rect,tg_canvas_render_bounds(c),&band_clip)) return TG_OK;
    int band_y0=band_clip.y, band_y1=band_clip.y+band_clip.h;
    int band_x0=band_clip.x, band_x1=band_clip.x+band_clip.w;
    int ox=w->rect.x-w->canvas_x, oy=w->rect.y-w->canvas_y;
    uint32_t *under=NULL; size_t capacity=0;
    for(size_t k=0;k<u->drawing->count;k++) {
        const tg_canvas_item *p=&u->drawing->item[k];
        unsigned alpha=p->opacity_set?p->opacity:255;
        if(p->owner!=w->id||!alpha||p->kind==TG_ITEM_WINDOW||p->hidden) continue;
        int x=ox,y=oy;
        /* Fast band rejection before expensive text measurement or polygon bounds. */
        if(p->kind==TG_ITEM_RECT||p->kind==TG_ITEM_OVAL||(p->kind==TG_ITEM_LINE&&p->point_count==2)) {
            int iy0=min(p->y0,p->y1)+y, iy1=max(p->y0,p->y1)+y;
            int ix0=min(p->x0,p->x1)+x, ix1=max(p->x0,p->x1)+x;
            int pad=(int)p->width+2;
            if(iy1+pad<band_y0||iy0-pad>=band_y1||ix1+pad<band_x0||ix0-pad>=band_x1) continue;
        } else if(p->kind==TG_ITEM_TEXT) {
            int ty=p->y0+y;
            if(ty+96<band_y0||ty-96>=band_y1) continue;
        }
        tg_rect q;
        if(!tg_canvas_item_bounds(u,p,&q)) continue;
        q.x+=x; q.y+=y;
        tg_rect visible, cull=q;
        if((p->kind==TG_ITEM_OVAL||p->kind==TG_ITEM_RECT)&&p->has_outline) {
            /* Thick outlines are centred on the bbox edge; include the half
               stroke so bands touching only the stroke still repaint it. */
            int pad=(int)(p->width/2)+2;
            cull=(tg_rect){q.x-pad,q.y-pad,q.w+2*pad,q.h+2*pad};
        }
        if(!tg_rect_intersect(cull,band_clip,&visible)) continue;
        const tg_photo *photo=p->kind==TG_ITEM_IMAGE?tg_image_lookup(u,p->image_id):NULL;
        if(alpha<255&&c->sink) {
            if(!c->layer_sink||c->layer_sink(c->sink_context,visible,alpha,1)!=TG_OK) { free(under); return TG_ERROR; }
        } else if(alpha<255) {
            size_t count=(size_t)visible.w*visible.h;
            if(count>SIZE_MAX/sizeof *under) { free(under); return TG_ERROR; }
            if(count>capacity) {
                uint32_t *next=realloc(under,count*sizeof *under);
                if(!next) { free(under); return TG_ERROR; }
                under=next; capacity=count;
            }
            for(int row=0;row<visible.h;row++)
                memcpy(under+(size_t)row*visible.w,c->p+(size_t)(visible.y+row)*c->w+visible.x,(size_t)visible.w*sizeof *under);
        }
        if(p->kind==TG_ITEM_IMAGE) {
            c->image_id=p->image_id; c->image_revision=tg_photo_revision(photo);
            int rc=tg_photo_paint(photo,c,w->rect,q.x,q.y);
            c->image_id=0; c->image_revision=0;
            if(rc!=TG_OK) { free(under); return rc; }
        }
        else if(p->kind==TG_ITEM_TEXT) {
            if(!tg_text_native_enabled()||tg_text_font_draw(c,w->rect,q.x,q.y,p->text,tg_ui_font_spec(u,p->font),p->fill)!=TG_OK) tg_text_draw(c,w->rect,q.x,q.y,p->text,(int)p->text_scale,p->fill);
        }
        else if(p->kind==TG_ITEM_POLYGON) {
            int smooth_pts[2*512];
            const int *pts=p->points; unsigned pcnt=p->point_count;
            if(p->smooth&&pcnt>=3) {
                pcnt=eval_smooth_points(p->points,p->point_count,1,p->splinesteps?p->splinesteps:8,smooth_pts,512);
                pts=smooth_pts;
            }
            if(p->has_fill&&pcnt>=3) {
                paint_filled_polygon(c,w->rect,pts,pcnt,x,y,p->fill);
            }
            if(p->has_outline&&pcnt>=2) {
                int width=p->width?(int)p->width:1;
                for(unsigned j=0;j<pcnt;j++) {
                    unsigned next=(j+1)%pcnt;
                    int ax=x+pts[j*2],ay=y+pts[j*2+1];
                    int bx=x+pts[next*2],by=y+pts[next*2+1];
                    thick_segment(c,w->rect,ax,ay,bx,by,width,p->outline);
                    if(width>1&&!p->smooth) disk(c,w->rect,ax,ay,width,p->outline);
                }
            }
        } else if(p->kind==TG_ITEM_LINE) {
            if(p->has_fill) {
                int smooth_pts[2*512];
                const int *pts=p->points; unsigned pcnt=p->point_count;
                if(p->smooth&&pcnt>=3) {
                    pcnt=eval_smooth_points(p->points,p->point_count,0,p->splinesteps?p->splinesteps:8,smooth_pts,512);
                    pts=smooth_pts;
                }
                int width=p->width?(int)p->width:1;
                for(unsigned j=1;j<pcnt;j++) {
                    int ax=x+pts[(j-1)*2],ay=y+pts[(j-1)*2+1];
                    int bx=x+pts[j*2],by=y+pts[j*2+1];
                    thick_segment(c,w->rect,ax,ay,bx,by,width,p->fill);
                    if(width>1&&((j==1&&p->round_cap)||(j>1&&p->round_join&&!p->smooth))) disk(c,w->rect,ax,ay,width,p->fill);
                    if(width>1&&j+1==pcnt&&p->round_cap) disk(c,w->rect,bx,by,width,p->fill);
                }
            }
        } else if(p->kind==TG_ITEM_RECT) {
            if(p->has_fill) tg_raster_fill(c,w->rect,q,p->fill);
            if(p->has_outline) {
                unsigned rw=p->width?p->width:1;
                if(q.w<=2||q.h<=2) tg_raster_fill(c,w->rect,q,p->outline);
                else if(p->dash_on&&p->dash_off) {
                    int on=(int)p->dash_on,step=(int)p->dash_on+(int)p->dash_off,th=(int)rw;
                    if(th>q.w/2) th=q.w/2; if(th>q.h/2) th=q.h/2;
                    if(th<1) th=1;
                    for(int dx=0;dx<q.w;dx+=step) {
                        int seg=dx+on<=q.w?on:q.w-dx;
                        tg_raster_fill(c,w->rect,(tg_rect){q.x+dx,q.y,seg,th},p->outline);
                        tg_raster_fill(c,w->rect,(tg_rect){q.x+dx,q.y+q.h-th,seg,th},p->outline);
                    }
                    for(int dy=0;dy<q.h;dy+=step) {
                        int seg=dy+on<=q.h?on:q.h-dy;
                        tg_raster_fill(c,w->rect,(tg_rect){q.x,q.y+dy,th,seg},p->outline);
                        tg_raster_fill(c,w->rect,(tg_rect){q.x+q.w-th,q.y+dy,th,seg},p->outline);
                    }
                } else for(unsigned j=0;j<rw&&q.w>2*(int)j&&q.h>2*(int)j;j++) tg_raster_outline(c,w->rect,(tg_rect){q.x+(int)j,q.y+(int)j,q.w-2*(int)j,q.h-2*(int)j},p->outline);
            }
        } else {
            if(p->is_arc) {
                if(p->has_fill&&p->arc_style!=2) paint_arc_ring(c,w->rect,q,0,p->fill,1,p->arc_start,p->arc_extent,1);
                if(p->has_outline) paint_arc_ring(c,w->rect,q,p->width?(int)p->width:1,p->outline,1,p->arc_start,p->arc_extent,0);
            } else {
                if(p->has_fill) tg_raster_ellipse(c,w->rect,q,p->fill,1);
                if(p->has_outline) {
                    if(p->width>1) paint_arc_ring(c,w->rect,q,(int)p->width,p->outline,0,0.0,360.0,0);
                    else tg_raster_ellipse(c,w->rect,q,p->outline,0);
                }
            }
        }
        if(alpha<255&&c->sink) {
            if(!c->layer_sink||c->sink_failed||c->layer_sink(c->sink_context,visible,alpha,0)!=TG_OK) { free(under); return TG_ERROR; }
        } else if(alpha<255) {
            for(int row=0;row<visible.h;row++) {
                uint32_t *dst=c->p+(size_t)(visible.y+row)*c->w+visible.x;
                const uint32_t *src=under+(size_t)row*visible.w;
                for(int col=0;col<visible.w;col++)
                    dst[col]=tg_pixel_blend(src[col],((dst[col]&0xffffffu)<<8)|alpha);
            }
        }
    }
    free(under); return TG_OK;
}
