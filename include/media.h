/**
 * @file media.h
 * @brief Universal Media Device Library Interface
 * @version 1.0.0
 * @date 2025-07-01
 * 
 * This header provides a unified interface for media device operations,
 * specifically designed for V4L2 video capture devices. It abstracts
 * the low-level V4L2 operations into a simple, easy-to-use API.
 */

#ifndef LIBMEDIA_H
#define LIBMEDIA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <linux/videodev2.h>

// ============================================================================
// Library Version and Information
// ============================================================================

#define LIBMEDIA_VERSION_MAJOR 1
#define LIBMEDIA_VERSION_MINOR 0
#define LIBMEDIA_VERSION_PATCH 0

/**
 * @brief Get library version string
 * @return Version string in format "major.minor.patch"
 */
const char* libmedia_get_version(void);

/**
 * @brief Initialize the media library
 * @return 0 on success, negative on error
 */
int libmedia_init(void);

/**
 * @brief Cleanup the media library
 */
void libmedia_deinit(void);

// ============================================================================
// Device Configuration and Capabilities
// ============================================================================

/**
 * @struct media_device_info
 * @brief Media device information structure
 */
typedef struct {
    char driver[32];        /**< Driver name */
    char card[32];          /**< Device name */
    char bus_info[32];      /**< Bus information */
    uint32_t version;       /**< Driver version */
    uint32_t capabilities;  /**< Device capabilities */
} media_device_info_t;

/**
 * @struct media_format
 * @brief Video format configuration
 */
typedef struct {
    uint32_t width;         /**< Frame width in pixels */
    uint32_t height;        /**< Frame height in pixels */
    uint32_t pixelformat;   /**< Pixel format (V4L2_PIX_FMT_*) */
    uint32_t field;         /**< Field type */
    uint32_t num_planes;    /**< Number of planes for multiplanar formats */
    uint32_t plane_size[VIDEO_MAX_PLANES]; /**< Size of each plane */
} media_format_t;

/**
 * @brief Open media device
 * @param device_path Path to the video device (e.g., "/dev/video0")
 * @return Device handle on success, negative on error
 */
int libmedia_open_device(const char* device_path);

/**
 * @brief Close media device
 * @param handle Device handle
 * @return 0 on success, negative on error
 */
int libmedia_close_device(int handle);

/**
 * @brief Get device capabilities and information
 * @param handle Device handle
 * @param info Output device information structure
 * @return 0 on success, negative on error
 */
int libmedia_get_device_info(int handle, media_device_info_t* info);

/**
 * @brief Check if device supports required capabilities
 * @param handle Device handle
 * @param required_caps Required capability flags
 * @return 1 if supported, 0 if not supported, negative on error
 */
int libmedia_check_capabilities(int handle, uint32_t required_caps);

// ============================================================================
// Format Configuration
// ============================================================================

/**
 * @brief Set video format for single-planar capture
 * @param handle Device handle
 * @param format Format configuration
 * @return 0 on success, negative on error
 */
int libmedia_set_format(int handle, media_format_t* format);

/**
 * @brief Set video format for multi-planar capture
 * @param handle Device handle
 * @param format Format configuration
 * @return 0 on success, negative on error
 */
int libmedia_set_format_mp(int handle, media_format_t* format);

/**
 * @brief Get current video format
 * @param handle Device handle
 * @param format Output format structure
 * @return 0 on success, negative on error
 */
int libmedia_get_format(int handle, media_format_t* format);

/**
 * @brief Get current multi-planar video format
 * @param handle Device handle
 * @param format Output format structure
 * @return 0 on success, negative on error
 */
int libmedia_get_format_mp(int handle, media_format_t* format);

// ============================================================================
// Buffer Management
// ============================================================================

/**
 * @struct media_buffer
 * @brief Media buffer structure for memory management
 */
typedef struct {
    void* start[VIDEO_MAX_PLANES];      /**< Memory mapped address for each plane */
    size_t length[VIDEO_MAX_PLANES];    /**< Length of each plane */
    int num_planes;                     /**< Number of planes */
    int index;                          /**< Buffer index */
    size_t bytes_used;                  /**< Actual bytes used in the buffer */
    uint64_t timestamp;                 /**< Frame timestamp */
} media_buffer_t;

