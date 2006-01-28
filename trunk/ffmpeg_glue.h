#ifndef FFMPEG_GLUE_H_
#define FFMPEG_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

int ffmpeg_main(int argc, char **argv, int(*cb)(void *, int), void *ptr);

void ffmpeg_init();

void ffmpeg_deinit();

#ifdef __cplusplus
}
#endif

#endif /*FFMPEG_GLUE_H_*/