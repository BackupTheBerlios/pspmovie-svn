// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//
#ifndef FFMPEG_GLUE_H_
#define FFMPEG_GLUE_H_

#ifdef __cplusplus
extern "C" {
#endif

int ffmpeg_main(int argc, char **argv, int(*cb)(void *, int), void *ptr);
int ffmpeg_do_transcode(char *in_file, char *out_file, int avitrate, int vbitrate,
		int size_v, int size_h, int pad_v, int pad_h, char *title,
		int(*cb)(void *, int), void *ptr);

void ffmpeg_init();

void ffmpeg_deinit();

#ifdef __cplusplus
}
#endif

#endif /*FFMPEG_GLUE_H_*/
