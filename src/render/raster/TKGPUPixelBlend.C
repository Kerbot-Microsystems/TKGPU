#include "TKGPURaster.H"
/* Straight alpha over opaque RGB, rounded to the nearest 8-bit channel. */
uint32_t tg_pixel_blend(uint32_t rgb,uint32_t rgba) {
    unsigned a=rgba&255u,inv=255u-a;
    unsigned r=(((rgba>>24)&255u)*a+((rgb>>16)&255u)*inv+127u)/255u;
    unsigned g=(((rgba>>16)&255u)*a+((rgb>>8)&255u)*inv+127u)/255u;
    unsigned b=(((rgba>>8)&255u)*a+(rgb&255u)*inv+127u)/255u;
    return (r<<16)|(g<<8)|b;
}
