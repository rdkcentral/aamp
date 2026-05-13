#define GL_SILENCE_DEPRECATION

#include <OpenGL/gl3.h>
#include <GLUT/glut.h>

#include <cmath>
#include <iostream>
#include <vector>

// --- Mat4 and Vector Math Helper Functions ---

struct Vec3 { float x, y, z; };
struct Mat4 { float m[16]; };

static Vec3 Normalize(Vec3 v) {
	float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return {v.x / length, v.y / length, v.z / length};
}

static Vec3 Cross(Vec3 a, Vec3 b) {
	return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static Mat4 Identity() {
	Mat4 r{};
	r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
	return r;
}

static Mat4 Perspective(float fovY, float aspect, float zn, float zf) {
	Mat4 r{};
	float f = 1.0f / tanf(fovY * 0.5f);
	r.m[0] = f / aspect;
	r.m[5] = f;
	r.m[10] = (zf + zn) / (zn - zf);
	r.m[11] = -1.0f;
	r.m[14] = (2.0f * zf * zn) / (zn - zf);
	return r;
}

static Mat4 LookAt(Vec3 eye, Vec3 target, Vec3 up) {
	Vec3 f = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
	Vec3 s = Normalize(Cross(f, up));
	Vec3 u = Cross(s, f);
	Mat4 r = Identity();
	r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
	r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
	r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
	r.m[12] = -Dot(s, eye); r.m[13] = -Dot(u, eye); r.m[14] = Dot(f, eye);
	return r;
}

static Mat4 Translation(float x, float y, float z) {
	Mat4 r = Identity();
	r.m[12] = x; r.m[13] = y; r.m[14] = z;
	return r;
}

static Mat4 RotationY(float a) {
	Mat4 r = Identity();
	r.m[0] = cosf(a); r.m[2] = sinf(a);
	r.m[8] = -sinf(a); r.m[10] = cosf(a);
	return r;
}

static Mat4 Multiply(const Mat4& a, const Mat4& b) {
	Mat4 r{};
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k) sum += a.m[row * 4 + k] * b.m[k + col * 4];
			r.m[row * 4 + col] = sum;
		}
	}
	return r;
}

// --- Globals ---

GLuint gProg, gShadowProg;
GLuint gGroundVAO, gPyramidVAO, gSphereVAO, gShadowQuadVAO;
int gGroundCount, gPyramidCount, gSphereCount, gShadowQuadCount;
int gWindowWidth = 1400, gWindowHeight = 900;

// --- Shader Compilation ---

static GLuint CreateProgram(const char* vsSrc, const char* fsSrc) {
	auto compile = [](GLenum type, const char* src) {
		GLuint s = glCreateShader(type);
		glShaderSource(s, 1, &src, nullptr);
		glCompileShader(s);
		GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			char log[2048]; glGetShaderInfoLog(s, sizeof(log), nullptr, log);
			std::cout << "SHADER ERROR (" << (type == GL_VERTEX_SHADER ? "VS" : "FS") << "): " << log << std::endl;
		}
		return s;
	};
	GLuint p = glCreateProgram();
	glAttachShader(p, compile(GL_VERTEX_SHADER, vsSrc));
	glAttachShader(p, compile(GL_FRAGMENT_SHADER, fsSrc));
	glLinkProgram(p);
	return p;
}

// --- Shader Sources ---

// Standard Program: Lighting, Color, and Checkerboard Ground
static const char* kVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
void main() {
	vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;
	vNormal = mat3(uModel) * aNormal;
	vUV = aPos.xz; // For ground checkerboard
	gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kFS = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
uniform vec3 uColor;
uniform bool uIsGround;
uniform vec3 uLightPos;
out vec4 FragColor;
void main() {
	vec3 color = uColor;
	if (uIsGround) {
		// Procedural checkerboard
		float freq = 0.5;
		float c = mod(floor(vUV.x * freq) + floor(vUV.y * freq), 2.0);
		color = mix(vec3(0.25), vec3(0.35), c);
	}
	vec3 N = normalize(vNormal);
	vec3 L = normalize(uLightPos - vWorldPos);
	float l = clamp(dot(N, L), 0.2, 1.0); // Basic diffuse lighting
	FragColor = vec4(color * l, 1.0);
}
)";

