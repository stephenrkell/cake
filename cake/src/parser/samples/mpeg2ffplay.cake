exists elf_reloc("libmpeg2play.o") mpeg2play;
exists elf_external_sharedlib("avcodec") avcodec;
exists elf_external_sharedlib("avformat") avformat;
exists elf_external_sharedlib("avutil") avutil;
alias any [avcodec, avformat, avutil] ffmpeg;

derive elf_reloc("mpeg2play2ffmpeg.o") program = link[
  mpeg2play, ffmpeg
] {
 mpeg2play <--> ffmpeg
 {
  fopen (fname, "rb")[0] --> av_open_input_file(out _, fname); 
  values FILE <--> AVFormatContext /*{}*/;
  mpeg2_init() --> { avcodec_init();
                     av_register_all() }
               <--
               {new mpeg2_dec_s};

   let f = fopen(fname, "rb"), ...,
   let dec = mpeg2_init(), ...,
   mpeg2_get_info(dec) --> { 
           av_find_stream_info(f) // in-place update to f
       ;& let dec...vid_idx = find( // standard algorithm
              f->streams, 
              fn x => x->codec->codec_type // lambda!
                        == CODEC_TYPE_VIDEO )
       ;& let codec_ctxt = f->streams[dec...vid_idx]
       ;& let codec = avcodec_find_decoder(
                               codec_ctxt->codec_id)
       ;& avcodec_open(codec_ctxt, codec)
       ;& codec_ctxt };
  
  values (dec: mpeg2_dec_s, info: mpeg2_info_s, 
  sequence: mpeg2_sequence_s, fbuf: mpeg2_fbuf_s)
   <--> (ctxt: AVCodecContext, vid_idx: int, 
        p: AVPacket, s: AVStream, codec: AVCodec)
  { 
    // ensure an AVPacket exists, on any flow L-to-R
    void -->?(new AVPacket tie ctxt) p;
    
    // picture dimensions are in sequence and ctxt
    sequence <--> ctxt {
     // width and height done automatically
     display_width <-- width;
     display_height <-- height;  // here we assume a
     chroma_width <-- width / 2; // 4:2:2 pixel format
     chroma_height <-- height / 2; }; 
     
     // info.sequence always points to sequence object
     info.sequence (&sequence)<--? void;
     
     // special conversion required for buffers
     fbuf <--> frame {
      buf[0] as packed_luma_line[height] ptr
       <--> data[0] as padded_line[ctxt.height] ptr;
      buf[1] as packed_chroma_line[chroma_height] ptr
       <--> data[1] as padded_line[ctxt.height / 2] ptr;
      buf[2] as packed_chroma_line[chroma_height] ptr
       <--> data[2] as padded_line[ctxt.height / 2] ptr;
     };
  };
  values packed_luma_line <-- padded_line {
   void (memcpy(this, that, display_width))<-- void; };
  values packed_chroma_line <-- padded_line {
   void (memcpy(this, that, chroma_width))<-- void; };
   
  /* The loop in ffmpeg proceeds frame-by-frame, 
   * whereas in libmpeg2 each iteration might yield
   * zero frames (in the STATE_BUFFER case) *or* 
   * one or more frames (in the STATE_SLICE} case). 
   * Solve this by ensuring that each iteration 
   * yields exactly one frame---a case supported 
   * by both library and client. */
  /*(*/mpeg2_parse(dec)/*)*/[0]   --> { void } // HACK: syntax simplification
                          <--
             STATE_BUFFER;
  /* Notice use of [0]: "the first call to 
   * mpeg2_parse()
   * *on a given dec, for all dec* 
   * ... returns STATE_BUFFER */

  /* Reading from the input file handle must also
   * be mapped to an ffmpeg library call. Since success
   * of fread() entails a return value of nmemb, we 
   * must return this, irrespective of the size of the
   * frame actually read. This is a rare example where 
   * error-reporting conventions must be explicitly 
   * satisfied in stub code. */

  let f = fopen (fname, "rb")[0], ...,
  let dec = mpeg2_init(), ...,
  fread(_, _, nmemb, f) --> 
    { { av_read_frame(dec...packet, f) ;& nmemb } ;| 0; };

  /* Since ffmpeg handles input buffering for us, no
   * action is required on a call to mpeg2_buffer(). */
  mpeg2_buffer(_, /*buf*/_, /*buf + siz*/_) --> { void };
  
  /* The client calls mpeg2_parse() to request decoded
   * frames. This translates to a call to 
   * avcodec_decode_video(). Since our earlier call to 
   * av_read_frame() may have returned a frame from a 
   * different stream (e.g. an audio stream in the same 
   * file), we have two cases to consider. These map
   * directly to the libmpeg2 constants STATE_BUFFER 
   * ("must read more data") and STATE_SLICE ("one or 
   * more decoded frames available"), distinguished by
   * an if--then--else. */
  f <- fopen (fname, "rb")[0], ...,
  dec <- mpeg2_init(), ...,
  size <- fread(_, _, nmemb, f),
  mpeg2_parse(dec) --> { let frame_avail = (
    if dec...packet.stream_index == dec...vid_idx
    then  { av_free(dec...frame); // this is null-safe
          let dec...frame = avcodec_alloc_frame(); 
          avcodec_decode_video2(dec...codec_ctxt,
            frame, out got_picture, dec...packet);
          true }
    else false )
    }--
    <--
    --{ if frame_avail then STATE_SLICE 
                       else STATE_BUFFER };
  /* Notice the special reverse-arrow syntax for 
   * returning. Moreover, the special "--{" ("continuing")
   * syntax retains all name bindings from the preceding 
   * stub. */

  /* Finally, we relate the state tear-down calls of the
   * two interfaces. */
  mpeg2_close(dec) --> { av_free(dec...picture); 
                         avcodec_close(dec...codec);
                         av_close_input_file(dec...ic); }
                   <--
   { delete dec };
 } // end mpeg2play <--> ffmpeg
}; // end derive
