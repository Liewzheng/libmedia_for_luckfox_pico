/**
 * @file media.c
 * @brief Universal Media Device Library Implementation
 * @version 1.0.0
 * @date 2025-07-01
 * 
 * Implementation of the universal media device interface for V4L2 video capture.
 * This library abstracts V4L2 operations and provides a simplified API for
 * camera and video device management.
 */

#define _GNU_SOURCE

#include "media.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <linux/videodev2.h>

// ============================================================================
// Internal Constants and Macros
// ============================================================================

#define MAX_DEVICES 16
#define DEVICE_NAME_SIZE 256
#define VERSION_STRING "1.0.0"

// Debug output macros
#define DEBUG_NONE 0
#define DEBUG_ERROR 1
#define DEBUG_WARNING 2
#define DEBUG_INFO 3
#define DEBUG_DEBUG 4

#define MEDIA_DEBUG(level, fmt, ...) \
    do { \
        if (g_debug_level >= level) { \
            fprintf(stderr, "[MEDIA %s] " fmt "\n", \
                level == DEBUG_ERROR ? "ERROR" : \
                level == DEBUG_WARNING ? "WARN" : \
                level == DEBUG_INFO ? "INFO" : "DEBUG", \
                ##__VA_ARGS__); \
        } \
    } while(0)

// ============================================================================
// Internal Data Structures
// ============================================================================

/**
 * @struct device_context
 * @brief Internal device context structure
 */
typedef struct {
    int fd;                         /**< File descriptor */
    char device_path[DEVICE_NAME_SIZE]; /**< Device path */
    media_device_info_t info;       /**< Device information */
    media_format_t format;          /**< Current format */
    media_buffer_t* buffers;        /**< Allocated buffers */
    int buffer_count;               /**< Number of buffers */
    int streaming;                  /**< Streaming state */
    int use_multiplanar;            /**< Multi-planar mode */
} device_context_t;

/**
 * @struct media_session
 * @brief Capture session structure
 */
struct media_session {
    device_context_t* device;       /**< Device context */
    media_session_config_t config;  /**< Session configuration */
    int active;                     /**< Session active state */
};

// ============================================================================
// Global Variables
// ============================================================================

static device_context_t g_devices[MAX_DEVICES];
static int g_device_count = 0;
static int g_initialized = 0;
static media_error_t g_last_error = MEDIA_ERROR_NONE;
static int g_debug_level = DEBUG_ERROR;

// ============================================================================
// Internal Utility Functions
// ============================================================================

/**
 * @brief Thread-safe ioctl wrapper with retry mechanism
 */
static int xioctl(int fd, int request, void* arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (-1 == r && EINTR == errno);
    return r;
}

/**
 * @brief Set last error code
 */
static void set_last_error(media_error_t error)
{
    g_last_error = error;
}

/**
 * @brief Helper function to get pixel format name
 */
static const char* get_format_name_internal(uint32_t pixelformat)
{
    switch (pixelformat) {
        case V4L2_PIX_FMT_YUYV: return "YUYV";
        case V4L2_PIX_FMT_UYVY: return "UYVY";
        case V4L2_PIX_FMT_YUV420: return "YU12";
        case V4L2_PIX_FMT_YVU420: return "YV12";
        case V4L2_PIX_FMT_NV12: return "NV12";
        case V4L2_PIX_FMT_NV21: return "NV21";
        case V4L2_PIX_FMT_RGB565: return "RGB565";
        case V4L2_PIX_FMT_RGB24: return "RGB24";
        case V4L2_PIX_FMT_BGR24: return "BGR24";
        case V4L2_PIX_FMT_RGB32: return "RGB32";
        case V4L2_PIX_FMT_BGR32: return "BGR32";
        case V4L2_PIX_FMT_MJPEG: return "MJPEG";
        case V4L2_PIX_FMT_JPEG: return "JPEG";
        case V4L2_PIX_FMT_H264: return "H264";
        case V4L2_PIX_FMT_SBGGR8: return "BGGR8";
        case V4L2_PIX_FMT_SGBRG8: return "GBRG8";
        case V4L2_PIX_FMT_SGRBG8: return "GRBG8";
        case V4L2_PIX_FMT_SRGGB8: return "RGGB8";
        case V4L2_PIX_FMT_SBGGR10: return "BG10";
        case V4L2_PIX_FMT_SGBRG10: return "GB10";
        case V4L2_PIX_FMT_SGRBG10: return "GR10";
        case V4L2_PIX_FMT_SRGGB10: return "RG10";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Find device context by handle
 */
static device_context_t* find_device(int handle)
{
    if (handle < 0 || handle >= g_device_count) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return NULL;
    }
    
    device_context_t* dev = &g_devices[handle];
    if (dev->fd < 0) {
        set_last_error(MEDIA_ERROR_DEVICE_NOT_FOUND);
        return NULL;
    }
    
    return dev;
}

/**
 * @brief Get current timestamp in nanoseconds
 */
uint64_t libmedia_get_timestamp_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// ============================================================================
// Library Management Functions
// ============================================================================

const char* libmedia_get_version(void)
{
    return VERSION_STRING;
}

int libmedia_init(void)
{
    if (g_initialized) {
        return 0;
    }
    
    memset(g_devices, 0, sizeof(g_devices));
    for (int i = 0; i < MAX_DEVICES; i++) {
        g_devices[i].fd = -1;
    }
    
    g_device_count = 0;
    g_last_error = MEDIA_ERROR_NONE;
    g_initialized = 1;
    
    MEDIA_DEBUG(DEBUG_INFO, "libMedia initialized, version %s", VERSION_STRING);
    return 0;
}

void libmedia_deinit(void)
{
    if (!g_initialized) {
        return;
    }
    
    // Close all open devices
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].fd >= 0) {
            libmedia_close_device(i);
        }
    }
    
    g_initialized = 0;
    MEDIA_DEBUG(DEBUG_INFO, "libMedia deinitialized");
}