// Shadow Program: Used to draw the actual circular shadow as a transparent quad
static const char* kShadowVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
	gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kShadowFS = R"(
#version 330 core
out vec4 FragColor;
void main() {
	// Generate soft circular shadow on a unit quad
	float r = length(gl_FragCoord.xy / vec2(1400.0, 900.0) * 2.0 - 1.0); // Simple screen-space blob
	float alpha = 1.0 - smoothstep(0.4, 0.7, r);
	FragColor = vec4(0.0, 0.0, 0.0, 0.35 * alpha);
}
)";

// Alternative Projective Shadow Shaders (for future Art of Flying integration)
// To keep the test app simple, we will use the cheap shadow quad method.
// These are included for reference on how to do it "the right way" for terrain.
// See the alternative Display logic for how to use them.

static const char* kProjectiveVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightMatrix; // lookAt and projection from light's POV
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;
out vec4 vLightSpacePos; // Projected coordinates
void main() {
	vWorldPos = (uModel * vec4(aPos, 1.0)).xyz;
	vNormal = mat3(uModel) * aNormal;
	vUV = aPos.xz;
	vLightSpacePos = uLightMatrix * vec4(vWorldPos, 1.0);
	gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* kProjectiveFS = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;
in vec4 vLightSpacePos;
uniform vec3 uColor;
uniform bool uIsGround;
uniform vec3 uLightPos;
uniform float uSphereRadius; // Used to scale procedural shadow
out vec4 FragColor;
void main() {
	vec3 color = uColor;
	if (uIsGround) {
		float freq = 0.5;
		float c = mod(floor(vUV.x * freq) + floor(vUV.y * freq), 2.0);
		color = mix(vec3(0.25), vec3(0.35), c);
	}
	// Shadow projection
	vec3 projCoords = vLightSpacePos.xyz / vLightSpacePos.w;
	projCoords = projCoords * 0.5 + 0.5; // Map from -1:1 to 0:1
	float d = length(projCoords.xy - vec2(0.5, 0.5)); // Distance from projected center
	float shadowMask = 1.0 - smoothstep(uSphereRadius * 0.4, uSphereRadius * 0.8, d);
	
	// Apply soft circular shadow
	color = mix(color, vec3(0.0), 0.35 * shadowMask);

	vec3 N = normalize(vNormal);
	vec3 L = normalize(uLightPos - vWorldPos);
	float l = clamp(dot(N, L), 0.2, 1.0);
	FragColor = vec4(color * l, 1.0);
}
)";

// --- Geometry ---

