#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <raylib.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class Video {
public:
    Video(std::string url, uint16_t width, uint16_t height);
    ~Video();
    void start();
    void stop();
    bool is_playing();
    void rotation(bool active);
    void display();
    bool capture_image(const char *file_name);
    void start_recording(const char *file_name);
    void stop_recording();
    bool is_recording();
private:
    std::string url;
    uint16_t width, height;
    Texture2D texture;
    bool rotate = false;
    AVFormatContext *format_ctx = nullptr;
    AVStream *video_stream = nullptr;
    AVCodecContext *video_codec_ctx = nullptr;
    AVFrame *frame = nullptr, *rgb_frame = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *sws_ctx = nullptr;
    std::atomic_bool _is_playing;
    std::mutex rgb_frame_mutex;
    std::thread *thread;
    struct Recording {
        std::atomic_bool is_recording, key_frame_received;
        AVFormatContext *format_ctx;
        AVStream *video_stream;
    } recording;
    friend void thread_func(Video *v);
};