/**
 * @brief Request and allocate buffers for single-planar capture
 * @param handle Device handle
 * @param count Number of buffers to allocate
 * @param buffers Output buffer array (must be pre-allocated)
 * @return Number of allocated buffers on success, negative on error
 */
int libmedia_request_buffers(int handle, int count, media_buffer_t* buffers);

/**
 * @brief Request and allocate buffers for multi-planar capture
 * @param handle Device handle
 * @param count Number of buffers to allocate
 * @param buffers Output buffer array (must be pre-allocated)
 * @return Number of allocated buffers on success, negative on error
 */
int libmedia_request_buffers_mp(int handle, int count, media_buffer_t* buffers);

/**
 * @brief Free allocated buffers
 * @param handle Device handle
 * @param buffers Buffer array to free
 * @param count Number of buffers
 * @return 0 on success, negative on error
 */
int libmedia_free_buffers(int handle, media_buffer_t* buffers, int count);

/**
 * @brief Queue a buffer for capture
 * @param handle Device handle
 * @param index Buffer index
 * @return 0 on success, negative on error
 */
int libmedia_queue_buffer(int handle, int index);

/**
 * @brief Queue a buffer for multi-planar capture
 * @param handle Device handle
 * @param index Buffer index
 * @return 0 on success, negative on error
 */
int libmedia_queue_buffer_mp(int handle, int index);

/**
 * @brief Dequeue a filled buffer
 * @param handle Device handle
 * @param buffer Output buffer information
 * @return 0 on success, negative on error
 */
int libmedia_dequeue_buffer(int handle, media_buffer_t* buffer);

/**
 * @brief Dequeue a filled buffer for multi-planar capture
 * @param handle Device handle
 * @param buffer Output buffer information
 * @return 0 on success, negative on error
 */
int libmedia_dequeue_buffer_mp(int handle, media_buffer_t* buffer);

// ============================================================================
// Streaming Control
// ============================================================================

/**
 * @brief Start video streaming
 * @param handle Device handle
 * @return 0 on success, negative on error
 */
int libmedia_start_streaming(int handle);

/**
 * @brief Start multi-planar video streaming
 * @param handle Device handle
 * @return 0 on success, negative on error
 */
int libmedia_start_streaming_mp(int handle);

/**
 * @brief Stop video streaming
 * @param handle Device handle
 * @return 0 on success, negative on error
 */
int libmedia_stop_streaming(int handle);

/**
 * @brief Stop multi-planar video streaming
 * @param handle Device handle
 * @return 0 on success, negative on error
 */
int libmedia_stop_streaming_mp(int handle);

// ============================================================================
// Frame Capture Interface
// ============================================================================

/**
 * @struct media_frame
 * @brief Frame data structure
 */
typedef struct {
    void* data;             /**< Frame data pointer */
    size_t size;            /**< Frame data size */
    uint32_t width;         /**< Frame width */
    uint32_t height;        /**< Frame height */
    uint32_t pixelformat;   /**< Pixel format */
    uint64_t timestamp;     /**< Frame timestamp */
    uint32_t frame_id;      /**< Frame sequence number */
} media_frame_t;

/**
 * @brief Wait for frame data to be available
 * @param handle Device handle
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return 1 if data available, 0 if timeout, negative on error
 */
int libmedia_wait_for_frame(int handle, int timeout_ms);

/**
 * @brief Capture a single frame (blocking)
 * @param handle Device handle
 * @param frame Output frame structure
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return 0 on success, negative on error
 */
int libmedia_capture_frame(int handle, media_frame_t* frame, int timeout_ms);

/**
 * @brief Capture a single frame from multi-planar device (blocking)
 * @param handle Device handle
 * @param frame Output frame structure
 * @param timeout_ms Timeout in milliseconds (-1 for infinite)
 * @return 0 on success, negative on error
 */
