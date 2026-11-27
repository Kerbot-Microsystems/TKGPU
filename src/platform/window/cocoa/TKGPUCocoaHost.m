/* Experimental AppKit host. All Cocoa operations are main-thread only.
   The common C scene remains authoritative; no system Tcl/Tk dependency. */
#import <Cocoa/Cocoa.h>
#include "../../../compat/tk/TKGPUWindow.H"
#include "TKGPUUtf8.H"
#include "../../../widgets/controls/TKGPUControls.H"
#include "../../../widgets/text/TKGPUTextWidget.H"
#include "../../../core/TKGPUCoreInternal.H"
#include <stdlib.h>
#include <string.h>

@interface TGPreviewView : NSView <NSTextInputClient> {
@public tg_window *host;
    NSTrackingArea *tracking;
    NSAttributedString *markedText;
    NSString *compositionText;
    NSRange markedSelection,markedBase;
    tg_id compositionTarget;
}
@end
@interface TGPreviewDelegate : NSObject <NSWindowDelegate> {
@public tg_window *host;
}
@end
typedef struct { NSWindow *window; TGPreviewView *view; TGPreviewDelegate *delegate;
    int width,height; } native;

static uint32_t modifiers(NSEvent *e) {
    NSEventModifierFlags f=e.modifierFlags;
    return (f&NSEventModifierFlagShift?TG_MOD_SHIFT:0)|
           (f&(NSEventModifierFlagControl|NSEventModifierFlagCommand)?TG_MOD_CONTROL:0)|
           (f&NSEventModifierFlagOption?TG_MOD_ALT:0);
}
static uint32_t special(NSEvent *e) {
    switch(e.keyCode) {
    case 123:return TG_KEY_LEFT; case 124:return TG_KEY_RIGHT;
    case 125:return TG_KEY_ARROW_DOWN; case 126:return TG_KEY_ARROW_UP;
    case 115:return TG_KEY_HOME; case 119:return TG_KEY_END;
    case 51:return TG_KEY_BACKSPACE; case 117:return TG_KEY_DELETE;
    case 36:case 76:return TG_KEY_RETURN; case 53:return TG_KEY_ESCAPE;
    case 48:return TG_KEY_TAB; case 116:return TG_KEY_PRIOR; case 121:return TG_KEY_NEXT;
    default:return 0;
    }
}
static const tg_widget *input_widget(tg_window *host) {
    if(!host||!host->alive) return NULL;
    tg_ui *u=tg_interp_ui(host->interp);
    const tg_widget *w=tg_ui_get(u,tg_ui_focused(u)),*p=w;
    if(!w||w->state==TG_STATE_DISABLED||
       (w->kind!=TG_TEXT&&!tg_textvar_editable(w->kind))) return NULL;
    while(p&&p->id!=host->root&&p->kind!=TG_TOPLEVEL) p=tg_ui_get(u,p->parent);
    return p&&p->id==host->root?w:NULL;
}
static NSRect input_rect(const tg_widget *w) {
    tg_rect r={0,0,1,20};
    if(w) {
        r=w->rect;
        int rc=w->kind==TG_TEXT?tg_document_caret_rect(w,&r):tg_entry_caret_rect(w,&r);
        if(rc!=TG_OK) r=w->rect;
    }
    return NSMakeRect(r.x,r.y,r.w,r.h);
}
static NSString *input_string(const tg_widget *w,NSRange *selection) {
    *selection=NSMakeRange(NSNotFound,0);
    if(!w||w->show[0]) return nil; /* Never expose a password to the input client. */
    size_t length,cursor,first,last; const char *bytes;
    if(w->kind==TG_TEXT) {
        bytes=tg_document_input_text(w,&length,&cursor); first=last=cursor;
    } else {
        bytes=w->text; length=strlen(bytes);
        first=tg_entry_offset(w,w->sel_first<w->sel_last?w->sel_first:w->cursor);
        last=tg_entry_offset(w,w->sel_first<w->sel_last?w->sel_last:w->cursor);
    }
    if(!bytes||first>last||last>length) return nil;
    NSString *s=[[[NSString alloc] initWithBytes:bytes length:length encoding:NSUTF8StringEncoding] autorelease];
    NSString *prefix=[[[NSString alloc] initWithBytes:bytes length:first encoding:NSUTF8StringEncoding] autorelease];
    NSString *selected=[[[NSString alloc] initWithBytes:bytes+first length:last-first encoding:NSUTF8StringEncoding] autorelease];
    if(s&&prefix&&selected) *selection=NSMakeRange(prefix.length,selected.length);
    return s;
}
/* Validate UTF-16 boundaries before NSString range operations; a surrogate
   pair is one scalar in the C editor. Combining marks may be replaced alone. */
