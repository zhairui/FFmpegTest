#include "videoutils.h"

#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "include/libyuv/libyuv.h"
//编码
#include "include/libavcodec/avcodec.h"
//封装格式处理
#include "include/libavformat/avformat.h"
//像素处理
#include "include/libswscale/swscale.h"
#include "include/libavutil/avutil.h"
#include "include/libavutil/frame.h"

#define LOGI(FORMAT,...) __android_log_print(ANDROID_LOG_INFO,"zr",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT,...) __android_log_print(ANDROID_LOG_ERROR,"zr",FORMAT,##__VA_ARGS__);


JNIEXPORT void JNICALL Java_com_ffmpegtest_VideoUtils_decode
  (JNIEnv * env, jclass jcls, jstring input_jstr, jstring output_jstr){
	//需要转码的视频文件(输入的视频文件)
	const char* input_cstr = (*env)->GetStringUTFChars(env,input_jstr,NULL);
	const char* output_cstr = (*env)->GetStringUTFChars(env,output_jstr,NULL);

	//1.注册所有组件
	av_register_all();

	//封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
	AVFormatContext *pFormatCtx = avformat_alloc_context();

	//2.打开输入视频文件
	if (avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL) != 0)
	{
		LOGE("%s","无法打开输入视频文件");
		return;
	}

	//3.获取视频文件信息
	if (avformat_find_stream_info(pFormatCtx,NULL) < 0)
	{
		LOGE("%s","无法获取视频文件信息");
		return;
	}

	//获取视频流的索引位置
	//遍历所有类型的流（音频流、视频流、字幕流），找到视频流
	int v_stream_idx = -1;
	int i = 0;
	//number of streams
	for (; i < pFormatCtx->nb_streams; i++)
	{
		//流的类型(根据流类型判断，是否是视频流)
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			v_stream_idx = i;
			break;
		}
	}

	if (v_stream_idx == -1)
	{
		LOGE("%s","找不到视频流\n");
		return;
	}

	//只有知道视频的编码方式，才能够根据编码方式去找到解码器
	//获取视频流中的编解码上下文
	AVCodecContext *pCodecCtx = pFormatCtx->streams[v_stream_idx]->codec;
	//4.根据编解码上下文中的编码id查找对应的解码
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	//（迅雷看看，找不到解码器，临时下载一个解码器）
	if (pCodec == NULL)
	{
		LOGE("%s","找不到解码器\n");
		return;
	}

	//5.打开解码器
	if (avcodec_open2(pCodecCtx,pCodec,NULL)<0)
	{
		LOGE("%s","解码器无法打开\n");
		return;
	}

	//输出视频信息
	LOGI("视频的文件格式：%s",pFormatCtx->iformat->name);
	LOGI("视频时长：%d", (pFormatCtx->duration)/1000000);
	LOGI("视频的宽高：%d,%d",pCodecCtx->width,pCodecCtx->height);
	LOGI("解码器的名称：%s",pCodec->name);

	//准备读取
	//AVPacket用于存储一帧一帧的压缩数据（H264）
	//缓冲区，开辟空间
	AVPacket *packet = (AVPacket*)av_malloc(sizeof(AVPacket));

	//AVFrame用于存储解码后的像素数据(YUV)
	//内存分配
	AVFrame *pFrame = av_frame_alloc();
	//YUV420
	AVFrame *pFrameYUV = av_frame_alloc();
	//只有指定了AVFrame的像素格式、画面大小才能真正分配内存
	//缓冲区分配内存
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));
	//初始化缓冲区
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

	//用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
	struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,pCodecCtx->height,pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);


	int got_picture, ret;

	FILE *fp_yuv = fopen(output_cstr, "wb+");

	int frame_count = 0;

	//6.一帧一帧的读取压缩数据
	while (av_read_frame(pFormatCtx, packet) >= 0)
	{
		//只要视频压缩数据（根据流的索引位置判断）
		if (packet->stream_index == v_stream_idx)
		{
			//7.解码一帧视频压缩数据，得到视频像素数据
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0)
			{
				LOGE("%s","解码错误");
				return;
			}

			//为0说明解码完成，非0正在解码
			if (got_picture)
			{
				//AVFrame转为像素格式YUV420，宽高
				//2 6输入、输出数据
				//3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
				//4 输入数据第一列要转码的位置 从0开始
				//5 输入画面的高度
				sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
					pFrameYUV->data, pFrameYUV->linesize);

				//输出到YUV文件
				//AVFrame像素帧写入文件
				//data解码后的图像像素数据（音频采样数据）
				//Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
				//U V 个数是Y的1/4
				int y_size = pCodecCtx->width * pCodecCtx->height;
				fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);
				fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);
				fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);

				frame_count++;
				LOGI("解码第%d帧",frame_count);
			}
		}

		//释放资源
		av_free_packet(packet);
	}

	fclose(fp_yuv);

	(*env)->ReleaseStringUTFChars(env,input_jstr,input_cstr);
	(*env)->ReleaseStringUTFChars(env,output_jstr,output_cstr);

	av_frame_free(&pFrame);

	avcodec_close(pCodecCtx);

	avformat_free_context(pFormatCtx);
}