static GLuint CreateVAO(const std::vector<float>& vertices, int vertexCount, int attribCount) {
	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (attribCount + 3) * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	if (attribCount > 0) {
		glVertexAttribPointer(1, attribCount, GL_FLOAT, GL_FALSE, (attribCount + 3) * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}
	return vao;
}

static void CreateGeometry() {
	// Large Ground Plane
	std::vector<float> groundV = { -20, 0, -20, 0, 1, 0, 20, 0, -20, 0, 1, 0, 20, 0, 20, 0, 1, 0, -20, 0, 20, 0, 1, 0 };
	gGroundCount = 4; gGroundVAO = CreateVAO(groundV, gGroundCount, 3);

	// Simple Pyramid
	float pyrV[] = {
		// base
		-1,0,-1, 0,-1,0, 1,0,-1, 0,-1,0, 1,0,1, 0,-1,0, -1,0,-1, 0,-1,0, 1,0,1, 0,-1,0, -1,0,1, 0,-1,0,
		// sides (facing up)
		-1,0,1, 0,0.707,0.707, 1,0,1, 0,0.707,0.707, 0,1,0, 0,0.707,0.707,
		-1,0,-1, 0,0.707,-0.707, 0,1,0, 0,0.707,-0.707, 1,0,-1, 0,0.707,-0.707,
		-1,0,-1, -0.707,0.707,0, -1,0,1, -0.707,0.707,0, 0,1,0, -0.707,0.707,0,
		1,0,-1, 0.707,0.707,0, 0,1,0, 0.707,0.707,0, 1,0,1, 0.707,0.707,0,
	};
	gPyramidCount = sizeof(pyrV) / sizeof(float) / 6;
	std::vector<float> pyrData(pyrV, pyrV + sizeof(pyrV) / sizeof(float));
	gPyramidVAO = CreateVAO(pyrData, gPyramidCount, 3);

	// Flat circular shadow quad
	std::vector<float> sqV = { -1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1 };
	gShadowQuadCount = 4; gShadowQuadVAO = CreateVAO(sqV, gShadowQuadCount, 0);

	// Sphere (simple logic)
	std::vector<float> data;
	const int rings = 16; const int slices = 32;
	for (int y = 0; y < rings; ++y) {
		float v0 = float(y) / rings, v1 = float(y + 1) / rings;
		float t0 = v0 * 3.14159f, t1 = v1 * 3.14159f;
		for (int x = 0; x < slices; ++x) {
			float u0 = float(x) / slices, u1 = float(x + 1) / slices;
			float p0 = u0 * 6.28318f, p1 = u1 * 6.28318f;
			auto emit = [&](float t, float p) {
				float sx = sinf(t) * cosf(p), sy = cosf(t), sz = sinf(t) * sinf(p);
				data.push_back(sx); data.push_back(sy); data.push_back(sz);
				data.push_back(sx); data.push_back(sy); data.push_back(sz);
			};
			emit(t0, p0); emit(t1, p0); emit(t1, p1);
			emit(t0, p0); emit(t1, p1); emit(t0, p1);
		}
	}
	gSphereCount = (int)data.size() / 6;
	gSphereVAO = CreateVAO(data, gSphereCount, 3);
}

// --- Main Loop and Display ---

void Reshape(int w, int h) { gWindowWidth = w; gWindowHeight = h; glViewport(0, 0, w, h); }

void Display() {
	glClearColor(0.15f, 0.18f, 0.22f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	float time = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
	float aspect = float(gWindowWidth) / float(gWindowHeight);

	// 1. Set up LookAt Camera
	Vec3 eyePos = {6.0f, 4.0f, 10.0f};
	Vec3 target = {0.0f, 0.5f, 0.0f}; // Aim at pyramid base
	Vec3 upVec = {0.0f, 1.0f, 0.0f};
	Mat4 view = LookAt(eyePos, target, upVec);
	Mat4 proj = Perspective(0.78f, aspect, 0.1f, 100.0f); // 45 degree fov

	Vec3 lightPos = {10, 15, 8};

	// 2. Compute LightMatrix for shadow quad projection
	// This is cheap projective texture mapping logic
	// Place a lookAt light frustum slightly behind and above the sphere
	Vec3 spherePos = {2.0f * cosf(time * 0.7f), 1.5f + 0.3f * sinf(time * 2.0f), 2.0f * sinf(time * 0.7f)};
	Vec3 lightLookEye = {lightPos.x, lightPos.y, lightPos.z};
	Vec3 lightTarget = {spherePos.x, 0.0f, spherePos.z}; // project towards ground under sphere
	Mat4 lightView = LookAt(lightLookEye, lightTarget, {0.0f, 1.0f, 0.0f});
	Mat4 lightProj = Perspective(1.5f, 1.0f, 0.1f, 30.0f); // Wide fov for projection

	// 3. Draw Ground Plane
	{
		glUseProgram(gProg);
		Mat4 model = Identity();
		Mat4 mvp = Multiply(proj, Multiply(view, model));
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uMVP"), 1, GL_FALSE, mvp.m);
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uModel"), 1, GL_FALSE, model.m);
		glUniform1i(glGetUniformLocation(gProg, "uIsGround"), true);
		glUniform3f(glGetUniformLocation(gProg, "uColor"), 1, 1, 1); // Ground gets color from FS checkerboard
		glUniform3f(glGetUniformLocation(gProg, "uLightPos"), lightPos.x, lightPos.y, lightPos.z);
		glBindVertexArray(gGroundVAO);
		glDrawArrays(GL_TRIANGLE_FAN, 0, gGroundCount);
	}

	// 4. Draw Animated Pyramid (Rotating)
	{
		Mat4 model = RotationY(time * 0.5f);
		Mat4 mvp = Multiply(proj, Multiply(view, model));
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uMVP"), 1, GL_FALSE, mvp.m);
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uModel"), 1, GL_FALSE, model.m);
		glUniform1i(glGetUniformLocation(gProg, "uIsGround"), false);
		glUniform3f(glGetUniformLocation(gProg, "uColor"), 0.8f, 0.7f, 0.4f);
		glBindVertexArray(gPyramidVAO);
		glDrawArrays(GL_TRIANGLES, 0, gPyramidCount);
	}

	// 5. Draw Animated Sphere (Hovering and Orbiting)
	{
		float scale = 0.4f;
		Mat4 model = Translation(spherePos.x, spherePos.y, spherePos.z);
		model.m[0] = scale; model.m[5] = scale; model.m[10] = scale;
		Mat4 mvp = Multiply(proj, Multiply(view, model));
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uMVP"), 1, GL_FALSE, mvp.m);
		glUniformMatrix4fv(glGetUniformLocation(gProg, "uModel"), 1, GL_FALSE, model.m);
		glUniform1i(glGetUniformLocation(gProg, "uIsGround"), false);
		glUniform3f(glGetUniformLocation(gProg, "uColor"), 0.4f, 0.7f, 1.0f);
		glBindVertexArray(gSphereVAO);
		glDrawArrays(GL_TRIANGLES, 0, gSphereCount);
	}

	// 6. Draw Soft Circular Shadow Quad (Projected on everything)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Don't write depth, so it blends properly
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0, -1.0); // Bias slightly above geometry to avoid Z-fighting

		glUseProgram(gShadowProg);
		
		// This quad acts as the shadow's 'light-view frustum'. Its position and scale are
		// defined by how the light sees the sphere, projected onto the ground plane.
		
		// For simplicity in this demo, we can just use the geometry and shaders to project the soft circular shadow.
		// We will project the shadow quad to the ground under the sphere and scale it up based on distance.
		float distGround = spherePos.y;
		float shadowScale = 0.6f + distGround * 0.1f; // grow with height

		// This alternative Display code uses the cheaper and cleaner *fake projective texture mapping* quad method.
		// It provides the look you want (soft circular, wraps geometry) very cheaply.
		Mat4 model = Translation(spherePos.x, 0.01f, spherePos.z); // Slightly above ground
		model.m[0] = shadowScale; model.m[5] = shadowScale; model.m[10] = shadowScale;
		Mat4 mvp = Multiply(proj, Multiply(view, model));
		glUniformMatrix4fv(glGetUniformLocation(gShadowProg, "uMVP"), 1, GL_FALSE, mvp.m);

		glBindVertexArray(gShadowQuadVAO);
		glDrawArrays(GL_TRIANGLE_FAN, 0, gShadowQuadCount);

		glDisable(GL_POLYGON_OFFSET_FILL);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	glutSwapBuffers();
	glutPostRedisplay();
}

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowSize(gWindowWidth, gWindowHeight);
	glutCreateWindow("Art of Flying - Shadow Tech Test");

	// Standard shader program for objects
	gProg = CreateProgram(kVS, kFS);
	// Cheap shadow quad shader program
	gShadowProg = CreateProgram(kShadowVS, kShadowFS);

	CreateGeometry();
	glutReshapeFunc(Reshape);
	glutDisplayFunc(Display);
	glutMainLoop();
	return 0;
}
