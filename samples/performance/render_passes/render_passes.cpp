/* Copyright (c) 2018-2019, Arm Limited and Contributors
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

#include "render_passes.h"

#include "common/vk_common.h"
#include "gltf_loader.h"
#include "gui.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "rendering/subpasses/forward_subpass.h"
#include "stats.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#	include "platform/android/android_platform.h"
#endif

RenderPassesSample::RenderPassesSample()
{
	auto &config = get_configuration();

	config.insert<vkb::BoolSetting>(0, cmd_clear, false);
	config.insert<vkb::IntSetting>(0, load.value, 0);
	config.insert<vkb::IntSetting>(0, store.value, 0);

	config.insert<vkb::BoolSetting>(1, cmd_clear, true);
	config.insert<vkb::IntSetting>(1, load.value, 1);
	config.insert<vkb::IntSetting>(1, store.value, 1);
}

void RenderPassesSample::reset_stats_view()
{
	if (load.value == VK_ATTACHMENT_LOAD_OP_LOAD)
	{
		gui->get_stats_view().reset_max_value(vkb::StatIndex::l2_ext_read_bytes);
	}

	if (store.value == VK_ATTACHMENT_STORE_OP_STORE)
	{
		gui->get_stats_view().reset_max_value(vkb::StatIndex::l2_ext_write_bytes);
	}
}

void RenderPassesSample::draw_gui()
{
	auto lines = radio_buttons.size() + 1 /* checkbox */;
	if (camera->get_aspect_ratio() < 1.0f)
	{
		// In portrait, show buttons below heading
		lines = lines * 2;
	}

	gui->show_options_window(
	    /* body = */ [this, lines]() {
		    // Checkbox vkCmdClear
		    ImGui::Checkbox("Use vkCmdClearAttachments (color)", &cmd_clear);

		    // For every option set
		    for (size_t i = 0; i < radio_buttons.size(); ++i)
		    {
			    // Avoid conflicts between buttons with identical labels
			    ImGui::PushID(vkb::to_u32(i));

			    auto &radio_button = radio_buttons[i];

			    ImGui::Text("%s: ", radio_button->description);

			    if (camera->get_aspect_ratio() > 1.0f)
			    {
				    // In landscape, show all options following the heading
				    ImGui::SameLine();
			    }

			    // For every option
			    for (size_t j = 0; j < radio_button->options.size(); ++j)
			    {
				    ImGui::RadioButton(radio_button->options[j], &radio_button->value, vkb::to_u32(j));

				    if (j < radio_button->options.size() - 1)
				    {
					    ImGui::SameLine();
				    }
			    }

			    ImGui::PopID();
		    }
	    },
	    /* lines = */ vkb::to_u32(lines));
}

bool RenderPassesSample::prepare(vkb::Platform &platform)
{
	if (!VulkanSample::prepare(platform))
	{
		return false;
	}

	auto enabled_stats = {vkb::StatIndex::fragment_cycles,
	                      vkb::StatIndex::l2_ext_read_bytes,
	                      vkb::StatIndex::l2_ext_write_bytes};

	stats = std::make_unique<vkb::Stats>(enabled_stats);

	load_scene("scenes/sponza/Sponza01.gltf");

	auto &camera_node = vkb::add_free_camera(*scene, "main_camera", get_render_context().get_surface_extent());
	camera            = dynamic_cast<vkb::sg::PerspectiveCamera *>(&camera_node.get_component<vkb::sg::Camera>());

	vkb::ShaderSource vert_shader("base.vert");
	vkb::ShaderSource frag_shader("base.frag");
	auto              scene_subpass = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), *scene, *camera);

	auto render_pipeline = vkb::RenderPipeline();
	render_pipeline.add_subpass(std::move(scene_subpass));

	set_render_pipeline(std::move(render_pipeline));

	gui = std::make_unique<vkb::Gui>(*this, platform.get_window().get_dpi_factor());

	return true;
}

void RenderPassesSample::draw_renderpass(vkb::CommandBuffer &command_buffer, vkb::RenderTarget &render_target)
{
	std::vector<vkb::LoadStoreInfo> load_store{2};

	// The load operation for the color attachment is selected by the user at run-time
	auto loadop            = static_cast<VkAttachmentLoadOp>(load.value);
	load_store[0].load_op  = loadop;
	load_store[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	load_store[1].load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// Store operation for depth attachment is selected by the user at run-time
	load_store[1].store_op = static_cast<VkAttachmentStoreOp>(store.value);

	get_render_pipeline().set_load_store(load_store);

	auto &extent = render_target.get_extent();

	VkViewport viewport{};
	viewport.width    = static_cast<float>(extent.width);
	viewport.height   = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	command_buffer.set_viewport(0, {viewport});

	VkRect2D scissor{};
	scissor.extent = extent;
	command_buffer.set_scissor(0, {scissor});

	auto &subpasses = render_pipeline->get_subpasses();
	command_buffer.begin_render_pass(render_target, load_store, render_pipeline->get_clear_value(), subpasses);

	if (cmd_clear)
	{
		VkClearAttachment attachment = {};
		// Clear color only
		attachment.aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
		attachment.clearValue      = {0, 0, 0};
		attachment.colorAttachment = 0;

		VkClearRect rect = {};
		rect.layerCount  = 1;
		rect.rect.extent = extent;

		command_buffer.clear(attachment, rect);
	}

	subpasses.at(0)->draw(command_buffer);

	gui->draw(command_buffer);

	command_buffer.end_render_pass();
}

std::unique_ptr<vkb::VulkanSample> create_render_passes()
{
	return std::make_unique<RenderPassesSample>();
}