int libmedia_capture_frame_mp(int handle, media_frame_t* frame, int timeout_ms);

/**
 * @brief Release a captured frame back to the driver
 * @param handle Device handle
 * @param frame Frame to release
 * @return 0 on success, negative on error
 */
int libmedia_release_frame(int handle, media_frame_t* frame);

// ============================================================================
// High-Level Capture Session Management
// ============================================================================

/**
 * @struct media_session
 * @brief Capture session handle (opaque structure)
 */
typedef struct media_session media_session_t;

/**
 * @struct media_session_config
 * @brief Configuration for capture session
 */
typedef struct {
    const char* device_path;    /**< Device path */
    media_format_t format;      /**< Video format */
    int buffer_count;           /**< Number of buffers */
    int use_multiplanar;        /**< Use multi-planar API */
    int nonblocking;            /**< Non-blocking mode */
} media_session_config_t;

/**
 * @brief Create a new capture session
 * @param config Session configuration
 * @return Session handle on success, NULL on error
 */
media_session_t* libmedia_create_session(const media_session_config_t* config);

/**
 * @brief Start capture session
 * @param session Session handle
 * @return 0 on success, negative on error
 */
int libmedia_start_session(media_session_t* session);

/**
 * @brief Stop capture session
 * @param session Session handle
 * @return 0 on success, negative on error
 */
int libmedia_stop_session(media_session_t* session);

/**
 * @brief Capture frame from session
 * @param session Session handle
 * @param frame Output frame structure
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative on error
 */
int libmedia_session_capture_frame(media_session_t* session, media_frame_t* frame, int timeout_ms);

/**
 * @brief Release frame back to session
 * @param session Session handle
 * @param frame Frame to release
 * @return 0 on success, negative on error
 */
int libmedia_session_release_frame(media_session_t* session, media_frame_t* frame);

/**
 * @brief Destroy capture session
 * @param session Session handle
 */
void libmedia_destroy_session(media_session_t* session);

/**
 * @brief Get device handle from session
 * @param session Session handle
 * @return Device handle on success, negative on error
 */
int libmedia_session_get_device_handle(media_session_t* session);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get pixel format name
 * @param pixelformat V4L2 pixel format code
 * @return Format name string
 */
const char* libmedia_get_format_name(uint32_t pixelformat);

/**
 * @brief Get bytes per pixel for a format
 * @param pixelformat V4L2 pixel format code
 * @return Bytes per pixel, 0 if unknown
 */
int libmedia_get_bytes_per_pixel(uint32_t pixelformat);

/**
 * @brief Calculate frame size for given format
 * @param format Format structure
 * @return Frame size in bytes
 */
size_t libmedia_calculate_frame_size(const media_format_t* format);

/**
 * @brief Get current timestamp in nanoseconds
 * @return Timestamp in nanoseconds
 */
uint64_t libmedia_get_timestamp_ns(void);

// ============================================================================
// Error Handling
// ============================================================================

typedef enum {
    MEDIA_ERROR_NONE = 0,
    MEDIA_ERROR_INVALID_PARAM = -1,
    MEDIA_ERROR_DEVICE_NOT_FOUND = -2,
    MEDIA_ERROR_DEVICE_BUSY = -3,
    MEDIA_ERROR_NOT_SUPPORTED = -4,
    MEDIA_ERROR_OUT_OF_MEMORY = -5,
    MEDIA_ERROR_IOCTL_FAILED = -6,
    MEDIA_ERROR_TIMEOUT = -7,
    MEDIA_ERROR_BUFFER_ERROR = -8,
    MEDIA_ERROR_FORMAT_ERROR = -9,
    MEDIA_ERROR_STREAMING_ERROR = -10
} media_error_t;

/**
 * @brief Get last error code
 * @return Error code
 */
media_error_t libmedia_get_last_error(void);

/**
 * @brief Get error description
 * @param error Error code
 * @return Error description string
 */
const char* libmedia_get_error_string(media_error_t error);

/**
 * @brief Set debug output level
 * @param level Debug level (0=none, 1=error, 2=warning, 3=info, 4=debug)
 */
