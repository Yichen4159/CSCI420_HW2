/*
  CSCI 420 Computer Graphics, Computer Science, USC
  Assignment f Height Fields with Shaders.
  C/C++ starter code

  Student username: <type your USC username here>
*/

#include "openGLHeader.h"
#include "glutHeader.h"
#include "openGLMatrix.h"
#include "imageIO.h"
#include "pipelineProgram.h"
#include "vbo.h"
#include "vao.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <iostream>
#include <cstring>
#include <vector>

#if defined(WIN32) || defined(_WIN32)
#ifdef _DEBUG
#pragma comment(lib, "glew32d.lib")
#else
#pragma comment(lib, "glew32.lib")
#endif
#endif

#if defined(WIN32) || defined(_WIN32)
char shaderBasePath[1024] = SHADER_BASE_PATH;
#else
char shaderBasePath[1024] = "../openGLHelper";
#endif

using namespace std;

const int NUM_SEGMENTS = 51; // Number of line segments to draw the spline

float scale = 0.5f;

int currentIndex = 0;
float waitInterval = 80; //ms
bool toggleAnimation = false;

// represents one control point along the spline 
struct Point
{
	double x;
	double y;
	double z;
};

// spline struct 
// contains how many control points the spline has, and an array of control points 
struct Spline
{
	int numControlPoints;
	Point* points;
};

// the spline array 
Spline* splines;
// total number of splines 
int numSplines = 1;

int loadSplines(char* argv)
{
	char* cName = (char*)malloc(128 * sizeof(char));
	FILE* fileList;
	FILE* fileSpline;
	int iType, i = 0, j = 0, iLength;

	// load the track file 
	fileList = fopen(argv, "r");
	if (fileList == NULL)
	{
		printf("can't open file\n");
		exit(1);
	}

	// stores the number of splines in a global variable 
	fscanf(fileList, "%d", &numSplines);

	splines = (Spline*)malloc(numSplines * sizeof(Spline));

	// reads through the spline files 
	for (j = 0; j < numSplines; j++)
	{
		i = 0;
		fscanf(fileList, "%s", cName);
		fileSpline = fopen(cName, "r");

		if (fileSpline == NULL)
		{
			printf("can't open file\n");
			exit(1);
		}

		// gets length for spline file
		fscanf(fileSpline, "%d %d", &iLength, &iType);

		// allocate memory for all the points
		splines[j].points = (Point*)malloc(iLength * sizeof(Point));
		splines[j].numControlPoints = iLength;

		double curMax = -DBL_MAX;

		// saves the data to the struct
		while (fscanf(fileSpline, "%lf %lf %lf",
			&splines[j].points[i].x,
			&splines[j].points[i].y,
			&splines[j].points[i].z) != EOF)
		{
			double m = max(max(splines[j].points[i].x, splines[j].points[i].y), splines[j].points[i].z);
			curMax = max(curMax, m);
			i++;
		}

		if (curMax > 1)
		{
			for (int k = 0; k < splines[j].numControlPoints; k++)
			{
				splines[j].points[k].x /= curMax;
				splines[j].points[k].y /= curMax;
				splines[j].points[k].z /= curMax;
			}
		}

		for (int k = 0; k < splines[j].numControlPoints; k++)
		{
			glm::vec3 v(splines[j].points[k].x, splines[j].points[k].y, splines[j].points[k].z);
			glm::mat4 mr = glm::rotate(glm::mat4(1.0f), glm::radians(-120.0f), glm::vec3(1.0, 1.0, 1.0));
			glm::mat4 mr2 = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
			glm::mat4 mt = glm::translate(glm::mat4(1.0f), glm::vec3(-0.7, -0.7, -0.7));
			glm::mat4 mt2 = glm::translate(glm::mat4(1.0f), glm::vec3(0.0, 0.2, 0.1));
			glm::vec4 new_pos = mt2 * mr2 * mr * mt * glm::vec4(v, 1.0);
			splines[j].points[k].x = new_pos.x;
			splines[j].points[k].y = new_pos.y;
			splines[j].points[k].z = new_pos.z;
		}
	}

	free(cName);

	return 0;
}

int mousePos[2]; // x,y screen coordinates of the current mouse position

int leftMouseButton = 0; // 1 if pressed, 0 if not 
int middleMouseButton = 0; // 1 if pressed, 0 if not
int rightMouseButton = 0; // 1 if pressed, 0 if not

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROL_STATE;
CONTROL_STATE controlState = ROTATE;

