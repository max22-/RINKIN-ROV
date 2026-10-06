#include <vector>
#include <imgui.h>

#include "video.h"
#include <rlgl.h>

void thread_func(Video *v) {
    while(v->is_playing()) {
        while(av_read_frame(v->format_ctx, v->packet) >= 0) {
            if(v->packet->stream_index == v->video_stream->index) {
                int rc = 0;
                if(v->recording.is_recording) {
                    AVPacket packet;
                    rc = av_packet_ref(&packet, v->packet);
                    if(rc < 0) {
                        TraceLog(LOG_ERROR, "av_packet_ref failed");
                        av_packet_unref(&packet);
                        goto skip_recording;
                    }
                    packet.stream_index = v->video_stream->index;
                    av_packet_rescale_ts(&packet, v->video_stream->time_base, v->recording.video_stream->time_base);
                    rc = av_interleaved_write_frame(v->recording.format_ctx, &packet);
                    if(rc < 0)
                        TraceLog(LOG_ERROR, "failed to write frame");
                    av_packet_unref(&packet);
                    
                }

                skip_recording:

                rc = avcodec_send_packet(v->video_codec_ctx, v->packet);
                av_packet_unref(v->packet);
                if(rc < 0) {
                    TraceLog(LOG_ERROR, "error sending packet");
                    continue;
                }
                while(rc >= 0) {
                    rc = avcodec_receive_frame(v->video_codec_ctx, v->frame);
                    if(rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) break;
                    v->rgb_frame_mutex.lock();
                    sws_scale(v->sws_ctx, (uint8_t const *const *)v->frame->data, v->frame->linesize, 0,
                              v->frame->height, v->rgb_frame->data, v->rgb_frame->linesize);
                    v->rgb_frame_mutex.unlock();
                }
                break;
            }
        }
    }
}

