#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"zr",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"zr",FORMAT,##__VA_ARGS__);
//封装格式
#include "include/libavformat/avformat.h"
//解码
#include "include/libavcodec/avcodec.h"
//缩放
#include "include/libswscale/swscale.h"
//重采样
#include "include/libswresample/swresample.h"

#define MAX_STREAM 2
#define MAX_AUDIO_FRAME_SIZE 48000 *4

struct Player{
    //封装格式上下文
    AVFormatContext *input_format_ctx;
    //音频视频索引位置
    int video_stream_index;
    int audio_stream_index;
    //解码上下文数组
    AVCodecContext *input_codec_ctx[MAX_STREAM];

    JavaVM  *javaVM;
    //解码线程ID
    pthread_t decode_threads[MAX_STREAM];

    SwrContext *swr_ctx;
    //输入的采样格式
    enum AVSampleFormat in_sample_fmt;
    //输出采样格式16bit PCM
    enum AVSampleFormat out_sample_fmt;
    //输入采样率
    int in_sample_rate;
    //输出采样率
    int out_sample_rate;
    //输出的声道个数
    int out_channel_nb;

    jobject audio_track;
    jmethodID audio_track_write_mid;
};

//初始化封装格式上下文,获取音频视频的索引位置
void init_input_format_ctx2(struct Player *player, const char* input_cstr){
    //注册组件
    av_register_all();
    //获取封装格式上下文
    AVFormatContext *format_ctx=avformat_alloc_context();

    //打开输入文件流
    if(avformat_open_input(&format_ctx,input_cstr,NULL,NULL)!=0){
        LOGE("%s","打开文件失败");
        return;
    }
    //获取流信息
    if(avformat_find_stream_info(format_ctx,NULL)<0){
        LOGE("%s","获取音频流信息失败");
        return;
    }

    //获取音频流的索引位置
    int i;
    for (i = 0; i < format_ctx->nb_streams; ++i) {
        if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            player->audio_stream_index=i;
        }else if(format_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
            player->video_stream_index=i;
        }
    }
    player->input_format_ctx=format_ctx;
}

//初始化解码器上下文
void init_codec_context2(struct Player *player,int stream_index){
    //获取解码器
    AVFormatContext *format_ctx=player->input_format_ctx;
    //解码器上下文
    AVCodecContext *codec_ctx=format_ctx->streams[stream_index]->codec;
    //解码器
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
    player->input_codec_ctx[stream_index]=codec_ctx;
}

//音频解码准备
void decode_audio_pre(struct Player *player){
    AVCodecContext *codec_ctx=player->input_codec_ctx[player->audio_stream_index];

    //重采样设置参数
    //输入的采样格式
    enum AVSampleFormat in_sample_fmt=codec_ctx->sample_fmt;
    //输出采样格式16bit PCM
    enum AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    //输入采样率
    int in_sample_rate=codec_ctx->sample_rate;
    //输出采样率
    int out_sample_rate=in_sample_rate;

    //获取输入的声道布局
    //根据声道个数获取默认的声道布局（2个声道，默认立体声stereo)
    uint64_t in_ch_layout=codec_ctx->channel_layout;
    //输出的声道布局(立体声）
    uint64_t  out_ch_layout=AV_CH_LAYOUT_STEREO;

    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swr_ctx=swr_alloc();
    swr_alloc_set_opts(swr_ctx,
        out_ch_layout,out_sample_fmt,out_sample_rate,
        in_ch_layout,in_sample_fmt,in_sample_rate,0,NULL);
    swr_init(swr_ctx);

    //输出的声道个数
    int out_channel_nb=av_get_channel_layout_nb_channels(out_ch_layout);


    player->in_sample_rate=in_sample_rate;
    player->out_sample_fmt=out_sample_fmt;
    player->in_sample_fmt=in_sample_fmt;
    player->out_sample_rate=out_sample_rate;
    player->out_channel_nb=out_channel_nb;
    player->swr_ctx=swr_ctx;
}

