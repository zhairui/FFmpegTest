#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"zr",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"zr",FORMAT,##__VA_ARGS__);

//封装格式
#include "include/libavformat/avformat.h"
//解码
#include "include/libavcodec/avcodec.h"
//缩放
#include "include/libswscale/swscale.h"
#include "include/libyuv/libyuv.h"

//nb_streams,视频文件中存在，音频流，视频流，字幕
#define MAX_STREAM 2

struct Player{
    //封装格式上下文
    AVFormatContext *input_format_ctx;
    //音频视频流索引位置
    int video_stream_index;
    int audio_stream_index;
    //解码上下文数组
    AVCodecContext *input_codec_ctx[MAX_STREAM];

    //解码线程ID
    pthread_t decode_threads[MAX_STREAM];
    ANativeWindow* nativeWindow;
};

//初始化封装格式上下文，获取音频视频流的索引位置
void init_input_format_ctx(struct Player *player, const char* input_cstr){
    //注册组件
    av_register_all();
    //封装格式上下文
    AVFormatContext *format_ctx=avformat_alloc_context();

    //打开输入视频文件
    if(avformat_open_input(&format_ctx,input_cstr,NULL,NULL)!=0){
        LOGE("%s","打开输入视频文件失败");
        return;
    }
    //获取视频信息
    if(avformat_find_stream_info(format_ctx,NULL)<0){
        LOGE("%s","获取视频信息失败");
        return;
    }
    //视频解码，需要找到视频对应的AVStream所在format_ctx_streams的索引位置
    //获取音频和视频流的索引位置
    int i;
    for (i= 0; i <format_ctx->nb_streams ;i ++) {
        if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            player->video_stream_index=i;
        }else if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            player->audio_stream_index=i;
        }
    }
    player->input_format_ctx=format_ctx;
}

//初始化解码器上下文
void init_codec_context(struct Player *player, int stream_idx){
    //获取解码器
    AVFormatContext *format_ctx=player->input_format_ctx;
    AVCodecContext *codec_ctx=format_ctx->streams[stream_idx]->codec;
    AVCodec *codec=avcodec_find_decoder(codec_ctx->codec_id);
    if(codec==NULL){
        LOGE("%s","无法解码");
        return;
    }
    //打开解码器
    if(avcodec_open2(codec_ctx,codec,NULL)<0){
        LOGE("%s","解码器无法打开");
        return;
    }
    player->input_codec_ctx[stream_idx]=codec_ctx;

}
//解码视频
void decode_video(struct Player *player,AVPacket *packet){
    //像素数据（解码数据）
    AVFrame *yuv_frame=av_frame_alloc();
    AVFrame *rgb_frame=av_frame_alloc();
    //绘制时缓冲区
    ANativeWindow_Buffer outBuffer;
    AVCodecContext *codec_ctx=player->input_codec_ctx[player->video_stream_index];
    int got_frame;
    //解码AVPacket->AVFrame
    avcodec_decode_video2(codec_ctx,yuv_frame,&got_frame,packet);
    //非0，正在解码
    LOGI("gotframe %d",got_frame);
    if(got_frame){
        //设置缓冲区的属性（宽、高、像素格式）
        ANativeWindow_setBuffersGeometry(player->nativeWindow,codec_ctx->width,codec_ctx->height,WINDOW_FORMAT_RGBA_8888);
        ANativeWindow_lock(player->nativeWindow,&outBuffer,NULL);

        //设置rgb_frame的属性
        //rgb_frame缓冲区与outBuffer.bits是同一块内存
        avpicture_fill((AVPicture *)rgb_frame,outBuffer.bits,AV_PIX_FMT_RGBA,codec_ctx->width,codec_ctx->height);

        //YUV->RGBA_8888
        I420ToARGB(yuv_frame->data[0],yuv_frame->linesize[0],
            yuv_frame->data[2],yuv_frame->linesize[2],
            yuv_frame->data[1],yuv_frame->linesize[1],
            rgb_frame->data[0],rgb_frame->linesize[0],
            codec_ctx->width,codec_ctx->height);

        //unlock
        ANativeWindow_unlockAndPost(player->nativeWindow);

        usleep(1000 *16);
    }

    av_frame_free(&yuv_frame);
    av_frame_free(&rgb_frame);
}
//解码子线程函数
void *decode_data(void* arg){
    struct Player *player=(struct Player*) arg;
    AVFormatContext *format_ctx=player->input_format_ctx;
    //编码数据
    AVPacket *packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    //一帧一帧读取压缩的视频数据AVPacket
    int video_frame_count=0;
    while(av_read_frame(format_ctx,packet)>=0){
        if(packet->stream_index ==player->video_stream_index){
            decode_video(player,packet);
            LOGI("video_frame_count:%d",video_frame_count++);
        }
        av_free_packet(packet);
    }
}

void decode_vido_prepare(JNIEnv *env, struct Player *player ,jobject surface){
    player->nativeWindow=ANativeWindow_fromSurface(env,surface);
}
JNIEXPORT void JNICALL Java_com_ffmpegtest_VideoUtils_thread_1play
        (JNIEnv * env,jobject jobj,jstring input_jstr,jobject surface){

    const  char* input_cstr=(*env)->GetStringUTFChars(env,input_jstr,NULL);
    struct Player *player=(struct Player*)malloc(sizeof(struct Player));
    //初始化封装格式上下文
    init_input_format_ctx(player,input_cstr);
    int video_stream_index=player->video_stream_index;
    int audio_stream_index=player->audio_stream_index;

    //获取音视频解码器,并打开
    init_codec_context(player,video_stream_index);
    init_codec_context(player,audio_stream_index);

    decode_vido_prepare(env,player,surface);

    //创建子线程编程
    pthread_create(&(player->decode_threads[video_stream_index]),NULL,decode_data,(void*)player);

}
