#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <opencv2/opencv.hpp>
#include <yarp/os/all.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Network.h>
#include <yarp/os/Thread.h>
#include <yarp/dev/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/IJoypadController.h>
#include <OVR_CAPI.h>


using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;
using std::cout;
using std::endl;


int window_width;
int window_height;





static GLuint matToTexture(const cv::Mat &mat, GLenum minFilter, GLenum magFilter, GLenum wrapFilter);
void error_callback(int error, const char* description);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void resize_callback(GLFWwindow* window, int new_width, int new_height);
void draw_frame(const cv::Mat& frame);
void init_opengl(int w, int h);
cv::Mat ShowCamera(IFrameGrabberImage** Ifram, bool debug = true);
void OpenView(const std::string& local, const std::string& remote, PolyDriver& cam, IFrameGrabberImage** Ifram);

int main(int argc, char **argv)
{

	Network yarp;
	Property driverOpt;
	IFrameGrabberImage *IframL;
	IFrameGrabberImage *IframR;
	PolyDriver camL;
	PolyDriver camR;
	OpenView("/local/cam/left", "/icubSim/cam/left", camL, &IframL);
	OpenView("/local/cam/right", "/icubSim/cam/right", camR, &IframR);
	cv::Mat img = ShowCamera(&IframL);
	window_width = img.cols;
	window_height = img.rows;

	//---- SETUP GLFW ----------//
	GLFWwindow* window;

	glfwSetErrorCallback(error_callback);

	if (!glfwInit()) {
		return 0;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	window = glfwCreateWindow(window_width, window_height, "Simple example", NULL, NULL);
	if (!window) {
		glfwTerminate();

	}

	glfwSetKeyCallback(window, key_callback);
	glfwSetWindowSizeCallback(window, resize_callback);

	glfwMakeContextCurrent(window);

	glfwSwapInterval(1);

	// ---- SETUP GLEW ---- //
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		cout << "GLEW initialisation error: " << glewGetErrorString(err) << endl;
		return -1;
	}
	cout << "GLEW okay - using version: " << glewGetString(GLEW_VERSION) << endl;
	//  ------ OVR SETUP --------
	ovrResult result = ovr_Initialize(nullptr);

	if (OVR_FAILURE(result))
		return 0;

	ovrSession session;
	ovrGraphicsLuid luid;
	result = ovr_Create(&session, &luid);
	if (!OVR_SUCCESS(result)) {
		std::cerr << "Oculus Rift not detected" << std::endl;
		std::exit(-1);
	}

	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_EyeLevel);

	ovrHmdDesc desc = ovr_GetHmdDesc(session);
	if (desc.ProductName[0] == '\0') {
		std::cerr << "Rift detected, display not enabled" << std::endl;
	}

	ovrSizei resolution = desc.Resolution;


	// --- Open window --- ///
	init_opengl(window_width, window_height);

	// --- drawing img ---- //
	while (!glfwWindowShouldClose(window)) {

		cv::Mat imgL = ShowCamera(&IframL);
		cv::Mat imgR = ShowCamera(&IframR);
		draw_frame(imgL);
		draw_frame(imgR);
		glfwSwapBuffers(window);
		glfwPollEvents();

	}
	glfwDestroyWindow(window);
	glfwTerminate();

	ovr_Destroy(session);
	ovr_Shutdown();
	std::cin.get();
	return 1;
}


// Function turn a cv::Mat into a texture, and return the texture ID as a GLuint for use
static GLuint matToTexture(const cv::Mat &mat, GLenum minFilter, GLenum magFilter, GLenum wrapFilter) {
	// Generate a number for our textureID's unique handle
	GLuint textureID;
	glGenTextures(1, &textureID);

	// Bind to our texture handle
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Catch silly-mistake texture interpolation method for magnification
	if (magFilter == GL_LINEAR_MIPMAP_LINEAR ||
		magFilter == GL_LINEAR_MIPMAP_NEAREST ||
		magFilter == GL_NEAREST_MIPMAP_LINEAR ||
		magFilter == GL_NEAREST_MIPMAP_NEAREST)
	{
		cout << "You can't use MIPMAPs for magnification - setting filter to GL_LINEAR" << endl;
		magFilter = GL_LINEAR;
	}

	// Set texture interpolation methods for minification and magnification
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	// Set texture clamping method
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapFilter);

	// Set incoming texture format to:
	// GL_BGR       for CV_CAP_OPENNI_BGR_IMAGE,
	// GL_LUMINANCE for CV_CAP_OPENNI_DISPARITY_MAP,
	GLenum inputColourFormat = GL_BGR;
	if (mat.channels() == 1)
	{
		inputColourFormat = GL_LUMINANCE;
	}
	// Create the texture
	glTexImage2D(GL_TEXTURE_2D,     // Type of texture
		0,                 // Pyramid level (for mip-mapping) - 0 is the top level
		GL_RGB,            // Internal colour format to convert to
		mat.cols,          // Image width  
		mat.rows,          // Image height 
		0,                 // Border width in pixels (can either be 1 or 0)
		inputColourFormat, // Input image format (i.e. GL_RGB, GL_RGBA, GL_BGR etc.)
		GL_UNSIGNED_BYTE,  // Image data type
		mat.ptr());        // The actual image data itself

						   // If we're using mipmaps then generate them. 
	if (minFilter == GL_LINEAR_MIPMAP_LINEAR ||
		minFilter == GL_LINEAR_MIPMAP_NEAREST ||
		minFilter == GL_NEAREST_MIPMAP_LINEAR ||
		minFilter == GL_NEAREST_MIPMAP_NEAREST)
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}

	return textureID;
}