void libmedia_set_debug_level(int level)
{
    g_debug_level = level;
}

// ============================================================================
// Device Management Functions
// ============================================================================

int libmedia_open_device(const char* device_path)
{
    if (!g_initialized) {
        libmedia_init();
    }
    
    if (!device_path) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    if (g_device_count >= MAX_DEVICES) {
        set_last_error(MEDIA_ERROR_OUT_OF_MEMORY);
        return -1;
    }
    
    // Open device
    int fd = open(device_path, O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "Failed to open device %s: %s", device_path, strerror(errno));
        set_last_error(MEDIA_ERROR_DEVICE_NOT_FOUND);
        return -1;
    }
    
    // Initialize device context
    device_context_t* dev = &g_devices[g_device_count];
    dev->fd = fd;
    strncpy(dev->device_path, device_path, DEVICE_NAME_SIZE - 1);
    dev->device_path[DEVICE_NAME_SIZE - 1] = '\0';
    dev->buffer_count = 0;
    dev->buffers = NULL;
    dev->streaming = 0;
    dev->use_multiplanar = 0;
    
    MEDIA_DEBUG(DEBUG_INFO, "Opened device %s with handle %d", device_path, g_device_count);
    return g_device_count++;
}

int libmedia_close_device(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    // Stop streaming if active
    if (dev->streaming) {
        if (dev->use_multiplanar) {
            libmedia_stop_streaming_mp(handle);
        } else {
            libmedia_stop_streaming(handle);
        }
    }
    
    // Free buffers if allocated
    if (dev->buffers) {
        libmedia_free_buffers(handle, dev->buffers, dev->buffer_count);
    }
    
    // Close file descriptor
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
    
    MEDIA_DEBUG(DEBUG_INFO, "Closed device %s", dev->device_path);
    return 0;
}

int libmedia_get_device_info(int handle, media_device_info_t* info)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !info) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_capability cap = {0};
    if (xioctl(dev->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QUERYCAP failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_IOCTL_FAILED);
        return -1;
    }
    
    strncpy(info->driver, (char*)cap.driver, sizeof(info->driver) - 1);
    info->driver[sizeof(info->driver) - 1] = '\0';
    strncpy(info->card, (char*)cap.card, sizeof(info->card) - 1);
    info->card[sizeof(info->card) - 1] = '\0';
    strncpy(info->bus_info, (char*)cap.bus_info, sizeof(info->bus_info) - 1);
    info->bus_info[sizeof(info->bus_info) - 1] = '\0';
    info->version = cap.version;
    info->capabilities = cap.capabilities;
    
    // Cache device info
    dev->info = *info;
    
    return 0;
}

int libmedia_check_capabilities(int handle, uint32_t required_caps)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    media_device_info_t info;
    if (libmedia_get_device_info(handle, &info) < 0) {
        return -1;
    }
    
    return (info.capabilities & required_caps) == required_caps ? 1 : 0;
}

