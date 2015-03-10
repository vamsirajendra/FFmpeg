/*
 * 012v decoder
 *
 * Copyright (C) 2012 Carl Eugen Hoyos
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "internal.h"
#include "libavutil/intreadwrite.h"

static av_cold int zero12v_decode_init(AVCodecContext *avctx)
{
    avctx->pix_fmt             = PIX_FMT_YUV422P16;
    avctx->bits_per_raw_sample = 10;

    avctx->coded_frame = avcodec_alloc_frame();
    if (!avctx->coded_frame)
        return AVERROR(ENOMEM);

    if (avctx->codec_tag == MKTAG('a', '1', '2', 'v'))
        av_log_ask_for_sample(avctx, "Samples with actual transparency needed\n");

    avctx->coded_frame->pict_type = AV_PICTURE_TYPE_I;
    avctx->coded_frame->key_frame = 1;
    return 0;
}

static int zero12v_decode_frame(AVCodecContext *avctx, void *data,
                                int *got_frame, AVPacket *avpkt)
{
    int line, ret;
    const int width = avctx->width;
    AVFrame *pic = avctx->coded_frame;
    uint16_t *y, *u, *v;
    const uint8_t *line_end, *src = avpkt->data;
    int stride = avctx->width * 8 / 3;

    if (pic->data[0])
        avctx->release_buffer(avctx, pic);

    if (width <= 1 || avctx->height <= 0) {
        av_log(avctx, AV_LOG_ERROR, "Dimensions %dx%d not supported.\n", width, avctx->height);
        return AVERROR_INVALIDDATA;
    }
    if (avpkt->size < avctx->height * stride) {
        av_log(avctx, AV_LOG_ERROR, "Packet too small: %d instead of %d\n",
               avpkt->size, avctx->height * stride);
        return AVERROR_INVALIDDATA;
    }

    pic->reference = 0;
    if ((ret = ff_get_buffer(avctx, pic)) < 0)
        return ret;

    line_end = avpkt->data + stride;
    for (line = 0; line < avctx->height; line++) {
        uint16_t y_temp[6] = {0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000};
        uint16_t u_temp[3] = {0x8000, 0x8000, 0x8000};
        uint16_t v_temp[3] = {0x8000, 0x8000, 0x8000};
        int x;
        y = (uint16_t *)(pic->data[0] + line * pic->linesize[0]);
        u = (uint16_t *)(pic->data[1] + line * pic->linesize[1]);
        v = (uint16_t *)(pic->data[2] + line * pic->linesize[2]);

        for (x = 0; x < width; x += 6) {
            uint32_t t;

            if (width - x < 6 || line_end - src < 16) {
                y = y_temp;
                u = u_temp;
                v = v_temp;
            }

            if (line_end - src < 4)
                break;

            t = AV_RL32(src);
            src += 4;
            *u++ = t <<  6 & 0xFFC0;
            *y++ = t >>  4 & 0xFFC0;
            *v++ = t >> 14 & 0xFFC0;

            if (line_end - src < 4)
                break;

            t = AV_RL32(src);
            src += 4;
            *y++ = t <<  6 & 0xFFC0;
            *u++ = t >>  4 & 0xFFC0;
            *y++ = t >> 14 & 0xFFC0;

            if (line_end - src < 4)
                break;

            t = AV_RL32(src);
            src += 4;
            *v++ = t <<  6 & 0xFFC0;
            *y++ = t >>  4 & 0xFFC0;
            *u++ = t >> 14 & 0xFFC0;

            if (line_end - src < 4)
                break;

            t = AV_RL32(src);
            src += 4;
            *y++ = t <<  6 & 0xFFC0;
            *v++ = t >>  4 & 0xFFC0;
            *y++ = t >> 14 & 0xFFC0;

            if (width - x < 6)
                break;
        }

        if (x < width) {
            y = x   + (uint16_t *)(pic->data[0] + line * pic->linesize[0]);
            u = x/2 + (uint16_t *)(pic->data[1] + line * pic->linesize[1]);
            v = x/2 + (uint16_t *)(pic->data[2] + line * pic->linesize[2]);
            memcpy(y, y_temp, sizeof(*y) * (width - x));
            memcpy(u, u_temp, sizeof(*u) * (width - x + 1) / 2);
            memcpy(v, v_temp, sizeof(*v) * (width - x + 1) / 2);
        }

        line_end += stride;
        src = line_end - stride;
    }

    *got_frame = 1;
    *(AVFrame*)data= *avctx->coded_frame;

    return avpkt->size;
}

static av_cold int zero12v_decode_close(AVCodecContext *avctx)
{
    AVFrame *pic = avctx->coded_frame;
    if (pic->data[0])
        avctx->release_buffer(avctx, pic);
    av_freep(&avctx->coded_frame);

    return 0;
}

AVCodec ff_zero12v_decoder = {
    .name           = "012v",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = AV_CODEC_ID_012V,
    .init           = zero12v_decode_init,
    .close          = zero12v_decode_close,
    .decode         = zero12v_decode_frame,
    .capabilities   = CODEC_CAP_DR1,
    .long_name      = NULL_IF_CONFIG_SMALL("Uncompressed 4:2:2 10-bit"),
};
