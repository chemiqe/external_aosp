#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <linux/videodev2.h>
#include <sstream>
#include <random>
#ifdef __ANDROID__
#include <unistd.h> // getopt
#else
#include <argp.h>
#endif

struct arguments {
    const char* device;
    bool enable_sleep;
    int no_of_frames;
};

#ifdef __ANDROID__
static int parse_arguments(int argc, char **argv, struct arguments& arguments) {
    int opt;
    while ((opt = getopt(argc, argv, "d:sn:")) != -1) {
        switch (opt) {
        case 'd':
            arguments.device = optarg;
            break;
        case 's':
            arguments.enable_sleep = true;
            break;
        case 'n':
            arguments.no_of_frames = std::atoi(optarg);
            if (arguments.no_of_frames <= 0) {
                std::cerr << "Invalid number of frames: " << optarg << std::endl;
                return 1;
            }
            break;
        default:
            std::cerr << "Usage: " << argv[0] << " -d device -s enable random sleeps -n number of the frames" << std::endl;
            return 1;
        }
    }

    return 0;
}
#else
const char *argp_program_version = "test_camera 1.0";
const char *argp_program_bug_address = "<lukasz.zawada@demware.com>";

static char doc[] = "test_camera -- Test camera on Linux using V4L2";
static char args_doc[] = "";

// Options
static struct argp_option options[] = {
    {"device",  'd', "PATH", 0, "Path to the device /dev/video0 by defautl" },
    {"enable-sleep", 's', 0, 0, "Enable random sleeps before each ioctl" },
    {"no_of_frames",  'n', "NUMBER", 0, "Number of the frames to dump" },
    { 0 }
};
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = (struct arguments*) state->input;

    switch (key) {
        case 'd': arguments->device  = arg; break;
        case 's': arguments->enable_sleep = true; break;
        case 'n': arguments->no_of_frames = std::atoi(arg); 
                  if (arguments->no_of_frames <= 0) {
                      std::cerr << "Invalid number of frames: " << arg << std::endl;
                      argp_usage(state);
                  }
                  break;
        case ARGP_KEY_END: break;
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

static int parse_arguments(int argc, char **argv, struct arguments& arguments) {
    return argp_parse(&argp, argc, argv, 0, 0, &arguments);
}

#endif

struct Buffer {
    void* start;
    size_t length;
};


std::string fourccToString(uint32_t fmt) {
    char fourcc[5] = {
        static_cast<char>(fmt & 0xFF),
        static_cast<char>((fmt >> 8) & 0xFF),
        static_cast<char>((fmt >> 16) & 0xFF),
        static_cast<char>((fmt >> 24) & 0xFF),
        '\0'
    };

    std::stringstream ss;
    ss << fourcc;
    return ss.str();
}

std::random_device *rd;
std::mt19937 *gen;
std::uniform_int_distribution<> *dist;
void init_random() {
    rd = new std::random_device();
    gen = new std::mt19937((*rd)());
    dist = new std::uniform_int_distribution<>(10000, 1000000);
}

int get_random(struct arguments& arguments) {
    int ret=0;
    if (arguments.enable_sleep && dist != nullptr)
        ret = (*dist)(*gen);
    return ret;
}

int main(int argc, char* argv[]) {
    // default arguments
    struct arguments arguments;
    arguments.device = "/dev/video0";
    arguments.enable_sleep = false;
    arguments.no_of_frames = 1;

    init_random();
    parse_arguments(argc, argv, arguments);

    std::ofstream ofs("frame.raw", std::ios::binary);

    int fd = open(arguments.device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // 1. Set format
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1280;
    fmt.fmt.pix.height = 720;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    std::cout << "Config: width: " << fmt.fmt.pix.width << " height: " << fmt.fmt.pix.height << " device: " << arguments.device << std::endl;
    std::cout << "Pixelformat: " << fourccToString(fmt.fmt.pix.pixelformat) << std::endl;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT");
        close(fd);
        return 1;
    }

    // 2. Request buffers
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    usleep(get_random(arguments));
    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        perror("VIDIOC_REQBUFS");
        close(fd);
        return 1;
    }

    Buffer* buffers = new Buffer[req.count];

    // 3. Mmap buffers
    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        usleep(get_random(arguments));
        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF");
            close(fd);
            return 1;
        }

        buffers[i].length = buf.length;
        buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].start == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return 1;
        }
    }

    // 4. Queue buffers
    for (size_t i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        usleep(get_random(arguments));
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF");
            close(fd);
            return 1;
        }
    }

    // 5. Start stream
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    usleep(get_random(arguments));
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON");
        close(fd);
        return 1;
    }
    std::cout << "Streaming started on " << arguments.device << std::endl;


    int i=0;
    while (i < arguments.no_of_frames) {
        ++i;
        // 6. Wait for frame (poll with timeout)
        // struct pollfd pfd;
        // pfd.fd = fd;
        // pfd.events = POLLIN;
        // int pret = poll(&pfd, 1, 1000); // 1 sek timeout
        // if (pret <= 0) {
        //     if (pret == 0) {
        //         std::cerr << "Timeout waiting for frame" << std::endl;
        //     } else {
        //         perror("poll");
        //     }
        //     close(fd);
        //     return 1;
        // }

        // 7. Dequeue buffer
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        usleep(get_random(arguments));
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            perror("VIDIOC_DQBUF");
            close(fd);
            return 1;
        }

        std::cout << "Captured frame: " << buf.bytesused << " bytes" << std::endl;

        // 8. Write to file
        ofs.write(reinterpret_cast<char*>(buffers[buf.index].start), buf.bytesused);
        std::cout << "Frame saved to frame.raw" << std::endl;

        // 9. Return buffer to the queue
        usleep(get_random(arguments));
        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            perror("VIDIOC_QBUF (return)");
        }
    }

    // 10. Stream off
    usleep(get_random(arguments));
    if (ioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
        perror("VIDIOC_STREAMOFF");
    }

    // 11. Clean up
    for (size_t i = 0; i < req.count; ++i) {
        munmap(buffers[i].start, buffers[i].length);
    }
    ofs.close();
    delete[] buffers;
    close(fd);

    return 0;
}
