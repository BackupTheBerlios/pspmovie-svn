/*
 * FFmpeg main
 * Copyright (c) 2000-2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>

#include <ffmpeg/avcodec.h>
#include <ffmpeg/avformat.h>
#include <ffmpeg/avio.h>
#include <ffmpeg/common.h>

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))
#define ABS(a) ((a) >= 0 ? (a) : (-(a)))
static inline int clip(int a, int amin, int amax)
{
    if (a < amin)
        return amin;
    else if (a > amax)
        return amax;
    else
        return a;
}



#ifndef CONFIG_WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <sys/resource.h>
#include <signal.h>
#endif

#include <time.h>

#undef NDEBUG
#include <assert.h>

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

#include "ffmpeg_opt.h"
#include "ffmpeg_cmdutils.h"

//
// Binding to cpp glue. When callback returns 0, set termination signal. If
// we want to pass signal to cpp - run callback with frame=-1
// 
void *cpp_passed_ptr;
int (*cpp_callback)(void *ptr, int frame) = 0;

int received_sigterm = 0;

void av_abort()
{
	received_sigterm = 1;
}


/* select an input stream for an output stream */
typedef struct AVStreamMap {
    int file_index;
    int stream_index;
    int sync_file_index;
    int sync_stream_index;
} AVStreamMap;

/** select an input file for an output file */
typedef struct AVMetaDataMap {
    int out_file;
    int in_file;
} AVMetaDataMap;

extern const OptionDef options[];

static int opt_default(const char *opt, const char *arg);

#define MAX_FILES 20

static AVFormatContext *input_files[MAX_FILES];
static int64_t input_files_ts_offset[MAX_FILES];
static int nb_input_files = 0;

static AVFormatContext *output_files[MAX_FILES];
static int nb_output_files = 0;

static AVStreamMap stream_maps[MAX_FILES];
static int nb_stream_maps;

static AVMetaDataMap meta_data_maps[MAX_FILES];
static int nb_meta_data_maps;