/**
 * @brief Check device capabilities (for v4l2_usb.c compatibility)
 */
int libmedia_check_device_caps(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    struct v4l2_capability cap = {0};
    if (xioctl(dev->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QUERYCAP failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_IOCTL_FAILED);
        return -1;
    }
    
    MEDIA_DEBUG(DEBUG_INFO, "Device: %s", cap.card);
    MEDIA_DEBUG(DEBUG_INFO, "Driver: %s", cap.driver);
    
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)) {
        MEDIA_DEBUG(DEBUG_ERROR, "Device does not support multiplanar video capture");
        set_last_error(MEDIA_ERROR_NOT_SUPPORTED);
        return -1;
    }
    
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        MEDIA_DEBUG(DEBUG_ERROR, "Device does not support streaming");
        set_last_error(MEDIA_ERROR_NOT_SUPPORTED);
        return -1;
    }
    
    MEDIA_DEBUG(DEBUG_INFO, "Device supports multiplanar streaming capture");
    return 0;
}

// ============================================================================
// Format Configuration Functions
// ============================================================================

int libmedia_set_format(int handle, media_format_t* format)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !format) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = format->width;
    fmt.fmt.pix.height = format->height;
    fmt.fmt.pix.pixelformat = format->pixelformat;
    fmt.fmt.pix.field = format->field;
    
    if (xioctl(dev->fd, VIDIOC_S_FMT, &fmt) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_S_FMT failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_FORMAT_ERROR);
        return -1;
    }
    
    // Update format structure with actual values
    format->width = fmt.fmt.pix.width;
    format->height = fmt.fmt.pix.height;
    format->pixelformat = fmt.fmt.pix.pixelformat;
    format->field = fmt.fmt.pix.field;
    format->num_planes = 1;
    format->plane_size[0] = fmt.fmt.pix.sizeimage;
    
    // Cache format
    dev->format = *format;
    dev->use_multiplanar = 0;
    
    MEDIA_DEBUG(DEBUG_INFO, "Set format: %dx%d, fourcc=0x%08x", 
                format->width, format->height, format->pixelformat);
    
    return 0;
}

int libmedia_set_format_mp(int handle, media_format_t* format)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !format) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = format->width;
    fmt.fmt.pix_mp.height = format->height;
    fmt.fmt.pix_mp.pixelformat = format->pixelformat;
    fmt.fmt.pix_mp.field = format->field;
    
    if (xioctl(dev->fd, VIDIOC_S_FMT, &fmt) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_S_FMT (MP) failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_FORMAT_ERROR);
        return -1;
    }
    
    // Update format structure
    format->width = fmt.fmt.pix_mp.width;
    format->height = fmt.fmt.pix_mp.height;
    format->pixelformat = fmt.fmt.pix_mp.pixelformat;
    format->field = fmt.fmt.pix_mp.field;
    format->num_planes = fmt.fmt.pix_mp.num_planes;
    
    for (uint32_t i = 0; i < format->num_planes; i++) {
        format->plane_size[i] = fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
    }
    
    // Cache format
    dev->format = *format;
    dev->use_multiplanar = 1;
    
    MEDIA_DEBUG(DEBUG_INFO, "Set MP format: %dx%d, %s, %d planes", 
                format->width, format->height, get_format_name_internal(format->pixelformat), format->num_planes);
    
    return 0;
}

int libmedia_get_format(int handle, media_format_t* format)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !format) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    *format = dev->format;
    return 0;
}

int libmedia_get_format_mp(int handle, media_format_t* format)
{
    return libmedia_get_format(handle, format);
}

// ============================================================================
// Buffer Management Functions
// ============================================================================

