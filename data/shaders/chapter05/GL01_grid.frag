//
#version 460 core

// This fragment shader calculates a programmatic texture that looks like a grid

// The grid lines are rendered based on how fast the uv coordinates change in the image space to avoid the Moiré pattern
// Therefore, we are going to need screen space derivatives

#include <data/shaders/chapter05/GridParameters.h>
#include <data/shaders/chapter05/GridCalculation.h>

layout (location=0) in vec2 uv;
layout (location=0) out vec4 out_FragColor;

void main()
{
	// TODO: gridColor is very complex, remember to come back
	out_FragColor = gridColor(uv);
};