static AVInputFormat *file_iformat;
static AVOutputFormat *file_oformat;
static AVImageFormat *image_format;
static int frame_width  = 0;
static int frame_height = 0;
static float frame_aspect_ratio = 0;
static enum PixelFormat frame_pix_fmt = PIX_FMT_NONE;
static int frame_padtop  = 0;
static int frame_padbottom = 0;
static int frame_padleft  = 0;
static int frame_padright = 0;
static int padcolor[3] = {16,128,128}; /* default to black */
static int frame_topBand  = 0;
static int frame_bottomBand = 0;
static int frame_leftBand  = 0;
static int frame_rightBand = 0;
static int max_frames[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
static int frame_rate = 25;
static int frame_rate_base = 1;
static int video_bit_rate = 200*1000;
static int video_bit_rate_tolerance = 4000*1000;
static float video_qscale = 0;
static int video_qmin = 2;
static int video_qmax = 31;
static int video_lmin = 2*FF_QP2LAMBDA;
static int video_lmax = 31*FF_QP2LAMBDA;
static int video_mb_lmin = 2*FF_QP2LAMBDA;
static int video_mb_lmax = 31*FF_QP2LAMBDA;
static int video_qdiff = 3;
static float video_qblur = 0.5;
static float video_qsquish = 0.0;
static float video_qcomp = 0.5;
static uint16_t *intra_matrix = NULL;
static uint16_t *inter_matrix = NULL;
#if 0 //experimental, (can be removed)
static float video_rc_qsquish=1.0;
static float video_rc_qmod_amp=0;
static int video_rc_qmod_freq=0;
#endif
static char *video_rc_override_string=NULL;
static char *video_rc_eq="tex^qComp";
static int video_rc_buffer_size=0;
static float video_rc_buffer_aggressivity=1.0;
static int video_rc_max_rate=0;
static int video_rc_min_rate=0;
static float video_rc_initial_cplx=0;
static float video_b_qfactor = 1.25;
static float video_b_qoffset = 1.25;
static float video_i_qfactor = -0.8;
static float video_i_qoffset = 0.0;
static int video_intra_quant_bias= FF_DEFAULT_QUANT_BIAS;
static int video_inter_quant_bias= FF_DEFAULT_QUANT_BIAS;
static int me_method = ME_EPZS;
static int video_disable = 0;
static int video_discard = 0;
static int video_codec_id = CODEC_ID_NONE;
static int video_codec_tag = 0;
static int same_quality = 0;
static int b_frames = 0;
static int pre_me = 0;
static int do_deinterlace = 0;
static int workaround_bugs = FF_BUG_AUTODETECT;
static int packet_size = 0;
static int error_rate = 0;
static int strict = 0;
static int top_field_first = -1;
static int sc_threshold = 0;
static int me_threshold = 0;
static int mb_threshold = 0;
static int intra_dc_precision = 8;
static int me_penalty_compensation= 256;
static int frame_skip_threshold= 0;
static int frame_skip_factor= 0;
static int frame_skip_exp= 0;
extern int loop_input; /* currently a hack */
static int loop_output = AVFMT_NOOUTPUTLOOP;
static int genpts = 0;
static int qp_hist = 0;

static int gop_size = 12;
static int intra_only = 0;
static int audio_sample_rate = 44100;
static int audio_bit_rate = 64000;
#define QSCALE_NONE -99999
static float audio_qscale = QSCALE_NONE;
static int audio_disable = 0;
static int audio_channels = 1;
static int audio_codec_id = CODEC_ID_NONE;
static int audio_codec_tag = 0;
static char *audio_language = NULL;

static int subtitle_codec_id = CODEC_ID_NONE;
static char *subtitle_language = NULL;

static int mux_rate= 0;
static int mux_packet_size= 0;
static float mux_preload= 0.5;
static float mux_max_delay= 0.7;

static int64_t recording_time = 0;
static int64_t start_time = 0;
static int64_t rec_timestamp = 0;
static int64_t input_ts_offset = 0;
static int file_overwrite = 0;
static char *str_title = NULL;
static char *str_author = NULL;
static char *str_copyright = NULL;
static char *str_comment = NULL;
static int do_benchmark = 0;
static int do_hex_dump = 0;
static int do_pkt_dump = 0;
static int do_psnr = 0;
static int do_vstats = 0;
static int do_pass = 0;
static char *pass_logfilename = NULL;
static int audio_stream_copy = 0;
static int video_stream_copy = 0;
static int subtitle_stream_copy = 0;
static int video_sync_method= 1;
static int audio_sync_method= 0;
static int copy_ts= 0;
static int opt_shortest = 0; //
static int video_global_header = 0;

static int rate_emu = 0;

#ifdef CONFIG_BKTR
static char *video_grab_format = "bktr";
#else
static char *video_grab_format = "video4linux";
#endif
static char *video_device = NULL;
static char *grab_device = NULL;
static int  video_channel = 0;
static char *video_standard = "ntsc";

static char *audio_grab_format = "audio_device";
static char *audio_device = NULL;
static int audio_volume = 256;

static int using_stdin = 0;
static int using_vhook = 0;
static int verbose = 1;
static int thread_count= 1;
static int me_range = 0;
static int64_t video_size = 0;
static int64_t audio_size = 0;
static int64_t extra_size = 0;
static int nb_frames_dup = 0;
static int nb_frames_drop = 0;
static int input_sync;
static int limit_filesize = 0; //

static int pgmyuv_compatibility_hack=0;

const char **opt_names=NULL;
int opt_name_count=0;
AVCodecContext *avctx_opts;


#define DEFAULT_PASS_LOGFILENAME "ffmpeg2pass"

struct AVInputStream;

typedef struct AVOutputStream {
    int file_index;          /* file index */
    int index;               /* stream index in the output file */
    int source_index;        /* AVInputStream index */
    AVStream *st;            /* stream in the output file */
    int encoding_needed;     /* true if encoding needed for this stream */
    int frame_number;
    /* input pts and corresponding output pts
       for A/V sync */
    //double sync_ipts;        /* dts from the AVPacket of the demuxer in second units */
    struct AVInputStream *sync_ist; /* input stream to sync against */
    int64_t sync_opts;       /* output frame counter, could be changed to some true timestamp */ //FIXME look at frame_number
    /* video only */
    int video_resample;      /* video_resample and video_crop are mutually exclusive */
    AVFrame pict_tmp;      /* temporary image for resampling */
    ImgReSampleContext *img_resample_ctx; /* for image resampling */

    int video_crop;          /* video_resample and video_crop are mutually exclusive */
    int topBand;             /* cropping area sizes */
    int leftBand;

    int video_pad;           /* video_resample and video_pad are mutually exclusive */
    int padtop;              /* padding area sizes */
    int padbottom;
    int padleft;
    int padright;

    /* audio only */
    int audio_resample;
    ReSampleContext *resample; /* for audio resampling */
    FifoBuffer fifo;     /* for compression: one audio fifo per codec */
    FILE *logfile;
} AVOutputStream;

typedef struct AVInputStream {
    int file_index;
    int index;
    AVStream *st;
    int discard;             /* true if stream data should be discarded */
    int decoding_needed;     /* true if the packets must be decoded in 'raw_fifo' */
    int64_t sample_index;      /* current sample */

    int64_t       start;     /* time when read started */
    unsigned long frame;     /* current frame */
    int64_t       next_pts;  /* synthetic pts for cases where pkt.pts
                                is not defined */
    int64_t       pts;       /* current pts */
    int is_start;            /* is 1 at the start and after a discontinuity */
} AVInputStream;

typedef struct AVInputFile {
    int eof_reached;      /* true if eof reached */
    int ist_index;        /* index of first stream in ist_table */
    int buffer_size;      /* current total buffer size */
    int buffer_size_max;  /* buffer size at which we consider we can stop
                             buffering */
    int nb_streams;       /* nb streams we are aware of */
} AVInputFile;

static int read_ffserver_streams(AVFormatContext *s, const char *filename)
{
    int i, err;
    AVFormatContext *ic;

    err = av_open_input_file(&ic, filename, NULL, FFM_PACKET_SIZE, NULL);
    if (err < 0)
        return err;
    /* copy stream format */
    s->nb_streams = ic->nb_streams;
    for(i=0;i<ic->nb_streams;i++) {
        AVStream *st;

        // FIXME: a more elegant solution is needed
        st = av_mallocz(sizeof(AVStream));
        memcpy(st, ic->streams[i], sizeof(AVStream));
        st->codec = avcodec_alloc_context();
        memcpy(st->codec, ic->streams[i]->codec, sizeof(AVCodecContext));
        s->streams[i] = st;
    }

    av_close_input_file(ic);
    return 0;
}

static double
get_sync_ipts(const AVOutputStream *ost)
{
    const AVInputStream *ist = ost->sync_ist;
    return (double)(ist->pts + input_files_ts_offset[ist->file_index] - start_time)/AV_TIME_BASE;
}

#define MAX_AUDIO_PACKET_SIZE (128 * 1024)

static void do_audio_out(AVFormatContext *s,
                         AVOutputStream *ost,
                         AVInputStream *ist,
                         unsigned char *buf, int size)
{
    uint8_t *buftmp;
    static uint8_t *audio_buf = NULL;
    static uint8_t *audio_out = NULL;
    const int audio_out_size= 4*MAX_AUDIO_PACKET_SIZE;

    int size_out, frame_bytes, ret;
    AVCodecContext *enc= ost->st->codec;

    /* SC: dynamic allocation of buffers */
    if (!audio_buf)
        audio_buf = av_malloc(2*MAX_AUDIO_PACKET_SIZE);
    if (!audio_out)
        audio_out = av_malloc(audio_out_size);
    if (!audio_buf || !audio_out)
        return;               /* Should signal an error ! */

    if(audio_sync_method){
        double delta = get_sync_ipts(ost) * enc->sample_rate - ost->sync_opts
                - fifo_size(&ost->fifo, ost->fifo.rptr)/(ost->st->codec->channels * 2);
        double idelta= delta*ist->st->codec->sample_rate / enc->sample_rate;
        int byte_delta= ((int)idelta)*2*ist->st->codec->channels;

        //FIXME resample delay
        if(fabs(delta) > 50){
            if(ist->is_start){
                if(byte_delta < 0){
                    byte_delta= FFMAX(byte_delta, -size);
                    size += byte_delta;
                    buf  -= byte_delta;
                    if(verbose > 2)
                        fprintf(stderr, "discarding %d audio samples\n", (int)-delta);
                    if(!size)
                        return;
                    ist->is_start=0;
                }else{
                    static uint8_t *input_tmp= NULL;
                    input_tmp= av_realloc(input_tmp, byte_delta + size);

                    if(byte_delta + size <= MAX_AUDIO_PACKET_SIZE)
                        ist->is_start=0;
                    else
                        byte_delta= MAX_AUDIO_PACKET_SIZE - size;

                    memset(input_tmp, 0, byte_delta);
                    memcpy(input_tmp + byte_delta, buf, size);
                    buf= input_tmp;
                    size += byte_delta;
                    if(verbose > 2)
                        fprintf(stderr, "adding %d audio samples of silence\n", (int)delta);
                }
            }else if(audio_sync_method>1){
                int comp= clip(delta, -audio_sync_method, audio_sync_method);
                assert(ost->audio_resample);
                if(verbose > 2)
                    fprintf(stderr, "compensating audio timestamp drift:%f compensation:%d in:%d\n", delta, comp, enc->sample_rate);
//                fprintf(stderr, "drift:%f len:%d opts:%lld ipts:%lld fifo:%d\n", delta, -1, ost->sync_opts, (int64_t)(get_sync_ipts(ost) * enc->sample_rate), fifo_size(&ost->fifo, ost->fifo.rptr)/(ost->st->codec->channels * 2));
                av_resample_compensate(*(struct AVResampleContext**)ost->resample, comp, enc->sample_rate);
            }
        }
    }else
        ost->sync_opts= lrintf(get_sync_ipts(ost) * enc->sample_rate)
                        - fifo_size(&ost->fifo, ost->fifo.rptr)/(ost->st->codec->channels * 2); //FIXME wrong

    if (ost->audio_resample) {
        buftmp = audio_buf;
        size_out = audio_resample(ost->resample,
                                  (short *)buftmp, (short *)buf,
                                  size / (ist->st->codec->channels * 2));
        size_out = size_out * enc->channels * 2;
    } else {
        buftmp = buf;
        size_out = size;
    }

    /* now encode as many frames as possible */
    if (enc->frame_size > 1) {
        /* output resampled raw samples */
        fifo_write(&ost->fifo, buftmp, size_out,
                   &ost->fifo.wptr);

        frame_bytes = enc->frame_size * 2 * enc->channels;

        while (fifo_read(&ost->fifo, audio_buf, frame_bytes,
                     &ost->fifo.rptr) == 0) {
            AVPacket pkt;
            av_init_packet(&pkt);

            ret = avcodec_encode_audio(enc, audio_out, audio_out_size,
                                       (short *)audio_buf);
            audio_size += ret;
            pkt.stream_index= ost->index;
            pkt.data= audio_out;
            pkt.size= ret;
            if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
            pkt.flags |= PKT_FLAG_KEY;
            av_interleaved_write_frame(s, &pkt);

            ost->sync_opts += enc->frame_size;
        }
    } else {
        AVPacket pkt;
        av_init_packet(&pkt);

        ost->sync_opts += size_out / (2 * enc->channels);

        /* output a pcm frame */
        /* XXX: change encoding codec API to avoid this ? */
        switch(enc->codec->id) {
        case CODEC_ID_PCM_S32LE:
        case CODEC_ID_PCM_S32BE:
        case CODEC_ID_PCM_U32LE:
        case CODEC_ID_PCM_U32BE:
            size_out = size_out << 1;
            break;
        case CODEC_ID_PCM_S24LE:
        case CODEC_ID_PCM_S24BE:
        case CODEC_ID_PCM_U24LE:
        case CODEC_ID_PCM_U24BE:
        case CODEC_ID_PCM_S24DAUD:
            size_out = size_out / 2 * 3;
            break;
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            break;
        default:
            size_out = size_out >> 1;
            break;
        }
        ret = avcodec_encode_audio(enc, audio_out, size_out,
                                   (short *)buftmp);
        audio_size += ret;
        pkt.stream_index= ost->index;
        pkt.data= audio_out;
        pkt.size= ret;
        if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
            pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
        pkt.flags |= PKT_FLAG_KEY;
        av_interleaved_write_frame(s, &pkt);
    }
}

static void pre_process_video_frame(AVInputStream *ist, AVPicture *picture, void **bufp)
{
    AVCodecContext *dec;
    AVPicture *picture2;
    AVPicture picture_tmp;
    uint8_t *buf = 0;

    dec = ist->st->codec;

    /* deinterlace : must be done before any resize */
    if (do_deinterlace || using_vhook) {
        int size;

        /* create temporary picture */
        size = avpicture_get_size(dec->pix_fmt, dec->width, dec->height);
        buf = av_malloc(size);
        if (!buf)
            return;

        picture2 = &picture_tmp;
        avpicture_fill(picture2, buf, dec->pix_fmt, dec->width, dec->height);

        if (do_deinterlace){
            if(avpicture_deinterlace(picture2, picture,
                                     dec->pix_fmt, dec->width, dec->height) < 0) {
                /* if error, do not deinterlace */
                av_free(buf);
                buf = NULL;
                picture2 = picture;
            }
        } else {
            if (img_convert(picture2, dec->pix_fmt, picture,
                            dec->pix_fmt, dec->width, dec->height) < 0) {
                /* if error, do not copy */
                av_free(buf);
                buf = NULL;
                picture2 = picture;
            }
        }
    } else {
        picture2 = picture;
    }

    frame_hook_process(picture2, dec->pix_fmt, dec->width, dec->height);

    if (picture != picture2)
        *picture = *picture2;
    *bufp = buf;
}

/* we begin to correct av delay at this threshold */
#define AV_DELAY_MAX 0.100


/* Expects img to be yuv420 */
static void fill_pad_region(AVPicture* img, int height, int width,
        int padtop, int padbottom, int padleft, int padright, int *color) {

    int i, y, shift;
    uint8_t *optr;

    for (i = 0; i < 3; i++) {
        shift = (i == 0) ? 0 : 1;

        if (padtop || padleft) {
            memset(img->data[i], color[i], (((img->linesize[i] * padtop) +
                            padleft) >> shift));
        }

        if (padleft || padright) {
            optr = img->data[i] + (img->linesize[i] * (padtop >> shift)) +
                (img->linesize[i] - (padright >> shift));

            for (y = 0; y < ((height - (padtop + padbottom) - 1) >> shift); y++) {
                memset(optr, color[i], (padleft + padright) >> shift);
                optr += img->linesize[i];
            }
        }

        if (padbottom || padright) {
            optr = img->data[i] + (((img->linesize[i] * (height - padbottom)) - padright) >> shift);
            memset(optr, color[i], (((img->linesize[i] * padbottom) + padright) >> shift));
        }
    }
}

static void do_subtitle_out(AVFormatContext *s,
                            AVOutputStream *ost,
                            AVInputStream *ist,
                            AVSubtitle *sub,
                            int64_t pts)
{
    static uint8_t *subtitle_out = NULL;
    int subtitle_out_max_size = 65536;
    int subtitle_out_size, nb, i;
    AVCodecContext *enc;
    AVPacket pkt;

    if (pts == AV_NOPTS_VALUE) {
        fprintf(stderr, "Subtitle packets must have a pts\n");
        return;
    }

    enc = ost->st->codec;

    if (!subtitle_out) {
        subtitle_out = av_malloc(subtitle_out_max_size);
    }

    /* Note: DVB subtitle need one packet to draw them and one other
       packet to clear them */
    /* XXX: signal it in the codec context ? */
    if (enc->codec_id == CODEC_ID_DVB_SUBTITLE)
        nb = 2;
    else
        nb = 1;

    for(i = 0; i < nb; i++) {
        subtitle_out_size = avcodec_encode_subtitle(enc, subtitle_out,
                                                    subtitle_out_max_size, sub);

        av_init_packet(&pkt);
        pkt.stream_index = ost->index;
        pkt.data = subtitle_out;
        pkt.size = subtitle_out_size;
        pkt.pts = av_rescale_q(av_rescale_q(pts, ist->st->time_base, AV_TIME_BASE_Q) + input_files_ts_offset[ist->file_index], AV_TIME_BASE_Q,  ost->st->time_base);
        if (enc->codec_id == CODEC_ID_DVB_SUBTITLE) {
            /* XXX: the pts correction is handled here. Maybe handling
               it in the codec would be better */
            if (i == 0)
                pkt.pts += 90 * sub->start_display_time;
            else
                pkt.pts += 90 * sub->end_display_time;
        }
        av_interleaved_write_frame(s, &pkt);
    }
}

static int bit_buffer_size= 1024*256;
static uint8_t *bit_buffer= NULL;

static void do_video_out(AVFormatContext *s,
                         AVOutputStream *ost,
                         AVInputStream *ist,
                         AVFrame *in_picture,
                         int *frame_size)
{
    int nb_frames, i, ret;
    AVFrame *final_picture, *formatted_picture;
    AVFrame picture_format_temp, picture_crop_temp;
    uint8_t *buf = NULL, *buf1 = NULL;
    AVCodecContext *enc, *dec;
    enum PixelFormat target_pixfmt;

    avcodec_get_frame_defaults(&picture_format_temp);
    avcodec_get_frame_defaults(&picture_crop_temp);

    enc = ost->st->codec;
    dec = ist->st->codec;

    /* by default, we output a single frame */
    nb_frames = 1;

    *frame_size = 0;

    if(video_sync_method){
        double vdelta;
        vdelta = get_sync_ipts(ost) / av_q2d(enc->time_base) - ost->sync_opts;
        //FIXME set to 0.5 after we fix some dts/pts bugs like in avidec.c
        if (vdelta < -1.1)
            nb_frames = 0;
        else if (vdelta > 1.1)
            nb_frames = lrintf(vdelta);
//fprintf(stderr, "vdelta:%f, ost->sync_opts:%lld, ost->sync_ipts:%f nb_frames:%d\n", vdelta, ost->sync_opts, ost->sync_ipts, nb_frames);
        if (nb_frames == 0){
            ++nb_frames_drop;
            if (verbose>2)
                fprintf(stderr, "*** drop!\n");
        }else if (nb_frames > 1) {
            nb_frames_dup += nb_frames;
            if (verbose>2)
                fprintf(stderr, "*** %d dup!\n", nb_frames-1);
        }
    }else
        ost->sync_opts= lrintf(get_sync_ipts(ost) / av_q2d(enc->time_base));

    nb_frames= FFMIN(nb_frames, max_frames[CODEC_TYPE_VIDEO] - ost->frame_number);
    if (nb_frames <= 0)
        return;

    /* convert pixel format if needed */
    target_pixfmt = ost->video_resample || ost->video_pad
        ? PIX_FMT_YUV420P : enc->pix_fmt;
    if (dec->pix_fmt != target_pixfmt) {
        int size;

        /* create temporary picture */
        size = avpicture_get_size(target_pixfmt, dec->width, dec->height);
        buf = av_malloc(size);
        if (!buf)
            return;
        formatted_picture = &picture_format_temp;
        avpicture_fill((AVPicture*)formatted_picture, buf, target_pixfmt, dec->width, dec->height);

        if (img_convert((AVPicture*)formatted_picture, target_pixfmt,
                        (AVPicture *)in_picture, dec->pix_fmt,
                        dec->width, dec->height) < 0) {

            if (verbose >= 0)
                fprintf(stderr, "pixel format conversion not handled\n");

            goto the_end;
        }
    } else {
        formatted_picture = in_picture;
    }

    /* XXX: resampling could be done before raw format conversion in
       some cases to go faster */
    /* XXX: only works for YUV420P */
    if (ost->video_resample) {
        final_picture = &ost->pict_tmp;
        img_resample(ost->img_resample_ctx, (AVPicture*)final_picture, (AVPicture*)formatted_picture);

        if (ost->padtop || ost->padbottom || ost->padleft || ost->padright) {
            fill_pad_region((AVPicture*)final_picture, enc->height, enc->width,
                    ost->padtop, ost->padbottom, ost->padleft, ost->padright,
                    padcolor);
        }

        if (enc->pix_fmt != PIX_FMT_YUV420P) {
            int size;

            av_free(buf);
            /* create temporary picture */
            size = avpicture_get_size(enc->pix_fmt, enc->width, enc->height);
            buf = av_malloc(size);
            if (!buf)
                return;
            final_picture = &picture_format_temp;
            avpicture_fill((AVPicture*)final_picture, buf, enc->pix_fmt, enc->width, enc->height);

            if (img_convert((AVPicture*)final_picture, enc->pix_fmt,
                            (AVPicture*)&ost->pict_tmp, PIX_FMT_YUV420P,
                            enc->width, enc->height) < 0) {

                if (verbose >= 0)
                    fprintf(stderr, "pixel format conversion not handled\n");

                goto the_end;
            }
        }
    } else if (ost->video_crop) {
        picture_crop_temp.data[0] = formatted_picture->data[0] +
                (ost->topBand * formatted_picture->linesize[0]) + ost->leftBand;

        picture_crop_temp.data[1] = formatted_picture->data[1] +
                ((ost->topBand >> 1) * formatted_picture->linesize[1]) +
                (ost->leftBand >> 1);

        picture_crop_temp.data[2] = formatted_picture->data[2] +
                ((ost->topBand >> 1) * formatted_picture->linesize[2]) +
                (ost->leftBand >> 1);

        picture_crop_temp.linesize[0] = formatted_picture->linesize[0];
        picture_crop_temp.linesize[1] = formatted_picture->linesize[1];
        picture_crop_temp.linesize[2] = formatted_picture->linesize[2];
        final_picture = &picture_crop_temp;
    } else if (ost->video_pad) {
        final_picture = &ost->pict_tmp;

        for (i = 0; i < 3; i++) {
            uint8_t *optr, *iptr;
            int shift = (i == 0) ? 0 : 1;
            int y, yheight;

            /* set offset to start writing image into */
            optr = final_picture->data[i] + (((final_picture->linesize[i] *
                            ost->padtop) + ost->padleft) >> shift);
            iptr = formatted_picture->data[i];

            yheight = (enc->height - ost->padtop - ost->padbottom) >> shift;
            for (y = 0; y < yheight; y++) {
                /* copy unpadded image row into padded image row */
                memcpy(optr, iptr, formatted_picture->linesize[i]);
                optr += final_picture->linesize[i];
                iptr += formatted_picture->linesize[i];
            }
        }

        fill_pad_region((AVPicture*)final_picture, enc->height, enc->width,
                ost->padtop, ost->padbottom, ost->padleft, ost->padright,
                padcolor);

        if (enc->pix_fmt != PIX_FMT_YUV420P) {
            int size;

            av_free(buf);
            /* create temporary picture */
            size = avpicture_get_size(enc->pix_fmt, enc->width, enc->height);
            buf = av_malloc(size);
            if (!buf)
                return;
            final_picture = &picture_format_temp;
            avpicture_fill((AVPicture*)final_picture, buf, enc->pix_fmt, enc->width, enc->height);

            if (img_convert((AVPicture*)final_picture, enc->pix_fmt,
                        (AVPicture*)&ost->pict_tmp, PIX_FMT_YUV420P,
                        enc->width, enc->height) < 0) {

                if (verbose >= 0)
                    fprintf(stderr, "pixel format conversion not handled\n");

                goto the_end;
            }
        }
    } else {
        final_picture = formatted_picture;
    }
    /* duplicates frame if needed */
    for(i=0;i<nb_frames;i++) {
        AVPacket pkt;
        av_init_packet(&pkt);
        pkt.stream_index= ost->index;

        if (s->oformat->flags & AVFMT_RAWPICTURE) {
            /* raw pictures are written as AVPicture structure to
               avoid any copies. We support temorarily the older
               method. */
            AVFrame* old_frame = enc->coded_frame;
            enc->coded_frame = dec->coded_frame; //FIXME/XXX remove this hack
            pkt.data= (uint8_t *)final_picture;
            pkt.size=  sizeof(AVPicture);
            if(dec->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
            if(dec->coded_frame && dec->coded_frame->key_frame)
                pkt.flags |= PKT_FLAG_KEY;

            av_interleaved_write_frame(s, &pkt);
            enc->coded_frame = old_frame;
        } else {
            AVFrame big_picture;

            big_picture= *final_picture;
            /* better than nothing: use input picture interlaced
               settings */
            big_picture.interlaced_frame = in_picture->interlaced_frame;
            if(avctx_opts->flags & (CODEC_FLAG_INTERLACED_DCT|CODEC_FLAG_INTERLACED_ME)){
                if(top_field_first == -1)
                    big_picture.top_field_first = in_picture->top_field_first;
                else
                    big_picture.top_field_first = top_field_first;
            }

            /* handles sameq here. This is not correct because it may
               not be a global option */
            if (same_quality) {
                big_picture.quality = ist->st->quality;
            }else
                big_picture.quality = ost->st->quality;
            if(!me_threshold)
                big_picture.pict_type = 0;
//            big_picture.pts = AV_NOPTS_VALUE;
            big_picture.pts= ost->sync_opts;
//            big_picture.pts= av_rescale(ost->sync_opts, AV_TIME_BASE*(int64_t)enc->time_base.num, enc->time_base.den);
//av_log(NULL, AV_LOG_DEBUG, "%lld -> encoder\n", ost->sync_opts);
            ret = avcodec_encode_video(enc,
                                       bit_buffer, bit_buffer_size,
                                       &big_picture);
            //enc->frame_number = enc->real_pict_num;
            if(ret>0){
                pkt.data= bit_buffer;
                pkt.size= ret;
                if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                    pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
/*av_log(NULL, AV_LOG_DEBUG, "encoder -> %lld/%lld\n",
   pkt.pts != AV_NOPTS_VALUE ? av_rescale(pkt.pts, enc->time_base.den, AV_TIME_BASE*(int64_t)enc->time_base.num) : -1,
   pkt.dts != AV_NOPTS_VALUE ? av_rescale(pkt.dts, enc->time_base.den, AV_TIME_BASE*(int64_t)enc->time_base.num) : -1);*/

                if(enc->coded_frame && enc->coded_frame->key_frame)
                    pkt.flags |= PKT_FLAG_KEY;
                av_interleaved_write_frame(s, &pkt);
                *frame_size = ret;
                //fprintf(stderr,"\nFrame: %3d %3d size: %5d type: %d",
                //        enc->frame_number-1, enc->real_pict_num, ret,
                //        enc->pict_type);
                /* if two pass, output log */
                if (ost->logfile && enc->stats_out) {
                    fprintf(ost->logfile, "%s", enc->stats_out);
                }
            }
        }
        ost->sync_opts++;
        ost->frame_number++;
    }
 the_end:
    av_free(buf);
    av_free(buf1);
}

static double psnr(double d){
    if(d==0) return INFINITY;
    return -10.0*log(d)/log(10.0);
}

static void do_video_stats(AVFormatContext *os, AVOutputStream *ost,
                           int frame_size)
{
    static FILE *fvstats=NULL;
    char filename[40];
    time_t today2;
    struct tm *today;
    AVCodecContext *enc;
    int frame_number;
    int64_t ti;
    double ti1, bitrate, avg_bitrate;

    if (!fvstats) {
        today2 = time(NULL);
        today = localtime(&today2);
        snprintf(filename, sizeof(filename), "vstats_%02d%02d%02d.log", today->tm_hour,
                                               today->tm_min,
                                               today->tm_sec);
        fvstats = fopen(filename,"w");
        if (!fvstats) {
            perror("fopen");
            exit(1);
        }
    }

    ti = MAXINT64;
    enc = ost->st->codec;
    if (enc->codec_type == CODEC_TYPE_VIDEO) {
        frame_number = ost->frame_number;
        fprintf(fvstats, "frame= %5d q= %2.1f ", frame_number, enc->coded_frame->quality/(float)FF_QP2LAMBDA);
        if (enc->flags&CODEC_FLAG_PSNR)
            fprintf(fvstats, "PSNR= %6.2f ", psnr(enc->coded_frame->error[0]/(enc->width*enc->height*255.0*255.0)));

        fprintf(fvstats,"f_size= %6d ", frame_size);
        /* compute pts value */
        ti1 = ost->sync_opts * av_q2d(enc->time_base);
        if (ti1 < 0.01)
            ti1 = 0.01;

        bitrate = (frame_size * 8) / av_q2d(enc->time_base) / 1000.0;
        avg_bitrate = (double)(video_size * 8) / ti1 / 1000.0;
        fprintf(fvstats, "s_size= %8.0fkB time= %0.3f br= %7.1fkbits/s avg_br= %7.1fkbits/s ",
            (double)video_size / 1024, ti1, bitrate, avg_bitrate);
        fprintf(fvstats,"type= %c\n", av_get_pict_type_char(enc->coded_frame->pict_type));
    }
}

static void print_report(AVFormatContext **output_files,
                         AVOutputStream **ost_table, int nb_ostreams,
                         int is_last_report)
{
	if ( cpp_callback ) {
		AVOutputStream *ost = ost_table[0];
		if ( !cpp_callback(cpp_passed_ptr, ost->frame_number) ) {
			received_sigterm = 1;
		}
	}
//    char buf[1024];
//    AVOutputStream *ost;
//    AVFormatContext *oc, *os;
//    int64_t total_size;
//    AVCodecContext *enc;
//    int frame_number, vid, i;
//    double bitrate, ti1, pts;
//    static int64_t last_time = -1;
//    static int qp_histogram[52];
//
//    if (!is_last_report) {
//        int64_t cur_time;
//        /* display the report every 0.5 seconds */
//        cur_time = av_gettime();
//        if (last_time == -1) {
//            last_time = cur_time;
//            return;
//        }
//        if ((cur_time - last_time) < 500000)
//            return;
//        last_time = cur_time;
//    }
//
//
//    oc = output_files[0];
//
//    total_size = url_ftell(&oc->pb);
//
//    buf[0] = '\0';
//    ti1 = 1e10;
//    vid = 0;
//    for(i=0;i<nb_ostreams;i++) {
//        ost = ost_table[i];
//        os = output_files[ost->file_index];
//        enc = ost->st->codec;
//        if (vid && enc->codec_type == CODEC_TYPE_VIDEO) {
//            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "q=%2.1f ",
//                    enc->coded_frame->quality/(float)FF_QP2LAMBDA);
//        }
//        if (!vid && enc->codec_type == CODEC_TYPE_VIDEO) {
//            frame_number = ost->frame_number;
//            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "frame=%5d q=%3.1f ",
//                    frame_number, enc->coded_frame ? enc->coded_frame->quality/(float)FF_QP2LAMBDA : -1);
//            if(is_last_report)
//                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "L");
//            if(qp_hist && enc->coded_frame){
//                int j;
//                int qp= lrintf(enc->coded_frame->quality/(float)FF_QP2LAMBDA);
//                if(qp>=0 && qp<sizeof(qp_histogram)/sizeof(int))
//                    qp_histogram[qp]++;
//                for(j=0; j<32; j++)
//                    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%X", (int)lrintf(log(qp_histogram[j]+1)/log(2)));
//            }
//            if (enc->flags&CODEC_FLAG_PSNR){
//                int j;
//                double error, error_sum=0;
//                double scale, scale_sum=0;
//                char type[3]= {'Y','U','V'};
//                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "PSNR=");
//                for(j=0; j<3; j++){
//                    if(is_last_report){
//                        error= enc->error[j];
//                        scale= enc->width*enc->height*255.0*255.0*frame_number;
//                    }else{
//                        error= enc->coded_frame->error[j];
//                        scale= enc->width*enc->height*255.0*255.0;
//                    }
//                    if(j) scale/=4;
//                    error_sum += error;
//                    scale_sum += scale;
//                    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%c:%2.2f ", type[j], psnr(error/scale));
//                }
//                snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "*:%2.2f ", psnr(error_sum/scale_sum));
//            }
//            vid = 1;
//        }
//        /* compute min output value */
//        pts = (double)ost->st->pts.val * ost->st->time_base.num / ost->st->time_base.den;
//        if ((pts < ti1) && (pts > 0))
//            ti1 = pts;
//    }
//    if (ti1 < 0.01)
//        ti1 = 0.01;
//
//    if (verbose || is_last_report) {
//        bitrate = (double)(total_size * 8) / ti1 / 1000.0;
//
//        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
//            "size=%8.0fkB time=%0.1f bitrate=%6.1fkbits/s",
//            (double)total_size / 1024, ti1, bitrate);
//
//        if (verbose > 1)
//          snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " dup=%d drop=%d",
//                  nb_frames_dup, nb_frames_drop);
//
//        if (verbose >= 0)
//            fprintf(stderr, "%s    \r", buf);
//
//        fflush(stderr);
//    }
//
//    if (is_last_report && verbose >= 0){
//        int64_t raw= audio_size + video_size + extra_size;
//        fprintf(stderr, "\n");
//        fprintf(stderr, "video:%1.0fkB audio:%1.0fkB global headers:%1.0fkB muxing overhead %f%%\n",
//                video_size/1024.0,
//                audio_size/1024.0,
//                extra_size/1024.0,
//                100.0*(total_size - raw)/raw
//        );
//    }
}

/* pkt = NULL means EOF (needed to flush decoder buffers) */
static int output_packet(AVInputStream *ist, int ist_index,
                         AVOutputStream **ost_table, int nb_ostreams,
                         const AVPacket *pkt)
{
    AVFormatContext *os;
    AVOutputStream *ost;
    uint8_t *ptr;
    int len, ret, i;
    uint8_t *data_buf;
    int data_size, got_picture;
    AVFrame picture;
    void *buffer_to_free;
    static unsigned int samples_size= 0;
    static short *samples= NULL;
    AVSubtitle subtitle, *subtitle_to_free;
    int got_subtitle;

    if(!pkt){
        ist->pts= ist->next_pts; // needed for last packet if vsync=0
    } else if (pkt->dts != AV_NOPTS_VALUE) { //FIXME seems redundant, as libavformat does this too
        ist->next_pts = ist->pts = av_rescale_q(pkt->dts, ist->st->time_base, AV_TIME_BASE_Q);
    } else {
//        assert(ist->pts == ist->next_pts);
    }

    if (pkt == NULL) {
        /* EOF handling */
        ptr = NULL;
        len = 0;
        goto handle_eof;
    }

    len = pkt->size;
    ptr = pkt->data;
    while (len > 0) {
    handle_eof:
        /* decode the packet if needed */
        data_buf = NULL; /* fail safe */
        data_size = 0;
        subtitle_to_free = NULL;
        if (ist->decoding_needed) {
            switch(ist->st->codec->codec_type) {
            case CODEC_TYPE_AUDIO:{
                if(pkt)
                    samples= av_fast_realloc(samples, &samples_size, FFMAX(pkt->size, AVCODEC_MAX_AUDIO_FRAME_SIZE));
                    /* XXX: could avoid copy if PCM 16 bits with same
                       endianness as CPU */
                ret = avcodec_decode_audio(ist->st->codec, samples, &data_size,
                                           ptr, len);
                if (ret < 0)
                    goto fail_decode;
                ptr += ret;
                len -= ret;
                /* Some bug in mpeg audio decoder gives */
                /* data_size < 0, it seems they are overflows */
                if (data_size <= 0) {
                    /* no audio frame */
                    continue;
                }
                data_buf = (uint8_t *)samples;
                ist->next_pts += ((int64_t)AV_TIME_BASE/2 * data_size) /
                    (ist->st->codec->sample_rate * ist->st->codec->channels);
                break;}
            case CODEC_TYPE_VIDEO:
                    data_size = (ist->st->codec->width * ist->st->codec->height * 3) / 2;
                    /* XXX: allocate picture correctly */
                    avcodec_get_frame_defaults(&picture);

                    ret = avcodec_decode_video(ist->st->codec,
                                               &picture, &got_picture, ptr, len);
                    ist->st->quality= picture.quality;
                    if (ret < 0)
                        goto fail_decode;
                    if (!got_picture) {
                        /* no picture yet */
                        goto discard_packet;
                    }
                    if (ist->st->codec->time_base.num != 0) {
                        ist->next_pts += ((int64_t)AV_TIME_BASE *
                                          ist->st->codec->time_base.num) /
                            ist->st->codec->time_base.den;
                    }
                    len = 0;
                    break;
            case CODEC_TYPE_SUBTITLE:
                ret = avcodec_decode_subtitle(ist->st->codec,
                                              &subtitle, &got_subtitle, ptr, len);
                if (ret < 0)
                    goto fail_decode;
                if (!got_subtitle) {
                    goto discard_packet;
                }
                subtitle_to_free = &subtitle;
                len = 0;
                break;
            default:
                goto fail_decode;
            }
        } else {
                switch(ist->st->codec->codec_type) {
                case CODEC_TYPE_AUDIO:
                    ist->next_pts += ((int64_t)AV_TIME_BASE * ist->st->codec->frame_size) /
                        (ist->st->codec->sample_rate * ist->st->codec->channels);
                    break;
                case CODEC_TYPE_VIDEO:
                    if (ist->st->codec->time_base.num != 0) {
                        ist->next_pts += ((int64_t)AV_TIME_BASE *
                                          ist->st->codec->time_base.num) /
                            ist->st->codec->time_base.den;
                    }
                    break;
                }
                data_buf = ptr;
                data_size = len;
                ret = len;
                len = 0;
            }

            buffer_to_free = NULL;
            if (ist->st->codec->codec_type == CODEC_TYPE_VIDEO) {
                pre_process_video_frame(ist, (AVPicture *)&picture,
                                        &buffer_to_free);
            }

            // preprocess audio (volume)
            if (ist->st->codec->codec_type == CODEC_TYPE_AUDIO) {
                if (audio_volume != 256) {
                    short *volp;
                    volp = samples;
                    for(i=0;i<(data_size / sizeof(short));i++) {
                        int v = ((*volp) * audio_volume + 128) >> 8;
                        if (v < -32768) v = -32768;
                        if (v >  32767) v = 32767;
                        *volp++ = v;
                    }
                }
            }

            /* frame rate emulation */
            if (ist->st->codec->rate_emu) {
                int64_t pts = av_rescale((int64_t) ist->frame * ist->st->codec->time_base.num, 1000000, ist->st->codec->time_base.den);
                int64_t now = av_gettime() - ist->start;
                if (pts > now)
                    usleep(pts - now);

                ist->frame++;
            }

#if 0
            /* mpeg PTS deordering : if it is a P or I frame, the PTS
               is the one of the next displayed one */
            /* XXX: add mpeg4 too ? */
            if (ist->st->codec->codec_id == CODEC_ID_MPEG1VIDEO) {
                if (ist->st->codec->pict_type != B_TYPE) {
                    int64_t tmp;
                    tmp = ist->last_ip_pts;
                    ist->last_ip_pts  = ist->frac_pts.val;
                    ist->frac_pts.val = tmp;
                }
            }
#endif
            /* if output time reached then transcode raw format,
               encode packets and output them */
            if (start_time == 0 || ist->pts >= start_time)
                for(i=0;i<nb_ostreams;i++) {
                    int frame_size;

                    ost = ost_table[i];
                    if (ost->source_index == ist_index) {
                        os = output_files[ost->file_index];

#if 0
                        printf("%d: got pts=%0.3f %0.3f\n", i,
                               (double)pkt->pts / AV_TIME_BASE,
                               ((double)ist->pts / AV_TIME_BASE) -
                               ((double)ost->st->pts.val * ost->st->time_base.num / ost->st->time_base.den));
#endif
                        /* set the input output pts pairs */
                        //ost->sync_ipts = (double)(ist->pts + input_files_ts_offset[ist->file_index] - start_time)/ AV_TIME_BASE;

                        if (ost->encoding_needed) {
                            switch(ost->st->codec->codec_type) {
                            case CODEC_TYPE_AUDIO:
                                do_audio_out(os, ost, ist, data_buf, data_size);
                                break;
                            case CODEC_TYPE_VIDEO:
                                    do_video_out(os, ost, ist, &picture, &frame_size);
                                    video_size += frame_size;
                                    if (do_vstats && frame_size)
                                        do_video_stats(os, ost, frame_size);
                                break;
                            case CODEC_TYPE_SUBTITLE:
                                do_subtitle_out(os, ost, ist, &subtitle,
                                                pkt->pts);
                                break;
                            default:
                                av_abort();
                            }
                        } else {
                            AVFrame avframe; //FIXME/XXX remove this
                            AVPacket opkt;
                            av_init_packet(&opkt);

                            /* no reencoding needed : output the packet directly */
                            /* force the input stream PTS */

                            avcodec_get_frame_defaults(&avframe);
                            ost->st->codec->coded_frame= &avframe;
                            avframe.key_frame = pkt->flags & PKT_FLAG_KEY;

                            if(ost->st->codec->codec_type == CODEC_TYPE_AUDIO)
                                audio_size += data_size;
                            else if (ost->st->codec->codec_type == CODEC_TYPE_VIDEO) {
                                video_size += data_size;
                                ost->sync_opts++;
                            }

                            opkt.stream_index= ost->index;
                            if(pkt->pts != AV_NOPTS_VALUE)
                                opkt.pts= av_rescale_q(av_rescale_q(pkt->pts, ist->st->time_base, AV_TIME_BASE_Q) + input_files_ts_offset[ist->file_index], AV_TIME_BASE_Q,  ost->st->time_base);
                            else
                                opkt.pts= AV_NOPTS_VALUE;

                            {
                                int64_t dts;
                                if (pkt->dts == AV_NOPTS_VALUE)
                                    dts = ist->next_pts;
                                else
                                    dts= av_rescale_q(pkt->dts, ist->st->time_base, AV_TIME_BASE_Q);
                                opkt.dts= av_rescale_q(dts + input_files_ts_offset[ist->file_index], AV_TIME_BASE_Q,  ost->st->time_base);
                            }
                            opkt.flags= pkt->flags;
                            if(av_parser_change(ist->st->parser, ost->st->codec, &opkt.data, &opkt.size, data_buf, data_size, pkt->flags & PKT_FLAG_KEY))
                                opkt.destruct= av_destruct_packet;
                            av_interleaved_write_frame(os, &opkt);
                            ost->st->codec->frame_number++;
                            ost->frame_number++;
                            av_free_packet(&opkt);
                        }
                    }
                }
            av_free(buffer_to_free);
            /* XXX: allocate the subtitles in the codec ? */
            if (subtitle_to_free) {
                if (subtitle_to_free->rects != NULL) {
                    for (i = 0; i < subtitle_to_free->num_rects; i++) {
                        av_free(subtitle_to_free->rects[i].bitmap);
                        av_free(subtitle_to_free->rects[i].rgba_palette);
                    }
                    av_freep(&subtitle_to_free->rects);
                }
                subtitle_to_free->num_rects = 0;
                subtitle_to_free = NULL;
            }
        }
 discard_packet:
    if (pkt == NULL) {
        /* EOF handling */

        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            if (ost->source_index == ist_index) {
                AVCodecContext *enc= ost->st->codec;
                os = output_files[ost->file_index];

                if(ost->st->codec->codec_type == CODEC_TYPE_AUDIO && enc->frame_size <=1)
                    continue;
                if(ost->st->codec->codec_type == CODEC_TYPE_VIDEO && (os->oformat->flags & AVFMT_RAWPICTURE))
                    continue;

                if (ost->encoding_needed) {
                    for(;;) {
                        AVPacket pkt;
                        av_init_packet(&pkt);
                        pkt.stream_index= ost->index;

                        switch(ost->st->codec->codec_type) {
                        case CODEC_TYPE_AUDIO:
                            ret = avcodec_encode_audio(enc, bit_buffer, bit_buffer_size, NULL);
                            audio_size += ret;
                            pkt.flags |= PKT_FLAG_KEY;
                            break;
                        case CODEC_TYPE_VIDEO:
                            ret = avcodec_encode_video(enc, bit_buffer, bit_buffer_size, NULL);
                            video_size += ret;
                            if(enc->coded_frame && enc->coded_frame->key_frame)
                                pkt.flags |= PKT_FLAG_KEY;
                            if (ost->logfile && enc->stats_out) {
                                fprintf(ost->logfile, "%s", enc->stats_out);
                            }
                            break;
                        default:
                            ret=-1;
                        }

                        if(ret<=0)
                            break;
                        pkt.data= bit_buffer;
                        pkt.size= ret;
                        if(enc->coded_frame && enc->coded_frame->pts != AV_NOPTS_VALUE)
                            pkt.pts= av_rescale_q(enc->coded_frame->pts, enc->time_base, ost->st->time_base);
                        av_interleaved_write_frame(os, &pkt);
                    }
                }
            }
        }
    }

    return 0;
 fail_decode:
    return -1;
}


/*
 * The following code is the main loop of the file converter
 */
static int av_encode(AVFormatContext **output_files,
                     int nb_output_files,
                     AVFormatContext **input_files,
                     int nb_input_files,
                     AVStreamMap *stream_maps, int nb_stream_maps)
{
    int ret, i, j, k, n, nb_istreams = 0, nb_ostreams = 0;
    AVFormatContext *is, *os;
    AVCodecContext *codec, *icodec;
    AVOutputStream *ost, **ost_table = NULL;
    AVInputStream *ist, **ist_table = NULL;
    AVInputFile *file_table;
    AVFormatContext *stream_no_data;
    int key;

    file_table= (AVInputFile*) av_mallocz(nb_input_files * sizeof(AVInputFile));
    if (!file_table)
        goto fail;

    /* input stream init */
    j = 0;
    for(i=0;i<nb_input_files;i++) {
        is = input_files[i];
        file_table[i].ist_index = j;
        file_table[i].nb_streams = is->nb_streams;
        j += is->nb_streams;
    }
    nb_istreams = j;

    ist_table = av_mallocz(nb_istreams * sizeof(AVInputStream *));
    if (!ist_table)
        goto fail;

    for(i=0;i<nb_istreams;i++) {
        ist = av_mallocz(sizeof(AVInputStream));
        if (!ist)
            goto fail;
        ist_table[i] = ist;
    }
    j = 0;
    for(i=0;i<nb_input_files;i++) {
        is = input_files[i];
        for(k=0;k<is->nb_streams;k++) {
            ist = ist_table[j++];
            ist->st = is->streams[k];
            ist->file_index = i;
            ist->index = k;
            ist->discard = 1; /* the stream is discarded by default
                                 (changed later) */

            if (ist->st->codec->rate_emu) {
                ist->start = av_gettime();
                ist->frame = 0;
            }
        }
    }

    /* output stream init */
    nb_ostreams = 0;
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        nb_ostreams += os->nb_streams;
    }
    if (nb_stream_maps > 0 && nb_stream_maps != nb_ostreams) {
        fprintf(stderr, "Number of stream maps must match number of output streams\n");
        exit(1);
    }

    /* Sanity check the mapping args -- do the input files & streams exist? */
    for(i=0;i<nb_stream_maps;i++) {
        int fi = stream_maps[i].file_index;
        int si = stream_maps[i].stream_index;

        if (fi < 0 || fi > nb_input_files - 1 ||
            si < 0 || si > file_table[fi].nb_streams - 1) {
            fprintf(stderr,"Could not find input stream #%d.%d\n", fi, si);
            exit(1);
        }
        fi = stream_maps[i].sync_file_index;
        si = stream_maps[i].sync_stream_index;
        if (fi < 0 || fi > nb_input_files - 1 ||
            si < 0 || si > file_table[fi].nb_streams - 1) {
            fprintf(stderr,"Could not find sync stream #%d.%d\n", fi, si);
            exit(1);
        }
    }

    ost_table = av_mallocz(sizeof(AVOutputStream *) * nb_ostreams);
    if (!ost_table)
        goto fail;
    for(i=0;i<nb_ostreams;i++) {
        ost = av_mallocz(sizeof(AVOutputStream));
        if (!ost)
            goto fail;
        ost_table[i] = ost;
    }

    n = 0;
    for(k=0;k<nb_output_files;k++) {
        os = output_files[k];
        for(i=0;i<os->nb_streams;i++) {
            int found;
            ost = ost_table[n++];
            ost->file_index = k;
            ost->index = i;
            ost->st = os->streams[i];
            if (nb_stream_maps > 0) {
                ost->source_index = file_table[stream_maps[n-1].file_index].ist_index +
                    stream_maps[n-1].stream_index;

                /* Sanity check that the stream types match */
                if (ist_table[ost->source_index]->st->codec->codec_type != ost->st->codec->codec_type) {
                    fprintf(stderr, "Codec type mismatch for mapping #%d.%d -> #%d.%d\n",
                        stream_maps[n-1].file_index, stream_maps[n-1].stream_index,
                        ost->file_index, ost->index);
                    exit(1);
                }

            } else {
                /* get corresponding input stream index : we select the first one with the right type */
                found = 0;
                for(j=0;j<nb_istreams;j++) {
                    ist = ist_table[j];
                    if (ist->discard &&
                        ist->st->codec->codec_type == ost->st->codec->codec_type) {
                        ost->source_index = j;
                        found = 1;
                        break;
                    }
                }

                if (!found) {
                    /* try again and reuse existing stream */
                    for(j=0;j<nb_istreams;j++) {
                        ist = ist_table[j];
                        if (ist->st->codec->codec_type == ost->st->codec->codec_type) {
                            ost->source_index = j;
                            found = 1;
                        }
                    }
                    if (!found) {
                        fprintf(stderr, "Could not find input stream matching output stream #%d.%d\n",
                                ost->file_index, ost->index);
                        exit(1);
                    }
                }
            }
            ist = ist_table[ost->source_index];
            ist->discard = 0;
            ost->sync_ist = (nb_stream_maps > 0) ?
                ist_table[file_table[stream_maps[n-1].sync_file_index].ist_index +
                         stream_maps[n-1].sync_stream_index] : ist;
        }
    }

    /* for each output stream, we compute the right encoding parameters */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        ist = ist_table[ost->source_index];

        codec = ost->st->codec;
        icodec = ist->st->codec;

        if (ost->st->stream_copy) {
            /* if stream_copy is selected, no need to decode or encode */
            codec->codec_id = icodec->codec_id;
            codec->codec_type = icodec->codec_type;
            if(!codec->codec_tag) codec->codec_tag = icodec->codec_tag;
            codec->bit_rate = icodec->bit_rate;
            codec->extradata= icodec->extradata;
            codec->extradata_size= icodec->extradata_size;
            codec->time_base = icodec->time_base;
            switch(codec->codec_type) {
            case CODEC_TYPE_AUDIO:
                codec->sample_rate = icodec->sample_rate;
                codec->channels = icodec->channels;
                codec->frame_size = icodec->frame_size;
                codec->block_align= icodec->block_align;
                break;
            case CODEC_TYPE_VIDEO:
                codec->width = icodec->width;
                codec->height = icodec->height;
                codec->has_b_frames = icodec->has_b_frames;
                break;
            case CODEC_TYPE_SUBTITLE:
                break;
            default:
                av_abort();
            }
        } else {
            switch(codec->codec_type) {
            case CODEC_TYPE_AUDIO:
                if (fifo_init(&ost->fifo, 2 * MAX_AUDIO_PACKET_SIZE))
                    goto fail;

                if (codec->channels == icodec->channels &&
                    codec->sample_rate == icodec->sample_rate) {
                    ost->audio_resample = 0;
                } else {
                    if (codec->channels != icodec->channels &&
                        (icodec->codec_id == CODEC_ID_AC3 ||
                         icodec->codec_id == CODEC_ID_DTS)) {
                        /* Special case for 5:1 AC3 and DTS input */
                        /* and mono or stereo output      */
                        /* Request specific number of channels */
                        icodec->channels = codec->channels;
                        if (codec->sample_rate == icodec->sample_rate)
                            ost->audio_resample = 0;
                        else {
                            ost->audio_resample = 1;
                        }
                    } else {
                        ost->audio_resample = 1;
                    }
                }
                if(audio_sync_method>1)
                    ost->audio_resample = 1;

                if(ost->audio_resample){
                    ost->resample = audio_resample_init(codec->channels, icodec->channels,
                                                    codec->sample_rate, icodec->sample_rate);
                    if(!ost->resample){
                        printf("Can't resample.  Aborting.\n");
                        av_abort();
                    }
                }
                ist->decoding_needed = 1;
                ost->encoding_needed = 1;
                break;
            case CODEC_TYPE_VIDEO:
                if (codec->width == icodec->width &&
                    codec->height == icodec->height &&
                    frame_topBand == 0 &&
                    frame_bottomBand == 0 &&
                    frame_leftBand == 0 &&
                    frame_rightBand == 0 &&
                    frame_padtop == 0 &&
                    frame_padbottom == 0 &&
                    frame_padleft == 0 &&
                    frame_padright == 0)
                {
                    ost->video_resample = 0;
                    ost->video_crop = 0;
                    ost->video_pad = 0;
                } else if ((codec->width == icodec->width -
                                (frame_leftBand + frame_rightBand)) &&
                        (codec->height == icodec->height -
                                (frame_topBand  + frame_bottomBand)))
                {
                    ost->video_resample = 0;
                    ost->video_crop = 1;
                    ost->topBand = frame_topBand;
                    ost->leftBand = frame_leftBand;
                } else if ((codec->width == icodec->width +
                                (frame_padleft + frame_padright)) &&
                        (codec->height == icodec->height +
                                (frame_padtop + frame_padbottom))) {
                    ost->video_resample = 0;
                    ost->video_crop = 0;
                    ost->video_pad = 1;
                    ost->padtop = frame_padtop;
                    ost->padleft = frame_padleft;
                    ost->padbottom = frame_padbottom;
                    ost->padright = frame_padright;
                    avcodec_get_frame_defaults(&ost->pict_tmp);
                    if( avpicture_alloc( (AVPicture*)&ost->pict_tmp, PIX_FMT_YUV420P,
                                codec->width, codec->height ) )
                        goto fail;
                } else {
                    ost->video_resample = 1;
                    ost->video_crop = 0; // cropping is handled as part of resample
                    avcodec_get_frame_defaults(&ost->pict_tmp);
                    if( avpicture_alloc( (AVPicture*)&ost->pict_tmp, PIX_FMT_YUV420P,
                                         codec->width, codec->height ) )
                        goto fail;

                    ost->img_resample_ctx = img_resample_full_init(
                                      codec->width, codec->height,
                                      icodec->width, icodec->height,
                                      frame_topBand, frame_bottomBand,
                            frame_leftBand, frame_rightBand,
                            frame_padtop, frame_padbottom,
                            frame_padleft, frame_padright);

                    ost->padtop = frame_padtop;
                    ost->padleft = frame_padleft;
                    ost->padbottom = frame_padbottom;
                    ost->padright = frame_padright;

                }
                ost->encoding_needed = 1;
                ist->decoding_needed = 1;
                break;
            case CODEC_TYPE_SUBTITLE:
                ost->encoding_needed = 1;
                ist->decoding_needed = 1;
                break;
            default:
                av_abort();
                break;
            }
            /* two pass mode */
            if (ost->encoding_needed &&
                (codec->flags & (CODEC_FLAG_PASS1 | CODEC_FLAG_PASS2))) {
                char logfilename[1024];
                FILE *f;
                int size;
                char *logbuffer;

                snprintf(logfilename, sizeof(logfilename), "%s-%d.log",
                         pass_logfilename ?
                         pass_logfilename : DEFAULT_PASS_LOGFILENAME, i);
                if (codec->flags & CODEC_FLAG_PASS1) {
                    f = fopen(logfilename, "w");
                    if (!f) {
                        perror(logfilename);
                        exit(1);
                    }
                    ost->logfile = f;
                } else {
                    /* read the log file */
                    f = fopen(logfilename, "r");
                    if (!f) {
                        perror(logfilename);
                        exit(1);
                    }
                    fseek(f, 0, SEEK_END);
                    size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    logbuffer = av_malloc(size + 1);
                    if (!logbuffer) {
                        fprintf(stderr, "Could not allocate log buffer\n");
                        exit(1);
                    }
                    size = fread(logbuffer, 1, size, f);
                    fclose(f);
                    logbuffer[size] = '\0';
                    codec->stats_in = logbuffer;
                }
            }
        }
        if(codec->codec_type == CODEC_TYPE_VIDEO){
            int size= codec->width * codec->height;
            bit_buffer_size= FFMAX(bit_buffer_size, 4*size);
        }
    }

    if (!bit_buffer)
        bit_buffer = av_malloc(bit_buffer_size);
    if (!bit_buffer)
        goto fail;

    /* dump the file output parameters - cannot be done before in case
       of stream copy */
    for(i=0;i<nb_output_files;i++) {
        dump_format(output_files[i], i, output_files[i]->filename, 1);
    }

    /* dump the stream mapping */
    if (verbose >= 0) {
        fprintf(stderr, "Stream mapping:\n");
        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            fprintf(stderr, "  Stream #%d.%d -> #%d.%d",
                    ist_table[ost->source_index]->file_index,
                    ist_table[ost->source_index]->index,
                    ost->file_index,
                    ost->index);
            if (ost->sync_ist != ist_table[ost->source_index])
                fprintf(stderr, " [sync #%d.%d]",
                        ost->sync_ist->file_index,
                        ost->sync_ist->index);
            fprintf(stderr, "\n");
        }
    }

    /* open each encoder */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        if (ost->encoding_needed) {
            AVCodec *codec;
            codec = avcodec_find_encoder(ost->st->codec->codec_id);
            if (!codec) {
                fprintf(stderr, "Unsupported codec for output stream #%d.%d\n",
                        ost->file_index, ost->index);
                exit(1);
            }
            if (avcodec_open(ost->st->codec, codec) < 0) {
                fprintf(stderr, "Error while opening codec for output stream #%d.%d - maybe incorrect parameters such as bit_rate, rate, width or height\n",
                        ost->file_index, ost->index);
                exit(1);
            }
            extra_size += ost->st->codec->extradata_size;
        }
    }

    /* open each decoder */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            AVCodec *codec;
            codec = avcodec_find_decoder(ist->st->codec->codec_id);
            if (!codec) {
                fprintf(stderr, "Unsupported codec (id=%d) for input stream #%d.%d\n",
                        ist->st->codec->codec_id, ist->file_index, ist->index);
                exit(1);
            }
            if (avcodec_open(ist->st->codec, codec) < 0) {
                fprintf(stderr, "Error while opening codec for input stream #%d.%d\n",
                        ist->file_index, ist->index);
                exit(1);
            }
            //if (ist->st->codec->codec_type == CODEC_TYPE_VIDEO)
            //    ist->st->codec->flags |= CODEC_FLAG_REPEAT_FIELD;
        }
    }

    /* init pts */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        is = input_files[ist->file_index];
        ist->pts = 0;
        ist->next_pts = av_rescale_q(ist->st->start_time, ist->st->time_base, AV_TIME_BASE_Q);
        if(ist->st->start_time == AV_NOPTS_VALUE)
            ist->next_pts=0;
        if(input_files_ts_offset[ist->file_index])
            ist->next_pts= AV_NOPTS_VALUE;
        ist->is_start = 1;
    }

    /* compute buffer size max (should use a complete heuristic) */
    for(i=0;i<nb_input_files;i++) {
        file_table[i].buffer_size_max = 2048;
    }

    /* set meta data information from input file if required */
    for (i=0;i<nb_meta_data_maps;i++) {
        AVFormatContext *out_file;
        AVFormatContext *in_file;

        int out_file_index = meta_data_maps[i].out_file;
        int in_file_index = meta_data_maps[i].in_file;
        if ( out_file_index < 0 || out_file_index >= nb_output_files ) {
            fprintf(stderr, "Invalid output file index %d map_meta_data(%d,%d)\n", out_file_index, out_file_index, in_file_index);
            ret = -EINVAL;
            goto fail;
        }
        if ( in_file_index < 0 || in_file_index >= nb_input_files ) {
            fprintf(stderr, "Invalid input file index %d map_meta_data(%d,%d)\n", in_file_index, out_file_index, in_file_index);
            ret = -EINVAL;
            goto fail;
        }

        out_file = output_files[out_file_index];
        in_file = input_files[in_file_index];

        strcpy(out_file->title, in_file->title);
        strcpy(out_file->author, in_file->author);
        strcpy(out_file->copyright, in_file->copyright);
        strcpy(out_file->comment, in_file->comment);
        strcpy(out_file->album, in_file->album);
        out_file->year = in_file->year;
        out_file->track = in_file->track;
        strcpy(out_file->genre, in_file->genre);
    }

    /* open files and write file headers */
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        if (av_write_header(os) < 0) {
            fprintf(stderr, "Could not write header for output file #%d (incorrect codec parameters ?)\n", i);
            ret = -EINVAL;
            goto fail;
        }
    }

    stream_no_data = 0;
    key = -1;

    for(; received_sigterm == 0;) {
        int file_index, ist_index;
        AVPacket pkt;
        double ipts_min;
        double opts_min;

    redo:
        ipts_min= 1e100;
        opts_min= 1e100;

        /* select the stream that we must read now by looking at the
           smallest output pts */
        file_index = -1;
        for(i=0;i<nb_ostreams;i++) {
            double ipts, opts;
            ost = ost_table[i];
            os = output_files[ost->file_index];
            ist = ist_table[ost->source_index];
            if(ost->st->codec->codec_type == CODEC_TYPE_VIDEO)
                opts = ost->sync_opts * av_q2d(ost->st->codec->time_base);
            else
                opts = ost->st->pts.val * av_q2d(ost->st->time_base);
            ipts = (double)ist->pts;
            if (!file_table[ist->file_index].eof_reached){
                if(ipts < ipts_min) {
                    ipts_min = ipts;
                    if(input_sync ) file_index = ist->file_index;
                }
                if(opts < opts_min) {
                    opts_min = opts;
                    if(!input_sync) file_index = ist->file_index;
                }
            }
            if(ost->frame_number >= max_frames[ost->st->codec->codec_type]){
                file_index= -1;
                break;
            }
        }
        /* if none, if is finished */
        if (file_index < 0) {
            break;
        }

        /* finish if recording time exhausted */
        if (recording_time > 0 && opts_min >= (recording_time / 1000000.0))
            break;

        /* finish if limit size exhausted */
        if (limit_filesize != 0 && (limit_filesize * 1024) < url_ftell(&output_files[0]->pb))
            break;

        /* read a frame from it and output it in the fifo */
        is = input_files[file_index];
        if (av_read_frame(is, &pkt) < 0) {
            file_table[file_index].eof_reached = 1;
            if (opt_shortest) break; else continue; //
        }

        if (!pkt.size) {
            stream_no_data = is;
        } else {
            stream_no_data = 0;
        }
        if (do_pkt_dump) {
            av_pkt_dump(stdout, &pkt, do_hex_dump);
        }
        /* the following test is needed in case new streams appear
           dynamically in stream : we ignore them */
        if (pkt.stream_index >= file_table[file_index].nb_streams)
            goto discard_packet;
        ist_index = file_table[file_index].ist_index + pkt.stream_index;
        ist = ist_table[ist_index];
        if (ist->discard)
            goto discard_packet;

//        fprintf(stderr, "next:%lld dts:%lld off:%lld %d\n", ist->next_pts, pkt.dts, input_files_ts_offset[ist->file_index], ist->st->codec->codec_type);
        if (pkt.dts != AV_NOPTS_VALUE && ist->next_pts != AV_NOPTS_VALUE) {
            int64_t delta= av_rescale_q(pkt.dts, ist->st->time_base, AV_TIME_BASE_Q) - ist->next_pts;
            if(ABS(delta) > 10LL*AV_TIME_BASE && !copy_ts){
                input_files_ts_offset[ist->file_index]-= delta;
                if (verbose > 2)
                    fprintf(stderr, "timestamp discontinuity %"PRId64", new offset= %"PRId64"\n", delta, input_files_ts_offset[ist->file_index]);
                for(i=0; i<file_table[file_index].nb_streams; i++){
                    int index= file_table[file_index].ist_index + i;
                    ist_table[index]->next_pts += delta;
                    ist_table[index]->is_start=1;
                }
            }
        }

        //fprintf(stderr,"read #%d.%d size=%d\n", ist->file_index, ist->index, pkt.size);
        if (output_packet(ist, ist_index, ost_table, nb_ostreams, &pkt) < 0) {

            if (verbose >= 0)
                fprintf(stderr, "Error while decoding stream #%d.%d\n",
                        ist->file_index, ist->index);

            av_free_packet(&pkt);
            goto redo;
        }

    discard_packet:
        av_free_packet(&pkt);

        /* dump report by using the output first video and audio streams */
        print_report(output_files, ost_table, nb_ostreams, 0);
    }

    /* at the end of stream, we must flush the decoder buffers */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            output_packet(ist, i, ost_table, nb_ostreams, NULL);
        }
    }

    /* write the trailer if needed and close file */
    for(i=0;i<nb_output_files;i++) {
        os = output_files[i];
        av_write_trailer(os);
    }

    /* dump report by using the first video and audio streams */
    print_report(output_files, ost_table, nb_ostreams, 1);

    /* close each encoder */
    for(i=0;i<nb_ostreams;i++) {
        ost = ost_table[i];
        if (ost->encoding_needed) {
            av_freep(&ost->st->codec->stats_in);
            avcodec_close(ost->st->codec);
        }
    }

    /* close each decoder */
    for(i=0;i<nb_istreams;i++) {
        ist = ist_table[i];
        if (ist->decoding_needed) {
            avcodec_close(ist->st->codec);
        }
    }

    /* finished ! */

    ret = 0;
 fail1:
    av_freep(&bit_buffer);
    av_free(file_table);

    if (ist_table) {
        for(i=0;i<nb_istreams;i++) {
            ist = ist_table[i];
            av_free(ist);
        }
        av_free(ist_table);
    }
    if (ost_table) {
        for(i=0;i<nb_ostreams;i++) {
            ost = ost_table[i];
            if (ost) {
                if (ost->logfile) {
                    fclose(ost->logfile);
                    ost->logfile = NULL;
                }
                fifo_free(&ost->fifo); /* works even if fifo is not
                                          initialized but set to zero */
                av_free(ost->pict_tmp.data[0]);
                if (ost->video_resample)
                    img_resample_close(ost->img_resample_ctx);
                if (ost->audio_resample)
                    audio_resample_close(ost->resample);
                av_free(ost);
            }
        }
        av_free(ost_table);
    }
    return ret;
 fail:
    ret = -ENOMEM;
    goto fail1;
}