int libmedia_request_buffers(int handle, int count, media_buffer_t* buffers)
{
    device_context_t* dev = find_device(handle);
    if (!dev || count <= 0 || !buffers) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_requestbuffers reqbuf = {0};
    reqbuf.count = count;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(dev->fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_REQBUFS failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    // Map buffers
    for (uint32_t i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        
        if (xioctl(dev->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QUERYBUF failed for buffer %d: %s", i, strerror(errno));
            set_last_error(MEDIA_ERROR_BUFFER_ERROR);
            return -1;
        }
        
        buffers[i].start[0] = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, dev->fd, buf.m.offset);
        if (buffers[i].start[0] == MAP_FAILED) {
            MEDIA_DEBUG(DEBUG_ERROR, "mmap failed for buffer %d: %s", i, strerror(errno));
            set_last_error(MEDIA_ERROR_OUT_OF_MEMORY);
            return -1;
        }
        
        buffers[i].length[0] = buf.length;
        buffers[i].num_planes = 1;
        buffers[i].index = i;
    }
    
    dev->buffers = buffers;
    dev->buffer_count = reqbuf.count;
    
    MEDIA_DEBUG(DEBUG_INFO, "Allocated %d buffers", reqbuf.count);
    return reqbuf.count;
}

int libmedia_request_buffers_mp(int handle, int count, media_buffer_t* buffers)
{
    device_context_t* dev = find_device(handle);
    if (!dev || count <= 0 || !buffers) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_requestbuffers reqbuf = {0};
    reqbuf.count = count;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(dev->fd, VIDIOC_REQBUFS, &reqbuf) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_REQBUFS (MP) failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    // Map buffers
    for (uint32_t i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
        
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = VIDEO_MAX_PLANES;
        
        if (xioctl(dev->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QUERYBUF (MP) failed for buffer %d: %s", i, strerror(errno));
            set_last_error(MEDIA_ERROR_BUFFER_ERROR);
            return -1;
        }
        
        buffers[i].num_planes = buf.length;
        buffers[i].index = i;
        
        for (uint32_t p = 0; p < buf.length; p++) {
            buffers[i].start[p] = mmap(NULL, buf.m.planes[p].length, PROT_READ | PROT_WRITE, 
                                     MAP_SHARED, dev->fd, buf.m.planes[p].m.mem_offset);
            if (buffers[i].start[p] == MAP_FAILED) {
                MEDIA_DEBUG(DEBUG_ERROR, "mmap failed for buffer %d plane %d: %s", i, p, strerror(errno));
                set_last_error(MEDIA_ERROR_OUT_OF_MEMORY);
                return -1;
            }
            buffers[i].length[p] = buf.m.planes[p].length;
        }
    }
    
    dev->buffers = buffers;
    dev->buffer_count = reqbuf.count;
    
    MEDIA_DEBUG(DEBUG_INFO, "Allocated %d MP buffers", reqbuf.count);
    return reqbuf.count;
}

int libmedia_free_buffers(int handle, media_buffer_t* buffers, int count)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !buffers || count <= 0) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    // Unmap all buffers
    for (int i = 0; i < count; i++) {
        for (int p = 0; p < buffers[i].num_planes; p++) {
            if (buffers[i].start[p] != MAP_FAILED && buffers[i].start[p] != NULL) {
                munmap(buffers[i].start[p], buffers[i].length[p]);
                buffers[i].start[p] = NULL;
            }
        }
    }
    
    dev->buffers = NULL;
    dev->buffer_count = 0;
    
    MEDIA_DEBUG(DEBUG_INFO, "Freed %d buffers", count);
    return 0;
}

int libmedia_queue_buffer(int handle, int index)
{
    device_context_t* dev = find_device(handle);
    if (!dev || index < 0 || index >= dev->buffer_count) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    
    if (xioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QBUF failed for buffer %d: %s", index, strerror(errno));
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    return 0;
}

int libmedia_queue_buffer_mp(int handle, int index)
{
    device_context_t* dev = find_device(handle);
    if (!dev || index < 0 || index >= dev->buffer_count) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = index;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    if (xioctl(dev->fd, VIDIOC_QBUF, &buf) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_QBUF (MP) failed for buffer %d: %s", index, strerror(errno));
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    return 0;
}

int libmedia_dequeue_buffer(int handle, media_buffer_t* buffer)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !buffer) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    
    if (xioctl(dev->fd, VIDIOC_DQBUF, &buf) == -1) {
        if (errno == EAGAIN) {
            set_last_error(MEDIA_ERROR_TIMEOUT);
        } else {
            MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_DQBUF failed: %s", strerror(errno));
            set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        }
        return -1;
    }
    
    if ((uint32_t)buf.index >= (uint32_t)dev->buffer_count) {
        MEDIA_DEBUG(DEBUG_ERROR, "Invalid buffer index %d", buf.index);
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    *buffer = dev->buffers[buf.index];
    buffer->bytes_used = buf.bytesused;
    buffer->timestamp = (uint64_t)buf.timestamp.tv_sec * 1000000000ULL + 
                       (uint64_t)buf.timestamp.tv_usec * 1000ULL;
    
    return 0;
}

int libmedia_dequeue_buffer_mp(int handle, media_buffer_t* buffer)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !buffer) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {0};
    
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = VIDEO_MAX_PLANES;
    
    if (xioctl(dev->fd, VIDIOC_DQBUF, &buf) == -1) {
        if (errno == EAGAIN) {
            set_last_error(MEDIA_ERROR_TIMEOUT);
        } else {
            MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_DQBUF (MP) failed: %s", strerror(errno));
            set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        }
        return -1;
    }
    
    if ((uint32_t)buf.index >= (uint32_t)dev->buffer_count) {
        MEDIA_DEBUG(DEBUG_ERROR, "Invalid buffer index %d", buf.index);
        set_last_error(MEDIA_ERROR_BUFFER_ERROR);
        return -1;
    }
    
    *buffer = dev->buffers[buf.index];
    buffer->bytes_used = buf.m.planes[0].bytesused;
    buffer->timestamp = (uint64_t)buf.timestamp.tv_sec * 1000000000ULL + 
                       (uint64_t)buf.timestamp.tv_usec * 1000ULL;
    
    return 0;
}