void jni_audio_prepare(JNIEnv *env,jobject jthiz,struct Player *player){
    jclass player_class=(*env)->GetObjectClass(env,jthiz);

    //AudioTrack对象
    jmethodID  create_audio_track_mid=(*env)->GetMethodID(env,player_class,"createAudioTrack","(II)Landroid/media/AudioTrack;");
    jobject audio_track=(*env)->CallObjectMethod(env,jthiz,create_audio_track_mid,player->out_sample_rate,player->out_channel_nb);

    //调用AudioTrack.play方法
    jclass audio_track_class=(*env)->GetObjectClass(env,audio_track);
    jmethodID audio_track_play_mid=(*env)->GetMethodID(env,audio_track_class,"play","()V");
    (*env)->CallVoidMethod(env,audio_track,audio_track_play_mid);

    //AudioTrack.write
    jmethodID audio_track_write_mid=(*env)->GetMethodID(env,audio_track_class,"write","([BII)I");

    player->audio_track=(*env)->NewGlobalRef(env,audio_track);
    player->audio_track_write_mid=audio_track_write_mid;
}

void decode_audio(struct Player *player,AVPacket *packet){
    AVCodecContext *codec_ctx=player->input_codec_ctx[player->audio_stream_index];

    //解压缩数据
    AVFrame *frame=av_frame_alloc();
    int got_frame;
    avcodec_decode_audio4(codec_ctx,frame,&got_frame,packet);

    //16bit 44100 PCM数据（重采样缓冲区）
    uint8_t *out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE);
    //解码一帧成功
    if(got_frame>0){
        swr_convert(player->swr_ctx,&out_buffer,MAX_AUDIO_FRAME_SIZE,(const uint8_t **)frame->data,frame->nb_samples);
        //获取sample的size
        int out_buffer_size=av_samples_get_buffer_size(NULL,player->out_channel_nb,
            frame->nb_samples,player->out_sample_fmt,1);
        //关联当前线程的JNIEnv
        JavaVM *javaVM=player->javaVM;
        JNIEnv *env;
        (*javaVM)->AttachCurrentThread(javaVM,&env,NULL);

        //out_buffer缓冲区数据，转成byte数组
        jbyteArray audio_sample_array=(*env)->NewByteArray(env,out_buffer_size);
        jbyte * sample_bytep=(*env)->GetByteArrayElements(env,audio_sample_array,NULL);
        //out_buffer的数据复制到sample_bytep
        memcpy(sample_bytep,out_buffer,out_buffer_size);
        //同步
        (*env)->ReleaseByteArrayElements(env,audio_sample_array,sample_bytep,0);

        //AudioTrack.write PCM数据
        (*env)->CallIntMethod(env,player->audio_track,player->audio_track_write_mid,
            audio_sample_array,0,out_buffer_size);
        //释放局部引用
        (*env)->DeleteLocalRef(env,audio_sample_array);

        (*javaVM)->DetachCurrentThread(javaVM);
        usleep(1000*16);
    }
    av_frame_free(&frame);
}

void *decode_data2(void* arg){
    struct Player *player=(struct Player*)arg;
    int stream_index=player->audio_stream_index;
    AVFormatContext *format_ctx=player->input_format_ctx;
    AVPacket *packet=(AVPacket *)av_malloc(sizeof(AVPacket));

    int audio_frame_count=0;
    while(av_read_frame(format_ctx,packet)>=0){
        if(packet->stream_index==stream_index){
            decode_audio(player,packet);
            LOGI("audio:%d",audio_frame_count++);
        }
        av_free_packet(packet);
    }
    return NULL;
}

JNIEXPORT void JNICALL Java_com_ffmpegtest_VideoUtils_thread_1audio_1play
        (JNIEnv *env,jobject jobj,jstring input_str){
    const char* input_cstr=(*env)->GetStringUTFChars(env,input_str,NULL);
    struct Player *player=(struct Player*)malloc(sizeof(struct Player));
    (*env)->GetJavaVM(env,&(player->javaVM));
    //初始化封装格式上下文
    init_input_format_ctx2(player,input_cstr);
    int audio_stream_index=player->audio_stream_index;

    //获取音频解码器，并打开
    init_codec_context2(player,audio_stream_index);

    decode_audio_pre(player);

    jni_audio_prepare(env,jobj,player);
    //创建子线程编程
    pthread_create(&(player->decode_threads[audio_stream_index]),NULL,decode_data2,(void*)player);

}
