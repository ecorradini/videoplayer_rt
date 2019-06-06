#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <SDL.h>
#include <SDL_thread.h>

#ifdef __MINGW32__
#undef main /* Prevents SDL from overriding main() */
#endif

#include <stdio.h>
#include <assert.h>

#include "libdl/dl_syscalls.h"
#include <sched.h>

// compatibility with newer API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55,28,1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif


typedef struct PacketQueue {
  AVPacketList *first_pkt, *last_pkt;
  int nb_packets;
  int size;
  SDL_mutex *mutex;
  SDL_cond *cond;
} PacketQueue;

PacketQueue audioq;

int quit = 0;

struct sched_attr backup;

void set_deadline_sched() {
	memset(&backup, 0, sizeof(struct sched_attr));
	sched_getattr(0, &backup, sizeof(struct sched_attr), NULL);
	struct sched_attr attr;
	memset(&attr, 0, sizeof(struct sched_attr));
	attr.size = sizeof(struct sched_attr);
	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime  =  4599220;
	attr.sched_period   = 16000000;
	attr.sched_deadline = 10000000;
	sched_setattr(0, &attr, 0);
}

void unset_deadline_sched() {
	sched_setattr(0, &backup, 0);
}

void print_scheduler() {
	if(sched_getscheduler(0)==SCHED_OTHER) {
		printf("Scheduler: SCHED_OTHER\n");
	}
	else if(sched_getscheduler(0)==SCHED_DEADLINE) {
		printf("Scheduler: SCHED_DEADLINE\n");
	}
	else {
		printf("Scheduler: NON CONOSCIUTO\n");
	}
}

void packet_queue_init(PacketQueue *q) {
  memset(q, 0, sizeof(PacketQueue));
  q->mutex = SDL_CreateMutex();
  q->cond = SDL_CreateCond();
}
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

  AVPacketList *pkt1;
  if(av_dup_packet(pkt) < 0) {
    return -1;
  }
  pkt1 = av_malloc(sizeof(AVPacketList));
  if (!pkt1)
    return -1;
  pkt1->pkt = *pkt;
  pkt1->next = NULL;
  
  
  SDL_LockMutex(q->mutex);
  
  if (!q->last_pkt)
    q->first_pkt = pkt1;
  else
    q->last_pkt->next = pkt1;
  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size;
  SDL_CondSignal(q->cond);
  
  SDL_UnlockMutex(q->mutex);
  return 0;
}
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
  AVPacketList *pkt1;
  int ret;
  
  SDL_LockMutex(q->mutex);
  
  for(;;) {
    
    if(quit) {
      ret = -1;
      break;
    }

    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt)
	q->last_pkt = NULL;
      q->nb_packets--;
      q->size -= pkt1->pkt.size;
      *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      SDL_CondWait(q->cond, q->mutex);
    }
  }
  SDL_UnlockMutex(q->mutex);
  return ret;
}
int main(int argc, char *argv[]) {
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream, audioStream;
  AVCodecContext  *pCodecCtxOrig = NULL;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL;
  AVPacket        packet;
  int             frameFinished;
  struct SwsContext *sws_ctx = NULL;

  SDL_Overlay     *bmp;
  SDL_Surface     *screen;
  SDL_Rect        rect;
  SDL_Event       event;
  SDL_AudioSpec   wanted_spec, spec;

  if(argc < 2) {
    fprintf(stderr, "Usage: test <file>\n");
    exit(1);
  }
	
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }
  
  print_scheduler();

  // Open video file
  if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0)
    return -1; // Couldn't open file
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    return -1; // Couldn't find stream information
  
  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);
    
  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++) {
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO &&
       videoStream < 0) {
      videoStream=i;
    }
  }
  if(videoStream==-1)
    return -1; // Didn't find a video stream
  

  // Get a pointer to the codec context for the video stream
  pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
  
  // Find the decoder for the video stream
  pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }

  // Copy context
  pCodecCtx = avcodec_alloc_context3(pCodec);
  if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
    fprintf(stderr, "Couldn't copy codec context");
    return -1; // Error copying codec context
  }

  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    return -1; // Could not open codec
  
  // Allocate video frame
  pFrame=av_frame_alloc();

  // Make a screen to put our video

#ifndef __DARWIN__
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 0, 0);
#else
        screen = SDL_SetVideoMode(pCodecCtx->width, pCodecCtx->height, 24, 0);
#endif
  if(!screen) {
    fprintf(stderr, "SDL: could not set video mode - exiting\n");
    exit(1);
  }
  
  // Allocate a place to put our YUV image on that screen
  bmp = SDL_CreateYUVOverlay(pCodecCtx->width,
				 pCodecCtx->height,
				 SDL_YV12_OVERLAY,
				 screen);

  // initialize SWS context for software scaling
  sws_ctx = sws_getContext(pCodecCtx->width,
			   pCodecCtx->height,
			   pCodecCtx->pix_fmt,
			   pCodecCtx->width,
			   pCodecCtx->height,
			   AV_PIX_FMT_YUV420P,
			   SWS_BILINEAR,
			   NULL,
			   NULL,
			   NULL
			   );


  // Read frames and save first five frames to disk
  set_deadline_sched();
  i=0;
  print_scheduler();
  double max_runtime = 0;
  struct timespec start, end;
  while(av_read_frame(pFormatCtx, &packet)>=0) {
    // Is this a packet from the video stream?
    if(packet.stream_index==videoStream) {
		clock_gettime(CLOCK_MONOTONIC, &start);
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      
      // Did we get a video frame?
      if(frameFinished) {
		SDL_LockYUVOverlay(bmp);

		AVPicture pict;
		pict.data[0] = bmp->pixels[0];
		pict.data[1] = bmp->pixels[2];
		pict.data[2] = bmp->pixels[1];

		pict.linesize[0] = bmp->pitches[0];
		pict.linesize[1] = bmp->pitches[2];
		pict.linesize[2] = bmp->pitches[1];

		// Convert the image into YUV format that SDL uses	
		sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
			  pFrame->linesize, 0, pCodecCtx->height,
			  pict.data, pict.linesize);
		
		SDL_UnlockYUVOverlay(bmp);
		
		rect.x = 0;
		rect.y = 0;
		rect.w = pCodecCtx->width;
		rect.h = pCodecCtx->height;
		SDL_DisplayYUVOverlay(bmp, &rect);
		av_free_packet(&packet);
      }
    } else {
      av_free_packet(&packet);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_time = (end.tv_nsec - start.tv_nsec); // / 1.0e6; //(end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    if(total_time>=max_runtime) max_runtime=total_time;
    // Free the packet that was allocated by av_read_frame
    SDL_PollEvent(&event);
    switch(event.type) {
    case SDL_QUIT:
      unset_deadline_sched();
      quit = 1;
      SDL_Quit();
      exit(0);
      break;
    default:
      break;
    }
	
  }
  
  unset_deadline_sched();
  print_scheduler();

  // Free the YUV frame
  av_frame_free(&pFrame);
  
  // Close the codecs
  avcodec_close(pCodecCtxOrig);
  avcodec_close(pCodecCtx);
  //avcodec_close(aCodecCtxOrig);
  //avcodec_close(aCodecCtx);
  
  // Close the video file
  avformat_close_input(&pFormatCtx);
  
  	printf("WORST CASE EXECUTION %d\n", max_runtime);
  
  return 0;
}


