// extents of grid in world coordinates (how far from the camera the grid will be visible)
float gridSize = 100.0;

// size of one cell
float gridCellSize = 0.025;

// Choosing the line color: 
// since we render everything against a white background, we are good with black and 50% gray

// color of regular thin lines
vec4 gridColorThin = vec4(0.5, 0.5, 0.5, 1.0);

// color of thick lines (which are rendered every tenth line)
vec4 gridColorThick = vec4(0.0, 0.0, 0.0, 1.0);

// Our grid implementation will change the number of rendered lines based on the grid LOD

// We will switch the LOD when the number of pixels between two adjacent cell lines drops below gridMinPixelsBetweenCells

// minimum number of pixels between cell lines before LOD switch should occur. 
const float gridMinPixelsBetweenCells = 2.0;

const vec3 pos[4] = vec3[4](
	vec3(-1.0, 0.0, -1.0),
	vec3( 1.0, 0.0, -1.0),
	vec3( 1.0, 0.0,  1.0),
	vec3(-1.0, 0.0,  1.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);