// Transformations of the terrain.
float terrainRotate[3] = { 0.0f, 0.0f, 0.0f };
// terrainRotate[0] gives the rotation around x-axis (in degrees)
// terrainRotate[1] gives the rotation around y-axis (in degrees)
// terrainRotate[2] gives the rotation around z-axis (in degrees)
float terrainTranslate[3] = { 0.0f, 0.0f, 0.0f };
float terrainScale[3] = { 1.0f, 1.0f, 1.0f };

// Width and height of the OpenGL window, in pixels.
int windowWidth = 1280;
int windowHeight = 720;
char windowTitle[512] = "CSCI 420 Homework 2";

// Stores the image loaded from disk.
ImageIO* textureImage;

// Number of vertices in the single triangle (starter code).
int numPointVertices;
int numLineVertices;
int numLeftTrackVertices;
int numRightTrackVertices;
int numCrossSectionVertices;

// CSCI 420 helper classes.
OpenGLMatrix matrix;
PipelineProgram* pipelineProgram = nullptr;
PipelineProgram* skyboxProgram = nullptr;
VBO* pointPositionsVBO = nullptr;
VBO* linePositionsVBO = nullptr;
VBO* pointColorsVBO = nullptr;
VBO* lineColorsVBO = nullptr;

VBO* leftTrackPositionsVBO = nullptr;
VBO* leftTrackNormalsVBO = nullptr;
VBO* rightTrackPositionsVBO = nullptr;
VBO* rightTrackNormalsVBO = nullptr;
VBO* crossSectionPositionsVBO = nullptr;
VBO* crossSectionNormalsVBO = nullptr;

VAO* pointVAO = nullptr;
VAO* lineVAO = nullptr;

VAO* leftTrackVAO = nullptr;
VAO* rightTrackVAO = nullptr;
VAO* crossSectionVAO = nullptr;

std::vector<glm::vec3> terrainPointPositions;
std::vector<glm::vec3> terrainPointTangents;
std::vector<glm::vec3> terrainPointNormals;
std::vector<glm::vec3> terrainPointBinormals;
std::vector<glm::vec3> terrainLinePositions;
std::vector<glm::vec4> terrainPointColors;
std::vector<glm::vec4> terrainLineColors;

std::vector<glm::vec3> leftTrackPositions;
std::vector<glm::vec3> leftTrackNormals;
std::vector<glm::vec3> rightTrackPositions;
std::vector<glm::vec3> rightTrackNormals;
std::vector<glm::vec3> crossSectionPositions;
std::vector<glm::vec3> crossSectionNormals;

glm::vec3 eye{ 0.0f, 0.0f, 5.0f };

glm::vec3 lightDir{ -1.0f, -1.0f, -1.0f };
glm::vec3 lightColor{ 1.0f, 1.0f, 1.0f };

const float CameraMoveSpeed = 1.0f;

GLuint skyboxVAO = 0;

GLuint cubemapTexture = 0;

// Write a screenshot to the specified filename.
void saveScreenshot(const char* filename)
{
	unsigned char* screenshotData = new unsigned char[windowWidth * windowHeight * 3];
	glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, screenshotData);

	ImageIO screenshotImg(windowWidth, windowHeight, 3, screenshotData);

	if (screenshotImg.save(filename, ImageIO::FORMAT_JPEG) == ImageIO::OK)
		cout << "File " << filename << " saved successfully." << endl;
	else cout << "Failed to save file " << filename << '.' << endl;

	delete[] screenshotData;
}

// loads a cubemap texture from 6 individual texture faces
// order:
// +X (right)
// -X (left)
// +Y (top)
// -Y (bottom)
// +Z (front) 
// -Z (back)
// -------------------------------------------------------
unsigned int loadCubemap(vector<std::string> faces)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

	for (unsigned int i = 0; i < faces.size(); i++)
	{
		auto textureImage = new ImageIO();

		if (textureImage->loadJPEG(faces[i].c_str()) != ImageIO::OK)
		{
			cout << "Error reading image " << faces[i] << "." << endl;
			exit(EXIT_FAILURE);
		}

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, textureImage->getWidth(), textureImage->getHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, textureImage->getPixels());
	
		delete textureImage;
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return textureID;
}

void generateSkybox()
{
	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	// skybox VAO
	unsigned int skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// load textures
	// -------------
	vector<std::string> faces
	{
		"textures/skybox/right.jpg",
		"textures/skybox/left.jpg",
		"textures/skybox/top.jpg",
		"textures/skybox/bottom.jpg",
		"textures/skybox/front.jpg",
		"textures/skybox/back.jpg",
	};
	
	cubemapTexture = loadCubemap(faces);
}

