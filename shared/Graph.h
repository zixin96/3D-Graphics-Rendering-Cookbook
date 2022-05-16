#pragma once

#include <deque>
#include <limits>

#include "shared/EasyProfilerWrapper.h"
#include "shared/vkRenderers/VulkanCanvas.h"

/**
 * \brief The LinearGraph class is used to render a graph of floating-point values
 */
class LinearGraph
{
public:
	explicit LinearGraph(size_t maxGraphPoints = 256)
		: maxPoints_(maxGraphPoints)
	{
	}

	void addPoint(float value)
	{
		graph_.push_back(value);

		// check and maintain the maximum number of points
		if (graph_.size() > maxPoints_)
			graph_.pop_front();
	}

	void renderGraph(VulkanCanvas& c, const glm::vec4& color = vec4(1.0)) const
	{
		EASY_FUNCTION();

		//  find minimumand maximum values to normalize the graph into the 0...1 range
		float minfps = std::numeric_limits<float>::max();
		float maxfps = std::numeric_limits<float>::min();
		for (float f : graph_)
		{
			if (f < minfps) minfps = f;
			if (f > maxfps) maxfps = f;
		}
		const float range = maxfps - minfps;

		float x = 0.0;
		vec3 p1 = vec3(0, 0, 0);

		// iterate all the points and draw them from left to right near the bottom part of the screen
		for (float f : graph_)
		{
			const float val = (f - minfps) / range;
			const vec3 p2 = vec3(x, val * 0.15f, 0);
			x += 1.0f / maxPoints_;
			c.line(p1, p2, color);
			p1 = p2;
		}
	}

private:
	// stores a collection of values
	std::deque<float> graph_;
	// the maximum number of points that should be visible on the screen
	const size_t maxPoints_;
};
