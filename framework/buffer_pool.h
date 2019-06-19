/* Copyright (c) 2019, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "core/device.h"

namespace vkb
{
/**
 * @brief An allocation of vulkan memory; different buffer allocations,
 *        with different offset and size, may come from the same Vulkan buffer
 */
class BufferAllocation : public NonCopyable
{
  public:
	BufferAllocation() = default;

	BufferAllocation(core::Buffer &buffer, VkDeviceSize size, VkDeviceSize offset);

	void update(uint32_t offset, const std::vector<uint8_t> &data);

	template <class T>
	void update(uint32_t offset, const T &value)
	{
		update(offset, std::vector<uint8_t>{reinterpret_cast<const uint8_t *>(&value),
		                                    reinterpret_cast<const uint8_t *>(&value) + sizeof(T)});
	}

	bool empty() const;

	VkDeviceSize get_size() const;

	VkDeviceSize get_offset() const;

	core::Buffer &get_buffer();

  private:
	core::Buffer *buffer{nullptr};

	VkDeviceSize base_offset{0};

	VkDeviceSize size{0};
};

/**
 * @brief Helper class which handles multiple allocation from the same underlying Vulkan buffer.
 */
class BufferBlock : public NonCopyable
{
  public:
	BufferBlock(Device &device, VkDeviceSize size, VkBufferUsageFlags usage);

	/**
	 * @return An usable view on a portion of the underlying buffer
	 */
	BufferAllocation allocate(uint32_t size);

	VkDeviceSize get_size() const;

	void reset();

  private:
	core::Buffer buffer;

	// Memory alignment, it may change according to the usage
	VkDeviceSize alignment{0};

	// Current offset, it increases on every allocation
	VkDeviceSize offset{0};
};

/**
 * @brief A pool of buffer blocks for a specific usage
 *        It may contain inactive blocks that can be recycled
 */
class BufferPool : public NonCopyable
{
  public:
	BufferPool(Device &device, VkDeviceSize block_size, VkBufferUsageFlags usage);

	BufferBlock &request_buffer_block(VkDeviceSize minimum_size);

	void reset();

  private:
	Device &device;

	/// List of blocks requested
	std::vector<BufferBlock> buffer_blocks;

	/// Minimum size of the blocks
	VkDeviceSize block_size{0};

	VkBufferUsageFlags usage{};

	/// Numbers of active blocks from the start of buffer_blocks
	uint32_t active_buffer_block_count{0};
};
}        // namespace vkb