void timerFunc(int t)
{
	if (currentIndex == terrainPointPositions.size() - 1)
		currentIndex = 0;
	if (!toggleAnimation)
	{
		glutTimerFunc(waitInterval, timerFunc, currentIndex);
	}
	else {
		glutTimerFunc(waitInterval, timerFunc, ++currentIndex);
	}
	glutPostRedisplay();
}

void idleFunc()
{
	// Do some stuff... 
	// For example, here, you can save the screenshots to disk (to make the animation).
	if (GetAsyncKeyState('W') & 0x8000)
	{
		eye.z -= CameraMoveSpeed;
	}

	if (GetAsyncKeyState('S') & 0x8000)
	{
		eye.z += CameraMoveSpeed;
	}

	if (GetAsyncKeyState('A') & 0x8000)
	{
		eye.x -= CameraMoveSpeed;
	}

	if (GetAsyncKeyState('D') & 0x8000)
	{
		eye.x += CameraMoveSpeed;
	}

	if (GetAsyncKeyState('Q') & 0x8000)
	{
		eye.y -= CameraMoveSpeed;
	}

	if (GetAsyncKeyState('E') & 0x8000)
	{
		eye.y += CameraMoveSpeed;
	}

	// Notify GLUT that it should call displayFunc.
	glutPostRedisplay();
}

void reshapeFunc(int w, int h)
{
	glViewport(0, 0, w, h);

	// When the window has been resized, we need to re-set our projection matrix.
	matrix.SetMatrixMode(OpenGLMatrix::Projection);
	matrix.LoadIdentity();
	// You need to be careful about setting the zNear and zFar. 
	// Anything closer than zNear, or further than zFar, will be culled.
	const float zNear = 0.01f;
	const float zFar = 100.0f;
	const float humanFieldOfView = 45.0f;
	matrix.Perspective(humanFieldOfView, 1.0f * w / h, zNear, zFar);
}

void mouseMotionDragFunc(int x, int y)
{
	// Mouse has moved, and one of the mouse buttons is pressed (dragging).

	// the change in mouse position since the last invocation of this function
	int mousePosDelta[2] = { x - mousePos[0], y - mousePos[1] };

	switch (controlState)
	{
		// translate the terrain
	case TRANSLATE:
		if (leftMouseButton)
		{
			// control x,y translation via the left mouse button
			terrainTranslate[0] += mousePosDelta[0] * 0.01f;
			terrainTranslate[1] -= mousePosDelta[1] * 0.01f;
		}
		if (middleMouseButton)
		{
			// control z translation via the middle mouse button
			terrainTranslate[2] += mousePosDelta[1] * 0.01f;
		}
		break;

		// rotate the terrain
	case ROTATE:
		if (leftMouseButton)
		{
			// control x,y rotation via the left mouse button
			terrainRotate[0] += mousePosDelta[1];
			terrainRotate[1] += mousePosDelta[0];
		}
		if (middleMouseButton)
		{
			// control z rotation via the middle mouse button
			terrainRotate[2] += mousePosDelta[1];
		}
		break;

		// scale the terrain
	case SCALE:
		if (leftMouseButton)
		{
			// control x,y scaling via the left mouse button
			terrainScale[0] *= 1.0f + mousePosDelta[0] * 0.01f;
			terrainScale[1] *= 1.0f - mousePosDelta[1] * 0.01f;
		}
		if (middleMouseButton)
		{
			// control z scaling via the middle mouse button
			terrainScale[2] *= 1.0f - mousePosDelta[1] * 0.01f;
		}
		break;
	}

	// store the new mouse position
	mousePos[0] = x;
	mousePos[1] = y;
}

void mouseMotionFunc(int x, int y)
{
	// Mouse has moved.
	// Store the new mouse position.
	mousePos[0] = x;
	mousePos[1] = y;
}