void error_callback(int error, const char* description) {
	fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

void resize_callback(GLFWwindow* window, int new_width, int new_height) {
	glViewport(0, 0, window_width = new_width, window_height = new_height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, window_width, window_height, 0.0, 0.0, 100.0);
	glMatrixMode(GL_MODELVIEW);
}

void draw_frame(const cv::Mat& frame) {
	// Clear color and depth buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);     // Operate on model-view matrix

	glEnable(GL_TEXTURE_2D);
	GLuint image_tex = matToTexture(frame, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, GL_CLAMP);

	/* Draw a quad */
	glBegin(GL_QUADS);
	glTexCoord2i(0, 0); glVertex2i(0, 0);
	glTexCoord2i(0, 1); glVertex2i(0, window_height);
	glTexCoord2i(1, 1); glVertex2i(window_width, window_height);
	glTexCoord2i(1, 0); glVertex2i(window_width, 0);
	glEnd();

	glDeleteTextures(1, &image_tex);
	glDisable(GL_TEXTURE_2D);
}

void init_opengl(int w, int h) {
	glViewport(0, 0, w, h); // use a screen size of WIDTH x HEIGHT

	glMatrixMode(GL_PROJECTION);     // Make a simple 2D projection on the entire window
	glLoadIdentity();
	glOrtho(0.0, w, h, 0.0, 0.0, 100.0);

	glMatrixMode(GL_MODELVIEW);    // Set the matrix mode to object modeling

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the window
}

cv::Mat ShowCamera(IFrameGrabberImage** Ifram , bool debug)
{
	ImageOf<PixelRgb> img;
	printf(" started receiving images \n");
	if (*Ifram == nullptr)
	{
		printf("NULLPTR_ERROR");

	}
	if ((*Ifram)->getImage(img))
	{
		//img.resize(640, 480);
		if(debug)
			std::cout << " imgsize is: width  " << img.width() << " height : " << img.height() << std::endl;
		IplImage *cvImage = cvCreateImage(cvSize(img.width(),
			img.height()),
			IPL_DEPTH_8U, 3);
		cvCvtColor((IplImage*)img.getIplImage(), cvImage, CV_RGB2BGR);
		cv::Mat m = cv::cvarrToMat(cvImage);
		return m;
	}
}

void OpenView(const std::string& local, const std::string& remote, PolyDriver& cam, IFrameGrabberImage** Ifram)
{
	Property driverOpt;
	driverOpt.put("device", "remote_grabber");
	driverOpt.put("remote", remote);
	driverOpt.put("local", local);
	if (!cam.open(driverOpt))
	{
		std::cout << "Can't connect to WORLD cameras" << std::endl;
		std::exit(1);
	}
	if (!cam.view(*Ifram))
	{
		std::cout << "unable to dynamic cast remote_grabber to IFrameGrabber interface " << std::endl;
		std::exit(1);
	}
}



//void initTexture(IplImage** Image)
//{
//	/*glClearColor(0.0, 0.0, 0.0, 0.0);
//	glShadeModel(GL_FLAT);
//	glEnable(GL_DEPTH_TEST);*/
//	glGenTextures(1, &texName);
//	glBindTexture(GL_TEXTURE_2D, texName);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//	glTexImage2D(GL_TEXTURE_2D, 0, 3, (*Image)->width, (*Image)->height, 0, GL_BGR, GL_UNSIGNED_BYTE, (*Image)->imageData);
//}
//
//void applyTexture(int img_width, int img_height)
//{
//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//	glEnable(GL_TEXTURE_2D);
//	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
//	glBindTexture(GL_TEXTURE_2D, texName);
//	glBegin(GL_QUADS);
//	glEnd();
//	glFlush();
//	glDisable(GL_TEXTURE_2D);
//}
//
//void loadImage(IplImage **Image, GLFWwindow** window)
//{
//	initTexture(Image);
//	glMatrixMode(GL_PROJECTION);
//	glLoadIdentity();
//
//	glMatrixMode(GL_MODELVIEW);
//	glLoadIdentity();
//	applyTexture((*Image)->width, (*Image)->height);
//	glfwSwapBuffers(*window);
//	glfwPollEvents();
//}