static BOOL scalar_boundary(NSString *s,NSUInteger at) {
    if(at>s.length) return NO;
    if(at==0||at==s.length) return YES;
    unichar a=[s characterAtIndex:at-1],b=[s characterAtIndex:at];
    return !(a>=0xd800&&a<=0xdbff&&b>=0xdc00&&b<=0xdfff);
}
static BOOL valid_range(NSString *s,NSRange r) {
    return s&&r.location<=s.length&&r.length<=s.length-r.location&&
        scalar_boundary(s,r.location)&&scalar_boundary(s,NSMaxRange(r));
}
static BOOL queue_replacement(tg_window *host,const tg_widget *w,NSString *old,NSRange r,NSString *replacement) {
    if(!host||!w||!valid_range(old,r)||!replacement) return NO;
    NSData *before=[old dataUsingEncoding:NSUTF8StringEncoding],*after=[replacement dataUsingEncoding:NSUTF8StringEncoding];
    if(!before||!after||(after.length&&memchr(after.bytes,0,after.length))) return NO;
    size_t first=[[old substringToIndex:r.location] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    size_t last=[[old substringToIndex:NSMaxRange(r)] lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
    const char *text=replacement.UTF8String;
    return text&&tg_ime_queue(host->interp,w->id,first,last,before.length?before.bytes:"",before.length,text)==TG_OK;
}
@implementation TGPreviewView
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    if(tracking) { [self removeTrackingArea:tracking]; [tracking release]; }
    tracking=[[NSTrackingArea alloc] initWithRect:NSZeroRect
        options:NSTrackingMouseEnteredAndExited|NSTrackingMouseMoved|NSTrackingActiveInKeyWindow|NSTrackingInVisibleRect
        owner:self userInfo:nil];
    [self addTrackingArea:tracking];
}
- (void)dealloc {
    if(tracking) { [self removeTrackingArea:tracking]; [tracking release]; }
    [markedText release]; [compositionText release]; [super dealloc];
}
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    if(!host||!host->canvas) return;
    int width=tg_canvas_width(host->canvas),height=tg_canvas_height(host->canvas);
    if(width<=0||height<=0) return;
    const uint32_t *pixels=tg_canvas_pixels(host->canvas);
    CGColorSpaceRef rgb=CGColorSpaceCreateDeviceRGB();
    if(!rgb) return;
    CGDataProviderRef data=CGDataProviderCreateWithData(NULL,pixels,(size_t)width*height*4,NULL);
    if(data) {
        CGImageRef image=CGImageCreate(width,height,8,32,(size_t)width*4,rgb,
            kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,data,NULL,false,kCGRenderingIntentDefault);
        if(image) {
            CGContextRef ctx=NSGraphicsContext.currentContext.CGContext;
            CGContextSaveGState(ctx);
            CGContextTranslateCTM(ctx,0,self.bounds.size.height);
            CGContextScaleCTM(ctx,1,-1);
            CGContextSetInterpolationQuality(ctx,kCGInterpolationNone);
            CGContextDrawImage(ctx,CGRectMake(0,0,self.bounds.size.width,self.bounds.size.height),image);
            CGContextRestoreGState(ctx); CGImageRelease(image);
        }
        CGDataProviderRelease(data);
    }
    CGColorSpaceRelease(rgb);
    const tg_widget *w=input_widget(host);
    if(markedText.length&&w&&w->id==compositionTarget&&!w->show[0]) {
        NSRect r=input_rect(w),clip=NSMakeRect(w->rect.x,w->rect.y,w->rect.w,w->rect.h);
        NSDictionary *attrs=@{NSFontAttributeName:[NSFont systemFontOfSize:MAX(11,r.size.height-2)],
            NSForegroundColorAttributeName:[NSColor textColor],
            NSBackgroundColorAttributeName:[NSColor textBackgroundColor],
            NSUnderlineStyleAttributeName:@(NSUnderlineStyleSingle)};
        [NSGraphicsContext saveGraphicsState];
        NSRectClip(clip);
        [markedText.string drawAtPoint:r.origin withAttributes:attrs];
        [NSGraphicsContext restoreGraphicsState];
    }
}
- (void)pointer:(NSEvent *)e kind:(tg_event_kind)kind {
    if(!host||!host->alive) return;
    NSPoint p=[self convertPoint:e.locationInWindow fromView:nil];
    uint32_t button=(kind==TG_POINTER_DOWN||kind==TG_POINTER_UP)?(e.buttonNumber==1?3u:e.buttonNumber==2?2u:1u):0u;
    tg_window_input(host,kind,(int)p.x,(int)p.y,button|modifiers(e));
}
- (void)mouseDown:(NSEvent *)e { [self unmarkText]; [self.window makeFirstResponder:self]; [self pointer:e kind:TG_POINTER_DOWN]; }
- (void)mouseUp:(NSEvent *)e { [self pointer:e kind:TG_POINTER_UP]; }
- (void)rightMouseDown:(NSEvent *)e { [self pointer:e kind:TG_POINTER_DOWN]; }
- (void)rightMouseUp:(NSEvent *)e { [self pointer:e kind:TG_POINTER_UP]; }
- (void)scrollWheel:(NSEvent *)e {
    if(!host||!host->alive||e.scrollingDeltaY==0) return;
    NSPoint p=[self convertPoint:e.locationInWindow fromView:nil];
    int delta=e.scrollingDeltaY>0?120:-120;
    tg_window_input(host,TG_WHEEL,(int)p.x,(int)p.y,(uint32_t)(uint16_t)(int16_t)delta|modifiers(e));
}
- (void)mouseMoved:(NSEvent *)e { [self pointer:e kind:TG_POINTER_MOVE]; }
- (void)mouseDragged:(NSEvent *)e { [self pointer:e kind:TG_POINTER_MOVE]; }
- (void)mouseExited:(NSEvent *)e { [self pointer:e kind:TG_POINTER_EXIT]; }
- (void)keyDown:(NSEvent *)e {
    if(!host||!host->alive) return;
    uint32_t key=special(e),mods=modifiers(e);
    if(key&&!self.hasMarkedText) { tg_window_input(host,TG_KEY_DOWN,0,0,key|mods); return; }
    if(mods&TG_MOD_CONTROL) {
        const char *plain=e.charactersIgnoringModifiers.UTF8String;
        if(plain&&*plain) {
            uint32_t scalar; size_t used;
            if(tg_utf8_decode((const unsigned char *)plain,strlen(plain),&scalar,&used)==TG_OK) {
                tg_window_input(host,TG_KEY_DOWN,0,0,scalar|mods); return;
            }
        }
    }
    /* AppKit owns composition and only committed text enters the C editor. */
    [self interpretKeyEvents:@[e]];
}
- (void)insertText:(id)value replacementRange:(NSRange)replacementRange {
    const tg_widget *w=input_widget(host);
    NSString *s=[value isKindOfClass:[NSAttributedString class]]?[value string]:value;
    if(!w||w->state!=TG_STATE_NORMAL||![s isKindOfClass:[NSString class]]||
       (compositionTarget&&compositionTarget!=w->id)) { [self unmarkText]; return; }
    NSRange current; NSString *old=input_string(w,&current);
    BOOL composing=self.hasMarkedText;
    /* Preserve ordinary scalar key bindings; password fields never expose
       their text or accept an external UTF-16 range. */
    const char *text=s.UTF8String; uint32_t scalar=0; size_t used=0,n=text?strlen(text):0;
    if(replacementRange.location==NSNotFound&&(!composing||w->show[0])&&
       text&&tg_utf8_decode((const unsigned char *)text,n,&scalar,&used)==TG_OK&&used==n) {
        [self unmarkText];
        if(scalar>=32&&scalar!=127&&!(scalar>=0xf700&&scalar<=0xf8ff))
            tg_window_input(host,TG_TEXT_INPUT,0,0,scalar);
        return;
    }
    if(w->show[0]||!old||(composing&&![old isEqualToString:compositionText])) {
        [self unmarkText]; NSBeep(); return;
    }
    NSRange range=replacementRange; NSString *commit=s;
    if(composing) {
        if(range.location==NSNotFound||NSEqualRanges(range,self.markedRange)) range=markedBase;
        else {
            NSString *virtualText=[old stringByReplacingCharactersInRange:markedBase withString:markedText.string];
            if(!valid_range(virtualText,range)) { [self unmarkText]; NSBeep(); return; }
            /* Apply the explicit range in the virtual document. Preserve the
               unaffected composition and map the resulting edit back to C. */
            NSString *updated=[virtualText stringByReplacingCharactersInRange:range withString:s];
            NSUInteger prefix=0,suffix=0,limit=MIN(old.length,updated.length);
            while(prefix<limit&&[old characterAtIndex:prefix]==[updated characterAtIndex:prefix]) prefix++;
            while(prefix&&!scalar_boundary(old,prefix)) prefix--;
            while(suffix<old.length-prefix&&suffix<updated.length-prefix&&
                  [old characterAtIndex:old.length-1-suffix]==[updated characterAtIndex:updated.length-1-suffix]) suffix++;
            while(suffix&&(!scalar_boundary(old,old.length-suffix)||!scalar_boundary(updated,updated.length-suffix))) suffix--;
            range=NSMakeRange(prefix,old.length-prefix-suffix);
            commit=[updated substringWithRange:NSMakeRange(prefix,updated.length-prefix-suffix)];
        }
    } else if(range.location==NSNotFound) range=current;
    BOOL ok=queue_replacement(host,w,old,range,commit);
    [self unmarkText]; if(!ok) NSBeep();
}
- (void)setMarkedText:(id)value selectedRange:(NSRange)selection replacementRange:(NSRange)replacement {
    const tg_widget *w=input_widget(host);
    if(!w||w->state!=TG_STATE_NORMAL||w->show[0]) { [self unmarkText]; return; }
    NSRange current; NSString *old=input_string(w,&current);
    if(!old) { [self unmarkText]; return; }
    NSAttributedString *next=[value isKindOfClass:[NSAttributedString class]]?[value copy]:
        [[NSAttributedString alloc] initWithString:[value isKindOfClass:[NSString class]]?value:@""];
    if(!self.hasMarkedText||compositionTarget!=w->id) {
        [self unmarkText];
        markedBase=replacement.location==NSNotFound?current:replacement;
        if(!valid_range(old,markedBase)) { [next release]; NSBeep(); return; }
        compositionText=[old copy]; compositionTarget=w->id;
    } else {
        if(![old isEqualToString:compositionText]) { [next release]; [self unmarkText]; return; }
        if(replacement.location!=NSNotFound&&!NSEqualRanges(replacement,self.markedRange)) {
            NSString *virtualText=[old stringByReplacingCharactersInRange:markedBase withString:markedText.string];
            if(!valid_range(virtualText,replacement)) { [next release]; [self unmarkText]; NSBeep(); return; }
            NSRange previous=self.markedRange;
            NSUInteger begin=MIN(previous.location,replacement.location),end=MAX(NSMaxRange(previous),NSMaxRange(replacement));
            NSString *span=[virtualText substringWithRange:NSMakeRange(begin,end-begin)];
            NSString *updated=[span stringByReplacingCharactersInRange:NSMakeRange(replacement.location-begin,replacement.length) withString:next.string];
            NSUInteger baseEnd=NSMaxRange(markedBase)+(end-NSMaxRange(previous));
            markedBase=NSMakeRange(begin,baseEnd-begin);
            selection.location=MIN(selection.location,next.length)+(replacement.location-begin);
            [next release]; next=[[NSAttributedString alloc] initWithString:updated];
        }
    }
    [markedText release]; markedText=next;
    NSUInteger at=MIN(selection.location,markedText.length);
    markedSelection=NSMakeRange(at,MIN(selection.length,markedText.length-at));
    [self setNeedsDisplay:YES];
}
- (void)unmarkText {
    [markedText release]; markedText=nil; [compositionText release]; compositionText=nil; compositionTarget=0;
    markedSelection=markedBase=NSMakeRange(NSNotFound,0);
    [self setNeedsDisplay:YES];
}
- (BOOL)hasMarkedText {
    const tg_widget *w=input_widget(host);
    return markedText.length!=0&&w&&w->id==compositionTarget;
}
- (NSRange)markedRange { return self.hasMarkedText?NSMakeRange(markedBase.location,markedText.length):NSMakeRange(NSNotFound,0); }
- (NSRange)selectedRange {
    if(self.hasMarkedText) return NSMakeRange(markedBase.location+markedSelection.location,markedSelection.length);
    NSRange selected; (void)input_string(input_widget(host),&selected); return selected;
}
- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText { return @[]; }
- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actual {
    NSRange selection;
    NSString *s=input_string(input_widget(host),&selection);
    if(!s) return nil;
    if(self.hasMarkedText&&markedBase.location<=s.length&&markedBase.length<=s.length-markedBase.location)
        s=[s stringByReplacingCharactersInRange:markedBase withString:markedText.string];
    if(!valid_range(s,range)) return nil;
    if(actual) *actual=range;
    return [[[NSAttributedString alloc] initWithString:[s substringWithRange:range]] autorelease];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return NSNotFound; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actual {
    if(actual) *actual=range;
    const tg_widget *w=input_widget(host);
    NSRect r=input_rect(w);
    return [self.window convertRectToScreen:[self convertRect:r toView:nil]];
}
- (void)doCommandBySelector:(SEL)selector {
    if(!host||!host->alive) return;
    if(selector==@selector(cancelOperation:)) { [self unmarkText]; return; }
    uint32_t key=selector==@selector(deleteBackward:)?TG_KEY_BACKSPACE:
        selector==@selector(deleteForward:)?TG_KEY_DELETE:
        selector==@selector(insertNewline:)?TG_KEY_RETURN:
        selector==@selector(insertTab:)?TG_KEY_TAB:
        selector==@selector(moveLeft:)?TG_KEY_LEFT:selector==@selector(moveRight:)?TG_KEY_RIGHT:
        selector==@selector(moveUp:)?TG_KEY_ARROW_UP:selector==@selector(moveDown:)?TG_KEY_ARROW_DOWN:0;
    if(key) tg_window_input(host,TG_KEY_DOWN,0,0,key);
}
- (BOOL)resignFirstResponder { [self unmarkText]; return [super resignFirstResponder]; }
- (void)keyUp:(NSEvent *)e {
    uint32_t key=special(e);
    if(host&&host->alive&&key) tg_window_input(host,TG_KEY_UP,0,0,key|modifiers(e));
}
@end
@implementation TGPreviewDelegate
- (BOOL)windowShouldClose:(NSWindow *)sender { (void)sender; if(host) tg_window_request_close(host); return NO; }
- (void)windowDidBecomeKey:(NSNotification *)n { (void)n; if(host&&host->alive) tg_window_input(host,TG_FOCUS_IN,0,0,0); }
- (void)windowDidResignKey:(NSNotification *)n { (void)n; if(host&&host->alive) tg_window_input(host,TG_FOCUS_OUT,0,0,0); }
@end

int tg_window_open(tg_window *w) {
    if(!w||![NSThread isMainThread]) return 0;
    @autoreleasepool {
        NSApplication *app=[NSApplication sharedApplication];
        if(app.activationPolicy==NSApplicationActivationPolicyProhibited)
            [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        static int prepared;
        if(!prepared) { if(!app.isRunning) [app finishLaunching]; prepared=1; }
        native *n=calloc(1,sizeof *n); if(!n) return 0;
        NSRect r=NSMakeRect(0,0,w->requested_width,w->requested_height);
        n->window=[[NSWindow alloc] initWithContentRect:r styleMask:NSWindowStyleMaskTitled|
            NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        if(!n->window) { free(n); return 0; }
        n->view=[[TGPreviewView alloc] initWithFrame:r];
        n->delegate=[[TGPreviewDelegate alloc] init];
        if(!n->view||!n->delegate) {
            [n->window setReleasedWhenClosed:NO]; [n->window close];
            [n->window release]; [n->view release]; [n->delegate release]; free(n); return 0;
        }
        n->view->host=w; n->delegate->host=w;
        [n->window setReleasedWhenClosed:NO]; [n->window setDelegate:n->delegate];
        [n->window setContentView:n->view]; [n->window setAcceptsMouseMovedEvents:YES];
        [n->window makeFirstResponder:n->view]; [n->window center];
        n->width=w->requested_width; n->height=w->requested_height; w->native=n;
        [n->window setTitle:[NSString stringWithUTF8String:w->title]];
        [n->window makeKeyAndOrderFront:nil];
        return 1;
    }
}
void tg_window_close_native(tg_window *w) {
    native *n=w?w->native:NULL; if(!n||!n->window) return;
    @autoreleasepool {
        n->view->host=NULL; n->delegate->host=NULL;
        [n->window setDelegate:nil]; [n->window orderOut:nil]; [n->window close];
        [n->window release]; n->window=nil; w->alive=0;
    }
}
void tg_window_free_native(tg_window *w) {
    native *n=w?w->native:NULL; if(!n) return;
    tg_window_close_native(w);
    [n->view release]; [n->delegate release]; free(n); w->native=NULL;
}
int tg_window_poll_native(tg_window *w) {
    native *n=w?w->native:NULL; if(!n||!n->window||![NSThread isMainThread]) return 0;
    @autoreleasepool {
        int handled=0;
        NSEvent *e=[NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
            inMode:NSDefaultRunLoopMode dequeue:YES];
        if(e) { [NSApp sendEvent:e]; handled=1; }
        NSRect r=n->view.bounds; int width=(int)r.size.width,height=(int)r.size.height;
        if(width>0&&height>0&&(width!=n->width||height!=n->height)) {
            n->width=width; n->height=height; w->dirty=w->layout_dirty=1;
            tg_window_input(w,TG_CONFIGURE,width,height,0); handled=1;
        }
        if(tg_window_refresh(w,width,height)) { [n->view setNeedsDisplay:YES]; handled=1; }
        [n->window displayIfNeeded];
        return handled;
    }
}
int tg_window_set_title(tg_window *w,const char *s) {
    native *n=w?w->native:NULL; if(!n||!n->window||!s) return 0;
    @autoreleasepool { NSString *title=[NSString stringWithUTF8String:s]; if(!title) return 0; [n->window setTitle:title]; }
    return 1;
}
int tg_window_set_size(tg_window *w,int width,int height) {
    native *n=w?w->native:NULL; if(!n||!n->window||width<1||height<1) return 0;
    @autoreleasepool { [n->window setContentSize:NSMakeSize(width,height)]; }
    return 1;
}
int tg_window_set_visible(tg_window *w,int visible) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    @autoreleasepool { if(visible) [n->window makeKeyAndOrderFront:nil]; else [n->window orderOut:nil]; }
    return 1;
}
static NSRect screen_frame(void) {
    NSArray<NSScreen *> *screens=NSScreen.screens;
    return screens.count?screens[0].frame:NSZeroRect;
}
int tg_window_screen(tg_window *w,int *x,int *y) {
    (void)w; if(!x||!y) return 0;
    @autoreleasepool { NSRect s=screen_frame(); *x=(int)s.size.width; *y=(int)s.size.height; }
    return *x>0&&*y>0;
}
int tg_window_origin(tg_window *w,int *x,int *y) {
    native *n=w?w->native:NULL; if(!n||!n->window||!x||!y) return 0;
    @autoreleasepool {
        NSRect r=[n->window contentRectForFrameRect:n->window.frame],s=screen_frame();
        *x=(int)(r.origin.x-s.origin.x); *y=(int)(NSMaxY(s)-NSMaxY(r));
    }
    return 1;
}
int tg_window_set_geometry(tg_window *w,int width,int height,int pos,int x,int y,int nx,int ny) {
    native *n=w?w->native:NULL; if(!n||!n->window||!tg_window_set_size(w,width,height)) return 0;
    if(pos) @autoreleasepool {
        NSRect s=screen_frame();
        CGFloat left=s.origin.x+(nx?s.size.width-width-x:x);
        CGFloat top=NSMaxY(s)-(ny?s.size.height-height-y:y);
        NSRect content=NSMakeRect(left,top-height,width,height);
        [n->window setFrame:[n->window frameRectForContentRect:content] display:YES];
    }
    return 1;
}
int tg_window_limits(tg_window *w) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    @autoreleasepool {
        [n->window setContentMinSize:NSMakeSize(w->resize_x?w->min_width:w->requested_width,w->resize_y?w->min_height:w->requested_height)];
        [n->window setContentMaxSize:NSMakeSize(w->resize_x?w->max_width:w->requested_width,w->resize_y?w->max_height:w->requested_height)];
    }
    return 1;
}
int tg_window_override(tg_window *w,int enabled) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    @autoreleasepool {
        NSRect content=[n->window contentRectForFrameRect:n->window.frame];
        [n->window setStyleMask:enabled?NSWindowStyleMaskBorderless:
            NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable];
        [n->window setFrame:[n->window frameRectForContentRect:content] display:YES];
        [n->window makeFirstResponder:n->view];
        w->override_redirect=enabled; w->dirty=w->layout_dirty=1;
    }
    return 1;
}
int tg_window_custom_caption(tg_window *w,int enabled) { (void)w;(void)enabled; return 0; }
int tg_window_caption_action(tg_window *w,int action) { (void)w;(void)action; return 0; }
int tg_window_caption_state(tg_window *w) { (void)w; return 0; }