void mouseButtonFunc(int button, int state, int x, int y)
{
	// A mouse button has has been pressed or depressed.

	// Keep track of the mouse button state, in leftMouseButton, middleMouseButton, rightMouseButton variables.
	switch (button)
	{
	case GLUT_LEFT_BUTTON:
		leftMouseButton = (state == GLUT_DOWN);
		break;

	case GLUT_MIDDLE_BUTTON:
		middleMouseButton = (state == GLUT_DOWN);
		break;

	case GLUT_RIGHT_BUTTON:
		rightMouseButton = (state == GLUT_DOWN);
		break;
	}

	// Keep track of whether CTRL and SHIFT keys are pressed.
	switch (glutGetModifiers())
	{
	case GLUT_ACTIVE_CTRL:
		controlState = TRANSLATE;
		break;

	case GLUT_ACTIVE_SHIFT:
		controlState = SCALE;
		break;

		// If CTRL and SHIFT are not pressed, we are in rotate mode.
	default:
		controlState = ROTATE;
		break;
	}

	// Store the new mouse position.
	mousePos[0] = x;
	mousePos[1] = y;
}

void keyboardFunc(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27: // ESC key
		exit(0); // exit the program
		break;
	case 'x':
		// Take a screenshot.
		saveScreenshot("screenshot.jpg");
		break;
	case '1':
		break;
	case '2':
		break;
	case '3':
		break;
	case '4':
		break;
	case '5':
		break;
	case '+':
		scale *= 2.0f;
		break;
	case '-':
		scale *= 0.5f;
		break;
	case ' ':
		toggleAnimation = !toggleAnimation;
		break;
	}
}

void displayFunc()
{
	// This function performs the actual rendering.

	// First, clear the screen.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Bind the pipeline program that we just created. 
	// The purpose of binding a pipeline program is to activate the shaders that it contains, i.e.,
	// any object rendered from that point on, will use those shaders.
	// When the application starts, no pipeline program is bound, which means that rendering is not set up.
	// So, at some point (such as below), we need to bind a pipeline program.
	// From that point on, exactly one pipeline program is bound at any moment of time.
	pipelineProgram->Bind();

	// In here, you can do additional modeling on the object, such as performing translations, rotations and scales.
	// ...

	// Read the current modelview and projection matrices from our helper class.
	// The matrices are only read here; nothing is actually communicated to OpenGL yet.
	float modelViewMatrix[16];

	matrix.SetMatrixMode(OpenGLMatrix::ModelView);
	matrix.LoadIdentity();
	matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
	matrix.Rotate(terrainRotate[0], 1.0, 0.0, 0.0);
	matrix.Rotate(terrainRotate[1], 0.0, 1.0, 0.0);
	matrix.Rotate(terrainRotate[2], 0.0, 0.0, 1.0);
	matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

	// Set up the camera position, focus point, and the up vector.
	eye = glm::vec3(terrainPointPositions[currentIndex].x - 0.01 * terrainPointNormals[currentIndex].x, 
					terrainPointPositions[currentIndex].y - 0.01 * terrainPointNormals[currentIndex].y, 
					terrainPointPositions[currentIndex].z - 0.01 * terrainPointNormals[currentIndex].z);

	matrix.LookAt(eye.x, eye.y, eye.z,
		terrainPointPositions[currentIndex].x + terrainPointTangents[currentIndex].x, 
		terrainPointPositions[currentIndex].y + terrainPointTangents[currentIndex].y,
		terrainPointPositions[currentIndex].z + terrainPointTangents[currentIndex].z,
		-terrainPointNormals[currentIndex].x, -terrainPointNormals[currentIndex].y, -terrainPointNormals[currentIndex].z);

	matrix.GetMatrix(modelViewMatrix);

	float projectionMatrix[16];
	matrix.SetMatrixMode(OpenGLMatrix::Projection);
	matrix.GetMatrix(projectionMatrix);

	// Upload the modelview and projection matrices to the GPU. Note that these are "uniform" variables.
	// Important: these matrices must be uploaded to *all* pipeline programs used.
	// In hw1, there is only one pipeline program, but in hw2 there will be several of them.
	// In such a case, you must separately upload to *each* pipeline program.
	// Important: do not make a typo in the variable name below; otherwise, the program will malfunction.
	pipelineProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
	pipelineProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);

	pipelineProgram->SetUniformVariableVec3("lightDir", &lightDir[0]);
	pipelineProgram->SetUniformVariableVec3("lightColor", &lightColor[0]);

	leftTrackVAO->Bind();
	glDrawArrays(GL_TRIANGLES, 0, numLeftTrackVertices);

	rightTrackVAO->Bind();
	glDrawArrays(GL_TRIANGLES, 0, numRightTrackVertices);

	crossSectionVAO->Bind();
	glDrawArrays(GL_TRIANGLES, 0, numCrossSectionVertices);

	skyboxProgram->Bind();

	matrix.SetMatrixMode(OpenGLMatrix::ModelView);
	matrix.LoadIdentity();
	matrix.Translate(terrainTranslate[0], terrainTranslate[1], terrainTranslate[2]);
	matrix.Rotate(terrainRotate[0], 1.0, 0.0, 0.0);
	matrix.Rotate(terrainRotate[1], 0.0, 1.0, 0.0);
	matrix.Rotate(-90.0, 0.0, 0.0, 1.0);
	matrix.Scale(terrainScale[0], terrainScale[1], terrainScale[2]);

	// Set up the camera position, focus point, and the up vector.
	eye = glm::vec3(terrainPointPositions[currentIndex].x - 0.01 * terrainPointNormals[currentIndex].x,
		terrainPointPositions[currentIndex].y - 0.01 * terrainPointNormals[currentIndex].y,
		terrainPointPositions[currentIndex].z - 0.01 * terrainPointNormals[currentIndex].z);

	matrix.LookAt(eye.x, eye.y, eye.z,
		terrainPointPositions[currentIndex].x + terrainPointTangents[currentIndex].x,
		terrainPointPositions[currentIndex].y + terrainPointTangents[currentIndex].y,
		terrainPointPositions[currentIndex].z + terrainPointTangents[currentIndex].z,
		-terrainPointNormals[currentIndex].x, -terrainPointNormals[currentIndex].y, -terrainPointNormals[currentIndex].z);

	matrix.GetMatrix(modelViewMatrix);

	// draw skybox as last
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content

	//modelViewMatrix[3] = 0.0f;
	//modelViewMatrix[7] = 0.0f;
	//modelViewMatrix[11] = 0.0f;
	//modelViewMatrix[15] = 0.0f;

	skyboxProgram->SetUniformVariableMatrix4fv("modelViewMatrix", GL_FALSE, modelViewMatrix);
	skyboxProgram->SetUniformVariableMatrix4fv("projectionMatrix", GL_FALSE, projectionMatrix);

	// skybox cube
	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
	glDepthFunc(GL_LESS); // set depth function back to default

	// Swap the double-buffers.
	glutSwapBuffers();
}

