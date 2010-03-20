typedef unsigned int size_t;
typedef unsigned char __u_char;
typedef short unsigned int __u_short;
typedef unsigned int __u_int;
typedef unsigned int __u_long;
typedef char __int8_t;
typedef unsigned char __uint8_t;
typedef short int __int16_t;
typedef short unsigned int __uint16_t;
typedef int __int32_t;
typedef unsigned int __uint32_t;
typedef long long int __int64_t;
typedef long long unsigned int __uint64_t;
typedef long long int __quad_t;
typedef long long unsigned int __u_quad_t;
typedef __u_quad_t __dev_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef unsigned int __ino_t;
typedef __u_quad_t __ino64_t;
typedef unsigned int __mode_t;
typedef unsigned int __nlink_t;
typedef int __off_t;
typedef __quad_t __off64_t;
typedef int __pid_t;
struct _dwarfhpp_anon_2ad { 

	int __val[2] ; // offset: 0
	
} __attribute__((packed));
typedef int _dwarfhpp_anon_2c6[2];
typedef _dwarfhpp_anon_2ad __fsid_t;
typedef int __clock_t;
typedef unsigned int __rlim_t;
typedef __u_quad_t __rlim64_t;
typedef unsigned int __id_t;
typedef int __time_t;
typedef unsigned int __useconds_t;
typedef int __suseconds_t;
typedef int __daddr_t;
typedef int __swblk_t;
typedef int __key_t;
typedef int __clockid_t;
typedef void * __timer_t;
typedef void *_dwarfhpp_anon_3bc;
typedef int __blksize_t;
typedef int __blkcnt_t;
typedef __quad_t __blkcnt64_t;
typedef unsigned int __fsblkcnt_t;
typedef __u_quad_t __fsblkcnt64_t;
typedef unsigned int __fsfilcnt_t;
typedef __u_quad_t __fsfilcnt64_t;
typedef int __ssize_t;
typedef __off64_t __loff_t;
typedef __quad_t* __qaddr_t;
typedef __quad_t _dwarfhpp_anon_47d;
typedef char* __caddr_t;
typedef char _dwarfhpp_anon_494;
typedef int __intptr_t;
typedef unsigned int __socklen_t;
typedef struct _IO_FILE FILE;
struct _IO_FILE { 

	int _flags; // offset: 0
	char* _IO_read_ptr __attribute__((aligned(1))); // offset: 4
	char* _IO_read_end __attribute__((aligned(1))); // offset: 8
	char* _IO_read_base __attribute__((aligned(1))); // offset: 12
	char* _IO_write_base __attribute__((aligned(1))); // offset: 16
	char* _IO_write_ptr __attribute__((aligned(1))); // offset: 20
	char* _IO_write_end __attribute__((aligned(1))); // offset: 24
	char* _IO_buf_base __attribute__((aligned(1))); // offset: 28
	char* _IO_buf_end __attribute__((aligned(1))); // offset: 32
	char* _IO_save_base __attribute__((aligned(1))); // offset: 36
	char* _IO_backup_base __attribute__((aligned(1))); // offset: 40
	char* _IO_save_end __attribute__((aligned(1))); // offset: 44
	struct _IO_marker* _markers __attribute__((aligned(1))); // offset: 48
	struct _IO_FILE* _chain __attribute__((aligned(1))); // offset: 52
	int _fileno __attribute__((aligned(1))); // offset: 56
	int _flags2 __attribute__((aligned(1))); // offset: 60
	__off_t _old_offset __attribute__((aligned(1))); // offset: 64
	short unsigned int _cur_column __attribute__((aligned(1))); // offset: 68
	char _vtable_offset __attribute__((aligned(1))); // offset: 70
	char _shortbuf[1]  __attribute__((aligned(1))); // offset: 71
	_IO_lock_t* _lock __attribute__((aligned(1))); // offset: 72
	__off64_t _offset __attribute__((aligned(1))); // offset: 76
	void * __pad1 __attribute__((aligned(1))); // offset: 84
	void * __pad2 __attribute__((aligned(1))); // offset: 88
	void * __pad3 __attribute__((aligned(1))); // offset: 92
	void * __pad4 __attribute__((aligned(1))); // offset: 96
	size_t __pad5 __attribute__((aligned(1))); // offset: 100
	int _mode __attribute__((aligned(1))); // offset: 104
	char _unused2[40]  __attribute__((aligned(1))); // offset: 108
	
} __attribute__((packed));
typedef struct _IO_FILE __FILE;
typedef int _dwarfhpp_wchar_t;
typedef unsigned int wint_t;
union _dwarfhpp_anon_77b { 

	wint_t __wch __attribute__((aligned(sizeof(int)))); // no DW_AT_data_member_location, so it's a guess
	char __wchb[4]  __attribute__((aligned(sizeof(int)))); // no DW_AT_data_member_location, so it's a guess
	
};
typedef char _dwarfhpp_anon_79f[4];
struct _dwarfhpp_anon_7af { 

