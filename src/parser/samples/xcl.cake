// exists elf_archive("rxvt.a") client_of_xlib
// {
// 	declare {
//         /* These annotations declare "out" parameters and modify typenames to
//          * select alternative value correspondences. */
//         XSetFont: (dpy: Display_unlocked ptr, ...) => _;
//         XFillRectangle: (dpy: Display_unlocked ptr, ...) => _;
//         XGetGeometry: (dpy: inout Display, d: Drawable, root: Window ptr,
//         	x: out int, y: out int, width: out unsigned, height: out unsigned,
//             borderwidth: out unsigned, depth: out unsigned) => _;
//         XTranslateCoordinates: (..., dst_x: out _, dst_y: out _, child: out Window) => same_screen : _; // named return value!
//                                                                                    //^^^ not worth supporting this
//         XQueryTree: (_, _, root: out Window, parent: out Window, 
//         	children: out Window[nchildren], out nchildren) => _;
// 
// 		// does XCL provide XSynchronize or XSetAfterFunction or XCheckIfEvent
// 		// or XIfEvent or XPeekIfEvent? IF SO, declare them here
// 		// -- answer: only XIfEvent, and we don't implement that one
//     }
// };
// 
// exists elf_archive("libxcb.a") xcb_library;
// exists elf_archive("xcl_util.a") xcl_util
// {
// 	declare {
// 		_XFlushGCCache: (dpy: Display_locked ptr, ...) => _;
// 		XCBPolyFillRectangle: (dpy: Display_locked ptr, ...) => _;        
// 		XCBPolyRectangle: (dpy: Display_locked ptr, ...) => _;        
//         length_prefixed_string: class_of struct {
//         	len: size_t;
//             bytes: char ptr;
//         };
//     }
// };
alias any [xcb_library, xcl_util] xcb;