GLuint createTexture(const std::string& path)
{
	textureImage = new ImageIO();

	if (textureImage->loadJPEG(path.c_str()) != ImageIO::OK)
	{
		cout << "Error reading image " << path << "." << endl;
		exit(EXIT_FAILURE);
	}

	GLuint texture = 0;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureImage->getWidth(), textureImage->getHeight(), 0, GL_RGB, GL_UNSIGNED_BYTE, textureImage->getPixels());
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	return texture;
}

void generateNormalAndBinormals()
{
	glm::vec3 arb(0.0, 1.0, 0.0);
	glm::vec3 n0 = glm::normalize(glm::cross(terrainPointTangents[0], arb));
	glm::vec3 b0 = glm::normalize(glm::cross(terrainPointTangents[0], n0));
	terrainPointNormals.push_back(n0);
	terrainPointBinormals.push_back(b0);

	for (int i = 1; i < terrainPointPositions.size(); i++)
	{
		glm::vec3 normal = glm::normalize(glm::cross(terrainPointBinormals[i - 1], terrainPointTangents[i]));
		glm::vec3 binormal = glm::normalize(glm::cross(terrainPointTangents[i], normal));
		terrainPointNormals.push_back(normal);
		terrainPointBinormals.push_back(binormal);
	}
}

void generateSplinePoints()
{
	for (int i = 0; i < numSplines; i++)
	{
		// Generate Catmull-Rom spline points
		std::vector<glm::vec3> controlPoints;

		for (int n = 0; n < splines[0].numControlPoints; n++)
		{
			auto point = splines[0].points[n];
			controlPoints.push_back({ point.x, point.y, point.z });
		}

		std::vector<glm::vec3> splinePoints;

		for (int j = 1; j < controlPoints.size() - 2; j++)
		{
			glm::mat3x4 control = glm::mat3x4(
				splines[i].points[j - 1].x, splines[i].points[j].x, splines[i].points[j + 1].x, splines[i].points[j + 2].x,
				splines[i].points[j - 1].y, splines[i].points[j].y, splines[i].points[j + 1].y, splines[i].points[j + 2].y,
				splines[i].points[j - 1].z, splines[i].points[j].z, splines[i].points[j + 1].z, splines[i].points[j + 2].z
			);

			glm::mat4 basis = glm::mat4(
				-scale, 2 * scale, -scale, 0,
				2 - scale, scale - 3, 0, 1,
				scale - 2, 3 - 2 * scale, scale, 0,
				scale, -scale, 0, 0
			);

			for (int k = 0; k < NUM_SEGMENTS; k++)
			{
				float u = k * (1.0 / (NUM_SEGMENTS - 1));
				glm::vec4 uu(u * u * u, u * u, u, 1);
				glm::vec3 point = glm::transpose(control) * glm::transpose(basis) * uu;
				terrainPointPositions.push_back(point);

				uu = { 3 * u * u, 2 * u, 1, 0 };
				glm::vec3 tan = glm::normalize(glm::transpose(control) * glm::transpose(basis) * uu);
				terrainPointTangents.push_back(tan);
			}
		}
	}
}

