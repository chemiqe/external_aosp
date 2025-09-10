#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <vector>
#include <unistd.h>

#define LOG_TAG "EGLImageStressTest"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

int main() {
    // Init EGL
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        LOGE("No EGL display found");
        return -1;
    }
    if (!eglInitialize(display, nullptr, nullptr)) {
        LOGE("Failed to initialize EGL");
        return -1;
    }

    PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR_fn =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR_fn =
        (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC eglGetNativeClientBufferANDROID_fn =
        (PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC)eglGetProcAddress("eglGetNativeClientBufferANDROID");

    if (!eglCreateImageKHR_fn || !eglDestroyImageKHR_fn || !eglGetNativeClientBufferANDROID_fn) {
        LOGE("Required EGL extension functions not found");
        return -1;
    }

    // prepare buffer
    AHardwareBuffer_Desc desc = {};
    desc.width = 128;
    desc.height = 128;
    desc.layers = 1;
    desc.format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;

    std::vector<AHardwareBuffer*> buffers;
    std::vector<EGLImageKHR> images;

    int count = 0;
    bool fail_detected = false;

    while (!fail_detected) {
        AHardwareBuffer* hardwareBuffer = nullptr;
        if (AHardwareBuffer_allocate(&desc, &hardwareBuffer) != 0) {
            LOGE("Failed to allocate AHardwareBuffer at iteration %d", count);
            break;
        }
        buffers.push_back(hardwareBuffer);

        EGLClientBuffer clientBuffer = eglGetNativeClientBufferANDROID_fn(hardwareBuffer);
        EGLint attribs[] = {
            EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
            EGL_NONE
        };

        EGLImageKHR image = eglCreateImageKHR_fn(display, EGL_NO_CONTEXT,
                                                 EGL_NATIVE_BUFFER_ANDROID,
                                                 clientBuffer, attribs);
        if (image == EGL_NO_IMAGE_KHR) {
            EGLint err = eglGetError();
            LOGE("Could not create EGL image at iteration %d, err = 0x%x", count, err);
            fail_detected = true;
            break;
        }
        images.push_back(image);

        if (count % 100 == 0) {
            LOGI("Created %d images...", count);
        }
        count++;
    }

    LOGI("Cleaning up...");
    count = 0;

    for (auto img : images) {
        eglDestroyImageKHR_fn(display, img);

        if (count % 100 == 0) {
            LOGI("Destroy %d images...", count);
        }
        count++;
    }
    LOGI("Destroy %d images...", count);

    for (auto buf : buffers) {
        AHardwareBuffer_release(buf);
    }

    eglTerminate(display);
    return 0;
}
