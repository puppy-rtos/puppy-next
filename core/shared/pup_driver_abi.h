#ifndef __PUP_DRIVER_ABI_H__
#define __PUP_DRIVER_ABI_H__

#define PUP_DRIVER_DEVICE_FLAG_NONE       0x00000000U
#define PUP_DRIVER_DEVICE_FLAG_RESERVED   0x00000001U
#define PUP_DRIVER_DEVICE_FLAG_DYNAMIC    0x00000002U
#define PUP_DRIVER_DEVICE_FLAG_ONLINE     0x00000004U

#define PUP_DRIVER_CAP_NONE               0x00000000U

typedef struct {
    const char* name;
    unsigned int flags;
    unsigned int capabilities;
    int          channel_id;   /* IPC channel for DevFS proxy dispatch; -1 = no channel */
    int          device_id;    /* driver-defined logical device id for channel multiplexing */
} pup_user_devfs_registration_t;

typedef struct {
    unsigned int irq_num;
    unsigned int flags;
    int          channel_id;   /* channel to receive IRQ pulses; -1 = legacy irq_wait mode */
    unsigned int reserved1;
} pup_user_irq_subscription_t;

#endif