// ============================================================================
// Streaming Control Functions
// ============================================================================

int libmedia_start_streaming(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (xioctl(dev->fd, VIDIOC_STREAMON, &type) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_STREAMON failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_STREAMING_ERROR);
        return -1;
    }
    
    dev->streaming = 1;
    MEDIA_DEBUG(DEBUG_INFO, "Streaming started");
    return 0;
}

int libmedia_start_streaming_mp(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    if (xioctl(dev->fd, VIDIOC_STREAMON, &type) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_STREAMON (MP) failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_STREAMING_ERROR);
        return -1;
    }
    
    dev->streaming = 1;
    MEDIA_DEBUG(DEBUG_INFO, "MP Streaming started");
    return 0;
}

int libmedia_stop_streaming(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if (xioctl(dev->fd, VIDIOC_STREAMOFF, &type) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_STREAMOFF failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_STREAMING_ERROR);
        return -1;
    }
    
    dev->streaming = 0;
    MEDIA_DEBUG(DEBUG_INFO, "Streaming stopped");
    return 0;
}

int libmedia_stop_streaming_mp(int handle)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    
    if (xioctl(dev->fd, VIDIOC_STREAMOFF, &type) == -1) {
        MEDIA_DEBUG(DEBUG_ERROR, "VIDIOC_STREAMOFF (MP) failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_STREAMING_ERROR);
        return -1;
    }
    
    dev->streaming = 0;
    MEDIA_DEBUG(DEBUG_INFO, "MP Streaming stopped");
    return 0;
}

// ============================================================================
// Frame Capture Interface
// ============================================================================

int libmedia_wait_for_frame(int handle, int timeout_ms)
{
    device_context_t* dev = find_device(handle);
    if (!dev) {
        return -1;
    }
    
    fd_set fds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    
    FD_ZERO(&fds);
    FD_SET(dev->fd, &fds);
    
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }
    
    int result = select(dev->fd + 1, &fds, NULL, NULL, ptv);
    
    if (result == -1) {
        if (errno == EINTR) {
            set_last_error(MEDIA_ERROR_TIMEOUT);
            return 0;
        }
        MEDIA_DEBUG(DEBUG_ERROR, "select failed: %s", strerror(errno));
        set_last_error(MEDIA_ERROR_IOCTL_FAILED);
        return -1;
    }
    
    if (result == 0) {
        set_last_error(MEDIA_ERROR_TIMEOUT);
        return 0;
    }
    
    return 1;
}

int libmedia_capture_frame(int handle, media_frame_t* frame, int timeout_ms)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !frame) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    // Wait for frame if timeout specified
    if (timeout_ms != 0) {
        int wait_result = libmedia_wait_for_frame(handle, timeout_ms);
        if (wait_result <= 0) {
            return wait_result;
        }
    }
    
    media_buffer_t buffer;
    int result = libmedia_dequeue_buffer(handle, &buffer);
    if (result < 0) {
        return result;
    }
    
    // Fill frame structure
    frame->data = buffer.start[0];
    frame->size = buffer.bytes_used;
    frame->width = dev->format.width;
    frame->height = dev->format.height;
    frame->pixelformat = dev->format.pixelformat;
    frame->timestamp = buffer.timestamp;
    frame->frame_id = buffer.index;  // Use buffer index as frame ID
    
    return 0;
}