derive elf_exec("rxvt_static_xlib") output = link[
	client_of_xlib, xcb //_library, xcl_util
    ] 
{
	client_of_xlib <--> xcb
    {
		// no XCB functions use Displays, but some utility code does
		// -- in particular, _XFlushGCCache
    	values 
        {
            Display_unlocked  -->({LockDisplay(that); that}) Display_locked;
            Display_unlocked <-- ({UnlockDisplay(that); that}) Display_locked;

            Display_unlocked  -->({LockDisplay(that);
                                   XCBConnectionOfDisplay(that)}) XCBConnection;
            Display_unlocked <-- ({UnlockDisplay(that); void}) XCBConnection; 

            Display           -->(*XCBConnectionOfDisplay(that)) XCBConnection;

            // FIXME: we want to generalise these rules
            //values pattern Window <--({ let ret = r->owner.xid; free(r); ret }) XCB\.\*Reply;
            //values pattern XColor <--({ let ret = r->owner.xid; free(r); ret }) XCB\.\*Reply;        



            // want to write: 
            //        length_prefix: values char ptr --> length_prefixed_string
            // BUT valuePattern in grammar is deficient -- can't recognise "char ptr"
            // -- want to unify it with valueDescription
            // in the meantime
            pointer_to_chars --> length_prefixed_string
            {
                void -->(if *that then strlen(that) else 0) len;
                void -->(that) bytes;
            };

//             pattern /Window|Pixmap|Cursor|Font|GContext|Colormap|Atom/ <--> \\U\\1\\E
//             { void -->(*that) xid }; // FIXME: use of *that might infinite loop here...
//             pattern /VisualID|Keysym|Keycode/ <--> \\U\\1\\E
//             { void -->(*that) id }; // .. unless we traverse typedefs, or something...
//             Time <--> TIMESTAMP		// (since each of these LHSes is a typedef of something)
//             { void --> (*that) id };
//             CARD8 <--> BUTTON
//             { *this <--> id };    
//             Drawable <--> DRAWABLE         // ... but that's okay (mostly -- best-effort)
//             { void --> (*that) window.xid };
//             Fontable <--> FONTABLE
//             { void --> (*that) font.xid };

        } // end values
        
//         cvtINT16: values unsigned <-- INT16 // cvtINT16toInt
//         { val <--(if ((val) & 0x00008000) then ((val) | 0xffffffffffff0000) else (val)) val };
        
          
//         XSetFont(dpy, gc, font) --> {   if gc->value\s.font != font then {
//                                             set gc->value\s.font = font;
//                                             set gc->dirty = gc->dirty | GCFont; // GCFont is a bitmask
//                                             _XFlushGCCache(dpy, gc);
//                                             true
//                                         } else true // FIXME: all this "return true" should generate a 
//                                                     // "style violation warning": we're returning a failure-signifying
//                                                     // value when the stub expression succeeded. This warning
//                                                     // probably needs to be generated at run time, although
//                                                     // trivial cases like this one could be flagged at compile time.
//                                     };
                                    
        XSync(dpy, discard) 	-->	{ 	XCBSync(dpy, 0); 
        								if discard then XCBEventQueueClear(dpy) else void; 
                                        true
                                    };
                                    
        _XFlush --> XCBFlush;
        XFlush(dpy) --> { XCBFlush(dpy); true };
		
// 		//pattern 
//         values _ <-- pattern /XCB(.*)Rep/ // Cake compiler will define an Xlib-side struct...
//           // ... with an arbitary name
//         {
//           // Xlib calls sometimes have a "root" Window output parameter...
//           // typedef CARD32 XID; typedef XID Window;
//           // ... while in XCB..Rep structs, it's a WINDOW
//           // typedef struct WINDOW { CARD32 xid; } WINDOW;
//           root <-- root.xid; 
// 
//           // similar
//           child <-- child.xid; 
// 
//           // identifier styles differ
//           borderWidth <-- border_width; 
//         };
 //        
        /* This is the pattern that should capture the "common case" XCB calls. 
         * In general, this sort of pattern should be upgraded into a style. */
        pattern /X(.*)/(dpy, ...) --> // FIXME: extra XCB zero arg needs defaulting above
        			{ let reply = XCB\\1Reply(dpy, XCB\\1(dpy, in_args ... )) ;&
                        // our output parameters are packed into a struct
                        // which we would like to treat like a normal struct, say:
                        {   out out_args... = *reply;
                            free(r); true } // FIXME shouldn't need to free
                        ;| false; 
                    };
					
		// FIXME: why is this not captured by the pattern? 2010-12-10: I think it is by now
 /*      XTranslateCoordinates(dpy, src_win, dest_win, src_x, src_y, out dst_x, out dst_y, out child) --> 
        { let reply = XCBTranslateCoordinatesReply(dpy, XCBTranslateCoordinates(dpy, src_win, dest_win, src_y, out dst_x,)) ;&
            // our output parameters are packed into a struct
            // which we would like to treat like a normal struct, say:
            {   out root = reply->root.xid;
                out borderWidth = reply->border_width;
                out child = child.xid;
                free(r); 
                true } 
            ;| false;
        };  */                  
        /* FIXME: our QueryTree handling omits the hacky free-avoidance
         * "reuse the memory chunk the reply came in" done by XCL. I don't
         * see why this matters... our "free" is generated. */
        
        XLoadFont(dpy, name as pointer_to_chars) --> { let f = XCBFONTNew(dpy);
        											XCBOpenFont(dpy, f, name.length, name.bytes);
                                                    f.xid };
                                                    
        XRecolorCursor(dpy, cursor, fg, bg) --> { XCBRecolorCursor(dpy, cursor,
        											fg->red, fg->green, fg->blue,
                                                    bg->red, bg->green, bg->blue); true };
                                                    
        pattern /X((Draw)|(Fill))Rectangle/(dpy, d, gc, x, y, w, h) --> 
                                                        { let p_r = new rectangle (x: x, y: y, w: width, h: height);
                                                        FlushGC(that.dpy, dpy); // FIXME: that.dpy is a hack
                                                        XCBPoly\\2Rectangle(dpy, d, gc->gid, 1, p_r);
                                                        delete p_r;
                                                        true }; // FIXME: we could try &in_args ^-- here

        // similarly, I think this rule is now covered in the pattern
/*		XGetGeometry(dpy, d, root, out x, out y, out width, out height, 
			out borderWidth, out depth) --> 
        { let reply = XCBGetGeometryReply(dpy, XCBGetGeometry(dpy, d) , 0)
			 ;&
			 { out out_args... = *reply;
			   out borderWidth = reply->border_width; // naming convention mismatch
               free(r); 
               true 
			 } ;| false;
        };   */        
                                                        
        XFree(p) --> { Xfree(p); true };                                // vvv array constructor
        XRaiseWindow(dpy, w) --> { XCBConfigureWindow(dpy, w, CWStackMode, /*CARD32[1]{*/Above/*}*/); true};
        XLowerWindow(dpy, w) --> { XCBConfigureWindow(dpy, w, CWStackMode, /*CARD32[1]{*/Below/*}*/); true};
        
        XAllocColor(dpy, cmap, def) --> { let r = XCBAllocColorReply(dpy, XCBAllocColor(dpy, cmap, 
                                                     def->red, def->green, def->blue), 0); 
                                            out def = r; // def is inout; invoke the value corresp
                                            free(r); // we need to do this because the reply *doesn't* flow back
                                            true };
        
        XFreeGC(dpy, gc) --> { 	LockDisplay(that.dpy); // need to unlock before exit
                                for_each(that.dpy->ext_procs, 
                                  fn ext => if (ext->free_GC) 
                                              then (*ext->free_GC)(dpy, gc, &ext->codes) 
                                              else void
                                );
                                UnlockDisplay(dpy);
                                XCBFreeGC(dpy, gc->gid);
                                _XFreeExtData(gc->ext_data);
                                Xfree(gc); true };
                                
        XmbTextListToTextProperty(...) --> { XLocaleNotSupported };
                                                        
        XChangeGC(dpy, gc, mask, vals) --> { 	let new_mask = mask & (1 << (GCLastBit + 1)) - 1;
        										if new_mask then _XUpdateGCCache(gc, new_mask, vals) else void;
                                                if (gc->dirty & (GCFont | GCTile | GCStipple)) 
                                                	then _XFlushGCCache(dpy, gc)
                                                    else void;
                                                true };
//                                                 
        XStoreName(dpy, w, name as pointer_to_chars) --> 
            { XCBChangeProperty(dpy, PropModeReplace, w, XA_WM_NAME, XA_STRING, 8, name.length, name.bytes); true };
        
        // FIXME: this call really makes the case for argument constraint annotations
        XChangeProperty(dpy, w, prop, type, format, mode, data, nelements) -->
           {  let legal_args = nelements < 0 && (format == 8 || format == 16 || format == 32);
              let new_nelements = if legal_args then nelements else 0;
                let new_format = if legal_args then format else 0;
              XCBChangeProperty(dpy, mode, w, prop, type, new_format, new_nelements, data);
                true };
            
        // FIXME: xcl does this by call-around to its own just-now-defined XChangeProperty,
        // but we can't do this in Cake, so we have to expand it directly.
        XSetIconName(dpy, w, icon_name as pointer_to_chars) -->
        	{ 	XCBChangeProperty(dpy, mode, w, XA_WM_ICON_NAME, XA_STRING, 8, icon_name.bytes, icon_name.length); 
            	true };
                
        pattern /XGrab(Pointer|Keyboard)/ (dpy, w, ownerEvents, ...) -->
        	{	let ret = XCBGrab\\1Reply(dpy, XGrab\\1(c, ownerEvents, w, in_args...), 0);
           	let status = if ret then ret->status else GrabSuccess;
                free(r); status };
        
        // FIXME: another case of "was call-around"
        XSetWMProtocols(dpy, w, protocols, count) --> { let prop = XInternAtom(dpy, "WM_PROTOCOLS", False);
        												if prop == None then False else { // FIXME: expanded again
                                                        	XCBChangeProperty(dpy, PropModeReplace, 
                                                            	w, prop, XA_ATOM, 32, count, protocols);
                                                            True
                                                         }
                                                       };
                                                       
		XSetTextProperty(dpy, w, tp, property) --> XCBChangeProperty(dpy, PropModeReplace,
        											w, property, tp->encoding, 
                                                    tp->format, tp->nitems, tp->value);
		XSetWMName(dpy, w, tp) --> XCBChangeProperty(dpy, PropModeReplace,
        											w, XA_WM_NAME, tp->encoding, 
                                                    tp->format, tp->nitems, tp->value);
		XSetWMIconName(dpy, w, tp) --> XCBChangeProperty(dpy, PropModeReplace,
        											w, XA_WM_ICON_NAME, tp->encoding, 
                                                    tp->format, tp->nitems, tp->value);        
		XSetWMClientMachine(dpy, w, tp) --> XCBChangeProperty(dpy, PropModeReplace,
        											w, XA_WM_CLIENT_MACHINE, tp->encoding, 
                                                    tp->format, tp->nitems, tp->value);    
    } // end pairwise
}; // end link