void generateTracks()
{
	float ratio = 0.01;
	float alpha = 0.001;
	float interval = 0.01;

	for (int i = 1; i < terrainPointPositions.size() - 1; i++)
	{
		glm::vec3 p0 = terrainPointPositions[i];
		glm::vec3 n0 = terrainPointNormals[i];
		glm::vec3 b0 = terrainPointBinormals[i];

		glm::vec3 p1 = terrainPointPositions[i + 1];
		glm::vec3 n1 = terrainPointNormals[i + 1];
		glm::vec3 b1 = terrainPointBinormals[i + 1];

		glm::vec3 vertices[8];
		vertices[0] = p0 + alpha * (-n0 + b0);
		vertices[1] = p0 + alpha * ( n0 + b0);
		vertices[2] = p0 + alpha * ( n0 - b0);
		vertices[3] = p0 + alpha * (-n0 - b0);
		vertices[4] = p1 + alpha * (-n1 + b1);
		vertices[5] = p1 + alpha * ( n1 + b1);
		vertices[6] = p1 + alpha * ( n1 - b1);
		vertices[7] = p1 + alpha * (-n1 - b1);

		int indices[24] = { 0, 4 ,5, 0, 1, 5,
							1, 5, 6, 1, 2, 6,
							3, 7, 6, 3, 2, 6,
							0, 4, 7, 0, 3, 7 };

		glm::vec3 faceNormal[4] = { -terrainPointBinormals[i], terrainPointNormals[i], terrainPointBinormals[i], -terrainPointNormals[i] };

		for (int t = 0; t < 24; t++)
		{
			glm::vec3 lv = vertices[indices[t]] + ratio * ((indices[t] <= 3) ? terrainPointBinormals[i] : terrainPointBinormals[i + 1]);
			leftTrackPositions.push_back(lv);

			glm::vec3 rv = vertices[indices[t]] - ratio * ((indices[t] <= 3) ? terrainPointBinormals[i] : terrainPointBinormals[i + 1]);
			rightTrackPositions.push_back(rv);

			leftTrackNormals.push_back(faceNormal[t / 6]);
			rightTrackNormals.push_back(faceNormal[t / 6]);
		}
	}

	glm::vec3 prev = terrainPointPositions[0];
	for (int i = 1; i < terrainPointPositions.size(); i = i + 1)
	{
		glm::vec3 curr = terrainPointPositions[i];
		if (glm::distance(prev, curr) < interval)
			continue;

		glm::vec3 p0 = terrainPointPositions[i] + ratio * terrainPointBinormals[i];
		glm::vec3 p1 = terrainPointPositions[i] - ratio * terrainPointBinormals[i];
		glm::vec3 n = terrainPointNormals[i];
		glm::vec3 t = terrainPointTangents[i];

		glm::vec3 vertices[8];
		float beta = 0.001 / 2;
		vertices[0] = p0 + beta * (-n - t);
		vertices[1] = p0 + beta * ( n - t);
		vertices[2] = p0 + beta * ( n + t);
		vertices[3] = p0 + beta * (-n + t);
		vertices[4] = p1 + beta * (-n - t);
		vertices[5] = p1 + beta * ( n - t);
		vertices[6] = p1 + beta * ( n + t);
		vertices[7] = p1 + beta * (-n + t);

		int indices[24] = { 0, 4, 5, 0, 1, 5,
							1, 5, 6, 1, 2, 6,
							3, 7, 6, 3, 2, 6,
							0, 4, 7, 0, 3, 7 };

		glm::vec3 faceNormal[4] = { -terrainPointTangents[i], terrainPointNormals[i], terrainPointTangents[i], -terrainPointNormals[i] };

		for (int t = 0; t < 24; t++)
		{
			crossSectionPositions.push_back(vertices[indices[t]]);
			crossSectionNormals.push_back(faceNormal[t / 6]);
		}
		prev = curr;
	}
}