int tg_cocoa_alpha(tg_window *w,double alpha) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    @autoreleasepool { [n->window setAlphaValue:(CGFloat)alpha]; } return 1;
}
int tg_cocoa_topmost(tg_window *w,int enabled) {
    native *n=w?w->native:NULL; if(!n||!n->window) return 0;
    @autoreleasepool { [n->window setLevel:enabled?NSFloatingWindowLevel:NSNormalWindowLevel]; } return 1;
}
int tg_cocoa_transient(tg_window *w,tg_window *parent) {
    native *n=w?w->native:NULL,*p=parent?parent->native:NULL;
    if(!n||!n->window||(parent&&(!p||!p->window))) return 0;
    @autoreleasepool {
        NSWindow *old=n->window.parentWindow;
        if(old) [old removeChildWindow:n->window];
        if(p) [p->window addChildWindow:n->window ordered:NSWindowAbove];
    }
    return 1;
}
int tg_cocoa_clipboard_put(const char *text) {
    if(!text||![NSThread isMainThread]) return 0;
    @autoreleasepool {
        NSString *s=[NSString stringWithUTF8String:text]; if(!s) return 0;
        NSPasteboard *pb=[NSPasteboard generalPasteboard]; [pb clearContents];
        return [pb setString:s forType:NSPasteboardTypeString];
    }
}
int tg_cocoa_clipboard_get(char *text,size_t capacity) {
    if(!text||!capacity||![NSThread isMainThread]) return 0;
    @autoreleasepool {
        NSString *s=[[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        const char *p=s.UTF8String; if(!p) return 0;
        size_t n=strlen(p); if(n>=capacity) return 0;
        memcpy(text,p,n+1); return n!=0;
    }
}