int libmedia_capture_frame_mp(int handle, media_frame_t* frame, int timeout_ms)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !frame) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    // Wait for frame if timeout specified
    if (timeout_ms != 0) {
        int wait_result = libmedia_wait_for_frame(handle, timeout_ms);
        if (wait_result <= 0) {
            return wait_result;
        }
    }
    
    media_buffer_t buffer;
    int result = libmedia_dequeue_buffer_mp(handle, &buffer);
    if (result < 0) {
        return result;
    }
    
    // Fill frame structure
    frame->data = buffer.start[0];
    frame->size = buffer.bytes_used;
    frame->width = dev->format.width;
    frame->height = dev->format.height;
    frame->pixelformat = dev->format.pixelformat;
    frame->timestamp = buffer.timestamp;
    frame->frame_id = buffer.index;  // Use buffer index as frame ID
    
    return 0;
}

int libmedia_release_frame(int handle, media_frame_t* frame)
{
    device_context_t* dev = find_device(handle);
    if (!dev || !frame) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    // Queue buffer back for reuse
    if (dev->use_multiplanar) {
        return libmedia_queue_buffer_mp(handle, frame->frame_id);
    } else {
        return libmedia_queue_buffer(handle, frame->frame_id);
    }
}

// ============================================================================
// High-Level Session Management
// ============================================================================