void initScene(int argc, char* argv[])
{
	if (argc < 2)
	{
		printf("usage: %s <trackfile>\n", argv[0]);
		exit(0);
	}

	// load the splines from the provided filename
	loadSplines(argv[1]);

	generateSkybox();

	printf("Loaded %d spline(s).\n", numSplines);
	for (int i = 0; i < numSplines; i++)
		printf("Num control points in spline %d: %d.\n", i, splines[i].numControlPoints);

	// Set the background color.
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black color.

	// Enable z-buffering (i.e., hidden surface removal using the z-buffer algorithm).
	glEnable(GL_DEPTH_TEST);

	// Create a pipeline program. This operation must be performed BEFORE we initialize any VAOs.
	// A pipeline program contains our shaders. Different pipeline programs may contain different shaders.
	// In this homework, we only have one set of shaders, and therefore, there is only one pipeline program.
	// In hw2, we will need to shade different objects with different shaders, and therefore, we will have
	// several pipeline programs (e.g., one for the rails, one for the ground/sky, etc.).
	pipelineProgram = new PipelineProgram(); // Load and set up the pipeline program, including its shaders.
	// Load and set up the pipeline program, including its shaders.
	if (pipelineProgram->BuildShadersFromFiles(shaderBasePath, "vertexShader.glsl", "fragmentShader.glsl") != 0)
	{
		cout << "Failed to build the pipeline program." << endl;
		throw 1;
	}

	skyboxProgram = new PipelineProgram(); // Load and set up the pipeline program, including its shaders.
	// Load and set up the pipeline program, including its shaders.
	if (skyboxProgram->BuildShadersFromFiles(shaderBasePath, "skyboxVertexShader.glsl", "skyboxFragmentShader.glsl") != 0)
	{
		cout << "Failed to build the pipeline program." << endl;
		throw 1;
	}

	cout << "Successfully built the pipeline program." << endl;

	// Prepare the triangle position and color data for the VBO. 
	// The code below sets up a single triangle (3 vertices).
	// The triangle will be rendered using GL_TRIANGLES (in displayFunc()).

	// Create the VBOs. 
	// We make a separate VBO for vertices and colors. 
	// This operation must be performed BEFORE we initialize any VAOs.

	generateSplinePoints();

	generateNormalAndBinormals();

	generateTracks();

	terrainPointPositions.push_back(terrainPointPositions[0]);

	terrainLinePositions = terrainPointPositions;
	terrainLineColors = terrainPointColors;

	numPointVertices = terrainPointPositions.size();

	terrainPointColors.resize(numPointVertices);

	std::fill(terrainPointColors.begin(), terrainPointColors.end(), glm::vec4(1.0f));

	pointPositionsVBO = new VBO(numPointVertices, 3, terrainPointPositions.data(), GL_STATIC_DRAW); // 3 values per position
	pointColorsVBO = new VBO(numPointVertices, 4, terrainPointColors.data(), GL_STATIC_DRAW); // 4 values per color

	terrainLinePositions = terrainPointPositions;
	terrainLineColors = terrainPointColors;

	numLineVertices = numPointVertices;
	linePositionsVBO = new VBO(numLineVertices, 3, terrainLinePositions.data(), GL_STATIC_DRAW);
	lineColorsVBO = new VBO(numLineVertices, 4, terrainLineColors.data(), GL_STATIC_DRAW);

	numLeftTrackVertices = leftTrackPositions.size();

	leftTrackPositionsVBO = new VBO(numLeftTrackVertices, 3, leftTrackPositions.data(), GL_STATIC_DRAW);

	numRightTrackVertices = rightTrackPositions.size();

	rightTrackPositionsVBO = new VBO(numRightTrackVertices, 3, rightTrackPositions.data(), GL_STATIC_DRAW);

	numLeftTrackVertices = leftTrackNormals.size();

	leftTrackNormalsVBO = new VBO(numLeftTrackVertices, 3, leftTrackNormals.data(), GL_STATIC_DRAW);

	numRightTrackVertices = rightTrackNormals.size();

	rightTrackNormalsVBO = new VBO(numRightTrackVertices, 3, rightTrackNormals.data(), GL_STATIC_DRAW);

	numCrossSectionVertices = crossSectionPositions.size();

	crossSectionPositionsVBO = new VBO(numCrossSectionVertices, 3, crossSectionPositions.data(), GL_STATIC_DRAW);

	numCrossSectionVertices = crossSectionNormals.size();

	crossSectionNormalsVBO = new VBO(numCrossSectionVertices, 3, crossSectionNormals.data(), GL_STATIC_DRAW);

	// Create the VAOs. There is a single VAO in this example.
	// Important: this code must be executed AFTER we created our pipeline program, and AFTER we set up our VBOs.
	// A VAO contains the geometry for a single object. There should be one VAO per object.
	// In this homework, "geometry" means vertex positions and colors. In homework 2, it will also include
	// vertex normal and vertex texture coordinates for texture mapping.
	pointVAO = new VAO();

	// Set up the relationship between the "position" shader variable and the VAO.
	// Important: any typo in the shader variable name will lead to malfunction.
	pointVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, pointPositionsVBO, "position");

	// Set up the relationship between the "color" shader variable and the VAO.
	// Important: any typo in the shader variable name will lead to malfunction.
	pointVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, pointColorsVBO, "color");

	lineVAO = new VAO();

	// Set up the relationship between the "position" shader variable and the VAO.
	// Important: any typo in the shader variable name will lead to malfunction.
	lineVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, linePositionsVBO, "position");

	// Set up the relationship between the "color" shader variable and the VAO.
	// Important: any typo in the shader variable name will lead to malfunction.
	lineVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, lineColorsVBO, "color");

	leftTrackVAO = new VAO();

	leftTrackVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, leftTrackPositionsVBO, "position");
	leftTrackVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, leftTrackNormalsVBO, "inNormal");

	rightTrackVAO = new VAO();

	rightTrackVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, rightTrackPositionsVBO, "position");
	rightTrackVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, rightTrackNormalsVBO, "inNormal");

	crossSectionVAO = new VAO();

	crossSectionVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, crossSectionPositionsVBO, "position");
	crossSectionVAO->ConnectPipelineProgramAndVBOAndShaderVariable(pipelineProgram, crossSectionNormalsVBO, "inNormal");

	// Check for any OpenGL errors.
	std::cout << "GL error status is: " << glGetError() << std::endl;
}