#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define RGB_TO_Y(r, g, b) \
((FIX(0.29900) * (r) + FIX(0.58700) * (g) + \
  FIX(0.11400) * (b) + ONE_HALF) >> SCALEBITS)

#define RGB_TO_U(r1, g1, b1, shift)\
(((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +         \
     FIX(0.50000) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V(r1, g1, b1, shift)\
(((FIX(0.50000) * r1 - FIX(0.41869) * g1 -           \
   FIX(0.08131) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)


static void opt_input_file(const char *filename)
{
    AVFormatContext *ic;
    AVFormatParameters params, *ap = &params;
    int err, i, ret, rfps, rfps_base;
    int64_t timestamp;

    if (!strcmp(filename, "-"))
        filename = "pipe:";

    using_stdin |= !strncmp(filename, "pipe:", 5) ||
                   !strcmp( filename, "/dev/stdin" );

    /* get default parameters from command line */
    memset(ap, 0, sizeof(*ap));
    ap->sample_rate = audio_sample_rate;
    ap->channels = audio_channels;
    ap->time_base.den = frame_rate;
    ap->time_base.num = frame_rate_base;
    ap->width = frame_width + frame_padleft + frame_padright;
    ap->height = frame_height + frame_padtop + frame_padbottom;
    ap->image_format = image_format;
    ap->pix_fmt = frame_pix_fmt;
    ap->device  = grab_device;
    ap->channel = video_channel;
    ap->standard = video_standard;
    ap->video_codec_id = video_codec_id;
    ap->audio_codec_id = audio_codec_id;
    if(pgmyuv_compatibility_hack)
        ap->video_codec_id= CODEC_ID_PGMYUV;

    /* open the input file with generic libav function */
    err = av_open_input_file(&ic, filename, file_iformat, 0, ap);
    if (err < 0) {
        print_error(filename, err);
        exit(1);
    }

    if(genpts)
        ic->flags|= AVFMT_FLAG_GENPTS;

    /* If not enough info to get the stream parameters, we decode the
       first frames to get it. (used in mpeg case for example) */
    ret = av_find_stream_info(ic);
    if (ret < 0 && verbose >= 0) {
        fprintf(stderr, "%s: could not find codec parameters\n", filename);
        exit(1);
    }

    timestamp = start_time;
    /* add the stream start time */
    if (ic->start_time != AV_NOPTS_VALUE)
        timestamp += ic->start_time;

    /* if seeking requested, we execute it */
    if (start_time != 0) {
        ret = av_seek_frame(ic, -1, timestamp, AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            fprintf(stderr, "%s: could not seek to position %0.3f\n",
                    filename, (double)timestamp / AV_TIME_BASE);
        }
        /* reset seek info */
        start_time = 0;
    }

    /* update the current parameters so that they match the one of the input stream */
    for(i=0;i<ic->nb_streams;i++) {
        int j;
        AVCodecContext *enc = ic->streams[i]->codec;
#if defined(HAVE_THREADS)
        if(thread_count>1)
            avcodec_thread_init(enc, thread_count);
#endif
        enc->thread_count= thread_count;
        switch(enc->codec_type) {
        case CODEC_TYPE_AUDIO:
            for(j=0; j<opt_name_count; j++){
                AVOption *opt;
                double d= av_get_double(avctx_opts, opt_names[j], &opt);
                if(d==d && (opt->flags&AV_OPT_FLAG_AUDIO_PARAM) && (opt->flags&AV_OPT_FLAG_DECODING_PARAM))
                    av_set_double(enc, opt_names[j], d);
            }
            //fprintf(stderr, "\nInput Audio channels: %d", enc->channels);
            audio_channels = enc->channels;
            audio_sample_rate = enc->sample_rate;
            if(audio_disable)
                ic->streams[i]->discard= AVDISCARD_ALL;
            break;
        case CODEC_TYPE_VIDEO:
            for(j=0; j<opt_name_count; j++){
                AVOption *opt;
                double d= av_get_double(avctx_opts, opt_names[j], &opt);
                if(d==d && (opt->flags&AV_OPT_FLAG_VIDEO_PARAM) && (opt->flags&AV_OPT_FLAG_DECODING_PARAM))
                    av_set_double(enc, opt_names[j], d);
            }
            frame_height = enc->height;
            frame_width = enc->width;
            frame_aspect_ratio = av_q2d(enc->sample_aspect_ratio) * enc->width / enc->height;
            frame_pix_fmt = enc->pix_fmt;
            rfps      = ic->streams[i]->r_frame_rate.num;
            rfps_base = ic->streams[i]->r_frame_rate.den;
            enc->workaround_bugs = workaround_bugs;
            if(enc->lowres) enc->flags |= CODEC_FLAG_EMU_EDGE;
            if(me_threshold)
                enc->debug |= FF_DEBUG_MV;

            if (enc->time_base.den != rfps || enc->time_base.num != rfps_base) {

                if (verbose >= 0)
                    fprintf(stderr,"\nSeems that stream %d comes from film source: %2.2f (%d/%d) -> %2.2f (%d/%d)\n",
                            i, (float)enc->time_base.den / enc->time_base.num, enc->time_base.den, enc->time_base.num,

                    (float)rfps / rfps_base, rfps, rfps_base);
            }
            /* update the current frame rate to match the stream frame rate */
            frame_rate      = rfps;
            frame_rate_base = rfps_base;

            enc->rate_emu = rate_emu;
            if(video_disable)
                ic->streams[i]->discard= AVDISCARD_ALL;
            else if(video_discard)
                ic->streams[i]->discard= video_discard;
            break;
        case CODEC_TYPE_DATA:
            break;
        case CODEC_TYPE_SUBTITLE:
            break;
        case CODEC_TYPE_UNKNOWN:
            break;
        default:
            av_abort();
        }
    }

    input_files[nb_input_files] = ic;
    input_files_ts_offset[nb_input_files] = input_ts_offset - (copy_ts ? 0 : timestamp);
    /* dump the file content */
    if (verbose >= 0)
        dump_format(ic, nb_input_files, filename, 0);

    nb_input_files++;
    file_iformat = NULL;
    file_oformat = NULL;
    image_format = NULL;

    grab_device = NULL;
    video_channel = 0;

    rate_emu = 0;
}

static void opt_grab(const char *arg)
{
    file_iformat = av_find_input_format(arg);
    opt_input_file("");
}

static void check_audio_video_inputs(int *has_video_ptr, int *has_audio_ptr)
{
    int has_video, has_audio, i, j;
    AVFormatContext *ic;

    has_video = 0;
    has_audio = 0;
    for(j=0;j<nb_input_files;j++) {
        ic = input_files[j];
        for(i=0;i<ic->nb_streams;i++) {
            AVCodecContext *enc = ic->streams[i]->codec;
            switch(enc->codec_type) {
            case CODEC_TYPE_AUDIO:
                has_audio = 1;
                break;
            case CODEC_TYPE_VIDEO:
                has_video = 1;
                break;
            case CODEC_TYPE_DATA:
            case CODEC_TYPE_UNKNOWN:
            case CODEC_TYPE_SUBTITLE:
                break;
            default:
                av_abort();
            }
        }
    }
    *has_video_ptr = has_video;
    *has_audio_ptr = has_audio;
}

static void new_video_stream(AVFormatContext *oc)
{
    AVStream *st;
    AVCodecContext *video_enc;
    int codec_id;

    st = av_new_stream(oc, oc->nb_streams);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }
#if defined(HAVE_THREADS)
    if(thread_count>1)
        avcodec_thread_init(st->codec, thread_count);
#endif

    video_enc = st->codec;

    if(video_codec_tag)
        video_enc->codec_tag= video_codec_tag;

    if(   (video_global_header&1)
       || (video_global_header==0 && (oc->oformat->flags & AVFMT_GLOBALHEADER))){
        video_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
        avctx_opts->flags|= CODEC_FLAG_GLOBAL_HEADER;
    }
    if(video_global_header&2){
        video_enc->flags2 |= CODEC_FLAG2_LOCAL_HEADER;
        avctx_opts->flags2|= CODEC_FLAG2_LOCAL_HEADER;
    }

    if (video_stream_copy) {
        st->stream_copy = 1;
        video_enc->codec_type = CODEC_TYPE_VIDEO;
    } else {
        char *p;
        int i;
        AVCodec *codec;

        codec_id = av_guess_codec(oc->oformat, NULL, oc->filename, NULL, CODEC_TYPE_VIDEO);
        if (video_codec_id != CODEC_ID_NONE)
            codec_id = video_codec_id;

        video_enc->codec_id = codec_id;
        codec = avcodec_find_encoder(codec_id);

        for(i=0; i<opt_name_count; i++){
             AVOption *opt;
             double d= av_get_double(avctx_opts, opt_names[i], &opt);
             if(d==d && (opt->flags&AV_OPT_FLAG_VIDEO_PARAM) && (opt->flags&AV_OPT_FLAG_ENCODING_PARAM))
                 av_set_double(video_enc, opt_names[i], d);
        }

        video_enc->bit_rate = video_bit_rate;
        video_enc->bit_rate_tolerance = video_bit_rate_tolerance;
        video_enc->time_base.den = frame_rate;
        video_enc->time_base.num = frame_rate_base;
        if(codec && codec->supported_framerates){
            const AVRational *p= codec->supported_framerates;
            AVRational req= (AVRational){frame_rate, frame_rate_base};
            const AVRational *best=NULL;
            AVRational best_error= (AVRational){INT_MAX, 1};
            for(; p->den!=0; p++){
                AVRational error= av_sub_q(req, *p);
                if(error.num <0) error.num *= -1;
                if(av_cmp_q(error, best_error) < 0){
                    best_error= error;
                    best= p;
                }
            }
            video_enc->time_base.den= best->num;
            video_enc->time_base.num= best->den;
        }

        video_enc->width = frame_width + frame_padright + frame_padleft;
        video_enc->height = frame_height + frame_padtop + frame_padbottom;
        video_enc->sample_aspect_ratio = av_d2q(frame_aspect_ratio*frame_height/frame_width, 255);
        video_enc->pix_fmt = frame_pix_fmt;

        if(codec && codec->pix_fmts){
            const enum PixelFormat *p= codec->pix_fmts;
            for(; *p!=-1; p++){
                if(*p == video_enc->pix_fmt)
                    break;
            }
            if(*p == -1)
                video_enc->pix_fmt = codec->pix_fmts[0];
        }

        if (!intra_only)
            video_enc->gop_size = gop_size;
        else
            video_enc->gop_size = 0;
        if (video_qscale || same_quality) {
            video_enc->flags |= CODEC_FLAG_QSCALE;
            video_enc->global_quality=
                st->quality = FF_QP2LAMBDA * video_qscale;
        }

        if(intra_matrix)
            video_enc->intra_matrix = intra_matrix;
        if(inter_matrix)
            video_enc->inter_matrix = inter_matrix;

        video_enc->pre_me = pre_me;

        if (b_frames) {
            video_enc->max_b_frames = b_frames;
            video_enc->b_quant_factor = 2.0;
        }
        video_enc->qmin = video_qmin;
        video_enc->qmax = video_qmax;
        video_enc->lmin = video_lmin;
        video_enc->lmax = video_lmax;
        video_enc->rc_qsquish = video_qsquish;
        video_enc->mb_lmin = video_mb_lmin;
        video_enc->mb_lmax = video_mb_lmax;
        video_enc->max_qdiff = video_qdiff;
        video_enc->qblur = video_qblur;
        video_enc->qcompress = video_qcomp;
        video_enc->rc_eq = video_rc_eq;
        video_enc->workaround_bugs = workaround_bugs;
        video_enc->thread_count = thread_count;
        p= video_rc_override_string;
        for(i=0; p; i++){
            int start, end, q;
            int e=sscanf(p, "%d,%d,%d", &start, &end, &q);
            if(e!=3){
                fprintf(stderr, "error parsing rc_override\n");
                exit(1);
            }
            video_enc->rc_override=
                av_realloc(video_enc->rc_override,
                           sizeof(RcOverride)*(i+1));
            video_enc->rc_override[i].start_frame= start;
            video_enc->rc_override[i].end_frame  = end;
            if(q>0){
                video_enc->rc_override[i].qscale= q;
                video_enc->rc_override[i].quality_factor= 1.0;
            }
            else{
                video_enc->rc_override[i].qscale= 0;
                video_enc->rc_override[i].quality_factor= -q/100.0;
            }
            p= strchr(p, '/');
            if(p) p++;
        }
        video_enc->rc_override_count=i;

        video_enc->rc_max_rate = video_rc_max_rate;
        video_enc->rc_min_rate = video_rc_min_rate;
        video_enc->rc_buffer_size = video_rc_buffer_size;
        video_enc->rc_initial_buffer_occupancy = video_rc_buffer_size*3/4;
        video_enc->rc_buffer_aggressivity= video_rc_buffer_aggressivity;
        video_enc->rc_initial_cplx= video_rc_initial_cplx;
        video_enc->i_quant_factor = video_i_qfactor;
        video_enc->b_quant_factor = video_b_qfactor;
        video_enc->i_quant_offset = video_i_qoffset;
        video_enc->b_quant_offset = video_b_qoffset;
        video_enc->intra_quant_bias = video_intra_quant_bias;
        video_enc->inter_quant_bias = video_inter_quant_bias;
        video_enc->me_threshold= me_threshold;
        video_enc->mb_threshold= mb_threshold;
        video_enc->intra_dc_precision= intra_dc_precision - 8;
        video_enc->strict_std_compliance = strict;
        video_enc->error_rate = error_rate;
        video_enc->scenechange_threshold= sc_threshold;
        video_enc->me_range = me_range;
        video_enc->me_penalty_compensation= me_penalty_compensation;
        video_enc->frame_skip_threshold= frame_skip_threshold;
        video_enc->frame_skip_factor= frame_skip_factor;
        video_enc->frame_skip_exp= frame_skip_exp;

        if(packet_size){
            video_enc->rtp_mode= 1;
            video_enc->rtp_payload_size= packet_size;
        }

        if (do_psnr)
            video_enc->flags|= CODEC_FLAG_PSNR;

        video_enc->me_method = me_method;

        /* two pass mode */
        if (do_pass) {
            if (do_pass == 1) {
                video_enc->flags |= CODEC_FLAG_PASS1;
            } else {
                video_enc->flags |= CODEC_FLAG_PASS2;
            }
        }
    }

    /* reset some key parameters */
    video_disable = 0;
    video_codec_id = CODEC_ID_NONE;
    video_stream_copy = 0;
}

static void new_audio_stream(AVFormatContext *oc)
{
    AVStream *st;
    AVCodecContext *audio_enc;
    int codec_id, i;

    st = av_new_stream(oc, oc->nb_streams);
    if (!st) {
        fprintf(stderr, "Could not alloc stream\n");
        exit(1);
    }
#if defined(HAVE_THREADS)
    if(thread_count>1)
        avcodec_thread_init(st->codec, thread_count);
#endif

    audio_enc = st->codec;
    audio_enc->codec_type = CODEC_TYPE_AUDIO;

    if(audio_codec_tag)
        audio_enc->codec_tag= audio_codec_tag;

    if (oc->oformat->flags & AVFMT_GLOBALHEADER) {
        audio_enc->flags |= CODEC_FLAG_GLOBAL_HEADER;
        avctx_opts->flags|= CODEC_FLAG_GLOBAL_HEADER;
    }
    if (audio_stream_copy) {
        st->stream_copy = 1;
        audio_enc->channels = audio_channels;
    } else {
        codec_id = av_guess_codec(oc->oformat, NULL, oc->filename, NULL, CODEC_TYPE_AUDIO);

        for(i=0; i<opt_name_count; i++){
            AVOption *opt;
            double d= av_get_double(avctx_opts, opt_names[i], &opt);
            if(d==d && (opt->flags&AV_OPT_FLAG_AUDIO_PARAM) && (opt->flags&AV_OPT_FLAG_ENCODING_PARAM))
                av_set_double(audio_enc, opt_names[i], d);
        }

        if (audio_codec_id != CODEC_ID_NONE)
            codec_id = audio_codec_id;
        audio_enc->codec_id = codec_id;

        audio_enc->bit_rate = audio_bit_rate;
        if (audio_qscale > QSCALE_NONE) {
            audio_enc->flags |= CODEC_FLAG_QSCALE;
            audio_enc->global_quality = st->quality = FF_QP2LAMBDA * audio_qscale;
        }
        audio_enc->strict_std_compliance = strict;
        audio_enc->thread_count = thread_count;
        /* For audio codecs other than AC3 or DTS we limit */
        /* the number of coded channels to stereo   */
        if (audio_channels > 2 && codec_id != CODEC_ID_AC3
            && codec_id != CODEC_ID_DTS) {
            audio_enc->channels = 2;
        } else
            audio_enc->channels = audio_channels;
    }
    audio_enc->sample_rate = audio_sample_rate;
    audio_enc->time_base= (AVRational){1, audio_sample_rate};
    if (audio_language) {
        pstrcpy(st->language, sizeof(st->language), audio_language);
        av_free(audio_language);
        audio_language = NULL;
    }

    /* reset some key parameters */
    audio_disable = 0;
    audio_codec_id = CODEC_ID_NONE;
    audio_stream_copy = 0;
}


static void opt_output_file(const char *filename)
{
    AVFormatContext *oc;
    int use_video, use_audio, input_has_video, input_has_audio;
    AVFormatParameters params, *ap = &params;

    if (!strcmp(filename, "-"))
        filename = "pipe:";

    oc = av_alloc_format_context();

    if (!file_oformat) {
        file_oformat = guess_format(NULL, filename, NULL);
        if (!file_oformat) {
            fprintf(stderr, "Unable for find a suitable output format for '%s'\n",
                    filename);
            exit(1);
        }
    }

    oc->oformat = file_oformat;
    pstrcpy(oc->filename, sizeof(oc->filename), filename);

    if (!strcmp(file_oformat->name, "ffm") &&
        strstart(filename, "http:", NULL)) {
        /* special case for files sent to ffserver: we get the stream
           parameters from ffserver */
        if (read_ffserver_streams(oc, filename) < 0) {
            fprintf(stderr, "Could not read stream parameters from '%s'\n", filename);
            exit(1);
        }
    } else {
        use_video = file_oformat->video_codec != CODEC_ID_NONE || video_stream_copy || video_codec_id != CODEC_ID_NONE;
        use_audio = file_oformat->audio_codec != CODEC_ID_NONE || audio_stream_copy || audio_codec_id != CODEC_ID_NONE;

        /* disable if no corresponding type found and at least one
           input file */
        if (nb_input_files > 0) {
            check_audio_video_inputs(&input_has_video, &input_has_audio);
            if (!input_has_video)
                use_video = 0;
            if (!input_has_audio)
                use_audio = 0;
        }

        /* manual disable */
        if (audio_disable) {
            use_audio = 0;
        }
        if (video_disable) {
            use_video = 0;
        }

        if (use_video) {
            new_video_stream(oc);
        }

        if (use_audio) {
            new_audio_stream(oc);
        }

        if (!oc->nb_streams) {
            fprintf(stderr, "No audio or video streams available\n");
            exit(1);
        }

        oc->timestamp = rec_timestamp;

        if (str_title)
            pstrcpy(oc->title, sizeof(oc->title), str_title);
        if (str_author)
            pstrcpy(oc->author, sizeof(oc->author), str_author);
        if (str_copyright)
            pstrcpy(oc->copyright, sizeof(oc->copyright), str_copyright);
        if (str_comment)
            pstrcpy(oc->comment, sizeof(oc->comment), str_comment);
    }

    output_files[nb_output_files++] = oc;

    /* check filename in case of an image number is expected */
    if (oc->oformat->flags & AVFMT_NEEDNUMBER) {
        if (filename_number_test(oc->filename) < 0) {
            print_error(oc->filename, AVERROR_NUMEXPECTED);
            exit(1);
        }
    }

    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
        /* test if it already exists to avoid loosing precious files */
        if (!file_overwrite &&
            (strchr(filename, ':') == NULL ||
             strstart(filename, "file:", NULL))) {
            if (url_exist(filename)) {
                int c;

                if ( !using_stdin ) {
                    fprintf(stderr,"File '%s' already exists. Overwrite ? [y/N] ", filename);
                    fflush(stderr);
                    c = getchar();
                    if (toupper(c) != 'Y') {
                        fprintf(stderr, "Not overwriting - exiting\n");
                        exit(1);
                    }
                                }
                                else {
                    fprintf(stderr,"File '%s' already exists. Exiting.\n", filename);
                    exit(1);
                                }
            }
        }

        /* open the file */
        if (url_fopen(&oc->pb, filename, URL_WRONLY) < 0) {
            fprintf(stderr, "Could not open '%s'\n", filename);
            exit(1);
        }
    }

    memset(ap, 0, sizeof(*ap));
    ap->image_format = image_format;
    if (av_set_parameters(oc, ap) < 0) {
        fprintf(stderr, "%s: Invalid encoding parameters\n",
                oc->filename);
        exit(1);
    }

    oc->packet_size= mux_packet_size;
    oc->mux_rate= mux_rate;
    oc->preload= (int)(mux_preload*AV_TIME_BASE);
    oc->max_delay= (int)(mux_max_delay*AV_TIME_BASE);
    oc->loop_output = loop_output;

    /* reset some options */
    file_oformat = NULL;
    file_iformat = NULL;
    image_format = NULL;
}

/* prepare dummy protocols for grab */
static void prepare_grab(void)
{
    int has_video, has_audio, i, j;
    AVFormatContext *oc;
    AVFormatContext *ic;
    AVFormatParameters vp1, *vp = &vp1;
    AVFormatParameters ap1, *ap = &ap1;

    /* see if audio/video inputs are needed */
    has_video = 0;
    has_audio = 0;
    memset(ap, 0, sizeof(*ap));
    memset(vp, 0, sizeof(*vp));
    vp->time_base.num= 1;
    for(j=0;j<nb_output_files;j++) {
        oc = output_files[j];
        for(i=0;i<oc->nb_streams;i++) {
            AVCodecContext *enc = oc->streams[i]->codec;
            switch(enc->codec_type) {
            case CODEC_TYPE_AUDIO:
                if (enc->sample_rate > ap->sample_rate)
                    ap->sample_rate = enc->sample_rate;
                if (enc->channels > ap->channels)
                    ap->channels = enc->channels;
                has_audio = 1;
                break;
            case CODEC_TYPE_VIDEO:
                if (enc->width > vp->width)
                    vp->width = enc->width;
                if (enc->height > vp->height)
                    vp->height = enc->height;

                if (vp->time_base.num*(int64_t)enc->time_base.den > enc->time_base.num*(int64_t)vp->time_base.den){
                    vp->time_base = enc->time_base;
                }
                has_video = 1;
                break;
            default:
                av_abort();
            }
        }
    }

    if (has_video == 0 && has_audio == 0) {
        fprintf(stderr, "Output file must have at least one audio or video stream\n");
        exit(1);
    }

    if (has_video) {
        AVInputFormat *fmt1;
        fmt1 = av_find_input_format(video_grab_format);
        vp->device  = video_device;
        vp->channel = video_channel;
        vp->standard = video_standard;
        vp->pix_fmt = frame_pix_fmt;
        if (av_open_input_file(&ic, "", fmt1, 0, vp) < 0) {
            fprintf(stderr, "Could not find video grab device\n");
            exit(1);
        }
        /* If not enough info to get the stream parameters, we decode the
           first frames to get it. */
        if ((ic->ctx_flags & AVFMTCTX_NOHEADER) && av_find_stream_info(ic) < 0) {
            fprintf(stderr, "Could not find video grab parameters\n");
            exit(1);
        }
        /* by now video grab has one stream */
        ic->streams[0]->r_frame_rate.num = vp->time_base.den;
        ic->streams[0]->r_frame_rate.den = vp->time_base.num;
        input_files[nb_input_files] = ic;

        if (verbose >= 0)
            dump_format(ic, nb_input_files, "", 0);

        nb_input_files++;
    }
    if (has_audio && audio_grab_format) {
        AVInputFormat *fmt1;
        fmt1 = av_find_input_format(audio_grab_format);
        ap->device = audio_device;
        if (av_open_input_file(&ic, "", fmt1, 0, ap) < 0) {
            fprintf(stderr, "Could not find audio grab device\n");
            exit(1);
        }
        input_files[nb_input_files] = ic;

        if (verbose >= 0)
            dump_format(ic, nb_input_files, "", 0);

        nb_input_files++;
    }
}

#if defined(CONFIG_WIN32) || defined(CONFIG_OS2)
static int64_t getutime(void)
{
  return av_gettime();
}
#else
static int64_t getutime(void)
{
    struct rusage rusage;

    getrusage(RUSAGE_SELF, &rusage);
    return (rusage.ru_utime.tv_sec * 1000000LL) + rusage.ru_utime.tv_usec;
}
#endif

extern int ffm_nopts;



//const OptionDef options[] = {
//    /* main options */
//    { "formats", 0, {(void*)show_formats}, "show available formats, codecs, protocols, ..." },
//    { "f", HAS_ARG, {(void*)opt_format}, "force format", "fmt" },
//    { "img", HAS_ARG, {(void*)opt_image_format}, "force image format", "img_fmt" },
//    { "i", HAS_ARG, {(void*)opt_input_file}, "input file name", "filename" },
//    { "y", OPT_BOOL, {(void*)&file_overwrite}, "overwrite output files" },
//    { "map", HAS_ARG | OPT_EXPERT, {(void*)opt_map}, "set input stream mapping", "file:stream[:syncfile:syncstream]" },
//    { "map_meta_data", HAS_ARG | OPT_EXPERT, {(void*)opt_map_meta_data}, "set meta data information of outfile from infile", "outfile:infile" },
//    { "t", HAS_ARG, {(void*)opt_recording_time}, "set the recording time", "duration" },
//    { "fs", HAS_ARG | OPT_INT, {(void*)&limit_filesize}, "set the limit file size", "limit_size" }, //
//    { "ss", HAS_ARG, {(void*)opt_start_time}, "set the start time offset", "time_off" },
//    { "itsoffset", HAS_ARG, {(void*)opt_input_ts_offset}, "set the input ts offset", "time_off" },
//    { "title", HAS_ARG | OPT_STRING, {(void*)&str_title}, "set the title", "string" },
//    { "timestamp", HAS_ARG, {(void*)&opt_rec_timestamp}, "set the timestamp", "time" },
//    { "author", HAS_ARG | OPT_STRING, {(void*)&str_author}, "set the author", "string" },
//    { "copyright", HAS_ARG | OPT_STRING, {(void*)&str_copyright}, "set the copyright", "string" },
//    { "comment", HAS_ARG | OPT_STRING, {(void*)&str_comment}, "set the comment", "string" },
//    { "benchmark", OPT_BOOL | OPT_EXPERT, {(void*)&do_benchmark},
//      "add timings for benchmarking" },
//    { "dump", OPT_BOOL | OPT_EXPERT, {(void*)&do_pkt_dump},
//      "dump each input packet" },
//    { "hex", OPT_BOOL | OPT_EXPERT, {(void*)&do_hex_dump},
//      "when dumping packets, also dump the payload" },
//    { "re", OPT_BOOL | OPT_EXPERT, {(void*)&rate_emu}, "read input at native frame rate", "" },
//    { "loop", OPT_BOOL | OPT_EXPERT, {(void*)&loop_input}, "loop (current only works with images)" },
//    { "loop_output", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&loop_output}, "number of times to loop output in formats that support looping (0 loops forever)", "" },
//    { "v", HAS_ARG, {(void*)opt_verbose}, "control amount of logging", "verbose" },
//    { "target", HAS_ARG, {(void*)opt_target}, "specify target file type (\"vcd\", \"svcd\", \"dvd\", \"dv\", \"pal-vcd\", \"ntsc-svcd\", ...)", "type" },
//    { "threads", HAS_ARG | OPT_EXPERT, {(void*)opt_thread_count}, "thread count", "count" },
//    { "vsync", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&video_sync_method}, "video sync method", "" },
//    { "async", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&audio_sync_method}, "audio sync method", "" },
//    { "vglobal", HAS_ARG | OPT_INT | OPT_EXPERT, {(void*)&video_global_header}, "video global header storage type", "" },
//    { "copyts", OPT_BOOL | OPT_EXPERT, {(void*)&copy_ts}, "copy timestamps" },
//    { "shortest", OPT_BOOL | OPT_EXPERT, {(void*)&opt_shortest}, "finish encoding within shortest input" }, //
//
//    /* video options */
//    { "b", HAS_ARG | OPT_VIDEO, {(void*)opt_video_bitrate}, "set video bitrate (in kbit/s)", "bitrate" },
//    { "vframes", OPT_INT | HAS_ARG | OPT_VIDEO, {(void*)&max_frames[CODEC_TYPE_VIDEO]}, "set the number of video frames to record", "number" },
//    { "aframes", OPT_INT | HAS_ARG | OPT_AUDIO, {(void*)&max_frames[CODEC_TYPE_AUDIO]}, "set the number of audio frames to record", "number" },
//    { "dframes", OPT_INT | HAS_ARG, {(void*)&max_frames[CODEC_TYPE_DATA]}, "set the number of data frames to record", "number" },
//    { "r", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_rate}, "set frame rate (Hz value, fraction or abbreviation)", "rate" },
//    { "s", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_size}, "set frame size (WxH or abbreviation)", "size" },
//    { "aspect", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_aspect_ratio}, "set aspect ratio (4:3, 16:9 or 1.3333, 1.7777)", "aspect" },
//    { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_frame_pix_fmt}, "set pixel format", "format" },
//    { "croptop", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_top}, "set top crop band size (in pixels)", "size" },
//    { "cropbottom", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_bottom}, "set bottom crop band size (in pixels)", "size" },
//    { "cropleft", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_left}, "set left crop band size (in pixels)", "size" },
//    { "cropright", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_crop_right}, "set right crop band size (in pixels)", "size" },
//    { "padtop", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_top}, "set top pad band size (in pixels)", "size" },
//    { "padbottom", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_bottom}, "set bottom pad band size (in pixels)", "size" },
//    { "padleft", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_left}, "set left pad band size (in pixels)", "size" },
//    { "padright", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_pad_right}, "set right pad band size (in pixels)", "size" },
//    { "padcolor", HAS_ARG | OPT_VIDEO, {(void*)opt_pad_color}, "set color of pad bands (Hex 000000 thru FFFFFF)", "color" },
//    { "g", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_gop_size}, "set the group of picture size", "gop_size" },
//    { "intra", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&intra_only}, "use only intra frames"},
//    { "vn", OPT_BOOL | OPT_VIDEO, {(void*)&video_disable}, "disable video" },
//    { "vdt", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&video_discard}, "discard threshold", "n" },
//    { "qscale", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qscale}, "use fixed video quantiser scale (VBR)", "q" },
//    { "qmin", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qmin}, "min video quantiser scale (VBR)", "q" },
//    { "qmax", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qmax}, "max video quantiser scale (VBR)", "q" },
//    { "lmin", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_lmin}, "min video lagrange factor (VBR)", "lambda" },
//    { "lmax", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_lmax}, "max video lagrange factor (VBR)", "lambda" },
//    { "mblmin", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_mb_lmin}, "min macroblock quantiser scale (VBR)", "q" },
//    { "mblmax", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_mb_lmax}, "max macroblock quantiser scale (VBR)", "q" },
//    { "qdiff", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qdiff}, "max difference between the quantiser scale (VBR)", "q" },
//    { "qblur", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qblur}, "video quantiser scale blur (VBR)", "blur" },
//    { "qsquish", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qsquish}, "how to keep quantiser between qmin and qmax (0 = clip, 1 = use differentiable function)", "squish" },
//    { "qcomp", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_qcomp}, "video quantiser scale compression (VBR)", "compression" },
//    { "rc_init_cplx", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_rc_initial_cplx}, "initial complexity for 1-pass encoding", "complexity" },
//    { "b_qfactor", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_b_qfactor}, "qp factor between p and b frames", "factor" },
//    { "i_qfactor", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_i_qfactor}, "qp factor between p and i frames", "factor" },
//    { "b_qoffset", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_b_qoffset}, "qp offset between p and b frames", "offset" },
//    { "i_qoffset", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_i_qoffset}, "qp offset between p and i frames", "offset" },
//    { "ibias", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_ibias}, "intra quant bias", "bias" },
//    { "pbias", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_pbias}, "inter quant bias", "bias" },
//    { "rc_eq", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_video_rc_eq}, "set rate control equation", "equation" },
//    { "rc_override", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_video_rc_override_string}, "rate control override for specific intervals", "override" },
//    { "bt", HAS_ARG | OPT_VIDEO, {(void*)opt_video_bitrate_tolerance}, "set video bitrate tolerance (in kbit/s)", "tolerance" },
//    { "maxrate", HAS_ARG | OPT_VIDEO, {(void*)opt_video_bitrate_max}, "set max video bitrate tolerance (in kbit/s)", "bitrate" },
//    { "minrate", HAS_ARG | OPT_VIDEO, {(void*)opt_video_bitrate_min}, "set min video bitrate tolerance (in kbit/s)", "bitrate" },
//    { "bufsize", HAS_ARG | OPT_VIDEO, {(void*)opt_video_buffer_size}, "set ratecontrol buffer size (in kByte)", "size" },
//    { "vcodec", HAS_ARG | OPT_VIDEO, {(void*)opt_video_codec}, "force video codec ('copy' to copy stream)", "codec" },
//    { "me", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_motion_estimation}, "set motion estimation method",
//      "method" },
//    { "me_threshold", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_me_threshold}, "motion estimaton threshold",  "" },
//    { "mb_threshold", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_mb_threshold}, "macroblock threshold",  "" },
//    { "bf", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_b_frames}, "use 'frames' B frames", "frames" },
//    { "preme", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_pre_me}, "pre motion estimation", "" },
//    { "bug", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_workaround_bugs}, "workaround not auto detected encoder bugs", "param" },
//    { "ps", HAS_ARG | OPT_EXPERT, {(void*)opt_packet_size}, "set packet size in bits", "size" },
//    { "error", HAS_ARG | OPT_EXPERT, {(void*)opt_error_rate}, "error rate", "rate" },
//    { "strict", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_strict}, "how strictly to follow the standards", "strictness" },
//    { "sameq", OPT_BOOL | OPT_VIDEO, {(void*)&same_quality},
//      "use same video quality as source (implies VBR)" },
//    { "pass", HAS_ARG | OPT_VIDEO, {(void*)&opt_pass}, "select the pass number (1 or 2)", "n" },
//    { "passlogfile", HAS_ARG | OPT_STRING | OPT_VIDEO, {(void*)&pass_logfilename}, "select two pass log file name", "file" },
//    { "deinterlace", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&do_deinterlace},
//      "deinterlace pictures" },
//    { "psnr", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&do_psnr}, "calculate PSNR of compressed frames" },
//    { "vstats", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, {(void*)&do_vstats}, "dump video coding statistics to file" },
//    { "vhook", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)add_frame_hooker}, "insert video processing module", "module" },
//    { "intra_matrix", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_intra_matrix}, "specify intra matrix coeffs", "matrix" },
//    { "inter_matrix", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_inter_matrix}, "specify inter matrix coeffs", "matrix" },
//    { "top", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_top_field_first}, "top=1/bottom=0/auto=-1 field first", "" },
//    { "sc_threshold", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_sc_threshold}, "scene change threshold", "threshold" },
//    { "me_range", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_me_range}, "limit motion vectors range (1023 for DivX player)", "range" },
//    { "dc", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&intra_dc_precision}, "intra_dc_precision", "precision" },
//    { "mepc", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&me_penalty_compensation}, "motion estimation bitrate penalty compensation", "factor (1.0 = 256)" },
//    { "vtag", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_video_tag}, "force video tag/fourcc", "fourcc/tag" },
//    { "skip_threshold", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&frame_skip_threshold}, "frame skip threshold", "threshold" },
//    { "skip_factor", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&frame_skip_factor}, "frame skip factor", "factor" },
//    { "skip_exp", OPT_INT | HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)&frame_skip_exp}, "frame skip exponent", "exponent" },
//    { "newvideo", OPT_VIDEO, {(void*)opt_new_video_stream}, "add a new video stream to the current output stream" },
//    { "genpts", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, { (void *)&genpts }, "generate pts" },
//    { "qphist", OPT_BOOL | OPT_EXPERT | OPT_VIDEO, { (void *)&qp_hist }, "show QP histogram" },
//
//    /* audio options */
//    { "ab", HAS_ARG | OPT_AUDIO, {(void*)opt_audio_bitrate}, "set audio bitrate (in kbit/s)", "bitrate", },
//    { "aq", OPT_FLOAT | HAS_ARG | OPT_AUDIO, {(void*)&audio_qscale}, "set audio quality (codec-specific)", "quality", },
//    { "ar", HAS_ARG | OPT_AUDIO, {(void*)opt_audio_rate}, "set audio sampling rate (in Hz)", "rate" },
//    { "ac", HAS_ARG | OPT_AUDIO, {(void*)opt_audio_channels}, "set number of audio channels", "channels" },
//    { "an", OPT_BOOL | OPT_AUDIO, {(void*)&audio_disable}, "disable audio" },
//    { "acodec", HAS_ARG | OPT_AUDIO, {(void*)opt_audio_codec}, "force audio codec ('copy' to copy stream)", "codec" },
//    { "atag", HAS_ARG | OPT_EXPERT | OPT_AUDIO, {(void*)opt_audio_tag}, "force audio tag/fourcc", "fourcc/tag" },
//    { "vol", OPT_INT | HAS_ARG | OPT_AUDIO, {(void*)&audio_volume}, "change audio volume (256=normal)" , "volume" }, //
//    { "newaudio", OPT_AUDIO, {(void*)opt_new_audio_stream}, "add a new audio stream to the current output stream" },
//    { "alang", HAS_ARG | OPT_STRING | OPT_AUDIO, {(void *)&audio_language}, "set the ISO 639 language code (3 letters) of the current audio stream" , "code" },
//
//    /* subtitle options */
//    { "scodec", HAS_ARG | OPT_SUBTITLE, {(void*)opt_subtitle_codec}, "force subtitle codec ('copy' to copy stream)", "codec" },
//    { "newsubtitle", OPT_SUBTITLE, {(void*)opt_new_subtitle_stream}, "add a new subtitle stream to the current output stream" },
//    { "slang", HAS_ARG | OPT_STRING | OPT_SUBTITLE, {(void *)&subtitle_language}, "set the ISO 639 language code (3 letters) of the current subtitle stream" , "code" },
//
//    /* grab options */
//    { "vd", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_video_device}, "set video grab device", "device" },
//    { "vc", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_video_channel}, "set video grab channel (DV1394 only)", "channel" },
//    { "tvstd", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_video_standard}, "set television standard (NTSC, PAL (SECAM))", "standard" },
//    { "ad", HAS_ARG | OPT_EXPERT | OPT_AUDIO | OPT_GRAB, {(void*)opt_audio_device}, "set audio device", "device" },
//
//    /* G.2 grab options */
//    { "grab", HAS_ARG | OPT_EXPERT | OPT_GRAB, {(void*)opt_grab}, "request grabbing using", "format" },
//    { "gd", HAS_ARG | OPT_EXPERT | OPT_VIDEO | OPT_GRAB, {(void*)opt_grab_device}, "set grab device", "device" },
//
//    /* muxer options */
//    { "muxrate", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&mux_rate}, "set mux rate", "rate" },
//    { "packetsize", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&mux_packet_size}, "set packet size", "size" },
//    { "muxdelay", OPT_FLOAT | HAS_ARG | OPT_EXPERT, {(void*)&mux_max_delay}, "set the maximum demux-decode delay", "seconds" },
//    { "muxpreload", OPT_FLOAT | HAS_ARG | OPT_EXPERT, {(void*)&mux_preload}, "set the initial demux-decode delay", "seconds" },
//    { "default", OPT_FUNC2 | HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, {(void*)opt_default}, "generic catch all option", "" },
//    { NULL, },
//};


void parse_arg_file(const char *filename)
{
    opt_output_file(filename);
}

void ffmpeg_init()
{
    av_register_all();

    avctx_opts= avcodec_alloc_context();
}

void ffmpeg_deinit()
{
    av_free_static();
}

int ffmpeg_do_transcode(char *in_file, char *out_file, int avitrate, int vbitrate,
		int size_v, int size_h, int pad_v, int pad_h, char *title,
		int(*cb)(void *, int), void *ptr)
{
	int i;
	received_sigterm = 0;
	file_overwrite = 1;
	nb_input_files = nb_output_files = nb_stream_maps = nb_meta_data_maps = 0;
	
	audio_disable = video_disable = 0;
	recording_time = 0;
	start_time = 0;

	cpp_passed_ptr = ptr;
	cpp_callback = cb;

	opt_input_file(in_file);

	// PSP codec params
	audio_channels = 2;
	audio_sample_rate = 24000;
	frame_rate = 30000000;
	frame_rate_base = 1001000;
	
	// size & padding	
	frame_width = size_h;
	frame_height = size_v;
	frame_padtop = frame_padbottom = pad_v;
	frame_padleft = frame_padright = pad_h;

	// codecs
	file_iformat = 0;
	file_oformat = guess_format("psp", 0, 0);
	
	// rates
	video_bit_rate = vbitrate * 1000;
	audio_bit_rate = avitrate * 1000;
	
	str_title = title;
	opt_output_file(out_file);
	
    av_encode(output_files, nb_output_files, input_files, nb_input_files,
              stream_maps, nb_stream_maps);

    /* close files */
    for(i=0;i<nb_output_files;i++) {
        /* maybe av_close_output_file ??? */
        AVFormatContext *s = output_files[i];
        int j;
        if (!(s->oformat->flags & AVFMT_NOFILE))
            url_fclose(&s->pb);
        for(j=0;j<s->nb_streams;j++)
            av_free(s->streams[j]);
        av_free(s);
    }
    for(i=0;i<nb_input_files;i++) {
        av_close_input_file(input_files[i]);
    }
    
    if(intra_matrix) {
        av_free(intra_matrix);
    }
    if(inter_matrix) {
        av_free(inter_matrix);
    }
    
	return 1;
}

/*
int ffmpeg_main(int argc, char **argv, int(*cb)(void *, int), void *ptr)
{
    int i;
    int64_t ti;

	received_sigterm = 0;
	nb_input_files = nb_output_files = nb_stream_maps = nb_meta_data_maps = 0;
	file_oformat = file_iformat = 0;
	audio_disable = video_disable = 0;
	recording_time = 0;
	start_time = 0;
	frame_padtop = frame_padbottom = 0;
	frame_padleft = frame_padright = 0;

    parse_options(argc, argv, options);

    if ( (nb_output_files != 1) || (nb_input_files != 1)) {
        fprintf(stderr, "Must supply at least one output file\n");
        return 0;
    }

	cpp_passed_ptr = ptr;
	cpp_callback = cb;
	
    ti = getutime();
    av_encode(output_files, nb_output_files, input_files, nb_input_files,
              stream_maps, nb_stream_maps);
    ti = getutime() - ti;

    for(i=0;i<nb_output_files;i++) {
        AVFormatContext *s = output_files[i];
        int j;
        if (!(s->oformat->flags & AVFMT_NOFILE))
            url_fclose(&s->pb);
        for(j=0;j<s->nb_streams;j++)
            av_free(s->streams[j]);
        av_free(s);
    }
    for(i=0;i<nb_input_files;i++)
        av_close_input_file(input_files[i]);

    if(intra_matrix)
        av_free(intra_matrix);
    if(inter_matrix)
        av_free(inter_matrix);

    return 0;
}
*/