media_session_t* libmedia_create_session(const media_session_config_t* config)
{
    if (!config || !config->device_path) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return NULL;
    }
    
    media_session_t* session = malloc(sizeof(media_session_t));
    if (!session) {
        set_last_error(MEDIA_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    
    memset(session, 0, sizeof(media_session_t));
    session->config = *config;
    session->active = 0;
    
    // Open device
    int handle = libmedia_open_device(config->device_path);
    if (handle < 0) {
        free(session);
        return NULL;
    }
    
    session->device = &g_devices[handle];
    
    // Set format
    media_format_t format = config->format;
    int result;
    if (config->use_multiplanar) {
        result = libmedia_set_format_mp(handle, &format);
    } else {
        result = libmedia_set_format(handle, &format);
    }
    
    if (result < 0) {
        libmedia_close_device(handle);
        free(session);
        return NULL;
    }
    
    // Allocate buffers
    media_buffer_t* buffers = malloc(sizeof(media_buffer_t) * config->buffer_count);
    if (!buffers) {
        libmedia_close_device(handle);
        free(session);
        set_last_error(MEDIA_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    
    int buffer_count;
    if (config->use_multiplanar) {
        buffer_count = libmedia_request_buffers_mp(handle, config->buffer_count, buffers);
    } else {
        buffer_count = libmedia_request_buffers(handle, config->buffer_count, buffers);
    }
    
    if (buffer_count <= 0) {
        free(buffers);
        libmedia_close_device(handle);
        free(session);
        return NULL;
    }
    
    MEDIA_DEBUG(DEBUG_INFO, "Created session with %d buffers", buffer_count);
    return session;
}

int libmedia_start_session(media_session_t* session)
{
    if (!session || !session->device) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    int handle = session->device - g_devices;
    
    // Queue all buffers
    for (int i = 0; i < session->device->buffer_count; i++) {
        int result;
        if (session->config.use_multiplanar) {
            result = libmedia_queue_buffer_mp(handle, i);
        } else {
            result = libmedia_queue_buffer(handle, i);
        }
        
        if (result < 0) {
            return result;
        }
    }
    
    // Start streaming
    int result;
    if (session->config.use_multiplanar) {
        result = libmedia_start_streaming_mp(handle);
    } else {
        result = libmedia_start_streaming(handle);
    }
    
    if (result < 0) {
        return result;
    }
    
    session->active = 1;
    return 0;
}

int libmedia_stop_session(media_session_t* session)
{
    if (!session || !session->device) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    int handle = session->device - g_devices;
    
    int result;
    if (session->config.use_multiplanar) {
        result = libmedia_stop_streaming_mp(handle);
    } else {
        result = libmedia_stop_streaming(handle);
    }
    
    session->active = 0;
    return result;
}

int libmedia_session_capture_frame(media_session_t* session, media_frame_t* frame, int timeout_ms)
{
    if (!session || !session->device || !frame) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    if (!session->active) {
        set_last_error(MEDIA_ERROR_STREAMING_ERROR);
        return -1;
    }
    
    int handle = session->device - g_devices;
    
    if (session->config.use_multiplanar) {
        return libmedia_capture_frame_mp(handle, frame, timeout_ms);
    } else {
        return libmedia_capture_frame(handle, frame, timeout_ms);
    }
}

int libmedia_session_release_frame(media_session_t* session, media_frame_t* frame)
{
    if (!session || !session->device || !frame) {
        set_last_error(MEDIA_ERROR_INVALID_PARAM);
        return -1;
    }
    
    int handle = session->device - g_devices;
    return libmedia_release_frame(handle, frame);
}

void libmedia_destroy_session(media_session_t* session)
{
    if (!session) {
        return;
    }
    
    if (session->device) {
        int handle = session->device - g_devices;
        
        if (session->active) {
            libmedia_stop_session(session);
        }
        
        libmedia_close_device(handle);
    }
    
    free(session);
    MEDIA_DEBUG(DEBUG_INFO, "Session destroyed");
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* libmedia_get_format_name(uint32_t pixelformat)
{
    switch (pixelformat) {
        case V4L2_PIX_FMT_YUYV: return "YUYV";
        case V4L2_PIX_FMT_UYVY: return "UYVY";
        case V4L2_PIX_FMT_NV12: return "NV12";
        case V4L2_PIX_FMT_NV21: return "NV21";
        case V4L2_PIX_FMT_YUV420: return "YUV420";
        case V4L2_PIX_FMT_RGB24: return "RGB24";
        case V4L2_PIX_FMT_BGR24: return "BGR24";
        case V4L2_PIX_FMT_RGB32: return "RGB32";
        case V4L2_PIX_FMT_BGR32: return "BGR32";
        case V4L2_PIX_FMT_MJPEG: return "MJPEG";
        case V4L2_PIX_FMT_SBGGR8: return "BGGR8";
        case V4L2_PIX_FMT_SBGGR10: return "BGGR10";
        case V4L2_PIX_FMT_SBGGR12: return "BGGR12";
        default: return "UNKNOWN";
    }
}

int libmedia_get_bytes_per_pixel(uint32_t pixelformat)
{
    switch (pixelformat) {
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_UYVY:
            return 2;
        case V4L2_PIX_FMT_RGB24:
        case V4L2_PIX_FMT_BGR24:
            return 3;
        case V4L2_PIX_FMT_RGB32:
        case V4L2_PIX_FMT_BGR32:
            return 4;
        case V4L2_PIX_FMT_NV12:
        case V4L2_PIX_FMT_NV21:
        case V4L2_PIX_FMT_YUV420:
            return 1; // Main plane
        case V4L2_PIX_FMT_SBGGR8:
            return 1;
        case V4L2_PIX_FMT_SBGGR10:
        case V4L2_PIX_FMT_SBGGR12:
            return 2;
        default:
            return 0;
    }
}

size_t libmedia_calculate_frame_size(const media_format_t* format)
{
    if (!format) {
        return 0;
    }
    
    size_t total_size = 0;
    for (uint32_t i = 0; i < format->num_planes; i++) {
        total_size += format->plane_size[i];
    }
    return total_size;
}

// ============================================================================
// Error Handling
// ============================================================================

media_error_t libmedia_get_last_error(void)
{
    return g_last_error;
}

const char* libmedia_get_error_string(media_error_t error)
{
    switch (error) {
        case MEDIA_ERROR_NONE:
            return "No error";
        case MEDIA_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case MEDIA_ERROR_DEVICE_NOT_FOUND:
            return "Device not found";
        case MEDIA_ERROR_DEVICE_BUSY:
            return "Device busy";
        case MEDIA_ERROR_NOT_SUPPORTED:
            return "Operation not supported";
        case MEDIA_ERROR_OUT_OF_MEMORY:
            return "Out of memory";
        case MEDIA_ERROR_IOCTL_FAILED:
            return "IOCTL operation failed";
        case MEDIA_ERROR_TIMEOUT:
            return "Operation timeout";
        case MEDIA_ERROR_BUFFER_ERROR:
            return "Buffer error";
        case MEDIA_ERROR_FORMAT_ERROR:
            return "Format error";
        case MEDIA_ERROR_STREAMING_ERROR:
            return "Streaming error";
        default:
            return "Unknown error";
    }
}