void libmedia_set_debug_level(int level);

// ============================================================================
// V4L2 Compatibility Functions (for v4l2_usb.c migration)
// ============================================================================

/**
 * @brief xioctl wrapper function
 * @param fd File descriptor
 * @param request IOCTL request
 * @param arg Argument pointer
 * @return 0 on success, -1 on error
 */
int libmedia_xioctl(int fd, int request, void* arg);

/**
 * @brief Check device capabilities (multiplanar support)
 * @param handle Device handle
 * @return 0 on success, -1 on error
 */
int libmedia_check_device_caps(int handle);

/**
 * @brief Set multiplanar format (V4L2 compatible)
 * @param handle Device handle
 * @param fmt Output format structure
 * @param width Frame width
 * @param height Frame height
 * @param pixelformat V4L2 pixel format
 * @return 0 on success, -1 on error
 */
int libmedia_set_format_mp_v4l2(int handle, struct v4l2_format* fmt, uint32_t width, uint32_t height, uint32_t pixelformat);

/**
 * @brief Queue buffer for multiplanar capture
 * @param handle Device handle
 * @param index Buffer index
 * @return 0 on success, -1 on error
 */
int libmedia_queue_buffer_mp(int handle, int index);

/**
 * @brief Dequeue buffer from multiplanar capture (V4L2 compatible)
 * @param handle Device handle
 * @param index Output buffer index
 * @param bytes_used Output bytes used
 * @return 0 on success, -1 on error
 */
int libmedia_dequeue_buffer_mp_v4l2(int handle, int* index, size_t* bytes_used);

/**
 * @brief Start multiplanar streaming
 * @param handle Device handle
 * @return 0 on success, -1 on error
 */
int libmedia_start_streaming_mp(int handle);

/**
 * @brief Stop multiplanar streaming
 * @param handle Device handle
 * @return 0 on success, -1 on error
 */
int libmedia_stop_streaming_mp(int handle);

// ============================================================================
// Camera Control Interface
// ============================================================================

/**
 * @enum media_control_id
 * @brief Camera control identifiers
 */
typedef enum {
    MEDIA_CTRL_EXPOSURE = 0x00980911,           /**< Exposure time */
    MEDIA_CTRL_HORIZONTAL_FLIP = 0x00980914,    /**< Horizontal flip */
    MEDIA_CTRL_VERTICAL_FLIP = 0x00980915,      /**< Vertical flip */
    MEDIA_CTRL_VERTICAL_BLANKING = 0x009e0901,  /**< Vertical blanking */
    MEDIA_CTRL_HORIZONTAL_BLANKING = 0x009e0902, /**< Horizontal blanking */
    MEDIA_CTRL_ANALOGUE_GAIN = 0x009e0903,      /**< Analogue gain */
    MEDIA_CTRL_LINK_FREQUENCY = 0x009f0901,     /**< Link frequency */
    MEDIA_CTRL_PIXEL_RATE = 0x009f0902,         /**< Pixel rate */
    MEDIA_CTRL_TEST_PATTERN = 0x009f0903        /**< Test pattern */
} media_control_id_t;

/**
 * @struct media_control_info
 * @brief Control information structure
 */
typedef struct {
    uint32_t id;            /**< Control ID */
    char name[32];          /**< Control name */
    int32_t min;            /**< Minimum value */
    int32_t max;            /**< Maximum value */
    int32_t step;           /**< Step size */
    int32_t default_value;  /**< Default value */
    int32_t current_value;  /**< Current value */
    uint32_t flags;         /**< Control flags */
    uint32_t type;          /**< Control type */
} media_control_info_t;

/**
 * @brief Open sub-device for control operations
 * @param subdev_path Path to the sub-device (e.g., "/dev/v4l-subdev2")
 * @return Sub-device handle on success, negative on error
 */
int libmedia_open_subdev(const char* subdev_path);

/**
 * @brief Close sub-device
 * @param subdev_handle Sub-device handle
 * @return 0 on success, negative on error
 */