	int __count; // offset: 0
	_dwarfhpp_anon_77b __value __attribute__((aligned(1))); // offset: 4
	
} __attribute__((packed));
typedef _dwarfhpp_anon_7af __mbstate_t;
struct _dwarfhpp_anon_7ef { 

	__off_t __pos; // offset: 0
	__mbstate_t __state __attribute__((aligned(1))); // offset: 4
	
} __attribute__((packed));
typedef _dwarfhpp_anon_7ef _G_fpos_t;
struct _dwarfhpp_anon_827 { 

	__off64_t __pos; // offset: 0
	__mbstate_t __state __attribute__((aligned(1))); // offset: 8
	
} __attribute__((packed));
typedef _dwarfhpp_anon_827 _G_fpos64_t;
enum _dwarfhpp_anon_861 { 

	__GCONV_OK, 
	__GCONV_NOCONV, 
	__GCONV_NODB, 
	__GCONV_NOMEM, 
	__GCONV_EMPTY_INPUT, 
	__GCONV_FULL_OUTPUT, 
	__GCONV_ILLEGAL_INPUT, 
	__GCONV_INCOMPLETE_INPUT, 
	__GCONV_ILLEGAL_DESCRIPTOR, 
	__GCONV_INTERNAL_ERROR

};
enum _dwarfhpp_anon_93c { 

	__GCONV_IS_LAST, 
	__GCONV_IGNORE_ERRORS

};
typedef int(*__gconv_fct)(struct __gconv_step*, struct __gconv_step_data*, const unsigned char**, const unsigned char*, unsigned char**, size_t*, int, int);
typedef int(*_dwarfhpp_anon_982)(struct __gconv_step*, struct __gconv_step_data*, const unsigned char**, const unsigned char*, unsigned char**, size_t*, int, int);
typedef struct __gconv_step _dwarfhpp_anon_9bb;
struct __gconv_step { 

	struct __gconv_loaded_object* __shlib_handle; // offset: 0
	const char* __modname __attribute__((aligned(1))); // offset: 4
	int __counter __attribute__((aligned(1))); // offset: 8
	char* __from_name __attribute__((aligned(1))); // offset: 12
	char* __to_name __attribute__((aligned(1))); // offset: 16
	__gconv_fct __fct __attribute__((aligned(1))); // offset: 20
	__gconv_btowc_fct __btowc_fct __attribute__((aligned(1))); // offset: 24
	__gconv_init_fct __init_fct __attribute__((aligned(1))); // offset: 28
	__gconv_end_fct __end_fct __attribute__((aligned(1))); // offset: 32
	int __min_needed_from __attribute__((aligned(1))); // offset: 36
	int __max_needed_from __attribute__((aligned(1))); // offset: 40
	int __min_needed_to __attribute__((aligned(1))); // offset: 44
	int __max_needed_to __attribute__((aligned(1))); // offset: 48
	int __stateful __attribute__((aligned(1))); // offset: 52
	void * __data __attribute__((aligned(1))); // offset: 56
	
} __attribute__((packed));
typedef struct __gconv_step_data _dwarfhpp_anon_b20;
struct __gconv_step_data { 

	unsigned char* __outbuf; // offset: 0
	unsigned char* __outbufend __attribute__((aligned(1))); // offset: 4
	int __flags __attribute__((aligned(1))); // offset: 8
	int __invocation_counter __attribute__((aligned(1))); // offset: 12
	int __internal_use __attribute__((aligned(1))); // offset: 16
	__mbstate_t* __statep __attribute__((aligned(1))); // offset: 20
	__mbstate_t __state __attribute__((aligned(1))); // offset: 24
	struct __gconv_trans_data* __trans __attribute__((aligned(1))); // offset: 32
	
} __attribute__((packed));
typedef const unsigned char* _dwarfhpp_anon_be7;
typedef const unsigned char _dwarfhpp_anon_bed;
typedef const unsigned char _dwarfhpp_anon_bf3;
typedef unsigned char* _dwarfhpp_anon_bf8;
typedef unsigned char _dwarfhpp_anon_bfe;
typedef size_t _dwarfhpp_anon_c04;
typedef wint_t(*__gconv_btowc_fct)(struct __gconv_step*, unsigned char);
typedef wint_t(*_dwarfhpp_anon_c23)(struct __gconv_step*, unsigned char);
typedef int(*__gconv_init_fct)(struct __gconv_step*);
typedef int(*_dwarfhpp_anon_c56)(struct __gconv_step*);
typedef void (*__gconv_end_fct)(struct __gconv_step*);
typedef void (*_dwarfhpp_anon_c83)(struct __gconv_step*);
typedef int(*__gconv_trans_fct)(struct __gconv_step*, struct __gconv_step_data*, void *, const unsigned char*, const unsigned char**, const unsigned char*, unsigned char**, size_t*);
typedef int(*_dwarfhpp_anon_cae)(struct __gconv_step*, struct __gconv_step_data*, void *, const unsigned char*, const unsigned char**, const unsigned char*, unsigned char**, size_t*);
typedef int(*__gconv_trans_context_fct)(void *, const unsigned char*, const unsigned char*, unsigned char*, unsigned char*);
typedef int(*_dwarfhpp_anon_d08)(void *, const unsigned char*, const unsigned char*, unsigned char*, unsigned char*);
typedef int(*__gconv_trans_query_fct)(const char*, const char***, size_t*);
typedef int(*_dwarfhpp_anon_d51)(const char*, const char***, size_t*);
typedef const char _dwarfhpp_anon_d71;
typedef const char _dwarfhpp_anon_d77;
typedef const char** _dwarfhpp_anon_d7c;
typedef const char* _dwarfhpp_anon_d82;
typedef int(*__gconv_trans_init_fct)(void **, const char*);
typedef int(*_dwarfhpp_anon_da6)(void **, const char*);
typedef void * _dwarfhpp_anon_dc1;
typedef void (*__gconv_trans_end_fct)(void *);
typedef void (*_dwarfhpp_anon_de4)(void *);
struct __gconv_trans_data { 

