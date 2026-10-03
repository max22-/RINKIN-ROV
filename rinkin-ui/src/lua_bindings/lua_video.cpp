#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <lua.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <rlgl.h>
#include "lua_video.h"
#include "../util.h"

static const char metatable_name[] = "video";

struct Video {
    Video() : is_running(false) {}
    std::string url;
    unsigned int width, height;
    Texture2D texture;
    bool rotate = false;
    // FFmpeg stuff
    AVFormatContext *format_ctx = nullptr;
    AVStream *video_stream = nullptr;
    AVCodecContext *video_codec_ctx = nullptr;
    AVFrame *frame = nullptr, *rgb_frame = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *sws_ctx = nullptr;
    std::atomic_bool is_running;
    std::mutex rgb_frame_mutex;
    std::thread *thread;
    struct Recording {
        std::atomic_bool is_recording, key_frame_received;
        AVFormatContext *format_ctx;
        AVStream *video_stream;
    } recording;
};

static Video *checkvideo(lua_State *L) {
    void *ud = luaL_checkudata(L, 1, metatable_name);
    luaL_argcheck(L, ud != NULL, 1, "`video' expected");
    return (Video*)ud;
}

static void video_stop(Video *v) {
    if(v->is_running.exchange(false)) {
        if(v->thread->joinable()) v->thread->join();
        av_frame_free(&v->frame);
        av_frame_free(&v->rgb_frame);
        av_packet_unref(v->packet);
        av_packet_free(&v->packet);
        avcodec_free_context(&v->video_codec_ctx);
        sws_freeContext(v->sws_ctx);
        avformat_close_input(&v->format_ctx);
        std::vector<unsigned char> black(v->width * v->height * 3, 0);
        UpdateTexture(v->texture, black.data());
    }
}

#define LUA_AV_ERROR(x) \
    do { \
        char buf[AV_ERROR_MAX_STRING_SIZE]; \
        av_strerror(x, buf, AV_ERROR_MAX_STRING_SIZE); \
        luaL_error(L, "%s", buf); \
    } while(0)