Video::Video(std::string url, uint16_t width, uint16_t height) : url(url), width(width), height(height), _is_playing(false) {
    texture.id = rlLoadTexture(NULL, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
    texture.width = width;
    texture.height = height;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    texture.mipmaps = 1;
    recording.is_recording = false;
    recording.key_frame_received = false;
    recording.format_ctx = nullptr;
}

Video::~Video() {
    if(is_recording())
        stop_recording();
    stop();
    rlUnloadTexture(texture.id);
}

#define TRACELOG_AV_ERROR(x) \
    do { \
        char buf[AV_ERROR_MAX_STRING_SIZE]; \
        av_strerror(x, buf, AV_ERROR_MAX_STRING_SIZE); \
        TraceLog(LOG_ERROR, "%s", buf); \
    } while(0)

void Video::start() {
    if(is_playing()) return;
//FFmpeg stuff
    AVDictionary *options = nullptr;
    if(av_dict_set(&options, "flags", "nobuffer", 0) < 0) {
        TraceLog(LOG_ERROR, "failed to set FFmpeg options");
        return;
    }
    if(av_dict_set(&options, "flags", "low_delay", 0) < 0) {
        TraceLog(LOG_ERROR, "failed to set FFmpeg options");
        return;
    }
    if(av_dict_set(&options, "max_delay", "0", 0) < 0) {
        TraceLog(LOG_ERROR, "failed to set FFmpeg options");
        return;
    }
    format_ctx = avformat_alloc_context();
    int rc = avformat_open_input(&format_ctx, url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if(rc < 0) {
        TRACELOG_AV_ERROR(rc);
        return;
    }

    TraceLog(LOG_INFO, "CODEC: format %s", format_ctx->iformat->long_name);

    rc = avformat_find_stream_info(format_ctx, nullptr);
    if(rc < 0) {
        TRACELOG_AV_ERROR(rc);
        return;
    }

    AVCodecParameters *video_params = nullptr;
    for(unsigned int i = 0; i < format_ctx->nb_streams; i++) {
        AVStream *tmp_stream = format_ctx->streams[i];
        AVCodecParameters *tmp_params = tmp_stream->codecpar;
        if(tmp_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = tmp_stream;
            video_params = tmp_params;
            TraceLog(LOG_INFO, "CODEC: Resolution: %d x %d, type: %d", video_params->width, video_params->height, video_params->codec_id);
        }
    }
    if(!video_stream) {
        TraceLog(LOG_ERROR, "failed to find video stream");
        return;
    }

    // Debug
    TraceLog(LOG_INFO, "CODEC: Resolution: %d x %d, type: %d", video_params->width, video_params->height, video_params->codec_id);

    const AVCodec *video_codec = avcodec_find_decoder(video_params->codec_id);
    if(!video_codec) {
        TraceLog(LOG_ERROR, "failed to find video codec");
        return;
    }

    TraceLog(LOG_INFO, "CODEC: %s ID %d, Bit rate %ld", video_codec->name, video_codec->id, video_params->bit_rate);
    TraceLog(LOG_INFO, "FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d", video_stream->avg_frame_rate.num,
             video_stream->avg_frame_rate.den, video_stream->r_frame_rate.num,
             video_stream->r_frame_rate.den, video_stream->time_base.num, video_stream->time_base.den);

    video_codec_ctx = avcodec_alloc_context3(video_codec);
    if(!video_codec_ctx) {
        TraceLog(LOG_ERROR, "failed to allocate video codec context");
        return;
    }

    rc = avcodec_parameters_to_context(video_codec_ctx, video_params);
    if(rc < 0) {
        TRACELOG_AV_ERROR(rc);
        return;
    }

    // Debug
    TraceLog(LOG_INFO, "Codec context: width = %d, height = %d", video_codec_ctx->width, video_codec_ctx->height);

    rc = avcodec_open2(video_codec_ctx, video_codec, NULL);
    if(rc < 0) {
        TraceLog(LOG_ERROR, "failed to open codec");
        return;
    }

    frame = av_frame_alloc();
    if(!frame) {
        TraceLog(LOG_ERROR, "failed to allocate frame");
        return;
    }

    packet = av_packet_alloc();
    if(!packet) {
        TraceLog(LOG_ERROR, "failed to allocate packet");
        return;
    }

    TraceLog(LOG_INFO, "Codec context: width = %d, height = %d", video_codec_ctx->width, video_codec_ctx->height);

    sws_ctx = sws_getContext(video_codec_ctx->width, video_codec_ctx->height, video_codec_ctx->pix_fmt,
                             width, height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
    if(!sws_ctx) {
        TraceLog(LOG_ERROR, "failed to get sws context");
        return;
    }

    rgb_frame = av_frame_alloc();
    if(!rgb_frame) {
        TraceLog(LOG_ERROR, "failed to allocate RGB frame");
        return;
    }

    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = width;
    rgb_frame->height = height;

    rc = av_frame_get_buffer(rgb_frame, 0);
    if(rc < 0) {
        TRACELOG_AV_ERROR(rc);
        return;
    }

    _is_playing = true;
    thread = new std::thread(thread_func, this);
}

void Video::stop() {
    if(is_recording()) stop_recording();
    if(_is_playing.exchange(false)) {
        if(thread->joinable()) thread->join();
        av_frame_free(&frame);
        av_frame_free(&rgb_frame);
        av_packet_unref(packet);
        av_packet_free(&packet);
        avcodec_free_context(&video_codec_ctx);
        sws_freeContext(sws_ctx);
        avformat_close_input(&format_ctx);
        std::vector<unsigned char> black(width * height * 3, 0);
        UpdateTexture(texture, black.data());
    }
}

bool Video::is_playing() {
    return _is_playing;
}

void Video::rotation(bool active) {
    rotate = active;
}

void Video::display() {
    if(is_playing()) {
        rgb_frame_mutex.lock();
        UpdateTexture(texture, rgb_frame->data[0]);
        rgb_frame_mutex.unlock();
    }
    ImVec2 uv0, uv1;
    if(rotate) {
        uv0 = ImVec2(1.0f, 1.0f);
        uv1 = ImVec2(0.0f, 0.0f);
    } else {
        uv0 = ImVec2(0.0f, 0.0f);
        uv1 = ImVec2(1.0f, 1.0f);
    }
    ImGui::Image(ImTextureID(texture.id), ImVec2(texture.width, texture.height), uv0, uv1);
}

bool Video::capture_image(const char *file_name) {
    bool ret = false;
    if(is_playing()) {
        rgb_frame_mutex.lock();
        Image img = LoadImageFromTexture(texture);
        ret = ExportImage(img, file_name);
        rgb_frame_mutex.unlock();
    }
    return ret;
}

void Video::start_recording(const char *file_name) {
    const char *err = nullptr;
    int rc = 0;
    if(!is_playing()) {
        err = "video stream is not started";
        goto cleanup0;
    }
    if(is_recording()) {
        err = "already recording video";
        goto cleanup0;
    }
    rc = avformat_alloc_output_context2(&recording.format_ctx, NULL, "mp4", file_name);
    if(rc < 0) {
        err = "failed to start recording";
        goto cleanup0;
    }
    recording.video_stream = avformat_new_stream(recording.format_ctx, NULL);
    if(recording.video_stream == NULL) {
        err = "failed to create output stream";
        goto cleanup1;
    }
    rc = avcodec_parameters_copy(recording.video_stream->codecpar, video_stream->codecpar);
    if(rc < 0) {
        err = "failed to copy codec parameters";
        goto cleanup1;
    }
    recording.video_stream->time_base = video_stream->time_base;
    rc = avio_open(&recording.format_ctx->pb, file_name, AVIO_FLAG_WRITE);
    if(rc < 0) {
        err = "failed to open video file";
        goto cleanup1;
    }
    rc = avformat_write_header(recording.format_ctx, NULL);
    if(rc < 0) {
        err = "failed to write header";
        goto cleanup2;
    }
    recording.is_recording = true;
    return;

cleanup2:
    avio_closep(&recording.format_ctx->pb);
cleanup1:
    avformat_free_context(recording.format_ctx);
cleanup0:
    recording.format_ctx = nullptr;
    recording.video_stream = nullptr;
    TraceLog(LOG_ERROR, err);
}

void Video::stop_recording() {
    av_write_trailer(recording.format_ctx);
    avio_closep(&recording.format_ctx->pb);
    avformat_free_context(recording.format_ctx);
    recording.format_ctx = nullptr;
    recording.video_stream = nullptr;
    recording.is_recording = false;
    recording.key_frame_received = false;
}

bool Video::is_recording() {
    return recording.is_recording;
}