void cleanup()
{
	delete leftTrackPositionsVBO;
	delete leftTrackNormalsVBO;
	delete rightTrackPositionsVBO;
	delete rightTrackNormalsVBO;
	delete crossSectionPositionsVBO;
	delete crossSectionNormalsVBO;

	delete leftTrackVAO;
	delete rightTrackVAO;
	delete crossSectionVAO;

	delete pointVAO;
	delete lineVAO;

	delete pointPositionsVBO;
	delete linePositionsVBO;

	delete pointColorsVBO;
	delete lineColorsVBO;

	delete textureImage;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		cout << "The arguments are incorrect." << endl;
		cout << "usage: ./hw1 <heightmap file>" << endl;
		exit(EXIT_FAILURE);
	}

	cout << "Initializing GLUT..." << endl;
	glutInit(&argc, argv);

	cout << "Initializing OpenGL..." << endl;

#ifdef __APPLE__
	glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#else
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_STENCIL);
#endif

	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(0, 0);
	glutCreateWindow(windowTitle);

	cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
	cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << endl;
	cout << "Shading Language Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;

#ifdef __APPLE__
	// This is needed on recent Mac OS X versions to correctly display the window.
	glutReshapeWindow(windowWidth - 1, windowHeight - 1);
#endif

	// Tells GLUT to use a particular display function to redraw.
	glutDisplayFunc(displayFunc);
	// Perform animation inside idleFunc.
	glutIdleFunc(idleFunc);
	// callback for mouse drags
	glutMotionFunc(mouseMotionDragFunc);
	// callback for idle mouse movement
	glutPassiveMotionFunc(mouseMotionFunc);
	// callback for mouse button changes
	glutMouseFunc(mouseButtonFunc);
	// callback for resizing the window
	glutReshapeFunc(reshapeFunc);
	// callback for pressing the keys on the keyboard
	glutKeyboardFunc(keyboardFunc);

	// init glew
#ifdef __APPLE__
  // nothing is needed on Apple
#else
  // Windows, Linux
	GLint result = glewInit();
	if (result != GLEW_OK)
	{
		cout << "error: " << glewGetErrorString(result) << endl;
		exit(EXIT_FAILURE);
	}
#endif

	// Perform the initialization.
	initScene(argc, argv);

	glutTimerFunc(waitInterval, timerFunc, currentIndex);

	// Sink forever into the GLUT loop.
	glutMainLoop();

	cleanup();
}