static int lua_video_new(lua_State *L) {
    const char *url = luaL_checkstring(L, 1);
    int width = luaL_checkinteger(L, 2);
    if(width <= 0) luaL_error(L, "invalid video width");
    int height = luaL_checkinteger(L, 3);
    if(height <= 0) luaL_error(L, "invalid video height");
    Video *v = (Video*)lua_newuserdata(L, sizeof(Video));
    new (v) Video(); // placement new
    luaL_getmetatable(L, metatable_name);
    lua_setmetatable(L, -2);
    v->url = std::string(url);
    v->width = (unsigned int)width;
    v->height = (unsigned int)height;
    v->texture.id = rlLoadTexture(NULL, width, height, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
    v->texture.width= width;
    v->texture.height = height;
    v->texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    v->texture.mipmaps = 1;
    v->recording.is_recording = false;
    v->recording.key_frame_received = false;
    v->recording.format_ctx = nullptr;
    return 1;
}

static void thread_func(Video *v) {
    while(v->is_running) {
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

int lua_video_start(lua_State *L) {
    Video *v = checkvideo(L);
    if(v->is_running) return 0;
//FFmpeg stuff
    AVDictionary *options = nullptr;
    if(av_dict_set(&options, "flags", "nobuffer", 0) < 0) luaL_error(L, "failed to set FFmpeg options");
    if(av_dict_set(&options, "flags", "low_delay", 0) < 0) luaL_error(L, "failed to set FFmpeg options");
    if(av_dict_set(&options, "max_delay", "0", 0) < 0) luaL_error(L, "failed to set FFmpeg options");
    v->format_ctx = avformat_alloc_context();
    int rc = avformat_open_input(&v->format_ctx, v->url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if(rc < 0) LUA_AV_ERROR(rc);

    TraceLog(LOG_INFO, "CODEC: format %s", v->format_ctx->iformat->long_name);

    rc = avformat_find_stream_info(v->format_ctx, nullptr);
    if(rc < 0) LUA_AV_ERROR(rc);

    AVCodecParameters *video_params = nullptr;
    for(unsigned int i = 0; i < v->format_ctx->nb_streams; i++) {
        AVStream *tmp_stream = v->format_ctx->streams[i];
        AVCodecParameters *tmp_params = tmp_stream->codecpar;
        if(tmp_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            v->video_stream = tmp_stream;
            video_params = tmp_params;
            TraceLog(LOG_INFO, "CODEC: Resolution: %d x %d, type: %d", video_params->width, video_params->height, video_params->codec_id);
        }
    }
    if(!v->video_stream) luaL_error(L, "failed to find video stream");

    // DEbug
    TraceLog(LOG_INFO, "CODEC: Resolution: %d x %d, type: %d", video_params->width, video_params->height, video_params->codec_id);

    const AVCodec *video_codec = avcodec_find_decoder(video_params->codec_id);
    if(!video_codec) luaL_error(L, "failed to find video codec");

    TraceLog(LOG_INFO, "CODEC: %s ID %d, Bit rate %ld", video_codec->name, video_codec->id, video_params->bit_rate);
    TraceLog(LOG_INFO, "FPS: %d/%d, TBR: %d/%d, TimeBase: %d/%d", v->video_stream->avg_frame_rate.num,
             v->video_stream->avg_frame_rate.den, v->video_stream->r_frame_rate.num,
             v->video_stream->r_frame_rate.den, v->video_stream->time_base.num, v->video_stream->time_base.den);

    v->video_codec_ctx = avcodec_alloc_context3(video_codec);
    if(!v->video_codec_ctx) luaL_error(L, "failed to allocate video codec context");

    rc = avcodec_parameters_to_context(v->video_codec_ctx, video_params);
    if(rc < 0) LUA_AV_ERROR(rc);

    // Debug
    TraceLog(LOG_INFO, "Codec context: width = %d, height = %d", v->video_codec_ctx->width, v->video_codec_ctx->height);

    rc = avcodec_open2(v->video_codec_ctx, video_codec, NULL);
    if(rc < 0) luaL_error(L, "failed to open codec");

    v->frame = av_frame_alloc();
    if(!v->frame) luaL_error(L, "failed to allocate frame");

    v->packet = av_packet_alloc();
    if(!v->packet) luaL_error(L, "failed to allocate packet");

    TraceLog(LOG_INFO, "Codec context: width = %d, height = %d", v->video_codec_ctx->width, v->video_codec_ctx->height);

    v->sws_ctx = sws_getContext(v->video_codec_ctx->width, v->video_codec_ctx->height, v->video_codec_ctx->pix_fmt,
                             v->width, v->height, AV_PIX_FMT_RGB24, SWS_FAST_BILINEAR, 0, 0, 0);
    if(!v->sws_ctx) luaL_error(L, "failed to get sws context");

    v->rgb_frame = av_frame_alloc();
    if(!v->rgb_frame) luaL_error(L, "failed to allocate RGB frame");

    v->rgb_frame->format = AV_PIX_FMT_RGB24;
    v->rgb_frame->width = v->width;
    v->rgb_frame->height = v->height;

    rc = av_frame_get_buffer(v->rgb_frame, 0);
    if(rc < 0) LUA_AV_ERROR(rc);

    v->is_running = true;
    v->thread = new std::thread(thread_func, v);
    return 0;
}

static void stop_recording(Video *v);

int lua_video_stop(lua_State *L) {
    Video *v = checkvideo(L);
    if(v->recording.is_recording)
        stop_recording(v);
    video_stop(v);
    return 0;
}

int lua_video_rotate(lua_State *L) {
    Video *v = checkvideo(L);
    luaL_checktype(L, 2, LUA_TBOOLEAN);
    v->rotate = lua_toboolean(L, 2);
    return 0;
}

int lua_video_display(lua_State *L) {
    Video *v = checkvideo(L);
    if(v->is_running) {
        v->rgb_frame_mutex.lock();
        UpdateTexture(v->texture, v->rgb_frame->data[0]);
        v->rgb_frame_mutex.unlock();
    }
    ImVec2 uv0, uv1;
    if(v->rotate) {
        uv0 = ImVec2(1.0f, 1.0f);
        uv1 = ImVec2(0.0f, 0.0f);
    } else {
        uv0 = ImVec2(0.0f, 0.0f);
        uv1 = ImVec2(1.0f, 1.0f);
    }
    ImGui::Image(ImTextureID(v->texture.id), ImVec2(v->texture.width, v->texture.height), uv0, uv1);
    return 0;
}

static int lua_capture_image(lua_State *L) {
    Video *v = checkvideo(L);
    const char *file_name = luaL_checkstring(L, 2);
    if(v->is_running) {
        v->rgb_frame_mutex.lock();
        Image img = LoadImageFromTexture(v->texture);
        ExportImage(img, file_name);
        v->rgb_frame_mutex.unlock();
    }
    return 0;
}

static int lua_start_recording(lua_State *L) {
    const char *err = nullptr;
    const char *file_name = nullptr;
    int rc = 0;
    Video *v = checkvideo(L);
    if(!v->is_running) {
        err = "video stream is not started";
        goto cleanup0;
    }
    if(v->recording.is_recording) {
        err = "already recording video";
        goto cleanup0;
    }
    file_name = luaL_checkstring(L, 2);
    rc = avformat_alloc_output_context2(&v->recording.format_ctx, NULL, "mp4", file_name);
    if(rc < 0) {
        err =  "failed to start recording";
        goto cleanup0;
    }
    v->recording.video_stream = avformat_new_stream(v->recording.format_ctx, NULL);
    if(v->recording.video_stream == NULL) {
        err = "failed to create output stream";
        goto cleanup1;
    }
    rc = avcodec_parameters_copy(v->recording.video_stream->codecpar, v->video_stream->codecpar);
    if(rc < 0) {
        err = "failed to copy codec parameters";
        goto cleanup1;
    }
    v->recording.video_stream->time_base = v->video_stream->time_base;
    rc = avio_open(&v->recording.format_ctx->pb, file_name, AVIO_FLAG_WRITE);
    if(rc < 0) {
        err = "failed to open video file";
        goto cleanup1;
    }
    rc = avformat_write_header(v->recording.format_ctx, NULL);
    if(rc < 0) {
        err = "failed to write header";
        goto cleanup2;
    }
    v->recording.is_recording = true;
    return 0;

cleanup2:
    avio_closep(&v->recording.format_ctx->pb);
cleanup1:
    avformat_free_context(v->recording.format_ctx);
cleanup0:
    v->recording.format_ctx = nullptr;
    v->recording.video_stream = nullptr;
    luaL_error(L, err);
    return 0;
}

static void stop_recording(Video *v) {
    av_write_trailer(v->recording.format_ctx);
    avio_closep(&v->recording.format_ctx->pb);
    avformat_free_context(v->recording.format_ctx);
    v->recording.format_ctx = nullptr;
    v->recording.video_stream = nullptr;
    v->recording.is_recording = false;
    v->recording.key_frame_received = false;
}

static int lua_stop_recording(lua_State *L) {
    Video *v = checkvideo(L);
    if(!v->recording.is_recording)
        luaL_error(L, "video recording already stopped");
    stop_recording(v);
    return 0;
}

static int lua_is_recording(lua_State *L) {
    Video *v = checkvideo(L);
    lua_pushboolean(L, v->recording.is_recording);
    return 1;
}

static int lua_video_to_string(lua_State *L) {
    Video *v = checkvideo(L);
    lua_pushfstring(L, "video: \"%s\"", v->url.c_str());
    return 1;
}

static int lua_video_gc(lua_State *L) {
    Video *v = checkvideo(L);
    if(v->recording.is_recording)
        stop_recording(v);
    video_stop(v);
    rlUnloadTexture(v->texture.id);
    v->~Video();
    return 0;
}

static const struct luaL_Reg video_lib[] = {
    {"new", lua_video_new},
    {nullptr, nullptr},
};

static const struct luaL_Reg video_lib_m[] = {
    {"start", lua_video_start},
    {"stop", lua_video_stop},
    {"rotate", lua_video_rotate},
    {"display", lua_video_display},
    {"capture_image", lua_capture_image},
    {"start_recording", lua_start_recording},
    {"stop_recording", lua_stop_recording},
    {"is_recording", lua_is_recording},
    {"__tostring", lua_video_to_string},
    {"__gc", lua_video_gc},
    {nullptr, nullptr},
};

int lua_open_video(lua_State *L) {
    luaL_newmetatable(L, metatable_name);
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_setfuncs(L, video_lib_m, 0);
    lua_pop(L, 2);
    luaL_newlib(L, video_lib);
    return 1;
}