	__gconv_trans_fct __trans_fct; // offset: 0
	__gconv_trans_context_fct __trans_context_fct __attribute__((aligned(1))); // offset: 4
	__gconv_trans_end_fct __trans_end_fct __attribute__((aligned(1))); // offset: 8
	void * __data __attribute__((aligned(1))); // offset: 12
	struct __gconv_trans_data* __next __attribute__((aligned(1))); // offset: 16
	
} __attribute__((packed));
typedef struct __gconv_trans_data _dwarfhpp_anon_e7f;
struct __gconv_loaded_object { 


} __attribute__((packed));
typedef struct __gconv_loaded_object _dwarfhpp_anon_e9d;
typedef __mbstate_t _dwarfhpp_anon_ea3;
struct __gconv_info { 

	size_t __nsteps; // offset: 0
	struct __gconv_step* __steps __attribute__((aligned(1))); // offset: 4
	struct __gconv_step_data __data[]  __attribute__((aligned(1))); // offset: 8
	
} __attribute__((packed));
typedef struct __gconv_step_data _dwarfhpp_anon_ef2[];
typedef struct __gconv_info* __gconv_t;
typedef struct __gconv_info _dwarfhpp_anon_f12;
struct _dwarfhpp_anon_f18 { 

	struct __gconv_info __cd; // offset: 0
	struct __gconv_step_data __data __attribute__((aligned(1))); // offset: 8
	
} __attribute__((packed));
union _dwarfhpp_anon_f3e { 

	struct __gconv_info __cd __attribute__((aligned(sizeof(int)))); // no DW_AT_data_member_location, so it's a guess
	_dwarfhpp_anon_f18 __combined __attribute__((aligned(sizeof(int)))); // no DW_AT_data_member_location, so it's a guess
	
};
typedef _dwarfhpp_anon_f3e _G_iconv_t;
typedef short int _G_int16_t;
typedef int _G_int32_t;
typedef short unsigned int _G_uint16_t;
typedef unsigned int _G_uint32_t;
typedef char* __gnuc_va_list;
typedef char _dwarfhpp_anon_fd7;
typedef int _IO_lock_t;
struct _IO_marker { 

	struct _IO_marker* _next; // offset: 0
	struct _IO_FILE* _sbuf __attribute__((aligned(1))); // offset: 4
	int _pos __attribute__((aligned(1))); // offset: 8
	
} __attribute__((packed));
typedef struct _IO_marker _dwarfhpp_anon_102e;
typedef struct _IO_FILE _dwarfhpp_anon_1034;
enum __codecvt_result { 

	__codecvt_ok, 
	__codecvt_partial, 
	__codecvt_error, 
	__codecvt_noconv

};
typedef char _dwarfhpp_anon_109c[1];
typedef _IO_lock_t _dwarfhpp_anon_10ac;
typedef char _dwarfhpp_anon_10b2[40];
typedef struct _IO_FILE _IO_FILE;
typedef __ssize_t(*__io_read_fn)(void *, char*, size_t);
typedef __ssize_t(*__io_write_fn)(void *, const char*, size_t);
typedef int(*__io_seek_fn)(void *, __off64_t*, int);
typedef __off64_t _dwarfhpp_anon_115c;
typedef int(*__io_close_fn)(void *);
typedef _G_fpos_t fpos_t;
extern "C" { char* wider_still(
int _dwarfhpp_arg_a, int _dwarfhpp_arg_b, int _dwarfhpp_arg_c
);}
extern "C" { char* narrow(
int _dwarfhpp_arg_a
);}
extern "C" { char* narrowish(
int _dwarfhpp_arg_a, int _dwarfhpp_arg_b
);}
extern "C" { char* not_always(
int _dwarfhpp_arg_a
);}
extern "C" { char* take_string(
char* _dwarfhpp_arg_s
);}
typedef char _dwarfhpp_anon_1301[4096];
struct _IO_jump_t { 


} __attribute__((packed));
struct _IO_FILE_plus { 


} __attribute__((packed));
