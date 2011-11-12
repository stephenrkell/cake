exists elf_archive("rump.a")  kfs;
exists elf_archive("puffs.a") puffs;
derive elf_archive puffs_inst = instantiate(puffs, puffs_ops, pops, "puffs");
/* instantiate args: (component, structure type, structure name, symbol prefix) */
puffs_inst
{
	declare 
    {
        /*puffs_fs_blah : (_) => _;*/ // FIXME: <-- was this something?
        puffs_fs_fhtonode : (_, _, _, out puffs_newinfo as puffs_full_newinfo) => _;
        puffs_node_lookup : (_, _,    out puffs_newinfo as puffs_full_newinfo, _) => _;
    }
}

derive elf_archive("user_kfs.a") fs = link[
	puffs, 	// already created pops in this module, meaning it
	kfs		// will declare an UND symbol for each el
]			// of puffs_ops, and a table called pops
			// note UNDs are WEAK,=> null on no-impl!
{ puffs <--> kfs {
// FIXME: can omit arrow-block if only two components? 
// Perhaps not -- force the user to define the order? Or just say "use list order"?

	pattern /puffs_fs_(.*)/ { names (mount: _) } <--> rump_vfs_\\1 
    												{ names (mount: _) };
    // And perhaps we want to insist that names are a stronger "semantic tag"
    // than positions, so we don't want to match positions willy-nilly. YES.

    // So we can explicitly describe positional correspondences, using
    // the correspondence notation (somehow), but they don't get formed 
    // automatically.
    
    /* The above pattern is supposed to generate (at most):
     * puffs_fs_{umount,statvfs,sync,fhtonode,nodetofh,suspend} */
    /* This one generates 
     * puffs_node_{lookup,create,mknod,open,close,access,getattr,setattr,
     *  fsync,mmap,seek,remove,link,rename,mkdir,rmdir,symlink,readdir,
     *  readlink,read,write,inactive,reclaim}.
     */
	pattern /puffs_node_(.*)/ { names (mount: _, cookie: _) } 
                        (/*.*/cookie as vnode_unlocked ptr)  
    	 <--> RUMP_VOP_\\U\\1\\E { names (cookie: _) };        

    values puffs_usermount (puffs_getspecific(this))--> mount;

    //values puffs_cred (cred_create(this))--> kauth_cred;
    //values puffs_cred <--(cred_destroy(this)) kauth_cred;
	values puffs_cred ({
	  puffs_cred_getuid(this, out uid) ;| let uid = 0;
	  puffs_cred_getgid(this, out gid) ;| let gid = 0;
	  puffs_cred_getgroups(pcr, out groups[NGROUPS], out ngroups)})
	  -->(rump_cred_create(uid, gid, ngroups, groups)) kauth_cred;
	values puffs_cred <--(rump_cred_destroy(this)) kauth_cred;

    values vnode_unlocked -->({RUMP_VOP_LOCK(that, LK_EXCLUSIVE); that}) vnode;
    values vnode_unlocked <--(RUMP_VOP_UNLOCK(that, 0)) vnode;

    //values puffs_cn (makecn(this))--> component_name;
    //values puffs_cn <--(freecn(this, 0)) component_name;
	values puffs_cn -->(rump_makecn(that->pcn_nameiop, that->pcn_flags, 
	  that->pcn_name, that->pcn_namelen, that->pcn_cred, curlwp)) component_name;
	values puffs_cn <--(rump_freecn(this, RUMPCN_FREECRED)) component_name;

    values vnode_bump_no_unlk -->({RUMP_VOP_LOCK(that, LK_EXCLUSIVE); rump_vp_incref(that); that}) vnode;
    values vnode_bump_no_unlk <--({assert(RUMP_VOP_ISLOCKED(that) == 0); that}) vnode;

    puffs_node_create(mount, vn as vnode_bump_no_unlk, ni, cn, vap) 
    --> RUMP_VOP_CREATE(vn, ni, cn, vap);
    puffs_node_mknod(mount, vn as vnode_bump_no_unlk, ni, cn, vap)
    --> RUMP_VOP_MKNOD(vn, ni, cn, vap);
    puffs_node_remove(mount, vn as vnode_bump_no_unlk, targ_vn as vnode_bump_no_unlk, cn)
    --> RUMP_VOP_REMOVE(vn, targ_vn, cn);
    puffs_node_link(mount, vn as vnode_bump_no_unlk, targ_vn as void ptr, cn)
    --> RUMP_VOP_LINK(vn, targ_vn, cn);
    puffs_node_rename(mount, srcdir_vn as vnode_bump_no_unlk, src_vn as vnode_bump_no_unlk, src_cn,
                            targdir_vn as vnode_bump_no_unlk, targ_vn as vnode_bump_no_unlk, targ_cn)
    --> RUMP_VOP_RENAME(srcdir_vn, src_vn, src_cn, targdir_vn, targ_vn, targ_cn);
    // note automatic null handling: when passing null ptrs, no corresps for ptd-to are run
    puffs_node_mkdir(mount, vn as vnode_bump_no_unlk, ni, cn, vap)
    --> RUMP_VOP_MKDIR(vn, out ni, cn, vap);
    puffs_node_rmdir(mount, vn as vnode_bump_no_unlk, targ_vn as vnode_bump_no_unlk, cn)
    --> RUMP_VOP_RMDIR(vn, targ_vn, cn);
    puffs_node_symlink(mount, vn as vn_bump_no_unlk, ni, src_cn, vap, linktgt)
    --> RUMP_VOP_SYMLINK(vn, out ni, cn, vap, linktgt);


    // override name-matching rules
    puffs_fs_fhtonode(mount, cookie, fid, _, _, out newvp as puffs_full_newinfo) <--> 
      rump_vfs_fhtovp(mount, fid, out newvp);    
    puffs_node_lookup(mount, cookie, out newvp as puffs_full_newinfo, cn)
	    <--> RUMP_VOP_LOOKUP(cookie, out newvp, cn);

    values puffs_newinfo ({puffs_newinfo_setcookie(this, that); this})
						    <--(RUMP_VOP_UNLOCK(this, 0)) vnode;
    // Some calls return a fuller set of newinfo
    values puffs_full_newinfo ({puffs_newinfo_setcookie(this, that);
							    puffs_newinfo_setvtype(this, vtype);
                                puffs_newinfo_setsize(this, vsize);
                                 puffs_newinfo_setrdev(this, rdev); this}) 
	    <-- ({let (vtype, vsize, rdev) = rump_getvninfo(this); this}) vnode;


    values vnode_lkshared -->({RUMP_VOP_LOCK(that, LK_SHARED); that}) vnode;
    values vnode_lkshared <--({RUMP_VOP_UNLOCK(that, 0); that}) vnode;

	// this is a special sort of rule, defining a constructor... hmm
    uio_outbuf: 
    values /*uio_outbuf*/ (buf: /*(invalid*/ uint8_t/*)*/[] ptr, resid: size_t, off: const off_t)
     <--> uio
    { void -->(rump_uio_setup(that->buf, that->resid, that->offset, RUMPUIO_READ)) void; };

    values uio_outresult <--(rump_uio_free(this)) uio;
    values uio_outresult_subtract (*this - that)<--(rump_uio_free(this)) uio;
    values uio_outres_len_off 
        <--({rump_uio_getresid(that->resid); 
            rump_uio_getoff(that->readoff); 
            rump_uio_free(this)}) uio;

    puffs_node_read(mount, vn as vnode_lkshared, 
        uio as uio_outbuf(buf, *resid, offset), 
        _, inout resid out_as uio_outresult, cr, ioflag) // {in,out}_as mean interp_{in,out}_as...
    --> RUMP_VOP_READ(vn, uio, ioflag, cr);

    puffs_node_readlink(mount, vn, cr, uio as uio_outbuf(linkname, linklen, 0), 
        inout linkname out_as uio_outresult_subtract)
    --> RUMP_VOP_READLINK(vn, uio, cr);

    puffs_node_readdir(mount, vn as vnode_lkshared, uio as uio_outbuf(dent, *reslen, *readoff), 
         /* readoff */ _ out_as uio_outres_len_off(readoff, reslen), /* reslen */ _ , 
        cr, inout eofflag, 
        out cookies as (invalid off_t)[ncookies] , /* ncookies */ _
    )
        // TODO: check strange bug in p2k code: no size given for "cookies" buffer!
    --> RUMP_VOP_READDIR(vn, uio, cr, eofflag, cookies, if cookies == null then null else out ncookies);
    // NOTE: Cake has to notice the mismtach between 
    // (out _)[] and caller_frees(free) out(_ []) 
    // and insert the necessary memcpy/free!

// FIXME: this rule needs some syntax -- use a many-to-many value correspondence instead?
   uio_inbuf: values /*uio_inbuf*/ (buf: /*(invalid */uint8_t/*)*/[] ptr, resid: size_t, off: const off_t) 
    <--> uio
   { void <--(rump_uio_setup(that->buf, that->resid, that->offset, RUMPUIO_WRITE)) uio; };

    values uio_inresult <--(rump_uio_free(this)) uio;

    puffs_node_write(mount, vn, uio as uio_inbuf(buf, *resid, offset), 
	    _, _ out_as uio_inresult, cr, ioflag)
    --> RUMP_VOP_WRITE(vn, uio, ioflag, cr);

    // "inactive" notification requires special action in rump
    puffs_node_inactive(mount, vn as vn_no_lk) --> {
	    rump_vp_interlock(vn);
	    RUMP_VOP_PUTPAGES(vn, 0, 0, PGO_ALLPAGES);
        RUMP_VOP_LOCK(vn, LK_EXCLUSIVE);
        RUMP_VOP_INACTIVE(vn, out recycle) }--
     <--
     --{ if recycle then puffs_setback(
 	    puffs_cc_getcc(mount, PUFFS_SETBACK_NOREF_N1
       )) else void };

    // reclaim maps to call with non-analogous name
    puffs_node_reclaim(mount, vn as vn_no_lk) --> { rump_vp_recycle_nokidding(vn); void };

    // unmount requires special action
    // FIXME: instead of yet more syntax below, could use association?
    puffs_fs_unmount(mount, flags) (let rvp in_as vn_no_lk = puffs_getroot(mount)->pn_data)--> {
	    rump_vp_recycle_nokidding(rvp);
        rump_vfs_unmount(mount, flags) ;|
            { rump_vfs_root(mount, out rvp2, 0); assert(success && rvp == rvp2); } };

    // puffs sync needs two calls in rump        
    puffs_fs_sync(mount, waitfor, cr) --> { rump_vfs_sync(mount, waitfor, cr); rump_bioops_sync(); };

    // fhtonode and nodetofh map to non-analogous names
    puffs_fs_fhtonode(mount, fid, _, out ni as puffs_full_newinfo) --> 
	    rump_vfs_fhtovp(mount, fid, ni);

    puffs_fs_nodetofh(mount, vn as vnode_nolk, fid, fidsize) --> rump_vfs_vptofh(vn, fid, fidsize);	

} };
