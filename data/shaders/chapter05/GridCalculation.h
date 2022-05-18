// This file contains helper function that aids the grid calculations

float log10(float x)
{
	return log(x) / log(10.0);
}

float satf(float x)
{
	return clamp(x, 0.0, 1.0);
}

vec2 satv(vec2 x)
{
	return clamp(x, vec2(0.0), vec2(1.0));
}

float max2(vec2 v)
{
	return max(v.x, v.y);
}

// dFdx, dFdy — return the partial derivative of an argument with respect to x or y

vec4 gridColor(vec2 uv)
{
	// start by calculating the screen space length of the derivatives of the uv coordinates
	vec2 dudv = vec2(
		length(vec2(dFdx(uv.x), dFdy(uv.x))),
		length(vec2(dFdx(uv.y), dFdy(uv.y)))
	);

	// By knowing the derivatives, the current LOD of our grid can be calculated in the following way:
	//  A logarithm base of 10 is used to ensure each next LOD covers at least pow(10, lodLevel) more cells of the previous LOD
	float lodLevel = max(0.0, log10((length(dudv) * gridMinPixelsBetweenCells) / gridCellSize) + 1.0);
	// Besides the LOD value itself, we are going to need a fading factor to render smooth transitions between the adjacent levels
	// This can be obtained by taking a fractional part of the floating-point LOD level
	float lodFade = fract(lodLevel);

	// The LOD levels are blended between each other
	// To render them, we have to calculate the cell size for each LOD
	// cell sizes for lod0, lod1 and lod2
	float lod0 = gridCellSize * pow(10.0, floor(lodLevel));
	float lod1 = lod0 * 10.0;
	float lod2 = lod1 * 10.0;

	// To be able to draw antialiased lines using alpha transparency, 
	// we need to increase the screen coverage of our lines
	// Let's make sure each line covers up to 4 pixels:
	// each anti-aliased line covers up to 4 pixels
	dudv *= 4.0;

	// Now we should get a coverage alpha value that corresponds to each calculated LOD level of the grid
	// calculate absolute distances to cell line centers for each lod and pick max X/Y to get coverage alpha value
	float lod0a = max2( vec2(1.0) - abs(satv(mod(uv, lod0) / dudv) * 2.0 - vec2(1.0)) );
	float lod1a = max2( vec2(1.0) - abs(satv(mod(uv, lod1) / dudv) * 2.0 - vec2(1.0)) );
	float lod2a = max2( vec2(1.0) - abs(satv(mod(uv, lod2) / dudv) * 2.0 - vec2(1.0)) );

	// Nonzero alpha values represent non-empty transition areas of the grid
	// blend between falloff colors to handle LOD transition
	vec4 c = lod2a > 0.0 ? gridColorThick : lod1a > 0.0 ? mix(gridColorThick, gridColorThin, lodFade) : gridColorThin;

	// Last but not least, make the grid disappear when it is far away from the camera
	// calculate opacity falloff based on distance to grid extents
	float opacityFalloff = (1.0 - satf(length(uv) / gridSize));

	// blend between LOD level alphas and scale with opacity falloff
	c.a *= (lod2a > 0.0 ? lod2a : lod1a > 0.0 ? lod1a : (lod0a * (1.0-lodFade))) * opacityFalloff;

	return c;
}
