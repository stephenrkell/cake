/*extern rep_sync_func_t **rep_conv_funcs[];
extern const size_t *object_rep_layout_sizes[];
extern const int **subobject_forms[];	
extern const size_t **subobject_offsets[];
extern const int **derefed_forms[];	
extern const size_t **derefed_offsets[];
extern const char *object_forms[];*/

rep_sync_func_t get_rep_conv_func(int from_rep, int to_rep, int form) __attribute__((weak));
/* get_rep_conv_func(from_rep, to_rep, form)(p->reps[from_rep], p->reps[to_rep]);  */
               
size_t get_object_rep_layout_size(int rep, int form) __attribute__((weak));
/* object_rep_layout_sizes[co_object_rep][form]      */
         
int get_subobject_form(int rep, int form, int index) __attribute__((weak));
/* get_subobject_form(rep, start_subobject_form, i) */
                
size_t get_subobject_offset(int rep, int form, int index) __attribute__((weak));
/* get_subobject_offset(rep, start_subobject_form, i) != (size_t) -1; */
                
int get_derefed_form(int rep, int form, int index) __attribute__((weak));
/* get_derefed_form(rep, start_subobject_form, i) */

size_t get_derefed_offset(int rep, int form, int index) __attribute__((weak));
/* get_derefed_offset(rep, start_subobject_form, i) != (size_t) -1; */

const char *get_object_form(int form) __attribute__((weak));
/* get_object_form(start_subobject_form) */