JNIEXPORT void JNICALL Java_com_ffmpegtest_VideoUtils_render
		(JNIEnv *env, jobject jobj, jstring input_jstr, jobject surface){
	const char* input_cstr=(*env)->GetStringUTFChars(env,input_jstr,NULL);
	//1.注册组件
	av_register_all();

	//封装格式上下文
	AVFormatContext *pFormatCtx=avformat_alloc_context();

	//2.打开输入视频文件
	if(avformat_open_input(&pFormatCtx,input_cstr,NULL,NULL)!=0){
		LOGE("%s","打开输入视频文件失败");
		return;
	}
	//3.获取视频信息
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		LOGE("%s","获取视频信息失败");
		return;
	}

	//视频解码，需要找到视频对应的AVStream所在pFormatCtx->streams的索引位置
	int video_stream_idx=-1;

	for (int i = 0; i < pFormatCtx->nb_streams; ++i) {

		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			video_stream_idx=i;
			break;
		}
	}

	//4.获取视频解码器
	AVCodecContext *pCodeCtx=pFormatCtx->streams[video_stream_idx]->codec;
	AVCodec *pCodec=avcodec_find_decoder(pCodeCtx->codec_id);
	if(pCodec==NULL){
		LOGE("%s","无法解码");
		return;
	}
	//5.打开解码器
	if(avcodec_open2(pCodeCtx,pCodec,NULL)<0){
		LOGE("%s","解码器无法打开");
		return;
	}
	//编码数据
	AVPacket *packet=(AVPacket *) av_malloc(sizeof(AVPacket));

	//像素数据(解码数据)
	AVFrame *yu_frame=av_frame_alloc();
	AVFrame *rgb_frame=av_frame_alloc();

	//native绘制
	//窗体
	ANativeWindow * nativeWindow=ANativeWindow_fromSurface(env,surface);
	//绘制时的缓冲区
	ANativeWindow_Buffer outBuffer;

	int len,got_frame,framecount=0;
	//6.一阵一阵读取压缩的视频数据AVPacket
	while (av_read_frame(pFormatCtx,packet)>=0){
		//解码AVPacket->AVFrame
		len=avcodec_decode_video2(pCodeCtx,yu_frame,&got_frame,packet);

		//非0,正在解码
		if(got_frame){
			LOGI("解码%d帧",framecount++);

			//设置缓冲区的属性(宽、高、像素格式)
			ANativeWindow_setBuffersGeometry(nativeWindow,pCodeCtx->width,pCodeCtx->height,WINDOW_FORMAT_RGBA_8888);
			ANativeWindow_lock(nativeWindow,&outBuffer,NULL);

			//设置rgb_frame的属性（像素格式，宽高）和缓冲区
			//rgb_frame缓冲区与outBuffer.bits是同一块内存
			avpicture_fill((AVPicture *)rgb_frame,outBuffer.bits,PIX_FMT_RGBA,pCodeCtx->width,pCodeCtx->height);

			//YUV->RGBA_8888
			I420ToARGB(yu_frame->data[0],yu_frame->linesize[0],
					yu_frame->data[2],yu_frame->linesize[2],
					yu_frame->data[1],yu_frame->linesize[1],
			rgb_frame->data[0],rgb_frame->linesize[0],
			pCodeCtx->width,pCodeCtx->height);

			//unlock
			ANativeWindow_unlockAndPost(nativeWindow);

			usleep(1000*16);

		}
		av_free_packet(packet);
	}
	ANativeWindow_release(nativeWindow);
	av_frame_free(&yu_frame);
	avcodec_close(pCodeCtx);
	avformat_free_context(pFormatCtx);

	(*env)->ReleaseStringUTFChars(env,input_jstr,input_cstr);
}
