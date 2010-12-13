// exists  ephy_orig
// {
// 	declare {
//     	ephy_get_forward_history: (_) => GList_of_EphyHistoryItems;
// 		ephy_get_backward_history: (_) => GList_of_EphyHistoryItems;
//     }
// };
derive elf_archive("ephy+.a") ephy = instantiate(
	instantiate(elf_archive("ephy.a"), EphyCommandManagerIface, man_impl, ""),
		EphyEmbedIface, embed_impl, "");
/* instantiate args: (component, structure type, structure name, symbol prefix) */
exists elf_external_sharedlib("webkit") webkit
{
	declare
    {
        webkit_web_back_forward_list_get_forward_list_with_limits:
	        (_) => GList_of_WebKitWebHistoryItems;
		WebKitEmbedLoadState: class_of enum {
        	WEBKIT_EMBED_LOAD_STARTED;
            WEBKIT_EMBED_LOAD_REDIRECTING;
            WEBKIT_EMBED_LOAD_LOADING;
            WEBKIT_EMBED_LOAD_STOPPED;
        };
        WEBKIT_BACK_FORWARD_LIMIT: const int = 100;
        WebKitHistoryType: class_of enum {
        	WEBKIT_HISTORY_BACKWARD;
            WEBKIT_HISTORY_FORWARD;
        };
    }
};
derive elf_exec ephy_webkit = make_exec(
	link[ephy, webkit]
    {
        ephy <--> webkit
        {
    	    /* Summary of history item handling.

             * webkit_construct_history_list is an embed function
             * which gets a webkit back_forward_list from webkit
             * then calls webkit again to get either a forward_list or a back_list
             * and then constructs a webkit_history_item for each element. 
             * webkit_history_item is an ephy-specific class simply wrapping the underlying gobject.
             * Its getters use strdup.
             * A back_forward_list provides methods to get_ either a forward GList or
             * a back GList, containing WebKitWebHistoryItems */
            values GList_of_EphyHistoryItem <--> GList_of_WebKitWebHistoryItem
            {
    	        data as EphyHistoryItem ptr <--> data as WebKitWebHistoryItem ptr;
                // This ensures that pointers will be followed as the relevant structs.
                // BUT this is not sufficient because these structs are opaque.
                // SO we need the extra rule on EphyHistoryItem and WebKitWebHistoryItem, below
            };
            // the block above results in two different GList types, which are rep-*in*compatible!
			// Actually, strictly speaking they are rep-compatible but non-shareable
			// because of reachability.

//             values EphyHistoryItem <--> WebKitWebHistoryItem
//             {
//     	        /* EphyHistoryItem is an interface providing get_url and get_title.
//                  * A WebKitHistoryItem is just a GObject with a single added field:
//                  * data, a pointer to a WebKitWebHistoryItem.
// 				 * NOTE that WebKitHistoryItem is, confusingly, part of the Epiphany codebase
// 				 * which we borrow. 
// 				 * -- in our case, the pointer is left undefined?
// 				 * -- How do we handle the calls on ephy_history_item_get_{url,title}?
// 				 * ** We have simply lifted the implementation! That's why it works!
// 				 * ** Q. Why do we even need this block? A. To set up the co-object relation,
// 				 *       so that when we implicitly use co-object pointer substitution
// 				 *       in the relevant event correspondence.
// 				 *       What are thse event correspondences?
// 				 * ** Q. So why are we creating a new WebkitWebHistoryItem? Surely we can
// 				 *       just grab the existing one from the "data" field?
// 				 * ** IN other words, the WebKitWebHistoryItem structure is used by both
// 				 *    interfaces. It just needs some contextual 
// 				 * ** SO (FIXME) get rid of the instantiation below. It should already be
// 				 *    done (CHECK) in the Epiphany code we retained. 
// 				 *    in embed/webkit/webkit-history-item.c
// 				 *    -- 
// 				 *
//                  * A WebKitWebHistoryItem is a GObject with a priv field. The priv
//                  * type is opaque and accessed through methods get_uri, get_title, 
//                  * get_alternate_title, set_alternate_title, get_last_visited_time.... */
// 
//   			    // NOTE: the "assign-to-this" RHS is a special syntax
// 				// allowed only when initializing a co-object
// 				// This is needed when
//                 // allocation needs to be done by the library and not by the Cake runtime!
//                 // This is when the library doesn't separate allocation from init.
//                 
// // 				void ({let url = ephy_history_item_get_url(this);
// //             		    let title = ephy_history_item_get_title(this) }) -->?
// //                     	    (let this = webkit_web_history_item_new_with_data(url, title)) void;
//                 
//                 
// 				
// 				// In the other direction initialization is harder: 
// 				// we need to define a GObject class.
//                 // Let's just abuse the existing code and document this. What's actually
//                 // hard about defining a new GObject class? FIXME.
//                 void (webkit_history_item_new(that))<--? void;
// 				// which side creates history entries? anyone mutate them?
//             };

			values
			{
				/* history item handling */

				// ARGH: this won't work because WebKitHistoryItem is
				// part of the code we want to rule out!
				// OH: actually we're okay because we decided that we
				// would keep WebKitHistoryItem as utility code
				
				// update rule 
				EphyHistoryItem (this->data)--> WebKitWebHistoryItem;
				// init rule only: we never send updates to webkit
				EphyHistoryItem (*(webkit_history_item_new(that) tie this))<--? WebKitWebHistoryItem;
				// this call --------------^^^^^^^^^^^^^^^^^ MIGHT FAIL by returning null.
				// If it fails, our stub will abort (before dereferencing).
				// This implies that our value correspondence will fail.
				// Q. What happens when a value correspondence fails?
				// A. The invoking stub fails.
				// This is often okay, but NOT what we want in the case of history list conversion...
				// ... where we want to silently pass over failed
			
			
				/* raw (user-typed) URL handling */
				raw_url <--> raw_url
            	{
                	// FIXME: can we really tie here? Shouldn't we free-then-dup like the embed code does?
                	pattern "(about:|(http[s]?|file|ftp://)).*" -->(g_strdup(that) tie that) void;
                	pattern ".*" -->(g_strconcat("http://", that) tie that) void;
            	};
			
			}

            //values string as 

			//ephy_embed_factory_new_object(EPHY_TYPE_EMBED) (let alloc = new GObject)--> { alloc };
			// NOT necessary: 
			ephy_embed_factory_new_object(EPHY_TYPE_EMBED) --> g_object_new(EPHY_TYPE_BASE_EMBED);
			// YES necessary: 
			
            // interfaces we instantiate:
            // ephy_embed_iface
            // ephy_command_manager_iface
			// We'd ideally like to define a new GObject type.
			// HACK: well, how does the client code get the iface table?
			// #define EPHY_EMBED_GET_IFACE(inst)      (G_TYPE_INSTANCE_GET_INTERFACE ((inst), EPHY_TYPE_EMBED, EphyEmbedIface))
			// #define G_TYPE_INSTANCE_GET_INTERFACE(instance, g_type, c_type) (_G_TYPE_IGI ((instance), (g_type), c_type))
			// #define _G_TYPE_IGI(ip, gt, ct)         ((ct*) g_type_interface_peek (((GTypeInstance*) ip)->g_class, gt))
			// GObject has a g_class
			// ... amounts to a call to g_type_interface_peek(inst->g_class, EPHY_TYPE_EMBED)
			// 
			// HACK! We can't do this because g_type_interface_peek is not in the trace
			// between ephy and webkit
			// -- HMM. Why not? How do we actually split the trace between glib and webkit?
			// HMM. Maybe we do this after processing Cake rules. SO the rule below is declaring that
			// this interaction is handled by this pair of components (since the RHS is clearly provided, so the LHS must be
			// required).
			// FIXME: *slices* need to be able to handle this.
			g_type_interface_peek(_, EPHY_TYPE_EMBED) --> { ephy_impl };
			g_type_interface_peek(_, EPHY_TYPE_COMMAND_MANAGER) --> { man_impl };
			// What's the right way to do this?
			// Root of the problem is that we don't have a class (data type definition) in the LHS
			// --- it uses function calls exclusively to instantiate and access
			// BUT the GObject infrastructure requires that the object have a certain form/schema/grammar
			// --- they're not completely opaque, so we can't just invent a new type within Cake

            values (embed: EphyBaseEmbed,
                    history: EphyHistory)  <-->    (web_view: WebKitWebView,
                                                    scrolled_window: GtkScrolledWindow,
                                                    load_state: WebKitEmbedLoadState,
                                                    loading_uri: char [])  
            {
                     void // history is a singleton, so always add it when forming the association
                    // FIXME: does this mean that we can only have one EphyEmbed? Since the co-object
                    // relation will want to map that singleton to many differnet RHS umbrella co-objects.
                    // MAybe introduce a "weak" class of association for which "..." is not valid?
                    // I think this is effectively what "value" members of associations are. They
                    // can't act as keys. You can't do "..." on them. In return, they can be present
                    // in more than one association.
                    (let history = ephy_embed_shell_get_global_history(ephy_embed_shell_get_default()))
                    -->? ({ // from webkit_embed_init
                        let web_view = webkit_web_view_new();
                    let sw = gtk_scrolled_window_new(null, null);

                        gtk_scrolled_window_set_policy(sw, 
                            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

                        gtk_container_add(sw, web_view);
                        gtk_widget_show(sw);
                        gtk_widget_show(web_view);
                        gtk_container_add(that, sw);

                        /* How is code generation for these lambdas going to look?
                         * We want to use C++ lambdas. we can deal with a call to
                         * a far-side function: we use the wrapper. But this is assuming
                         * our lambda is supposed to be a "near-side" function. What if
                         * we want to write the lambda as on the far side, then wrap the
                         * lambda itself? We don't want to support this. Why would it
                         * matter? Our lambda should assume that it always receives
                         * its argument as near-side values; the lambda body might 
                         * need to contain value conversions etc.. So in these examples,
                         * "embed" is actually received as the umbrella co-object. How
                         * did the webkit code get a pointer to this umbrella co-object
                         * in the first place? AH, it's the callback argument: we passed
                         * it when setting up the callback (below)! */

                        // set callbacks           
                        g_object_connect(web_view,  // in our use of a LHS function (ephy_base_...)
                           "signal::load-committed", // we imply that all *variables* i.e. globals,
                                fn (view, frame, embed) => { // including functions, may be named
                                    ephy_base_location_changed(embed, // across arrows, i.e. denoting. What about 
                                        webkit_web_frame_get_uri(frame)) // the co-object. What if
                                    }, embed, // the name is ambiguous? I suppose we just warn.
                            "signal::load-started", 
                                fn (view, frame, embed) => {
                                    if loading_uri != null then 
                                        ephy_history_add_page(history, loading_uri, FALSE, FALSE)
                                    else void;
                                    ephy_base_embed_update_from_net_state(
                                        EPHY_EMBED_STATE_UNKNOWN | EPHY_EMBED_STATE_START
                                        | EPHY_EMBED_STATE_NEGOTIATING | EPHY_EMBED_STATE_IS_REQUEST
                                        | EPHY_EMBED_STATE_IS_NETWORK); 
                                    }, embed,
                            "signal::load-progress-changed", 
                                fn (view, progress, embed) => {
                                    if load_state == WEBKIT_EMBED_LOAD_STARTED then // NOTE: new "set" keyword (was: let should not be used as assignment!)
                                        { set load_state = WEBKIT_EMBED_LOAD_LOADING } else void;
                                    ephy_base_embed_set_load_percent(embed, progress)
                                    }, embed,
                            "signal::load_finished", 
                                fn (view, frame, embed) => {
                                    set load_state = WEBKIT_EMBED_LOAD_STOPPED;
                                    ephy_base_embed_update_from_net_state(
                                        EPHY_EMBED_STATE_UNKNOWN | EPHY_EMBED_STATE_STOP
                                        | EPHY_EMBED_STATE_IS_DOCUMENT | EPHY_EMBED_STATE_IS_NETWORK) 
                                    }, embed,
                            "signal::title-changed",
                                fn (view, frame, title, embed) => {
                                    ephy_base_embed_set_title(embed, title);
                                }, embed,
                            "signal::hovering-over-link", 
                                fn (view, frame, embed) => {
                                    ephy_base_embed_set_link_message(embed, location);
                                    }, embed, s,
                            null);

                        // associate preferences somehow
                        webkit_web_view_set_settings(web_view, settings)//; // "settings" is a global

                        /*vals*/ /*}*/}) void;
            };

            // interfaces we instantiate:
            // ephy_embed_iface
            // ephy_command_manager_iface

            ephy_manager_do_command(man, "cmd_copy") --> webkit_web_view_copy_clipboard(man...web_view);
            ephy_manager_do_command(man, "cmd_cut") --> webkit_web_view_cut_clipboard(man...web_view);
            ephy_manager_do_command(man, "cmd_paste") --> webkit_web_view_paste_clipboard(man...web_view);
            ephy_manager_do_command(man, "cmd_selectAll") --> webkit_web_view_select_all(man...web_view);

            ephy_manager_do_command(man, "cmd_copy") --> webkit_web_view_can_copy_clipboard(man...web_view);
            ephy_manager_do_command(man, "cmd_cut") --> webkit_web_view_can_cut_clipboard(man...web_view);
            ephy_manager_do_command(man, "cmd_paste") --> webkit_web_view_can_paste_clipboard(man...web_view);
            ephy_manager_do_command(man, _) --> { false };

            ephy_load_url(embed, url) --> webkit_web_view_open(embed...web_view, url);

            ephy_load(embed, url as raw_url, flags, preview_embed) 
    	        --> { 	set embed...loading_url = url; // hmm, is "let" the right behaviour?
        		        webkit_web_view_open(embed...web_view, url) };    			

            ephy_stop_load(embed) --> webkit_web_view_stop_loading(embed...web_view);
            ephy_can_go_back(embed) --> webkit_web_view_can_go_back(embed...web_view);
            ephy_can_go_forward(embed) --> webkit_web_view_can_go_forward(embed...web_view);
            ephy_can_go_up(embed) --> { false };
            ephy_get_go_up_list(embed) --> { set [] }; // FIXME: should be list
            ephy_go_back(embed) --> webkit_web_view_go_back(embed...web_view);
            ephy_go_forward(embed) --> webkit_web_view_go_forward(embed...web_view);
            ephy_go_up(embed) --> { void };
            ephy_get_js_status(embed) --> { "" };

            // toplevel is ignored
            ephy_get_location(embed, toplevel) --> { // FIXME: use "copy" and a glib-aware style here
    	        g_strdup(webkit_web_frame_get_uri(webkit_web_view_get_main_frame(embed...web_view))); };

            ephy_reload(embed, force) --> webkit_web_view_reload(embed...web_view);

            // some functions are no-ops
            pattern /ephy_(set_zoom|scroll_lines|scroll_page|scroll_pixels|shistory_copy)/
    	        (...) --> { void }; // silent ignore
            ephy_get_zoom(embed) --> { 1.0 }; // no zoom support in webkit

            ephy_get_security_level(embed, out level, description) 
    	        (let unknown = EPHY_EMBED_STATE_IS_UNKNOWN)--> { out level = unknown; };

            // more silent ignore 
            pattern /ephy_(show_page_certificate|print|.*print_preview.*|set_encoding|get_encoding)/
    	        (...) --> { void }; // silent ignore: "return 0" for preview_n_pages is inferred
            ephy_has_automatic_encoding(_) --> { false };
            ephy_has_modified_forms(_) --> { false };

            // returns a GList* of objects that are EphyHistoryItems 
            pattern /ephy_get_(((back)|(forward))((ward)?))_history/ (embed) /*out_as GList_of_EphyHistoryItems*/ --> {
				// get a pointer to the WebKit-internal list
    	        let full_bf_list = webkit_web_view_get_back_forward_list(embed...web_view);
				// copy out the portion we want into a GList
                let copied_sublist = webkit_web_back_forward_list_get_\1\4_list_with_limits(
            	    full_bf_list,
        	        WEBKIT_BACK_FORWARD_LIMIT)
                // when our walker traverses the input list, it will see WebKitWebHistoryItems
				// thanks to the annotations we added above
                // and know that it has to allocate co-objects.
                // HMM. It also needs to allocate a new list, because although it could share
                // the list nodes, their pointers are pointing into rep-incompatible memory.
				// This is a classic use-case of shareability analysis.

                //g_list_free(
                // NO free -- we will get freed when our peer glist is freed
		        } ;
// 			<--
// 	{  let co_list = copied_sublist untie_all; // FORCE an early copy AND untie
// 		// FIXME: can we leave the untie implicit, i.e. that being the only reason to early-copy?
// 		// FIXME: forgotten why the "free when peer list is freed" strategy doesn't work
// 	   g_list_free(copied_sublist as GList ptr); 
// 	   co_list };
	
//             ephy_get_forward_history(embed) /*out_as GList_of_EphyHistoryItems*/ --> { 
//     	        let full_list = webkit_web_view_get_back_forward_list(embed...web_view);
//                 webkit_web_back_forward_list_get_forward_list_with_limits(
//             	    full_list,
//         	        WEBKIT_BACK_FORWARD_LIMIT)
//             };
            ephy_get_next_history_item(embed) --> { 
        	    webkit_web_back_forward_list_get_forward_item(
            	    webkit_web_view_get_back_forward_list(embed...web_view)
                    )
            };
		    ephy_get_previous_history_item(embed) --> {
        	    webkit_web_back_forward_list_get_back_item(
            	    webkit_web_view_get_back_forward_list(embed...web_view)
                    )
            };
            ephy_go_to_history_item(embed, item) --> 
        	    webkit_web_view_go_to_back_forward_item (embed...web_view, item);
        } // end ephy <--> webkit
    } // end link
); // end make_exec