int libmedia_close_subdev(int subdev_handle);

/**
 * @brief Get control value
 * @param subdev_handle Sub-device handle
 * @param control_id Control ID
 * @param value Output control value
 * @return 0 on success, negative on error
 */
int libmedia_get_control(int subdev_handle, uint32_t control_id, int32_t* value);

/**
 * @brief Set control value
 * @param subdev_handle Sub-device handle
 * @param control_id Control ID
 * @param value Control value to set
 * @return 0 on success, negative on error
 */
int libmedia_set_control(int subdev_handle, uint32_t control_id, int32_t value);

/**
 * @brief Get control information (min, max, step, etc.)
 * @param subdev_handle Sub-device handle
 * @param control_id Control ID
 * @param info Output control information
 * @return 0 on success, negative on error
 */
int libmedia_get_control_info(int subdev_handle, uint32_t control_id, media_control_info_t* info);

/**
 * @brief List all available controls
 * @param subdev_handle Sub-device handle
 * @param controls Output control array (must be pre-allocated)
 * @param max_controls Maximum number of controls to return
 * @return Number of controls found on success, negative on error
 */
int libmedia_list_controls(int subdev_handle, media_control_info_t* controls, int max_controls);

// ============================================================================
// Convenience Functions for Common Controls
// ============================================================================

/**
 * @brief Set exposure value
 * @param subdev_handle Sub-device handle
 * @param exposure Exposure value (1-1352)
 * @return 0 on success, negative on error
 */
int libmedia_set_exposure(int subdev_handle, int32_t exposure);

/**
 * @brief Get exposure value
 * @param subdev_handle Sub-device handle
 * @param exposure Output exposure value
 * @return 0 on success, negative on error
 */
int libmedia_get_exposure(int subdev_handle, int32_t* exposure);

/**
 * @brief Set analogue gain value
 * @param subdev_handle Sub-device handle
 * @param gain Gain value (128-99614)
 * @return 0 on success, negative on error
 */
int libmedia_set_gain(int subdev_handle, int32_t gain);

/**
 * @brief Get analogue gain value
 * @param subdev_handle Sub-device handle
 * @param gain Output gain value
 * @return 0 on success, negative on error
 */
int libmedia_get_gain(int subdev_handle, int32_t* gain);

/**
 * @brief Set horizontal flip
 * @param subdev_handle Sub-device handle
 * @param enable 1 to enable, 0 to disable
 * @return 0 on success, negative on error
 */
int libmedia_set_horizontal_flip(int subdev_handle, int enable);

/**
 * @brief Set vertical flip
 * @param subdev_handle Sub-device handle
 * @param enable 1 to enable, 0 to disable
 * @return 0 on success, negative on error
 */
int libmedia_set_vertical_flip(int subdev_handle, int enable);

/**
 * @brief Set vertical blanking
 * @param subdev_handle Sub-device handle
 * @param blanking Vertical blanking value (64-31471)
 * @return 0 on success, negative on error
 */
int libmedia_set_vertical_blanking(int subdev_handle, int32_t blanking);

/**
 * @brief Set test pattern
 * @param subdev_handle Sub-device handle
 * @param pattern Test pattern mode (0=disabled, 1-4=various patterns)
 * @return 0 on success, negative on error
 */
int libmedia_set_test_pattern(int subdev_handle, int32_t pattern);

/**
 * @brief Set crop selection
 * @param handle Device handle (not sub-device)
 * @param top Top offset for crop
 * @param left Left offset for crop
 * @param width Crop width
 * @param height Crop height
 * @return 0 on success, negative on error
 */
int libmedia_set_crop_selection(int handle, int top, int left, int width, int height);

/**
 * @brief Get crop selection
 * @param handle Device handle (not sub-device)
 * @param top Output top offset
 * @param left Output left offset
 * @param width Output crop width
 * @param height Output crop height
 * @return 0 on success, negative on error
 */
int libmedia_get_crop_selection(int handle, int* top, int* left, int* width, int* height);

#ifdef __cplusplus
}
#endif

#endif // LIBMEDIA_H