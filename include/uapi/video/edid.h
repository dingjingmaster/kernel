/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__linux_video_edid_h__
#define _UAPI__linux_video_edid_h__

/**
 * struct edid_info 是 Linux 内核中的一个数据结构，用于存储和管理EDID（Extended Display Identification Data，扩展显示标识数据）信息。
 * EDID 是一种标准化的数据格式，显示器通过它向计算机提供其详细规格和功能信息。
 *
 * EDID 是由视频电子标准协会（VESA）制定的一种数据结构，通常存储在显示器的 EEPROM（电可擦除可编程只读存储器）中。
 * EDID 包含了显示器的制造商信息、支持的显示模式（分辨率、刷新率等）、色彩特性以及其他相关信息。
 * 计算机通过读取显示器的 EDID 数据，可以自动识别并配置最佳的显示设置。
 */
struct edid_info
{
    unsigned char dummy[128];
};

#endif /* _UAPI__linux_video_edid_h__ */
