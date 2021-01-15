#pragma once

#include "AV4L2Buffer.h"
#include <functional>
#include <libv4l2.h>

#include <pthread.h>

class AV4L2ElementPlane
{
public:
    AV4L2ElementPlane(int fd, uint32_t type, uint32_t pixelFormat);
    ~AV4L2ElementPlane();

    void setPlaneFormat(int width, int height);
    void setupPlane();
    void deinitPlane();
    void setStreamStatus(bool status);

    AV4L2Buffer* getFreeBuffer();

    typedef std::function<void (AV4L2Buffer*)> dqThreadCallback;
    void setDQThreadCallback(dqThreadCallback callback);
    void startDQThread();
    int waitForDQThread(uint32_t max_wait_ms);
    static void* dqThread(void *plane);

    int qBuffer(uint32_t index);
    void qAllBuffers();

    void setEOSFlag(AV4L2Buffer* buffer);

private:
    void reqbufs(uint32_t count);
    void queryBuffer(uint32_t i);
    void exportBuffer(uint32_t i);

    int dqBuffer(AV4L2Buffer **buffer, uint32_t retries);    
 
public:
    struct v4l2_format mFormat;

private:
    int mCount;
    int mFreeCount;
    uint32_t mType;
    uint32_t mPixelFormat;
    uint32_t mNumPlanes;
    int mFD;

    uint32_t mSrcWidth;
    uint32_t mSrcHeight;

    uint32_t mMemType;

    AV4L2Buffer **mBuffers;
    AV4L2Buffer *mTempBuffer;

    bool mStreamOn;

    pthread_mutex_t plane_lock;
    pthread_cond_t plane_cond;

    dqThreadCallback mCallback;
    bool mDQThreadRunning;
    bool mStopDQThread;
    pthread_t mDQThread;
};