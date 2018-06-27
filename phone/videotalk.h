#ifndef VIDEOTALK_H_
#define VIDEOTALK_H_

#define IMG_WIDTH 160
#define IMG_HEIGHT 60

typedef struct video_args_ {
    int videoout1fd;
    int videoout2fd;
    int sockfd;
} video_args;

typedef struct pixelval_ {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} pixelval;

typedef pixelval image_t[IMG_HEIGHT][IMG_WIDTH];

void *videotalk(void *arg);

#endif /* VIDEOTALK_H_ */
